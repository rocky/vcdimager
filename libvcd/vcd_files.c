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
#include <stdlib.h>

#include "vcd_files.h"
#include "vcd_files_private.h"
#include "vcd_bytesex.h"
#include "vcd_obj.h"

void
set_entries_vcd(VcdObj *obj)
{
  int n;
  EntriesVcd entries_vcd;

  assert(sizeof(EntriesVcd) == 2048);

  assert(obj->mpeg_tracks_num <= 509);
  assert(obj->mpeg_tracks_num > 0);

  memset(&entries_vcd, 0, sizeof(entries_vcd)); /* paranoia / fixme */
  strncpy(entries_vcd.ID, ENTRIES_ID, 8);
  entries_vcd.version = UINT16_TO_BE(ENTRIES_VERSION);
  entries_vcd.tracks = UINT16_TO_BE(obj->mpeg_tracks_num);

  for(n = 0;n < obj->mpeg_tracks_num;n++) {
    unsigned lsect = obj->mpeg_tracks[n].relative_start_extent;

    lsect += obj->iso_size;

    entries_vcd.entry[n].n = to_bcd8(n+2);
    entries_vcd.entry[n].m = to_bcd8(lsect / (60*75));
    entries_vcd.entry[n].s = to_bcd8(((lsect / 75) % 60)+2);
    entries_vcd.entry[n].f = to_bcd8(lsect % 75);
  }

  memcpy(obj->entries_vcd_buf, &entries_vcd, sizeof(entries_vcd));
}


static void
_set_bit(uint8_t bitset[], unsigned bitnum)
{
  unsigned _byte = bitnum / 8;
  unsigned _bit  = bitnum % 8;

  bitset[_byte] |= (1 << _bit);
}

void
set_psd_size(VcdObj *obj)
{
  obj->psd_size = obj->mpeg_tracks_num*16; /* 2<<3 */
  obj->psd_size += 8; /* stop descriptor */
}

void
set_psd_vcd(VcdObj *obj)
{
  int n;

  memset(obj->psd_vcd_buf, 0, sizeof(obj->psd_vcd_buf));

  for(n = 0;n < obj->mpeg_tracks_num;n++) {
    PsdPlayListDescriptor _md;
    memset(&_md, 0, sizeof(_md));

    _md.type = PSD_TYPE_PLAY_LIST;
    _md.noi = 1;
    _md.lid = UINT16_TO_BE(n+1);
    _md.prev_ofs = UINT16_TO_BE(n ? (n-1)<<1 : 0xffff);
    _md.next_ofs = UINT16_TO_BE((n+1) << 1);
    _md.retn_ofs = UINT16_TO_BE((obj->psd_size-8) >> 3);
    _md.ptime = UINT16_TO_BE(0x0000);
    _md.wtime = 0x05;
    _md.atime = 0x00;
    _md.itemid[0] = UINT16_TO_BE(n+2);

    memcpy(obj->psd_vcd_buf+(n << 4), &_md, sizeof(_md));
  }

  {
    PsdEndOfListDescriptor _sd;
    
    memset(&_sd, 0, sizeof(_sd));
    _sd.type = PSD_TYPE_END_OF_LIST;
    memcpy(obj->psd_vcd_buf+(n << 4), &_sd, sizeof(_sd));
  }
}

void
set_lot_vcd(VcdObj *obj)
{
  LotVcd *lot_vcd = NULL;
  int n;

  lot_vcd = malloc(sizeof(LotVcd));
  memset(lot_vcd, 0xff, sizeof(LotVcd));
  
  /* lot_vcd->unknown = UINT16_TO_BE(0x0000); -- fixme */
  
  for(n = 0;n < obj->mpeg_tracks_num+1;n++)
    lot_vcd->offset[n] = UINT16_TO_BE(n << 1); /* quick'n'dirty */

  memcpy(obj->lot_vcd_buf, lot_vcd, sizeof(LotVcd));
  free(lot_vcd);
}

void
set_info_vcd(VcdObj *obj)
{
  InfoVcd info_vcd;
  int n;
  
  assert(sizeof(InfoVcd) == 2048);
  assert(obj->mpeg_tracks_num <= 98);
  
  memset(&info_vcd, 0, sizeof(info_vcd));
  strncpy(info_vcd.ID, INFO_ID_VCD, sizeof(info_vcd.ID));
  info_vcd.version = INFO_VERSION_VCD2;
  info_vcd.sys_prof_tag = INFO_SPTAG_VCD2;
  
  strncpy(info_vcd.album_desc, 
          "by GNU VCDImager",
          sizeof(info_vcd.album_desc));

  info_vcd.vol_count = UINT16_TO_BE(0x0001);
  info_vcd.vol_id = UINT16_TO_BE(0x0001);

  for(n = 0; n < obj->mpeg_tracks_num;n++)
    if(obj->mpeg_tracks[n].mpeg_info.norm == MPEG_NORM_PAL)
      _set_bit(info_vcd.pal_flags, n);

  info_vcd.psd_size = UINT32_TO_BE(obj->psd_size);

/*   info_vcd.unknown2[0] = 0x00; /* ??????  */
/*   info_vcd.unknown2[1] = 0x02; /* ?????? BCD offset of something perhaps ???? */
/*   info_vcd.unknown2[2] = 0x00; /* ??????  */

  info_vcd.offset_mult = INFO_OFFSET_MULT;

  info_vcd.last_psd_ofs = UINT16_TO_BE((obj->mpeg_tracks_num+1)<<1); /* maybe this is the autoplay offset? */

  info_vcd.item_count = UINT16_TO_BE(0x0000); /* no items in /SEGMENT supported yet */

  memcpy(obj->info_vcd_buf, &info_vcd, sizeof(info_vcd));
}

/* eof */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
