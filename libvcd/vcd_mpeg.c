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

const uint8_t mpeg_sync[] = {
  0x00, 0x00, 0x01, 0xba 
};

const uint8_t mpeg_seq_start[] = {
  0x00, 0x00, 0x01, 0xb3 
};

const uint8_t mpeg_end[] = {
  0x00, 0x00, 0x01, 0xb9 
};

const double frame_rates[16] = {
   0.00, 23.976, 24.0, 25.0, 
  29.97, 30.0,   50.0, 59.94, 
  60.00, 00.0, 
};

const double aspect_ratios[16] = { 
  0.0000, 1.0000, 0.6735, 0.7031,
  0.7615, 0.8055, 0.8437, 0.8935,
  0.9375, 0.9815, 1.0255, 1.0695,
  1.1250, 1.1575, 1.2015, 0.0000
};

mpeg_type_t 
mpeg_type (const void *mpeg_block)
{
  const uint8_t *data = mpeg_block;
  int n;

  if (!memcmp (data, mpeg_end, sizeof (mpeg_end)))
    return MPEG_END;
  
  if (memcmp(data, mpeg_sync, sizeof (mpeg_sync))) 
    {
      for (n = 0;n < 2324;n++)
        if (data[n])
          return MPEG_INVALID;

      return MPEG_NULL;
    }

  if ((data[15] == 0xe0) ||
      (data[15] == 0xbb && data[24] == 0xe0))
    return MPEG_VIDEO;
  
  if ((data[15] == 0xc0) ||
      (data[15] == 0xbb && data[24] == 0xc0))
    return MPEG_AUDIO;

  return MPEG_UNKNOWN;
}

bool 
mpeg_analyze_start_seq(const void *packet, mpeg_info_t *info)
{
  const uint8_t *pkt = packet;
  int fixup = 0;

  assert(info != NULL);

  if(memcmp(pkt, mpeg_sync, sizeof(mpeg_sync)))
    return FALSE;

  for(fixup = 0;fixup < 4;fixup++, pkt++)
    if(!memcmp(pkt+30, mpeg_seq_start, sizeof(mpeg_seq_start)) ) {
      unsigned hsize, vsize, aratio, frate, brate, bufsize;

      hsize = pkt[34] << 4;
      hsize += pkt[35] >> 4;

      vsize = (pkt[35] & 0x0f) << 8;
      vsize += pkt[36];

      aratio = pkt[37] >> 4;
      frate = pkt[37] & 0x0f;

      brate = pkt[38] << 10;
      brate += pkt[39] << 2;
      brate += pkt[40] >> 6;

      bufsize = (pkt[40] & 0x3f) << 6;
      bufsize += pkt[41] >> 4;
    
      info->hsize = hsize;
      info->vsize = vsize;
      info->aratio = aspect_ratios[aratio];
      info->frate = frame_rates[frate];
      info->bitrate = 400*brate;
      info->vbvsize = bufsize;

      if(hsize == 352 && vsize == 288 && frate == 3)
        info->norm = MPEG_NORM_PAL;
      else if(hsize == 352 && vsize == 240 && frate == 1)
        info->norm = MPEG_NORM_FILM;
      else if(hsize == 352 && vsize == 240 && frate == 4)
        info->norm = MPEG_NORM_NTSC;
      else
        info->norm = MPEG_NORM_OTHER;

      if(fixup)
        vcd_debug("fixup of %d bytes was necessary for mpeg format detection...\n", fixup);

      return TRUE;
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
