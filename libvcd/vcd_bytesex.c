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

#include <libvcd/vcd_assert.h>

#include "vcd_bytesex.h"

static const char _rcsid[] = "$Id$";

uint8_t
to_bcd8 (uint8_t n)
{
  vcd_assert (n < 100);

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
  vcd_assert (msf != 0);

  msf->m = to_bcd8 (lba / (60 * 75));
  msf->s = to_bcd8 ((lba / 75) % 60);
  msf->f = to_bcd8 (lba % 75);
}

uint32_t
msf_to_lba (const msf_t *msf)
{
  uint32_t lba = 0;

  vcd_assert (msf != 0);

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
