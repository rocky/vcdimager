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
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include <libvcd/vcd_assert.h>

#include "vcd_files.h"
#include "vcd_files_private.h"
#include "vcd_bytesex.h"
#include "vcd_obj.h"
#include "vcd_logging.h"
#include "vcd_util.h"
#include "vcd_mpeg_stream.h"

#include <libvcd/vcd_pbc.h>

static const char _rcsid[] = "$Id$";

static uint32_t
_get_closest_aps (const struct vcd_mpeg_source_info *_mpeg_info, double t,
                  struct aps_data *_best_aps)
{
  VcdListNode *node;
  struct aps_data best_aps;
  bool first = true;

  vcd_assert (_mpeg_info != NULL);
  vcd_assert (_mpeg_info->aps_list != NULL);

  _VCD_LIST_FOREACH (node, _mpeg_info->aps_list)
    {
      struct aps_data *_aps = _vcd_list_node_data (node);
  
      if (first)
        {
          best_aps = *_aps;
          first = false;
        }
      else if (fabs (_aps->timestamp - t) < fabs (best_aps.timestamp - t))
        best_aps = *_aps;
      else 
        break;
    }

  if (_best_aps)
    *_best_aps = best_aps;

  return best_aps.packet_no;
}

void
set_entries_vcd (VcdObj *obj, void *buf)
{
  VcdListNode *node = NULL;
  int idx = 0;
  int track_idx = 0;
  EntriesVcd entries_vcd;

  vcd_assert (sizeof(EntriesVcd) == 2048);

  vcd_assert (_vcd_list_length (obj->mpeg_track_list) <= MAX_ENTRIES);
  vcd_assert (_vcd_list_length (obj->mpeg_track_list) > 0);

  memset(&entries_vcd, 0, sizeof(entries_vcd)); /* paranoia / fixme */

  switch (obj->type)
    {
    case VCD_TYPE_VCD:
      strncpy(entries_vcd.ID, ENTRIES_ID_VCD, 8);
      entries_vcd.version = ENTRIES_VERSION_VCD;
      entries_vcd.sys_prof_tag = ENTRIES_SPTAG_VCD;
      break;

    case VCD_TYPE_VCD11:
      strncpy(entries_vcd.ID, ENTRIES_ID_VCD, 8);
      entries_vcd.version = ENTRIES_VERSION_VCD11;
      entries_vcd.sys_prof_tag = ENTRIES_SPTAG_VCD11;
      break;

    case VCD_TYPE_VCD2:
      strncpy(entries_vcd.ID, ENTRIES_ID_VCD, 8);
      entries_vcd.version = ENTRIES_VERSION_VCD2;
      entries_vcd.sys_prof_tag = ENTRIES_SPTAG_VCD2;
      break;

    case VCD_TYPE_SVCD:
      if (!obj->svcd_vcd3_entrysvd)
        strncpy(entries_vcd.ID, ENTRIES_ID_SVCD, 8);
      else
        {
          vcd_warn ("setting ENTRYSVD signature for *DEPRECATED* VCD 3.0 type SVCD");
          strncpy(entries_vcd.ID, ENTRIES_ID_VCD3, 8);
        }
      entries_vcd.version = ENTRIES_VERSION_SVCD;
      entries_vcd.sys_prof_tag = ENTRIES_SPTAG_SVCD;
      break;

    case VCD_TYPE_HQVCD:
      strncpy(entries_vcd.ID, ENTRIES_ID_SVCD, 8);
      entries_vcd.version = ENTRIES_VERSION_HQVCD;
      entries_vcd.sys_prof_tag = ENTRIES_SPTAG_HQVCD;
      break;
      
    default:
      vcd_assert_not_reached ();
      break;
    }


  idx = 0;
  track_idx = 2;
  _VCD_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      mpeg_sequence_t *track = _vcd_list_node_data (node);
      uint32_t lsect = track->relative_start_extent;
      VcdListNode *node2;

      lsect += obj->iso_size;

      entries_vcd.entry[idx].n = to_bcd8(track_idx);
      lba_to_msf(lsect + 150, &(entries_vcd.entry[idx].msf));

      idx++;
      lsect += obj->pre_data_gap;

      _VCD_LIST_FOREACH (node2, track->entry_list)
        {
          entry_t *_entry = _vcd_list_node_data (node2);
          /* additional entries */
          struct aps_data _closest_aps;

          vcd_assert (idx < MAX_ENTRIES);

          _get_closest_aps (track->info, _entry->time, &_closest_aps);

          entries_vcd.entry[idx].n = to_bcd8(track_idx);
          lba_to_msf(lsect + _closest_aps.packet_no + 150,
                     &(entries_vcd.entry[idx].msf));

          vcd_log ((fabs (_closest_aps.timestamp - _entry->time) > 1
                    ? LOG_WARN
                    : LOG_DEBUG),
                   "requested entry point at %f, "
                   "closest possible entry point at %f",
                   _entry->time, _closest_aps.timestamp);

          idx++;
        }

      track_idx++;
    }

  entries_vcd.entry_count = uint16_to_be (idx);

  memcpy(buf, &entries_vcd, sizeof(entries_vcd));
}

static void
_set_bit (uint8_t bitset[], unsigned bitnum)
{
  unsigned _byte = bitnum / 8;
  unsigned _bit  = bitnum % 8;

  bitset[_byte] |= (1 << _bit);
}

uint32_t 
get_psd_size (VcdObj *obj, bool extended)
{
  if (extended)
    vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_PBC_X));

  if (!_vcd_pbc_available (obj))
    return 0;
  
  if (extended)
    return obj->psdx_size;

  return obj->psd_size;
}

void
set_psd_vcd (VcdObj *obj, void *buf, bool extended)
{
  VcdListNode *node;

  if (extended)
    vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_PBC_X));

  vcd_assert (_vcd_pbc_available (obj));

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);
      char *_buf = buf;
      unsigned offset = (extended ? _pbc->offset_ext : _pbc->offset);
      
      vcd_assert (offset % INFO_OFFSET_MULT == 0);

      _vcd_pbc_node_write (obj, _pbc, _buf + offset, extended);
    }
}

void
set_lot_vcd(VcdObj *obj, void *buf, bool extended)
{
  LotVcd *lot_vcd = NULL;
  VcdListNode *node;

  if (extended)
    vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_PBC_X));

  vcd_assert (_vcd_pbc_available (obj));

  lot_vcd = _vcd_malloc (sizeof (LotVcd));
  memset(lot_vcd, 0xff, sizeof(LotVcd));

  lot_vcd->reserved = 0x0000;

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);
      unsigned offset = extended ? _pbc->offset_ext : _pbc->offset;
      
      vcd_assert (offset % INFO_OFFSET_MULT == 0);

      if (_pbc->rejected)
        continue;

      offset /= INFO_OFFSET_MULT;

      lot_vcd->offset[_pbc->lid - 1] = uint16_to_be (offset);
    }

  memcpy(buf, lot_vcd, sizeof(LotVcd));
  free(lot_vcd);
}

void
set_info_vcd(VcdObj *obj, void *buf)
{
  InfoVcd info_vcd;
  VcdListNode *node = NULL;
  int n = 0;

  vcd_assert (sizeof (InfoVcd) == 2048);
  vcd_assert (_vcd_list_length (obj->mpeg_track_list) <= 98);
  
  memset (&info_vcd, 0, sizeof (info_vcd));

  switch (obj->type)
    {
    case VCD_TYPE_VCD:
      strncpy (info_vcd.ID, INFO_ID_VCD, sizeof (info_vcd.ID));
      info_vcd.version = INFO_VERSION_VCD;
      info_vcd.sys_prof_tag = INFO_SPTAG_VCD;
      break;

    case VCD_TYPE_VCD11:
      strncpy (info_vcd.ID, INFO_ID_VCD, sizeof (info_vcd.ID));
      info_vcd.version = INFO_VERSION_VCD11;
      info_vcd.sys_prof_tag = INFO_SPTAG_VCD11;
      break;

    case VCD_TYPE_VCD2:
      strncpy (info_vcd.ID, INFO_ID_VCD, sizeof (info_vcd.ID));
      info_vcd.version = INFO_VERSION_VCD2;
      info_vcd.sys_prof_tag = INFO_SPTAG_VCD2;
      break;

    case VCD_TYPE_SVCD:
      strncpy (info_vcd.ID, INFO_ID_SVCD, sizeof (info_vcd.ID));
      info_vcd.version = INFO_VERSION_SVCD;
      info_vcd.sys_prof_tag = INFO_SPTAG_SVCD;
      break;

    case VCD_TYPE_HQVCD:
      strncpy (info_vcd.ID, INFO_ID_HQVCD, sizeof (info_vcd.ID));
      info_vcd.version = INFO_VERSION_HQVCD;
      info_vcd.sys_prof_tag = INFO_SPTAG_HQVCD;
      break;
      
    default:
      vcd_assert_not_reached ();
      break;
    }
  
  _vcd_strncpy_pad (info_vcd.album_desc, 
                    obj->info_album_id,
                    sizeof(info_vcd.album_desc), VCD_DCHARS); 
  /* fixme, maybe it's VCD_ACHARS? */

  info_vcd.vol_count = uint16_to_be (obj->info_volume_count);
  info_vcd.vol_id = uint16_to_be (obj->info_volume_number);

  if (_vcd_obj_has_cap_p (obj, _CAP_PAL_BITS))
    {
      /* NTSC/PAL bitset */

      n = 0;
      _VCD_LIST_FOREACH (node, obj->mpeg_track_list)
        {
          mpeg_track_t *track = _vcd_list_node_data (node);
          
          if (track->info->norm == MPEG_NORM_PAL 
             || track->info->norm == MPEG_NORM_PAL_S)
            _set_bit(info_vcd.pal_flags, n);
          else if (track->info->vsize == 288 || track->info->vsize == 576)
            {
              vcd_warn ("INFO.{VCD,SVD}: assuming PAL-type resolution for track #%d"
                        " -- are we creating a X(S)VCD?", n);
              _set_bit(info_vcd.pal_flags, n);
            }
        
          n++;
        }
    }

  if (_vcd_obj_has_cap_p (obj, _CAP_PBC))
    {
      info_vcd.flags.restriction = obj->info_restriction;
      info_vcd.flags.use_lid2 = obj->info_use_lid2;
      info_vcd.flags.use_track3 = obj->info_use_seq2;

      if (_vcd_obj_has_cap_p (obj, _CAP_PBC_X)
          &&_vcd_pbc_available (obj))
        info_vcd.flags.pbc_x = true;
      
      info_vcd.psd_size = uint32_to_be (get_psd_size (obj, false));
      info_vcd.offset_mult = _vcd_pbc_available (obj) ? INFO_OFFSET_MULT : 0;
      info_vcd.lot_entries = uint16_to_be (_vcd_pbc_max_lid (obj));
      
      if (_vcd_list_length (obj->mpeg_segment_list))
        {
          unsigned segments = 0;
        
          if (!_vcd_pbc_available (obj))
            vcd_warn ("segment items available, but no PBC items set!"
                      " SPIs will be unreachable");

          _VCD_LIST_FOREACH (node, obj->mpeg_segment_list)
            {
              mpeg_segment_t *segment = _vcd_list_node_data (node);
              unsigned idx;
              InfoSpiContents contents = { 0, };

              contents.audio_type = (int) segment->info->audio_type;
              contents.video_type = (int) segment->info->video_type;

              for (idx = 0; idx < segment->segment_count; idx++)
                {
                  vcd_assert (segments + idx < MAX_SEGMENTS);

                  info_vcd.spi_contents[segments + idx] = contents;
                
                  if (!contents.item_cont)
                    contents.item_cont = true;
                }

              segments += idx;
            }

          info_vcd.item_count = uint16_to_be (segments); 

          lba_to_msf (obj->mpeg_segment_start_extent + 150, &info_vcd.first_seg_addr);
        }
    }

  memcpy(buf, &info_vcd, sizeof(info_vcd));
}

void
set_tracks_svd (VcdObj *obj, void *buf)
{
  char tracks_svd[ISO_BLOCKSIZE] = { 0, };
  TracksSVD *tracks_svd1 = (void *) tracks_svd;
  TracksSVD2 *tracks_svd2;
  VcdListNode *node;
  int n;

  vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_4C_SVCD));

  vcd_assert (sizeof (SVDTrackContent) == 1);

  strncpy (tracks_svd1->file_id, TRACKS_SVD_FILE_ID, sizeof (TRACKS_SVD_FILE_ID));
  tracks_svd1->version = TRACKS_SVD_VERSION;

  tracks_svd1->tracks = _vcd_list_length (obj->mpeg_track_list);

  tracks_svd2 = (void *) &(tracks_svd1->playing_time[tracks_svd1->tracks]);

  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_track_list)
    {
      mpeg_track_t *track = _vcd_list_node_data (node);
      double playtime = track->info->playing_time;
       
      switch (track->info->norm)
        {
        case MPEG_NORM_PAL:
        case MPEG_NORM_PAL_S:
          tracks_svd2->contents[n].video = 0x07;
          break;

        case MPEG_NORM_NTSC:
        case MPEG_NORM_NTSC_S:
          tracks_svd2->contents[n].video = 0x03;
          break;
          
        default:
          if (track->info->vsize == 240 || track->info->vsize == 480)
            {
              vcd_warn ("TRACKS.SVD: assuming NTSC-type resolution for track #%d"
                        " -- are we creating a X(S)VCD?", n);
              tracks_svd2->contents[n].video = 0x03;
            }
          else if (track->info->vsize == 288 || track->info->vsize == 576)
            {
              vcd_warn ("TRACKS.SVD: assuming PAL-type resolution for track #%d"
                        " -- are we creating a X(S)VCD?", n);
              tracks_svd2->contents[n].video = 0x07;
            }
          else
            vcd_warn("SVCD/TRACKS.SVCD: No MPEG video for track #%d?", n);
          break;
        }

      tracks_svd2->contents[n].audio = (int) track->info->audio_type;

      /* setting playtime */
      
      {
        double i, f;

        f = modf(playtime, &i);

        if (playtime >= 6000.0)
          {
            vcd_warn ("SVCD/TRACKS.SVD: playing time value (%d seconds) to great,"
                      " clipping to 100 minutes", (int) i);
            i = 5999.0;
            f = 74.0 / 75.0;
          }

        lba_to_msf (i * 75, &(tracks_svd1->playing_time[n]));
        tracks_svd1->playing_time[n].f = to_bcd8 (floor (f * 75.0));
      }
      
      n++;
    }  
  
  memcpy (buf, &tracks_svd, sizeof(tracks_svd));
}

static double
_get_cumulative_playing_time (const VcdObj *obj, unsigned up_to_track_no)
{
  double result = 0;
  VcdListNode *node;

  _VCD_LIST_FOREACH (node, obj->mpeg_track_list)
    {
      mpeg_track_t *track = _vcd_list_node_data (node);

      if (!up_to_track_no)
        break;

      result += track->info->playing_time;
      up_to_track_no--;
    }
  
  if (up_to_track_no)
    vcd_warn ("internal error...");

  return result;
}

static unsigned 
_get_scanpoint_count (const VcdObj *obj)
{
  double total_playing_time;

  total_playing_time = _get_cumulative_playing_time (obj, _vcd_list_length (obj->mpeg_track_list));

  return ceil (total_playing_time * 2.0);
}

uint32_t 
get_search_dat_size (const VcdObj *obj)
{
  return sizeof (SearchDat) 
    + (_get_scanpoint_count (obj) * sizeof (msf_t));
}

static VcdList *
_make_track_scantable (const VcdObj *obj)
{
  VcdList *all_aps = _vcd_list_new ();
  VcdList *scantable = _vcd_list_new ();
  unsigned scanpoints = _get_scanpoint_count (obj);
  unsigned track_no;
  VcdListNode *node;

  track_no = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_track_list)
    {
      mpeg_track_t *track = _vcd_list_node_data (node);
      VcdListNode *node2;
      
      _VCD_LIST_FOREACH (node2, track->info->aps_list)
        {
          struct aps_data *_data = _vcd_malloc (sizeof (struct aps_data));
          
          *_data = *(struct aps_data *)_vcd_list_node_data (node2);

          _data->timestamp += _get_cumulative_playing_time (obj, track_no);
          _data->packet_no += obj->iso_size + track->relative_start_extent;
          _data->packet_no += obj->pre_data_gap;

          _vcd_list_append (all_aps, _data);
        }
      track_no++;
    }
  
  {
    VcdListNode *aps_node = _vcd_list_begin (all_aps);
    VcdListNode *n;
    struct aps_data *_data;
    double aps_time;
    double playing_time;
    int aps_packet;
    double t;

    playing_time = scanpoints;
    playing_time /= 2;

    vcd_assert (aps_node != NULL);

    _data = _vcd_list_node_data (aps_node);
    aps_time = _data->timestamp;
    aps_packet = _data->packet_no;

    for (t = 0; t < playing_time; t += 0.5)
      {
	for(n = _vcd_list_node_next (aps_node); n; n = _vcd_list_node_next (n))
	  {
	    _data = _vcd_list_node_data (n);

	    if (fabs (_data->timestamp - t) < fabs (aps_time - t))
	      {
		aps_node = n;
		aps_time = _data->timestamp;
		aps_packet = _data->packet_no;
	      }
	    else 
	      break;
	  }

        {
          uint32_t *lsect = _vcd_malloc (sizeof (uint32_t));
          
          *lsect = aps_packet;
          _vcd_list_append (scantable, lsect);
        }
        
      }

  }

  _vcd_list_free (all_aps, true);

  vcd_assert (scanpoints == _vcd_list_length (scantable));

  return scantable;
}

void
set_search_dat (VcdObj *obj, void *buf)
{
  VcdList *scantable;
  VcdListNode *node;
  SearchDat search_dat;
  unsigned n;

  vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_4C_SVCD));
  /* vcd_assert (sizeof (SearchDat) == ?) */

  memset (&search_dat, 0, sizeof (search_dat));

  strncpy (search_dat.file_id, SEARCH_FILE_ID, sizeof (SEARCH_FILE_ID));
  
  search_dat.version = SEARCH_VERSION;
  search_dat.scan_points = uint16_to_be (_get_scanpoint_count (obj));
  search_dat.time_interval = SEARCH_TIME_INTERVAL;

  memcpy (buf, &search_dat, sizeof (search_dat));
  
  scantable = _make_track_scantable (obj);

  n = 0;
  _VCD_LIST_FOREACH (node, scantable)
    {
      SearchDat *search_dat2 = buf;
      uint32_t sect = *(uint32_t *) _vcd_list_node_data (node);
          
      lba_to_msf(sect + 150, &(search_dat2->points[n]));

      n++;
    }

  vcd_assert (n = _get_scanpoint_count (obj));

  _vcd_list_free (scantable, true);
}

static uint32_t 
_get_scandata_count (const struct vcd_mpeg_source_info *info)
{ 
  return ceil (info->playing_time * 2.0);
}

static uint32_t *
_get_scandata_table (const struct vcd_mpeg_source_info *info)
{
  VcdListNode *n, *aps_node = _vcd_list_begin (info->aps_list);
  struct aps_data *_data;
  double aps_time, t;
  int aps_packet;
  uint32_t *retval;
  unsigned i;
  
  retval = _vcd_malloc (_get_scandata_count (info) * sizeof (uint32_t));

  _data = _vcd_list_node_data (aps_node);
  aps_time = _data->timestamp;
  aps_packet = _data->packet_no;

  for (t = 0, i = 0; t < info->playing_time; t += 0.5, i++)
    {
      for(n = _vcd_list_node_next (aps_node); n; n = _vcd_list_node_next (n))
        {
          _data = _vcd_list_node_data (n);

          if (fabs (_data->timestamp - t) < fabs (aps_time - t))
            {
              aps_node = n;
              aps_time = _data->timestamp;
              aps_packet = _data->packet_no;
            }
          else 
            break;
        }

      /* vcd_debug ("%f %f %d", t, aps_time, aps_packet); */

      vcd_assert (i < _get_scandata_count (info));

      retval[i] = aps_packet;
    }

  vcd_assert (i = _get_scandata_count (info));

  return retval;
}

uint32_t 
get_scandata_dat_size (const VcdObj *obj)
{
  uint32_t retval = 0;

  /* struct 1 */
  retval += sizeof (ScandataDat1);
  retval += sizeof (msf_t) * _vcd_list_length (obj->mpeg_track_list);

  /* struct 2 */
  vcd_assert (sizeof (ScandataDat2) == 0);
  retval += sizeof (ScandataDat2);
  retval += sizeof (uint16_t) * 0;

  /* struct 3 */
  retval += sizeof (ScandataDat3);
  retval += (sizeof (uint8_t) + sizeof (uint16_t)) * _vcd_list_length (obj->mpeg_track_list);

  /* struct 4 */
  vcd_assert (sizeof (ScandataDat4) == 0);
  retval += sizeof (ScandataDat4);
  {
    VcdListNode *node;
    _VCD_LIST_FOREACH (node, obj->mpeg_track_list)
      {
        const mpeg_track_t *track = _vcd_list_node_data (node);
        
        retval += sizeof (msf_t) * _get_scandata_count (track->info);
      }
    
  }

  return retval;
}

void
set_scandata_dat (VcdObj *obj, void *buf)
{
  const unsigned tracks = _vcd_list_length (obj->mpeg_track_list);

  ScandataDat1 *scandata_dat1 = (ScandataDat1 *) buf;
  ScandataDat2 *scandata_dat2 = 
    (ScandataDat2 *) &(scandata_dat1->cum_playtimes[tracks]);
  ScandataDat3 *scandata_dat3 =
    (ScandataDat3 *) &(scandata_dat2->spi_indexes[0]);
  ScandataDat4 *scandata_dat4 = 
    (ScandataDat4 *) &(scandata_dat3->mpeg_track_offsets[tracks]);

  const uint16_t _begin_offset =
    offsetof (ScandataDat3, mpeg_track_offsets[tracks])
    - offsetof (ScandataDat3, mpeg_track_offsets);

  VcdListNode *node;
  unsigned n;
  uint16_t _tmp_offset;

  vcd_assert (_vcd_obj_has_cap_p (obj, _CAP_4C_SVCD));

  /* memset (buf, 0, get_scandata_dat_size (obj)); */

  /* struct 1 */
  strncpy (scandata_dat1->file_id, SCANDATA_FILE_ID, sizeof (SCANDATA_FILE_ID));
  
  scandata_dat1->version = SCANDATA_VERSION_SVCD;
  scandata_dat1->reserved = 0x00;
  scandata_dat1->scandata_count = uint16_to_be (_get_scanpoint_count (obj));

  scandata_dat1->track_count = uint16_to_be (tracks);
  scandata_dat1->spi_count = uint16_to_be (0);

  for (n = 0; n < tracks; n++)
    {
      double playtime = _get_cumulative_playing_time (obj, n + 1);
      double i = 0, f = 0;

      f = modf(playtime, &i);

      while (i >= (60 * 100))
        i -= (60 * 100);

      vcd_assert (i >= 0);

      lba_to_msf (i * 75, &(scandata_dat1->cum_playtimes[n]));
      scandata_dat1->cum_playtimes[n].f = to_bcd8 (floor (f * 75.0));
    }

  /* struct 2 -- nothing yet */

  /* struct 3/4 */

  vcd_assert ((_begin_offset % sizeof (msf_t) == 0)
              && _begin_offset > 0);

  _tmp_offset = 0;

  scandata_dat3->mpegtrack_start_index = uint16_to_be (_begin_offset);

  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_track_list)
    {
      const mpeg_track_t *track = _vcd_list_node_data (node);
      uint32_t *_table;
      const unsigned scanpoints = _get_scandata_count (track->info);
      const unsigned _table_ofs =
        (_tmp_offset * sizeof (msf_t)) + _begin_offset;
      unsigned point;

      scandata_dat3->mpeg_track_offsets[n].track_num = n + 2;
      scandata_dat3->mpeg_track_offsets[n].table_offset = uint16_to_be (_table_ofs);

      _table = _get_scandata_table (track->info);

      for (point = 0; point < scanpoints; point++)
        {
          uint32_t lsect = _table[point];

          lsect += track->relative_start_extent;
          lsect += obj->iso_size;
          lsect += obj->pre_data_gap;

          /* vcd_debug ("lsect %d %d", point, lsect); */

          lba_to_msf(lsect + 150,
                     &(scandata_dat4->scandata_table[_tmp_offset + point]));
        }

      free (_table);

      _tmp_offset += scanpoints;
      n++;
    }

  /* struct 4 */

  
}

vcd_type_t
vcd_files_info_detect_type (const void *info_buf)
{
  const InfoVcd *_info = info_buf;
  vcd_type_t _type = VCD_TYPE_INVALID;

  vcd_assert (info_buf != NULL);
  
  if (!strncmp (_info->ID, INFO_ID_VCD, sizeof (_info->ID)))
    switch (_info->version)
      {
      case INFO_VERSION_VCD2:
        if (_info->sys_prof_tag != INFO_SPTAG_VCD2)
          vcd_warn ("INFO.VCD: unexpected system profile tag encountered");
        _type = VCD_TYPE_VCD2;
        break;

      case INFO_VERSION_VCD:
   /* case INFO_VERSION_VCD11: */
        switch (_info->sys_prof_tag)
          {
          case INFO_SPTAG_VCD:
            _type = VCD_TYPE_VCD;
            break;
          case INFO_SPTAG_VCD11:
            _type = VCD_TYPE_VCD11;
            break;
          default:
            vcd_warn ("INFO.VCD: unexpected system profile tag encountered, assuming VCD 1.1");
            break;
          }
        break;

      default:
        vcd_warn ("unexpected vcd version encountered -- assuming vcd 2.0");
        break;
      }
  else if (!strncmp (_info->ID, INFO_ID_SVCD, sizeof (_info->ID)))
    switch (_info->version) 
      {
      case INFO_VERSION_SVCD:
        if (_info->sys_prof_tag != INFO_SPTAG_SVCD)
          vcd_warn ("INFO.SVD: unexpected system profile tag value -- assuming svcd");
        _type = VCD_TYPE_SVCD;
        break;
        
      default:
        vcd_warn ("INFO.SVD: unexpected version value seen -- still assuming svcd");
        _type = VCD_TYPE_SVCD;
        break;
      }
  else if (!strncmp (_info->ID, INFO_ID_HQVCD, sizeof (_info->ID)))
    switch (_info->version) 
      {
      case INFO_VERSION_HQVCD:
  if (_info->sys_prof_tag != INFO_SPTAG_HQVCD)
          vcd_warn ("INFO.SVD: unexpected system profile tag value -- assuming hqvcd");
        _type = VCD_TYPE_HQVCD;
        break;
        
      default:
        vcd_warn ("INFO.SVD: unexpected version value seen -- still assuming hqvcd");
        _type = VCD_TYPE_HQVCD;
        break;
      }
  else
    vcd_warn ("INFO.SVD: signature not found");
  
  return _type;
}

/* eof */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
