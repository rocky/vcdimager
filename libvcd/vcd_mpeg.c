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

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "vcd_types.h"
#include "vcd_mpeg.h"
#include "vcd_logging.h"

static const char _rcsid[] = "$Id$";

#define MPEG_START_CODE_PATTERN  ((uint32_t) 0x00000100)
#define MPEG_START_CODE_MASK     ((uint32_t) 0xffffff00)

#define _MPEG_START_CODE_P(code) (((code) & MPEG_START_CODE_MASK) == MPEG_START_CODE_PATTERN)

#define MPEG_PICTURE_START_CODE  ((uint32_t) 0x00000100)

#define MPEG_USER_DATA_CODE      ((uint32_t) 0x000001b2)
#define MPEG_SEQUENCE_CODE       ((uint32_t) 0x000001b3)
#define MPEG_EXT_START_CODE      ((uint32_t) 0x000001b5)
#define MPEG_SEQ_END_CODE        ((uint32_t) 0x000001b7)
#define MPEG_GOP_START_CODE      ((uint32_t) 0x000001b8)
#define MPEG_PROGRAM_END_CODE    ((uint32_t) 0x000001b9)
#define MPEG_PACK_HEADER_CODE    ((uint32_t) 0x000001ba)
#define MPEG_SYSTEM_HEADER_CODE  ((uint32_t) 0x000001bb)
#define MPEG_OGT_CODE            ((uint32_t) 0x000001bd)
#define MPEG_NULL_CODE           ((uint32_t) 0x000001be)

#define MPEG_AUDIO_CODE          ((uint32_t) 0x000001c0) 
#define MPEG_AUDIO_CODE_1        ((uint32_t) 0x000001c1) /* 2nd audio channel */

#define MPEG_VIDEO_CODE          ((uint32_t) 0x000001e0) /* motion */
#define MPEG_VIDEO_CODE_1        ((uint32_t) 0x000001e1) /* lowres still */
#define MPEG_VIDEO_CODE_2        ((uint32_t) 0x000001e2) /* hires still */

#define PICT_TYPE_I             1
#define PICT_TYPE_P             2
#define PICT_TYPE_B             3
#define PICT_TYPE_D             4

typedef struct 
{
  char            frame_type;
  uint32_t        packet;
  uint32_t        offset;
  uint32_t        timecode;
} frames_t;

frames_t *frames;
int frames_count;
int frames_alloc;

const static double frame_rates[16] = 
  {
    0.00, 23.976, 24.0, 25.0, 
    29.97, 30.0,   50.0, 59.94, 
    60.00, 00.0, 
  };

const static double aspect_ratios[16] = 
  { 
    0.0000, 1.0000, 0.6735, 0.7031,
    0.7615, 0.8055, 0.8437, 0.8935,
    0.9375, 0.9815, 1.0255, 1.0695,
    1.1250, 1.1575, 1.2015, 0.0000
  };

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

#define MPEG_PACKET_SIZE_BITS (MPEG_PACKET_SIZE << 3)

#define _BIT_SET_P(n, bit)  (((n) >> (bit)) & 0x1)

static uint32_t 
_bitvec_get_bits32 (const uint8_t bitvec[], int offset, int bits)
{
  uint32_t result = 0;
  int i;

  assert (bits > 0 && bits <= 32);

#if 1
  if ((offset & 7) == 0 && (bits & 7) == 0) 
    {
      for (i = offset; i < (offset + bits); i+= 8)
        result = (result << 8) + bitvec[i/8];
      return result;
    }
#endif

  for (i = offset; i < (offset + bits); i++)
    {
      result <<= 1;
      if (_BIT_SET_P (bitvec[i / 8], 7 - (i % 8)))
        result |= 0x1;
    }
  
  return result;
}

static void 
_mpeg_generic_packet_skip (const void *packet, int *offsetp)
{
  int size;

  assert (*offsetp + 2 <= MPEG_PACKET_SIZE_BITS);

  size = _bitvec_get_bits32 (packet, *offsetp, 16); 
  *offsetp += 16;
  
  *offsetp += size << 3;

  assert (*offsetp <= MPEG_PACKET_SIZE_BITS);
}

static void
_vcd_mpeg_parse_sequence_header (const void *packet, int *offsetp,
                                 mpeg_type_info_t *info)
{
  int offset = *offsetp;
  unsigned hsize, vsize, aratio, frate, brate, bufsize, constr;
  const uint8_t *data = packet;
  
  assert (packet != NULL);
  assert (offsetp != NULL);
  
  assert (info->type == MPEG_TYPE_VIDEO);

  hsize = _bitvec_get_bits32 (data, offset, 12);
  offset += 12;

  vsize = _bitvec_get_bits32 (data, offset, 12);
  offset += 12;

  aratio = _bitvec_get_bits32 (data, offset, 4);
  offset += 4;

  frate = _bitvec_get_bits32 (data, offset, 4);
  offset += 4;

  brate = _bitvec_get_bits32 (data, offset, 18);
  offset += 18;
              
  /* marker bit const == 1 */
  offset += 1;

  bufsize = _bitvec_get_bits32 (data, offset, 10);
  offset += 10;

  constr = _bitvec_get_bits32 (data, offset, 1);
  offset += 1;

  /* skip intra quantizer matrix */

  if (_bitvec_get_bits32 (data, offset++, 1))
    offset += 64 * 8;

  /* skip non-intra quantizer matrix */

  if (_bitvec_get_bits32 (data, offset++, 1))
    offset += 64 * 8;
  
  info->video.hsize = hsize;
  info->video.vsize = vsize;
  info->video.aratio = aspect_ratios[aratio];
  info->video.frate = frame_rates[frate];
  info->video.bitrate = 400*brate;
  info->video.vbvsize = bufsize * 16 * 1024; 
  info->video.constrained_flag = (constr != 0);

  *offsetp = offset;
}

static void
_vcd_mpeg_parse_picture (const void *packet, int *offsetp, mpeg_type_info_t *info)
{
  const uint8_t *data = packet;
  int pict_type;
  int offset = *offsetp;
  int timecode;

  timecode = _bitvec_get_bits32 (data, offset, 10);
  offset += 10;

  pict_type = _bitvec_get_bits32 (data, offset, 3);
  offset += 3;

  /* type = "0IPBDpb7"[pict_type]; */

  offset += 16;			/* vbv delay */

  switch (pict_type)
    {
    case PICT_TYPE_I:
      info->video.i_frame_flag = true;
      info->video.rel_timecode = timecode;
      break;
    case PICT_TYPE_P:
      offset += 4;
      break;
    case PICT_TYPE_B:
      offset += 8;
      break;
    default:
      break;
    }
  
  *offsetp = offset;
}

static void
_vcd_mpeg_parse_gop (const void *packet, int *offsetp, mpeg_type_info_t *info)
{
  const uint8_t *data = packet;
  int offset = *offsetp;

  bool drop_flag;
  bool close_gop;
  bool broken_link;

  unsigned hour, minute, second, frame;

  drop_flag = _bitvec_get_bits32(data, offset++, 1) != 0;

  hour = _bitvec_get_bits32(data, offset, 5);
  offset += 5;

  minute = _bitvec_get_bits32(data, offset, 6);
  offset += 6;

  (void) _bitvec_get_bits32(data, offset++, 1); 

  second = _bitvec_get_bits32(data, offset, 6);
  offset += 6;

  frame = _bitvec_get_bits32(data, offset, 6);
  offset += 6;

  close_gop = _bitvec_get_bits32(data, offset++, 1) != 0;

  broken_link = _bitvec_get_bits32(data, offset++, 1) != 0;

  info->video.timecode.h = hour;
  info->video.timecode.m = minute;
  info->video.timecode.s = second;
  info->video.timecode.f = frame;

  info->video.gop_flag = true;

  *offsetp = offset;
}

static void
_vcd_mpeg_parse_video (const void *packet, int *offsetp, mpeg_type_info_t *info)
{
  const uint8_t *data = packet;
  int offset = *offsetp;
  int size;
  int offset_end;

  size = _bitvec_get_bits32 (data, offset, 16);
  offset += 16;
  
  size <<= 3;
  offset_end = offset + size;

  assert (offset_end <= MPEG_PACKET_SIZE_BITS);
  assert (offset % 8 == 0);

  while (offset < offset_end - 32)
    {
      uint32_t code = _bitvec_get_bits32 (data, offset, 32);
      offset += 32;

      if (!_MPEG_START_CODE_P (code))
        {
          offset -= 24;
          continue;
        }

      switch(code) 
        {
        case MPEG_SEQUENCE_CODE:
          _vcd_mpeg_parse_sequence_header (data, &offset, info);
          break;

        case MPEG_EXT_START_CODE:
          {
            int ext_type = _bitvec_get_bits32 (data, offset, 4);
            offset += 4;

            switch(ext_type) 
              {
              case 1:
              case 8:
                offset += 36;
                break;
          
              case 2:
                offset += 52;
                break;

              default:
                vcd_warn ("mpeg_video: unknown ext start type %x", ext_type);
                offset -= 4; /* restore offset */
                break;
              }
            break;
          }
        
        case MPEG_GOP_START_CODE:
          _vcd_mpeg_parse_gop (packet, &offset, info);
          break;

        case MPEG_PICTURE_START_CODE:
          _vcd_mpeg_parse_picture (packet, &offset, info);
          break;

        case MPEG_SEQ_END_CODE:
          break;

        default:
          /* do nothing! */
      }

      /* round up to next byte boundary */
      if (offset % 8)
        offset += 8 - (offset % 8);

    } /* for */

  *offsetp = offset;
}

mpeg_type_t 
vcd_mpeg_get_type (const void *packet, mpeg_type_info_t *extended_type_info)
{
  const uint8_t *data = packet;
  int n;
  uint32_t code;
  int offset = 0;

  assert (packet != NULL);

  if (extended_type_info)
    memset (extended_type_info, 0, sizeof (mpeg_type_info_t));

  code = _bitvec_get_bits32 (data, offset, 32);
  offset += 32;

  switch (code)
    {
    case MPEG_PROGRAM_END_CODE:
      if (extended_type_info)
        extended_type_info->type = MPEG_TYPE_END;
      return MPEG_TYPE_END;
      break;
      
    case MPEG_PACK_HEADER_CODE:
      {
        switch (_bitvec_get_bits32(data, offset, 2))
          {
          case 0x0: /* mpeg1 */
            if (extended_type_info)
              extended_type_info->version = MPEG_VERS_MPEG1;
            offset += 8 << 3;
            break;
          case 0x1: /* mpeg2 */
            if (extended_type_info)
              extended_type_info->version = MPEG_VERS_MPEG2;
            offset += 10 << 3;
            break;
          default:
            vcd_warn ("packet not recognized as either mpeg1 or mpeg2 stream (%d)", 
                      _bitvec_get_bits32(data, offset, 2));
            if (extended_type_info)
              {
                extended_type_info->type = MPEG_TYPE_UNKNOWN;
                extended_type_info->version = MPEG_VERS_INVALID;
              }

            return MPEG_TYPE_UNKNOWN;  /* return MPEG_INVALID; */
            break;
          }

        while ((offset + 32) < MPEG_PACKET_SIZE_BITS)
          {
            code = _bitvec_get_bits32 (data, offset, 32);

            if (!_MPEG_START_CODE_P (code))
              {
                offset += 8;
                continue;
              }

            offset += 32;

            switch (code)
              {
              case MPEG_NULL_CODE:
                if (extended_type_info)
                  extended_type_info->type = MPEG_TYPE_NULL;
                return MPEG_TYPE_NULL;
                break;

              case MPEG_AUDIO_CODE_1:
                if (extended_type_info)
                  extended_type_info->type = MPEG_TYPE_AUDIO_1;

                _mpeg_generic_packet_skip (packet, &offset);
                
                return MPEG_TYPE_AUDIO_1;
                break;

              case MPEG_AUDIO_CODE:
                if (extended_type_info)
                  extended_type_info->type = MPEG_TYPE_AUDIO;
                
                _mpeg_generic_packet_skip (packet, &offset);
                
                return MPEG_TYPE_AUDIO;
                break;
       
              case MPEG_VIDEO_CODE_1:
              case MPEG_VIDEO_CODE_2:
                vcd_warn ("(unsupported) still images video header detected");
              case MPEG_VIDEO_CODE:
                if (extended_type_info) 
                  {
                    extended_type_info->type = MPEG_TYPE_VIDEO;

                    _vcd_mpeg_parse_video (packet, &offset, extended_type_info);
                  }
                else
                  _mpeg_generic_packet_skip (packet, &offset);

                return MPEG_TYPE_VIDEO;
                break;
                
              case MPEG_SYSTEM_HEADER_CODE:
                /* just a hack, since I don't know exactly what the
                   format of the system header is */
                if (!extended_type_info)
                  switch (_bitvec_get_bits32 (data, offset + ((6 + 2) * 8), 8))
                    {
                    case 0xe0:
                      return MPEG_TYPE_VIDEO;
                      break;
                    case 0xc0:
                      return MPEG_TYPE_AUDIO;
                      break;
                    default:
                      /* noop */
                      break;
                    }

                _mpeg_generic_packet_skip (packet, &offset);
                break;

              case MPEG_OGT_CODE:
                if (extended_type_info)
                  extended_type_info->type = MPEG_TYPE_OGT;

                _mpeg_generic_packet_skip (packet, &offset);

                return MPEG_TYPE_OGT;
                break;

              case MPEG_PICTURE_START_CODE:
                vcd_warn ("unexpected picture start code encountered");
                offset -= 8; /* paranoia */
                break;

              default:
                /* noop -- just let the next 32 bit be analyzed */
                break;
              }
          }
        /* ?? */
      }
      break;

    default:
      if (extended_type_info)
        extended_type_info->type = MPEG_TYPE_INVALID;
      
      for (n = 0;n < MPEG_PACKET_SIZE; n++)
        if (data[n])
          return MPEG_TYPE_INVALID;

      if (extended_type_info)
        extended_type_info->type = MPEG_TYPE_NULL;

      return MPEG_TYPE_NULL;
      break;
    }

  return MPEG_TYPE_UNKNOWN;
}

double
vcd_mpeg_get_timecode (const void *packet)
{
  mpeg_type_info_t _info;

  assert (packet != NULL);
  
  memset (&_info, 0, sizeof (mpeg_type_info_t));

  if (vcd_mpeg_get_type (packet, &_info) == MPEG_TYPE_VIDEO)
    {
      assert (_info.type == MPEG_TYPE_VIDEO);

      if(_info.video.gop_flag)
        return -1; /* _info.video.timecode */;
    }

  return -1;
}

bool
vcd_mpeg_get_info (const void *packet, mpeg_info_t *info)
{
  mpeg_type_info_t _info;

  assert (info != NULL);
  assert (packet != NULL);

  memset (&_info, 0, sizeof (mpeg_type_info_t));

  if (vcd_mpeg_get_type (packet, &_info) == MPEG_TYPE_VIDEO)
    {
      assert (_info.type == MPEG_TYPE_VIDEO);

      memset (info, 0, sizeof (mpeg_info_t));

      info->hsize = _info.video.hsize;
      info->vsize = _info.video.vsize;
      info->aratio = _info.video.aratio;
      info->frate = _info.video.frate;
      info->bitrate = _info.video.bitrate;
      info->vbvsize = _info.video.vbvsize;
      info->constrained_flag = _info.video.constrained_flag;
      
      info->version = _info.version;

      info->norm = _get_norm (info->hsize, info->vsize, info->frate);

      return true;
    }              

  return false;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
