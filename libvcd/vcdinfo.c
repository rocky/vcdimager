/*
    $Id$

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
#include <stddef.h>
#include <errno.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_types.h>
#include <libvcd/vcd_files.h>
#include <libvcd/vcd_files_private.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_iso9660_private.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_salloc.h>
#include <libvcd/vcd_stream_stdio.h>
#include <libvcd/vcd_image_bincue.h>
#include <libvcd/vcd_image_cd.h>
#include <libvcd/vcd_image_fs.h>
#include <libvcd/vcd_xa.h>
#include <libvcd/vcd_cd_sector.h>

/* Eventually move above libvcd includes but having vcdinfo including. */
#include "vcdinfo.h"

static const char _rcsid[] = "$Id$";

#define MIN_ENCODED_TRACK_NUM 100
#define MIN_ENCODED_SEGMENT_NUM 1000
#define MAX_ENCODED_SEGMENT_NUM 2979

#define BUF_COUNT 16
#define BUF_SIZE 80

/* Pick out the segment LSN's and sizes the hard way: by looking
   in the filesystem for /SEGMENT and pulling out filesystem info.
*/
static void
_init_fs_segment (const vcdinfo_obj_t *obj, const char pathname[],
                  bool look_for_segment)
{
  VcdList *entlist = vcd_image_source_fs_readdir (obj->img, pathname);
  VcdList *dirlist =  _vcd_list_new ();
  VcdListNode *entnode;
  bool found_segment = false;
    
  vcd_assert (entlist != NULL);

  /* just iterate */
  
  _VCD_LIST_FOREACH (entnode, entlist)
    {
      char *_name = _vcd_list_node_data (entnode);
      char _fullname[4096] = { 0, };
      vcd_image_stat_t statbuf;

      found_segment = strcmp(_name, "SEGMENT") == 0;
      
      if (!found_segment && look_for_segment) continue;

      snprintf (_fullname, sizeof (_fullname), "%s%s", pathname, _name);
  
      if (vcd_image_source_fs_stat (obj->img, _fullname, &statbuf))
        vcd_assert_not_reached ();

      strncat (_fullname, "/", sizeof (_fullname));

      if (statbuf.type == _STAT_DIR
          && strcmp (_name, ".") 
          && strcmp (_name, ".."))
        _vcd_list_append (dirlist, strdup (_fullname));

      else if (!(look_for_segment ||
                 strcmp (_name, ".") == 0 || strcmp (_name, "..") == 0)) {

        vcd_image_stat_t *segment = _vcd_malloc (sizeof (vcd_image_stat_t));

        *segment = statbuf;
        // memcpy(segment, &statbuf, sizeof(vcd_image_stat_t));

        _vcd_list_append (obj->segment_list, segment);
        
        vcd_debug ("%c %s %d %d [fn %.2d] [lsn %6d] %9d  %s",
                   (statbuf.type == _STAT_DIR) ? 'd' : '-',
                   vcdinfo_get_xa_attr_str (statbuf.xa.attributes),
                   uint16_from_be (statbuf.xa.user_id),
                   uint16_from_be (statbuf.xa.group_id),
                   statbuf.xa.filenum,
                   statbuf.lsn,
                   statbuf.size,
                   _name);
        
      } else if (found_segment) break;

    }

  _vcd_list_free (entlist, true);

  /* now recurse */

  _VCD_LIST_FOREACH (entnode, dirlist)
    {
      char *_fullname = _vcd_list_node_data (entnode);

      _init_fs_segment (obj, _fullname, false);
    }

  _vcd_list_free (dirlist, true);
}

/*!
  Note first seg_num is 0!
*/
const char * 
vcdinfo_audio_type2str(const vcdinfo_obj_t *obj, unsigned int seg_num)
{
  const InfoVcd *info = &obj->info;
  const char *audio_types[2][4] =
    {
      { "no stream", "1 stream", "2 streams",
        "1 multi-channel stream (surround sound)" },
      { "no audio", "single channel", "stereo", "dual channel" }
    };
  
  return audio_types[obj->vcd_type == VCD_TYPE_VCD2 ? 1 : 0]
    [info->spi_contents[seg_num].audio_type];

}

/*!
  Note first seg_num is 0!
*/
const char * 
vcdinfo_ogt2str(const vcdinfo_obj_t *obj, unsigned int seg_num)
{
  const InfoVcd *info = &obj->info;
  const char *ogt_str[] =
    {
      "None",
      "1 available",
      "0 & 1 available",
      "all 4 available"
    };
  
  return ogt_str[info->spi_contents[seg_num].ogt];
}


const char * 
vcdinfo_video_type2str(const vcdinfo_obj_t *obj, unsigned int seg_num)
{
  const char *video_types[] =
    {
      "no stream",
      "NTSC still",
      "NTSC still (lo+hires)",
      "NTSC motion",
      "reserved (0x4)",
      "PAL still",
      "PAL still (lo+hires)",
      "PAL motion"
      "INVALID ENTRY"
    };
  
  return video_types[vcdinfo_get_video_type(obj, seg_num)];
}


/* Return a pointer to a internal free buffer */
static char *
_getbuf (void)
{
  static char _buf[BUF_COUNT][BUF_SIZE];
  static int _num = -1;
  
  _num++;
  _num %= BUF_COUNT;

  memset (_buf[_num], 0, BUF_SIZE);

  return _buf[_num];
}

/* Comparison routine used in sorting. */
static int
_offset_t_cmp (vcdinfo_offset_t *a, vcdinfo_offset_t *b)
{
  if (a->offset > b->offset)
    return 1;

  if (a->offset < b->offset)
    return -1;
  
  return 0;
}

static void
_visit_pbc (vcdinfo_obj_t *obj, unsigned lid, unsigned int offset, 
                   bool in_lot, bool ext)
{
  VcdListNode *node;
  vcdinfo_offset_t *ofs;
  unsigned psd_size = ext ? obj->psd_x_size : vcdinfo_get_psd_size (obj);
  const uint8_t *psd = ext ? obj->psd_x : obj->psd;
  unsigned _rofs = offset * obj->info.offset_mult;
  VcdList *offset_list;

  vcd_assert (psd_size % 8 == 0);

  if (offset == PSD_OFS_DISABLED
      || offset == PSD_OFS_MULTI_DEF 
      || offset == PSD_OFS_MULTI_DEF_NO_NUM)
    return;

  if (_rofs >= psd_size)
    {
      vcd_warn ("psd offset out of range (%d >= %d)", _rofs, psd_size);
      return;
    }

  vcd_assert (_rofs < psd_size);

  if (!obj->offset_list)
    obj->offset_list = _vcd_list_new ();

  if (!obj->offset_x_list)
    obj->offset_x_list = _vcd_list_new ();

  offset_list = ext ? obj->offset_x_list : obj->offset_list;

  _VCD_LIST_FOREACH (node, offset_list)
    {
      ofs = _vcd_list_node_data (node);

      if (offset == ofs->offset)
        {
          if (in_lot)
            ofs->in_lot = true;

          return; /* already been there... */
        }
    }

  ofs = _vcd_malloc (sizeof (vcdinfo_offset_t));

  ofs->offset = offset;
  ofs->lid = lid;
  ofs->in_lot = in_lot;

  switch (psd[_rofs])
    {
    case PSD_TYPE_PLAY_LIST:
      _vcd_list_append (offset_list, ofs);
      {
        const PsdPlayListDescriptor *d = (const void *) (psd + _rofs);
        const uint16_t lid = vcdinfo_get_lid_from_pld(d);

        if (!ofs->lid)
          ofs->lid = lid;
        else 
          if (ofs->lid != lid)
            vcd_warn ("LOT entry assigned LID %d, but descriptor has LID %d",
                      ofs->lid, lid);

        _visit_pbc (obj, 0, uint16_from_be (d->prev_ofs), false, ext);
        _visit_pbc (obj, 0, uint16_from_be (d->next_ofs), false, ext);
        _visit_pbc (obj, 0, uint16_from_be (d->return_ofs), false, ext);
      }
      break;

    case PSD_TYPE_EXT_SELECTION_LIST:
    case PSD_TYPE_SELECTION_LIST:
      _vcd_list_append (offset_list, ofs);
      {
        const PsdSelectionListDescriptor *d =
          (const void *) (psd + _rofs);

        int idx;

        if (!ofs->lid)
          ofs->lid = uint16_from_be (d->lid) & 0x7fff;
        else 
          if (ofs->lid != (uint16_from_be (d->lid) & 0x7fff))
            vcd_warn ("LOT entry assigned LID %d, but descriptor has LID %d",
                      ofs->lid, uint16_from_be (d->lid) & 0x7fff);

        _visit_pbc (obj, 0, uint16_from_be (d->prev_ofs), false, ext);
        _visit_pbc (obj, 0, uint16_from_be (d->next_ofs), false, ext);
        _visit_pbc (obj, 0, uint16_from_be (d->return_ofs), false, ext);
        _visit_pbc (obj, 0, uint16_from_be (d->default_ofs), false, ext);
        _visit_pbc (obj, 0, uint16_from_be (d->timeout_ofs), false, ext);

        for (idx = 0; idx < d->nos; idx++)
          _visit_pbc (obj, 0, uint16_from_be (d->ofs[idx]), false, ext);
        
      }
      break;

    case PSD_TYPE_END_LIST:
      _vcd_list_append (offset_list, ofs);
      break;

    default:
      vcd_warn ("corrupt PSD???????");
      free (ofs);
      return;
      break;
    }
}

/*!
  \brief Classify itemid_num into the kind of item it is: track #, entry #, 
  segment #. 
  \param itemid is set to contain this classification an the converted 
  entry number. 
*/
void
vcdinfo_classify_itemid (uint16_t itemid_num, vcdinfo_itemid_t *itemid)
{

  itemid->num = itemid_num;
  if (itemid_num < 2) 
    itemid->type = VCDINFO_ITEM_TYPE_NOTFOUND;
  else if (itemid_num < MIN_ENCODED_TRACK_NUM) {
    itemid->type = VCDINFO_ITEM_TYPE_TRACK;
    itemid->num--;
  } else if (itemid_num < 600) {
    itemid->type = VCDINFO_ITEM_TYPE_ENTRY;
    itemid->num -= MIN_ENCODED_TRACK_NUM;
  } else if (itemid_num < MIN_ENCODED_SEGMENT_NUM)
    itemid->type = VCDINFO_ITEM_TYPE_LID;
  else if (itemid_num <= MAX_ENCODED_SEGMENT_NUM) {
    itemid->type = VCDINFO_ITEM_TYPE_SEGMENT;
    itemid->num -= (MIN_ENCODED_SEGMENT_NUM-1);
  } else 
    itemid->type = VCDINFO_ITEM_TYPE_SPAREID2;
}

const char *
vcdinfo_pin2str (uint16_t itemid_num)
{
  char *buf = _getbuf ();
  vcdinfo_itemid_t itemid;
  
  vcdinfo_classify_itemid(itemid_num, &itemid);
  strcpy (buf, "??");

  switch(itemid.type) {
  case VCDINFO_ITEM_TYPE_NOTFOUND:
    snprintf (buf, BUF_SIZE, "play nothing (0x%4.4x)", itemid.num);
    break;
  case VCDINFO_ITEM_TYPE_TRACK:
    snprintf (buf, BUF_SIZE, "SEQUENCE[%d] (0x%4.4x)", itemid.num, 
              itemid_num);
    break;
  case VCDINFO_ITEM_TYPE_ENTRY:
    snprintf (buf, BUF_SIZE, "ENTRY[%d] (0x%4.4x)", itemid.num, itemid_num);
    break;
  case VCDINFO_ITEM_TYPE_SEGMENT:
    snprintf (buf, BUF_SIZE, "SEGMENT[%d] (0x%4.4x)", itemid.num, itemid_num);
    break;
  case VCDINFO_ITEM_TYPE_LID:
    snprintf (buf, BUF_SIZE, "spare id (0x%4.4x)", itemid.num);
    break;
  case VCDINFO_ITEM_TYPE_SPAREID2:
    snprintf (buf, BUF_SIZE, "spare id2 (0x%4.4x)", itemid.num);
    break;
  }

  return buf;
}

/*!
   Return a string containing the VCD album description, or NULL if there is 
   some problem in getting this. 
*/
const char *
vcdinfo_get_album_description(vcdinfo_obj_t *obj)
{
  InfoVcd *info;
  if ( NULL == obj || NULL == &obj->info ) return (NULL);
  info = &obj->info;
  return(info->album_desc);
}

/*!
  Get autowait time value for PsdPlayListDescriptor *d.
  Time is in seconds unless it is -1 (unlimited).
*/
int
vcdinfo_get_autowait_time (const PsdPlayListDescriptor *d) 
{
  return vcdinfo_calc_psd_wait_time (d->atime);
}

/*!
  Return a string containing the default VCD device if none is specified.
  Return "" if we can't get this information.
*/
char *
vcdinfo_get_default_device (const vcdinfo_obj_t *obj)
{
  if (obj != NULL && obj->img != NULL && 
      obj->img->op.get_default_device != NULL) 
    return obj->img->op.get_default_device();
  else {
    /* Assume we mean a CD device rather than a file image.
       So create a new one just so we can find out what the
       default device name is for the CD. Then get rid of 
       the device name.
     */
    VcdImageSource *img = vcd_image_source_new_cd ();
    char *source_name=vcd_image_source_get_default_device(img);
    vcd_image_source_destroy (img);
    return(source_name);
  }
}

/*!
  Return number of sector units in of an entry. 0 is returned if entry_num
  is out of range.
  The first entry number is 0.
*/
uint32_t
vcdinfo_get_entry_size (const vcdinfo_obj_t *obj, unsigned int entry_num)
{
  const EntriesVcd *entries = &obj->entries;
  const unsigned int entry_count = uint16_from_be (entries->entry_count);
  if (entry_num > entry_count) 
    return 0;
  else {
    const lba_t this_lba = vcdinfo_get_entry_lba(obj, entry_num);
    lba_t next_lba;
    if (entry_num < entry_count-1) {
       next_lba = vcdinfo_get_entry_lba(obj, entry_num+1);
    } else {
      /* entry_num == entry_count -1. Or the last entry.
         This is really really ugly. There's probably a better
         way to do it. 
         Below we get the track of the current entry and then the LBA of the
         beginning of the following (leadout?) track.
      */
      unsigned int track = vcdinfo_get_track(obj, entry_num);
      if (track != VCDINFO_INVALID_TRACK) {
        next_lba = vcdinfo_get_track_lba(obj, track+1);
        if (next_lba == VCDINFO_NULL_LBA)
          return 0;
      } else {
        /* Something went wrong. Set up size to zero. */
        return 0;
      }
    }
    return (next_lba - this_lba);
  }
}

/*!  Return the starting MSF (minutes/secs/frames) for sequence
  entry_num in obj.  NULL is returned if there is no entry.
  The first entry number is 0.
*/
const msf_t *
vcdinfo_get_entry_msf(const vcdinfo_obj_t *obj, const unsigned int entry_num)
{
  const EntriesVcd *entries = &obj->entries;
  const unsigned int entry_count = uint16_from_be (entries->entry_count);
  return entry_num < entry_count ?
    &(entries->entry[entry_num].msf)
    : NULL;
}

/*!  Return the starting LBA (logical block address) for sequence
  entry_num in obj.  VCDINFO_NULL_LBA is returned if there is no entry.
*/
lba_t
vcdinfo_get_entry_lba(const vcdinfo_obj_t *obj, const unsigned int entry_num)
{
  const msf_t *msf = vcdinfo_get_entry_msf(obj, entry_num);
  return (msf != NULL) ? msf_to_lba(msf) : VCDINFO_NULL_LBA;
}

/*!
   Get the VCD format (VCD 1.0 VCD 1.1, SVCD, ... for this object.
   The type is also set inside obj.
*/
const vcd_type_t
vcdinfo_get_format_version (vcdinfo_obj_t *obj)
{
  obj->vcd_type = vcd_files_info_detect_type (&obj->info);
  return obj->vcd_type;
}

/*!
   Return a string giving VCD format (VCD 1.0 VCD 1.1, SVCD, ... 
   for this object.
*/
const char *
vcdinfo_get_format_version_str (const vcdinfo_obj_t *obj) 
{
  switch (obj->vcd_type)
    {
    case VCD_TYPE_VCD:
      return ("VCD 1.0");
      break;
    case VCD_TYPE_VCD11:
      return ("VCD 1.1");
      break;
    case VCD_TYPE_VCD2:
      return ("VCD 2.0");
      break;
    case VCD_TYPE_SVCD:
      return ("SVCD");
      break;
    case VCD_TYPE_HQVCD:
      return ("HQVCD");
      break;
    case VCD_TYPE_INVALID:
      return ("INVALID");
      break;
    default:
      return ( "????");
    }
}

/*!
  FIXME: THIS NEEDS TO BE RETHOUGHT! 

  Get the itemid for a given list ID. 
  VCDINFO_REJECTED_MASK is returned on error or if obj is NULL. 
*/
uint16_t
vcdinfo_get_itemid_from_lid(const vcdinfo_obj_t *obj, uint16_t lid, bool ext)
{
  PsdListDescriptor pxd;

  if (obj == NULL) return VCDINFO_REJECTED_MASK;
  vcdinfo_get_pxd_from_lid(obj, &pxd, lid, ext);
  switch (pxd.descriptor_type) {
  case PSD_TYPE_SELECTION_LIST:
  case PSD_TYPE_EXT_SELECTION_LIST:
    if (pxd.psd == NULL) return VCDINFO_REJECTED_MASK;
    return vcdinfo_get_itemid_from_psd(pxd.psd);
    break;
  case PSD_TYPE_PLAY_LIST:
    /* FIXME: There is an array of items */
  case PSD_TYPE_END_LIST:
  case PSD_TYPE_COMMAND_LIST:
    return VCDINFO_REJECTED_MASK;
  }

  return VCDINFO_REJECTED_MASK;

}

/*!
  Get the item id for a given PSD selector descriptor. 
  VCDINFO_REJECTED_MASK is returned on error or if d is NULL. 
*/
uint16_t
vcdinfo_get_itemid_from_psd(const PsdSelectionListDescriptor *psd)
{
  return (psd != NULL) ? uint16_from_be(psd->itemid) : VCDINFO_REJECTED_MASK;
}

/* Get the LID from a given PSD selector descriptor. 
   VCDINFO_REJECTED_MASK is returned d on error or d is NULL. 
*/
uint16_t
vcdinfo_get_lid_from_pld(const PsdPlayListDescriptor *d)
{
  return (d != NULL) 
    ? uint16_from_be (d->lid) & VCDINFO_LID_MASK
    : VCDINFO_REJECTED_MASK;
}

/*!
  Get the LID from a given PSD play-list descriptor. 
  VCDINFO_REJECTED_MASK is returned d on error or d is NULL. 
*/
uint16_t
vcdinfo_get_lid_from_psd(const PsdSelectionListDescriptor *d)
{
  return (d != NULL) 
    ? uint16_from_be (d->lid) & VCDINFO_LID_MASK
    : VCDINFO_REJECTED_MASK;
}

/*!
  Get the LID rejected status for a given PSD selector descriptor. 
  true is also returned d is NULL. 
*/
bool
vcdinfo_get_lid_rejected_from_psd(const PsdSelectionListDescriptor *d)
{
  return (d != NULL) 
    ? vcdinfo_is_rejected(uint16_from_be(d->lid)) 
    : true;
}

/*!
  Return highest LID value. 
*/
uint16_t
vcdinfo_get_max_lid (const vcdinfo_obj_t *obj) 
{
  // Should probably use _vcd_pbc_max_lid instead? 
  return uint16_from_be (obj->info.lot_entries);
}

/**
 * \fn vcdinfo_get_next_from_pld(const PsdPlayListDescriptor *d);
 * \brief Get next offset for a given PSD selector descriptor. 
 * \return VCDINFO_INVALID_OFFSET is returned if d on error or d is
 * NULL. Otherwise the LID offset is returned.
 */
uint16_t
vcdinfo_get_next_from_pld(const PsdPlayListDescriptor *pld)
{
  return (pld != NULL) 
    ? uint16_from_be (pld->next_ofs) : VCDINFO_INVALID_OFFSET;
}

/**
 * \fn vcdinfo_get_next_from_psd(const PsdSelectionListDescriptor *d);
 * \brief Get next offset for a given PSD selector descriptor. 
 * \return VCDINFO_INVALID_OFFSET is returned if d on error or d is
 * NULL. Otherwise the LID offset is returned.
 */
uint16_t
vcdinfo_get_next_from_psd(const PsdSelectionListDescriptor *psd)
{
  return (psd != NULL) 
    ? uint16_from_be (psd->next_ofs) : VCDINFO_INVALID_OFFSET;
}

/*!
  Return the number of entries in the VCD.
*/
unsigned int
vcdinfo_get_num_entries(const vcdinfo_obj_t *obj)
{
  const EntriesVcd *entries = &obj->entries;
  return (uint16_from_be (entries->entry_count));
}

/*!
  Return the number of segments in the VCD. 
*/
unsigned int
vcdinfo_get_num_segments(const vcdinfo_obj_t *obj)
{
  const InfoVcd *info = &obj->info;
  return (uint16_from_be (info->item_count));
}

/*!
  FIXME: THIS NEEDS TO BE RETHOUGHT! 

  \fn vcdinfo_get_offset_from_lid(const vcdinfo_obj_t *obj, unsigned int entry_num);
  \brief Get offset entry_num for a given LID. 
  \return VCDINFO_INVALID_OFFSET is returned if obj on error or obj
  is NULL. Otherwise the LID offset is returned.
*/
uint16_t
vcdinfo_get_offset_from_lid(const vcdinfo_obj_t *obj, uint16_t lid,
			    unsigned int entry_num, bool ext) 
{
  PsdListDescriptor pxd;

  if (obj == NULL) return VCDINFO_INVALID_OFFSET;
  vcdinfo_get_pxd_from_lid(obj, &pxd, lid, ext);

  switch (pxd.descriptor_type) {
  case PSD_TYPE_SELECTION_LIST:
  case PSD_TYPE_EXT_SELECTION_LIST:
    if (pxd.psd == NULL) return VCDINFO_INVALID_OFFSET;
    return vcdinfo_get_offset_from_psd(pxd.psd, entry_num-1);
    break;
  case PSD_TYPE_PLAY_LIST:
    /* FIXME: There is an array of items */
  case PSD_TYPE_END_LIST:
  case PSD_TYPE_COMMAND_LIST:
    return VCDINFO_INVALID_OFFSET;
  }
  return VCDINFO_INVALID_OFFSET;

}

/**
 * \fn vcdinfo_get_offset_from_psd(const PsdSelectionListDescriptor *d, unsigned int entry_num);
 * \brief Get offset entry_num for a given PSD selector descriptor. 
 * \return VCDINFO_INVALID_OFFSET is returned if d on error or d is
 * NULL. Otherwise the LID offset is returned.
 */
uint16_t
vcdinfo_get_offset_from_psd(const PsdSelectionListDescriptor *d, 
			    unsigned int entry_num) 
{
  return (d != NULL || entry_num < d->nos) 
    ? uint16_from_be (d->ofs[entry_num]) : VCDINFO_INVALID_OFFSET;
}

vcdinfo_offset_t *
vcdinfo_get_offset_t (const vcdinfo_obj_t *obj, unsigned int offset, bool ext)
{
  VcdListNode *node;
  VcdList *offset_list = ext ? obj->offset_x_list : obj->offset_list;

  switch (offset) {
  case PSD_OFS_DISABLED:
  case PSD_OFS_MULTI_DEF:
  case PSD_OFS_MULTI_DEF_NO_NUM:
    return NULL;
  default: ;
  }
  
  _VCD_LIST_FOREACH (node, offset_list)
    {
      vcdinfo_offset_t *ofs = _vcd_list_node_data (node);
      if (offset == ofs->offset)
        return ofs;
    }
  return NULL;
}

/*!
  Get autowait time value for PsdPlayListDescriptor *d.
  Time is in seconds.
*/
uint16_t
vcdinfo_get_play_time (const PsdPlayListDescriptor *d) 
{
  return uint16_from_be (d->ptime);
}

/*!
  Get the PSD Selection List Descriptor for a given lid.
  NULL is returned if error or not found.
*/
void
vcdinfo_get_pxd_from_lid(const vcdinfo_obj_t *obj, PsdListDescriptor *pxd,
                         uint16_t lid, bool ext) 
{
  VcdListNode *node;
  unsigned mult = obj->info.offset_mult;
  const uint8_t *psd = ext ? obj->psd_x : obj->psd;
  VcdList *offset_list = ext ? obj->offset_x_list : obj->offset_list;

  if (offset_list == NULL) return;
  
  _VCD_LIST_FOREACH (node, offset_list)
    {
      vcdinfo_offset_t *ofs = _vcd_list_node_data (node);
      unsigned _rofs = ofs->offset * mult;

      pxd->descriptor_type = psd[_rofs];

      switch (pxd->descriptor_type)
        {
        case PSD_TYPE_PLAY_LIST:
          {
            pxd->pld = (PsdPlayListDescriptor *) (psd + _rofs);
            if (vcdinfo_get_lid_from_pld(pxd->pld) == lid) {
              return;
            }
            break;
          }

        case PSD_TYPE_EXT_SELECTION_LIST:
        case PSD_TYPE_SELECTION_LIST: 
          {
            pxd->psd = (PsdSelectionListDescriptor *) (psd + _rofs);
            if (vcdinfo_get_lid_from_psd(pxd->psd) == lid) {
              return;
            }
            break;
          }
        default: ;
        }
    }
  return;
}

/**
 * \fn vcdinfo_get_prev_from_pld(const PsdPlayListDescriptor *d);
 * \brief Get prev offset for a given PSD selector descriptor. 
 * \return VCDINFO_INVALID_OFFSET is returned if d on error or d is
 * NULL. Otherwise the LID offset is returned.
 */
uint16_t
vcdinfo_get_prev_from_pld(const PsdPlayListDescriptor *pld)
{
  return (pld != NULL) ? 
    uint16_from_be (pld->prev_ofs) : VCDINFO_INVALID_OFFSET;
}

/**
 * \fn vcdinfo_get_prev_from_psd(const PsdSelectionListDescriptor *psd);
 * \brief Get prev offset for a given PSD selector descriptor. 
 * \return VCDINFO_INVALID_OFFSET is returned if d on error or psd is
 * NULL. Otherwise the LID offset is returned.
 */
uint16_t
vcdinfo_get_prev_from_psd(const PsdSelectionListDescriptor *psd)
{
  return (psd != NULL) ? 
    uint16_from_be (psd->prev_ofs) : VCDINFO_INVALID_OFFSET;
}

/*!
  Return number of bytes in PSD. 
*/
uint32_t
vcdinfo_get_psd_size (const vcdinfo_obj_t *obj)
{
  return uint32_from_be (obj->info.psd_size);
}

/**
 * \fn vcdinfo_get_return_from_psd(const PsdPlayListDescriptor *pld);
 * \brief Get return offset for a given PLD selector descriptor. 
 * \return VCDINFO_INVALID_OFFSET is returned if pld on error or pld is
 * NULL. Otherwise the LID offset is returned.
 */
uint16_t
vcdinfo_get_return_from_pld(const PsdPlayListDescriptor *pld)
{
  return (pld != NULL) ? 
    uint16_from_be (pld->return_ofs) : VCDINFO_INVALID_OFFSET;
}

/**
 * \fn vcdinfo_get_return_from_psd(const PsdSelectionListDescriptor *d);
 * \brief Get return offset for a given PSD selector descriptor. 
 * \return VCDINFO_INVALID_OFFSET is returned if d on error or d is
 * NULL. Otherwise the LID offset is returned.
 */
uint16_t
vcdinfo_get_return_from_psd(const PsdSelectionListDescriptor *psd)
{
  return (psd != NULL) ? 
    uint16_from_be (psd->return_ofs) : VCDINFO_INVALID_OFFSET;
}

/*!  Return the starting LBA (logical block address) for segment
  entry_num in obj.  VCDINFO_LBA_NULL is returned if there is no entry.

  Note first seg_num is 0.
*/
lba_t
vcdinfo_get_seg_lba(const vcdinfo_obj_t *obj, const unsigned int seg_num)
{ 
  if (obj == NULL) return VCDINFO_NULL_LBA;
  {
    vcd_image_stat_t *statbuf = 
      _vcd_list_node_data (_vcd_list_at (obj->segment_list, seg_num));
    if (statbuf == NULL) return VCDINFO_NULL_LBA;
    return vcdinfo_lsn2lba(statbuf->lsn);
  }
}

/*!  Return the starting MSF (minutes/secs/frames) for segment
  entry_num in obj.  NULL is returned if there is no entry.

  Note first seg_num is 0!
*/
const msf_t *
vcdinfo_get_seg_msf(const vcdinfo_obj_t *obj, const unsigned int seg_num)
{ 
  lba_t lba = vcdinfo_get_seg_lba(obj, seg_num);
  if (lba == VCDINFO_NULL_LBA) return NULL;
  {
    static msf_t msf;
    lba_to_msf(lba, &msf);
    return &msf;
  }
}

/*!  Return the size in bytes for segment for entry_num n obj ASSUMING
  THE SIZE OF A SECTOR IS ISO_BLOCKSIZE. This could be the case (but
  doesn't have to be) if you mount a CD, have the OS list sizes and
  its blocksize is ISO_BLOCKSIZE.

  In contrast, if you are issuing mode2/format2 reads which give 2324
  bytes per sector and want to know how many of these to read to get
  the entire segment, this is probably not the way to do do
  that. Instead, multiply vcdinfo_get_seg_sector_count() by the number
  of bytes per sector.

  NULL is returned if there is no entry.
*/
uint32_t
vcdinfo_get_seg_size(const vcdinfo_obj_t *obj, const unsigned int seg_num)
{ 
  if (obj == NULL) return 0;
  {
    vcd_image_stat_t *statbuf = 
      _vcd_list_node_data (_vcd_list_at (obj->segment_list, seg_num));

    if (statbuf == NULL) return 0;
    return statbuf->size;
  }
}

/*!  
  Return the number of sectors for segment
  entry_num in obj.  0 is returned if there is no entry.

  Use this routine to figure out the actual number of bytes a physical
  region of a disk or CD takes up for a segment.
*/
uint32_t
vcdinfo_get_seg_sector_count(const vcdinfo_obj_t *obj, 
                            const unsigned int seg_num)
{ 
  if (obj == NULL) return 0;
  {
    vcd_image_stat_t *statbuf = 
      _vcd_list_node_data (_vcd_list_at (obj->segment_list, seg_num));

    if (statbuf == NULL) return 0;
    return statbuf->secsize;
  }
}

/*!
  Return the track number for entry n in obj. The first track starts
  at 1. Note this is one less than the track number reported in vcddump.
  (We don't count the header track?)
*/
unsigned int
vcdinfo_get_track(const vcdinfo_obj_t *obj, const unsigned int entry_num)
{
  const EntriesVcd *entries = &obj->entries;
  const unsigned int entry_count = uint16_from_be (entries->entry_count);
  /* Note entry_num is 0 origin. */
  return entry_num < entry_count ?
    from_bcd8 (entries->entry[entry_num].n)-1:
    VCDINFO_INVALID_TRACK;
}

/*!
  Return the number of tracks in the current medium.
*/
unsigned int
vcdinfo_get_track_count(const vcdinfo_obj_t *obj)
{
  if (obj != NULL && obj->img != NULL && 
      obj->img->op.get_track_count != NULL) 
    return obj->img->op.get_track_count(obj->img->user_data);
  return 0;
}


/*!  
  Return the starting LBA (logical block address) for track number
  track_num in obj.  Tracks numbers start at 1.
  The "leadout" track is specified either by
  using track_num LEADOUT_TRACK or the total tracks+1.
  VCDINFO_NULL_LBA is returned on failure.
*/
lba_t 
vcdinfo_get_track_lba(const vcdinfo_obj_t *obj, unsigned int track_num)
{
  if (NULL == obj || NULL == obj->img)
    return VCDINFO_NULL_LBA;

  if (obj->img->op.get_track_lba != NULL) {
    return obj->img->op.get_track_lba(obj->img->user_data, track_num);
  } else if (obj->img->op.get_track_msf != NULL) {
    msf_t msf;
    int rc=obj->img->op.get_track_msf(obj->img->user_data, track_num, &msf);
    return (rc==0) ? msf_to_lba(&msf) : VCDINFO_NULL_LBA;
  }
  return VCDINFO_NULL_LBA;
}

/*!  
  Return the starting MSF (minutes/secs/frames) for track number
  track_num in obj.  Tracks numbers start at 1.
  The "leadout" track is specified either by
  using track_num LEADOUT_TRACK or the total tracks+1.
  O is returned on success; 1 is returned on error.
*/
int
vcdinfo_get_track_msf(const vcdinfo_obj_t *obj, unsigned int track_num,
                      uint8_t *min, uint8_t *sec, uint8_t *frame)
{
  if (NULL == obj || NULL == obj->img)
    return 1;

  if (obj->img->op.get_track_msf != NULL) {
    msf_t msf;
    int rc=obj->img->op.get_track_msf(obj->img->user_data, track_num, &msf);
    if (rc==0) {
      *min   = from_bcd8(msf.m);
      *sec   = from_bcd8(msf.s);
      *frame = from_bcd8(msf.f);
    } else 
      return 1;
  } else if (obj->img->op.get_track_lba != NULL) {
    lba_t lba = obj->img->op.get_track_lba(obj->img->user_data, track_num);
    if (lba != VCDINFO_NULL_LBA) {
      vcdinfo_lba2msf(lba, min, sec, frame);
      return 0;
    } else {
      return 1;
    }
  }
  return 1;
}

/*!
  Return the size in bytes for track n. The first track is 1.
*/
unsigned int
vcdinfo_get_track_size(const vcdinfo_obj_t *obj, const unsigned int track_num)
{
  if (obj != NULL && obj->img != NULL && 
      obj->img->op.get_track_size != NULL) 
    return obj->img->op.get_track_size(obj->img->user_data, track_num);
  return 0;
}

/*!
  \brief Get the kind of video stream segment of segment seg_num in obj.
  \return VCDINFO_FILES_VIDEO_INVALD is returned if  on error or obj is
  null. Otherwise the enumeration type.

  Note first seg_num is 0!
*/
vcdinfo_video_segment_type_t
vcdinfo_get_video_type(const vcdinfo_obj_t *obj, unsigned int seg_num)
{
  const InfoVcd *info;
  if (obj == NULL)  return VCDINFO_FILES_VIDEO_INVALID;
  info = &obj->info;
  if (info == NULL) return VCDINFO_FILES_VIDEO_INVALID;
  return info->spi_contents[seg_num].video_type;
}

/*!
  Return the VCD volume count - the number of CD's in the collection.
  O is returned if there is some problem in getting this. 
*/
unsigned int
vcdinfo_get_volume_count(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj || NULL == &obj->info ) return 0;
  return(uint16_from_be( obj->info.vol_count));
}

/*!
   Return a string containing the VCD volume id, or NULL if there is 
   some problem in getting this. 
*/
const char *
vcdinfo_get_volume_id(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj || NULL == &obj->pvd ) return (NULL);
  return(vcdinfo_strip_trail(obj->pvd.volume_id, 32));
}

/*!
  Return the VCD volume num - the number of the CD in the collection.
  This is a number between 1 and the volume count.
  O is returned if there is some problem in getting this. 
*/
unsigned int
vcdinfo_get_volume_num(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj || NULL == &obj->info ) return 0;
  return(uint16_from_be( obj->info.vol_id));
}

/*!
  Get wait time value for PsdPlayListDescriptor *d.
  Time is in seconds unless it is -1 (unlimited).
*/
int
vcdinfo_get_wait_time (const PsdPlayListDescriptor *d) 
{
  return vcdinfo_calc_psd_wait_time (d->wtime);
}

/*!
  Returns a string which interpreting the extended attribute xa_attr. 
  For example:
  \verbatim
  d---1xrxrxr
  ---2--r-r-r
  -a--1xrxrxr
  \endverbatim

  A description of the characters in the string follows
  The 1st character is either "d" if the entry is a directory, or "-" if not.
  The 2nd character is either "a" if the entry is CDDA (audio), or "-" if not.
  The 3rd character is either "i" if the entry is interleaved, or "-" if not.
  The 4th character is either "2" if the entry is mode2 form2 or "-" if not.
  The 5th character is either "1" if the entry is mode2 form1 or "-" if not.
   Note that an entry will either be in mode2 form1 or mode form2. That
   is you will either see "2-" or "-1" in the 4th & 5th positions.

  The 6th and 7th characters refer to permissions for everyone while the
  the 8th and 9th characters refer to permissions for a group while, and 
  the 10th and 11th characters refer to permissions for a user. 
 
  In each of these pairs the first character (6, 8, 10) is "x" if the 
  entry is executable. For a directory this means the directory is
  allowed to be listed or "searched".
  The second character of a pair (7, 9, 11) is "r" if the entry is allowed
  to be read. 
*/

const char *
vcdinfo_get_xa_attr_str (uint16_t xa_attr)
{
  char *result = _getbuf();

  xa_attr = uint16_from_be (xa_attr);

  result[0] = (xa_attr & XA_ATTR_DIRECTORY) ? 'd' : '-';
  result[1] = (xa_attr & XA_ATTR_CDDA) ? 'a' : '-';
  result[2] = (xa_attr & XA_ATTR_INTERLEAVED) ? 'i' : '-';
  result[3] = (xa_attr & XA_ATTR_MODE2FORM2) ? '2' : '-';
  result[4] = (xa_attr & XA_ATTR_MODE2FORM1) ? '1' : '-';

  result[5] = (xa_attr & XA_ATTR_O_EXEC) ? 'x' : '-';
  result[6] = (xa_attr & XA_ATTR_O_READ) ? 'r' : '-';

  result[7] = (xa_attr & XA_ATTR_G_EXEC) ? 'x' : '-';
  result[8] = (xa_attr & XA_ATTR_G_READ) ? 'r' : '-';

  result[9] = (xa_attr & XA_ATTR_U_EXEC) ? 'x' : '-';
  result[10] = (xa_attr & XA_ATTR_U_READ) ? 'r' : '-';

  result[11] = '\0';

  return result;
}

/*!
  Add one to the MSF.
*/
void
vcdinfo_inc_msf (uint8_t *min, uint8_t *sec, int8_t *frame)
{
  (*frame)++;
  if (*frame>=75) {
    *frame = 0;
    (*sec)++;
    if (*sec>=60) {
      *sec = 0;
      (*min)++;
    }
  }
}

/*!
  Convert minutes, seconds and frame (MSF components) into a
  logical block address (or LBA). 
  See also msf_to_lba which uses msf_t as its single parameter.
*/
lba_t
vcdinfo_msf2lba (uint8_t min, uint8_t sec, int8_t frame)
{
  return 75*(60*min + sec) + frame;
}

/*!
  Convert minutes, seconds and frame (MSF components) into a
  logical block address (or LBA). 
  See also msf_to_lba which uses msf_t as its single parameter.
*/
void 
vcdinfo_lba2msf (lba_t lba, uint8_t *min, uint8_t *sec, uint8_t *frame) 
{
  *min = lba / (60*75);
  lba %= (60*75);
  *sec = lba / 75; 
  *frame = lba % 75; 
}

/*!
  Convert minutes, seconds and frame (MSF components) into a
  logical sector number (or LSN). 
*/
lsn_t
vcdinfo_msf2lsn (uint8_t min, uint8_t sec, int8_t frame)
{
  lba_t lba=75*(60*min + sec) + frame;
  if (lba < PREGAP_SECTORS) {
    vcd_error ("lba (%u) less than pregap sector (%u)", 
               lba, PREGAP_SECTORS);
    return lba;
  }
  return lba - PREGAP_SECTORS;
}

const char *
vcdinfo_ofs2str (const vcdinfo_obj_t *obj, unsigned int offset, bool ext)
{
  vcdinfo_offset_t *ofs;
  char *buf;

  switch (offset) {
  case PSD_OFS_DISABLED:
    return "disabled";
  case PSD_OFS_MULTI_DEF:
    return "multi_def";
  case PSD_OFS_MULTI_DEF_NO_NUM:
    return "multi_def_no_num";
  default: ;
  }

  buf = _getbuf ();
  ofs = vcdinfo_get_offset_t(obj, offset, ext);
  if (ofs != NULL) {
    if (ofs->lid)
      snprintf (buf, BUF_SIZE, "LID[%d] @0x%4.4x", 
                ofs->lid, ofs->offset);
    else 
      snprintf (buf, BUF_SIZE, "PSD[?] @0x%4.4x", 
                ofs->offset);
  } else {
    snprintf (buf, BUF_SIZE, "? @0x%4.4x", offset);
  }
  return buf;
}

bool
vcdinfo_read_psd (vcdinfo_obj_t *obj)
{
  unsigned psd_size = vcdinfo_get_psd_size (obj);

  if (psd_size)
    {
      if (psd_size > 256*1024)
        {
          vcd_error ("weird psd size (%u) -- aborting", psd_size);
          return false;
        }

      obj->lot = _vcd_malloc (ISO_BLOCKSIZE * LOT_VCD_SIZE);
      obj->psd = _vcd_malloc (ISO_BLOCKSIZE * _vcd_len2blocks (psd_size, 
                                                               ISO_BLOCKSIZE));
      
      if (vcd_image_source_read_mode2_sectors (obj->img, (void *) obj->lot, 
                                               LOT_VCD_SECTOR, false,
                                               LOT_VCD_SIZE))
        return false;
          
      if (vcd_image_source_read_mode2_sectors (obj->img, (void *) obj->psd, 
                                               PSD_VCD_SECTOR, false,
                                               _vcd_len2blocks (psd_size, 
                                                                ISO_BLOCKSIZE)))
        return false;
  
    } else {
      return false;
    }
  return true;
}



void
vcdinfo_visit_lot (vcdinfo_obj_t *obj, bool ext)
{
  const LotVcd *lot = ext ? obj->lot_x : obj->lot;
  unsigned n;

  if (!ext && !vcdinfo_get_psd_size (obj))
    return;

  if (ext && !obj->psd_x_size)
    return;

  for (n = 0; n < LOT_VCD_OFFSETS; n++)
    _visit_pbc (obj, n + 1, uint16_from_be (lot->offset[n]), true, ext);

  _vcd_list_sort (ext ? obj->offset_x_list : obj->offset_list, 
                  (_vcd_list_cmp_func) _offset_t_cmp);
}

/*!
   Change trailing blanks in str to nulls.  Str has a maximum size of
   n characters.
*/
const char *
vcdinfo_strip_trail (const char str[], size_t n)
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

/*!
 Return true if offset is "rejected". That is shouldn't be displayed
 in a list of entries.
*/
bool
vcdinfo_is_rejected(uint16_t offset)
{
  return (offset & VCDINFO_REJECTED_MASK) != 0;
}

/*! 
  Return true if VCD has "extended attributes" (XA). Extended attributes
  add meta-data attributes to a entries of file describing the file.
  See also vcdinfo_get_xa_attr_str() which returns a string similar to
  a string you might get on a Unix filesystem listing ("ls").
*/
bool
vcdinfo_has_xa(const vcdinfo_obj_t *obj) 
{
  return obj->has_xa;
}

int
vcdinfo_calc_psd_wait_time (uint16_t wtime)
{
  // Note: this doesn't agree exactly with _wtime
  if (wtime < 61)
    return wtime;
  else if (wtime < 255)
    return (wtime - 60) * 10 + 60;
  else
    return -1;
}

const char *
vcdinfo_area_str (const struct psd_area_t *_area)
{
  char *buf;

  if (!_area->x1  
      && !_area->y1
      && !_area->x2
      && !_area->y2)
    return "disabled";

  buf = _getbuf ();

  snprintf (buf, BUF_SIZE, "[%3d,%3d] - [%3d,%3d]",
            _area->x1, _area->y1,
            _area->x2, _area->y2);
            
  return buf;
}

/*!
   Initialize the vcdinfo structure "obj". Should be done before other
   routines using obj are called.
*/
bool 
vcdinfo_init(vcdinfo_obj_t *obj)
{
  if (NULL == obj) return false;
  memset(obj, 0, sizeof(vcdinfo_obj_t));
  obj->vcd_type = VCDINFO_SOURCE_UNDEF;
  obj->img = NULL;
  obj->lot = NULL;
  obj->source_name = NULL;
  obj->segment_list = NULL;
  return true;
}

/*!
   Set up vcdinfo structure "obj" for reading and writing from a
   particular medium. This should be done before after initialization
   but before any routines that need to retrieve or write data.
 
   source_name is the device or file to use for inspection, and
   source_type indicates if this is a device or a file.
    
   VCDINFO_OPEN_VCD is returned if everything went okay; 
   VCDINFO_OPEN_ERROR if there was an error and VCDINFO_OPEN_OTHER if the
   medium is something other than a VCD.
 */
vcdinfo_open_return_t
vcdinfo_open(vcdinfo_obj_t *obj, char source_name[], 
             vcdinfo_source_t source_type)
{

  VcdImageSource *img;
  char is_sector_2352 = true;
  bool null_source = NULL == source_name;

  if (VCDINFO_SOURCE_AUTO == source_type) {
    struct stat buf;
    if (0 != stat(source_name, &buf)) {
      vcd_error ("Can't stat file %s:", strerror(errno));
      return VCDINFO_OPEN_ERROR;
    }
    if (S_ISBLK(buf.st_mode) || S_ISCHR(buf.st_mode)) {
      source_type = VCDINFO_SOURCE_DEVICE;
    } else if (S_ISREG(buf.st_mode)) {
      /* FIXME: check to see if is a text file. If so, then 
        set VCDINFO_SOURCE_CUE. _*/
      int i=strlen(source_name)-strlen("bin");
      if (i > 0
          && (source_name[i] =='c' || source_name[i+1] =='C')
          && (source_name[i+1] =='u' || source_name[i+1] =='U')
          && (source_name[i+2] =='e' || source_name[i+2] =='E') ) 
        source_type = VCDINFO_SOURCE_CUE;
      else
        source_type = VCDINFO_SOURCE_BIN;
    } else {
      vcd_error ("Source file `%s' should either be a block device "
                 "or a regular file", source_name);
      return VCDINFO_OPEN_ERROR;
    }
  }

  switch (source_type) {
  case VCDINFO_SOURCE_DEVICE:
    img = vcd_image_source_new_cd ();
    if (null_source) {
      source_name=vcd_image_source_get_default_device(img);
    }
    vcd_image_source_set_arg (img, "device", source_name);
    break;
  case VCDINFO_SOURCE_SECTOR_2336:
    is_sector_2352 = false;
    /* Fall through to next case */
  case VCDINFO_SOURCE_BIN:
    img = vcd_image_source_new_bincue ();
    if (null_source) {
      source_name=vcd_image_source_get_default_device(img);
    }
    vcd_image_source_set_arg (img, "bin", source_name);
    vcd_image_source_set_arg (img, "sector", is_sector_2352 ? "2352": "2336");
    break;
  case VCDINFO_SOURCE_CUE:
    img = vcd_image_source_new_bincue ();
    if (null_source) {
      source_name=vcd_image_source_get_default_device(img);
    } else {
      int i=strlen(source_name)-strlen("cue");
      if (i>0) {
        char *bin_name=strdup(source_name);
        if (bin_name[i]=='c') {
          bin_name[i++]='b'; bin_name[i++]='i'; bin_name[i++]='n';
          vcd_image_source_set_arg (img, "bin", bin_name);
        } else if (bin_name[i]=='C') {
          bin_name[i++]='B'; bin_name[i++]='I'; bin_name[i++]='N';
          vcd_image_source_set_arg (img, "bin", bin_name);
        }
        vcd_image_source_set_arg (img, "cue", source_name);
      } else {
        return VCDINFO_OPEN_ERROR;
      }
      
    }
    vcd_image_source_set_arg (img, "sector", is_sector_2352 ? "2352": "2336");
    break;
  default: 
    return VCDINFO_OPEN_ERROR;
  }
  
  memset (obj, 0, sizeof (vcdinfo_obj_t));
  obj->img = img;  /* Note we do this after the above wipeout! */
  obj->segment_list = _vcd_list_new ();
  
  if (vcd_image_source_read_mode2_sector (obj->img, &(obj->pvd), 
                                          ISO_PVD_SECTOR, false))
    return(false);

  /* Determine if VCD has XA attributes. */
  {
    
    struct iso_primary_descriptor const *pvd = &obj->pvd;
    
    obj->has_xa = !strncmp ((char *) pvd + ISO_XA_MARKER_OFFSET, 
                            ISO_XA_MARKER_STRING, 
                           strlen (ISO_XA_MARKER_STRING));
  }
    
  if (vcd_image_source_read_mode2_sector (obj->img, &(obj->info), 
                                          INFO_VCD_SECTOR, false))
    return(false);

  if (vcdinfo_get_format_version (obj) == VCD_TYPE_INVALID)
    return VCDINFO_OPEN_OTHER;

  if (vcd_image_source_read_mode2_sector (obj->img, &(obj->entries), 
                                          ENTRIES_VCD_SECTOR, false))
    return VCDINFO_OPEN_OTHER;
  
  {
    size_t len = strlen(source_name)+1;
    obj->source_name = (char *) malloc(len * sizeof(char));
    strncpy(obj->source_name, source_name, len);
  }

  if (null_source) {
    free(source_name);
  }

  _init_fs_segment (obj, "/", true);

  return(true);
  
}

/*!
  Convert logical block address (LBA), to a logical sector number (LSN).
*/
lsn_t
vcdinfo_lba2lsn(lba_t lba) 
{
  return lba-PREGAP_SECTORS;
}

/*!
  Convert logical sector number (LSN), to a logical block address (LBA).
*/
lba_t
vcdinfo_lsn2lba(lsn_t lsn) 
{
  return _vcd_lsn_to_lba(lsn);
}

/*!
 Dispose of any resources associated with vcdinfo structure "obj".
 Call this when "obj" it isn't needed anymore. 

 True is returned is everything went okay, and false if not.
*/
bool 
vcdinfo_close(vcdinfo_obj_t *obj)
{
  if (obj != NULL) {
    if (obj->segment_list != NULL) 
      _vcd_list_free(obj->segment_list, 0);
    free(obj->lot);
    free(obj->tracks_buf);
    free(obj->search_buf);
    free(obj->source_name);
  }
  if (obj->img != NULL) vcd_image_source_destroy (obj->img);
  vcdinfo_init(obj);
  return(true);
}



/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
