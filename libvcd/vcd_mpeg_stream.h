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

#ifndef __VCD_MPEG_STREAM__
#define __VCD_MPEG_STREAM__

#include "vcd_types.h"
#include "vcd_stream.h"
#include "vcd_data_structures.h"

#define MPEG_PACKET_SIZE 2324

typedef struct _VcdMpegSource VcdMpegSource;

/* used in APS list */

struct aps_data
{
  uint32_t packet_no;
  double timestamp;
};

/* enums */

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
  MPEG_VERS_MPEG1 = 1,
  MPEG_VERS_MPEG2 = 2
} mpeg_vers_t;

typedef enum {
  MPEG_AUDIO_NOSTREAM = 0,
  MPEG_AUDIO_1STREAM = 1,
  MPEG_AUDIO_2STREAM = 2,
  MPEG_AUDIO_EXT_STREAM = 3
} mpeg_audio_t;

typedef enum {
  MPEG_VIDEO_NOSTREAM = 0,
  MPEG_VIDEO_NTSC_STILL = 1,
  MPEG_VIDEO_NTSC_STILL2 = 2,
  MPEG_VIDEO_NTSC_MOTION = 3,

  MPEG_VIDEO_PAL_STILL = 5,
  MPEG_VIDEO_PAL_STILL2 = 6,
  MPEG_VIDEO_PAL_MOTION = 7
} mpeg_video_t;

/* mpeg stream info */

struct vcd_mpeg_source_info
{
  unsigned hsize;
  unsigned vsize;
  double aratio;
  double frate;
  unsigned bitrate;
  unsigned vbvsize;
  bool constrained_flag;

  bool audio_c0;
  bool audio_c1;
  bool audio_c2;

  bool video_e0;
  bool video_e1;
  bool video_e2;

  mpeg_video_t video_type;
  mpeg_audio_t audio_type;
  mpeg_vers_t version;
  mpeg_norm_t norm; 

  double playing_time;
  uint32_t packets;
  VcdList *aps_list;
};

/* mpeg packet info */

struct vcd_mpeg_packet_flags
{
  enum {
    PKT_TYPE_INVALID = 0,
    PKT_TYPE_VIDEO,
    PKT_TYPE_AUDIO,
    PKT_TYPE_OGT,
    PKT_TYPE_ZERO,
    PKT_TYPE_EMPTY
  } type;

  bool audio_c0;
  bool audio_c1;
  bool audio_c2;

  bool video_e0;
  bool video_e1;
  bool video_e2;

  bool pem;
};

/* access functions */

VcdMpegSource *
vcd_mpeg_source_new (VcdDataSource *mpeg_file);

/* scan the mpeg file... needed to be called only once */
void
vcd_mpeg_source_scan (VcdMpegSource *obj);

/* gets the packet at given position */
int
vcd_mpeg_source_get_packet (VcdMpegSource *obj, unsigned long packet_no,
			    void *packet_buf, struct vcd_mpeg_packet_flags *flags);

void
vcd_mpeg_source_close (VcdMpegSource *obj);

const struct vcd_mpeg_source_info *
vcd_mpeg_source_get_info (VcdMpegSource *obj);

long
vcd_mpeg_source_stat (VcdMpegSource *obj);

void
vcd_mpeg_source_destroy (VcdMpegSource *obj, bool destroy_file_obj);

#endif /* __VCD_MPEG_STREAM__ */

/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
