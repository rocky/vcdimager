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

struct _analyze_state
{
  struct {
    bool video;
    bool audio;

    bool audio_c0;
    bool audio_c1;
    bool audio_c2;

    bool ogt;
    bool padding;
    bool pem;
    bool zero;
    bool system_header;

    bool aps;
    double aps_timecode;

    bool gop;
    double gop_timecode;

    unsigned i_pict_rel_frame;
    unsigned last_rel_frame;

  } packet;

  struct {
    unsigned packets;

    bool video_info;
    unsigned hsize;
    unsigned vsize;
    double aratio;
    double frate;
    unsigned bitrate;
    unsigned vbvsize;
    bool constrained_flag;

    bool audio_info;
    bool audio_c0;
    bool audio_c1;
    bool audio_c2;

    mpeg_vers_t version;

    bool gop_seen;
    double min_gop_timecode;
    double max_gop_timecode;
    double max_pict_timecode;
  } stream;
};

static void
_parse_sequence_header (const void *buf, 
			struct _analyze_state *state)
{
  int offset = 0;
  unsigned hsize, vsize, aratio, frate, brate, bufsize, constr;
  const uint8_t *data = buf;

  const double aspect_ratios[16] = 
    { 
      0.0000, 1.0000, 0.6735, 0.7031,
      0.7615, 0.8055, 0.8437, 0.8935,
      0.9375, 0.9815, 1.0255, 1.0695,
      1.1250, 1.1575, 1.2015, 0.0000
    };
  
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
  
  state->stream.hsize = hsize;
  state->stream.vsize = vsize;
  state->stream.aratio = aspect_ratios[aratio];
  state->stream.frate = frame_rates[frate];
  state->stream.bitrate = 400*brate;
  state->stream.vbvsize = bufsize * 16 * 1024; 
  state->stream.constrained_flag = (constr != 0);
}


static void
_parse_gop_header (const void *buf, 
		   struct _analyze_state *state)
{
  const uint8_t *data = buf;
  int offset = 0;

  bool drop_flag;
  bool close_gop;
  bool broken_link;

  unsigned hour = 0, minute = 0, second = 0, frame = 0;
  double timecode = 0;

  drop_flag = _bitvec_read_bits(data, &offset, 1) != 0;

  hour = _bitvec_read_bits(data, &offset, 5);

  minute = _bitvec_read_bits(data, &offset, 6);

  (void) _bitvec_read_bits(data, &offset, 1); 

  second = _bitvec_read_bits(data, &offset, 6);

  frame = _bitvec_read_bits(data, &offset, 6);

  close_gop = _bitvec_read_bits(data, &offset, 1) != 0;

  broken_link = _bitvec_read_bits(data, &offset, 1) != 0;

  timecode = hour;
  timecode *= 60;
  timecode += minute;
  timecode *= 60;
  timecode += second;

  timecode += (double) frame / state->stream.frate;

  state->packet.gop = true;
  state->packet.gop_timecode = timecode;

  if (timecode && state->stream.min_gop_timecode > timecode)
    vcd_debug ("timecode seen smaller than min timecode (%f < %f)",
               timecode, state->stream.min_gop_timecode);

  if (!state->stream.gop_seen)
    {
      state->stream.min_gop_timecode = timecode;
      state->stream.gop_seen = true;
    }
  
  if (timecode && state->stream.max_gop_timecode > timecode)
    vcd_debug ("timecode seen smaller than max timecode... (%f < %f)",
               timecode, state->stream.max_gop_timecode);

  state->stream.max_gop_timecode = 
    MAX (timecode, state->stream.max_gop_timecode);
}

static void
_analyze_video_pes (uint32_t code, const uint8_t *buf, int len,
		    struct _analyze_state *state)
{
  int pos;
  int sequence_header_pos = -1;
  int gop_header_pos = -1;
  int ipicture_header_pos = -1;

  assert (code == MPEG_VIDEO_E0_CODE 
          || code == MPEG_VIDEO_E1_CODE
          || code == MPEG_VIDEO_E2_CODE);

  pos = 0;
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
	  
	  {
	    double timecode;
	    int rel_frame = 
	      _bitvec_peek_bits (buf, (pos << 3), 10);

	    state->packet.last_rel_frame = 
	      MAX (rel_frame, state->packet.last_rel_frame);

	    timecode = rel_frame;
	    timecode /= state->stream.frate;
	    timecode += state->stream.max_gop_timecode;

	    state->stream.max_pict_timecode = 
	      MAX (state->stream.max_pict_timecode, timecode);

	    if (_bitvec_peek_bits (buf, (pos << 3) + 10, 3) == 1)
	      {
		ipicture_header_pos = pos;
		
		state->packet.i_pict_rel_frame = rel_frame;
	      }
	  }
	  break;
	  
	case MPEG_SEQUENCE_CODE:
	  pos += 4;
	  sequence_header_pos = pos;

	  if (!state->stream.video_info)
	    {
	      _parse_sequence_header (buf + pos, state);
	      state->stream.video_info = true;
	    }

	  break;

	case MPEG_GOP_CODE:
	  pos += 4;
          if (pos + 4 > len)
            break;
	  gop_header_pos = pos;
	  _parse_gop_header (buf + pos, state);
	  state->packet.gop = true;
	  break;

	case MPEG_EXT_CODE:
	case MPEG_USER_CODE:
	default:
	  pos += 4;
	  break;
	}
    }

  if (sequence_header_pos != -1
      && sequence_header_pos < gop_header_pos
      && gop_header_pos < ipicture_header_pos)
    {
      double timecode;

      state->packet.aps = true;

      timecode = state->packet.i_pict_rel_frame;
      timecode /= state->stream.frate;

      timecode += state->packet.gop_timecode;

      state->packet.aps_timecode = timecode;
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
	    default:
	      vcd_warn ("packet not recognized as either version 1 or 2 (%d)", 
			_bitvec_peek_bits(buf, pos << 3, 2));
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

	  switch (code)
	    {
	    case MPEG_PAD_CODE:
	      state->packet.padding = true;
	      break;

	    case MPEG_SYSTEM_HEADER_CODE: 
	      state->packet.system_header = true;
              switch (MPEG_START_CODE_PATTERN | buf[pos + 6]) 
                {
                case MPEG_VIDEO_E0_CODE:
                case MPEG_VIDEO_E1_CODE: 
                case MPEG_VIDEO_E2_CODE: 
                  state->packet.video = true;
                  break;
                case MPEG_AUDIO_C0_CODE:
                  state->packet.audio = true;
                  state->packet.audio_c0 = true;
                  break;
                  
                case MPEG_AUDIO_C1_CODE:
                  state->packet.audio = true;
                  state->packet.audio_c1 = true;
                  break;
                  
                case MPEG_AUDIO_C2_CODE:
                  state->packet.audio = true;
                  state->packet.audio_c2 = true;
                  break;

                case MPEG_OGT_CODE:
                  state->packet.ogt = true;
                  break;
                }
	      break;

	    case MPEG_VIDEO_E0_CODE:
	    case MPEG_VIDEO_E1_CODE: 
	    case MPEG_VIDEO_E2_CODE: 
	      state->packet.video = true;
	      if (analyze_pes)
		_analyze_video_pes (code, buf + pos, size, state);
	      break;

	    case MPEG_AUDIO_C0_CODE:
	      state->packet.audio = true;
              state->packet.audio_c0 = true;
              break;

	    case MPEG_AUDIO_C1_CODE:
	      state->packet.audio = true;
              state->packet.audio_c1 = true;
              break;

	    case MPEG_AUDIO_C2_CODE:
	      state->packet.audio = true;
              state->packet.audio_c2 = true;
              break;

            case MPEG_OGT_CODE:
              state->packet.ogt = true;
              break;
	    }

          if (state->packet.audio)
            state->stream.audio_info = true;

          if (state->packet.audio_c0)
            state->stream.audio_c0 = true;

          if (state->packet.audio_c1)
            state->stream.audio_c1 = true;

          if (state->packet.audio_c2)
            state->stream.audio_c2 = true;

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
	  _data->timestamp = state.packet.aps_timecode;

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

  obj->info.audio_c0 = state.stream.audio_c0;
  obj->info.audio_c1 = state.stream.audio_c1;
  obj->info.audio_c2 = state.stream.audio_c2;

  if (!state.stream.audio_c0 && !state.stream.audio_c1 && !state.stream.audio_c2)
    obj->info.audio_type = MPEG_AUDIO_NOSTREAM;
  else if (state.stream.audio_c0 && !state.stream.audio_c1 && !state.stream.audio_c2)
    obj->info.audio_type = MPEG_AUDIO_1STREAM;
  else if (state.stream.audio_c0 && state.stream.audio_c1 && !state.stream.audio_c2)
    obj->info.audio_type = MPEG_AUDIO_2STREAM;
  else if (state.stream.audio_c0 && !state.stream.audio_c1 && state.stream.audio_c2)
    obj->info.audio_type = MPEG_AUDIO_EXT_STREAM;
  else
    {
      vcd_warn ("unsupported audio stream aggregation encountered! (c0: %d, c1: %d, c2: %d)",
                state.stream.audio_c0, state.stream.audio_c1, state.stream.audio_c2);
      obj->info.audio_type = MPEG_AUDIO_NOSTREAM;
    }

  obj->info.packets = state.stream.packets;
  obj->scanned = true;

  obj->info.playing_time = (MAX (state.stream.max_pict_timecode,
				 state.stream.max_gop_timecode) 
			    - state.stream.min_gop_timecode);

  if (state.stream.min_gop_timecode > 1)
    vcd_debug ("start offset %f", state.stream.min_gop_timecode);

  vcd_debug ("playing time %f", obj->info.playing_time);

  for(n = _vcd_list_begin (aps_list);
      n; n = _vcd_list_node_next (n))
    {
      struct aps_data *_data = _vcd_list_node_data (n);
      
      _data->timestamp -= state.stream.min_gop_timecode;
    }

  obj->info.aps_list = aps_list;

  obj->info.hsize = state.stream.hsize;
  obj->info.vsize = state.stream.vsize;
  obj->info.aratio = state.stream.aratio;
  obj->info.frate = state.stream.frate;
  obj->info.bitrate = state.stream.bitrate;
  obj->info.vbvsize = state.stream.vbvsize;
  obj->info.constrained_flag = state.stream.constrained_flag;
  
  obj->info.version = state.stream.version;

  obj->info.norm = _get_norm (state.stream.hsize,
			      state.stream.vsize,
			      state.stream.frate);
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

              if (state.packet.video)
                flags->type = PKT_TYPE_VIDEO;
              else if (state.packet.audio)
                {
                  flags->type = PKT_TYPE_AUDIO;
                  flags->audio_c0 = state.packet.audio_c0;
                  flags->audio_c1 = state.packet.audio_c1;
                  flags->audio_c2 = state.packet.audio_c2;
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
