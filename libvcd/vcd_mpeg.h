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

#ifndef _MPEG_H_
#define _MPEG_H_

#include "vcd_types.h"

typedef enum {
  MPEG_INVALID = 0,
  MPEG_VIDEO,
  MPEG_AUDIO,
  MPEG_NULL,
  MPEG_END,
  MPEG_UNKNOWN
} mpeg_type_t;


typedef enum {
  MPEG_NORM_OTHER,
  MPEG_NORM_PAL,
  MPEG_NORM_NTSC,
  MPEG_NORM_FILM,
  MPEG_SVHS_PAL,
  MPEG_SVHS_NTSC,
} mpeg_norm_t;

typedef struct {
  unsigned hsize;
  unsigned vsize;
  double aratio;
  double frate;
  unsigned bitrate;
  unsigned vbvsize;

  mpeg_norm_t norm;
} mpeg_info_t;

mpeg_type_t 
mpeg_type(const void *mpeg_block);

int
mpeg_analyze_start_seq(const void *packet, mpeg_info_t *info);

#endif /* _MPEG_H_ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
