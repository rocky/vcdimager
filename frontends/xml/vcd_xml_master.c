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
    vcd_error ("invalid type given");
  
  return type_str[i].id;
}

bool
vcd_xml_master (const struct vcdxml_t *obj)
{
  VcdObj *_vcd;
  VcdListNode *node;
  int idx;

  assert (obj != NULL);

  assert (obj->class != NULL);
  assert (obj->version != NULL);

  _vcd = vcd_obj_new (_type_id_by_str (obj->class, obj->version));

  if (obj->info.album_id)
    vcd_obj_set_param (_vcd, VCD_PARM_ALBUM_ID, obj->info.album_id);

  if (obj->info.volume_count)
    {
      unsigned _tmp;
      char *endptr;
      
      _tmp = strtoul (obj->info.volume_count, &endptr, 10);
      if (!*endptr)
	{
	  printf ("setting volume count to %u\n", _tmp);
	  vcd_obj_set_param (_vcd, VCD_PARM_VOLUME_COUNT, &_tmp);
	}
    }

  if (obj->info.volume_number)
    {
      unsigned _tmp;
      char *endptr;
      
      _tmp = strtoul (obj->info.volume_number, &endptr, 10);
      if (!*endptr)
	{
	  printf ("setting volume number to %u\n", _tmp);
	  vcd_obj_set_param (_vcd, VCD_PARM_VOLUME_NUMBER, &_tmp);
	}
    }

  if (obj->pvd.volume_id)
    vcd_obj_set_param (_vcd, VCD_PARM_VOLUME_LABEL, obj->pvd.volume_id);

  if (obj->pvd.application_id)
    vcd_obj_set_param (_vcd, VCD_PARM_APPLICATION_ID, obj->pvd.application_id);

  if (obj->filesystem)
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
  if (obj->mpeg_item_list)
    _VCD_LIST_FOREACH (node, obj->mpeg_item_list)
    {
      struct mpeg_item_t *item = _vcd_list_node_data (node);
      VcdDataSource *_source = vcd_data_source_new_stdio (item->src);

      printf ("adding item #%d, %s\n", idx, item->src);

      assert (_source != NULL);

      vcd_obj_append_segment_play_item (_vcd,
					vcd_mpeg_source_new (_source),
					item->id);

      

      idx++;
    }

  printf ("track count %d\n", _vcd_list_length (obj->mpeg_track_list));
  
  idx = 0;
  if (obj->mpeg_track_list)
    _VCD_LIST_FOREACH (node, obj->mpeg_track_list)
    {
      struct mpeg_track_t *track = _vcd_list_node_data (node);
      VcdDataSource *data_source;
      VcdListNode *node2;

      printf ("adding track #%d, %s\n", idx, track->src);

      data_source = vcd_data_source_new_stdio (track->src);
      assert (data_source != NULL);

      vcd_obj_append_sequence_play_item (_vcd,
					 vcd_mpeg_source_new (data_source),
					 track->id);

      _VCD_LIST_FOREACH (node2, track->entry_point_list)
	{
	  struct entry_point_t *entry = _vcd_list_node_data (node2);
	  double _time = 0;
	  char *endptr = NULL;

	  _time = strtod (entry->timestamp, &endptr);

	  if (*endptr)
	    {
	      printf ("ERROR -- broken value...\n");
	      continue;
	    }

	  vcd_obj_add_sequence_entry (_vcd, track->id, _time, entry->id);
	}
    }

  /****************************************************************************
   *
   */

  {
    unsigned sectors = vcd_obj_begin_output (_vcd);

    vcd_obj_write_image (_vcd,
			 vcd_data_sink_new_stdio ("videocd.bin"),
			 NULL, NULL);

    vcd_obj_write_cuefile (_vcd, vcd_data_sink_new_stdio ("videocd.cue"),
			   "videocd.bin");

    vcd_obj_end_output (_vcd);

    fprintf (stdout, "finished ok, image created with %d sectors\n", sectors);
  }

  vcd_obj_destroy (_vcd);

  printf ("%s\n", __PRETTY_FUNCTION__);
  return false;
}


