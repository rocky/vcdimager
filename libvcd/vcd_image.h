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

#ifndef __VCD_IMAGE_H__
#define __VCD_IMAGE_H__

#include <libvcd/vcd_types.h>
#include <libvcd/vcd_data_structures.h>

/* VcdImageSink ( --> image reader) */

typedef struct _VcdImageSource VcdImageSource;

typedef struct {
  int (*read_mode2_sector) (void *user_data, void *buf, uint32_t lsn, bool mode2raw);
  uint32_t (*stat_size) (void *user_data);
  void (*free) (void *user_data);
} vcd_image_source_funcs;

VcdImageSource *
vcd_image_source_new (void *user_data, const vcd_image_source_funcs *funcs);

void
vcd_image_source_destroy (VcdImageSource *obj);

int
vcd_image_source_read_mode2_sector (VcdImageSource *obj, void *buf, uint32_t lsn, bool mode2raw);

uint32_t
vcd_image_source_stat_size (VcdImageSource *obj);

/* VcdImageSink ( --> image writer) */

typedef struct _VcdImageSink VcdImageSink;

typedef struct {
  uint32_t lsn;
  enum {
    VCD_CUE_TRACK_START,
    VCD_CUE_PREGAP_START,
    VCD_CUE_END,
  } type;
} vcd_cue_t;

typedef struct {
  int (*set_cuesheet) (void *user_data, const VcdList *vcd_cue_list);
  int (*write) (void *user_data, void *buf, uint32_t lsn);
  void (*free) (void *user_data);
} vcd_image_sink_funcs;

VcdImageSink *
vcd_image_sink_new (void *user_data, const vcd_image_sink_funcs *funcs);

void
vcd_image_sink_destroy (VcdImageSink *obj);

int
vcd_image_sink_set_cuesheet (VcdImageSink *obj, const VcdList *vcd_cue_list);

int
vcd_image_sink_write (VcdImageSink *obj, void *buf, uint32_t lsn);

#endif /* __VCD_IMAGE_H__ */
