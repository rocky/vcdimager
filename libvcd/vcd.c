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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "vcd.h"
#include "vcd_obj.h"

#include "vcd_types.h"
#include "vcd_iso9660.h"
#include "vcd_salloc.h"
#include "vcd_files.h"
#include "vcd_files_private.h"
#include "vcd_cd_sector.h"
#include "vcd_directory.h"
#include "vcd_logging.h"
#include "vcd_util.h"

#include <libvcd/vcd_pbc.h>
#include <libvcd/vcd_dict.h>

static const char _rcsid[] = "$Id$";

static const char zero[CDDA_SIZE] = { 0, };

/*
 */

static mpeg_sequence_t *
_get_sequence_by_id (VcdObj *obj, const char id[])
{
  VcdListNode *node;

  assert (id != NULL);
  assert (obj != NULL);

  _VCD_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      mpeg_sequence_t *_sequence = _vcd_list_node_data (node);

      if (_sequence->id && !strcmp (id, _sequence->id))
        return _sequence;
    }

  return NULL;
}

/*
 */

VcdObj *
vcd_obj_new (vcd_type_t vcd_type)
{
  VcdObj *new_obj = NULL;
  static bool _first = true;
  
  if (_first)
    {
      vcd_debug ("initializing libvcd %s [%s]", VERSION, HOST_ARCH);
      _first = false;
    }

  switch (vcd_type) 
    {
    case VCD_TYPE_VCD11:
    case VCD_TYPE_VCD2:
      break;
      
    case VCD_TYPE_SVCD:
      break;
    default:
      vcd_error ("VCD type not supported");
      return new_obj; /* NULL */
      break;
    }

  new_obj = _vcd_malloc (sizeof (VcdObj));

  new_obj->iso_volume_label = strdup ("");
  new_obj->iso_application_id = strdup ("");
  new_obj->info_album_id = strdup ("");
  new_obj->info_volume_count = 1;
  new_obj->info_volume_number = 1;

  new_obj->custom_file_list = _vcd_list_new ();

  new_obj->type = vcd_type;

  new_obj->mpeg_sequence_list = _vcd_list_new ();

  new_obj->mpeg_segment_list = _vcd_list_new ();

  new_obj->pbc_list = _vcd_list_new ();

  switch (vcd_type) 
    {
    case VCD_TYPE_VCD11:
    case VCD_TYPE_VCD2:
      new_obj->pre_track_gap = 150;
      new_obj->pre_data_gap = 30;
      new_obj->post_data_gap = 45;
      break;
      
    case VCD_TYPE_SVCD:
      new_obj->pre_track_gap = 150;
      new_obj->pre_data_gap = 0;
      new_obj->post_data_gap = 0;
      break;

    default:
      assert (0);
    }

  return new_obj;
}

int 
vcd_obj_remove_item (VcdObj *obj, const char id[])
{
  vcd_warn ("vcd_obj_remove_item('%s') not implemented yet!", id);

  return -1;
}

static void
_vcd_obj_remove_mpeg_track (VcdObj *obj, int track_id)
{
  int length;
  mpeg_sequence_t *track = NULL;
  VcdListNode *node = NULL;

  assert (track_id >= 0);

  node = _vcd_list_at (obj->mpeg_sequence_list, track_id);
  
  assert (node != NULL);

  track = (mpeg_sequence_t *) _vcd_list_node_data (node);

  vcd_mpeg_source_destroy (track->source, true);

  length = track->info->packets;
  length += obj->pre_track_gap + obj->pre_data_gap + 0 + obj->post_data_gap;

  /* fixup offsets */
  {
    VcdListNode *node2 = node;
    while ((node2 = _vcd_list_node_next (node2)) != NULL)
      ((mpeg_sequence_t *) _vcd_list_node_data (node))->relative_start_extent -= length;
  }

  obj->relative_end_extent -= length;

  /* shift up */
  _vcd_list_node_free (node, true);
}

int
vcd_obj_append_segment_play_item (VcdObj *obj, VcdMpegSource *mpeg_source,
                                  const char item_id[])
{
  mpeg_segment_t *segment = NULL;

  assert (obj != NULL);
  assert (mpeg_source != NULL);

  if (obj->type != VCD_TYPE_SVCD
      && obj->type != VCD_TYPE_VCD2)
    {
      vcd_error ("segment play items only supported for VCD2.0 and SVCD");
      return -1;
    }

  if (item_id && _vcd_pbc_lookup (obj, item_id))
    {
      vcd_error ("item id (%s) exists already", item_id);
      return -1;
    }

  vcd_info ("scanning mpeg segment item #%d for scanpoints...", 
            _vcd_list_length (obj->mpeg_segment_list));
  vcd_mpeg_source_scan (mpeg_source);

  if (vcd_mpeg_source_get_info (mpeg_source)->packets == 0)
    {
      vcd_error ("mpeg is empty?");
      return -1;
    }

  /* create list node */

  segment = _vcd_malloc (sizeof (mpeg_sequence_t));

  segment->source = mpeg_source;

  if (item_id)
    segment->id = strdup (item_id);
 
  segment->info = vcd_mpeg_source_get_info (mpeg_source);
  segment->segment_count = _vcd_len2blocks (segment->info->packets, 150);

  vcd_debug ("SPI length is %d, rounded up to %d",
             segment->info->packets,
             segment->segment_count);

  _vcd_list_append (obj->mpeg_segment_list, segment);

  return 0;
}

int
vcd_obj_append_sequence_play_item (VcdObj *obj, VcdMpegSource *mpeg_source,
                                   const char item_id[])
{
  unsigned length;
  mpeg_sequence_t *track = NULL;
  int track_no = _vcd_list_length (obj->mpeg_sequence_list);

  assert (obj != NULL);
  assert (mpeg_source != NULL);

  if (item_id && _vcd_pbc_lookup (obj, item_id))
    {
      vcd_error ("item id (%s) exist already", item_id);
      return -1;
    }

  vcd_info ("scanning mpeg sequence item #%d for scanpoints...", track_no);
  vcd_mpeg_source_scan (mpeg_source);

  track = _vcd_malloc (sizeof (mpeg_sequence_t));

  track->source = mpeg_source;

  if (item_id)
    track->id = strdup (item_id);
  
  track->info = vcd_mpeg_source_get_info (mpeg_source);
  length = track->info->packets;

  track->entry_list = _vcd_list_new ();

  obj->relative_end_extent += obj->pre_track_gap;
  track->relative_start_extent = obj->relative_end_extent;

  obj->relative_end_extent += obj->pre_data_gap + length + obj->post_data_gap;

  if (length < 75)
    vcd_warn ("mpeg stream shorter than 75 sectors");

  if (obj->type == VCD_TYPE_VCD11
      && track->info->norm != MPEG_NORM_FILM
      && track->info->norm != MPEG_NORM_NTSC)
    vcd_warn ("VCD 1.x should contain only NTSC/FILM video");

  if (obj->type == VCD_TYPE_SVCD
      && track->info->version == MPEG_VERS_MPEG1)
    vcd_warn ("SVCD should not contain MPEG1 tracks!");
          
  if ((obj->type == VCD_TYPE_VCD2 || obj->type == VCD_TYPE_VCD11)
      && track->info->version == MPEG_VERS_MPEG2)
    vcd_warn ("VCD should not contain MPEG2 tracks!");
          
  vcd_debug ("track# %d's detected playing time: %.2f seconds", 
             track_no, track->info->playing_time);

  _vcd_list_append (obj->mpeg_sequence_list, track);

  return track_no;
}

int 
vcd_obj_add_sequence_entry (VcdObj *obj, const char sequence_id[], 
                            double entry_time, const char entry_id[])
{
  mpeg_sequence_t *_sequence;

  assert (obj != NULL);

  if (sequence_id)
    _sequence = _get_sequence_by_id (obj, sequence_id);
  else
    _sequence = _vcd_list_node_data (_vcd_list_end (obj->mpeg_sequence_list));

  if (!_sequence)
    {
      vcd_error ("sequence id `%s' not found", sequence_id);
      return -1;
    }

  if (entry_id && _vcd_pbc_lookup (obj, entry_id))
    {
      vcd_error ("item id (%s) exists already", entry_id);
      return -1;
    }

  {
    entry_t *_entry = _vcd_malloc (sizeof (entry_t));

    _entry->id = strdup (entry_id);
    _entry->time = entry_time;

    _vcd_list_append (_sequence->entry_list, _entry);
  }

  return 0;
}

void 
vcd_obj_destroy (VcdObj *obj)
{
  VcdListNode *node;

  assert (obj != NULL);
  assert (!obj->in_output);

  free (obj->iso_volume_label);
  free (obj->iso_application_id);

  _VCD_LIST_FOREACH (node, obj->custom_file_list)
    {
      custom_file_t *p = _vcd_list_node_data (node);
    
      free (p->iso_pathname);
    }

  _vcd_list_free (obj->custom_file_list, true);

  while (_vcd_list_length (obj->mpeg_sequence_list))
    _vcd_obj_remove_mpeg_track (obj, 0);
  _vcd_list_free (obj->mpeg_sequence_list, true);

  free (obj);
}

static void
cue_start (VcdDataSink *sink, const char fname[])
{
  char buf[1024] = { 0, };

  snprintf (buf, sizeof (buf), "FILE \"%s\" BINARY\r\n", fname);

  vcd_data_sink_write (sink, buf, 1, strlen (buf));
}

static void
cue_track (VcdDataSink *sink, int sect2336, uint8_t num, uint32_t extent)
{
  char buf[1024] = { 0, };

  uint8_t f = extent % 75;
  uint8_t s = (extent / 75) % 60;
  uint8_t m = (extent / 75) / 60;

  snprintf (buf, sizeof (buf),
            "  TRACK %2.2d MODE2/%d\r\n"
            "    INDEX %2.2d %2.2d:%2.2d:%2.2d\r\n",
            num, (sect2336 ? 2336 : 2352), 1, m, s, f);

  vcd_data_sink_write (sink, buf, 1, strlen (buf));
}

int
vcd_obj_write_cuefile (VcdObj *obj, VcdDataSink *cue_file,
                       const char bin_fname[])
{
  int n;
  VcdListNode *node;

  assert (obj != NULL);
  assert (obj->in_output);

  cue_start (cue_file, bin_fname);
  cue_track (cue_file, obj->bin_file_2336_flag, 1, 0); /* ISO9660 track */

  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      mpeg_sequence_t *track = _vcd_list_node_data (node);

      cue_track (cue_file, obj->bin_file_2336_flag, n+2, 
                 track->relative_start_extent + obj->iso_size);

      n++;
    }

  vcd_data_sink_destroy (cue_file);

  return 0;
}

void
vcd_obj_set_param (VcdObj *obj, vcd_parm_t param, const void *arg)
{
  assert (arg != NULL);

  switch (param) {
  case VCD_PARM_VOLUME_LABEL:
    free (obj->iso_volume_label);
    obj->iso_volume_label = strdup ((const char *) arg);
    if (strlen (obj->iso_volume_label) > 32)
      vcd_warn ("Volume label too long, will be truncated");
    break;

  case VCD_PARM_APPLICATION_ID:
    free (obj->iso_application_id);
    obj->iso_application_id = strdup ((const char *) arg);
    if (strlen (obj->iso_application_id) > 128)
      vcd_warn ("Application ID too long, will be truncated");
    break;
  
  case VCD_PARM_ALBUM_ID:
    free (obj->info_album_id);
    obj->info_album_id = strdup ((const char *) arg);
    if (strlen (obj->info_album_id) > 16)
      vcd_warn ("Album ID too long, will be truncated");
    break;

  case VCD_PARM_VOLUME_COUNT:
    obj->info_volume_count = (*(const unsigned *) arg);
    break;

  case VCD_PARM_VOLUME_NUMBER:
    obj->info_volume_number = (*(const unsigned *) arg);
    break;

  case VCD_PARM_SEC_TYPE:
    switch (*(const int *)arg) {
    case 2336:
      obj->bin_file_2336_flag = true;
      break;
    case 2352:
      obj->bin_file_2336_flag = false;
      break;
    default:
      assert (0);
    }
    break;
  case VCD_PARM_BROKEN_SVCD_MODE:
    if (obj->type == VCD_TYPE_SVCD)
      {
        if ((obj->broken_svcd_mode_flag = *(const bool*)arg))
          vcd_warn ("!! broken SVCD mode activated," 
                    " SVCD will not be compliant !!");
      }
    else
      vcd_error ("parameter not applicable for vcd type");
    break;
  default:
    assert (0);
    break;
  }
}

int
vcd_obj_add_dir (VcdObj *obj, const char iso_pathname[])
{
  vcd_warn ("vcd_obj_add_dir('%s') not implemented yet!", iso_pathname);

  return 0;
}

int
vcd_obj_add_file (VcdObj *obj, const char iso_pathname[],
                  VcdDataSource *file, bool raw_flag)
{
  uint32_t size = 0, sectors = 0;
 
  assert (obj != NULL);
  assert (file != NULL);
  assert (iso_pathname != NULL);
  assert (strlen (iso_pathname) > 0);
  assert (file != NULL);

  size = vcd_data_source_stat (file);

  /* close file to save file descriptors */
  vcd_data_source_close (file);

  if (raw_flag)
    {
      if (!size)
        {
          vcd_error("raw mode2 file must not be empty\n");
          return 1;
        }

      sectors = size / M2RAW_SIZE;

      if (size % M2RAW_SIZE)
        {
          vcd_error("raw mode2 file must have size multiple of %d \n", 
                    M2RAW_SIZE);
          return 1;
        }
    }
  else
    sectors = _vcd_len2blocks (size, M2F1_SIZE);

  {
    custom_file_t *p;
    char *_iso_pathname = _vcd_strdup_upper (iso_pathname);
    
    if (!_vcd_iso_pathname_valid_p (_iso_pathname))
      {
        vcd_error("pathname `%s' is not a valid iso pathname", 
                  _iso_pathname);
        return 1;
      }

    p = _vcd_malloc (sizeof (custom_file_t));
    
    p->file = file;
    p->iso_pathname = _iso_pathname;
    p->raw_flag = raw_flag;
  
    p->size = size;
    p->start_extent = 0;
    p->sectors = sectors;

    _vcd_list_append (obj->custom_file_list, p);
  }

  return 0;
}

static void
_finalize_vcd_iso_track_allocation (VcdObj *obj)
{
  int n;
  VcdListNode *node;

  uint32_t dir_secs = SECTOR_NIL;

  _dict_clean (obj);

  /* pre-alloc 16 blocks of ISO9660 required silence */
  if (_vcd_salloc (obj->iso_bitmap, 0, 16) == SECTOR_NIL)
    assert (0);

  /* keep karaoke sectors blank -- well... guess I'm too paranoid :) */
  if (_vcd_salloc (obj->iso_bitmap, 75, 75) == SECTOR_NIL) 
    assert (0);

  /* pre-alloc descriptors, PVD */
  _dict_insert (obj, "pvd", ISO_PVD_SECTOR, 1, SM_EOR);           /* EOR */
  /* EVD */
  _dict_insert (obj, "evd", ISO_EVD_SECTOR, 1, SM_EOR|SM_EOF);    /* EOR+EOF */

  /* reserve for iso directory */
  dir_secs = _vcd_salloc (obj->iso_bitmap, 18, 75-18);

  /* VCD information area */
  
  _dict_insert (obj, "info", INFO_VCD_SECTOR, 1, SM_EOF);              /* INFO.VCD */           /* EOF */
  _dict_insert (obj, "entries", ENTRIES_VCD_SECTOR, 1, SM_EOF);        /* ENTRIES.VCD */        /* EOF */

  /* PBC */

  if (_vcd_pbc_available (obj))
    {
      _dict_insert (obj, "lot", LOT_VCD_SECTOR, LOT_VCD_SIZE, SM_EOF); /* LOT.VCD */            /* EOF */
      _dict_insert (obj, "psd", PSD_VCD_SECTOR, 
                    _vcd_len2blocks (get_psd_size (obj, false), ISO_BLOCKSIZE), SM_EOF); /* PSD.VCD */ /* EOF */
    }

  if (obj->type == VCD_TYPE_SVCD)
    {
      _dict_insert (obj, "search", SECTOR_NIL, 
                    _vcd_len2blocks (get_search_dat_size (obj), ISO_BLOCKSIZE), SM_EOF); /* SEARCH.DAT */
      _dict_insert (obj, "tracks", SECTOR_NIL, 1, SM_EOF);      /* TRACKS.SVD */

      assert (_dict_get_bykey (obj, "tracks")->sector > INFO_VCD_SECTOR);
      assert (_dict_get_bykey (obj, "search")->sector > INFO_VCD_SECTOR);
    }

  /* done with primary information area */

  obj->mpeg_segment_start_extent = 
    _vcd_len2blocks (_vcd_salloc_get_highest (obj->iso_bitmap) + 1, 75) * 75;

  /* salloc up to end of vcd sector */
  for(n = 0;n < obj->mpeg_segment_start_extent;n++) 
    _vcd_salloc (obj->iso_bitmap, n, 1);
  
  assert (_vcd_salloc_get_highest (obj->iso_bitmap) + 1 == obj->mpeg_segment_start_extent);

  /* insert segments */

  _VCD_LIST_FOREACH (node, obj->mpeg_segment_list)
    {
      mpeg_segment_t *_segment = _vcd_list_node_data (node);
      
      _segment->start_extent = _vcd_salloc (obj->iso_bitmap, SECTOR_NIL, 
                                            _segment->segment_count * 150);

      assert (_segment->start_extent % 75 == 0);
      assert (_vcd_salloc_get_highest (obj->iso_bitmap) + 1 
              == _segment->start_extent + _segment->segment_count * 150);
    }

  obj->ext_file_start_extent = _vcd_salloc_get_highest (obj->iso_bitmap) + 1;

  assert (obj->ext_file_start_extent % 75 == 0);

  /* go on with EXT area */

  if (_vcd_pbc_available (obj))
    {
      _dict_insert (obj, "lot_x", SECTOR_NIL, LOT_VCD_SIZE, SM_EOF);
      
      _dict_insert (obj, "psd_x", SECTOR_NIL,
                    _vcd_len2blocks (get_psd_size (obj, true), ISO_BLOCKSIZE),
                    SM_EOF);
    }

  switch (obj->type) 
    {
    case VCD_TYPE_VCD11:
      /* noop */
      break;
    case VCD_TYPE_VCD2:
      /* scantable.dat */
      break;
    case VCD_TYPE_SVCD:
      _dict_insert (obj, "scandata", SECTOR_NIL, 
                    _vcd_len2blocks (get_scandata_dat_size (obj),
                                     ISO_BLOCKSIZE),
                    SM_EOF);
    default:
      assert (1);
      break;
    }

  obj->custom_file_start_extent =
    _vcd_salloc_get_highest (obj->iso_bitmap) + 1;

  /* now for the custom files */

  _VCD_LIST_FOREACH (node, obj->custom_file_list)
    {
      custom_file_t *p = _vcd_list_node_data (node);
      
      if (p->sectors)
        {
          p->start_extent =
            _vcd_salloc(obj->iso_bitmap, SECTOR_NIL, p->sectors);
          assert (p->start_extent != SECTOR_NIL);
        }
      else /* zero sized files -- set dummy extent */
        p->start_extent = obj->custom_file_start_extent;
    }

  /* calculate iso size -- after this point no sector shall be
     allocated anymore */

  obj->iso_size =
    MAX (MIN_ISO_SIZE, _vcd_salloc_get_highest (obj->iso_bitmap) + 1);

  vcd_debug ("iso9660: highest alloced sector is %d (using %d as isosize)", 
             _vcd_salloc_get_highest (obj->iso_bitmap), obj->iso_size);

  /* after this point the ISO9660's size is frozen */
}

static void
_finalize_vcd_iso_track_filesystem (VcdObj *obj)
{
  int n;
  VcdListNode *node;

  /* create filesystem entries */

  switch (obj->type) {
  case VCD_TYPE_VCD11:
  case VCD_TYPE_VCD2:
    /* _vcd_directory_mkdir (obj->dir, "CDDA"); */
    _vcd_directory_mkdir (obj->dir, "CDI");
    _vcd_directory_mkdir (obj->dir, "EXT");
    /* _vcd_directory_mkdir (obj->dir, "KARAOKE"); */
    _vcd_directory_mkdir (obj->dir, "MPEGAV");
    _vcd_directory_mkdir (obj->dir, "VCD");

    /* add segment dir only when there are actually segment play items */
    if (_vcd_list_length (obj->mpeg_segment_list))
      _vcd_directory_mkdir (obj->dir, "SEGMENT");

    _vcd_directory_mkfile (obj->dir, "VCD/ENTRIES.VCD", 
                           _dict_get_bykey (obj, "entries")->sector, 
                           ISO_BLOCKSIZE, false, 0);    
    _vcd_directory_mkfile (obj->dir, "VCD/INFO.VCD",
                           _dict_get_bykey (obj, "info")->sector, 
                           ISO_BLOCKSIZE, false, 0);

    /* only for vcd2.0 */
    if (_vcd_pbc_available (obj))
      {
        _vcd_directory_mkfile (obj->dir, "VCD/LOT.VCD", 
                               _dict_get_bykey (obj, "lot")->sector, 
                               ISO_BLOCKSIZE*LOT_VCD_SIZE, false, 0);
        _vcd_directory_mkfile (obj->dir, "VCD/PSD.VCD", 
                               _dict_get_bykey (obj, "psd")->sector, 
                               get_psd_size (obj, false), false, 0);
      }
    break;

  case VCD_TYPE_SVCD:
    _vcd_directory_mkdir (obj->dir, "EXT");
    _vcd_directory_mkdir (obj->dir, "MPEG2");

    if (obj->broken_svcd_mode_flag)
      {
        vcd_warn ("broken SVCD mode: adding MPEGAV dir");
        _vcd_directory_mkdir (obj->dir, "MPEGAV");
      }

    /* add segment dir only when there are actually segment play items */
    if (_vcd_list_length (obj->mpeg_segment_list))
      _vcd_directory_mkdir (obj->dir, "SEGMENT");

    _vcd_directory_mkdir (obj->dir, "SVCD");

    _vcd_directory_mkfile (obj->dir, "SVCD/ENTRIES.SVD",
                           _dict_get_bykey (obj, "entries")->sector, 
                           ISO_BLOCKSIZE, false, 0);    
    _vcd_directory_mkfile (obj->dir, "SVCD/INFO.SVD",
                           _dict_get_bykey (obj, "info")->sector, 
                           ISO_BLOCKSIZE, false, 0);

    if (_vcd_pbc_available (obj))
      {
        _vcd_directory_mkfile (obj->dir, "SVCD/LOT.SVD",
                               _dict_get_bykey (obj, "lot")->sector, 
                               ISO_BLOCKSIZE*LOT_VCD_SIZE, false, 0);
        _vcd_directory_mkfile (obj->dir, "SVCD/PSD.SVD", 
                               _dict_get_bykey (obj, "psd")->sector, 
                               get_psd_size (obj, false), false, 0);
      }

    _vcd_directory_mkfile (obj->dir, "SVCD/SEARCH.DAT", 
                           _dict_get_bykey (obj, "search")->sector, 
                           get_search_dat_size (obj), false, 0);
    _vcd_directory_mkfile (obj->dir, "SVCD/TRACKS.SVD",
                           _dict_get_bykey (obj, "tracks")->sector, 
                           ISO_BLOCKSIZE, false, 0);
    break;

  default:
    assert (0);
    break;
  }

  /* SEGMENTS */

  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_segment_list)
    {
      mpeg_segment_t *segment = _vcd_list_node_data (node);
      char segment_pathname[128] = { 0, };
      const char *fmt = NULL;
      
      switch (obj->type) 
        {
        case VCD_TYPE_VCD2:
          fmt = "SEGMENT/ITEM%4.4d.DAT";
          break;
        case VCD_TYPE_SVCD:
          fmt = "SEGMENT/ITEM%4.4d.MPG";
          break;
        default:
          assert(1);
        }

      assert (n < 500);
      
      snprintf (segment_pathname, sizeof (segment_pathname), fmt, n + 1);
        
      _vcd_directory_mkfile (obj->dir, segment_pathname, segment->start_extent,
                             segment->segment_count * ISO_BLOCKSIZE * 150,
                             true, 0);

      n++;
    }

  /* EXT files */

  switch (obj->type) 
    {
    case VCD_TYPE_VCD2:
      if (_vcd_pbc_available (obj))
        {
          /* psd_x -- extended PSD */
          _vcd_directory_mkfile (obj->dir, "EXT/PSD_X.VCD",
                                 _dict_get_bykey (obj, "psd_x")->sector, 
                                 get_psd_size (obj, true), false, 1);

          /* lot_x -- extended LOT */
          _vcd_directory_mkfile (obj->dir, "EXT/LOT_X.VCD",
                                 _dict_get_bykey (obj, "lot_x")->sector, 
                                 ISO_BLOCKSIZE*LOT_VCD_SIZE, false, 1);
        }
      break;

    case VCD_TYPE_SVCD:
      if (_vcd_pbc_available (obj))
        {
          /* psd_x -- extended PSD */
          _vcd_directory_mkfile (obj->dir, "EXT/PSD_X.SVD",
                                 _dict_get_bykey (obj, "psd_x")->sector, 
                                 get_psd_size (obj, true), false, 1);

          /* lot_x -- extended LOT */
          _vcd_directory_mkfile (obj->dir, "EXT/LOT_X.SVD",
                                 _dict_get_bykey (obj, "lot_x")->sector, 
                                 ISO_BLOCKSIZE*LOT_VCD_SIZE, false, 1);
        }

      /* scandata.dat -- scanpoints */
      _vcd_directory_mkfile (obj->dir, "EXT/SCANDATA.DAT",
                             _dict_get_bykey (obj, "scandata")->sector, 
                             get_scandata_dat_size (obj), false, 0);
      break;

    case VCD_TYPE_VCD11:
      break;

    default:
      assert (1);
    }


  /* custom files */

  _VCD_LIST_FOREACH (node, obj->custom_file_list)
    {
      custom_file_t *p = _vcd_list_node_data (node);

      _vcd_directory_mkfile(obj->dir, p->iso_pathname, p->start_extent,
                              p->size, p->raw_flag, 1);
    }


  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      char avseq_pathname[128] = { 0, };
      const char *fmt = NULL;
      mpeg_sequence_t *track = _vcd_list_node_data (node);
      uint32_t extent = track->relative_start_extent;
      uint8_t file_num = 0;
      
      extent += obj->iso_size;

      switch (obj->type) 
        {
        case VCD_TYPE_VCD11:
        case VCD_TYPE_VCD2:
          fmt = "MPEGAV/AVSEQ%2.2d.DAT";
          file_num = n + 1;
          break;
        case VCD_TYPE_SVCD:
          fmt = "MPEG2/AVSEQ%2.2d.MPG";
          file_num = 0;
          break;
        default:
          assert(1);
        }

      assert (n < 98);
      
      snprintf (avseq_pathname, sizeof (avseq_pathname), fmt, n + 1);
        
      _vcd_directory_mkfile (obj->dir, avseq_pathname, extent + obj->pre_data_gap,
                             track->info->packets * ISO_BLOCKSIZE,
                             true, file_num);

      if (obj->broken_svcd_mode_flag)
        {
          snprintf (avseq_pathname, sizeof (avseq_pathname), 
                    "MPEGAV/AVSEQ%2.2d.MPG", n + 1);

          vcd_warn ("broken svcd mode: adding additional `%s' entry", 
                    avseq_pathname);
            
          _vcd_directory_mkfile (obj->dir, avseq_pathname,
                                 extent+obj->pre_data_gap,
                                 track->info->packets * ISO_BLOCKSIZE,
                                 true, n + 1);
        }

      extent += obj->iso_size;
      n++;
    }

  /* register isofs dir structures */
  {
    uint32_t dirs_size = _vcd_directory_get_size (obj->dir);
    
    /* be sure to stay out of information areas */

    switch (obj->type) 
      {
      case VCD_TYPE_VCD11:
      case VCD_TYPE_VCD2:
        /* karaoke area starts at 03:00 */
        if (16 + 2 + dirs_size + 2 >= 75) 
          vcd_error ("directory section to big for a VCD");
        break;
      case VCD_TYPE_SVCD:
        /* since no karaoke exists the next fixed area starts at 04:00 */
        if (16 + 2 + dirs_size + 2 >= 150) 
          vcd_error ("directory section to big for a SVCD");
        break;
      default:
        assert(1);
      }
    
    /* un-alloc small area */
    
    _vcd_salloc_free (obj->iso_bitmap, 18, dirs_size + 2);
    
    /* alloc it again! */

    _dict_insert (obj, "dir", 18, dirs_size, SM_EOR|SM_EOF);
    _dict_insert (obj, "ptl", 18 + dirs_size, 1, SM_EOR|SM_EOF);
    _dict_insert (obj, "ptm", 18 + dirs_size + 1, 1, SM_EOR|SM_EOF);
  }
}

static void
_finalize_vcd_iso_track (VcdObj *obj)
{
  _finalize_vcd_iso_track_allocation (obj);
  _finalize_vcd_iso_track_filesystem (obj);
}

static int
_callback_wrapper (VcdObj *obj, int force)
{
  const int cb_frequency = 75;

  if (obj->last_cb_call + cb_frequency > obj->sectors_written && !force)
    return 0;

  obj->last_cb_call = obj->sectors_written;

  if (obj->progress_callback) {
    progress_info_t _pi;

    _pi.sectors_written = obj->sectors_written;
    _pi.total_sectors = obj->relative_end_extent + obj->iso_size; 
    _pi.in_track = obj->in_track;
    _pi.total_tracks = _vcd_list_length (obj->mpeg_sequence_list) + 1;

    return obj->progress_callback (&_pi, obj->callback_user_data);
  }
  else
    return 0;
}

static int
_write_m2_image_sector (VcdObj *obj, const void *data, uint32_t extent,
                        uint8_t fnum, uint8_t cnum, uint8_t sm, uint8_t ci) 
{
  char buf[CDDA_SIZE] = { 0, };

  assert (extent == obj->sectors_written);

  _vcd_make_mode2(buf, data, extent, fnum, cnum, sm, ci);

  vcd_data_sink_seek(obj->bin_file, 
                     extent * (obj->bin_file_2336_flag
                               ? M2RAW_SIZE
                               : CDDA_SIZE));

  if(obj->bin_file_2336_flag) 
    vcd_data_sink_write(obj->bin_file, buf + 12 + 4, M2RAW_SIZE, 1);
  else
    vcd_data_sink_write(obj->bin_file, buf, CDDA_SIZE, 1);
  
  obj->sectors_written++;

  return _callback_wrapper (obj, false);
}

static int
_write_m2_raw_image_sector (VcdObj *obj, const void *data, uint32_t extent)
{
  char buf[CDDA_SIZE] = { 0, };

  assert (extent == obj->sectors_written);

  _vcd_make_raw_mode2(buf, data, extent);

  vcd_data_sink_seek(obj->bin_file, 
                     extent * (obj->bin_file_2336_flag
                               ? M2RAW_SIZE : CDDA_SIZE));

  if(obj->bin_file_2336_flag) 
    vcd_data_sink_write(obj->bin_file, buf+12, M2RAW_SIZE, 1);
  else
    vcd_data_sink_write(obj->bin_file, buf, CDDA_SIZE, 1);

  obj->sectors_written++;

  return _callback_wrapper (obj, false);
}

static void
_write_source_mode2_raw (VcdObj *obj, VcdDataSource *source, uint32_t extent)
{
  int n;
  uint32_t sectors;

  sectors = vcd_data_source_stat (source) / M2RAW_SIZE;

  vcd_data_source_seek (source, 0); 

  for (n = 0;n < sectors;n++) {
    char buf[M2RAW_SIZE] = { 0, };

    vcd_data_source_read (source, buf, M2RAW_SIZE, 1);

    if (_write_m2_raw_image_sector (obj, buf, extent+n))
      break;
  }

  vcd_data_source_close (source);
}

static void
_write_source_mode2_form1 (VcdObj *obj, VcdDataSource *source, uint32_t extent)
{
  int n;
  uint32_t sectors, size, last_block_size;

  size = vcd_data_source_stat (source);

  sectors = _vcd_len2blocks (size, M2F1_SIZE);

  last_block_size = size % M2F1_SIZE;
  if (!last_block_size)
    last_block_size = M2F1_SIZE;

  vcd_data_source_seek (source, 0); 

  for (n = 0;n < sectors;n++) {
    char buf[M2F1_SIZE] = { 0, };

    vcd_data_source_read (source, buf, 
                          ((n + 1 == sectors) 
                           ? last_block_size
                           : M2F1_SIZE), 1);

    if (_write_m2_image_sector (obj, buf, extent+n, 1, 0, 
                                ((n+1 < sectors) 
                                 ? SM_DATA 
                                 : SM_DATA |SM_EOF),
                                0))
      break;
  }

  vcd_data_source_close (source);
}

static int
_write_vcd_iso_track (VcdObj *obj)
{
  VcdListNode *node;
  int n;

  /* generate dir sectors */
  
  _vcd_directory_dump_entries (obj->dir, 
                               _dict_get_bykey (obj, "dir")->buf, 
                               _dict_get_bykey (obj, "dir")->sector);

  _vcd_directory_dump_pathtables (obj->dir, 
                                  _dict_get_bykey (obj, "ptl")->buf, 
                                  _dict_get_bykey (obj, "ptm")->buf);
      
  /* generate PVD and EVD at last... */
  set_iso_pvd (_dict_get_bykey (obj, "pvd")->buf,
               obj->iso_volume_label, 
               obj->iso_application_id, 
               obj->iso_size, 
               _dict_get_bykey (obj, "dir")->buf, 
               _dict_get_bykey (obj, "ptl")->sector,
               _dict_get_bykey (obj, "ptm")->sector,
               pathtable_get_size (_dict_get_bykey (obj, "ptm")->buf));
    
  set_iso_evd (_dict_get_bykey (obj, "evd")->buf);

  /* fill VCD relevant files with data */

  set_info_vcd (obj, _dict_get_bykey (obj, "info")->buf);
  set_entries_vcd (obj, _dict_get_bykey (obj, "entries")->buf);

  if (_vcd_pbc_available (obj))
    {
      set_lot_vcd (obj, _dict_get_bykey (obj, "lot_x")->buf, true);
      set_psd_vcd (obj, _dict_get_bykey (obj, "psd_x")->buf, true);
  
      set_lot_vcd (obj, _dict_get_bykey (obj, "lot")->buf, false);
      set_psd_vcd (obj, _dict_get_bykey (obj, "psd")->buf, false);
    }

  if (obj->type == VCD_TYPE_SVCD) 
    {
      set_tracks_svd (obj, _dict_get_bykey (obj, "tracks")->buf);
      set_search_dat (obj, _dict_get_bykey (obj, "search")->buf);
      set_scandata_dat (obj, _dict_get_bykey (obj, "scandata")->buf);
    }

  /* start actually writing stuff */

  vcd_info ("writing track 1 (ISO9660)...");

  /* 00:02:00 -> 00:04:74 */
  for (n = 0;n < obj->mpeg_segment_start_extent; n++)
    {
      const void *content = NULL;
      uint8_t flags = SM_DATA;

      content = _dict_get_sector (obj, n);
      flags |= _dict_get_sector_flags (obj, n);
      
      if (content == NULL)
        content = zero;

      _write_m2_image_sector (obj, content, n, 0, 0, flags, 0);
    }

  /* SEGMENTS */

  assert (n == obj->mpeg_segment_start_extent);

  _VCD_LIST_FOREACH (node, obj->mpeg_segment_list)
    {
      mpeg_segment_t *_segment = _vcd_list_node_data (node);
      unsigned packet_no;

      assert (_segment->start_extent == n);

      for (packet_no = 0;packet_no < (_segment->segment_count * 150);packet_no++)
        {
          char buf[M2F2_SIZE] = { 0, };
          uint8_t fn, cn, sm, ci;

          if (packet_no < _segment->info->packets)
            {
              fn = 1;
              cn = 1;
              sm = SM_FORM2 | SM_REALT | SM_VIDEO;
              ci = 0x80; /* fixme -- different for VCD2.0 */

              if (packet_no + 1 == _segment->info->packets)
                {
                  sm |= SM_EOF;
                  vcd_debug ("eof at %d %d", n, packet_no);
                }

              vcd_mpeg_source_get_packet (_segment->source, packet_no, buf, NULL);
            }
          else
            {
              fn = 0;
              cn = 0;
              sm = SM_FORM2;
              ci = 0x00;
            }

          _write_m2_image_sector (obj, buf, n, fn, cn, sm, ci);
          
          n++;
        }

      vcd_mpeg_source_close (_segment->source);
    }

  /* EXT stuff */

  assert (n == obj->ext_file_start_extent);

  for (;n < obj->custom_file_start_extent; n++)
    {
      const void *content = NULL;
      uint8_t flags = SM_DATA;
      uint8_t fileno = (obj->type == VCD_TYPE_SVCD) ? 0 : 1;

      content = _dict_get_sector (obj, n);
      flags |= _dict_get_sector_flags (obj, n);
      
      if (content == NULL)
        {
          vcd_debug ("unexpected empty EXT sector");
          content = zero;
        }
      
      _write_m2_image_sector (obj, content, n, fileno, 0, flags, 0);
    }

  /* write custom files */

  assert (n == obj->custom_file_start_extent);
    
  _VCD_LIST_FOREACH (node, obj->custom_file_list)
    {
      custom_file_t *p = _vcd_list_node_data (node);
        
      vcd_info ("writing file `%s' (%d bytes%s)", 
                p->iso_pathname, p->size, 
                p->raw_flag ? ", raw sectors file": "");
      if (p->raw_flag)
        _write_source_mode2_raw (obj, p->file, p->start_extent);
      else
        _write_source_mode2_form1 (obj, p->file, p->start_extent);
    }
  
  /* blank unalloced tracks */
  while ((n = _vcd_salloc (obj->iso_bitmap, SECTOR_NIL, 1)) < obj->iso_size)
    _write_m2_image_sector (obj, zero, n, 0, 0, SM_DATA, 0);

  return 0;
}

static int
_write_sectors (VcdObj *obj, int track_idx)
{
  mpeg_sequence_t *track = 
    _vcd_list_node_data (_vcd_list_at (obj->mpeg_sequence_list, track_idx));
  int n, lastsect = obj->sectors_written;
  char buf[2324];
  struct {
    int audio;
    int video;
    int zero;
    int ogt;
    int unknown;
  } mpeg_packets = {0, };

  const char *audio_types[] = {
    "no audio stream",
    "1 audio stream",
    "2 audio streams",
    "ext MC audio stream",
    0
  };

  {
    char *norm_str = NULL;

    switch (track->info->norm) {
    case MPEG_NORM_PAL:
      norm_str = strdup ("PAL (352x288/25fps)");
      break;
    case MPEG_NORM_NTSC:
      norm_str = strdup ("NTSC (352x240/30fps)");
      break;
    case MPEG_NORM_FILM:
      norm_str = strdup ("FILM (352x240/24fps)");
      break;
    case MPEG_NORM_PAL_S:
      norm_str = strdup ("PAL S (480x576/25fps)");
      break;
    case MPEG_NORM_NTSC_S:
      norm_str = strdup ("NTSC S (480x480/30fps)");
      break;

	
    case MPEG_NORM_OTHER:
      {
        char buf[1024] = { 0, };
        snprintf (buf, sizeof (buf), "UNKNOWN (%dx%d/%2.2ffps)",
                  track->info->hsize, track->info->vsize, track->info->frate);
        norm_str = strdup (buf);
      }
      break;
    }

    vcd_info ("writing track %d, %s, %s, %s...", track_idx + 2,
              (track->info->version == MPEG_VERS_MPEG1 ? "MPEG1" : "MPEG2"),
              norm_str, audio_types[track->info->audio_type]);

    free (norm_str);
  }

  for (n = 0; n < obj->pre_track_gap;n++)
    _write_m2_image_sector (obj, zero, lastsect++, 0, 0, SM_FORM2, 0);

  for (n = 0; n < obj->pre_data_gap;n++)
    _write_m2_image_sector (obj, zero, lastsect++, track_idx + 1,
                            0, SM_FORM2|SM_REALT, 0);

  for (n = 0; n < track->info->packets; n++) {
    int ci = 0, sm = 0, cnum = 0, fnum = 0;
    struct vcd_mpeg_packet_flags pkt_flags;

    vcd_mpeg_source_get_packet (track->source, n, buf, &pkt_flags);

    switch (pkt_flags.type) {
    case PKT_TYPE_VIDEO:
      mpeg_packets.video++;
      sm = SM_FORM2|SM_REALT|SM_VIDEO;
      ci = CI_VIDEO;
      cnum = CN_VIDEO;
      break;
    
    case PKT_TYPE_AUDIO:
      mpeg_packets.audio++;
      sm = SM_FORM2|SM_REALT|SM_AUDIO;
      ci = CI_AUDIO;
      cnum = CN_AUDIO;
      if (pkt_flags.audio_c1 || pkt_flags.audio_c2)
        {
          ci = CI_AUDIO2;
          cnum = CN_AUDIO2;
        }
      break;

    case PKT_TYPE_OGT:
      mpeg_packets.ogt++;
      sm = SM_FORM2|SM_REALT|SM_VIDEO;
      ci = CI_OGT;
      cnum = CN_OGT;
      break;

    case PKT_TYPE_ZERO:
      mpeg_packets.zero++;
      mpeg_packets.unknown--;
    case PKT_TYPE_EMPTY:
      mpeg_packets.unknown++;
      sm = SM_FORM2|SM_REALT;
      ci = CI_OTHER;
      cnum = CN_OTHER;
      break;

    case PKT_TYPE_INVALID:
      vcd_error ("invalid mpeg packet found at packet# %d"
                 " -- please fix this mpeg file!", n);
      vcd_mpeg_source_close (track->source);
      return 1;
      break;

    default:
      assert (0);
    }

    if (n == track->info->packets - 1)
      sm |= SM_EOR;

    fnum = track_idx + 1;
      
    if (obj->type == VCD_TYPE_SVCD) /* svcds have a simplified subheader */
      {
        fnum = 1;
        ci = 0x80;
      }

    if (_write_m2_image_sector (obj, buf, lastsect++, fnum, cnum, sm, ci))
      break;
  }

  vcd_mpeg_source_close (track->source);

  for (n = 0; n < obj->post_data_gap; n++)
    _write_m2_image_sector (obj, zero, lastsect++, track_idx + 1,
                            0, SM_FORM2|SM_REALT, 0);

  vcd_debug ("MPEG packet statistics: %d video, %d audio, %d zero, %d ogt, %d unknown",
             mpeg_packets.video, mpeg_packets.audio, mpeg_packets.zero, mpeg_packets.ogt,
             mpeg_packets.unknown);

  return 0;
}

long
vcd_obj_get_image_size (VcdObj *obj)
{
  long size_sectors = -1;

  assert (!obj->in_output);
  
  if (_vcd_list_length (obj->mpeg_sequence_list) > 0) 
    {
      /* fixme -- make this efficient */
      size_sectors = vcd_obj_begin_output (obj);
      vcd_obj_end_output (obj);
    }

  return size_sectors;
}

long
vcd_obj_begin_output (VcdObj *obj)
{
  uint32_t image_size;

  assert (obj != NULL);
  assert (_vcd_list_length (obj->mpeg_sequence_list) > 0);

  assert (!obj->in_output);
  obj->in_output = true;

  obj->in_track = 1;
  obj->sectors_written = 0;

  obj->iso_bitmap = _vcd_salloc_new ();

  obj->dir = _vcd_directory_new ();

  obj->buffer_dict_list = _vcd_list_new ();

  _finalize_vcd_iso_track (obj);

  image_size = obj->relative_end_extent + obj->iso_size;

  if (image_size > CD_MAX_SECTORS)
    vcd_error ("image too big (%d sectors > %d sectors)", 
               image_size, CD_MAX_SECTORS);

  if (image_size > CD_74MIN_SECTORS)
    vcd_warn ("generated image (%d sectors) may not fit "
              "on 74min CDRs (%d sectors)", image_size, CD_74MIN_SECTORS);

  return image_size;
}

int
vcd_obj_write_image (VcdObj *obj, VcdDataSink *bin_file,
                     progress_callback_t callback, void *user_data)
{
  unsigned sectors, track;

  assert (obj != NULL);
  assert (sectors >= 0);
  assert (obj->sectors_written == 0);

  assert (obj->in_output);

  obj->progress_callback = callback;
  obj->callback_user_data = user_data;
  obj->bin_file = bin_file;
  
  if (_callback_wrapper (obj, true))
    return 1;

  if (_write_vcd_iso_track (obj))
    return 1;

  for (track = 0;track < _vcd_list_length (obj->mpeg_sequence_list);track++) {
    obj->in_track++;

    if (_callback_wrapper (obj, true))
      return 1;

    if (_write_sectors (obj, track))
      return 1;
  }

  if (_callback_wrapper (obj, true))
    return 1;
  
  return 0; /* ok */
}

void
vcd_obj_end_output (VcdObj *obj)
{
  assert (obj != NULL);

  assert (obj->in_output);
  obj->in_output = false;

  _vcd_directory_destroy (obj->dir);
  _vcd_salloc_destroy (obj->iso_bitmap);

  _dict_clean (obj);
  _vcd_list_free (obj->buffer_dict_list, true);

  if (obj->bin_file)
    vcd_data_sink_destroy (obj->bin_file); /* fixme -- try moving it to
                                              write_image */
  obj->bin_file = NULL;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
