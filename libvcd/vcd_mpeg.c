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

#include "vcd_types.h"
#include "vcd_mpeg.h"
#include "vcd_logging.h"

#define MPEG_PACK_HEADER_CODE    ((uint32_t) 0x000001ba)
#define MPEG_SYSTEM_HEADER_CODE  ((uint32_t) 0x000001bb)
#define MPEG_PROGRAM_END_CODE    ((uint32_t) 0x000001b9)
#define MPEG_SEQUENCE_CODE       ((uint32_t) 0x000001b3)
#define MPEG_NULL_CODE           ((uint32_t) 0x000001be) /* what is it really? */

#define MPEG_VIDEO_CODE          ((uint32_t) 0x000001e0)
#define MPEG_AUDIO_CODE          ((uint32_t) 0x000001c0)


const static double frame_rates[16] = {
   0.00, 23.976, 24.0, 25.0, 
  29.97, 30.0,   50.0, 59.94, 
  60.00, 00.0, 
};

const static double aspect_ratios[16] = { 
  0.0000, 1.0000, 0.6735, 0.7031,
  0.7615, 0.8055, 0.8437, 0.8935,
  0.9375, 0.9815, 1.0255, 1.0695,
  1.1250, 1.1575, 1.2015, 0.0000
};

#define _BIT_SET_P(n, bit)  (((n) >> (bit)) & 0x1)

static uint32_t 
_bitvec_get_bits32 (const uint8_t bitvec[], int offset, int bits)
{
  uint32_t result = 0;
  int i;

  assert (bits > 0 && bits <= 32);

  for (i = offset; i < (offset + bits); i++)
    {
      result <<= 1;
      if (_BIT_SET_P (bitvec[i / 8], 7 - (i % 8)))
        result |= 0x1;
    }
  
  return result;
}

mpeg_type_t 
mpeg_type (const void *mpeg_block)
{
  const uint8_t *data = mpeg_block;
  int n;
  uint32_t code;
  int offset = 0;

  code = _bitvec_get_bits32 (data, offset, 32);

  offset += 32;

  switch (code)
    {
    case MPEG_PROGRAM_END_CODE:
      return MPEG_END;
      break;

    case MPEG_PACK_HEADER_CODE:
      {
        switch (_bitvec_get_bits32(data, offset, 2))
          {
          case 0x0: /* mpeg1 */
          case 0x1: /* mpeg2 */
            break;
          default:
            vcd_warn ("packet not recognized as mpeg1 or mpeg2 stream (%d)", 
                      _bitvec_get_bits32(data, offset, 2));
            /* return MPEG_INVALID; */
            break;
          }

        while ((offset + 32) < (2324 * 8))
          {
            code = _bitvec_get_bits32 (data, offset, 32);
            offset += 32;

            switch (code)
              {
              case MPEG_NULL_CODE:
                return MPEG_NULL;
                break;

              case MPEG_AUDIO_CODE:
                return MPEG_AUDIO;
                break;
       
              case MPEG_VIDEO_CODE:
                return MPEG_VIDEO;
                break;
                
              case MPEG_SYSTEM_HEADER_CODE:
                {
                  int size = _bitvec_get_bits32 (data, offset, 16); 
                  /* fixme -- boundary check assert (offset + 16 < 2324 * 8) */
                  
                  offset += 16;
                  offset += size * 8;
                }
                break;

              default:
                offset -= 24; /* jump back 24 of 32 bits... */
                break;
              }
          }
        /* ?? */
      }
      break;

    default:
      for (n = 0;n < 2324; n++)
        if (data[n])
          return MPEG_INVALID;

      return MPEG_NULL;
      break;
    }

  return MPEG_UNKNOWN;
}


int
mpeg_analyze_start_seq (const void *packet, mpeg_info_t *info)
{
  const uint8_t *data = packet;
  int offset = 0;
  uint32_t code;

  assert (info != NULL);
  assert (packet != NULL);
  
  code = _bitvec_get_bits32 (data, offset, 32);
  offset += 32;

  if (code == MPEG_PACK_HEADER_CODE)
    {
      mpeg_vers_t mpeg_version;

      switch (_bitvec_get_bits32(data, offset, 2))
        {
        case 0x0:
          mpeg_version = MPEG_VERS_MPEG1;
          break;
        case 0x1:
          mpeg_version = MPEG_VERS_MPEG2;
          break;
        default:
          mpeg_version = MPEG_VERS_INVALID;
          vcd_warn ("failed to recognize mpeg version");
          return FALSE;
          break;
        }

      while (offset + 32 + 64 < 2324 * 8)
        {
          code = _bitvec_get_bits32 (data, offset, 32);
          offset += 32;
          
          if (code == MPEG_SEQUENCE_CODE)
            {
              unsigned hsize, vsize, aratio, frate, brate, bufsize, constr;

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

              info->hsize = hsize;
              info->vsize = vsize;
              info->aratio = aspect_ratios[aratio];
              info->frate = frame_rates[frate];
              info->bitrate = 400*brate;
              info->vbvsize = bufsize * 16 * 1024; 
              info->constrained_flag = constr;

              info->vers = mpeg_version;

              if (hsize == 352 && vsize == 288 && frate == 3)
                info->norm = MPEG_NORM_PAL;
              else if (hsize == 352 && vsize == 240 && frate == 1)
                info->norm = MPEG_NORM_FILM;
              else if (hsize == 352 && vsize == 240 && frate == 4)
                info->norm = MPEG_NORM_NTSC;
              else if (hsize == 480 && vsize == 576 && frate == 3)
                info->norm = MPEG_NORM_PAL_S;
              else if (hsize == 480 && vsize == 480 && frate == 4)
                info->norm = MPEG_NORM_NTSC_S;
              else
                info->norm = MPEG_NORM_OTHER;
              
              return TRUE;
            }

          offset -= 24;
        }
    }
  
  return FALSE;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
