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

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "vcd_mpeg_stream.h"

#include "vcd_util.h"
#include "vcd_data_structures.h"
#include "vcd_mpeg.h"
#include "vcd_logging.h"

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

const static double frame_rates[16] = 
  {
    0.00, 23.976, 24.0, 25.0, 
    29.97, 30.0,  50.0, 59.94, 
    60.00, 00.0, 
  };

struct _VcdMpegSource
{
  VcdDataSource *data_source;

  bool scanned;
  
  /* _get_packet cache */
  unsigned _read_pkt_pos;
  unsigned _read_pkt_no;

  struct vcd_mpeg_source_info info;
};

#define _bitvec_peek_bits32(bitvec, offset) _bitvec_peek_bits ((bitvec), (offset), 32)
#define _bitvec_peek_bits16(bitvec, offset) _bitvec_peek_bits ((bitvec), (offset), 16)

static uint32_t 
_bitvec_peek_bits (const uint8_t bitvec[], int offset, int bits)
{
  uint32_t result = 0;
  int i;

  assert (bits > 0 && bits <= 32);

  if ((offset & 7) == 0 && (bits & 7) == 0) /* optimization */
    for (i = offset; i < (offset + bits); i+= 8)
      {
        result <<= 8;
        result |= bitvec[i/8];
      }
  else /* general case */
    for (i = offset; i < (offset + bits); i++)
      {
        result <<= 1;
        if (_vcd_bit_set_p (bitvec[i / 8], 7 - (i % 8)))
          result |= 0x1;
      }
  
  return result;
}

static inline uint32_t 
_bitvec_read_bits (const uint8_t bitvec[], int *offset, int bits)
{
  uint32_t retval = _bitvec_peek_bits (bitvec, *offset, bits);
  
  *offset += bits;

  return retval;
}

static inline int
_align (int value, int boundary)
{
  if (value % boundary)
    value += (boundary - (value % boundary));

  return value;
}

static inline bool
_start_code_p (uint32_t code)
{
  return (code & MPEG_START_CODE_MASK) == MPEG_START_CODE_PATTERN;
};

/*
 * access functions
 */

VcdMpegSource *
vcd_mpeg_source_new (VcdDataSource *mpeg_file)
{
  VcdMpegSource *new_obj;
  
  assert (mpeg_file != NULL);

  new_obj = _vcd_malloc (sizeof (VcdMpegSource));

  new_obj->data_source = mpeg_file;
  new_obj->scanned = false;

  return new_obj;
}

void
vcd_mpeg_source_destroy (VcdMpegSource *obj, bool destroy_file_obj)
{
  assert (obj != NULL);

  if (destroy_file_obj)
    vcd_data_source_destroy (obj->data_source);

  _vcd_list_free (obj->info.aps_list, true);

  free (obj);
}

const struct vcd_mpeg_source_info *
vcd_mpeg_source_get_info (VcdMpegSource *obj)
{
  assert (obj != NULL);

  assert (obj->scanned);

  return &(obj->info);
}

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
      assert (0);
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
      assert (0);
      break;
    }
  
  return -1;
}

struct _analyze_state
{
  struct {
    bool video[3];
    bool audio[3];

    bool ogt;
    bool padding;
    bool pem;
    bool zero;
    bool system_header;

    bool aps;
    double aps_pts;

    bool gop;
    struct {
      uint8_t h, m, s, f;
    } gop_timecode;
  } packet;

  struct {
    unsigned packets;

    int first_shdr;
    struct {
      bool seen;
      unsigned hsize;
      unsigned vsize;
      double aratio;
      double frate;
      unsigned bitrate;
      unsigned vbvsize;
      bool constrained_flag;
    } shdr[3];

    bool video[3];
    bool audio[3];

    mpeg_vers_t version;

    bool seen_pts;
    double min_pts;
    double max_pts;

    double last_aps_pts[3];
  } stream;
};

static void
_parse_sequence_header (uint8_t streamid, const void *buf, 
			struct _analyze_state *state)
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

  hsize = _bitvec_read_bits (data, &offset, 12);

  vsize = _bitvec_read_bits (data, &offset, 12);

  aratio = _bitvec_read_bits (data, &offset, 4);

  frate = _bitvec_read_bits (data, &offset, 4);

  brate = _bitvec_read_bits (data, &offset, 18);
              
  /* marker bit const == 1 */
  (void) _bitvec_read_bits (data, &offset, 1);

  bufsize = _bitvec_read_bits (data, &offset, 10);

  constr = _bitvec_read_bits (data, &offset, 1);

  /* skip intra quantizer matrix */

  if (_bitvec_read_bits (data, &offset, 1))
    offset += 64 << 3;

  /* skip non-intra quantizer matrix */

  if (_bitvec_read_bits (data, &offset, 1))
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
		   struct _analyze_state *state)
{
  const uint8_t *data = buf;
  int offset = 0;
  
  bool drop_flag; 
  /* bool close_gop; */
  /* bool broken_link; */

  unsigned hour, minute, second, frame;

  drop_flag = _bitvec_read_bits(data, &offset, 1) != 0;
  
  hour = _bitvec_read_bits(data, &offset, 5);

  minute = _bitvec_read_bits(data, &offset, 6);

  (void) _bitvec_read_bits(data, &offset, 1); 

  second = _bitvec_read_bits(data, &offset, 6);

  frame = _bitvec_read_bits(data, &offset, 6);

  /* close_gop = _bitvec_read_bits(data, &offset, 1) != 0; */

  /* broken_link = _bitvec_read_bits(data, &offset, 1) != 0; */

  state->packet.gop = true;
  state->packet.gop_timecode.h = hour;
  state->packet.gop_timecode.m = minute;
  state->packet.gop_timecode.s = second;
  state->packet.gop_timecode.f = frame;
}

static void
_analyze_video_pes (uint8_t streamid, const uint8_t *buf, int len,
		    struct _analyze_state *state)
{
  int pos;
  int sequence_header_pos = -1;
  int gop_header_pos = -1;
  int ipicture_header_pos = -1;
  bool _has_pts = false;
  bool _has_dts = false;
  int64_t pts = 0;
  int mpeg_ver = 0;
  int _pts_pos = 0;

  assert (_vid_streamid_idx (streamid) != -1);

  switch (_bitvec_peek_bits (buf, 0, 2))
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
      assert (0);
      break;
    }

  if (mpeg_ver == 2)
    {
      int pos2 = 8;

      switch (_bitvec_peek_bits (buf, pos2, 2))
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

      pos = _bitvec_read_bits (buf, &pos2, 8);
      
      pos += pos2 >> 3;
    }
  else if (mpeg_ver == 1)
    {
      int pos2 = 0;

      while (((pos2 + 8) < (len << 3)) && _bitvec_peek_bits (buf, pos2, 8) == 0xff)
        pos2 += 8;

      switch (_bitvec_peek_bits (buf, pos2, 2)) 
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

      switch (_bitvec_peek_bits (buf, pos2, 4))
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
    assert (0);

  if (_has_pts)
    {
      int pos2 = _pts_pos;
      int marker;
      double pts2;
      
      marker = _bitvec_read_bits (buf, &pos2, 4);

      //vcd_warn ("%d @%d", marker, _pts_pos);

      assert (marker == 2 || marker == 3);

      pts = _bitvec_read_bits (buf, &pos2, 3);

      pos2++;

      pts <<= 15;
      pts |= _bitvec_read_bits (buf, &pos2, 15);
      
      pos2++;

      pts <<= 15;
      pts |= _bitvec_read_bits (buf, &pos2, 15);

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
    }

  while (pos + 4 <= len)
    {
      uint32_t code = _bitvec_peek_bits32 (buf, pos << 3);

      if (!_start_code_p (code))
	{
	  pos++;
	  continue;
	}

      switch (code)
	{
	case MPEG_PICTURE_CODE:
	  pos += 4;
	  
          if (_bitvec_peek_bits (buf, (pos << 3) + 10, 3) == 1)
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
  state->packet.aps = false; /* paranoia */

  if (ipicture_header_pos != -1)
    {
      bool _is_aps = false;

      switch (mpeg_ver)
        {
        case 1:
          /* for mpeg1 require just gop (before Iframe) */
          _is_aps = (gop_header_pos != 1 
                     && gop_header_pos < ipicture_header_pos);
          break;
        case 2:
          /* for mpeg2 we need 3 headers in place... */
          _is_aps = (sequence_header_pos != -1
                     && sequence_header_pos < gop_header_pos
                     && gop_header_pos < ipicture_header_pos);
            break;
        default:
          _is_aps = false;
          break;
        }

      if (_is_aps && !_has_pts)
        _is_aps = false; /* it's sad... */

      if (_is_aps) /* if still aps */
        {
          double pts2 = (double) pts / 90000.0;
          int vid_idx = _vid_streamid_idx (streamid);

          state->packet.aps = true;
          state->packet.aps_pts = pts2;

          if (state->stream.last_aps_pts[vid_idx] > pts2)
            vcd_warn ("aps pts seems out of order (actual pts %f, last seen pts %f)",
                      pts2, state->stream.last_aps_pts[vid_idx]);

          state->stream.last_aps_pts[vid_idx] = pts2;
        }
    }
}

static void
_register_streamid (uint8_t streamid, struct _analyze_state *state)
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

static int
_analyze (const uint8_t *buf, int len, bool analyze_pes, 
	  struct _analyze_state *state)
{
  int pos;

  assert (buf != NULL);
  assert (state != NULL);
  
  memset (&(state->packet), 0, sizeof (state->packet));

  state->stream.packets++;

  for (pos = 0; pos < len && !buf[pos]; pos++);

  if (pos == len)
    {
      state->packet.zero = true;
      return len;
    }

  if (_bitvec_peek_bits32 (buf, 0) != MPEG_PACK_HEADER_CODE)
    vcd_error ("mpeg scan: pack header code expected, but 0x%8.8x found (buflen = %d)",
               _bitvec_peek_bits32 (buf, 0), len);

  pos = 0;
  while (pos + 4 <= len)
    {
      uint32_t code = _bitvec_peek_bits32 (buf, pos << 3);

      if (!code)
	{
	  pos += (pos + 4 == len) ? 4 : 2;
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
	  
	  switch (_bitvec_peek_bits(buf, pos << 3, 2))
	    {
	    default:
	      vcd_warn ("packet not recognized as either version 1 or 2 (%d) -- assuming v1", 
			_bitvec_peek_bits(buf, pos << 3, 2));
	    case 0x0: /* %00 mpeg1 */
	      if (!state->stream.version)
		state->stream.version = MPEG_VERS_MPEG1;

	      if (state->stream.version != MPEG_VERS_MPEG1)
		vcd_warn ("mixed mpeg versions?");
              pos += 8;
	      break;
	    case 0x1: /* %11 mpeg2 */
	      if (!state->stream.version)
		state->stream.version = MPEG_VERS_MPEG2;

	      if (state->stream.version != MPEG_VERS_MPEG2)
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
	  size = _bitvec_peek_bits16 (buf, pos << 3);
	  pos += 2;
	  
	  if (pos + size > len)	  
            {
              vcd_error ("packet length beyond buffer...");
              return 0;
            }

          _register_streamid (code & 0xff, state);

	  switch (code)
	    {
	    case MPEG_SYSTEM_HEADER_CODE: 
              _register_streamid (buf[pos + 6], state);
              break;

	    case MPEG_VIDEO_E0_CODE:
	    case MPEG_VIDEO_E1_CODE: 
	    case MPEG_VIDEO_E2_CODE: 
	      if (analyze_pes)
		_analyze_video_pes (code & 0xff, buf + pos, size, state);
              break;
	    }

          { 
            int n;
            for (n = 0; n < 3; n++)
              {
                if (state->packet.audio[n])
                  state->stream.audio[n] = true;

                if (state->packet.video[n])
                  state->stream.video[n] = true;
              }
          }

	  pos += size;
	  break;
	  
	case MPEG_PROGRAM_END_CODE:
	  state->packet.pem = true;
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

  if (pos != len)
    vcd_debug ("pos != len (%d != %d)", pos, len); /* fixme? */

  return len;
}

static mpeg_norm_t 
_get_norm (unsigned hsize, unsigned vsize, double frate)
{
  struct {
    mpeg_norm_t norm;
    unsigned hsize;
    unsigned vsize;
    int frate_idx;
  } const static norm_table[] =
    {
      { MPEG_NORM_FILM,   352, 240, 1 },
      { MPEG_NORM_PAL,    352, 288, 3 },
      { MPEG_NORM_NTSC,   352, 240, 4 },
      { MPEG_NORM_PAL_S,  480, 576, 3 },
      { MPEG_NORM_NTSC_S, 480, 480, 4 },
      { MPEG_NORM_OTHER, }
    };

  int i;

        
  for (i = 0; norm_table[i].norm != MPEG_NORM_OTHER;i++)
    if (norm_table[i].hsize == hsize
        && norm_table[i].vsize == vsize
        && frame_rates[norm_table[i].frate_idx] == frate)
      break;

  return norm_table[i].norm;
}

long
vcd_mpeg_source_stat (VcdMpegSource *obj)
{
  assert (obj != NULL);
  assert (!obj->scanned);
  
  return obj->info.packets * 2324;
}

void
vcd_mpeg_source_scan (VcdMpegSource *obj)
{
  unsigned length = 0;
  unsigned pos = 0;
  unsigned pno = 0;
  struct _analyze_state state;
  VcdList *aps_list = 0;
  VcdListNode *n;
  bool _warned_padding = false;
  bool _pal = false;

  assert (obj != NULL);

  if (obj->scanned)
    {
      vcd_debug ("already scanned... not rescanning");
      return;
    }

  assert (!obj->scanned);

  memset (&state, 0, sizeof (state));

  vcd_data_source_seek (obj->data_source, 0);
  length = vcd_data_source_stat (obj->data_source);

  aps_list = _vcd_list_new ();

  while (pos < length)
    {
      char buf[2324] = { 0, };
      int read_len = MIN (sizeof (buf), (length - pos));
      int pkt_len;

      vcd_data_source_read (obj->data_source, buf, read_len, 1);

      pkt_len = _analyze (buf, read_len, true, &state);

      if (state.packet.aps)
	{
	  struct aps_data *_data = _vcd_malloc (sizeof (struct aps_data));

	  _data->packet_no = pno;
	  _data->timestamp = state.packet.aps_pts;

	  _vcd_list_append (aps_list, _data);
	}

      pos += pkt_len;
      pno++;

      if (pkt_len != read_len)
        {
          if (!_warned_padding)
            {
              vcd_warn ("mpeg stream will be padded on the fly -- hope that's ok for you!");
              _warned_padding = true;
            }

          vcd_data_source_seek (obj->data_source, pos);
        }
    }

  vcd_data_source_close (obj->data_source);

  assert (pos == length);

  obj->info.audio_c0 = state.stream.audio[0];
  obj->info.audio_c1 = state.stream.audio[1];
  obj->info.audio_c2 = state.stream.audio[2];

  obj->info.video_e0 = state.stream.video[0];
  obj->info.video_e1 = state.stream.video[1];
  obj->info.video_e2 = state.stream.video[2];

  if (!state.stream.audio[0] && !state.stream.audio[1] && !state.stream.audio[2])
    obj->info.audio_type = MPEG_AUDIO_NOSTREAM;
  else if (state.stream.audio[0] && !state.stream.audio[1] && !state.stream.audio[2])
    obj->info.audio_type = MPEG_AUDIO_1STREAM;
  else if (state.stream.audio[0] && state.stream.audio[1] && !state.stream.audio[2])
    obj->info.audio_type = MPEG_AUDIO_2STREAM;
  else if (state.stream.audio[0] && !state.stream.audio[1] && state.stream.audio[2])
    obj->info.audio_type = MPEG_AUDIO_EXT_STREAM;
  else
    {
      vcd_warn ("unsupported audio stream aggregation encountered! (c0: %d, c1: %d, c2: %d)",
                state.stream.audio[0], state.stream.audio[1], state.stream.audio[2]);
      obj->info.audio_type = MPEG_AUDIO_NOSTREAM;
    }

  obj->info.packets = state.stream.packets;
  obj->scanned = true;

  obj->info.playing_time = state.stream.max_pts - state.stream.min_pts;

  if (state.stream.min_pts)
    vcd_debug ("pts start offset %f (max pts = %f)", 
               state.stream.min_pts, state.stream.max_pts);

  vcd_debug ("playing time %f", obj->info.playing_time);

  for(n = _vcd_list_begin (aps_list);
      n; n = _vcd_list_node_next (n))
    {
      struct aps_data *_data = _vcd_list_node_data (n);
      
      _data->timestamp -= state.stream.min_pts; /* pts offset */
    }

  obj->info.aps_list = aps_list;

  {
    int vid_idx = state.stream.first_shdr;
    
    if (vid_idx)
      {
        vid_idx--;

        obj->info.hsize = state.stream.shdr[vid_idx].hsize;
        obj->info.vsize = state.stream.shdr[vid_idx].vsize;
        obj->info.aratio = state.stream.shdr[vid_idx].aratio;
        obj->info.frate = state.stream.shdr[vid_idx].frate;
        obj->info.bitrate = state.stream.shdr[vid_idx].bitrate;
        obj->info.vbvsize = state.stream.shdr[vid_idx].vbvsize;
        obj->info.constrained_flag = state.stream.shdr[vid_idx].constrained_flag;
      }
  }  

  obj->info.version = state.stream.version;

  obj->info.norm = _get_norm (obj->info.hsize,
			      obj->info.vsize,
			      obj->info.frate);

  _pal = (obj->info.vsize == 576 || obj->info.vsize == 288);

  switch (state.stream.first_shdr)
    {
    case 0:
      obj->info.video_type = MPEG_VIDEO_NOSTREAM;
      break;
    case 1:
      obj->info.video_type = _pal ? MPEG_VIDEO_PAL_MOTION : MPEG_VIDEO_NTSC_MOTION;
      break;
    case 2:
      obj->info.video_type = _pal ? MPEG_VIDEO_PAL_STILL : MPEG_VIDEO_NTSC_STILL;
      break;
    case 3:
      obj->info.video_type = _pal ? MPEG_VIDEO_PAL_STILL2 : MPEG_VIDEO_NTSC_STILL2;
      break;
    }
}

int
vcd_mpeg_source_get_packet (VcdMpegSource *obj, unsigned long packet_no,
			    void *packet_buf, struct vcd_mpeg_packet_flags *flags)
{
  unsigned length;
  unsigned pos;
  unsigned pno;
  struct _analyze_state state;

  assert (obj != NULL);
  assert (obj->scanned);
  assert (packet_buf != NULL);

  if (packet_no >= obj->info.packets)
    {
      vcd_error ("invalid argument");
      return -1;
    }

  if (packet_no < obj->_read_pkt_no)
    {
      vcd_warn ("rewinding...");
      obj->_read_pkt_no = 0;
      obj->_read_pkt_pos = 0;
    }

  memset (&state, 0, sizeof (state));

  pos = obj->_read_pkt_pos;
  pno = obj->_read_pkt_no;
  length = vcd_data_source_stat (obj->data_source);

  vcd_data_source_seek (obj->data_source, pos);

  while (pos < length)
    {
      char buf[2324] = { 0, };
      int read_len = MIN (sizeof (buf), (length - pos));
      int pkt_len;
      
      vcd_data_source_read (obj->data_source, buf, read_len, 1);

      pkt_len = _analyze (buf, read_len, false, &state);

      if (pno == packet_no)
	{
	  obj->_read_pkt_pos = pos;
	  obj->_read_pkt_no = pno;

	  memset (packet_buf, 0, 2324);
	  memcpy (packet_buf, buf, pkt_len);

          if (flags)
            {
              memset (flags, 0, sizeof (struct vcd_mpeg_packet_flags));

              if (state.packet.video[0] 
                  || state.packet.video[1]
                  || state.packet.video[2])
                {
                  flags->type = PKT_TYPE_VIDEO;
                  flags->video_e0 = state.packet.video[0];
                  flags->video_e1 = state.packet.video[1];
                  flags->video_e2 = state.packet.video[2];
                }
              else if (state.packet.audio[0] 
                       || state.packet.audio[1]
                       || state.packet.audio[2])
                {
                  flags->type = PKT_TYPE_AUDIO;
                  flags->audio_c0 = state.packet.audio[0];
                  flags->audio_c1 = state.packet.audio[1];
                  flags->audio_c2 = state.packet.audio[2];
                }
              else if (state.packet.zero)
                flags->type = PKT_TYPE_ZERO;
              else if (state.packet.ogt)
                flags->type = PKT_TYPE_OGT;
              else if (state.packet.system_header || state.packet.padding)
                flags->type = PKT_TYPE_EMPTY;

              if (state.packet.pem)
                flags->pem = true;
            }

	  return 0;
	}

      pos += pkt_len;
      pno++;

      if (pkt_len != read_len)
	vcd_data_source_seek (obj->data_source, pos);
    }

  assert (pos == length);

  vcd_error ("shouldnt be reached...");

  return -1;
}

void
vcd_mpeg_source_close (VcdMpegSource *obj)
{
  assert (obj != NULL);

  vcd_data_source_close (obj->data_source);
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
