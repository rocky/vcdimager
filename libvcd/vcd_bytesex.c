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

/* stuff */

#include <assert.h>

#include "vcd_bytesex.h"

static const char _rcsid[] = "$Id$";

uint16_t 
to_721(uint16_t i)
{
  return UINT16_TO_LE(i);
}

uint16_t
to_722(uint16_t i)
{
  return UINT16_TO_BE(i);
}

uint32_t
to_723(uint16_t i)
{
  return UINT32_SWAP_LE_BE(i) | i;
}

uint32_t
to_731(uint32_t i)
{
  return UINT32_TO_LE(i);
}

uint32_t
to_732(uint32_t i)
{
  return UINT32_TO_BE(i);
}

uint64_t
to_733(uint32_t i)
{
  return UINT64_SWAP_LE_BE(i) | i;
}

/* reverse path... */

uint16_t 
from_723 (uint32_t p)
{
  return (0xFFFF & p);
}

uint32_t 
from_733 (uint64_t p)
{
  return (UINT32_C(0xFFFFFFFF) & p);
}

uint8_t
to_bcd8 (uint8_t n)
{
  assert(n < 100);

  return ((n/10)<<4) | (n%10);
}

uint8_t
from_bcd8(uint8_t p)
{
  return (0xf & p)+(10*(p >> 4));
}

void
lba_to_msf (uint32_t lba, msf_t *msf)
{
  assert (msf != 0);

  msf->m = to_bcd8 (lba / (60 * 75));
  msf->s = to_bcd8 ((lba / 75) % 60);
  msf->f = to_bcd8 (lba % 75);
}

uint32_t
msf_to_lba (const msf_t *msf)
{
  uint32_t lba = 0;

  assert (msf != 0);

  lba = from_bcd8 (msf->m);
  lba *= 60;

  lba += from_bcd8 (msf->s);
  lba *= 75;
  
  lba += from_bcd8 (msf->f);

  return lba;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
