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

#ifndef _CD_SECTOR_H_
#define _CD_SECTOR_H_

#include "vcd_types.h"

#define CI_VIDEO   0x0f
#define CI_STILL   0x1f
#define CI_STILL2  0x3f
#define CI_AUDIO   0x7f
#define CI_OTHER   0x00

#define SM_EOR    (1<<0)
#define SM_VIDEO  (1<<1)
#define SM_AUDIO  (1<<2)
#define SM_DATA   (1<<3)
#define SM_TRIG   (1<<4)
#define SM_FORM2  (1<<5)
#define SM_REALT  (1<<6)
#define SM_EOF    (1<<7)

#define CDDA_SIZE  2352
#define M2F1_SIZE  2048
#define M2F2_SIZE  2324
#define M2RAW_SIZE 2336

#define CD_MAX_TRACKS 99 

#define CD_74MIN_SECTORS UINT32_C(74*60*75)
#define CD_80MIN_SECTORS UINT32_C(80*60*75)
#define CD_90MIN_SECTORS UINT32_C(90*60*75)
#define CD_MAX_SECTORS   UINT32_C(100*60*75-1)

/* make mode 2 form 1/2 sector
 *
 * data must be a buffer of size 2048 or 2324 for SM_FORM2
 * raw_sector must be a writable buffer of size 2352
 */
void
_vcd_make_mode2 (void *raw_sector, const void *data, uint32_t extent,
                 uint8_t fnum, uint8_t cnum, uint8_t sm, uint8_t ci);

/* ...data must be a buffer of size 2336 */

void
_vcd_make_raw_mode2 (void *raw_sector, const void *data, uint32_t extent);

#endif /* _CD_SECTOR_H_ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
