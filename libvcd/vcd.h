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


#ifndef __VCD_H__
#define __VCD_H__

#include "vcd_stream.h"
#include "vcd_mpeg.h"

/* ...opaque data structure */
typedef struct _VcdObj VcdObj;

/* methods... */

/* first you create an VCD object... */
typedef enum {
  VCD_TYPE_INVALID = 0,
  /* VCD_TYPE_VCD1, */
  /* VCD_TYPE_VCD11, */
  VCD_TYPE_VCD2,
  VCD_TYPE_SVCD
  /* VCD_TYPE_HQVCD */
} vcd_type_t;

VcdObj*
vcd_obj_new (vcd_type_t vcd_type);

/* then you can (optionally) set some vcd parameters... */
typedef enum {
  VCD_PARM_INVALID = 0,
  VCD_PARM_VOLUME_LABEL,      /* char *  max length 32 */
  VCD_PARM_SEC_TYPE           /* int     (2336/2352)   */
} vcd_parm_t;

void
vcd_obj_set_param (VcdObj *obj, vcd_parm_t param, const void *arg);

/* optionally call this for enabling CD-i support 
   -- fixme make it more generic */
/* void
 * vcd_obj_set_cdi_input (VcdObj *obj, VcdDataSource *image_file,
 * VcdDataSource *text_file, VcdDataSource *vcd_file);
 */

/* this one is for actually adding mpeg tracks to VCD, returns item
   id, or a negative value for error.. */
int
vcd_obj_append_mpeg_track (VcdObj *obj, VcdDataSource *mpeg_file);

/* for removing mpeg tracks by item id */
void
vcd_obj_remove_mpeg_track (VcdObj *obj, int track_id);

/* use this to retrieve a mpeg info structure... */
const mpeg_info_t*
vcd_obj_get_mpeg_info (VcdObj *obj, int track_id);

/* returns image size in sectors of actual compilation */
long
vcd_obj_get_image_size (VcdObj *obj);

/* this one is to be called when every parameter has been set and the
   image is about to be written. returns sectors to be written... */
long
vcd_obj_begin_output (VcdObj *obj);

/* call back called every few iterations, if returned TRUE the process
   gets aborted */

typedef int (*progress_callback_t)(long sectors_written, long total_sectors, 
                                   int in_track, int total_tracks, 
                                   void *user_data);

/* writes the actual bin image file...
   if return value != 0 -- aborted or other error */
int
vcd_obj_write_image (VcdObj *obj, VcdDataSink *bin_file,
                     progress_callback_t callback, void *user_data);

/* spit out cue file */
void
vcd_obj_write_cuefile (VcdObj *obj, VcdDataSink *cue_file,
                       const char bin_fname[]);

/* call this when finished writing, or aborting loop */
void
vcd_obj_end_output (VcdObj *obj);

/* when done with writing just destroy (almost) every trace... */
void 
vcd_obj_destroy (VcdObj *obj);

#endif /* __VCD_H__ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
