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
#include <libvcd/vcd_image.h>

typedef struct {
  double time;
  char *id;
} entry_t;

typedef struct {
  double time;
  char *id;
} pause_t;

typedef struct {
  VcdMpegSource *source;
  char *id;
  const struct vcd_mpeg_source_info *info;
  VcdList *pause_list; /* pause_t */
  VcdList *entry_list; /* entry_t */

  /* computed on sector allocation */
  unsigned relative_start_extent; /* relative to iso data end */
} mpeg_sequence_t;

/* work in progress -- fixme rename all occurences */
#define mpeg_track_t mpeg_sequence_t
#define mpeg_track_list mpeg_sequence_list 

typedef struct {
  VcdMpegSource *source;
  char *id;
  const struct vcd_mpeg_source_info *info;
  unsigned segment_count;

  /* computed on sector allocation */
  unsigned start_extent;
} mpeg_segment_t;


typedef struct {
  char *iso_pathname;
  VcdDataSource *file;
  bool raw_flag;
  
  uint32_t size;
  uint32_t start_extent;
  uint32_t sectors;
} custom_file_t;

struct _VcdObj {
  vcd_type_t type;
  bool broken_svcd_mode_flag;

  unsigned pre_track_gap;
  unsigned pre_data_gap;
  unsigned post_data_gap;

  /* output */
  VcdImageSink *image_sink;

  /* ... */
  unsigned iso_size;
  char *iso_volume_label;
  char *iso_application_id;

  char *info_album_id;
  unsigned info_volume_count;
  unsigned info_volume_number;
  unsigned info_restriction;
  bool info_use_seq2;
  bool info_use_lid2;

  /* input */
  unsigned mpeg_segment_start_extent;
  VcdList *mpeg_segment_list; /* mpeg_segment_t */

  VcdList *mpeg_sequence_list; /* mpeg_sequence_t */

  unsigned relative_end_extent; /* last mpeg sequence track end extent */

  /* PBC */
  VcdList *pbc_list; /* pbc_t */
  unsigned psd_size;
  unsigned psdx_size;

  /* custom files */
  unsigned ext_file_start_extent; 
  unsigned custom_file_start_extent; 
  VcdList *custom_file_list; /* custom_file_t */

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
