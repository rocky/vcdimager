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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "vcd.h"
#include "vcd_obj.h"

#include "vcd_types.h"
#include "vcd_iso9660.h"
#include "vcd_mpeg.h"
#include "vcd_salloc.h"
#include "vcd_transfer.h"
#include "vcd_files.h"
#include "vcd_files_private.h"
#include "vcd_cd_sector.h"
#include "vcd_directory.h"
#include "vcd_logging.h"
#include "vcd_util.h"

/* some parameters */
#define PRE_TRACK_GAP (2*75)
#define PRE_DATA_GAP  30
#define POST_DATA_GAP 45

static const char zero[ISO_BLOCKSIZE] = { 0, };

VcdObj *
vcd_obj_new (vcd_type_t vcd_type)
{
  VcdObj *new_obj = NULL;

  switch (vcd_type) {
  case VCD_TYPE_VCD2:
  case VCD_TYPE_SVCD:
    break;
  default:
    return new_obj; /* NULL */
  }

  new_obj = malloc (sizeof (VcdObj));
  memset (new_obj, 0, sizeof (VcdObj));
  new_obj->custom_files = NULL; /* paranoid */

  new_obj->type = vcd_type;

  return new_obj;
}

const mpeg_info_t *
vcd_obj_get_mpeg_info (VcdObj *obj, int track_id)
{
  assert (track_id >= 0);
  assert (track_id < obj->mpeg_tracks_num);

  return &(obj->mpeg_tracks[track_id].mpeg_info); 
}

void
vcd_obj_remove_mpeg_track (VcdObj *obj, int track_id)
{
  int n, length;

  assert (track_id >= 0);
  assert (track_id < obj->mpeg_tracks_num);
  
  vcd_data_source_destroy (obj->mpeg_tracks[track_id].source);
  
  length = obj->mpeg_tracks[track_id].length_sectors;
  length += PRE_TRACK_GAP + PRE_DATA_GAP + 0 + POST_DATA_GAP;

  /* fixup offsets */
  for (n = track_id+1;n < obj->mpeg_tracks_num;n++)
    obj->mpeg_tracks[n].relative_start_extent -= length;

  obj->relative_end_extent -= length;

  /* shift up */
  for (n = track_id+1;n < obj->mpeg_tracks_num;n++)
    memcpy (&(obj->mpeg_tracks[n-1]), &(obj->mpeg_tracks[n]), sizeof (obj->mpeg_tracks[n]));

  obj->mpeg_tracks_num--;
}

int
vcd_obj_append_mpeg_track (VcdObj *obj, VcdDataSource *mpeg_file)
{
  unsigned length;
  int j;
  struct mpeg_track_t *track;
  double begin = -1, end = -1;
  int got_info = 0;

  assert (obj != NULL);
  assert (mpeg_file != NULL);

  length = vcd_data_source_stat (mpeg_file);

  if (length % 2324)
    vcd_warn ("track# %d not a multiple of 2324 bytes", obj->mpeg_tracks_num);

  track = &(obj->mpeg_tracks[obj->mpeg_tracks_num]);

  track->source = mpeg_file;
  track->length_sectors = length / 2324;

  obj->relative_end_extent += PRE_TRACK_GAP;
  
  track->relative_start_extent = obj->relative_end_extent;

  obj->relative_end_extent += PRE_DATA_GAP + length / 2324 + POST_DATA_GAP;

  for (j = 0;;j++) 
    {
      char buf[M2F2_SIZE] = { 0, };
      
      vcd_data_source_read (mpeg_file, buf, sizeof (buf), 1);

      /* find beginning timecode */
      if (begin < 0)
        begin = vcd_mpeg_get_timecode (buf);
      
      if (!got_info && vcd_mpeg_get_info (buf, &(track->mpeg_info)))
        {
          if (obj->type == VCD_TYPE_SVCD
              && track->mpeg_info.version == MPEG_VERS_MPEG1)
            vcd_warn ("SVCD should not contain MPEG1 tracks!");
          
          if (obj->type == VCD_TYPE_VCD2
              && track->mpeg_info.version == MPEG_VERS_MPEG2)
            vcd_warn ("VCD should not contain MPEG2 tracks!");
          
          got_info = 1;
          break;
        }

      if (got_info && begin >= 0)
        break;

      if (j > 75)
        break;
  }

  if (!got_info) 
    {
      vcd_warn ("could not determine mpeg format"
                " -- assuming it is ok nevertheless");
    }

  if (begin < 0)
    vcd_warn ("couldn't determine starting timecode");

  /* find ending timecode */

  for(j = 1;;j++) 
    {
      char buf[M2F2_SIZE] = { 0, };

      vcd_data_source_seek (mpeg_file, (track->length_sectors - j) * 2324); 

      vcd_data_source_read (mpeg_file, buf, sizeof (buf), 1);
      
      if (end < 0)
        end = vcd_mpeg_get_timecode (buf);
      
      if (end >= 0)
        break;

      if (j > 150) 
        {
          vcd_warn ("could not determine ending timecode");
          break;
        }
    }

  vcd_data_source_close (mpeg_file);

  if (begin < 0 || end < 0 || begin >= end) 
    {
      vcd_warn ("track# %d: timecode begin with %f / end at %f " 
                "-- setting playtime to 0", 
                obj->mpeg_tracks_num, begin, end);
      track->playtime = 0;
    }

  track->playtime = (double) (0.5 + end - begin);

  vcd_debug ("track# %d's estimated playtime: %d seconds", 
             obj->mpeg_tracks_num, track->playtime);
  
  return obj->mpeg_tracks_num++;
}

void 
vcd_obj_destroy (VcdObj *obj)
{
  assert (obj != NULL);

  /* destroy cust_file linked list */
  {
    struct cust_file *p = obj->custom_files;
    
    while (p)
      {
        struct cust_file *tmp = p->next;

        free (p->iso_pathname);
        free (p);
        p = tmp;
      }

    obj->custom_files = NULL;
  }

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

  assert (obj != NULL);

  cue_start (cue_file, bin_fname);
  cue_track (cue_file, obj->bin_file_2336_flag, 1, 0); /* ISO9660 track */

  for (n = 0;n < obj->mpeg_tracks_num;n++)
    cue_track (cue_file, obj->bin_file_2336_flag, n+2, 
               obj->mpeg_tracks[n].relative_start_extent+obj->iso_size);

  vcd_data_sink_destroy (cue_file);

  return 0;
}

void
vcd_obj_set_param (VcdObj *obj, vcd_parm_t param, const void *arg)
{
  assert (arg != NULL);

  switch (param) {
  case VCD_PARM_VOLUME_LABEL:
    obj->iso_volume_label = strdup ((const char *)arg);
    break;
  case VCD_PARM_SEC_TYPE:
    switch (*(const int *)arg) {
    case 2336:
      obj->bin_file_2336_flag = TRUE;
      break;
    case 2352:
      obj->bin_file_2336_flag = FALSE;
      break;
    default:
      assert (0);
    }
    break;
  default:
    assert (0);
    break;
  }
}

int
vcd_obj_add_file (VcdObj *obj, const char iso_pathname[],
                  VcdDataSource *file, int raw_flag)
{
  uint32_t size = 0, sectors = 0;
 
  assert (obj != NULL);
  assert (file != NULL);
  assert (iso_pathname != NULL);
  assert (strlen (iso_pathname) > 0);
  assert (file != NULL);

  size = vcd_data_source_stat(file);

  sectors = size / (raw_flag ? M2RAW_SIZE : M2F1_SIZE);

  if (raw_flag && (size % M2RAW_SIZE))
    {
      vcd_error("raw mode2 file must have size multiple of %d \n", M2RAW_SIZE);
      return 1;
    }

  if (size % M2F1_SIZE)
    sectors++;

  {
    struct cust_file **p = &(obj->custom_files);
    char *_iso_pathname = _vcd_iso_pathname_isofy (iso_pathname);

    if (!_vcd_iso_pathname_valid_p (_iso_pathname))
      {
        vcd_error("pathname `%s' is not a valid iso pathname", 
                  _iso_pathname);
        return 1;
      }


    while (*p)
      p = &(*p)->next;

    *p = malloc (sizeof (struct cust_file));
    memset (*p, 0, sizeof (struct cust_file));
    
    (*p)->file = file;
    (*p)->iso_pathname = _iso_pathname;
    (*p)->raw_flag = raw_flag;
  
    (*p)->size = size;
    (*p)->start_extent = 0;
    (*p)->sectors = sectors;
  }

  return 0;
}

static void
_finalize_vcd_iso_track (VcdObj *obj)
{
  int n;

  /* pre-alloc 16 blocks of ISO9660 required silence */
  if (_vcd_salloc (obj->iso_bitmap, 0, 16) == SECTOR_NIL)
    assert (0);

  /* keep karaoke sectors blank -- well... guess I'm too paranoid :) */
  if (_vcd_salloc (obj->iso_bitmap, 75, 75) == SECTOR_NIL) 
    assert (0);


  /* keep info area blank -- paranoid ? */
  if (_vcd_salloc (obj->iso_bitmap, 187, 38) == SECTOR_NIL) 
    assert (0);

  obj->pvd_sec = _vcd_salloc (obj->iso_bitmap, 16, 2);      /* pre-alloc descriptors, PVD */  /* EOR */
  obj->evd_sec = obj->pvd_sec+1;                           /* EVD */                         /* EOR+EOF */
  obj->dir_secs = _vcd_salloc (obj->iso_bitmap, 18, 75-18);
  obj->info_sec = _vcd_salloc (obj->iso_bitmap, INFO_VCD_SECTOR, 1);    /* INFO.VCD */        /* EOF */
  obj->entries_sec = _vcd_salloc (obj->iso_bitmap, ENTRIES_VCD_SECTOR, 1); /* ENTRIES.VCD  */ /* EOF */
  obj->lot_secs = _vcd_salloc (obj->iso_bitmap, LOT_VCD_SECTOR, LOT_VCD_SIZE);  /* LOT.VCD */ /* EOF */
  obj->psd_sec = _vcd_salloc (obj->iso_bitmap, PSD_VCD_SECTOR, 1); /* just one sec for now... */

  switch (obj->type)
    {
    case VCD_TYPE_SVCD:
      obj->tracks_sec = _vcd_salloc (obj->iso_bitmap, TRACKS_SVD_SECTOR, 1); /* TRACKS.SVD */
      
      /* fixme -- we create it with zero scan points for now, cause it's simpler */
      obj->search_secs = _vcd_salloc (obj->iso_bitmap, SEARCH_DAT_SECTOR, 1); /* SEARCH.DAT */

      assert(obj->tracks_sec != SECTOR_NIL);
      assert(obj->search_secs != SECTOR_NIL);
      break;

    case VCD_TYPE_VCD2:
      {
        uint32_t rc = _vcd_salloc (obj->iso_bitmap, 185,2);
        assert(rc != SECTOR_NIL);
      }
      break;

  default:
    vcd_error("VCD type not supported");
    break;

    }

  /* keep rest of vcd sector blank -- paranoia */
  for(n = 75;n < 225;n++) 
    {
      uint32_t rc = _vcd_salloc (obj->iso_bitmap, n,1);
      assert(rc == SECTOR_NIL);
    }

  set_psd_size (obj);

  /* these are set/put after the directory tree/records have been created */
  obj->ptl_sec = SECTOR_NIL; /* EOR+EOF */
  obj->ptm_sec = SECTOR_NIL; /* EOR+EOF */
    
  assert (obj->dir_secs != SECTOR_NIL);
  assert (obj->pvd_sec != SECTOR_NIL);
  assert (obj->info_sec != SECTOR_NIL);
  assert (obj->entries_sec != SECTOR_NIL);
  assert (obj->lot_secs != SECTOR_NIL);
  assert (obj->psd_sec != SECTOR_NIL);

  switch(obj->type) {
  case VCD_TYPE_VCD2:
    /* _vcd_directory_mkdir (obj->dir, "CDDA"); */
    _vcd_directory_mkdir (obj->dir, "CDI");
    _vcd_directory_mkdir (obj->dir, "EXT");
    /* _vcd_directory_mkdir (obj->dir, "KARAOKE"); */
    _vcd_directory_mkdir (obj->dir, "MPEGAV");
    _vcd_directory_mkdir (obj->dir, "SEGMENT");
    _vcd_directory_mkdir (obj->dir, "VCD");

    _vcd_directory_mkfile (obj->dir, "VCD/ENTRIES.VCD;1", obj->entries_sec, ISO_BLOCKSIZE, FALSE, 0);    
    _vcd_directory_mkfile (obj->dir, "VCD/INFO.VCD;1", obj->info_sec, ISO_BLOCKSIZE, FALSE, 0);

    /* only for vcd2.0 */
    _vcd_directory_mkfile (obj->dir, "VCD/LOT.VCD;1", obj->lot_secs, ISO_BLOCKSIZE*LOT_VCD_SIZE, FALSE, 0);
    _vcd_directory_mkfile (obj->dir, "VCD/PSD.VCD;1", obj->psd_sec, obj->psd_size, FALSE, 0);
    break;

  case VCD_TYPE_SVCD:
    _vcd_directory_mkdir (obj->dir, "EXT");
    _vcd_directory_mkdir (obj->dir, "MPEG2");
    _vcd_directory_mkdir (obj->dir, "SVCD");

    _vcd_directory_mkfile (obj->dir, "SVCD/ENTRIES.SVD;1", obj->entries_sec, ISO_BLOCKSIZE, FALSE, 0);    
    _vcd_directory_mkfile (obj->dir, "SVCD/INFO.SVD;1", obj->info_sec, ISO_BLOCKSIZE, FALSE, 0);
    _vcd_directory_mkfile (obj->dir, "SVCD/LOT.SVD;1", obj->lot_secs, ISO_BLOCKSIZE*LOT_VCD_SIZE, FALSE, 0);
    _vcd_directory_mkfile (obj->dir, "SVCD/PSD.SVD;1", obj->psd_sec, obj->psd_size, FALSE, 0);

    _vcd_directory_mkfile (obj->dir, "SVCD/SEARCH.DAT;1", obj->search_secs, sizeof (SearchDat), FALSE, 0);

    _vcd_directory_mkfile (obj->dir, "SVCD/TRACKS.SVD;1", obj->tracks_sec, ISO_BLOCKSIZE, FALSE, 0);

    break;

  default:
    vcd_error("VCD type not supported");
    break;
  }

  /* custom files */

  {
    struct cust_file *p = obj->custom_files;
    
    while (p) 
      {
        p->start_extent = _vcd_salloc(obj->iso_bitmap, SECTOR_NIL, p->sectors);
        
        assert(p->start_extent != SECTOR_NIL);

        _vcd_directory_mkfile(obj->dir, p->iso_pathname, p->start_extent,
                              p->size, p->raw_flag, 1);
        
        p = p->next;
      }
  }


  /* calculate iso size -- after this point no sector shall be
     allocated anymore */

  obj->iso_size = MAX (MIN_ISO_SIZE, _vcd_salloc_get_highest (obj->iso_bitmap));

  vcd_debug ("iso9660: highest alloced sector is %d (using %d as isosize)", 
             _vcd_salloc_get_highest (obj->iso_bitmap), obj->iso_size);

  /* after this point the ISO9660's size is frozen */

  for (n = 0; n < obj->mpeg_tracks_num; n++) {
    char avseq_pathname[128] = { 0, };
    const char *fmt = NULL;
    uint32_t extent = obj->mpeg_tracks[n].relative_start_extent;
      
    switch (obj->type) {
    case VCD_TYPE_VCD2:
      fmt = "MPEGAV/AVSEQ%2.2d.DAT;1";
      break;
    case VCD_TYPE_SVCD:
      fmt = "MPEG2/AVSEQ%2.2d.MPG;1";
      break;
    default:
      assert(1);
    }
      
    snprintf (avseq_pathname, sizeof (avseq_pathname), fmt, n+1);

    extent += obj->iso_size;

    _vcd_directory_mkfile (obj->dir, avseq_pathname, extent+PRE_DATA_GAP,
                           obj->mpeg_tracks[n].length_sectors*ISO_BLOCKSIZE,
                           TRUE, n+1);
  }

  obj->dirs_size = _vcd_directory_get_size (obj->dir);

  obj->ptl_sec = obj->dir_secs + obj->dirs_size;
  obj->ptm_sec = obj->ptl_sec + 1;

  if (obj->ptm_sec >= 75)
    vcd_error ("directory section to big");
}

static int
_callback_wrapper (VcdObj *obj, int force)
{
  const int cb_frequency = 75*4;

  if (obj->last_cb_call + cb_frequency > obj->sectors_written && !force)
    return 0;

  obj->last_cb_call = obj->sectors_written;

  if (obj->progress_callback) {
    progress_info_t _pi;

    _pi.sectors_written = obj->sectors_written;
    _pi.total_sectors = obj->relative_end_extent + obj->iso_size; 
    _pi.in_track = obj->in_track;
    _pi.total_tracks = obj->mpeg_tracks_num + 1;

    return obj->progress_callback (&_pi, obj->callback_user_data);
  }
  else
    return 0;
}

static int
_write_m2_image_sector (VcdObj *obj, const void *data, uint32_t extent,
                        uint8_t fnum, uint8_t cnum, uint8_t sm, uint8_t ci) 
{
  assert (extent == obj->sectors_written);

  write_mode2_sector (obj->bin_file, obj->bin_file_2336_flag,
                      data, extent, fnum, cnum, sm, ci);

  obj->sectors_written++;

  return _callback_wrapper (obj, FALSE);
}

static int
_write_m2_raw_image_sector (VcdObj *obj, const void *data, uint32_t extent)
{
  assert (extent == obj->sectors_written);

  write_mode2_raw_sector (obj->bin_file, obj->bin_file_2336_flag,
                          data, extent);
  obj->sectors_written++;

  return _callback_wrapper (obj, FALSE);
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
  uint32_t sectors, size;

  size = vcd_data_source_stat (source);
  sectors = size / M2F1_SIZE;
  if (size % M2F1_SIZE)
    sectors++;

  vcd_data_source_seek (source, 0); 

  for (n = 0;n < sectors;n++) {
    char buf[M2F1_SIZE] = { 0, };

    vcd_data_source_read (source, buf, M2F1_SIZE, 1);

    if (_write_m2_image_sector (obj, buf, extent+n, 1, 0, 
                                ((n+1 < sectors) 
                                 ? SM_DATA : SM_DATA
                                 |SM_EOF),
                                0))
      break;
  }

  vcd_data_source_close (source);
}

static int
_write_vcd_iso_track (VcdObj *obj)
{
  char *dir_buf = NULL;
  char
    ptm_buf[ISO_BLOCKSIZE], ptl_buf[ISO_BLOCKSIZE], 
    pvd_buf[ISO_BLOCKSIZE], evd_buf[ISO_BLOCKSIZE];
  int n;

  /* generate dir sectors */
  
  dir_buf = malloc (ISO_BLOCKSIZE*obj->dirs_size);

  _vcd_directory_dump_entries (obj->dir, dir_buf, 18);
  _vcd_directory_dump_pathtables (obj->dir, ptl_buf, ptm_buf);
      
  /* generate PVD and EVD at last... */
  set_iso_pvd (pvd_buf, obj->iso_volume_label, obj->iso_size, dir_buf,
               obj->ptl_sec, obj->ptm_sec, pathtable_get_size (ptm_buf));
    
  set_iso_evd (evd_buf);

  /* fill VCD relevant files with data */

  set_info_vcd (obj);
  set_entries_vcd (obj);
  set_lot_vcd (obj);
  set_psd_vcd (obj);

  if (obj->type == VCD_TYPE_SVCD) 
    {
      set_tracks_svd (obj);
      set_search_dat (obj);
    }

  /* start actually writing stuff */

  vcd_debug ("writing track 1 (ISO9660)...");

  /* 00:00:00 */

  for (n = 0;n < 16; n++)
    _write_m2_image_sector (obj, zero, n, 0, 0, SM_DATA, 0);

  _write_m2_image_sector (obj, pvd_buf, obj->pvd_sec, 0, 0, SM_DATA|SM_EOR, 0);
  _write_m2_image_sector (obj, evd_buf, obj->evd_sec, 0, 0, 
                          SM_DATA|SM_EOR|SM_EOF, 0);

  for (n = 0;n < obj->dirs_size;n++)
    _write_m2_image_sector (obj, dir_buf+ (ISO_BLOCKSIZE*n), 18+n, 0, 0,
                            n+1 != obj->dirs_size ? SM_DATA : SM_DATA|SM_EOR|SM_EOF,
                            0);

  _write_m2_image_sector (obj, ptl_buf, obj->ptl_sec, 0, 0,
                          SM_DATA|SM_EOR|SM_EOF, 0);
  _write_m2_image_sector (obj, ptm_buf, obj->ptm_sec, 0, 0,
                          SM_DATA|SM_EOR|SM_EOF, 0);

  for (n = obj->ptm_sec+1;n < 75;n++)
    _write_m2_image_sector (obj, zero, n, 0, 0, SM_DATA, 0);
  
  /* 00:01:00 */

  for (n = 75;n < 150; n++)
    _write_m2_image_sector (obj, zero, n, 0, 0, SM_DATA, 0);

  /* 00:02:00 */

  _write_m2_image_sector (obj, obj->info_vcd_buf, obj->info_sec,
                          0, 0, SM_DATA|SM_EOF, 0);
  
  _write_m2_image_sector (obj, obj->entries_vcd_buf, obj->entries_sec, 
                          0, 0, SM_DATA|SM_EOF, 0);

  for (n = 0;n < LOT_VCD_SIZE; n++)
    _write_m2_image_sector (obj, obj->lot_vcd_buf+ (ISO_BLOCKSIZE*n), 
                            obj->lot_secs+n, 0, 0, 
                            ((n+1 < LOT_VCD_SIZE) ? SM_DATA : SM_DATA|SM_EOF),
                            0);

  _write_m2_image_sector (obj, obj->psd_vcd_buf, obj->psd_sec,
                          0, 0, SM_DATA|SM_EOF, 0);

  if(obj->type == VCD_TYPE_SVCD) 
    {
      _write_m2_image_sector (obj, obj->tracks_svd_buf, obj->tracks_sec,
                              0, 0, SM_DATA|SM_EOF, 0);
      _write_m2_image_sector (obj, obj->search_dat_buf, obj->search_secs,
                              0, 0, SM_DATA|SM_EOF, 0);
    }
  else
    {
      _write_m2_image_sector (obj, zero, TRACKS_SVD_SECTOR, 0, 0, SM_DATA, 0);
      _write_m2_image_sector (obj, zero, SEARCH_DAT_SECTOR, 0, 0, SM_DATA, 0);
    }

  for (n = 185+2;n < 225; n++)
    _write_m2_image_sector (obj, zero, n, 0, 0, SM_DATA, 0);

  /* write custom files */
  {
    struct cust_file *p = obj->custom_files;
    
    while (p) 
      {
        vcd_debug ("Writing custom file as `%s' (size=%d)", 
                   p->iso_pathname, p->size);
        if (p->raw_flag)
          _write_source_mode2_raw (obj, p->file, p->start_extent);
        else
          _write_source_mode2_form1 (obj, p->file, p->start_extent);

        p = p->next;
      }
  }
  
  /* blank unalloced tracks */
  while ((n = _vcd_salloc (obj->iso_bitmap, SECTOR_NIL, 1)) < obj->iso_size)
    _write_m2_image_sector (obj, zero, n, 0, 0, SM_DATA, 0);

  free (dir_buf);

  return 0;
}

static int
_write_sectors (VcdObj *obj, int track)
{
  int n, lastsect = obj->sectors_written;
  const char zero[2352] = { 0, };
  char buf[2324];
  struct {
    int audio;
    int video;
    int null;
    int unknown;
  } mpeg_packets = {0, 0, 0, 0};

  {
    char *norm_str = NULL;
    mpeg_info_t *info = &(obj->mpeg_tracks[track].mpeg_info);

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

    vcd_debug ("writing track %d, %s, %s...", track+2,
               (info->version == MPEG_VERS_MPEG1 ? "MPEG1" : "MPEG2"),
               norm_str);

    free (norm_str);
  }

  for (n = 0; n < PRE_TRACK_GAP;n++)
    _write_m2_image_sector (obj, zero, lastsect++, 0, 0, SM_FORM2, 0);

  for (n = 0; n < PRE_DATA_GAP;n++)
    _write_m2_image_sector (obj, zero, lastsect++, track+1,
                            0, SM_FORM2|SM_REALT, 0);

  vcd_data_source_seek (obj->mpeg_tracks[track].source, 0);

  for (n = 0; n < obj->mpeg_tracks[track].length_sectors;n++) {
    int ci = 0, sm = 0;

    vcd_data_source_read (obj->mpeg_tracks[track].source, buf, 2324, 1);

    switch (vcd_mpeg_get_type (buf, NULL)) {
    case MPEG_TYPE_VIDEO:
      mpeg_packets.video++;
      sm = SM_FORM2|SM_REALT|SM_VIDEO;
      ci = CI_NTSC;
      break;
    case MPEG_TYPE_AUDIO:
      mpeg_packets.audio++;
      sm = SM_FORM2|SM_REALT|SM_AUDIO;
      ci = CI_AUD;
      break;
    case MPEG_TYPE_NULL:
      mpeg_packets.null++;
      sm = SM_FORM2|SM_REALT;
      ci = 0;
      break;
    case MPEG_TYPE_UNKNOWN:
      mpeg_packets.unknown++;
      sm = SM_FORM2|SM_REALT;
      ci = 0;
      break;
    case MPEG_TYPE_END:
      if (n < obj->mpeg_tracks[track].length_sectors)
        vcd_warn ("Program end marker seen at packet %d"
                  " -- before actual end of stream", n);
      sm = SM_FORM2|SM_REALT;
      ci = 0;
      break;
    case MPEG_TYPE_INVALID:
      vcd_error ("invalid mpeg packet found at packet# %d"
                 " -- please fix this mpeg file!", n);
      vcd_data_source_close (obj->mpeg_tracks[track].source);
      return 1;
      break;
    default:
      assert (0);
    }

    if (n == obj->mpeg_tracks[track].length_sectors-1)
      sm |= SM_EOR;

    if (_write_m2_image_sector (obj, buf, lastsect++, track+1, 1, sm, ci))
      break;
  }

  vcd_data_source_close (obj->mpeg_tracks[track].source);

  for (n = 0; n < POST_DATA_GAP;n++)
    _write_m2_image_sector (obj, zero, lastsect++, track+1,
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

  if (obj->mpeg_tracks_num > 0) {
    /* fixme -- make this efficient */
    size_sectors = vcd_obj_begin_output (obj);
    vcd_obj_end_output (obj);
  }

  return size_sectors;
}

long
vcd_obj_begin_output (VcdObj *obj)
{
  assert (obj != NULL);
  assert (obj->mpeg_tracks_num > 0);

  obj->in_track = 1;
  obj->sectors_written = 0;

  obj->iso_bitmap = _vcd_salloc_new ();

  obj->dir = _vcd_directory_new ();

  _finalize_vcd_iso_track (obj);

  return obj->relative_end_extent + obj->iso_size;
}

int
vcd_obj_write_image (VcdObj *obj, VcdDataSink *bin_file,
                     progress_callback_t callback, void *user_data)
{
  unsigned sectors, track;

  assert (obj != NULL);
  assert (sectors >= 0);
  assert (obj->sectors_written == 0);

  obj->progress_callback = callback;
  obj->callback_user_data = user_data;
  obj->bin_file = bin_file;
  
  if (_callback_wrapper (obj, TRUE))
    return 1;

  if (_write_vcd_iso_track (obj))
    return 1;

  for (track = 0;track < obj->mpeg_tracks_num;track++) {
    obj->in_track++;

    if (_callback_wrapper (obj, TRUE))
      return 1;

    if (_write_sectors (obj, track))
      return 1;
  }

  if (_callback_wrapper (obj, TRUE))
    return 1;
  
  return 0; /* ok */
}

void
vcd_obj_end_output (VcdObj *obj)
{
  assert (obj != NULL);

  _vcd_directory_destroy (obj->dir);
  _vcd_salloc_destroy (obj->iso_bitmap);

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
