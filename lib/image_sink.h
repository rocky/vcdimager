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

#ifndef __VCD_IMAGE_SINK_H__
#define __VCD_IMAGE_SINK_H__

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <cdio/cdio.h>
#include <libvcd/types.h>

/* Private includes */
#include "data_structures.h"
#include "stream.h"

/* VcdImageSink ( --> image writer) */

typedef struct _VcdImageSink VcdImageSink;

typedef struct {
  uint32_t lsn;
  enum {
    VCD_CUE_TRACK_START = 1, /* INDEX 0 -> 1 transition, TOC entry */
    VCD_CUE_PREGAP_START,    /* INDEX = 0 start */
    VCD_CUE_SUBINDEX,        /* INDEX++; sub-index */
    VCD_CUE_END,             /* lead-out start */
    VCD_CUE_LEADIN,          /* lead-in start */
  } type;
} vcd_cue_t;

typedef struct {
  int (*set_cuesheet) (void *user_data, const VcdList *vcd_cue_list);
  int (*write) (void *user_data, const void *buf, lsn_t lsn);
  void (*free) (void *user_data);
  int (*set_arg) (void *user_data, const char key[], const char value[]);
} vcd_image_sink_funcs;

VcdImageSink *
vcd_image_sink_new (void *user_data, const vcd_image_sink_funcs *funcs);

void
vcd_image_sink_destroy (VcdImageSink *obj);

int
vcd_image_sink_set_cuesheet (VcdImageSink *obj, const VcdList *vcd_cue_list);

int
vcd_image_sink_write (VcdImageSink *obj, void *buf, lsn_t lsn);

/*!
  Set the arg "key" with "value" in the target device.
*/
int
vcd_image_sink_set_arg (VcdImageSink *obj, const char key[], 
			const char value[]);

VcdImageSink * vcd_image_sink_new_nrg (void);
VcdImageSink * vcd_image_sink_new_bincue (void);
VcdImageSink * vcd_image_sink_new_cdrdao (void);

#endif /* __VCD_IMAGE_SINK_H__ */
