/*
    $Id$

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>

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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <popt.h>

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_bitvec.h>
#include <libvcd/vcd_types.h>
#include <libvcd/vcd_bytesex.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_files.h>
#include <libvcd/vcd_files_private.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_iso9660_private.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_salloc.h>
#include <libvcd/vcd_stream_stdio.h>
#include <libvcd/vcd_image.h>
#include <libvcd/vcd_image_bincue.h>
#include <libvcd/vcd_image_linuxcd.h>
#include <libvcd/vcd_data_structures.h>

static const char _rcsid[] = "$Id$";

static const char DELIM[] = \
"----------------------------------------" \
"---------------------------------------\n";

#define BUF_COUNT 16
#define BUF_SIZE 80

static char *
_getbuf (void)
{
  static char _buf[BUF_COUNT][BUF_SIZE];
  static int _num = -1;
  
  _num++;
  _num %= BUF_COUNT;

  return _buf[_num];
}

static bool
_bitset_get_bit (const uint8_t bitvec[], int bit)
{
  bool result = false;
  
  if (_vcd_bit_set_p (bitvec[bit / 8], (bit % 8)))
    result = true;

  return result;
}

static const char *
_pin2str (uint16_t itemid)
{
  char *buf = _getbuf ();

  strcpy (buf, "??");

  if (itemid < 2) 
    snprintf (buf, BUF_SIZE, "no item (0x%4.4x)", itemid);
  else if (itemid < 100)
    snprintf (buf, BUF_SIZE, "SEQUENCE[%d] (0x%4.4x)", itemid - 2, itemid);
  else if (itemid < 600)
    snprintf (buf, BUF_SIZE, "ENTRY[%d] (0x%4.4x)", itemid - 100, itemid);
  else if (itemid < 1000)
    snprintf (buf, BUF_SIZE, "spare id (0x%4.4x)", itemid);
  else if (itemid < 2980)
    snprintf (buf, BUF_SIZE, "SEGMENT[%d] (0x%4.4x)", itemid-1000, itemid);
  else 
    snprintf (buf, BUF_SIZE, "spare id (0x%4.4x)", itemid);

  return buf;
}

#if 0
static void
_hexdump_full (VcdImageSource *img, char *buf, int len)
{
  int i;

  for (i = 0; i < len; i++)
    {
      if (i && !(i % 16))
        fprintf (fd, "\n");

      if (!(i % 16))
        fprintf (fd, "%.4x  ", i);

      fprintf (fd, "%.2x ", buf[i]);
    }
  fprintf (fd, "\n");
}
#endif

/******************************************************************************/

static int gl_verbose_flag = false;
static int gl_quiet_flag = false;

typedef struct {
  vcd_type_t vcd_type;

  struct iso_primary_descriptor pvd;

  InfoVcd info;
  EntriesVcd entries;
  
  LotVcd *lot;
  uint8_t *psd;

  VcdList *offset_list;

  void *tracks_buf;
  void *search_buf;
} debug_obj_t;

/******************************************************************************/

static inline uint32_t
_get_psd_size (const debug_obj_t *obj)
{
  return UINT32_FROM_BE (obj->info.psd_size);
}

static void
_hexdump (const void *data, unsigned len)
{
  unsigned n;
  const uint8_t *bytes = data;

  for (n = 0; n < len; n++)
    {
      if (n % 8 == 0)
        fprintf (stdout, " ");
      fprintf (stdout, "%2.2x ", bytes[n]);
    }
}

static int
_calc_psd_wait_time (uint16_t wtime)
{
  if (wtime < 61)
    return wtime;
  else if (wtime < 255)
    return (wtime - 60) * 10 + 60;
  else
    return -1;
}

typedef struct {
  uint16_t lid;
  uint16_t offset;
  bool in_lot;
} offset_t;

static const char *
_ofs2str (const debug_obj_t *obj, unsigned offset)
{
  VcdListNode *node;
  char *buf;

  if (offset == 0xffff)
    return "disabled";

  buf = _getbuf ();

  _VCD_LIST_FOREACH (node, obj->offset_list)
    {
      offset_t *ofs = _vcd_list_node_data (node);

      if (offset == ofs->offset)
        {
          if (ofs->lid)
            snprintf (buf, BUF_SIZE, "LID[%d] @0x%4.4x", 
                      ofs->lid, ofs->offset);
          else 
            snprintf (buf, BUF_SIZE, "PSD[?] @0x%4.4x", 
                      ofs->offset);
          return buf;
        }
    }
  
  snprintf (buf, BUF_SIZE, "? @0x%4.4x", offset);
  return buf;
}

static void
_visit_pbc (debug_obj_t *obj, unsigned lid, unsigned offset, bool in_lot)
{
  VcdListNode *node;
  offset_t *ofs;
  unsigned psd_size = _get_psd_size (obj);
  unsigned _rofs = offset * obj->info.offset_mult;

  vcd_assert (psd_size % 8 == 0);

  if (offset == 0xffff)
    return;

  vcd_assert (_rofs < psd_size);

  if (!obj->offset_list)
    obj->offset_list = _vcd_list_new ();

  _VCD_LIST_FOREACH (node, obj->offset_list)
    {
      ofs = _vcd_list_node_data (node);

      if (offset == ofs->offset)
        {
          if (in_lot)
            ofs->in_lot = true;

          return; /* already been there... */
        }
    }

  ofs = _vcd_malloc (sizeof (offset_t));

  ofs->offset = offset;
  ofs->lid = lid;
  ofs->in_lot = in_lot;

  switch (obj->psd[_rofs])
    {
    case PSD_TYPE_PLAY_LIST:
      _vcd_list_append (obj->offset_list, ofs);
      {
        const PsdPlayListDescriptor *d = 
          (const void *) (obj->psd + _rofs);

        if (!ofs->lid)
          ofs->lid = UINT16_FROM_BE (d->lid) & 0x7fff;
        else 
          if (ofs->lid != (UINT16_FROM_BE (d->lid) & 0x7fff))
            vcd_warn ("LOT entry assigned LID %d, but descriptor has LID %d",
                      ofs->lid, UINT16_FROM_BE (d->lid) & 0x7fff);

        _visit_pbc (obj, 0, UINT16_FROM_BE (d->prev_ofs), false);
        _visit_pbc (obj, 0, UINT16_FROM_BE (d->next_ofs), false);
        _visit_pbc (obj, 0, UINT16_FROM_BE (d->return_ofs), false);
      }
      break;

    case PSD_TYPE_SELECTION_LIST:
      _vcd_list_append (obj->offset_list, ofs);
      {
        const PsdSelectionListDescriptor *d =
          (const void *) (obj->psd + _rofs);

        int idx;

        if (!ofs->lid)
          ofs->lid = UINT16_FROM_BE (d->lid) & 0x7fff;
        else 
          if (ofs->lid != (UINT16_FROM_BE (d->lid) & 0x7fff))
            vcd_warn ("LOT entry assigned LID %d, but descriptor has LID %d",
                      ofs->lid, UINT16_FROM_BE (d->lid) & 0x7fff);

        _visit_pbc (obj, 0, UINT16_FROM_BE (d->prev_ofs), false);
        _visit_pbc (obj, 0, UINT16_FROM_BE (d->next_ofs), false);
        _visit_pbc (obj, 0, UINT16_FROM_BE (d->return_ofs), false);
        _visit_pbc (obj, 0, UINT16_FROM_BE (d->default_ofs), false);
        _visit_pbc (obj, 0, UINT16_FROM_BE (d->timeout_ofs), false);

        for (idx = 0; idx < d->nos; idx++)
          _visit_pbc (obj, 0, UINT16_FROM_BE (d->ofs[idx]), false);
        
      }
      break;

    case PSD_TYPE_END_LIST:
      _vcd_list_append (obj->offset_list, ofs);
      break;

    default:
      vcd_warn ("corrupt PSD???????");
      free (ofs);
      return;
      break;
    }
}

static int
_offset_t_cmp (offset_t *a, offset_t *b)
{
  if (a->offset > b->offset)
    return 1;

  if (a->offset < b->offset)
    return -1;
  
  return 0;
}

static void
_visit_lot (debug_obj_t *obj)
{
  const LotVcd *lot = obj->lot;
  unsigned n, tmp;

  if (!_get_psd_size (obj))
    return;

  for (n = 0; n < LOT_VCD_OFFSETS; n++)
    if ((tmp = UINT16_FROM_BE (lot->offset[n])) != 0xFFFF)
      _visit_pbc (obj, n + 1, tmp, true);

  _vcd_list_sort (obj->offset_list, (_vcd_list_cmp_func) _offset_t_cmp);
}

static void
dump_lot (const debug_obj_t *obj)
{
  const LotVcd *lot = obj->lot;
  
  unsigned n, tmp;
  unsigned max_lid = UINT16_FROM_BE (obj->info.lot_entries);
  unsigned mult = obj->info.offset_mult;

  fprintf (stdout, 
           obj->vcd_type == VCD_TYPE_SVCD 
           ? "SVCD/LOT.SVD\n"
           : "VCD/LOT.VCD\n");

  if (lot->reserved)
    fprintf (stdout, " RESERVED = 0x%4.4x (should be 0x0000)\n", UINT16_FROM_BE (lot->reserved));

  for (n = 0; n < LOT_VCD_OFFSETS; n++)
    {
      if ((tmp = UINT16_FROM_BE (lot->offset[n])) != 0xFFFF)
        {
          if (!n && tmp)
            fprintf (stdout, "warning, LID[1] should have offset = 0!\n");

          if (n >= max_lid)
            fprintf (stdout, "warning, the following entry is greater than the maximum lid field in info\n");
          fprintf (stdout, " LID[%d]: offset = %d (0x%4.4x)\n", 
                   n + 1, tmp * mult, tmp);
        }
      else if (n < max_lid)
        fprintf (stdout, " LID[%d]: rejected\n", n + 1);
    }
}

static void
dump_psd (const debug_obj_t *obj)
{
  VcdListNode *node;
  unsigned n = 0;
  unsigned mult = obj->info.offset_mult;

  fprintf (stdout, 
           obj->vcd_type == VCD_TYPE_SVCD 
           ? "SVCD/PSD.SVD\n"
           : "VCD/PSD.VCD\n");

  _VCD_LIST_FOREACH (node, obj->offset_list)
    {
      offset_t *ofs = _vcd_list_node_data (node);
      unsigned _rofs = ofs->offset * mult;

      uint8_t type;
      
      type = obj->psd[_rofs];

      switch (type)
        {
        case PSD_TYPE_PLAY_LIST:
          {
            const PsdPlayListDescriptor *d = (const void *) (obj->psd + _rofs);
            int i;

            fprintf (stdout,
                     " PSD[%.2d] (%s): play list descriptor\n"
                     "  NOI: %d | LID#: %d (rejected: %s)\n"
                     "  prev: %s | next: %s | return: %s\n"
                     "  ptime: %d/15s | wait: %ds | await: %ds\n",
                     n, _ofs2str (obj, ofs->offset),
                     d->noi,
                     UINT16_FROM_BE (d->lid) & 0x7fff,
                     _vcd_bool_str (UINT16_FROM_BE (d->lid) & 0x8000),
                     _ofs2str (obj, UINT16_FROM_BE (d->prev_ofs)),
                     _ofs2str (obj, UINT16_FROM_BE (d->next_ofs)),
                     _ofs2str (obj, UINT16_FROM_BE (d->return_ofs)),
                     UINT16_FROM_BE (d->ptime),
                     _calc_psd_wait_time (d->wtime),
                     _calc_psd_wait_time (d->atime));

            for (i = 0; i < d->noi; i++)
              {
                fprintf (stdout, "  item[%d]: %s\n",
                         i, _pin2str (UINT16_FROM_BE (d->itemid[i])));
              }
            fprintf (stdout, "\n");
          }
          break;
        case PSD_TYPE_END_LIST:
          fprintf (stdout, " PSD[%.2d] (%s): end list descriptor\n", n, _ofs2str (obj, ofs->offset));
          fprintf (stdout, "\n");
          break;

        case PSD_TYPE_SELECTION_LIST:
          {
            const PsdSelectionListDescriptor *d =
              (const void *) (obj->psd + _rofs);
            int i;

            fprintf (stdout,
                     "  PSD[%.2d] (%s): selection list descriptor\n"
                     "  NOS: %d | BSN: %d | LID: %d (rejected: %s)\n"
                     "  prev: %s | next: %s | return: %s\n"
                     "  default: %s | timeout: %s\n"
                     "  totime: %ds | loop: %d (delayed: %s)\n"
                     "  item: %s\n",
                     n, _ofs2str (obj, ofs->offset),
                     d->nos, 
                     d->bsn,
                     UINT16_FROM_BE (d->lid) & 0x7fff,
                     _vcd_bool_str (UINT16_FROM_BE (d->lid) & 0x8000),
                     _ofs2str (obj, UINT16_FROM_BE (d->prev_ofs)),
                     _ofs2str (obj, UINT16_FROM_BE (d->next_ofs)),
                     _ofs2str (obj, UINT16_FROM_BE (d->return_ofs)),
                     _ofs2str (obj, UINT16_FROM_BE (d->default_ofs)),
                     _ofs2str (obj, UINT16_FROM_BE (d->timeout_ofs)),
                     _calc_psd_wait_time(d->totime),
                     0x7f & d->loop, _vcd_bool_str (0x80 & d->loop),
                     _pin2str (UINT16_FROM_BE (d->itemid)));

            for (i = 0; i < d->nos; i++)
              fprintf (stdout, "  ofs[%d]: %s\n", i,
                       _ofs2str (obj, UINT16_FROM_BE (d->ofs[i])));


#if defined(EXTENDED_PSD)
            /* this is just for documentation... */

            {
              const PsdSelectionListDescriptorExtended *d2 =
                (const void *) &(d->ofs[d->nos]);

              fprintf (stdout, "  prev_area: (%d,%d) (%d,%d) ",
                       d2->prev_area.x1, d2->prev_area.y1, 
                       d2->prev_area.x2, d2->prev_area.y2);

              fprintf (stdout, "| next_area: (%d,%d) (%d,%d)\n",
                       d2->next_area.x1, d2->next_area.y1, 
                       d2->next_area.x2, d2->next_area.y2);

              fprintf (stdout, "  return_area: (%d,%d) (%d,%d) ",
                       d2->return_area.x1, d2->return_area.y1, 
                       d2->return_area.x2, d2->return_area.y2);

              fprintf (stdout, "| default_area: (%d,%d) (%d,%d)\n",
                       d2->default_area.x1, d2->default_area.y1, 
                       d2->default_area.x2, d2->default_area.y2);

              for (i = 0; i < d->nos; i++)
                fprintf (stdout, "  area[%d]: (%d,%d) (%d,%d)\n", i,
                         d2->area[i].x1, d2->area[i].y1, 
                         d2->area[i].x2, d2->area[i].y2);
            }
#endif

            fprintf (stdout, "\n");
          }
          break;
        default:
          fprintf (stdout, " PSD[%2d] (%s): unkown descriptor type (0x%2.2x)\n", 
                   n, _ofs2str (obj, ofs->offset), type);

          fprintf (stdout, "  hexdump: ");
          _hexdump (&obj->psd[_rofs], 24);
          fprintf (stdout, "\n");
          break;
        }

      n++;
    }
}

static vcd_type_t
detect_type (debug_obj_t *obj)
{
  obj->vcd_type = VCD_TYPE_INVALID;

  if (!strncmp (obj->info.ID, INFO_ID_VCD, sizeof (obj->info.ID)))
    switch (obj->info.version)
      {
      case INFO_VERSION_VCD2:
        if (obj->info.sys_prof_tag != INFO_SPTAG_VCD2)
          vcd_warn ("unexpected system profile tag encountered");
        obj->vcd_type = VCD_TYPE_VCD2;
        break;

      case INFO_VERSION_VCD11:
        if (obj->info.sys_prof_tag != INFO_SPTAG_VCD11)
          vcd_warn ("unexpected system profile tag encountered");
        obj->vcd_type = VCD_TYPE_VCD11;
        break;

      default:
        vcd_warn ("unexpected vcd version encountered -- assuming vcd 2.0");
        break;
      }
  else if (!strncmp (obj->info.ID, INFO_ID_SVCD, sizeof (obj->info.ID)))
    switch (obj->info.version) 
      {
      case INFO_VERSION_SVCD:
        if (obj->info.sys_prof_tag != INFO_SPTAG_VCD2)
          vcd_warn ("unexpected system profile tag value -- assuming svcd");
        obj->vcd_type = VCD_TYPE_SVCD;
        break;
        
      default:
        vcd_warn ("unexpected svcd version...");
        obj->vcd_type = VCD_TYPE_SVCD;
        break;
      }

  switch (obj->vcd_type)
    {
    case VCD_TYPE_VCD11:
      fprintf (stdout, "VCD 1.1 detected\n");
      break;
    case VCD_TYPE_VCD2:
      fprintf (stdout, "VCD 2.0 detected\n");
      break;
    case VCD_TYPE_SVCD:
      fprintf (stdout, "SVCD detected\n");
      break;
    case VCD_TYPE_INVALID:
      fprintf (stderr, "unknown ID encountered -- maybe not a proper (S)VCD?\n");
      exit (EXIT_FAILURE);
      break;
    }

  return obj->vcd_type;
}

static void
dump_info (const debug_obj_t *obj)
{
  const InfoVcd *info = &obj->info;
  int n;

  fprintf (stdout, 
           obj->vcd_type == VCD_TYPE_SVCD 
           ? "SVCD/INFO.SVD\n" 
           : "VCD/INFO.VCD\n");

  fprintf (stdout, " ID: `%.8s'\n", info->ID);
  fprintf (stdout, " version: 0x%2.2x\n", info->version);
  fprintf (stdout, " system profile tag: 0x%2.2x\n", info->sys_prof_tag);
  fprintf (stdout, " album desc: `%.16s'\n", info->album_desc);
  fprintf (stdout, " volume count: %d\n", UINT16_FROM_BE (info->vol_count));
  fprintf (stdout, " volume number: %d\n", UINT16_FROM_BE (info->vol_id));

  fprintf (stdout, " pal flags:");
  for (n = 0; n < 98; n++)
    {
      if (n == 48)
        fprintf (stdout, "\n           ");

      fprintf (stdout, n % 8 ? "%d" : " %d",
               _bitset_get_bit (info->pal_flags, n));
    }
  fprintf (stdout, "\n");

  fprintf (stdout, " flags:\n");
  fprintf (stdout, "  restriction: %d\n", info->flags.restriction);
  fprintf (stdout, "  special info: %s\n", _vcd_bool_str (info->flags.special_info));
  fprintf (stdout, "  user data cc: %s\n", _vcd_bool_str (info->flags.user_data_cc));
  fprintf (stdout, "  start lid #2: %s\n", _vcd_bool_str (info->flags.use_lid2));
  fprintf (stdout, "  start track #2: %s\n", _vcd_bool_str (info->flags.use_track3));

  fprintf (stdout, " psd size: %d\n", UINT32_FROM_BE (info->psd_size));
  fprintf (stdout, " first segment addr: %2.2x:%2.2x:%2.2x\n",
           info->first_seg_addr.m, info->first_seg_addr.s, info->first_seg_addr.f);

  fprintf (stdout, " offset multiplier: 0x%2.2x (must be 0x08)\n", info->offset_mult);

  fprintf (stdout, " maximum lid: %d\n",
           UINT16_FROM_BE (info->lot_entries));

  fprintf (stdout, " maximum segment number: %d\n", UINT16_FROM_BE (info->item_count));

  for (n = 0; n < UINT16_FROM_BE (info->item_count); n++)
    {
      const char *audio_types[] =
        {
          "no stream",
          "1 stream",
          "2 streams",
          "1 multi-channel stream (surround sound)"
        };

      const char *video_types[] =
        {
          "no stream",
          "NTSC still",
          "NTSC still (hires)",
          "NTSC motion",
          "reserved (0x4)",
          "PAL still",
          "PAL still (hires)",
          "PAL motion"
        };

      fprintf (stdout, " SEGMENT[%d]: audio: %s,"
               " video: %s, continuation %s\n",
               n,
               audio_types[info->spi_contents[n].audio_type],
               video_types[info->spi_contents[n].video_type],
               _vcd_bool_str (info->spi_contents[n].item_cont));
    }
}

static void
dump_entries (const debug_obj_t *obj)
{
  const EntriesVcd *entries = &obj->entries;
  int ntracks, n;

  ntracks = UINT16_FROM_BE (entries->entry_count);

  fprintf (stdout, 
           obj->vcd_type == VCD_TYPE_SVCD 
           ? "SVCD/ENTRIES.SVD\n"
           : "VCD/ENTRIES.VCD\n");

  if (!strncmp (entries->ID, ENTRIES_ID_VCD, sizeof (entries->ID)))
    { /* noop */ }
  else if (!strncmp (entries->ID, "ENTRYSVD", sizeof (entries->ID)))
    vcd_warn ("found obsolete (S)VCD3.0 ENTRIES.SVD signature");
  else
    vcd_warn ("unexpected ID signature encountered");

  fprintf (stdout, " ID: `%.8s'\n", entries->ID);
  fprintf (stdout, " version: 0x%2.2x\n", entries->version);
  fprintf (stdout, " system profile tag: 0x%2.2x\n", entries->sys_prof_tag);

  fprintf (stdout, " entries: %d\n", ntracks);

  for (n = 0; n < ntracks; n++)
    {
      const msf_t *msf = &(entries->entry[n].msf);
      uint32_t extent = msf_to_lba(msf);
      extent -= 150;

      fprintf (stdout, " ENTRY[%2.2d]: track# %d, LSN %d "
               "(msf: %2.2x:%2.2x:%2.2x)\n",
               n, from_bcd8 (entries->entry[n].n), extent,
               msf->m, msf->s, msf->f);
    }
}

static void
dump_tracks_svd (const debug_obj_t *obj)
{
  const TracksSVD *tracks = obj->tracks_buf;
  const TracksSVD2 *tracks2 = (const void *) &(tracks->playing_time[tracks->tracks]);
 
  unsigned j;

  fprintf (stdout, "SVCD/TRACKS.SVD\n");
  fprintf (stdout, " ID: `%.8s'\n", tracks->file_id);
  fprintf (stdout, " version: 0x%2.2x\n", tracks->version);
  
  fprintf (stdout, " tracks: %d\n", tracks->tracks);
  
  for (j = 0;j < tracks->tracks; j++)
    {
      const char *audio_types[] =
        {
          "no stream",
          "1 stream",
          "2 streams",
          "ext MC stream"
        };

      const char *video_types[] =
        {
          "no stream",
          "unknown (0x1)",
          "unknown (0x2)",
          "NTSC stream",
          "unknown (0x4)",
          "unknown (0x5)",
          "unknown (0x6)",
          "PAL stream",
        };

      fprintf (stdout, " track[%.2d]: %2.2x:%2.2x:%2.2x, audio: %s, video: %s\n",
               j,
               tracks->playing_time[j].m,
               tracks->playing_time[j].s,
               tracks->playing_time[j].f,
               audio_types[tracks2->contents[j].audio],
               video_types[tracks2->contents[j].video]);
    }
}

static void
dump_search_dat (const debug_obj_t *obj)
{
  const int _printed_points = 15;
  const SearchDat *searchdat = obj->search_buf;
  unsigned m;
  uint32_t scan_points = UINT16_FROM_BE (searchdat->scan_points);

  fprintf (stdout, "SVCD/SEARCH.DAT\n");
  fprintf (stdout, " ID: `%.8s'\n", searchdat->file_id);
  fprintf (stdout, " version: 0x%2.2x\n", searchdat->version);
  fprintf (stdout, " scanpoints: %d\n", scan_points);
  fprintf (stdout, " scaninterval: %d (in 0.5sec units -- must be `1')\n", 
           searchdat->time_interval);

  for (m = 0; m < scan_points;m++)
    {
      unsigned hh, mm, ss, ss2;

      if (!gl_verbose_flag 
          && m > _printed_points && m < scan_points - _printed_points)
        continue;
      
      ss2 = m * searchdat->time_interval;

      hh = ss2 / (2 * 60 * 60);
      mm = (ss2 / (2 * 60)) % 60;
      ss = (ss2 / 2) % 60;
      ss2 = (ss2 % 2) * 5;

      fprintf (stdout, " scanpoint[%.4d]: (real time: %.2d:%.2d:%.2d.%.1d) "
               " sector: %.2x:%.2x:%.2x \n", m, hh, mm, ss, ss2,
               searchdat->points[m].m,
               searchdat->points[m].s,
               searchdat->points[m].f);
      
      if (!gl_verbose_flag
          && m == _printed_points && scan_points > (_printed_points * 2))
        fprintf (stdout, " [..skipping...]\n");
    }
}

static const char *
_strip_trail (const char str[], size_t n)
{
  static char buf[1024];
  int j;

  vcd_assert (n < 1024);

  strncpy (buf, str, n);
  buf[n] = '\0';

  for (j = strlen (buf) - 1; j >= 0; j--)
    {
      if (buf[j] != ' ')
        break;

      buf[j] = '\0';
    }

  return buf;
}

static void
dump_pvd (const debug_obj_t *obj)
{
  struct iso_primary_descriptor const *pvd = &obj->pvd;

  fprintf (stdout, "ISO9660 primary volume descriptor\n");

  if (pvd->type != ISO_VD_PRIMARY)
    vcd_warn ("unexcected descriptor type");

  if (strncmp (pvd->id, ISO_STANDARD_ID, strlen (ISO_STANDARD_ID)))
    vcd_warn ("unexpected ID encountered (expected `" ISO_STANDARD_ID "'");
  
  fprintf (stdout, " ID: `%.5s'\n", pvd->id);
  fprintf (stdout, " version: %d\n", pvd->version);
  fprintf (stdout, " system id: `%s'\n",
           _strip_trail (pvd->system_id, 32));
  fprintf (stdout, " volume id: `%s'\n",
           _strip_trail (pvd->volume_id, 32));

  fprintf (stdout, " volumeset id: `%s'\n", 
           _strip_trail (pvd->volume_set_id, 128));
  fprintf (stdout, " publisher id: `%s'\n", 
           _strip_trail (pvd->publisher_id, 128));
  fprintf (stdout, " preparer id: `%s'\n", 
           _strip_trail (pvd->preparer_id, 128));
  fprintf (stdout, " application id: `%s'\n", 
           _strip_trail (pvd->application_id, 128));
  
  fprintf (stdout, " ISO size: %d blocks (logical blocksize: %d bytes)\n", 
           from_733 (pvd->volume_space_size),
           from_723 (pvd->logical_block_size));
  
  fprintf (stdout, " XA marker present: %s\n", 
           _vcd_bool_str (!strncmp ((char *) pvd + ISO_XA_MARKER_OFFSET, 
                               ISO_XA_MARKER_STRING, 
                               strlen (ISO_XA_MARKER_STRING))));
}

static void
dump_all (const debug_obj_t *obj)
{
  fprintf (stdout, DELIM);
  dump_pvd (obj);
  fprintf (stdout, DELIM);
  dump_info (obj);
  fprintf (stdout, DELIM);
  dump_entries (obj);
  if (_get_psd_size (obj))
    {
      fprintf (stdout, DELIM);
      dump_lot (obj);
      fprintf (stdout, DELIM);
      dump_psd (obj);
    }

  if (obj->tracks_buf)
    {
      fprintf (stdout, DELIM);
      dump_tracks_svd (obj);
    }

  if (obj->search_buf)
    {
      fprintf (stdout, DELIM);
      dump_search_dat (obj);
    }

  fprintf (stdout, DELIM);
}

static uint32_t
find_sect_by_fileid (VcdImageSource *img, uint32_t start, uint32_t end, 
                     const char file_id[])
{
  uint32_t sect;

  vcd_assert (strlen (file_id) == 8);

  for (sect = start; sect < end; sect++)
    {
      char _buf[ISO_BLOCKSIZE] = { 0, };
      
      if (vcd_image_source_read_mode2_sector (img, _buf, sect, false)) 
        break; 

      if (!strncmp (_buf, file_id, 8))
        return sect;
    }
  
  return SECTOR_NIL;
}

static void
dump (VcdImageSource *img, const char image_fname[])
{
  unsigned size, psd_size;
  debug_obj_t obj;

  memset (&obj, 0, sizeof (debug_obj_t));

  size = vcd_image_source_stat_size (img);
  
  if (vcd_image_source_read_mode2_sector (img, &(obj.pvd), ISO_PVD_SECTOR, false))
    exit (EXIT_FAILURE);

  if (vcd_image_source_read_mode2_sector (img, &obj.info, INFO_VCD_SECTOR, false))
    exit (EXIT_FAILURE);

  if (detect_type (&obj) == VCD_TYPE_INVALID)
    {
      vcd_error ("VCD detection failed - aborting -- maybe wrong image type?");
      exit (EXIT_FAILURE);
    }

  psd_size = _get_psd_size (&obj);

  if (vcd_image_source_read_mode2_sector (img, &obj.entries, ENTRIES_VCD_SECTOR, false))
    exit (EXIT_FAILURE);
  
  if (psd_size)
    {
      uint32_t n;

      if (psd_size > 256*1024)
        {
          vcd_error ("weird psd size (%u) -- aborting", psd_size);
          exit (EXIT_FAILURE);
        }

      obj.lot = _vcd_malloc (ISO_BLOCKSIZE * 32);
      obj.psd = _vcd_malloc (ISO_BLOCKSIZE 
                             * (((psd_size - 1) / ISO_BLOCKSIZE) + 1));
      
      for (n = LOT_VCD_SECTOR; n < PSD_VCD_SECTOR; n++)
        {
          char *p = (char *) obj.lot;
          p += ISO_BLOCKSIZE * (n - LOT_VCD_SECTOR);

          if (vcd_image_source_read_mode2_sector (img, p, n, false))
            exit (EXIT_FAILURE);
        }

      for (n = PSD_VCD_SECTOR;
           n < PSD_VCD_SECTOR + ((psd_size - 1) / ISO_BLOCKSIZE) + 1; n++)
        {
          char *p = obj.psd + (ISO_BLOCKSIZE * (n - PSD_VCD_SECTOR));

          if (vcd_image_source_read_mode2_sector (img, p, n, false))
            exit (EXIT_FAILURE);
        }
    }

  {
    char tmp[ISO_BLOCKSIZE] = { 0, };
    uint32_t n;

    n = find_sect_by_fileid (img, LOT_VCD_SECTOR, 225, TRACKS_SVD_FILE_ID);

    if (n != SECTOR_NIL)
      {
        obj.tracks_buf = _vcd_malloc (ISO_BLOCKSIZE);
        
        if (vcd_image_source_read_mode2_sector (img, obj.tracks_buf, n, false))
          exit (EXIT_FAILURE);

        vcd_debug ("found TRACKS.SVD signature at sector %d", n);
      }
    else
      vcd_debug ("no TRACKS.SVD signature found");

    n = find_sect_by_fileid (img, LOT_VCD_SECTOR, 225, SEARCH_FILE_ID);
    
    if (n != SECTOR_NIL)
      {
        uint32_t m;

        uint32_t size;
        uint32_t sectors;

        if (vcd_image_source_read_mode2_sector (img, tmp, n, false))
          exit (EXIT_FAILURE);

        size = (3 * UINT16_FROM_BE (((SearchDat *)tmp)->scan_points)) 
          + sizeof (SearchDat);

        sectors = _vcd_len2blocks (size, ISO_BLOCKSIZE);

        vcd_debug ("found SEARCH.DAT signature at sector %d", n);

        obj.search_buf = _vcd_malloc (sectors * ISO_BLOCKSIZE);

        for (m = 0; m < sectors;m++)
          {
            char *p = obj.search_buf;
            p += ISO_BLOCKSIZE * m;
            
            if (vcd_image_source_read_mode2_sector (img, p, m + n, false))
              exit (EXIT_FAILURE);
          }
      }
    else
      vcd_debug ("no SEARCH.DAT signature found");

    
  }
  
  fprintf (stdout, DELIM);

  fprintf (stdout, "VCDdebug - GNU VCDImager - (Super) Video CD Report\n"
           "%s\n\n", _rcsid);
  fprintf (stdout, "Source: image file `%s'\n", image_fname);
  fprintf (stdout, "Image size: %d sectors\n", size);

  _visit_lot (&obj);

  dump_all (&obj);

  free (obj.lot);
  free (obj.psd);
  free (obj.tracks_buf);
  free (obj.search_buf);
}

/* global static vars */

static char *gl_source_name = NULL;

static enum
{
  SOURCE_NONE = 0,
  SOURCE_FILE = 1,
  SOURCE_DEVICE = 2
}
gl_source_type = SOURCE_NONE;

/* end of vars */

static vcd_log_handler_t gl_default_vcd_log_handler = NULL;

static void 
_vcd_log_handler (log_level_t level, const char message[])
{
  if (level == LOG_DEBUG && !gl_verbose_flag)
    return;

  if (level == LOG_INFO && gl_quiet_flag)
    return;
  
  gl_default_vcd_log_handler (level, message);
}

static int gl_sector_2336_flag = 0;

enum {
  OP_VERSION = 1 << 7
};

int
main (int argc, const char *argv[])
{
  int opt;

  struct poptOption optionsTable[] = {

    {"bin-file", 'b', POPT_ARG_STRING, &gl_source_name, SOURCE_FILE,
     "set image file as source", "FILE"},

    {"sector-2336", '\0', POPT_ARG_NONE, &gl_sector_2336_flag, 0,
     "use 2336 byte sector mode for image file"},

#ifdef __linux__
    {"cdrom-device", '\0', POPT_ARG_STRING, &gl_source_name, SOURCE_DEVICE,
     "set CDROM device as source (linux only)", "DEVICE"},
#endif

    {"verbose", 'v', POPT_ARG_NONE, &gl_verbose_flag, 0, 
     "be verbose"},
    
    {"quiet", 'q', POPT_ARG_NONE, &gl_quiet_flag, 0, 
     "show only critical messages"},

    {"version", 'V', POPT_ARG_NONE, NULL, OP_VERSION,
     "display version and copyright information and exit"},
    POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
  };

  poptContext optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);

  /* end of local declarations */

  while ((opt = poptGetNextOpt (optCon)) != -1)
    switch (opt)
      {
      case OP_VERSION:
        fprintf (stdout, "GNU VCDImager " VERSION " [" HOST_ARCH "]\n\n"
                 "Copyright (c) 2001 Herbert Valerio Riedel <hvr@gnu.org>\n\n"
                 "GNU VCDRip may be distributed under the terms of the GNU General Public Licence;\n"
                 "For details, see the file `COPYING', which is included in the GNU VCDImager\n"
                 "distribution. There is no warranty, to the extent permitted by law.\n");
        exit (EXIT_SUCCESS);
        break;
      case SOURCE_FILE:
      case SOURCE_DEVICE:
        if (gl_source_type != SOURCE_NONE)
          {
            fprintf (stderr, "only one source allowed! - try --help\n");
            exit (EXIT_FAILURE);
          }

        gl_source_type = opt;
        break;

      default:
        fprintf (stderr, "error while parsing command line - try --help\n");
        exit (EXIT_FAILURE);
      }

  if (poptGetArgs (optCon) != NULL)
    {
      fprintf (stderr, "error - no arguments expected! - try --help\n");
      exit (EXIT_FAILURE);
    }


  gl_default_vcd_log_handler = vcd_log_set_handler (_vcd_log_handler);

  {
    VcdImageSource *img_src;

    switch (gl_source_type)
      {
      case SOURCE_DEVICE:
        img_src = vcd_image_source_new_linuxcd (gl_source_name);
        break;
      case SOURCE_FILE:
        {
          VcdDataSource *bin_source;

          bin_source = vcd_data_source_new_stdio (gl_source_name);
          vcd_assert (bin_source != NULL);

          img_src = vcd_image_source_new_bincue (bin_source, NULL, gl_sector_2336_flag);
        }
        break;
      default:
        fprintf (stderr, "no source given -- can't do anything...\n");
        exit (EXIT_FAILURE);
      }

    vcd_assert (img_src != NULL);

    dump (img_src, gl_source_name);

    vcd_image_source_destroy (img_src);
  }

  return EXIT_SUCCESS;
}

/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
