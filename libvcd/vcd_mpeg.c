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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libvcd/vcd_mpeg.h>
#include <libvcd/vcd_bitvec.h>
#include <libvcd/vcd_util.h>

static const char _rcsid[] = "$Id$";

#define MPEG_START_CODE_PATTERN  ((uint32_t) 0x00000100)
#define MPEG_START_CODE_MASK     ((uint32_t) 0xffffff00)

#define MPEG_PICTURE_CODE        ((uint32_t) 0x00000100)
/* [...slice codes... 0x1a7] */

#define MPEG_USER_CODE           ((uint32_t) 0x000001b2)
#define MPEG_SEQUENCE_CODE       ((uint32_t) 0x000001b3)
#define MPEG_EXT_CODE            ((uint32_t) 0x000001b5)
#define MPEG_SEQ_END_CODE        ((uint32_t) 0x000001b7)
#define MPEG_GOP_CODE            ((uint32_t) 0x000001b8)
#define MPEG_PROGRAM_END_CODE    ((uint32_t) 0x000001b9)
#define MPEG_PACK_HEADER_CODE    ((uint32_t) 0x000001ba)
#define MPEG_SYSTEM_HEADER_CODE  ((uint32_t) 0x000001bb)
#define MPEG_OGT_CODE            ((uint32_t) 0x000001bd)
#define MPEG_PAD_CODE            ((uint32_t) 0x000001be)

#define MPEG_AUDIO_C0_CODE       ((uint32_t) 0x000001c0) /* default */
#define MPEG_AUDIO_C1_CODE       ((uint32_t) 0x000001c1) /* 2nd audio stream id (dual channel) */
#define MPEG_AUDIO_C2_CODE       ((uint32_t) 0x000001c2) /* 3rd audio stream id (surround sound) */

#define MPEG_VIDEO_E0_CODE       ((uint32_t) 0x000001e0) /* motion */
#define MPEG_VIDEO_E1_CODE       ((uint32_t) 0x000001e1) /* lowres still */
#define MPEG_VIDEO_E2_CODE       ((uint32_t) 0x000001e2) /* hires still */

#define PICT_TYPE_I             1
#define PICT_TYPE_P             2
#define PICT_TYPE_B             3
#define PICT_TYPE_D             4

struct {
  mpeg_norm_t norm;
  unsigned hsize;
  unsigned vsize;
  int frate_idx;
} const static norm_table[] = {
  { MPEG_NORM_FILM,   352, 240, 1 },
    { MPEG_NORM_PAL,    352, 288, 3 },
    { MPEG_NORM_NTSC,   352, 240, 4 },
    { MPEG_NORM_PAL_S,  480, 576, 3 },
    { MPEG_NORM_NTSC_S, 480, 480, 4 },
    { MPEG_NORM_OTHER, }
  };

const static double frame_rates[16] =  {
    0.00, 23.976, 24.0, 25.0, 
    29.97, 30.0,  50.0, 59.94, 
    60.00, 00.0, 
  };

static inline bool
_start_code_p (uint32_t code)
{
  return (code & MPEG_START_CODE_MASK) == MPEG_START_CODE_PATTERN;
};

static inline int 
_vid_streamid_idx (uint8_t streamid)
{
  switch (streamid | MPEG_START_CODE_PATTERN)
    {
    case MPEG_VIDEO_E0_CODE:
      return 0;
      break;

    case MPEG_VIDEO_E1_CODE:
      return 1;
      break;

    case MPEG_VIDEO_E2_CODE:
      return 2;
      break;

    default:
      vcd_assert_not_reached ();
      break;
    }
  
  return -1;
}

static inline int 
_aud_streamid_idx (uint8_t streamid)
{
  switch (streamid | MPEG_START_CODE_PATTERN)
    {
    case MPEG_AUDIO_C0_CODE:
      return 0;
      break;

    case MPEG_AUDIO_C1_CODE:
      return 1;
      break;

    case MPEG_AUDIO_C2_CODE:
      return 2;
      break;

    default:
      vcd_assert_not_reached ();
      break;
    }
  
  return -1;
}


static void
_parse_sequence_header (uint8_t streamid, const void *buf, 
			VcdMpegStreamCtx *state)
{
  int offset = 0;
  unsigned hsize, vsize, aratio, frate, brate, bufsize, constr;
  const uint8_t *data = buf;
  int vid_idx = _vid_streamid_idx (streamid);

  const double aspect_ratios[16] = 
    { 
      0.0000, 1.0000, 0.6735, 0.7031,
      0.7615, 0.8055, 0.8437, 0.8935,
      0.9375, 0.9815, 1.0255, 1.0695,
      1.1250, 1.1575, 1.2015, 0.0000
    };

  if (state->stream.shdr[vid_idx].seen) /* we have it already */
    return;
  
  if (!state->stream.first_shdr)
    state->stream.first_shdr = vid_idx + 1;

  /* ++KSW (Keith White), for stills with both stream ids, the higher
     resolution takes precedence */
  if (state->stream.first_shdr == 2 && vid_idx == 2)
    {
      vcd_debug ("got still image with hires sequence header "
                 "coming later. declaring this still to be hires.");
      state->stream.first_shdr = vid_idx + 1;
    }

  hsize = vcd_bitvec_read_bits (data, &offset, 12);

  vsize = vcd_bitvec_read_bits (data, &offset, 12);

  aratio = vcd_bitvec_read_bits (data, &offset, 4);

  frate = vcd_bitvec_read_bits (data, &offset, 4);

  brate = vcd_bitvec_read_bits (data, &offset, 18);
              
  /* marker bit const == 1 */
  (void) vcd_bitvec_read_bits (data, &offset, 1);

  bufsize = vcd_bitvec_read_bits (data, &offset, 10);

  constr = vcd_bitvec_read_bits (data, &offset, 1);

  /* skip intra quantizer matrix */

  if (vcd_bitvec_read_bits (data, &offset, 1))
    offset += 64 << 3;

  /* skip non-intra quantizer matrix */

  if (vcd_bitvec_read_bits (data, &offset, 1))
    offset += 64 << 3;
  
  state->stream.shdr[vid_idx].hsize = hsize;
  state->stream.shdr[vid_idx].vsize = vsize;
  state->stream.shdr[vid_idx].aratio = aspect_ratios[aratio];
  state->stream.shdr[vid_idx].frate = frame_rates[frate];
  state->stream.shdr[vid_idx].bitrate = 400*brate;
  state->stream.shdr[vid_idx].vbvsize = bufsize * 16 * 1024; 
  state->stream.shdr[vid_idx].constrained_flag = (constr != 0);

  state->stream.shdr[vid_idx].seen = true;
}


static void
_parse_gop_header (uint8_t streamid, const void *buf, 
		   VcdMpegStreamCtx *state)
{
  const uint8_t *data = buf;
  int offset = 0;
  
  bool drop_flag; 
  /* bool close_gop; */
  /* bool broken_link; */

  unsigned hour, minute, second, frame;

  drop_flag = vcd_bitvec_read_bits(data, &offset, 1) != 0;
  
  hour = vcd_bitvec_read_bits(data, &offset, 5);

  minute = vcd_bitvec_read_bits(data, &offset, 6);

  (void) vcd_bitvec_read_bits(data, &offset, 1); 

  second = vcd_bitvec_read_bits(data, &offset, 6);

  frame = vcd_bitvec_read_bits(data, &offset, 6);

  /* close_gop = vcd_bitvec_read_bits(data, &offset, 1) != 0; */

  /* broken_link = vcd_bitvec_read_bits(data, &offset, 1) != 0; */

  state->packet.gop = true;
  state->packet.gop_timecode.h = hour;
  state->packet.gop_timecode.m = minute;
  state->packet.gop_timecode.s = second;
  state->packet.gop_timecode.f = frame;
}

static void
_analyze_video_pes (uint8_t streamid, const uint8_t *buf, int len, bool only_pts,
		    VcdMpegStreamCtx *state)
{
  int pos = 0;
  int sequence_header_pos = -1;
  int gop_header_pos = -1;
  int ipicture_header_pos = -1;
  bool _has_pts = false;
  bool _has_dts = false;
  int64_t pts = 0;
  int mpeg_ver = 0;
  int _pts_pos = 0;

  vcd_assert (_vid_streamid_idx (streamid) != -1);

  switch (vcd_bitvec_peek_bits (buf, 0, 2))
    {
    case 0:
    case 1:
    case 3: /* seems to be v1 as well... */
      mpeg_ver = 1;
      break;

    case 2: /* %10 */
      mpeg_ver = 2;
      break;

    default:
      vcd_assert_not_reached ();
      break;
    }

  if (mpeg_ver == 2)
    {
      int pos2 = 8;

      switch (vcd_bitvec_peek_bits (buf, pos2, 2))
        {
        case 2:
          _has_pts = true;
          _pts_pos = 24;
          break;
          
        case 3:
          _has_dts = _has_pts = true;
          _pts_pos = 24;
          break;

        case 0:
          break;

        default:
          vcd_debug ("?? v2");
          break;
        }

      pos2 += 8;

      pos = vcd_bitvec_read_bits (buf, &pos2, 8);
      
      pos += pos2 >> 3;
    }
  else if (mpeg_ver == 1)
    {
      int pos2 = 0;

      while (((pos2 + 8) < (len << 3)) && vcd_bitvec_peek_bits (buf, pos2, 8) == 0xff)
        pos2 += 8;

      switch (vcd_bitvec_peek_bits (buf, pos2, 2)) 
        {
        case 0: 
          /* %00 */
          break;

        case 1:
          /* %01 */
          pos2 += 16;
          break;

        case 2:
        case 3:
          /* %1x */
          vcd_warn ("heah??");
          break;
        }

      switch (vcd_bitvec_peek_bits (buf, pos2, 4))
        {
        case 2:
          _has_pts = true;
          _pts_pos = pos2;
          break;
          
        case 3:
          _has_dts = _has_pts = true;
          _pts_pos = pos2;
          break;

        case 0:
        case 0xf:
          break;

        default:
          break;
        }

      pos = pos2 >> 3;
    }
  else
    vcd_assert_not_reached ();

  if (_has_pts)
    {
      int pos2 = _pts_pos;
      int marker;
      double pts2;
      
      marker = vcd_bitvec_read_bits (buf, &pos2, 4);

      //vcd_warn ("%d @%d", marker, _pts_pos);

      vcd_assert (marker == 2 || marker == 3);

      pts = vcd_bitvec_read_bits (buf, &pos2, 3);

      pos2++;

      pts <<= 15;
      pts |= vcd_bitvec_read_bits (buf, &pos2, 15);
      
      pos2++;

      pts <<= 15;
      pts |= vcd_bitvec_read_bits (buf, &pos2, 15);

      pos2++;

      pts2 = (double) pts / 90000.0;

      if (!state->stream.seen_pts)
        {
          state->stream.max_pts = state->stream.min_pts = pts2;
          state->stream.seen_pts = true;
        }
      else
        {
          state->stream.max_pts = MAX (state->stream.max_pts, pts2);
          state->stream.min_pts = MIN (state->stream.min_pts, pts2);
        }

      state->packet.has_pts = true;
      state->packet.pts = pts2;
    }

  /* if only pts extraction was needed, we are done here... */
  if (only_pts)
    return;
  
  while (pos + 4 <= len)
    {
      uint32_t code = vcd_bitvec_peek_bits32 (buf, pos << 3);

      if (!_start_code_p (code))
	{
	  pos++;
	  continue;
	}

      switch (code)
	{
	case MPEG_PICTURE_CODE:
	  pos += 4;
	  
          if (vcd_bitvec_peek_bits (buf, (pos << 3) + 10, 3) == 1)
            ipicture_header_pos = pos;
	  break;
	  
	case MPEG_SEQUENCE_CODE:
	  pos += 4;
	  sequence_header_pos = pos;
          _parse_sequence_header (streamid, buf + pos, state);
	  break;

	case MPEG_GOP_CODE:
	  pos += 4;
          if (pos + 4 > len)
            break;
	  gop_header_pos = pos;
	  _parse_gop_header (streamid, buf + pos, state);
	  state->packet.gop = true;
	  break;

	case MPEG_EXT_CODE:
	case MPEG_USER_CODE:
	default:
	  pos += 4;
	  break;
	}
    }

  /* decide whether this packet qualifies as access point */
  state->packet.aps = APS_NONE; /* paranoia */

  if (_has_pts && ipicture_header_pos != -1)
    {
      enum aps_t _aps_type = APS_NONE;

      switch (mpeg_ver)
        {
        case 1:
        case 2:
          if (sequence_header_pos != -1
              && sequence_header_pos < gop_header_pos
              && gop_header_pos < ipicture_header_pos)
            _aps_type = APS_SGI;
          else if (gop_header_pos != 1 
                   && gop_header_pos < ipicture_header_pos)
            _aps_type = APS_GI;
          else
            _aps_type = APS_I;

          break;

        default:
          break;
        }

      if (_aps_type) 
        {
          double pts2 = (double) pts / 90000.0;
          int vid_idx = _vid_streamid_idx (streamid);

          if (state->stream.last_aps_pts[vid_idx] > pts2)
            vcd_warn ("aps pts seems out of order (actual pts %f, last seen pts %f) -- ignoring this aps",
                      pts2, state->stream.last_aps_pts[vid_idx]);
          else
            {
              state->packet.aps = _aps_type;
              state->packet.aps_pts = pts2;
              state->stream.last_aps_pts[vid_idx] = pts2;
            }
        }
    }
}

static void
_register_streamid (uint8_t streamid, VcdMpegStreamCtx *state)
{
  const uint32_t code = MPEG_START_CODE_PATTERN | streamid;

  switch (code)
    {
    case MPEG_VIDEO_E0_CODE:
    case MPEG_VIDEO_E1_CODE: 
    case MPEG_VIDEO_E2_CODE: 
      state->packet.video[_vid_streamid_idx (streamid)] = true;
      break;

    case MPEG_AUDIO_C0_CODE:
    case MPEG_AUDIO_C1_CODE:
    case MPEG_AUDIO_C2_CODE:
      state->packet.audio[_aud_streamid_idx (streamid)] = true;
      break;

    case MPEG_PAD_CODE:
      state->packet.padding = true;
      break;

    case MPEG_OGT_CODE:
      state->packet.ogt = true;
      break;

    case MPEG_SYSTEM_HEADER_CODE: 
      state->packet.system_header = true;
      break;
    }
}

int
vcd_mpeg_parse_packet (const void *_buf, unsigned buflen, bool parse_pes,
                       VcdMpegStreamCtx *ctx)
{
  const uint8_t *buf = _buf;
  int pos;

  vcd_assert (buf != NULL);
  vcd_assert (ctx != NULL);
  
  /* clear packet info */
  memset (&(ctx->packet), 0, sizeof (ctx->packet));

  ctx->stream.packets++;

  for (pos = 0; pos < buflen && !buf[pos]; pos++);

  if (pos == buflen)
    {
      ctx->packet.zero = true;
      return buflen;
    }

  if (vcd_bitvec_peek_bits32 (buf, 0) != MPEG_PACK_HEADER_CODE)
    vcd_error ("mpeg scan: pack header code expected, but 0x%8.8x found (buflen = %d)",
               vcd_bitvec_peek_bits32 (buf, 0), buflen);

  pos = 0;
  while (pos + 4 <= buflen)
    {
      uint32_t code = vcd_bitvec_peek_bits32 (buf, pos << 3);

      if (!code)
	{
	  pos += (pos + 4 == buflen) ? 4 : 2;
	  continue;
	}

      if (!_start_code_p (code))
	{
	  pos++;
	  continue;
	}

      switch (code)
	{
	  uint16_t size;

	case MPEG_PACK_HEADER_CODE:
	  if (pos)
	    return pos;

	  pos += 4;
	  
	  switch (vcd_bitvec_peek_bits(buf, pos << 3, 2))
	    {
	    default:
	      vcd_warn ("packet not recognized as either version 1 or 2 (%d)" 
                        " -- assuming v1", 
			vcd_bitvec_peek_bits(buf, pos << 3, 2));
	    case 0x0: /* %00 mpeg1 */
	      if (!ctx->stream.version)
		ctx->stream.version = MPEG_VERS_MPEG1;

	      if (ctx->stream.version != MPEG_VERS_MPEG1)
		vcd_warn ("mixed mpeg versions?");
              pos += 8;
	      break;
	    case 0x1: /* %11 mpeg2 */
	      if (!ctx->stream.version)
		ctx->stream.version = MPEG_VERS_MPEG2;

	      if (ctx->stream.version != MPEG_VERS_MPEG2)
		vcd_warn ("mixed mpeg versions?");
              pos += 10;
	      break;
	    }
	  
	  break;

	case MPEG_SYSTEM_HEADER_CODE:
	case MPEG_PAD_CODE:
	case MPEG_OGT_CODE:
	case MPEG_VIDEO_E0_CODE:
	case MPEG_VIDEO_E1_CODE:
	case MPEG_VIDEO_E2_CODE:
        case MPEG_AUDIO_C0_CODE:
        case MPEG_AUDIO_C1_CODE:
        case MPEG_AUDIO_C2_CODE:
	  pos += 4;
	  size = vcd_bitvec_peek_bits16 (buf, pos << 3);
	  pos += 2;
	  
	  if (pos + size > buflen)	  
            {
              vcd_warn ("packet length beyond buffer" 
                        " (pos = %d + size = %d > buflen = %d) "
                        "-- stream may be truncated or packet length > 2324 bytes!",
                        pos, size, buflen);
              return 0;
            }

          _register_streamid (code & 0xff, ctx);

	  switch (code)
	    {
	    case MPEG_SYSTEM_HEADER_CODE: 
              _register_streamid (buf[pos + 6], ctx);
              break;

	    case MPEG_VIDEO_E0_CODE:
	    case MPEG_VIDEO_E1_CODE: 
	    case MPEG_VIDEO_E2_CODE: 
              _analyze_video_pes (code & 0xff, buf + pos, size, !parse_pes, ctx);
              break;
	    }

          { 
            int n;
            for (n = 0; n < 3; n++)
              {
                if (ctx->packet.audio[n])
                  ctx->stream.audio[n] = true;

                if (ctx->packet.video[n])
                  ctx->stream.video[n] = true;
              }
          }

	  pos += size;
	  break;
	  
	case MPEG_PROGRAM_END_CODE:
	  ctx->packet.pem = true;
	  pos += 4;
	  break;

	case MPEG_PICTURE_CODE:
	  pos += 3;
	  break;

	default:
	  vcd_debug ("unexpected start code 0x%8.8x", code);
	  pos += 4;
	  break;
	}
    }

  if (pos != buflen)
    vcd_debug ("pos != buflen (%d != %d)", pos, buflen); /* fixme? */

  return buflen;
}

mpeg_norm_t 
vcd_mpeg_get_norm (unsigned hsize, unsigned vsize, double frate)
{
  int i;

        
  for (i = 0; norm_table[i].norm != MPEG_NORM_OTHER;i++)
    if (norm_table[i].hsize == hsize
        && norm_table[i].vsize == vsize
        && frame_rates[norm_table[i].frate_idx] == frate)
      break;

  return norm_table[i].norm;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
