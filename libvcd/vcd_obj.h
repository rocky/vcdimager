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

#include "vcd_mpeg.h"
#include "vcd_iso9660.h"
#include "vcd_files.h"
#include "vcd_salloc.h"
#include "vcd_directory.h"

struct _VcdObj {
  vcd_type_t type;

  /* output */
  int bin_file_2336_flag;
  VcdDataSink *bin_file;
  /* VcdDataSink* cue_file; */

  unsigned iso_size;
  char *iso_volume_label;

  /* input */
  unsigned relative_end_extent; /* last mpeg end extent */
  struct mpeg_track_t {
    VcdDataSource *source;
    unsigned relative_start_extent; /* relative to iso data end */
    unsigned length_sectors;
    unsigned playtime; /* estimated playtime in secs based on timecode */
    mpeg_info_t mpeg_info;
  } mpeg_tracks[100]; /* fixme */
  int mpeg_tracks_num;

  /* custom files */
  struct cust_file {
    char *iso_pathname;
    VcdDataSource *file;
    int raw_flag;

    uint32_t size;
    uint32_t start_extent;
    uint32_t sectors;
    
    struct cust_file *next;
  } *custom_files;

  /* dictionary */
  struct _dict_t *buf_dict;

  /* fixme -- vcd files */
  char info_vcd_buf[ISO_BLOCKSIZE];
  char entries_vcd_buf[ISO_BLOCKSIZE];
  char lot_vcd_buf[ISO_BLOCKSIZE*LOT_VCD_SIZE];
  char psd_vcd_buf[ISO_BLOCKSIZE]; /* fixme */

  char tracks_svd_buf[ISO_BLOCKSIZE];
  char search_dat_buf[ISO_BLOCKSIZE]; /* fixme */

  uint32_t info_sec;
  uint32_t entries_sec;
  uint32_t lot_secs;
  uint32_t psd_sec;
  uint32_t psd_size;
  
  uint32_t tracks_sec;
  uint32_t search_secs;

  unsigned dirs_size;

  VcdSalloc *iso_bitmap;

  VcdDirectory *dir;

  /* state info */
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
