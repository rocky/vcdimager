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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
  MPEG_TYPE_INVALID = 0,
  MPEG_TYPE_VIDEO,
  MPEG_TYPE_AUDIO,
  MPEG_TYPE_NULL,
  MPEG_TYPE_END,
  MPEG_TYPE_UNKNOWN
} mpeg_type_t;

typedef enum {
  MPEG_NORM_OTHER,
  MPEG_NORM_PAL,
  MPEG_NORM_NTSC,
  MPEG_NORM_FILM,
  MPEG_NORM_PAL_S,
  MPEG_NORM_NTSC_S
} mpeg_norm_t;

typedef enum {
  MPEG_VERS_INVALID = 0,
  MPEG_VERS_MPEG1,
  MPEG_VERS_MPEG2
} mpeg_vers_t;

typedef struct 
{
  unsigned hsize;
  unsigned vsize;
  double aratio;
  double frate;
  unsigned bitrate;
  unsigned vbvsize;
  int constrained_flag;
  
  mpeg_vers_t version;
  mpeg_norm_t norm;
} mpeg_info_t;

typedef struct 
{
  mpeg_type_t type;

  mpeg_vers_t version;

#if ANON_UNION
  union {
#endif

    struct
    {
      unsigned hsize;
      unsigned vsize;
      double aratio;
      double frate;
      unsigned bitrate;
      unsigned vbvsize;
      int constrained_flag;
    
      double timecode; /* linear timecode given in seconds */
      int rel_timecode; /* relative timecode offset to last gop given
                           in frames */

      /* flags to be set if corresponding element was present */
      int gop_flag:1; /* if set then timecode was set too */

      int i_frame_flag:1; /* if set then rel_timecode was set too */
      
    } video;
#ifdef ANON_UNION
  };
#endif
} mpeg_type_info_t;

mpeg_type_t 
vcd_mpeg_get_type (const void *packet, mpeg_type_info_t *type_info);

double
vcd_mpeg_get_timecode (const void *packet);

int
vcd_mpeg_get_info (const void *packet, mpeg_info_t *mpeg_info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MPEG_H_ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
