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

/* libvcd main header */

#ifndef __VCD_H__
#define __VCD_H__

#include <libvcd/vcd_stream.h>
#include <libvcd/vcd_mpeg_stream.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* opaque data structure representing a VideoCD object */
typedef struct _VcdObj VcdObj;

/* enum defining supported VideoCD types */
typedef enum
{
  VCD_TYPE_INVALID = 0,
  /* VCD_TYPE_VCD1, */
  VCD_TYPE_VCD11,
  VCD_TYPE_VCD2,
  VCD_TYPE_SVCD
  /* VCD_TYPE_HQVCD */
}
vcd_type_t;

/* allocates and initializes a new VideoCD object */
VcdObj *
vcd_obj_new (vcd_type_t vcd_type);

/* VideoCD parameters */
typedef enum
{
  VCD_PARM_INVALID = 0,
  VCD_PARM_VOLUME_LABEL,        /* char *  max length 32 */
  VCD_PARM_ALBUM_ID,            /* char *  max length 16 */
  VCD_PARM_VOLUME_COUNT,        /* unsigned */
  VCD_PARM_VOLUME_NUMBER,       /* unsigned */
  VCD_PARM_APPLICATION_ID,      /* char *  max length 128 */
  VCD_PARM_SEC_TYPE,            /* int     (2336|2352)   */
  VCD_PARM_BROKEN_SVCD_MODE     /* bool */
}
vcd_parm_t;

/* sets VideoCD parameter */
void 
vcd_obj_set_param (VcdObj *obj, vcd_parm_t param, const void *arg);

/* add custom files; if raw_flag set, the data source has to include a
   mode2 subheader, and thus needs to be a multiple of 2336 byte blocksize */
int
vcd_obj_add_file (VcdObj *obj, const char iso_pathname[],
                  VcdDataSource *file, bool raw_flag);

int
vcd_obj_add_dir (VcdObj *obj, const char iso_pathname[]);

/* this is for actually adding mpeg items to VCD, returns 
   a negative value for error..  */

int 
vcd_obj_append_sequence_play_item (VcdObj *obj, VcdMpegSource *mpeg_source, 
                                   const char item_id[]);

int 
vcd_obj_add_sequence_entry (VcdObj *obj, const char sequence_id[], 
                            double entry_time, const char entry_id[]);

int 
vcd_obj_append_segment_play_item (VcdObj *obj, VcdMpegSource *mpeg_source, 
                                  const char item_id[]);

/* removes item (sequence, entry, segment, ...) by id, returns
   negative value on error */
int 
vcd_obj_remove_item (VcdObj *obj, const char id[]);

/* returns image size in sectors */
long 
vcd_obj_get_image_size (VcdObj *obj);

/* this one is to be called when every parameter has been set and the
   image is about to be written. returns sectors to be written... */
long 
vcd_obj_begin_output (VcdObj *obj);

/* callback hook called every few (>|<) iterations, if it returns a value != 0
   the writing process gets aborted */
typedef struct
{
  long sectors_written;
  long total_sectors;
  int in_track;
  int total_tracks;
}
progress_info_t;

typedef int (*progress_callback_t) (const progress_info_t *progress_info,
                                    void *user_data);

/* writes the actual bin image file; a return value != 0 means the
   action was aborted by user or some other error has occured... */
int
vcd_obj_write_image (VcdObj *obj, VcdDataSink *bin_file,
                     progress_callback_t callback, void *user_data);

/* spit out cue file; return != 0 --> error */
int
vcd_obj_write_cuefile (VcdObj *obj, VcdDataSink *cue_file,
                       const char bin_fname[]);

/* this should be called writing the bin and/or cue file is done---even if 
   an error occurred */
void 
vcd_obj_end_output (VcdObj *obj);

/* destructor for VideoCD objects; call this to destory a VideoCD
   object created by vcd_obj_new () */
void 
vcd_obj_destroy (VcdObj *obj);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VCD_H__ */

/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
