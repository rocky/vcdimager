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
#include "vcd_logging.h"
#include "vcd_util.h"

void
set_entries_vcd(VcdObj *obj, void *buf)
{
  VcdListNode *node = NULL;
  int n = 0;
  EntriesVcd entries_vcd;

  assert(sizeof(EntriesVcd) == 2048);

  assert(_vcd_list_length (obj->mpeg_track_list) <= 509);
  assert(_vcd_list_length (obj->mpeg_track_list) > 0);

  memset(&entries_vcd, 0, sizeof(entries_vcd)); /* paranoia / fixme */

  switch (obj->type)
    {
    case VCD_TYPE_VCD2:
      strncpy(entries_vcd.ID, ENTRIES_ID_VCD, 8);
      entries_vcd.version = ENTRIES_VERSION_VCD2;
      break;

    case VCD_TYPE_SVCD:
      strncpy(entries_vcd.ID, ENTRIES_ID_SVCD, 8);
      entries_vcd.version = ENTRIES_VERSION_SVCD;
      break;
      
    default:
      vcd_error("VCD type not supported");
      break;
    }

  entries_vcd.tracks = UINT16_TO_BE(_vcd_list_length (obj->mpeg_track_list));

  for (n = 0, node = _vcd_list_begin (obj->mpeg_track_list);
       node != NULL;
       n++, node = _vcd_list_node_next (node))
    {
      mpeg_track_t *track = _vcd_list_node_data (node);
      uint32_t lsect = track->relative_start_extent;

      lsect += obj->iso_size;

      entries_vcd.entry[n].n = to_bcd8(n+2);

      lba_to_msf(lsect + 150, &(entries_vcd.entry[n].msf));
    }

  memcpy(buf, &entries_vcd, sizeof(entries_vcd));
}


static void
_set_bit(uint8_t bitset[], unsigned bitnum)
{
  unsigned _byte = bitnum / 8;
  unsigned _bit  = bitnum % 8;

  bitset[_byte] |= (1 << _bit);
}

uint32_t 
get_psd_size(VcdObj *obj)
{
  uint32_t psd_size;
  
  psd_size = _vcd_list_length (obj->mpeg_track_list)*16; /* 2<<3 */
  psd_size += 8; /* stop descriptor */

  return psd_size;
}

void
set_psd_vcd (VcdObj *obj, void *buf)
{
  int n;
  char psd_buf[ISO_BLOCKSIZE] = { 0, };

  /* memset (psd_buf, 0, sizeof (obj->psd_vcd_buf)); */

  for (n = 0; n < _vcd_list_length (obj->mpeg_track_list); n++)
    {
      const int noi = 1;
      int descriptor_size = sizeof (PsdPlayListDescriptor) + (noi * sizeof (uint16_t));
      PsdPlayListDescriptor *_md = _vcd_malloc (descriptor_size);

      _md->type = PSD_TYPE_PLAY_LIST;
      _md->noi = noi;
      _md->lid = UINT16_TO_BE (n+1);
      _md->prev_ofs = UINT16_TO_BE (n ? (n - 1) << 1 : 0xffff);
      _md->next_ofs = UINT16_TO_BE ((n + 1) << 1);
      _md->retn_ofs = UINT16_TO_BE ((get_psd_size (obj) - 8) >> 3);
      _md->ptime = UINT16_TO_BE (0x0000);
      _md->wtime = 0x05;
      _md->atime = 0x00;
      _md->itemid[0] = UINT16_TO_BE (n+2);

      memcpy (psd_buf+(n << 4), _md, descriptor_size);
      free (_md);
    }

  {
    PsdEndOfListDescriptor _sd;
    
    memset (&_sd, 0, sizeof (_sd));
    _sd.type = PSD_TYPE_END_OF_LIST;
    memcpy (psd_buf + (n << 4), &_sd, sizeof (_sd));
  }

  memcpy (buf, psd_buf, sizeof (psd_buf));
}

void
set_lot_vcd(VcdObj *obj, void *buf)
{
  LotVcd *lot_vcd = NULL;
  int n;

  lot_vcd = _vcd_malloc (sizeof (LotVcd));
  memset(lot_vcd, 0xff, sizeof(LotVcd));

  lot_vcd->reserved = 0x0000;

  for(n = 0;n < _vcd_list_length (obj->mpeg_track_list)+1;n++)
    lot_vcd->offset[n] = UINT16_TO_BE(n << 1); /* quick'n'dirty */

  memcpy(buf, lot_vcd, sizeof(LotVcd));
  free(lot_vcd);
}

void
set_info_vcd(VcdObj *obj, void *buf)
{
  InfoVcd info_vcd;
  VcdListNode *node = NULL;
  int n = 0;
  
  assert(sizeof(InfoVcd) == 2048);
  assert(_vcd_list_length (obj->mpeg_track_list) <= 98);
  
  memset(&info_vcd, 0, sizeof(info_vcd));

  switch (obj->type)
    {
    case VCD_TYPE_VCD2:
      strncpy(info_vcd.ID, INFO_ID_VCD, sizeof(info_vcd.ID));
      info_vcd.version = INFO_VERSION_VCD2;
      info_vcd.sys_prof_tag = INFO_SPTAG_VCD2;
      break;

    case VCD_TYPE_SVCD:
      strncpy(info_vcd.ID, INFO_ID_SVCD, sizeof(info_vcd.ID));
      info_vcd.version = INFO_VERSION_SVCD;
      info_vcd.sys_prof_tag = INFO_SPTAG_SVCD;
      break;
      
    default:
      vcd_error("VCD type not supported");
      break;
    }
  
  strncpy(info_vcd.album_desc, 
          "by GNU VCDImager",
          sizeof(info_vcd.album_desc));

  info_vcd.vol_count = UINT16_TO_BE(0x0001);
  info_vcd.vol_id = UINT16_TO_BE(0x0001);

  for (n = 0, node = _vcd_list_begin (obj->mpeg_track_list);
       node != NULL;
       n++, node = _vcd_list_node_next (node))
    {
      mpeg_track_t *track = _vcd_list_node_data (node);

      if(track->mpeg_info.norm == MPEG_NORM_PAL 
         || track->mpeg_info.norm == MPEG_NORM_PAL_S)
        _set_bit(info_vcd.pal_flags, n);
    }

  info_vcd.psd_size = UINT32_TO_BE(get_psd_size (obj));

  info_vcd.offset_mult = INFO_OFFSET_MULT;

  info_vcd.last_psd_ofs = UINT16_TO_BE((_vcd_list_length (obj->mpeg_track_list))<<1);

  info_vcd.item_count = UINT16_TO_BE(0x0000); /* no items in /SEGMENT supported yet */

  memcpy(buf, &info_vcd, sizeof(info_vcd));
}

void
set_tracks_svd (VcdObj *obj, void *buf)
{
  TracksSVD tracks_svd;
  VcdListNode *node;
  int n;
  
  assert (obj->type == VCD_TYPE_SVCD);
  assert (sizeof (SVDTrackContent) == 1);
  assert (sizeof (TracksSVD) == ISO_BLOCKSIZE);

  memset (&tracks_svd, 0, sizeof (tracks_svd));

  strncpy (tracks_svd.file_id, TRACKS_SVD_FILE_ID, sizeof (TRACKS_SVD_FILE_ID));
  tracks_svd.version = TRACKS_SVD_VERSION;


  tracks_svd.tracks = _vcd_list_length (obj->mpeg_track_list);

  for (n = 0, node = _vcd_list_begin (obj->mpeg_track_list);
       node != NULL;
       n++, node = _vcd_list_node_next (node))
    {
      mpeg_track_t *track = _vcd_list_node_data (node);
      unsigned playtime = track->playtime;
       
      switch (track->mpeg_info.norm)
        {
        case MPEG_NORM_PAL:
        case MPEG_NORM_PAL_S:
          tracks_svd.tracks_info[n].contents.video = 0x07;
          break;

        case MPEG_NORM_NTSC:
        case MPEG_NORM_NTSC_S:
          tracks_svd.tracks_info[n].contents.video = 0x03;
          break;
          
        default:
          vcd_warn("SVCD/TRACKS.SVCD: No MPEG video?");
          break;
        }

      tracks_svd.tracks_info[n].contents.audio = 0x02; /* fixme 
                                                          -- assumption */

      /* setting playtime */
      
      lba_to_msf (playtime * 75, &(tracks_svd.tracks_info[n].playing_time));
      tracks_svd.tracks_info[n].playing_time.f = 0; /* fixme */
    }
  
  memcpy (buf, &tracks_svd, sizeof(tracks_svd));
}

void
set_search_dat (VcdObj *obj, void *buf)
{
  SearchDat search_dat;

  /* assert (sizeof (SearchDat) == ?) */

  memset (&search_dat, 0, sizeof (search_dat));

  strncpy (search_dat.file_id, SEARCH_FILE_ID, sizeof (SEARCH_FILE_ID));
  
  search_dat.version = SEARCH_VERSION;
  search_dat.scan_points = UINT16_TO_BE (0x0000); /* fixme */
  search_dat.time_interval = SEARCH_TIME_INTERVAL;

  memcpy (buf, &search_dat, sizeof (search_dat));
}

/* eof */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
