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

#include <stdio.h>

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

#define MARKER(buf, offset) \
 vcd_assert (vcd_bitvec_read_bits (buf, offset, 1) == 1)

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

/* used for SCR, PTS and DTS */
static inline uint64_t
_parse_timecode (const void *buf, int *offset)
{
  uint64_t _retval;

  _retval = vcd_bitvec_read_bits (buf, offset, 3);

  MARKER (buf, offset);

  _retval <<= 15;
  _retval |= vcd_bitvec_read_bits (buf, offset, 15);
      
  MARKER (buf, offset);

  _retval <<= 15;
  _retval |= vcd_bitvec_read_bits (buf, offset, 15);

  MARKER (buf, offset);

  return _retval;
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

static inline void
_check_scan_data (const char str[], const msf_t *msf,
                  VcdMpegStreamCtx *state)
{
  char tmp[16];

  if (state->stream.scan_data_warnings > VCD_MPEG_SCAN_DATA_WARNS)
    return;

  if (state->stream.scan_data_warnings == VCD_MPEG_SCAN_DATA_WARNS)
    {
      vcd_warn ("mpeg user scan data: from now on, scan information "
                "data errors will not be reported anymore---consider"
                " enabling the 'update scan offsets' option, "
                "if it is not enabled already!");
      state->stream.scan_data_warnings++;
      return;
    }      

  if (msf->m == 0xff
      && msf->s == 0xff
      && msf->f == 0xff)
    return;

  if ((msf->s & 0x80) == 0
      || (msf->f & 0x80) == 0)
    {
      snprintf (tmp, sizeof (tmp), "%.2x:%.2x.%.2x", msf->m, msf->s, msf->f);

      vcd_warn ("mpeg user scan data: msb of second or frame field "
                "not set for '%s': [%s]", str, tmp);

      state->stream.scan_data_warnings++;

      return;
    }

  if ((msf->m >> 4) > 9
      || ((0x80 ^ msf->s) >> 4) > 9      
      || ((0x80 ^ msf->f) >> 4) > 9
      || (msf->m & 0xf) > 9
      || (msf->s & 0xf) > 9
      || (msf->f & 0xf) > 9)
    {
      snprintf (tmp, sizeof (tmp), "%.2x:%.2x.%.2x",
                msf->m, 0x80 ^ msf->s, 0x80 ^ msf->f);

      vcd_warn ("mpeg user scan data: one or more BCD fields out of range "
                "for '%s': [%s]", str, tmp);

      state->stream.scan_data_warnings++;
    }
}

static void
_parse_user_data (uint8_t streamid, const void *buf, unsigned len, 
                  unsigned offset,
                  VcdMpegStreamCtx *state)
{
  unsigned pos = 0;
  struct {
    uint8_t tag GNUC_PACKED;
    uint8_t len GNUC_PACKED;
    uint8_t data[EMPTY_ARRAY_SIZE] GNUC_PACKED;
  } const *udg = buf;

/*   if (state->stream.version != MPEG_VERS_MPEG2) */
/*     return; */

  while (pos + 2 < len && udg->tag)
    {
      if (pos + udg->len >= len)
        break;

      if (udg->len < 2)
        break;

      switch (udg->tag)
        {
        case 0x10: /* scan information */
          {
            struct vcd_mpeg_scan_data_t *usdi = (void *) udg;
            vcd_assert (sizeof (struct vcd_mpeg_scan_data_t) == 14);
            
            if (usdi->len != 14)
              {
                vcd_warn ("invalid user scan data length (%d != 14)", usdi->len);
                break;
              }

            vcd_assert (usdi->len == 14);
            _check_scan_data ("previous_I_offset", &usdi->prev_ofs, state);
            _check_scan_data ("next_I_offset    ", &usdi->next_ofs, state);
            _check_scan_data ("backward_I_offset", &usdi->back_ofs, state);
            _check_scan_data ("forward_I_offset ", &usdi->forw_ofs, state);
          
            state->packet.scan_data_ptr = usdi;
            state->stream.scan_data++;
          }
          break;

        case 0x11: /* closed caption data */
          vcd_debug ("caption data seen -- not supported yet (len = %d)", udg->len);
          break;

        default:
          break;
        }

      pos += udg->len;
      vcd_assert (udg->len >= 2);
      udg = (void *) &udg->data[udg->len - 2];
    }
  
  vcd_assert (pos <= len);
}

static int
_analyze_pes_header (const uint8_t *buf, int len,
                     VcdMpegStreamCtx *state)
{
  bool _has_pts = false;
  bool _has_dts = false;
  int64_t pts = 0;
  mpeg_vers_t pes_mpeg_ver = 0;

  int pos;

  if (vcd_bitvec_peek_bits (buf, 0, 2) == 2) /* %10 - ISO13818-1 */
    {
      int pos2 = 0;

      pes_mpeg_ver = MPEG_VERS_MPEG2;

      pos2 += 2;

      pos2 += 2; /* PES_scrambling_control */
      pos2++; /* PES_priority */
      pos2++; /* data_alignment_indicator */
      pos2++; /* copyright */
      pos2++; /* original_or_copy */

      switch (vcd_bitvec_read_bits (buf, &pos2, 2)) /* PTS_DTS_flags */
        {
        case 2: /* %10 */
          _has_pts = true;
          break;
          
        case 3: /* %11 */
          _has_dts = _has_pts = true;
          break;

        default:
          /* NOOP */
          break;
        }

      pos2++; /* ESCR_flag */
      
      pos2++; /* */
      pos2++; /* */
      pos2++; /* */
      pos2++; /* */

      pos2++; /* PES_extension_flag */

      pos = vcd_bitvec_read_bits (buf, &pos2, 8); /* PES_header_data_length */
      pos += pos2 >> 3;

      if (_has_pts && _has_dts)
        {
          vcd_assert (vcd_bitvec_peek_bits (buf, pos2, 4) == 3); /* %0011 */
          pos2 += 4;

          pts = _parse_timecode (buf, &pos2);

          vcd_assert (vcd_bitvec_peek_bits (buf, pos2, 4) == 1); /* %0001 */
          pos2 += 4;

          /* dts = */ _parse_timecode (buf, &pos2);
        }
      else if (_has_pts)
        {
          vcd_assert (vcd_bitvec_peek_bits (buf, pos2, 4) == 2); /* %0010 */
          pos2 += 4;

          pts = _parse_timecode (buf, &pos2);
        }
    }
  else /* ISO11172-1 */
    {
      int pos2 = 0;

      pes_mpeg_ver = MPEG_VERS_MPEG1;

      /* get rid of stuffing bytes */
      while (((pos2 + 8) < (len << 3)) 
             && vcd_bitvec_peek_bits (buf, pos2, 8) == 0xff)
        pos2 += 8;

      if (vcd_bitvec_peek_bits (buf, pos2, 2) == 1) /* %01 */
        {
          pos2 += 2;

          pos2++;     /* STD_buffer_scale */
          pos2 += 13; /* STD_buffer_size */
        }

      switch (vcd_bitvec_peek_bits (buf, pos2, 4)) 
        {
        case 0x2: /* %0010 */
          pos2 += 4;
          _has_pts = true;

          pts = _parse_timecode (buf, &pos2);
          break;

        case 0x3: /* %0011 */
          pos2 += 4;

          _has_dts = _has_pts = true;
          pts = _parse_timecode (buf, &pos2);

          vcd_assert (vcd_bitvec_peek_bits (buf, pos2, 4) == 1); /* %0001 */
          pos2 += 4;

          /* dts = */ _parse_timecode (buf, &pos2);
          break;

        case 0x0: /* %0000 */
          vcd_assert (vcd_bitvec_peek_bits (buf, pos2, 8) == 0x0f);
          pos2 += 8;
          break;

        default:
          vcd_error ("error in mpeg1 stream");
          break;
        }

      pos = pos2 >> 3;
    }

  if (_has_pts)
    {
      double pts2;
      
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

  if (state->stream.version != pes_mpeg_ver)
    vcd_warn ("pack header mpeg version does not match pes header mpeg version");

  return pos;
}

static void
_analyze_audio_pes (uint8_t streamid, const uint8_t *buf, int len, bool only_pts,
		    VcdMpegStreamCtx *state)
{
  vcd_assert (_aud_streamid_idx (streamid) != -1);

  _analyze_pes_header (buf, len, state);

  /* if only pts extraction was needed, we are done here... */
  if (only_pts)
    return;
  
}

static void
_analyze_video_pes (uint8_t streamid, const uint8_t *buf, int len, bool only_pts,
		    VcdMpegStreamCtx *state)
{
  int pos = 0;
  int sequence_header_pos = -1;
  int gop_header_pos = -1;
  int ipicture_header_pos = -1;
  
  int bits;

  vcd_assert (_vid_streamid_idx (streamid) != -1);

  _analyze_pes_header (buf, len, state);

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

	case MPEG_USER_CODE:
	  pos += 4;
          if (pos + 4 > len)
            break;
          _parse_user_data (streamid, buf + pos, len - pos, pos, state);
	  break;

	case MPEG_EXT_CODE:
	default:
	  pos += 4;
	  break;
	}
    }

  /* decide whether this packet qualifies as access point */
  state->packet.aps = APS_NONE; /* paranoia */

  if (state->packet.has_pts 
      && ipicture_header_pos != -1)
    {
      enum aps_t _aps_type = APS_NONE;

      switch (state->stream.version)
        {
        case MPEG_VERS_MPEG1:
        case MPEG_VERS_MPEG2:
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
          const double pts2 = state->packet.pts;
          const int vid_idx = _vid_streamid_idx (streamid);

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

static void
_analyze_system_header (const uint8_t *buf, int len, 
                        VcdMpegStreamCtx *state)
{
  int bitpos = 0;

  MARKER (buf, &bitpos);

  bitpos += 22; /* rate_bound */

  MARKER (buf, &bitpos);

  bitpos += 6; /* audio_bound */ 

  bitpos++; /* fixed_flag */
  bitpos++; /* CSPS_flag */
  bitpos++; /* system_audio_lock_flag */
  bitpos++; /* system_video_lock_flag */

  MARKER (buf, &bitpos);

  bitpos += 5; /* video_bound */ 

  bitpos += 1; /* packet_rate_restriction_flag -- only ISO 13818-1 */
  bitpos += 7; /* reserved */

  while (vcd_bitvec_peek_bits (buf, bitpos, 1) == 1 
         && bitpos <= (len << 3))
    {
      const uint8_t stream_id = vcd_bitvec_read_bits (buf, &bitpos, 8);

      bitpos += 2; /* %11 */

      bitpos++; /* P-STD_buffer_bound_scale */
      bitpos += 13; /* P-STD_buffer_size_bound */

      _register_streamid (stream_id, state);
    }

  vcd_assert (bitpos <= (len << 3));
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

  /* verify the packet begins with a pack header */
  if (vcd_bitvec_peek_bits32 (buf, 0) != MPEG_PACK_HEADER_CODE)
    {
      const uint32_t _code = vcd_bitvec_peek_bits32 (buf, 0);

      vcd_warn ("mpeg scan: pack header code (0x%8.8x) expected, "
                "but 0x%8.8x found (buflen = %d)",
                MPEG_PACK_HEADER_CODE, _code, buflen);

      ctx->stream.packets--;

      if (!ctx->stream.packets)
        {
          if (_code == MPEG_SEQUENCE_CODE)
            vcd_warn ("...this looks like a elementary video stream"
                      " but a multiplexed program stream was required.");

          if ((0xfff00000 & _code) == 0xfff00000)
            vcd_warn ("...this looks like a elementary audio stream"
                      " but a multiplexed program stream was required.");

          if (_code == 0x52494646)
            vcd_warn ("...this looks like a RIFF header"
                      " but a plain multiplexed program stream was required.");
        }
      else if (_code == MPEG_PROGRAM_END_CODE)
        vcd_warn ("...PEM (program end marker) found instead of pack header;"
                  " should be in last 4 bytes of pack");

      return 0;
    }

  /* take a look at the pack header */
  pos = 0;

  while (pos + 4 <= buflen)
    {
      uint32_t code = vcd_bitvec_peek_bits32 (buf, pos << 3);

      /* skip zero bytes... */
      if (!code)
	{
	  pos += (pos + 4 == buflen) ? 4 : 2;
	  continue;
	}

      /* continue until start code seen */
      if (!_start_code_p (code))
	{
	  pos++;
	  continue;
	}

      switch (code)
	{
	  uint16_t size;
          int bits;
          int bitpos;
          
	case MPEG_PACK_HEADER_CODE:
	  if (pos)
	    return pos;

	  pos += 4;

          bitpos = pos << 3;
          bits = vcd_bitvec_peek_bits (buf, bitpos, 4);

          if (bits == 0x2) /* %0010 ISO11172-1 */
            {
              uint64_t _scr;
              uint32_t _muxrate;

              bitpos += 4;

              if (!ctx->stream.version)
                ctx->stream.version = MPEG_VERS_MPEG1;

              if (ctx->stream.version != MPEG_VERS_MPEG1)
                vcd_warn ("mixed mpeg versions?");

              _scr = _parse_timecode (buf, &bitpos);

              bitpos++; /* marker */

              _muxrate = vcd_bitvec_read_bits (buf, &bitpos, 22);

              bitpos++; /* marker */

              vcd_assert (bitpos % 8 == 0);
              pos = bitpos >> 3;

              ctx->packet.scr = _scr;
              ctx->packet.muxrate = _muxrate * 50;
            }
          else if (bits >> 2 == 0x1) /* %01xx ISO13818-1 */
            {
              uint64_t _scr;
              uint32_t _muxrate;
              int tmp;

              bitpos += 2;

              if (!ctx->stream.version)
                ctx->stream.version = MPEG_VERS_MPEG2;

              if (ctx->stream.version != MPEG_VERS_MPEG2)
                vcd_warn ("mixed mpeg versions?");

              _scr = _parse_timecode (buf, &bitpos);

              _scr *= 300;
              _scr += vcd_bitvec_read_bits (buf, &bitpos, 9); /* SCR ext */

              bitpos++; /* marker */
              
              _muxrate = vcd_bitvec_read_bits (buf, &bitpos, 22);
              
              bitpos++; /* marker */
              bitpos++; /* marker */

              bitpos += 5; /* reserved */

              tmp = vcd_bitvec_read_bits (buf, &bitpos, 3) << 3;

              bitpos += tmp;

              vcd_assert (bitpos % 8 == 0);
              pos = bitpos >> 3;

              ctx->packet.scr = _scr;
              ctx->packet.muxrate = _muxrate * 50;
            }
          else
            {
              vcd_warn ("packet not recognized as either version 1 or 2 (%d)" 
                        " -- assuming v1", bits);
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
              ctx->stream.packets--;
              return 0;
            }

          _register_streamid (code & 0xff, ctx);

	  switch (code)
	    {
	    case MPEG_SYSTEM_HEADER_CODE: 
              _analyze_system_header (buf + pos, size, ctx);
              break;

	    case MPEG_VIDEO_E0_CODE:
	    case MPEG_VIDEO_E1_CODE: 
	    case MPEG_VIDEO_E2_CODE: 
              _analyze_video_pes (code & 0xff, buf + pos, size, !parse_pes, ctx);
              break;

            case MPEG_AUDIO_C0_CODE:
            case MPEG_AUDIO_C1_CODE:
            case MPEG_AUDIO_C2_CODE:
              _analyze_audio_pes (code & 0xff, buf + pos, size, !parse_pes, ctx);
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
