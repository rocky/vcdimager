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
#include "vcd_mpeg.h"
#include "vcd_salloc.h"
#include "vcd_files.h"
#include "vcd_files_private.h"
#include "vcd_cd_sector.h"
#include "vcd_directory.h"
#include "vcd_logging.h"
#include "vcd_util.h"

static const char _rcsid[] = "$Id$";

/* some parameters */
#define PRE_TRACK_GAP (2*75)
#define PRE_DATA_GAP  30
#define POST_DATA_GAP 45

static const char zero[CDDA_SIZE] = { 0, };

/*
 */

struct _cust_file_t {
  char *iso_pathname;
  VcdDataSource *file;
  bool raw_flag;
  
  uint32_t size;
  uint32_t start_extent;
  uint32_t sectors;
};

/*
 */

struct _dict_t
{
  char *key;
  uint32_t sector;
  uint32_t length;
  void *buf;
  uint8_t flags;
};

static void
_dict_insert (VcdObj *obj, const char key[], uint32_t sector, uint32_t length, 
              uint8_t end_flags)
{
  struct _dict_t *_new_node;

  assert (key != NULL);
  assert (sector != SECTOR_NIL);
  assert (length > 0);

  if (_vcd_salloc (obj->iso_bitmap, sector, length) == SECTOR_NIL)
    assert (0);

  _new_node = _vcd_malloc (sizeof (struct _dict_t));

  _new_node->key = strdup (key);
  _new_node->sector = sector;
  _new_node->length = length;
  _new_node->buf = _vcd_malloc (length * ISO_BLOCKSIZE);
  _new_node->flags = end_flags;

  _vcd_list_prepend (obj->buffer_dict_list, _new_node);
}

static 
int _dict_key_cmp (struct _dict_t *a, char *b)
{
  assert (a != NULL);
  assert (b != NULL);

  return !strcmp (a->key, b);
}

static 
int _dict_sector_cmp (struct _dict_t *a, uint32_t *b)
{
  assert (a != NULL);
  assert (b != NULL);

  return (a->sector <= *b && (*b - a->sector) < a->length);
}

static const struct _dict_t *
_dict_get_bykey (VcdObj *obj, const char key[])
{
  VcdListNode *node;

  assert (obj != NULL);
  assert (key != NULL);

  node = _vcd_list_find (obj->buffer_dict_list,
                         (_vcd_list_iterfunc) _dict_key_cmp,
                         (char *) key);
  
  if (node)
    return _vcd_list_node_data (node);

  return NULL;
}

static const struct _dict_t *
_dict_get_bysector (VcdObj *obj, uint32_t sector)
{
  VcdListNode *node;

  assert (obj != NULL);
  assert (sector != SECTOR_NIL);

  node = _vcd_list_find (obj->buffer_dict_list, 
                         (_vcd_list_iterfunc) _dict_sector_cmp, 
                         &sector);

  if (node)
    return _vcd_list_node_data (node);

  return NULL;
}

static uint8_t
_dict_get_sector_flags (VcdObj *obj, uint32_t sector)
{
  const struct _dict_t *p;

  assert (sector != SECTOR_NIL);

  p = _dict_get_bysector (obj, sector);

  if (p)
    return (((sector - p->sector)+1 == p->length)
            ? p->flags : 0);

  return 0;
}

static void *
_dict_get_sector (VcdObj *obj, uint32_t sector)
{
  const struct _dict_t *p;

  assert (sector != SECTOR_NIL);

  p = _dict_get_bysector (obj, sector);

  if (p)
    return ((char *)p->buf) + ((sector - p->sector) * ISO_BLOCKSIZE);

  return NULL;
}

static void
_dict_clean (VcdObj *obj)
{
  VcdListNode *node;

  while ((node = _vcd_list_begin (obj->buffer_dict_list)))
    {
      struct _dict_t *p = _vcd_list_node_data (node);

      free (p->key);
      free (p->buf);

      _vcd_list_node_free (node, true);
    }
}

/*
 */

VcdObj *
vcd_obj_new (vcd_type_t vcd_type)
{
  VcdObj *new_obj = NULL;

  switch (vcd_type) 
    {
    case VCD_TYPE_VCD11:
    case VCD_TYPE_VCD2:
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

  new_obj->custom_file_list = _vcd_list_new ();

  new_obj->type = vcd_type;

  new_obj->mpeg_track_list = _vcd_list_new ();

  return new_obj;
}

const mpeg_info_t *
vcd_obj_get_mpeg_info (VcdObj *obj, int track_id)
{
  VcdListNode *node = NULL;
  mpeg_track_t *track = NULL;

  assert (track_id >= 0);

  node = _vcd_list_at (obj->mpeg_track_list, track_id);

  assert (node != NULL);

  track = (mpeg_track_t *)_vcd_list_node_data (node);

  return &(track->mpeg_info);
}

void
vcd_obj_remove_mpeg_track (VcdObj *obj, int track_id)
{
  int length;
  mpeg_track_t *track = NULL;
  VcdListNode *node = NULL;

  assert (track_id >= 0);

  node = _vcd_list_at (obj->mpeg_track_list, track_id);
  
  assert (node != NULL);

  track = (mpeg_track_t *) _vcd_list_node_data (node);

  vcd_data_source_destroy (track->source);
  if (track->scanpoints)
    _vcd_list_free (track->scanpoints, true);

  length = track->length_sectors;
  length += PRE_TRACK_GAP + PRE_DATA_GAP + 0 + POST_DATA_GAP;

  /* fixup offsets */
  {
    VcdListNode *node2 = node;
    while ((node2 = _vcd_list_node_next (node2)) != NULL)
      ((mpeg_track_t *) _vcd_list_node_data (node))->relative_start_extent -= length;
  }

  obj->relative_end_extent -= length;

  /* shift up */
  _vcd_list_node_free (node, true);
}

static void
_make_scantable (mpeg_track_t *track)
{
  char buf[M2F2_SIZE] = { 0, };
  mpeg_type_info_t ti;
  unsigned sect;

  VcdList *table = _vcd_list_new ();
  uint32_t *last_sect;

  double last_gop_timecode = 0;
  double last_i_timecode = 0;
  double offset = 0;
  bool got_first_gop = false;

  unsigned n = 0;

  last_sect = _vcd_malloc (sizeof (uint32_t));
  *last_sect = 0;
  _vcd_list_append (table, last_sect);

  vcd_data_source_seek (track->source, 0); 

  for (sect = 0; sect < track->length_sectors; sect++)
    {
      vcd_data_source_read (track->source, buf, sizeof (buf), 1);
  
      switch (vcd_mpeg_get_type (buf, &ti))
        {
        case MPEG_TYPE_INVALID:
          vcd_debug ("mpeg scan: invalid mpeg block @%d", sect);
          break;
          
        case MPEG_TYPE_VIDEO:
          if (ti.video.gop_flag)
            {
              double timecode;

              timecode = ti.video.timecode.f;

              timecode /= track->mpeg_info.frate;

              timecode += ti.video.timecode.s;
              timecode += ti.video.timecode.m * 60;
              timecode += ti.video.timecode.h * 60 * 60;

              /* vcd_debug ("GOP: @%.4d  %6f", sect, timecode); */

              if (!got_first_gop)
                {
                  got_first_gop = true;
                  offset = timecode;

                  if (timecode > 1)
                    vcd_warn ("mpeg stream begins with offsetted timecode of %f seconds",
                              offset);
                  else
                    offset = 0; /* ignore offset */
                }

              last_gop_timecode = timecode;
            }

          if (ti.video.i_frame_flag)
            {
              double timecode;

              if (!got_first_gop)
                vcd_warn ("ups... got I-frame before GOP");

              timecode = ti.video.rel_timecode;
              
              timecode /= track->mpeg_info.frate;
              
              timecode += last_gop_timecode;

              if (timecode <= last_i_timecode)
                {
                  /* break */
                }
              else
                {
                  /* vcd_debug ("I-Frame: @%.4d +%6f", sect, timecode); */

                  while (fabs ((0.5 * (double) n) - (timecode - offset)) >
                         fabs ((0.5 * (double) n) - (last_i_timecode - offset)))
                    {
                      uint32_t tmp = *last_sect;
                      n++;

                      last_sect = _vcd_malloc (sizeof (uint32_t));
                      *last_sect = tmp;
                      _vcd_list_append (table, last_sect);
                      /* vcd_debug ("append I-Frame: @%.4d +%6f", *last_sect, last_i_timecode); */
                    }
                  
                  /* just overwrite last element in list... */
                  *last_sect = sect;
                  last_i_timecode = timecode;
                      
                  /* vcd_debug ("replace I-Frame: @%.4d +%6f", sect, timecode); */
                }
            }
          break;

        case MPEG_TYPE_AUDIO:
        case MPEG_TYPE_AUDIO_1:
        case MPEG_TYPE_OGT: 
        case MPEG_TYPE_NULL:
          break;

        default:
          vcd_debug ("mpeg scan: type %d @%d", ti.type, sect);
          break;
        }
    }


  /* be sure to have the right one... */
  sect = *last_sect;

  while ((last_i_timecode - offset) > (0.5 * n))
    {
      n++;
      last_sect = _vcd_malloc (sizeof (uint32_t));
      *last_sect = sect;
      _vcd_list_append (table, last_sect);
      /* vcd_debug ("filling I-Frame: @%.4d +%6f", sect, last_i_timecode); */
    }  

  track->playtime = last_i_timecode - offset;

  /* done... */

#ifdef VCD_DEBUG
  {
    unsigned s = 0;
    VcdListNode *n;

    for(n = _vcd_list_begin (table);
        n; n = _vcd_list_node_next (n), s++)
      {
        vcd_debug ("%6d %6d", s, *((uint32_t*) _vcd_list_node_data (n)));
      }
  }
#endif

  track->scanpoints = table;
}

int
vcd_obj_append_mpeg_track (VcdObj *obj, VcdDataSource *mpeg_file)
{
  unsigned length;
  int j;
  mpeg_track_t *track = NULL;
  bool got_info = false;

  int track_no = _vcd_list_length (obj->mpeg_track_list);

  assert (obj != NULL);
  assert (mpeg_file != NULL);

  length = vcd_data_source_stat (mpeg_file);

  if (length % 2324)
    vcd_warn ("track# %d not a multiple of 2324 bytes", track_no);
  
  track = _vcd_malloc (sizeof (mpeg_track_t));

  track->source = mpeg_file;
  track->length_sectors = length / 2324;

  obj->relative_end_extent += PRE_TRACK_GAP;
  
  track->relative_start_extent = obj->relative_end_extent;

  obj->relative_end_extent += PRE_DATA_GAP + length / 2324 + POST_DATA_GAP;

  if (track->length_sectors < 75)
    vcd_warn ("mpeg stream shorter than 75 sectors");

  for (j = 0;j < MIN(150, track->length_sectors);j++) 
    {
      char buf[M2F2_SIZE] = { 0, };
      
      vcd_data_source_read (mpeg_file, buf, sizeof (buf), 1);
      
      if (vcd_mpeg_get_info (buf, &(track->mpeg_info)))
        {
          if (obj->type == VCD_TYPE_VCD11
              && track->mpeg_info.norm != MPEG_NORM_FILM
              && track->mpeg_info.norm != MPEG_NORM_NTSC)
            vcd_warn ("VCD 1.x should contain only NTSC/FILM video");

          if (obj->type == VCD_TYPE_SVCD
              && track->mpeg_info.version == MPEG_VERS_MPEG1)
            vcd_warn ("SVCD should not contain MPEG1 tracks!");
          
          if ((obj->type == VCD_TYPE_VCD2 || obj->type == VCD_TYPE_VCD11)
              && track->mpeg_info.version == MPEG_VERS_MPEG2)
            vcd_warn ("VCD should not contain MPEG2 tracks!");
          
          got_info = true;
          break;
        }
  }

  if (!got_info) 
    {
      vcd_warn ("could not determine mpeg format"
                " -- assuming it is ok nevertheless");
    }

  if (obj->type == VCD_TYPE_SVCD ||
      obj->type == VCD_TYPE_VCD2)
    {
      vcd_info ("scanning mpeg track #%d for scanpoints...", 
                track_no);
      _make_scantable (track);
      vcd_debug ("track# %d's estimated playtime: %.2f seconds", 
                 track_no, track->playtime);
    }

  vcd_data_source_close (mpeg_file);
  
  _vcd_list_append (obj->mpeg_track_list, track);

  return track_no;
}

void 
vcd_obj_destroy (VcdObj *obj)
{
  VcdListNode *node;

  assert (obj != NULL);
  assert (!obj->in_output);

  free (obj->iso_volume_label);
  free (obj->iso_application_id);

  for(node = _vcd_list_begin (obj->custom_file_list);
      node;
      node = _vcd_list_node_next (node))
    {
      struct _cust_file_t *p = _vcd_list_node_data (node);
    
      free (p->iso_pathname);
    }

  _vcd_list_free (obj->custom_file_list, true);

  while (_vcd_list_length (obj->mpeg_track_list))
    vcd_obj_remove_mpeg_track (obj, 0);
  _vcd_list_free (obj->mpeg_track_list, true);

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

  for (n = 0, node = _vcd_list_begin (obj->mpeg_track_list);
         node != NULL;
         n++, node = _vcd_list_node_next (node))
    {
      mpeg_track_t *track = _vcd_list_node_data (node);

      cue_track (cue_file, obj->bin_file_2336_flag, n+2, 
                 track->relative_start_extent + obj->iso_size);
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
    obj->iso_volume_label = strdup ((const char *)arg);
    break;
  case VCD_PARM_APPLICATION_ID:
    free (obj->iso_application_id);
    obj->iso_application_id = strdup ((const char *)arg);
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
vcd_obj_add_file (VcdObj *obj, const char iso_pathname[],
                  VcdDataSource *file, bool raw_flag)
{
  uint32_t size = 0, sectors = 0;
 
  assert (obj != NULL);
  assert (file != NULL);
  assert (iso_pathname != NULL);
  assert (strlen (iso_pathname) > 0);
  assert (file != NULL);

  size = vcd_data_source_stat(file);

  if (raw_flag)
    {
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
    struct _cust_file_t *p;
    char *_iso_pathname = _vcd_strdup_upper (iso_pathname);
    
    if (!_vcd_iso_pathname_valid_p (_iso_pathname))
      {
        vcd_error("pathname `%s' is not a valid iso pathname", 
                  _iso_pathname);
        return 1;
      }

    p = _vcd_malloc (sizeof (struct _cust_file_t));
    
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
_finalize_vcd_iso_track (VcdObj *obj)
{
  int n;

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

  if (obj->type == VCD_TYPE_VCD2
      || obj->type == VCD_TYPE_SVCD)
    {
      _dict_insert (obj, "lot", LOT_VCD_SECTOR, LOT_VCD_SIZE, SM_EOF); /* LOT.VCD */            /* EOF */
      _dict_insert (obj, "psd", PSD_VCD_SECTOR, _vcd_len2blocks (get_psd_size (obj), ISO_BLOCKSIZE), SM_EOF);            /* PSD.VCD */            /* EOF */
    }

  if (obj->type == VCD_TYPE_SVCD)
    {
      _dict_insert (obj, "tracks", TRACKS_SVD_SECTOR, 1, SM_EOF);      /* TRACKS.SVD */
      _dict_insert (obj, "search", SEARCH_DAT_SECTOR, 
                    _vcd_len2blocks (get_search_dat_size (obj), ISO_BLOCKSIZE), SM_EOF); /* SEARCH.DAT */
    }

  /* keep rest of vcd sector blank -- paranoia */
  for(n = 0;n < 225;n++) 
    {
      uint32_t rc = _vcd_salloc (obj->iso_bitmap, n, 1);
#if VCD_DEBUG
      if (rc != SECTOR_NIL)
        vcd_debug ("blanking... %d", rc);
#else
      rc = 0; /* dummy */
#endif
    }
  
  assert (_vcd_salloc_get_highest (obj->iso_bitmap) + 1 == 225); /* fixme --
                                                                momentary
                                                                limitation */

  if (obj->type == VCD_TYPE_VCD2)
    {
      uint32_t sector = _vcd_salloc_get_highest (obj->iso_bitmap) + 1;

      _dict_insert (obj, "lot_x", sector, LOT_VCD_SIZE, SM_EOF);

      sector = _vcd_salloc_get_highest (obj->iso_bitmap) + 1;

      _dict_insert (obj, "psd_x", sector,
                    _vcd_len2blocks (get_psd_size (obj), ISO_BLOCKSIZE),
                    SM_EOF);

      sector = _vcd_salloc_get_highest (obj->iso_bitmap) + 1;

      _dict_insert (obj, "scandata", sector, 
                    _vcd_len2blocks (get_scandata_dat_size (obj),
                                     ISO_BLOCKSIZE),
                    SM_EOF);

    }

  obj->custom_file_start_extent =
    _vcd_salloc_get_highest (obj->iso_bitmap) + 1;

  switch (obj->type) {
  case VCD_TYPE_VCD11:
  case VCD_TYPE_VCD2:
    /* _vcd_directory_mkdir (obj->dir, "CDDA"); */
    _vcd_directory_mkdir (obj->dir, "CDI");
    _vcd_directory_mkdir (obj->dir, "EXT");
    /* _vcd_directory_mkdir (obj->dir, "KARAOKE"); */
    _vcd_directory_mkdir (obj->dir, "MPEGAV");
    _vcd_directory_mkdir (obj->dir, "SEGMENT");
    _vcd_directory_mkdir (obj->dir, "VCD");

    _vcd_directory_mkfile (obj->dir, "VCD/ENTRIES.VCD", 
                           _dict_get_bykey (obj, "entries")->sector, 
                           ISO_BLOCKSIZE, false, 0);    
    _vcd_directory_mkfile (obj->dir, "VCD/INFO.VCD",
                           _dict_get_bykey (obj, "info")->sector, 
                           ISO_BLOCKSIZE, false, 0);

    /* only for vcd2.0 */
    if (obj->type == VCD_TYPE_VCD2)
      {
        _vcd_directory_mkfile (obj->dir, "VCD/LOT.VCD", 
                               _dict_get_bykey (obj, "lot")->sector, 
                               ISO_BLOCKSIZE*LOT_VCD_SIZE, false, 0);
        _vcd_directory_mkfile (obj->dir, "VCD/PSD.VCD", 
                               _dict_get_bykey (obj, "psd")->sector, 
                               get_psd_size (obj), false, 0);
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
    _vcd_directory_mkdir (obj->dir, "SVCD");

    _vcd_directory_mkfile (obj->dir, "SVCD/ENTRIES.SVD",
                           _dict_get_bykey (obj, "entries")->sector, 
                           ISO_BLOCKSIZE, false, 0);    
    _vcd_directory_mkfile (obj->dir, "SVCD/INFO.SVD",
                           _dict_get_bykey (obj, "info")->sector, 
                           ISO_BLOCKSIZE, false, 0);
    _vcd_directory_mkfile (obj->dir, "SVCD/LOT.SVD",
                           _dict_get_bykey (obj, "lot")->sector, 
                           ISO_BLOCKSIZE*LOT_VCD_SIZE, false, 0);
    _vcd_directory_mkfile (obj->dir, "SVCD/PSD.SVD", 
                           _dict_get_bykey (obj, "psd")->sector, 
                           get_psd_size (obj), false, 0);
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

  /* EXT files */

  if (obj->type == VCD_TYPE_VCD2) 
    {
      /* psd_x -- extended PSD */
      _vcd_directory_mkfile (obj->dir, "EXT/PSD_X.VCD",
                             _dict_get_bykey (obj, "psd_x")->sector, 
                             get_psd_size (obj), false, 1);

      /* lot_x -- extended LOT */
      _vcd_directory_mkfile (obj->dir, "EXT/LOT_X.VCD",
                             _dict_get_bykey (obj, "lot_x")->sector, 
                             ISO_BLOCKSIZE*LOT_VCD_SIZE, false, 1);
      
      /* scandata.dat -- scanpoints */
      _vcd_directory_mkfile (obj->dir, "EXT/SCANDATA.DAT",
                             _dict_get_bykey (obj, "scandata")->sector, 
                             get_scandata_dat_size (obj), false, 1);
    }

  /* custom files */

  {
    VcdListNode *node;

    for (node = _vcd_list_begin (obj->custom_file_list);
           node != NULL;
           node = _vcd_list_node_next (node))
      {
        struct _cust_file_t *p = _vcd_list_node_data (node);

        p->start_extent = _vcd_salloc(obj->iso_bitmap, SECTOR_NIL, p->sectors);
        
        assert(p->start_extent != SECTOR_NIL);

        _vcd_directory_mkfile(obj->dir, p->iso_pathname, p->start_extent,
                              p->size, p->raw_flag, 1);
      }
  }

  /* calculate iso size -- after this point no sector shall be
     allocated anymore */

  obj->iso_size =
    MAX (MIN_ISO_SIZE, _vcd_salloc_get_highest (obj->iso_bitmap) + 1);

  vcd_debug ("iso9660: highest alloced sector is %d (using %d as isosize)", 
             _vcd_salloc_get_highest (obj->iso_bitmap), obj->iso_size);

  /* after this point the ISO9660's size is frozen */

  {
    VcdListNode *node = NULL;
    n = 0;

    for (n = 0, node = _vcd_list_begin (obj->mpeg_track_list);
         node != NULL;
         n++, node = _vcd_list_node_next (node))
      {
        char avseq_pathname[128] = { 0, };
        const char *fmt = NULL;
        mpeg_track_t *track = _vcd_list_node_data (node);
        uint32_t extent = track->relative_start_extent;
      
        extent += obj->iso_size;

        switch (obj->type) 
          {
          case VCD_TYPE_VCD11:
          case VCD_TYPE_VCD2:
            fmt = "MPEGAV/AVSEQ%2.2d.DAT";
            break;
          case VCD_TYPE_SVCD:
            fmt = "MPEG2/AVSEQ%2.2d.MPG";
            break;
          default:
            assert(1);
          }

        assert (n < 98);
      
        snprintf (avseq_pathname, sizeof (avseq_pathname), fmt, n+1);
        
        _vcd_directory_mkfile (obj->dir, avseq_pathname, extent+PRE_DATA_GAP,
                               track->length_sectors*ISO_BLOCKSIZE,
                               true, n + 1);

        if (obj->broken_svcd_mode_flag)
          {
            snprintf (avseq_pathname, sizeof (avseq_pathname), 
                      "MPEGAV/AVSEQ%2.2d.MPG", n+1);

            vcd_warn ("broken svcd mode: adding additional `%s' entry", 
                      avseq_pathname);
            
            _vcd_directory_mkfile (obj->dir, avseq_pathname,
                                   extent+PRE_DATA_GAP,
                                   track->length_sectors*ISO_BLOCKSIZE,
                                   true, n + 1);
          }

        extent += obj->iso_size;
      }

  }

  /* register isofs dir structures */
  {
    uint32_t dirs_size = _vcd_directory_get_size (obj->dir);
    
    if (dirs_size+2 >= 75)
      vcd_error ("directory section to big");
    
    _vcd_salloc_free (obj->iso_bitmap, 18, dirs_size + 2);
    
    _dict_insert (obj, "dir", 18, dirs_size, SM_EOR|SM_EOF);
    _dict_insert (obj, "ptl", 18 + dirs_size, 1, SM_EOR|SM_EOF);
    _dict_insert (obj, "ptm", 18 + dirs_size + 1, 1, SM_EOR|SM_EOF);
  }
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
    _pi.total_tracks = _vcd_list_length (obj->mpeg_track_list) + 1;

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
    vcd_data_sink_write(obj->bin_file, buf+12, M2RAW_SIZE, 1);
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

  if (obj->type == VCD_TYPE_VCD2)
    {
      set_lot_vcd (obj, _dict_get_bykey (obj, "lot_x")->buf);
      set_psd_vcd (obj, _dict_get_bykey (obj, "psd_x")->buf);
      set_scandata_dat (obj, _dict_get_bykey (obj, "scandata")->buf);
    }
  
  if (obj->type == VCD_TYPE_VCD2 
      || obj->type == VCD_TYPE_SVCD) 
    {
      set_lot_vcd (obj, _dict_get_bykey (obj, "lot")->buf);
      set_psd_vcd (obj, _dict_get_bykey (obj, "psd")->buf);
    }

  if (obj->type == VCD_TYPE_SVCD) 
    {
      set_tracks_svd (obj, _dict_get_bykey (obj, "tracks")->buf);
      set_search_dat (obj, _dict_get_bykey (obj, "search")->buf);
    }

  /* start actually writing stuff */

  vcd_info ("writing track 1 (ISO9660)...");

  /* 00:02:00 -> 00:04:74 */
  for (n = 0;n < 225; n++)
    {
      const void *content = NULL;
      uint8_t flags = SM_DATA;

      content = _dict_get_sector (obj, n);
      flags |= _dict_get_sector_flags (obj, n);
      
      if (content == NULL)
        content = zero;

      _write_m2_image_sector (obj, content, n, 0, 0, flags, 0);
    }

  /* EXT stuff */

  for (n = 225;n < obj->custom_file_start_extent; n++)
    {
      const void *content = NULL;
      uint8_t flags = SM_DATA;

      content = _dict_get_sector (obj, n);
      flags |= _dict_get_sector_flags (obj, n);
      
      if (content == NULL)
        {
          vcd_debug ("unexpected empty EXT sector");
          content = zero;
        }

      _write_m2_image_sector (obj, content, n, 1, 0, flags, 0);
    }

  /* write custom files */
  {
    VcdListNode *node;
    
    for (node = _vcd_list_begin (obj->custom_file_list);
           node != NULL;
           node = _vcd_list_node_next (node))
      {
        struct _cust_file_t *p = _vcd_list_node_data (node);
        
        vcd_info ("writing file `%s' (%d bytes%s)", 
                  p->iso_pathname, p->size, 
                  p->raw_flag ? ", raw sectors file": "");
        if (p->raw_flag)
          _write_source_mode2_raw (obj, p->file, p->start_extent);
        else
          _write_source_mode2_form1 (obj, p->file, p->start_extent);
      }
  }
  
  /* blank unalloced tracks */
  while ((n = _vcd_salloc (obj->iso_bitmap, SECTOR_NIL, 1)) < obj->iso_size)
    _write_m2_image_sector (obj, zero, n, 0, 0, SM_DATA, 0);

  return 0;
}

static int
_write_sectors (VcdObj *obj, int track_idx)
{
  mpeg_track_t *track = 
    _vcd_list_node_data (_vcd_list_at (obj->mpeg_track_list, track_idx));
  int n, lastsect = obj->sectors_written;
  char buf[2324];
  struct {
    int audio;
    int video;
    int null;
    int unknown;
  } mpeg_packets = {0, 0, 0, 0};

  {
    char *norm_str = NULL;
    mpeg_info_t *info = &(track->mpeg_info);

    switch (info->norm) {
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
                  info->hsize, info->vsize, info->frate);
        norm_str = strdup (buf);
      }
      break;
    }

    vcd_info ("writing track %d, %s, %s...", track_idx + 2,
              (info->version == MPEG_VERS_MPEG1 ? "MPEG1" : "MPEG2"),
              norm_str);

    free (norm_str);
  }

  for (n = 0; n < PRE_TRACK_GAP;n++)
    _write_m2_image_sector (obj, zero, lastsect++, 0, 0, SM_FORM2, 0);

  for (n = 0; n < PRE_DATA_GAP;n++)
    _write_m2_image_sector (obj, zero, lastsect++, track_idx + 1,
                            0, SM_FORM2|SM_REALT, 0);

  vcd_data_source_seek (track->source, 0);

  for (n = 0; n < track->length_sectors;n++) {
    int ci = 0, sm = 0, cnum = 0;

    vcd_data_source_read (track->source, buf, 2324, 1);

    switch (vcd_mpeg_get_type (buf, NULL)) {
    case MPEG_TYPE_VIDEO:
      mpeg_packets.video++;
      sm = SM_FORM2|SM_REALT|SM_VIDEO;
      ci = CI_VIDEO;
      cnum = 1;
      break;
    case MPEG_TYPE_AUDIO:
      mpeg_packets.audio++;
      sm = SM_FORM2|SM_REALT|SM_AUDIO;
      ci = CI_AUDIO;
      cnum = CN_AUDIO;
      break;
    case MPEG_TYPE_AUDIO_1:
      mpeg_packets.audio++;
      sm = SM_FORM2|SM_REALT|SM_AUDIO;
      ci = CI_AUDIO2;
      cnum = CN_AUDIO2;
      break;
    case MPEG_TYPE_OGT:
      mpeg_packets.video++; /* fixme */
      sm = SM_FORM2|SM_REALT|SM_VIDEO;
      ci = CI_OGT;
      cnum = CN_OGT;
      break;
    case MPEG_TYPE_NULL:
      mpeg_packets.null++;
      sm = SM_FORM2|SM_REALT;
      ci = CI_OTHER;
      cnum = 0;
      break;
    case MPEG_TYPE_UNKNOWN:
      mpeg_packets.unknown++;
      vcd_warn ("unknown packet @%d encounterd", n);
      sm = SM_FORM2|SM_REALT;
      ci = CI_OTHER;
      cnum = 0;
      break;
    case MPEG_TYPE_END:
      if (n < track->length_sectors)
        vcd_warn ("Program end marker seen at packet %d"
                  " -- before actual end of stream", n);
      sm = SM_FORM2|SM_REALT;
      ci = CI_OTHER;
      cnum = 0;
      break;
    case MPEG_TYPE_INVALID:
      vcd_error ("invalid mpeg packet found at packet# %d"
                 " -- please fix this mpeg file!", n);
      vcd_data_source_close (track->source);
      return 1;
      break;
    default:
      assert (0);
    }

    if (n == track->length_sectors-1)
      sm |= SM_EOR;

    if (_write_m2_image_sector (obj, buf, lastsect++, track_idx + 1, cnum, sm, ci))
      break;
  }

  vcd_data_source_close (track->source);

  for (n = 0; n < POST_DATA_GAP;n++)
    _write_m2_image_sector (obj, zero, lastsect++, track_idx + 1,
                            0, SM_FORM2|SM_REALT, 0);

  vcd_debug ("MPEG packet statistics: %d video, %d audio, %d null, %d unknown",
             mpeg_packets.video, mpeg_packets.audio, mpeg_packets.null,
             mpeg_packets.unknown);

  return 0;
}

long
vcd_obj_get_image_size (VcdObj *obj)
{
  long size_sectors = -1;

  assert (!obj->in_output);
  
  if (_vcd_list_length (obj->mpeg_track_list) > 0) {
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
  assert (_vcd_list_length (obj->mpeg_track_list) > 0);

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
    vcd_error ("image too big (%d sectors)", image_size);

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

  for (track = 0;track < _vcd_list_length (obj->mpeg_track_list);track++) {
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
