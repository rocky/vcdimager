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
#include <assert.h>
#include <stdlib.h>

#include "vcd_xml_master.h"

#include <libvcd/vcd.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_stream_stdio.h>
#include <libvcd/vcd_image_bincue.h>

static vcd_type_t
_type_id_by_str (const char class[], const char version[])
{
  struct {
    const char *class;
    const char *version;
    vcd_type_t id;
  } type_str[] = {
    { "vcd", "1.1", VCD_TYPE_VCD11 },
    { "vcd", "2.0", VCD_TYPE_VCD2 },
    { "svcd", "1.0", VCD_TYPE_SVCD },
    { NULL, }
  };
      
  int i = 0;

  while (type_str[i].class) 
    if (strcasecmp(class, type_str[i].class)
	|| strcasecmp(version, type_str[i].version))
      i++;
    else
      break;

  if (!type_str[i].class)
    {
      vcd_error ("invalid type given");
      return 0;
    }
  
  return type_str[i].id;
}

bool vcd_xml_master (const struct vcdxml_t *obj, const char cue_fname[],
		     const char bin_fname[], bool sector_2336_flag)
{
  VcdObj *_vcd;
  VcdListNode *node;
  int idx;

  assert (obj != NULL);

  assert (obj->class != NULL);
  assert (obj->version != NULL);

  _vcd = vcd_obj_new (_type_id_by_str (obj->class, obj->version));

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
	  
	  assert (_source != NULL);

	  vcd_obj_add_file (_vcd, dentry->name, _source, dentry->file_raw);
	}
      else
	vcd_obj_add_dir (_vcd, dentry->name);
      
    }

  idx = 0;
  _VCD_LIST_FOREACH (node, obj->segment_list)
    {
      struct segment_t *item = _vcd_list_node_data (node);
      VcdDataSource *_source = vcd_data_source_new_stdio (item->src);

      vcd_debug ("adding item #%d, %s", idx, item->src);

      assert (_source != NULL);

      vcd_obj_append_segment_play_item (_vcd,
					vcd_mpeg_source_new (_source),
					item->id);

      idx++;
    }

  vcd_debug ("sequence count %d", _vcd_list_length (obj->sequence_list));
  
  idx = 0;
  _VCD_LIST_FOREACH (node, obj->sequence_list)
    {
      struct sequence_t *sequence = _vcd_list_node_data (node);
      VcdDataSource *data_source;
      VcdListNode *node2;

      vcd_debug ("adding sequence #%d, %s", idx, sequence->src);

      data_source = vcd_data_source_new_stdio (sequence->src);
      assert (data_source != NULL);

      vcd_obj_append_sequence_play_item (_vcd,
					 vcd_mpeg_source_new (data_source),
					 sequence->id);

      _VCD_LIST_FOREACH (node2, sequence->entry_point_list)
	{
	  struct entry_point_t *entry = _vcd_list_node_data (node2);

	  vcd_obj_add_sequence_entry (_vcd, sequence->id, entry->timestamp, entry->id);
	}
    }

  /****************************************************************************
   *
   */

  {
    unsigned sectors;
    VcdImageSink *image_sink;

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

    vcd_obj_write_image (_vcd, image_sink, NULL, NULL);

    vcd_obj_end_output (_vcd);

    fprintf (stdout, "finished ok, image created with %d sectors\n", sectors);
  }

  vcd_obj_destroy (_vcd);

  return false;
}


