/*
    $Id$

    Copyright (C) 2001,2002 Herbert Valerio Riedel <hvr@gnu.org>
    Copyright (C) 2002,2003 Rocky Bernstein <rocky@panix.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Foundation
    Software, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stddef.h>

#include <popt.h>

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_bitvec.h>
#include <libvcd/vcd_types.h>
#include <libvcd/vcd_files.h>
#include <libvcd/vcd_files_private.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_iso9660_private.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_salloc.h>
#include <libvcd/vcd_image_fs.h>

/* Eventually move above libvcd includes but having vcdinfo including. */
#include <libvcd/vcdinfo.h>

static const char _rcsid[] = "$Id$";

static const char DELIM[] = \
"----------------------------------------" \
"---------------------------------------\n";

/* global static vars */
static struct gl_t
{
  vcdinfo_source_t source_type;

  /* Boolean values set by command-line options to reduce output.
     Note: because these are used by popt, the datatype here has to be 
     int, not bool. 
  */
  int verbose_flag;
  int quiet_flag;

  struct show_t
  {
    int all;     /* True makes all of the below "show" variables true. */

    struct no_t  /* Switches that are on by default and you turn off. */
    {
      int banner;     /* True supresses initial program banner and Id */
      int delimiter;  /* True supresses delimiters between sections   */
      int header;     /* True supresses the section headers           */
    } no;
    
    struct entries_t  /* Switches for the ENTRIES section. */
    {
      int any;   /* True if any of the below variables are set true. */
      int all;   /* True makes all of the below variables set true.  */
      int count; /* Show total number of entries.                    */
      int data;  /* Show all of the entry points .                   */
      int id;    /* Show INFO Id */
      int prof;  /* Show system profile tag. */
      int vers;  /* Show version */
    } entries;

    struct info_t     /* Switches for the INFO section. */
    {
      int any;
      int all;
      int album; /* Show album description info. */ 
      int cc;    /* Show data cc. */
      int count; /* Show volume count */
      int id;    /* Show ID */
      int ofm;   /* Show offset multiplier */
      int lid2;  /* Show start LID #2 */
      int lidn;  /* Show maximum LID */
      int pal;   /* Show PAL flags and reserved1 */
      int pbc;   /* Show reserved2 or extended pbc */
      int prof;  /* Show system profile flag */
      int psds;  /* Show PSD size. */
      int res;   /* Show restriction */
      int seg;   /* Show first segment address */
      int segn;  /* Show maximum segment number */
      int segs;  /* Show segments */
      int spec;  /* Show special info */
      int start; /* Show volume start times */
      int st2;   /* Show start track #2 */
      int vers;  /* Show INFO version. */
      int vol;   /* Show volume number */
    } info;
    
    struct pvd_t  /* Switches for the PVD section. */
    {
      int any;    /* True if any of the below variables are set true. */
      int all;    /* True makes all of the below variables set true.  */
      int app;    /* show application ID */
      int id;     /* show PVD ID */
      int iso;    /* Show ISO size */
      int prep;   /* Show preparer ID */
      int pub;    /* Show publisher ID */
      int sys;    /* Show system id */
      int vers;   /* Show version number */
      int vol;    /* Show volume ID */
      int volset; /* Show volumeset ID */
      int xa;     /* Show if XA marker is present. */
    } pvd;

    int format;   /* Show VCD format VCD 1.1, VCD 2.0, SVCD, ... */
    int fs;
    int lot;
    int psd;      /* Show PSD group -- needs to be broken out. */
    int scandata; /* Show scan data group -- needs to broken out. */
    int search;
    int source;   /* Show image source and size. */
    int tracks;   
  } show;

}
gl;                             /* global variables */

/* end of vars */


#define PRINTED_POINTS 15


static bool
_bitset_get_bit (const uint8_t bitvec[], int bit)
{
  bool result = false;
  
  if (_vcd_bit_set_p (bitvec[bit / 8], (bit % 8)))
    result = true;

  return result;
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

static void
dump_lot (const vcdinfo_obj_t *obj, bool ext)
{
  const LotVcd *lot = ext ? obj->lot_x : obj->lot;
  
  unsigned n, tmp;
  const uint16_t max_lid = vcdinfo_get_num_LIDs(obj);
  unsigned mult = obj->info.offset_mult;

  if (!gl.show.no.header)
    fprintf (stdout, 
             (obj->vcd_type == VCD_TYPE_SVCD 
              || obj->vcd_type == VCD_TYPE_HQVCD)
             ? "SVCD/LOT.SVD\n"
             : (ext ? "EXT/LOT_X.VCD\n": "VCD/LOT.VCD\n"));

  if (lot->reserved)
    fprintf (stdout, " RESERVED = 0x%4.4x (should be 0x0000)\n", uint16_from_be (lot->reserved));

  for (n = 0; n < LOT_VCD_OFFSETS; n++)
    {
      if ((tmp = uint16_from_be (lot->offset[n])) != PSD_OFS_DISABLED)
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
dump_psd (const vcdinfo_obj_t *obj, bool ext)
{
  VcdListNode *node;
  unsigned n = 0;
  unsigned mult = obj->info.offset_mult;
  const uint8_t *psd = ext ? obj->psd_x : obj->psd;
  VcdList *offset_list = ext ? obj->offset_x_list : obj->offset_list;

  fprintf (stdout, 
           (obj->vcd_type == VCD_TYPE_SVCD 
            || obj->vcd_type == VCD_TYPE_HQVCD)
           ? "SVCD/PSD.SVD\n"
           : (ext ? "EXT/PSD_X.VCD\n": "VCD/PSD.VCD\n"));

  _VCD_LIST_FOREACH (node, offset_list)
    {
      vcdinfo_offset_t *ofs = _vcd_list_node_data (node);
      unsigned _rofs = ofs->offset * mult;

      uint8_t type;
      
      type = psd[_rofs];

      switch (type)
        {
        case PSD_TYPE_PLAY_LIST:
          {
            const PsdPlayListDescriptor *pld = (const void *) (psd + _rofs);
            
            int i;
            uint16_t lid = vcdinfo_get_lid_from_pld(pld);

            fprintf (stdout,
                     " PSD[%.2d] (%s): play list descriptor\n"
                     "  NOI: %d | LID#: %d (rejected: %s)\n"
                     "  prev: %s | next: %s | return: %s\n"
                     "  playtime: %d/15s | wait: %ds | autowait: %ds\n",
                     n, vcdinfo_ofs2str (obj, ofs->offset, ext),
                     pld->noi, lid, 
                     _vcd_bool_str(vcdinfo_is_rejected(uint16_from_be(pld->lid))),
                     vcdinfo_ofs2str(obj, vcdinfo_get_prev_from_pld(pld), ext),
                     vcdinfo_ofs2str(obj, vcdinfo_get_next_from_pld(pld), ext),
                     vcdinfo_ofs2str(obj, vcdinfo_get_return_from_pld(pld),
                                     ext),
                     vcdinfo_get_play_time(pld), vcdinfo_get_wait_time (pld),
                     vcdinfo_get_autowait_time(pld));

            for (i = 0; i < pld->noi; i++)
              {
                fprintf (stdout, "  play-item[%d]: %s\n",
                         i, vcdinfo_pin2str (uint16_from_be (pld->itemid[i])));
              }
            fprintf (stdout, "\n");
          }
          break;

        case PSD_TYPE_END_LIST:
          {
            const PsdEndListDescriptor *d = (const void *) (psd + _rofs);
            fprintf (stdout, " PSD[%.2d] (%s): end list descriptor\n", n, vcdinfo_ofs2str (obj, ofs->offset, ext));
            if (obj->vcd_type != VCD_TYPE_VCD2)
              {
                fprintf (stdout, "  next disc number: %d (if 0 stop PBC handling)\n", d->next_disc);
                fprintf (stdout, "  change picture item: %s\n", vcdinfo_pin2str (uint16_from_be (d->change_pic)));
              }
            fprintf (stdout, "\n");
          }
          break;

        case PSD_TYPE_EXT_SELECTION_LIST:
        case PSD_TYPE_SELECTION_LIST:
          {
            const PsdSelectionListDescriptor *d =
              (const void *) (psd + _rofs);
            int i;

            fprintf (stdout,
                     "  PSD[%.2d] (%s): %sselection list descriptor\n"
                     "  Flags: 0x%.2x | NOS: %d | BSN: %d | LID: %d (rejected: %s)\n"
                     "  prev: %s | next: %s | return: %s\n"
                     "  default: %s | timeout: %s\n"
                     "  wait: %d secs | loop: %d (delayed: %s)\n"
                     "  play-item: %s\n",
                     n, vcdinfo_ofs2str (obj, ofs->offset, ext),
                     (type == PSD_TYPE_EXT_SELECTION_LIST ? "extended " : ""),
                     *(uint8_t *) &d->flags,
                     d->nos, 
                     vcdinfo_get_bsn(d),
                     vcdinfo_get_lid_from_psd(d),
                     _vcd_bool_str (vcdinfo_get_lid_rejected_from_psd(d)),
                     vcdinfo_ofs2str (obj, vcdinfo_get_prev_from_psd(d), ext),
                     vcdinfo_ofs2str (obj, vcdinfo_get_next_from_psd(d), ext),
                     vcdinfo_ofs2str (obj, vcdinfo_get_return_from_psd(d),ext),
                     vcdinfo_ofs2str (obj, uint16_from_be (d->default_ofs), ext),
                     vcdinfo_ofs2str (obj, vcdinfo_get_timeout_LID(d), ext),
                     vcdinfo_get_timeout_time(d),
                     vcdinfo_get_loop_count(d), _vcd_bool_str (0x80 & d->loop),
                     vcdinfo_pin2str (vcdinfo_get_itemid_from_psd(d)));

            for (i = 0; i < d->nos; i++)
              fprintf (stdout, "  ofs[%d]: %s\n", i,
                       vcdinfo_ofs2str (obj, 
                                        vcdinfo_get_offset_from_psd(d, i), 
                                        ext));

            if (type == PSD_TYPE_EXT_SELECTION_LIST 
                || d->flags.SelectionAreaFlag)
              {
                const PsdSelectionListDescriptorExtended *d2 =
                  (const void *) &(d->ofs[d->nos]);

                fprintf (stdout, "  prev_area: %s | next_area: %s\n",
                         vcdinfo_area_str (&d2->prev_area),
                         vcdinfo_area_str (&d2->next_area));


                fprintf (stdout, "  retn_area: %s | default_area: %s\n",
                         vcdinfo_area_str (&d2->return_area),
                         vcdinfo_area_str (&d2->default_area));

                for (i = 0; i < d->nos; i++)
                  fprintf (stdout, "  area[%d]: %s\n", i,
                           vcdinfo_area_str (&d2->area[i]));
              }

            fprintf (stdout, "\n");
          }
          break;
        default:
          fprintf (stdout, " PSD[%2d] (%s): unkown descriptor type (0x%2.2x)\n", 
                   n, vcdinfo_ofs2str (obj, ofs->offset, ext), type);

          fprintf (stdout, "  hexdump: ");
          _hexdump (&psd[_rofs], 24);
          fprintf (stdout, "\n");
          break;
        }

      n++;
    }
}

static void
dump_info (const vcdinfo_obj_t *obj)
{
  const InfoVcd *info = &obj->info;
  unsigned int num_segments = vcdinfo_get_num_segments(obj);
  int n;

  if (!gl.show.no.header)
    fprintf (stdout, 
             (obj->vcd_type == VCD_TYPE_SVCD 
              || obj->vcd_type == VCD_TYPE_HQVCD)
             ? "SVCD/INFO.SVD\n" 
             : "VCD/INFO.VCD\n");

  if (gl.show.info.id) 
    fprintf (stdout, " ID: `%.8s'\n", info->ID);
  if (gl.show.info.vers)
    fprintf (stdout, " version: 0x%2.2x\n", info->version);
  if (gl.show.info.prof)
    fprintf (stdout, " system profile tag: 0x%2.2x\n", info->sys_prof_tag);
  if (gl.show.info.album)
    fprintf (stdout, " album id: `%.16s'\n", vcdinfo_get_album_id(obj));
  if (gl.show.info.count)
    fprintf (stdout, " volume count: %d\n", vcdinfo_get_volume_count(obj));
  if (gl.show.info.vol)
    fprintf (stdout, " volume number: %d\n", vcdinfo_get_volume_num(obj));

  if (gl.show.info.pal)
    {
      fprintf (stdout, " pal flags:");
      for (n = 0; n < 98; n++)
        {
          if (n == 48)
            fprintf (stdout, "\n  (bslbf)  ");
          
          fprintf (stdout, n % 8 ? "%d" : " %d",
                   _bitset_get_bit (info->pal_flags, n));
        }
      fprintf (stdout, "\n");
      
      fprintf (stdout, " flags:\n");
      fprintf (stdout, 
               ((obj->vcd_type == VCD_TYPE_SVCD || obj->vcd_type == VCD_TYPE_HQVCD) 
                ? "  reserved1: %s\n"
                : "  karaoke area: %s\n"),
               _vcd_bool_str (info->flags.reserved1));
    }

  if (gl.show.info.res)
    fprintf (stdout, "  restriction: %d\n", info->flags.restriction);
  if (gl.show.info.spec)
    fprintf (stdout, "  special info: %s\n", _vcd_bool_str (info->flags.special_info));
  if (gl.show.info.cc)
    fprintf (stdout, "  user data cc: %s\n", _vcd_bool_str (info->flags.user_data_cc));
  if (gl.show.info.lid2)
    fprintf (stdout, "  start lid #2: %s\n", _vcd_bool_str (info->flags.use_lid2));
  if (gl.show.info.st2)
    fprintf (stdout, "  start track #2: %s\n", _vcd_bool_str (info->flags.use_track3));
  if (gl.show.info.pbc)
    fprintf (stdout, 
             ((obj->vcd_type == VCD_TYPE_SVCD || obj->vcd_type == VCD_TYPE_HQVCD) 
              ? "  reserved2: %s\n"
              : "  extended pbc: %s\n"),
             _vcd_bool_str (info->flags.pbc_x));

  if (gl.show.info.psds)
    fprintf (stdout, " psd size: %d\n", uint32_from_be (info->psd_size));

  if (gl.show.info.seg)
    fprintf (stdout, " first segment addr: %2.2x:%2.2x:%2.2x\n",
             info->first_seg_addr.m, info->first_seg_addr.s, info->first_seg_addr.f);
  
  if (gl.show.info.ofm)
    fprintf (stdout, " offset multiplier: 0x%2.2x\n", info->offset_mult);

  if (gl.show.info.lidn)
    fprintf (stdout, " maximum lid: %d\n",
             uint16_from_be (info->lot_entries));

  if (gl.show.info.segn)
    fprintf (stdout, " maximum segment number: %d\n", num_segments);
  
  if (gl.show.info.segs)
    for (n = 0; n < num_segments; n++)
      {
        fprintf (stdout, " SEGMENT[%d]: audio: %s,"
                 " video: %s, continuation %s%s %s\n",
                 n + 1,
                 vcdinfo_audio_type2str(obj,
                                        vcdinfo_get_seg_audio_type(obj, n)),
                 vcdinfo_video_type2str(obj, n),
                 _vcd_bool_str (info->spi_contents[n].item_cont),
                 (obj->vcd_type == VCD_TYPE_VCD2) ? "" : ", OGT substream:",
                 (obj->vcd_type == VCD_TYPE_VCD2) ? "" : vcdinfo_ogt2str(obj, n));
      }
  
  if (gl.show.info.start)
    if (obj->vcd_type == VCD_TYPE_SVCD
        || obj->vcd_type == VCD_TYPE_HQVCD)
      for (n = 0; n < 5; n++)
        fprintf (stdout, " volume start time[%d]: %d secs\n", 
                 n, uint16_from_be (info->playing_time[n]));
}

static void
dump_entries (const vcdinfo_obj_t *obj)
{
  const EntriesVcd *entries = &obj->entries;
  int num_entries, n;

  num_entries = vcdinfo_get_num_entries(obj);

  if (!gl.show.no.header) 
    fprintf (stdout, 
             (obj->vcd_type == VCD_TYPE_SVCD 
              || obj->vcd_type == VCD_TYPE_HQVCD)
             ? "SVCD/ENTRIES.SVD\n"
             : "VCD/ENTRIES.VCD\n");

  if (!strncmp (entries->ID, ENTRIES_ID_VCD, sizeof (entries->ID)))
    { /* noop */ }
  else if (!strncmp (entries->ID, "ENTRYSVD", sizeof (entries->ID)))
    vcd_warn ("found obsolete (S)VCD3.0 ENTRIES.SVD signature");
  else
    vcd_warn ("unexpected ID signature encountered");

  if (gl.show.entries.id) 
    fprintf (stdout, " ID: `%.8s'\n", entries->ID);
  if (gl.show.entries.vers)
    fprintf (stdout, " version: 0x%2.2x\n", entries->version);
  if (gl.show.entries.prof)
    fprintf (stdout, " system profile tag: 0x%2.2x\n", entries->sys_prof_tag);

  if (gl.show.entries.count)
    fprintf (stdout, " entries: %d\n", num_entries);

  if (gl.show.entries.data) 
    for (n = 0; n < num_entries; n++)
      {
        const msf_t *msf = vcdinfo_get_entry_msf(obj, n);
        const uint32_t lsn = vcdinfo_lba2lsn(msf_to_lba(msf));
        
        fprintf (stdout, " ENTRY[%2.2d]: track# %d (SEQUENCE[%d]), LSN %d "
                 "(MSF %2.2x:%2.2x:%2.2x)\n",
                 n, vcdinfo_get_track(obj, n),
                 vcdinfo_get_track(obj, n) - 1,               
                 lsn,
                 msf->m, msf->s, msf->f);
      }
}

/* 
   Dump the track contents using information from TRACKS.SVCD.
   See also dump_tracks which gives similar information but doesn't 
   need TRACKS.SVCD
*/
static void
dump_tracks_svd (const vcdinfo_obj_t *obj)
{
  const TracksSVD *tracks = obj->tracks_buf;
  const TracksSVD2 *tracks2 = (const void *) &(tracks->playing_time[tracks->tracks]);
  const TracksSVD_v30 *tracks_v30 = obj->tracks_buf;

  unsigned j;

  if (!gl.show.no.header)
    fprintf (stdout, "SVCD/TRACKS.SVD\n");

  fprintf (stdout, " ID: `%.8s'\n", tracks->file_id);
  fprintf (stdout, " version: 0x%2.2x\n", tracks->version);
  
  fprintf (stdout, " tracks: %d\n", tracks->tracks);
  
  for (j = 0; j < tracks->tracks; j++)
    {
      const char *video_types[] =
        {
          "no stream",
          "reserved (0x1)",
          "reserved (0x2)",
          "NTSC stream",
          "reserved (0x4)",
          "reserved (0x5)",
          "reserved (0x6)",
          "PAL stream",
        };

      const char *ogt_str[] =
        {
          "None",
          "0 available",
          "0 & 1 available",
          "all available"
        };

      fprintf (stdout, " track[%.2d]: %2.2x:%2.2x:%2.2x,"
               " audio: %s, video: %s, OGT stream: %s\n",
               j,
               tracks->playing_time[j].m,
               tracks->playing_time[j].s,
               tracks->playing_time[j].f,
               vcdinfo_audio_type2str(obj, 
                                      vcdinfo_get_track_audio_type(obj, j+1)),
               video_types[tracks2->contents[j].video],
               ogt_str[tracks2->contents[j].ogt]);
    }


  fprintf (stdout, "\nCVD interpretation (probably)\n");
  for (j = 0;j < tracks->tracks; j++)
    {
      fprintf (stdout, "(track[%.2d]: %2.2x:%2.2x:%2.2x (cumulated),"
               " audio: %.2x, ogt: %.2x)\n",
               j,
               tracks_v30->track[j].cum_playing_time.m,
               tracks_v30->track[j].cum_playing_time.s,
               tracks_v30->track[j].cum_playing_time.f,
               tracks_v30->track[j].audio_info,
               tracks_v30->track[j].ogt_info);
    }
}

/* 
   Dump the track contents based on low-level CD datas.
   See also dump_tracks which gives more information but requires
   TRACKS.SVCD to exist on the medium.
*/
static void
dump_tracks (const vcdinfo_obj_t *obj)
{
  unsigned int j;
  unsigned int num_tracks = vcdinfo_get_num_tracks(obj);
  uint8_t min, sec, frame;

  if (!gl.show.no.header)
    fprintf (stdout, "CDROM TRACKS\n");

  fprintf (stdout, " tracks: %2d\n", num_tracks);
  
  for (j = 1; j <= num_tracks; j++)
    {
      vcdinfo_get_track_msf(obj, j, &min, &sec, &frame);
      fprintf (stdout, " track # %2d: %2.2d:%2.2d:%2.2d, size: %10d\n",
               j, min, sec, frame, vcdinfo_get_track_size(obj, j));
    }
  if (num_tracks != 0) {
      vcdinfo_get_track_msf(obj, num_tracks+1, &min, &sec, &frame);
      fprintf (stdout, " leadout   : %2.2d:%2.2d:%2.2d\n",
               min, sec, frame);
  }
}

static void
dump_scandata_dat (const vcdinfo_obj_t *obj)
{
  const ScandataDat1 *_sd1 = obj->scandata_buf;
  const uint16_t scandata_count = uint16_from_be (_sd1->scandata_count);
  const uint16_t track_count = uint16_from_be (_sd1->track_count);
  const uint16_t spi_count = uint16_from_be (_sd1->spi_count);

  fprintf (stdout, "EXT/SCANDATA.DAT\n");
  fprintf (stdout, " ID: `%.8s'\n", _sd1->file_id);

  fprintf (stdout, " version: 0x%2.2x\n", _sd1->version);
  fprintf (stdout, " reserved: 0x%2.2x\n", _sd1->reserved);
  fprintf (stdout, " scandata_count: %d\n", scandata_count);

  if (_sd1->version == SCANDATA_VERSION_VCD2)
    {
      const ScandataDat_v2 *_sd_v2 = obj->scandata_buf;

      int n;

      for (n = 0; n < scandata_count; n++)
        {
          const msf_t *msf = &_sd_v2->points[n];
          const uint32_t lsn = vcdinfo_lba2lsn(msf_to_lba(msf));

          if (!gl.verbose_flag 
              && n > PRINTED_POINTS
              && n < scandata_count - PRINTED_POINTS)
            continue;

          fprintf (stdout, "  scanpoint[%.4d]: LSN %u (msf %2.2x:%2.2x:%2.2x)\n",
                   n, lsn, msf->m, msf->s, msf->f);

          if (!gl.verbose_flag
              && n == PRINTED_POINTS
              && scandata_count > (PRINTED_POINTS * 2))
            fprintf (stdout, " [..skipping...]\n");
        }
    }
  else if (_sd1->version == SCANDATA_VERSION_SVCD)
    {
      const ScandataDat2 *_sd2 = 
        (const void *) &_sd1->cum_playtimes[track_count];

      const ScandataDat3 *_sd3 = 
        (const void *) &_sd2->spi_indexes[spi_count];

      const ScandataDat4 *_sd4 = 
        (const void *) &_sd3->mpeg_track_offsets[track_count];

      const int scandata_ofs0 = 
        offsetof (ScandataDat3, mpeg_track_offsets[track_count])
        - offsetof (ScandataDat3, mpeg_track_offsets);

      int n;

      fprintf (stdout, " sequence_count: %d\n", track_count);
      fprintf (stdout, " segment_count: %d\n", spi_count);

      for (n = 0; n < track_count; n++)
        {
          const msf_t *msf = &_sd1->cum_playtimes[n];

          fprintf (stdout, "  cumulative_playingtime[%d]: %2.2x:%2.2x:%2.2x\n",
                   n, msf->m, msf->s, msf->f);
        }
 
      for (n = 0; n < spi_count; n++)
        {
          const int _ofs = uint16_from_be (_sd2->spi_indexes[n]);

          fprintf (stdout, "  segment scandata ofs[n]: %d\n", _ofs);
        }
 
      fprintf (stdout, " sequence scandata ofs: %d\n",
               uint16_from_be (_sd3->mpegtrack_start_index));

      for (n = 0; n < track_count; n++)
        {
          const int _ofs = 
            uint16_from_be (_sd3->mpeg_track_offsets[n].table_offset);
          const int _toc = _sd3->mpeg_track_offsets[n].track_num;

          fprintf (stdout, "  track [%d]: TOC num: %d, sd offset: %d\n",
                   n, _toc, _ofs);
        }
  
      fprintf (stdout, " (scanpoint[0] offset = %d)\n", scandata_ofs0);

      for (n = 0; n < scandata_count; n++)
        {
          const msf_t *msf = &_sd4->scandata_table[n];
          const uint32_t lsn = vcdinfo_lba2lsn(msf_to_lba(msf));

          if (!gl.verbose_flag 
              && n > PRINTED_POINTS
              && n < scandata_count - PRINTED_POINTS)
            continue;

          fprintf (stdout, 
                   "  scanpoint[%.4d] (ofs:%5d): LSN %u (MSF %2.2x:%2.2x:%2.2x)\n",
                   n, scandata_ofs0 + (n * 3), lsn, msf->m, msf->s, msf->f);

          if (!gl.verbose_flag
              && n == PRINTED_POINTS
              && scandata_count > (PRINTED_POINTS * 2))
            fprintf (stdout, " [..skipping...]\n");
        }
    }
  else
    fprintf (stdout, "!unsupported version!\n");
}

static void
dump_search_dat (const vcdinfo_obj_t *obj)
{
  const SearchDat *searchdat = obj->search_buf;
  unsigned m;
  uint32_t scan_points = uint16_from_be (searchdat->scan_points);

  fprintf (stdout, "SVCD/SEARCH.DAT\n");
  fprintf (stdout, " ID: `%.8s'\n", searchdat->file_id);
  fprintf (stdout, " version: 0x%2.2x\n", searchdat->version);
  fprintf (stdout, " scanpoints: %d\n", scan_points);
  fprintf (stdout, " scaninterval: %d (in 0.5sec units -- must be `1')\n", 
           searchdat->time_interval);

  for (m = 0; m < scan_points;m++)
    {
      unsigned hh, mm, ss, ss2;
      const msf_t *msf = &(searchdat->points[m]);
      const uint32_t lsn = vcdinfo_lba2lsn(msf_to_lba(msf));

      if (!gl.verbose_flag 
          && m > PRINTED_POINTS 
          && m < (scan_points - PRINTED_POINTS))
        continue;
      
      ss2 = m * searchdat->time_interval;

      hh = ss2 / (2 * 60 * 60);
      mm = (ss2 / (2 * 60)) % 60;
      ss = (ss2 / 2) % 60;
      ss2 = (ss2 % 2) * 5;

      fprintf (stdout, " scanpoint[%.4d]: (real time: %.2d:%.2d:%.2d.%.1d) "
               " sector: LSN %u (MSF %.2x:%.2x:%.2x)\n", m, hh, mm, ss, ss2,
               lsn, msf->m, msf->s, msf->f);
      
      if (!gl.verbose_flag
          && m == PRINTED_POINTS && scan_points > (PRINTED_POINTS * 2))
        fprintf (stdout, " [..skipping...]\n");
    }
}


static void
_dump_fs_recurse (const vcdinfo_obj_t *obj, const char pathname[])
{
  VcdList *entlist = vcd_image_source_fs_readdir (obj->img, pathname);
  VcdList *dirlist =  _vcd_list_new ();
  VcdListNode *entnode;
    
  fprintf (stdout, " %s:\n", pathname);

  vcd_assert (entlist != NULL);

  /* just iterate */
  
  _VCD_LIST_FOREACH (entnode, entlist)
    {
      char *_name = _vcd_list_node_data (entnode);
      char _fullname[4096] = { 0, };
      vcd_image_stat_t statbuf;

      snprintf (_fullname, sizeof (_fullname), "%s%s", pathname, _name);
  
      if (vcd_image_source_fs_stat (obj->img, _fullname, &statbuf))
        vcd_assert_not_reached ();

      strncat (_fullname, "/", sizeof (_fullname));

      if (statbuf.type == _STAT_DIR
          && strcmp (_name, ".") 
          && strcmp (_name, ".."))
        _vcd_list_append (dirlist, strdup (_fullname));

      fprintf (stdout, 
               "  %c %s %d %d [fn %.2d] [LSN %6d] ",
               (statbuf.type == _STAT_DIR) ? 'd' : '-',
               vcdinfo_get_xa_attr_str (statbuf.xa.attributes),
               uint16_from_be (statbuf.xa.user_id),
               uint16_from_be (statbuf.xa.group_id),
               statbuf.xa.filenum,
               statbuf.lsn);

      if (uint16_from_be(statbuf.xa.attributes) & XA_ATTR_MODE2FORM2) {
        fprintf (stdout, "%9d (%9d)",
                 statbuf.secsize * VCDINFO_M2F2_SECTOR_SIZE,
                 statbuf.size);
      } else {
        fprintf (stdout, "%9d",
                 statbuf.size);
      }
      fprintf (stdout, "  %s\n", _name);

    }

  _vcd_list_free (entlist, true);

  fprintf (stdout, "\n");

  /* now recurse */

  _VCD_LIST_FOREACH (entnode, dirlist)
    {
      char *_fullname = _vcd_list_node_data (entnode);

      _dump_fs_recurse (obj, _fullname);
    }

  _vcd_list_free (dirlist, true);
}

static void
dump_fs (const vcdinfo_obj_t *obj)
{
  struct iso_primary_descriptor const *pvd = &obj->pvd;
  struct iso_directory_record *idr = (void *) pvd->root_directory_record;
  uint32_t extent;

  extent = from_733 (idr->extent);

  fprintf (stdout, "ISO9660 filesystem dump\n");
  fprintf (stdout, " root dir in PVD set to lsn %d\n\n", extent);

  _dump_fs_recurse (obj, "/");
}

static void
dump_pvd (const vcdinfo_obj_t *obj)
{
  struct iso_primary_descriptor const *pvd = &obj->pvd;

  if (!gl.show.no.header)
    fprintf (stdout, "ISO9660 primary volume descriptor\n");

  if (pvd->type != ISO_VD_PRIMARY)
    vcd_warn ("unexpected descriptor type");

  if (strncmp (pvd->id, ISO_STANDARD_ID, strlen (ISO_STANDARD_ID)))
    vcd_warn ("unexpected ID encountered (expected `" ISO_STANDARD_ID "'");
  
  if (gl.show.pvd.id)
    fprintf (stdout, " ID: `%.5s'\n", pvd->id);

  if (gl.show.pvd.vers)
    fprintf (stdout, " version: %d\n", pvd->version);

  if (gl.show.pvd.sys)
    fprintf (stdout, " system id: `%s'\n",    vcdinfo_get_system_id(obj));

  if (gl.show.pvd.vol)
    fprintf (stdout, " volume id: `%s'\n",    vcdinfo_get_volume_id(obj));

  if (gl.show.pvd.volset)
    fprintf (stdout, " volumeset id: `%s'\n", vcdinfo_get_volumeset_id(obj));

  if (gl.show.pvd.pub)
    fprintf (stdout, " publisher id: `%s'\n", vcdinfo_get_publisher_id(obj));

  if (gl.show.pvd.prep)
    fprintf (stdout, " preparer id: `%s'\n",  vcdinfo_get_preparer_id(obj));

  if (gl.show.pvd.app)
    fprintf (stdout, " application id: `%s'\n", 
             vcdinfo_get_application_id(obj));
  
  if (gl.show.pvd.iso)
    fprintf (stdout, " ISO size: %d blocks (logical blocksize: %d bytes)\n", 
             from_733 (pvd->volume_space_size),
             from_723 (pvd->logical_block_size));

  if (gl.show.pvd.xa) 
    fprintf (stdout, " XA marker present: %s\n", 
             _vcd_bool_str (vcdinfo_has_xa(obj)));
}

static void
dump_all (vcdinfo_obj_t *obj)
{
  if (gl.show.pvd.any) 
    {
      if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
      dump_pvd (obj);
    }
  
  if (gl.show.fs) 
    {
      if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
      dump_fs (obj);
    }

  if (gl.show.info.any) 
    {
      if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
      dump_info (obj);
    }
  
  if (gl.show.entries.any) 
    {
      if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
      dump_entries (obj);
    }

  if (gl.show.psd) 
    {
      if (vcdinfo_get_psd_size (obj))
        {
          vcdinfo_visit_lot (obj, false);
          if (gl.show.lot)
            {
              if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
              dump_lot (obj, false);
            }
          
          if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
          dump_psd (obj, false);
        }
      
      if (obj->psd_x_size)
        {
          vcdinfo_visit_lot (obj, true);
          if (gl.show.lot) 
            {
              if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
              dump_lot (obj, true);
            }
          if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
          dump_psd (obj, true);
        }
    }

  if (gl.show.tracks) 
    {
      if (obj->tracks_buf)
        {
          if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
          dump_tracks_svd (obj);
        } 
      if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
      dump_tracks (obj);
    }
  
  if (gl.show.search) 
    {
      if (obj->search_buf)
        {
          if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
          dump_search_dat (obj);
        }
    }
  

  if (gl.show.scandata) 
    {
      if (obj->scandata_buf)
        {
          if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
          dump_scandata_dat (obj);
        }
      
    }
  if (!gl.show.no.delimiter) fprintf (stdout, DELIM);

}


static void
dump (char image_fname[])
{
  unsigned size, psd_size;
  vcdinfo_obj_t obj;
  vcd_image_stat_t statbuf;
  vcdinfo_open_return_t open_rc;
  
  if (!gl.show.no.banner)
    {
      if (!gl.show.no.delimiter)
        fprintf (stdout, DELIM);

      fprintf (stdout, "vcddump - GNU VCDImager - (Super) Video CD Report\n"
               "%s\n\n", _rcsid);
    }

  if (VCDINFO_SOURCE_UNDEF==gl.source_type) 
    {
      gl.source_type = VCDINFO_SOURCE_DEVICE;
    }
  
  open_rc = vcdinfo_open(&obj, image_fname, gl.source_type);
  if (open_rc==VCDINFO_OPEN_ERROR)
    exit (EXIT_FAILURE);

  vcd_assert (obj.img != NULL);

  size = vcd_image_source_stat_size (obj.img);

  if (gl.show.source) 
    {
      if (NULL == image_fname) {
        image_fname = vcdinfo_get_default_device(&obj);
        fprintf (stdout, "Source: default image file `%s'\n", image_fname);
        free(image_fname);
      } else {
        fprintf (stdout, "Source: image file `%s'\n", image_fname);
      }
      fprintf (stdout, "Image size: %d sectors\n", size);
    }
    
  if (open_rc == VCDINFO_OPEN_OTHER) {
    vcd_warn ("Medium is not VCD image");
    if (gl.show.tracks) {
      if (!gl.show.no.delimiter) fprintf (stdout, DELIM);
      dump_tracks (&obj);
    }
    exit (EXIT_FAILURE);
  }

  if (vcdinfo_get_format_version (&obj) == VCD_TYPE_INVALID) {
    vcd_error ("VCD detection failed - aborting");
    exit (EXIT_FAILURE);
  } else if (gl.show.format) {
    fprintf (stdout, "%s detected\n", vcdinfo_get_format_version_str(&obj));
  }

  psd_size = vcdinfo_get_psd_size (&obj);

  if (vcdinfo_read_psd (&obj) && vcdinfo_read_psd (&obj))
    {
      /* ISO9660 crosscheck */
      if (vcd_image_source_fs_stat (obj.img, 
                                    ((obj.vcd_type == VCD_TYPE_SVCD 
                                      || obj.vcd_type == VCD_TYPE_HQVCD)
                                     ? "/SVCD/PSD.SVD;1" 
                                     : "/VCD/PSD.VCD;1"),
                                    &statbuf))
        vcd_warn ("no PSD file entry found in ISO9660 fs");
      else
        {
          if (psd_size != statbuf.size)
            vcd_warn ("ISO9660 psd size != INFO psd size");
          if (statbuf.lsn != PSD_VCD_SECTOR)
            vcd_warn ("psd fileentry in ISO9660 not at fixed lsn");
        }
          
    }

  if (obj.vcd_type == VCD_TYPE_SVCD
      || obj.vcd_type == VCD_TYPE_HQVCD)
    {
      if (!vcd_image_source_fs_stat (obj.img, "MPEGAV", &statbuf))
        vcd_warn ("non compliant /MPEGAV folder detected!");

      if (vcd_image_source_fs_stat (obj.img, "SVCD/TRACKS.SVD;1", &statbuf))
        vcd_warn ("mandatory /SVCD/TRACKS.SVD not found!");
      else
        vcd_debug ("found TRACKS.SVD signature at sector %d", statbuf.lsn);

      if (vcd_image_source_fs_stat (obj.img, "SVCD/SEARCH.DAT;1", &statbuf))
        vcd_warn ("mandatory /SVCD/SEARCH.DAT not found!");
      else
        {
          uint32_t size;
          vcd_debug ("found SEARCH.DAT at sector %d", statbuf.lsn);

          obj.search_buf = _vcd_malloc (ISO_BLOCKSIZE * statbuf.secsize);

          if (vcd_image_source_read_mode2_sectors (obj.img, obj.search_buf,
                                                   statbuf.lsn, false, 
                                                   statbuf.secsize))
            exit (EXIT_FAILURE);

          size = (3 * uint16_from_be (((SearchDat *)obj.search_buf)->scan_points)) 
            + sizeof (SearchDat);
          
          if (size > statbuf.size)
            {
              vcd_warn ("number of scanpoints leads to bigger size than "
                        "file size of SEARCH.DAT! -- rereading");

              free (obj.search_buf);
              obj.search_buf = _vcd_malloc (ISO_BLOCKSIZE * _vcd_len2blocks (size, ISO_BLOCKSIZE));
          
              if (vcd_image_source_read_mode2_sectors (obj.img, 
                                                       obj.search_buf,
                                                       statbuf.lsn, false, 
                                                       statbuf.secsize))
                exit (EXIT_FAILURE);
            }
        }
    }

  if (!vcd_image_source_fs_stat (obj.img, "EXT/SCANDATA.DAT;1", &statbuf))
    {
      vcd_debug ("found /EXT/SCANDATA.DAT at sector %d", statbuf.lsn);

      obj.scandata_buf = _vcd_malloc (ISO_BLOCKSIZE * statbuf.secsize);

      if (vcd_image_source_read_mode2_sectors (obj.img, obj.scandata_buf,
                                               statbuf.lsn, false,
                                               statbuf.secsize))
        exit (EXIT_FAILURE);
    }

  if (obj.vcd_type == VCD_TYPE_VCD2)
    {
      if (!vcd_image_source_fs_stat (obj.img, "EXT/PSD_X.VCD;1", &statbuf))
        {
          vcd_debug ("found /EXT/PSD_X.VCD at sector %d", statbuf.lsn);

          obj.psd_x = _vcd_malloc (ISO_BLOCKSIZE * statbuf.secsize);
          obj.psd_x_size = statbuf.size;

          if (vcd_image_source_read_mode2_sectors (obj.img, obj.psd_x,
                                                   statbuf.lsn, false, 
                                                   statbuf.secsize))
            exit (EXIT_FAILURE);
        }

      if (!vcd_image_source_fs_stat (obj.img, "EXT/LOT_X.VCD;1", &statbuf))
        {
          vcd_debug ("found /EXT/LOT_X.VCD at sector %d", statbuf.lsn);

          obj.lot_x = _vcd_malloc (ISO_BLOCKSIZE * statbuf.secsize);

          if (vcd_image_source_read_mode2_sectors (obj.img, obj.lot_x,
                                                   statbuf.lsn, false, 
                                                   statbuf.secsize))
            exit (EXIT_FAILURE);

          if (statbuf.size != LOT_VCD_SIZE * ISO_BLOCKSIZE)
            vcd_warn ("LOT_X.VCD size != 65535");
        }
    }

  dump_all (&obj);
  vcdinfo_close(&obj);
}

static vcd_log_handler_t gl_default_vcd_log_handler = NULL;

static void 
_vcd_log_handler (log_level_t level, const char message[])
{
  if (level == LOG_DEBUG && !gl.verbose_flag)
    return;

  if (level == LOG_INFO && gl.quiet_flag)
    return;
  
  gl_default_vcd_log_handler (level, message);
}

/* Configuration option codes */
enum {

  /* These correspond to vcdinfo_source_t in vcdinfo.h and have to MATCH! */
  OP_SOURCE_UNDEF       = VCDINFO_SOURCE_UNDEF,
  OP_SOURCE_AUTO        = VCDINFO_SOURCE_AUTO,
  OP_SOURCE_BIN         = VCDINFO_SOURCE_BIN,
  OP_SOURCE_CUE         = VCDINFO_SOURCE_CUE,
  OP_SOURCE_DEVICE      = VCDINFO_SOURCE_DEVICE,
  OP_SOURCE_SECTOR_2336 = VCDINFO_SOURCE_SECTOR_2336,

  /* These are the remaining configuration options */
  OP_VERSION,       OP_ENTRIES,   OP_INFO,      OP_PVD,       OP_SHOW, 

};

/* Initialize global variables. */
static void 
init() 
{
  gl.verbose_flag     = false;
  gl.quiet_flag       = false;
  gl.source_type      = VCDINFO_SOURCE_UNDEF;

  /* Set all of show-flag entries false in one go. */
  memset(&gl.show, false, sizeof(gl.show));
  /* Actually this was a little too much. :-) The default behaviour is
     to show everything. So the one below assignment (if it persists
     until after options processing) negates, all of the work of the
     memset above! */
  gl.show.all = true;
}

/* Structure used so we can binary sort and set a --show-xxx flag switch. */
typedef struct
{
  char name[30];
  int *flag;
} subopt_entry_t;

/* Comparison function called by bearch() to find sub-option record. */
static int
compare_subopts(const void *key1, const void *key2) 
{
  subopt_entry_t *a = (subopt_entry_t *) key1;
  subopt_entry_t *b = (subopt_entry_t *) key2;
  return (strncmp(a->name, b->name, 30));
}

/* Do processing of a --show-xxx sub option. 
   Basically we find the option in the array, set it's corresponding
   flag variable to true as well as the "show.all" false. 
*/
static void
process_suboption(const char *subopt, subopt_entry_t *sublist, const int num,
                  const char *subopt_name, int *any_flag) 
{
  subopt_entry_t *subopt_rec = 
    bsearch(subopt, sublist, num, sizeof(subopt_entry_t), 
            &compare_subopts);
  if (subopt_rec != NULL) {
    if (strcmp(subopt_name, "help") != 0) {
      gl.show.all         = false;
      *(subopt_rec->flag) = true;
      *any_flag           = true;
      return;
    }
  } else {
    unsigned int i;
    bool is_help=strcmp(subopt, "help")==0;
    if (is_help) {
      fprintf (stderr, "The list of sub options for \"%s\" are:\n", 
               subopt_name);
    } else {
      fprintf (stderr, "Invalid option following \"%s\": %s.\n", 
               subopt_name, subopt);
      fprintf (stderr, "Should be one of: ");
    }
    for (i=0; i<num-1; i++) {
      fprintf(stderr, "%s, ", sublist[i].name);
    }
    fprintf(stderr, "or %s.\n", sublist[num-1].name);
    exit (is_help ? EXIT_SUCCESS : EXIT_FAILURE);
  }
}


int
main (int argc, const char *argv[])
{
  int terse_flag       = false;
  int sector_2336_flag = 0;
  char *source_name    = NULL;

  int opt;
  char *opt_arg;

  /* Command-line options */
  struct poptOption optionsTable[] = {

    {"bin-file", 'b', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, &source_name, 
     OP_SOURCE_BIN, "set \"bin\" image file as source", "FILE"},

    {"cue-file", 'c', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, &source_name, 
     OP_SOURCE_CUE, "set \"cue\" image file as source", "FILE"},

    {"input", 'i', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, &source_name, 
     OP_SOURCE_AUTO,
     "set source and determine if \"bin\" image or device", "INPUT"},

    {"sector-2336", '\0', 
     POPT_ARG_NONE, &sector_2336_flag, 
     OP_SOURCE_SECTOR_2336,
     "use 2336 byte sector mode for image file"},

    {"cdrom-device", 'C', 
     POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, &source_name, 
     OP_SOURCE_DEVICE,
     "set CD-ROM device as source", "DEVICE"},

    {"verbose", 'v', POPT_ARG_NONE, &gl.verbose_flag, 0, 
     "be verbose"},

    {"terse", 't', POPT_ARG_NONE, &terse_flag, 0, 
     "same as --no-header --no-banner --no-delimiter"},

    {"no-banner", 'B', POPT_ARG_NONE, &gl.show.no.banner, 0,
     "do not show program banner header and RCS version string"},
    
    {"no-delimiter", 'D', POPT_ARG_NONE, &gl.show.no.delimiter, 0,
     "do not show delimiter lines around various sections of output"},
    
    {"no-header", 'H', POPT_ARG_NONE, &gl.show.no.header, 0,
     "do not show section header titles"},
    
    {"show-entries", '\0', POPT_ARG_STRING, &opt_arg, OP_ENTRIES, 
     "show specific entry of the ENTRIES section "},
    
    {"show-entries-all", 'E', POPT_ARG_NONE, &gl.show.entries.all, OP_SHOW, 
     "show ENTRIES section"},
    
    {"show-filesystem", 'F', POPT_ARG_NONE, &gl.show.fs, OP_SHOW, 
     "show filesystem info"},
    
    {"show-info", '\0', POPT_ARG_STRING, &opt_arg, OP_INFO, 
     "show specific entry of the INFO section "},
    
    {"show-info-all", 'I', POPT_ARG_NONE, &gl.show.info.all, OP_SHOW, 
     "show INFO section"},
    
    {"show-lot", 'L', POPT_ARG_NONE, &gl.show.lot, OP_SHOW, 
     "show LOT section"},
    
    {"show-psd", 'p', POPT_ARG_NONE, &gl.show.psd, OP_SHOW, 
     "show PSD section(s)"},
    
    {"show-pvd-all", 'P', POPT_ARG_NONE, &gl.show.pvd.all, OP_SHOW, 
     "show PVD section(s)"},
    
    {"show-pvd", '\0', POPT_ARG_STRING, &opt_arg, OP_PVD, 
     "show a specific entry of the Primary Volume Descriptor (PVD) section"},
    
    {"show-scandata", 's', POPT_ARG_NONE, &gl.show.scandata, OP_SHOW, 
     "show scan data"},

    {"show-search", 'X', POPT_ARG_NONE, &gl.show.search, OP_SHOW, 
     "show search data"},

    {"show-source", 'S', POPT_ARG_NONE, &gl.show.source, OP_SHOW, 
     "show source image filename and size"},

    {"show-tracks", 'T', POPT_ARG_NONE, &gl.show.tracks, OP_SHOW, 
     "show tracks"},

    {"show-format", 'f', POPT_ARG_NONE, &gl.show.format, OP_SHOW, 
     "show VCD format (VCD 1.1, VCD 2.0, SVCD, ...)"},
    
    {"quiet", 'q', POPT_ARG_NONE, &gl.quiet_flag, 0, 
     "show only critical messages"},

    {"version", 'V', POPT_ARG_NONE, NULL, OP_VERSION,
     "display version and copyright information and exit"},
    POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
  };

  /* Sub-options of for --show-entries. Note: entries must be sorted! */
  subopt_entry_t entries_sublist[] = {
    {"count", &gl.show.entries.count},
    {"data",  &gl.show.entries.data},
    {"id",    &gl.show.entries.id},
    {"prof",  &gl.show.entries.prof},
    {"vers",  &gl.show.entries.vers}
  };

  /* Sub-options of for --show-info.  Note: entries must be sorted! */
  subopt_entry_t info_sublist[] = {
    {"album", &gl.show.info.album},
    {"cc",    &gl.show.info.cc},
    {"count", &gl.show.info.count},
    {"id",    &gl.show.info.id},
    {"ofm",   &gl.show.info.ofm},
    {"lid2",  &gl.show.info.lid2},
    {"lidn",  &gl.show.info.lidn},
    {"pal",   &gl.show.info.pal},
    {"pbc",   &gl.show.info.pbc},
    {"prof",  &gl.show.info.prof},
    {"psds",  &gl.show.info.psds},
    {"res",   &gl.show.info.res},
    {"seg",   &gl.show.info.seg},
    {"segn",  &gl.show.info.segn},
    {"segs",  &gl.show.info.segs},
    {"spec",  &gl.show.info.spec},
    {"start", &gl.show.info.start},
    {"st2",   &gl.show.info.st2},
    {"vers",  &gl.show.info.vers},
    {"vol",   &gl.show.info.vol},
  };

  /* Sub-options of for --show-pvd.  Note: entries must be sorted! */
  subopt_entry_t pvd_sublist[] = {
    {"app",   &gl.show.pvd.app},
    {"id",    &gl.show.pvd.id},
    {"iso",   &gl.show.pvd.iso},
    {"prep",  &gl.show.pvd.prep},
    {"pub",   &gl.show.pvd.pub},
    {"sys",   &gl.show.pvd.sys},
    {"vers",  &gl.show.pvd.vers},
    {"vol",   &gl.show.pvd.vol},
    {"volset",&gl.show.pvd.volset},
    {"xa",    &gl.show.pvd.xa},
  };

  poptContext optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);

  init();

  /* end of local declarations */

  while ((opt = poptGetNextOpt (optCon)) != -1)
    switch (opt)
      {
      case OP_ENTRIES:
        {
          process_suboption(opt_arg, entries_sublist,     
                            sizeof(entries_sublist) / sizeof(subopt_entry_t),
                            "--show-entries", &gl.show.entries.any);
          break;
        }
      case OP_INFO:
        {
          process_suboption(opt_arg, info_sublist,     
                            sizeof(info_sublist) / sizeof(subopt_entry_t),
                            "--show-info", &gl.show.info.any);
          break;
        }
      case OP_PVD:
        {
          process_suboption(opt_arg, pvd_sublist,     
                            sizeof(pvd_sublist) / sizeof(subopt_entry_t),
                            "--show-pvd", &gl.show.pvd.any);
          break;
        }
      case OP_SHOW:
        gl.show.all = false;
        break;
      case OP_VERSION:
        fprintf (stdout, vcd_version_string (true), "vcddump");
        fflush (stdout);
        exit (EXIT_SUCCESS);
        break;

      case OP_SOURCE_AUTO:
      case OP_SOURCE_BIN: 
      case OP_SOURCE_CUE: 
      case OP_SOURCE_DEVICE: 
      case OP_SOURCE_SECTOR_2336:
        {
          /* Check that we didn't speciy both DEVICE and SECTOR */
          bool okay = false;
          switch (gl.source_type) {
          case OP_SOURCE_UNDEF:
            /* Nothing was set before - okay. */
            okay = true;
            gl.source_type = opt;
            break;
          case OP_SOURCE_BIN:
            /* Going from 2352 (default) to 2336 is okay. */
            okay = OP_SOURCE_SECTOR_2336 == opt;
            if (okay) 
              gl.source_type = OP_SOURCE_SECTOR_2336;
            break;
          case OP_SOURCE_SECTOR_2336:
            /* Make sure a we didn't do a second device. FIX: 
               This also allows two -bin options if we had -2336 in the middle
             */
            okay = OP_SOURCE_DEVICE != opt;
            break;
          case OP_SOURCE_AUTO:
          case OP_SOURCE_CUE:
          case OP_SOURCE_DEVICE:
            /* This case is implied, but we'll make it explicit anyway. */
            okay = false;
            break;
          }

          if (!okay) 
          {
            fprintf (stderr, "only one source allowed! - try --help\n");
            exit (EXIT_FAILURE);
          }
          break;
        }

      default:
        fprintf (stderr, "%s: %s\n", 
                 poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
                 poptStrerror(opt));
        fprintf (stderr, "error while parsing command line - try --help\n");
        exit (EXIT_FAILURE);
      }

  if (poptGetArgs (optCon) != NULL)
    {
      fprintf (stderr, "error - no arguments expected! - try --help\n");
      exit (EXIT_FAILURE);
    }

  /* Handle massive show flag reversals below. */
  if (gl.show.all) {
    gl.show.entries.all  = gl.show.pvd.all  = gl.show.info.all 
      = gl.show.format   = gl.show.fs       = gl.show.lot    = gl.show.psd
      = gl.show.scandata = gl.show.scandata = gl.show.search = gl.show.source 
      = gl.show.tracks   = true;
  } 

  if (gl.show.entries.all) 
    memset(&gl.show.entries, true, sizeof(gl.show.entries));
  
  if (gl.show.pvd.all) 
    memset(&gl.show.pvd, true, sizeof(gl.show.pvd));
  
  if (gl.show.info.all) 
    memset(&gl.show.info, true, sizeof(gl.show.info));
  
  if (terse_flag) 
    memset(&gl.show.no, true, sizeof(gl.show.no));
  
  gl_default_vcd_log_handler = vcd_log_set_handler (_vcd_log_handler);

  dump (source_name);

  return EXIT_SUCCESS;
}

/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
