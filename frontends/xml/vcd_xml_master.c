/*
    $Id$

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "vcd_xml_master.h"
#include "vcd_xml_common.h"

#include <libvcd/vcd.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_stream_stdio.h>
#include <libvcd/vcd_image_bincue.h>
#include <libvcd/vcd_image_cdrdao.h>
#include <libvcd/vcd_image_nrg.h>
#include <libvcd/vcd_bytesex.h>

static const char _rcsid[] = "$Id$";

bool vcd_xml_master (const struct vcdxml_t *obj, const char cue_fname[],
		     const char bin_fname[], const char cdrdao_base[],
		     const char nrg_fname[], bool sector_2336_flag)
{
  VcdObj *_vcd;
  VcdListNode *node;
  int idx;
  bool _relaxed_aps = false;

  vcd_assert (obj != NULL);

  _vcd = vcd_obj_new (obj->vcd_type);

  if (obj->info.album_id)
    vcd_obj_set_param_str (_vcd, VCD_PARM_ALBUM_ID, obj->info.album_id);

  vcd_obj_set_param_uint (_vcd, VCD_PARM_VOLUME_NUMBER, obj->info.volume_number);
  vcd_obj_set_param_uint (_vcd, VCD_PARM_VOLUME_COUNT, obj->info.volume_count);
  vcd_obj_set_param_uint (_vcd, VCD_PARM_RESTRICTION, obj->info.restriction);
  vcd_obj_set_param_bool (_vcd, VCD_PARM_NEXT_VOL_SEQ2, obj->info.use_sequence2);
  vcd_obj_set_param_bool (_vcd, VCD_PARM_NEXT_VOL_LID2, obj->info.use_lid2);

  if (obj->pvd.volume_id)
    vcd_obj_set_param_str (_vcd, VCD_PARM_VOLUME_ID, obj->pvd.volume_id);

  if (obj->pvd.application_id)
    vcd_obj_set_param_str (_vcd, VCD_PARM_APPLICATION_ID, obj->pvd.application_id);

  _VCD_LIST_FOREACH (node, obj->option_list)
    {
      const struct option_t *_option = _vcd_list_node_data (node);

      struct {
	const char *str;
	enum {
	  OPT_BOOL = 1,
	  OPT_UINT,
	  OPT_STR
	} val_type;
	vcd_parm_t parm_id;
      } const _opt_cfg[] = {
	{ OPT_SVCD_VCD3_MPEGAV, OPT_BOOL, VCD_PARM_SVCD_VCD3_MPEGAV },
	{ OPT_SVCD_VCD3_ENTRYSVD, OPT_BOOL, VCD_PARM_SVCD_VCD3_ENTRYSVD },
	{ OPT_SVCD_VCD3_TRACKSVD, OPT_BOOL, VCD_PARM_SVCD_VCD3_TRACKSVD },
	{ OPT_RELAXED_APS, OPT_BOOL, VCD_PARM_RELAXED_APS },
	{ OPT_UPDATE_SCAN_OFFSETS, OPT_BOOL, VCD_PARM_UPDATE_SCAN_OFFSETS },
	{ OPT_LEADOUT_PAUSE, OPT_BOOL, VCD_PARM_LEADOUT_PAUSE },

	{ OPT_TRACK_PREGAP, OPT_UINT, VCD_PARM_TRACK_PREGAP },
	{ OPT_TRACK_FRONT_MARGIN, OPT_UINT, VCD_PARM_TRACK_FRONT_MARGIN },
	{ OPT_TRACK_REAR_MARGIN, OPT_UINT, VCD_PARM_TRACK_REAR_MARGIN },
	{ 0, }
      }, *_opt_cfg_p = _opt_cfg;

      if (!_option->name)
	{
	  vcd_error ("no option name given!");
	  continue;
	}

      if (!_option->value)
	{
	  vcd_error ("no value given for option name '%s'", _option->name);
	  continue;
	}

      for (; _opt_cfg_p->str; _opt_cfg_p++)
	if (!strcmp (_opt_cfg_p->str, _option->name))
	  break;

      if (!_opt_cfg_p->str)
	{
	  vcd_error ("unknown option name '%s'", _option->name);
	  continue;
	}

      switch (_opt_cfg_p->val_type) 
	{
	case OPT_BOOL:
	  {
	    bool _value;
	    if (!strcmp (_option->value, "true"))
	      _value = true;
	    else if (!strcmp (_option->value, "false"))
	      _value = false;
	    else
	      {
		vcd_error ("option value '%s' invalid (use 'true' or 'false')", _option->value);
		continue;
	      }

	    vcd_obj_set_param_bool (_vcd, _opt_cfg_p->parm_id, _value);

	    if (_opt_cfg_p->parm_id == VCD_PARM_RELAXED_APS)
	      _relaxed_aps = _value;
	  }
	  break;

	case OPT_UINT:
	  {
	    unsigned _value;
	    char *endptr;

	    _value = strtol (_option->value, &endptr, 10);
	    
	    if (*endptr)
	      {
		vcd_error ("error while converting string '%s' to integer", _option->value);
		_value = 0;
	      }
	   
	    vcd_obj_set_param_uint (_vcd, _opt_cfg_p->parm_id, _value);
	  }
	  break;

	case OPT_STR:
	  vcd_assert_not_reached ();
	  break;
	}
    }  

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);

      vcd_debug ("adding pbc (%s/%d)", _pbc->id, _pbc->type);

      vcd_obj_append_pbc_node (_vcd, _pbc);
    }

  _VCD_LIST_FOREACH (node, obj->filesystem)
    {
      struct filesystem_t *dentry = _vcd_list_node_data (node);
      
      if (dentry->file_src) 
	{
	  VcdDataSource *_source = 
	    vcd_data_source_new_stdio (dentry->file_src);
	  
	  vcd_assert (_source != NULL);

	  vcd_obj_add_file (_vcd, dentry->name, _source, dentry->file_raw);
	}
      else
	vcd_obj_add_dir (_vcd, dentry->name);
      
    }

  idx = 0;
  _VCD_LIST_FOREACH (node, obj->segment_list)
    {
      struct segment_t *segment = _vcd_list_node_data (node);
      VcdDataSource *_source = vcd_data_source_new_stdio (segment->src);
      VcdListNode *node2;
      VcdMpegSource *_mpeg_src;

      vcd_debug ("adding segment #%d, %s", idx, segment->src);

      vcd_assert (_source != NULL);

      _mpeg_src = vcd_mpeg_source_new (_source);

      vcd_mpeg_source_scan (_mpeg_src, !_relaxed_aps,
			    vcd_xml_show_progress ? vcd_xml_scan_progress_cb : NULL,
			    segment->id);

      vcd_obj_append_segment_play_item (_vcd, _mpeg_src, segment->id);
      
      _VCD_LIST_FOREACH (node2, segment->autopause_list)
	{
	  double *_ap_ts = _vcd_list_node_data (node2);

	  vcd_obj_add_segment_pause (_vcd, segment->id, *_ap_ts, NULL);
	}

      idx++;
    }

  vcd_debug ("sequence count %d", _vcd_list_length (obj->sequence_list));
  
  idx = 0;
  _VCD_LIST_FOREACH (node, obj->sequence_list)
    {
      struct sequence_t *sequence = _vcd_list_node_data (node);
      VcdDataSource *data_source;
      VcdListNode *node2;
      VcdMpegSource *_mpeg_src;

      vcd_debug ("adding sequence #%d, %s", idx, sequence->src);

      data_source = vcd_data_source_new_stdio (sequence->src);
      vcd_assert (data_source != NULL);

      _mpeg_src = vcd_mpeg_source_new (data_source);

      vcd_mpeg_source_scan (_mpeg_src, !_relaxed_aps,
			    vcd_xml_show_progress ? vcd_xml_scan_progress_cb : NULL,
			    sequence->id);

      vcd_obj_append_sequence_play_item (_vcd, _mpeg_src,
					 sequence->id, sequence->default_entry_id);

      _VCD_LIST_FOREACH (node2, sequence->entry_point_list)
	{
	  struct entry_point_t *entry = _vcd_list_node_data (node2);

	  vcd_obj_add_sequence_entry (_vcd, sequence->id, entry->timestamp, entry->id);
	}

      _VCD_LIST_FOREACH (node2, sequence->autopause_list)
	{
	  double *_ap_ts = _vcd_list_node_data (node2);

	  vcd_obj_add_sequence_pause (_vcd, sequence->id, *_ap_ts, NULL);
	}
    }

  /****************************************************************************
   *
   */

  {
    unsigned sectors;
    VcdImageSink *image_sink;
    char *_tmp;

    if (cdrdao_base)
      {
	char buf[4096] = { 0, };

	vcd_info ("cdrdao-style image requested!");

	strncat (buf, cdrdao_base, sizeof (buf));
	strncat (buf, ".toc", sizeof (buf));

	image_sink = 
	  vcd_image_sink_new_cdrdao (buf, cdrdao_base, sector_2336_flag);
      }
    else if (nrg_fname)
      {
	vcd_info ("nrg-style image requested!");

	image_sink = 
	  vcd_image_sink_new_nrg (vcd_data_sink_new_stdio (nrg_fname));
      }
    else
      image_sink = 
	vcd_image_sink_new_bincue (vcd_data_sink_new_stdio (bin_fname),
				   vcd_data_sink_new_stdio (cue_fname),
				   bin_fname, sector_2336_flag);

    if (!image_sink)
      {
        vcd_error ("failed to create image object");
        exit (EXIT_FAILURE);
      }

    sectors = vcd_obj_begin_output (_vcd);

    vcd_obj_write_image (_vcd, image_sink, 
			 vcd_xml_show_progress ? vcd_xml_write_progress_cb : NULL, NULL);

    vcd_obj_end_output (_vcd);

    vcd_info ("finished ok, image created with %d sectors [%s]",
	      sectors, _tmp = _vcd_lba_to_msf_str (sectors));

    free (_tmp);
  }

  vcd_obj_destroy (_vcd);

  return false;
}


