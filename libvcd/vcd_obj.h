/*
    $Id$

    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>

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

#ifndef __VCD_OBJ_H__
#define __VCD_OBJ_H__

#include "vcd_data_structures.h"
#include "vcd_mpeg_stream.h"
#include "vcd_iso9660.h"
#include "vcd_files.h"
#include "vcd_salloc.h"
#include "vcd_directory.h"

typedef struct {
  VcdMpegSource *source;
  const struct vcd_mpeg_source_info *info;
  unsigned relative_start_extent; /* relative to iso data end */
} mpeg_track_t;

struct _VcdObj {
  vcd_type_t type;
  bool broken_svcd_mode_flag;

  unsigned pre_track_gap;
  unsigned pre_data_gap;
  unsigned post_data_gap;

  /* output */
  bool bin_file_2336_flag;
  VcdDataSink *bin_file;
  /* VcdDataSink* cue_file; */

  unsigned iso_size;
  char *iso_volume_label;
  char *iso_application_id;

  /* input */
  VcdList *mpeg_track_list; /* mpeg_track_t */

  unsigned relative_end_extent; /* last mpeg end extent */

  /* custom files */
  unsigned custom_file_start_extent; 
  VcdList *custom_file_list;

  /* dictionary */
  VcdList *buffer_dict_list;

  /* aggregates */
  VcdSalloc *iso_bitmap;

  VcdDirectory *dir;

  /* state info */
  bool in_output;

  unsigned sectors_written;
  unsigned in_track;

  long last_cb_call;

  progress_callback_t progress_callback;
  void *callback_user_data;
};

#endif /* __VCD_OBJ_H__ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
