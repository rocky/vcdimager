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

#ifndef __VCD_BITVEC_H__
#define __VCD_BITVEC_H__

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_types.h>

#define vcd_bitvec_peek_bits32(bitvec, offset) \
 vcd_bitvec_peek_bits ((bitvec), (offset), 32)

#define vcd_bitvec_peek_bits16(bitvec, offset) \
 vcd_bitvec_peek_bits ((bitvec), (offset), 16)

static inline bool
_vcd_bit_set_p (uint32_t n, unsigned bit)
{
  return ((n >> bit) & 0x1) == 0x1;
}

static inline uint32_t 
vcd_bitvec_peek_bits (const uint8_t bitvec[], int offset, int bits)
{
  uint32_t result = 0;
  int i;

  vcd_assert (bits > 0 && bits <= 32);

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
vcd_bitvec_read_bits (const uint8_t bitvec[], int *offset, int bits)
{
  uint32_t retval = vcd_bitvec_peek_bits (bitvec, *offset, bits);
  
  *offset += bits;

  return retval;
}

static inline int
vcd_bitvec_align (int value, int boundary)
{
  if (value % boundary)
    value += (boundary - (value % boundary));

  return value;
}

#endif /* __VCD_BITVEC_H__ */
