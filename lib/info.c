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
/* 
   Things here refer to higher-level structures usually accessed via
   vcdinfo_t. For lower-level access which generally use 
   structures other than vcdinfo_t, see inf.c
*/


/* Private headers */
#include "info_private.h"
#include "vcd_assert.h"
#include "bytesex.h"
#include "pbc.h"
#include "util.h"
#include "vcd_read.h"

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

#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/util.h>

/* Eventually move above libvcd includes but having vcdinfo including. */
#include <libvcd/info.h>

#include <stdio.h>
#include <stddef.h>
#include <errno.h>

static const char _rcsid[] = "$Id$";

#define MIN_ENCODED_TRACK_NUM 100
#define MIN_ENCODED_SEGMENT_NUM 1000
#define MAX_ENCODED_SEGMENT_NUM 2979

#define BUF_COUNT 16
#define BUF_SIZE 80

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

/* 
   Initialize/allocate segment portion of vcdinfo_obj_t. 

   Getting exact segments sizes is done in a rather complicated way. 
   A simple approach would be to use the fixed size allocated on disk, 
   but players have trouble with the internal fragmentation padding. 
   More accurate results are obtained by consulting with ISO 9660 
   information for the corresponding file entry.

   Another approach to get segment sizes is to read/scan the
   MPEGs. That would be rather slow.
*/
static void
_init_segments (vcdinfo_obj_t *obj)
{
  InfoVcd *info = vcdinfo_get_infoVcd(obj);
  segnum_t num_segments = vcdinfo_get_num_segments(obj);
  VcdListNode *entnode;
  VcdList *entlist;
  int i;
  lsn_t last_lsn=0;
  
  obj->first_segment_lsn = cdio_msf_to_lsn(&info->first_seg_addr);
  obj->seg_sizes         = _vcd_malloc( num_segments * sizeof(uint32_t *));

  if (NULL == obj->seg_sizes || 0 == num_segments) return;

  entlist = iso9660_fs_readdir(obj->img, "SEGMENT", true);

  i=0;
  _VCD_LIST_FOREACH (entnode, entlist) {
    iso9660_stat_t *statbuf = _vcd_list_node_data (entnode);

    if (statbuf->type == _STAT_DIR) continue;

    while(info->spi_contents[i].item_cont) {
      obj->seg_sizes[i] = VCDINFO_SEGMENT_SECTOR_SIZE;
      i++;
    }
    
    /* Should have an entry in the ISO 9660 filesystem. Get and save 
       in statbuf.secsize this size.
    */
    obj->seg_sizes[i] = statbuf->secsize;

    if (last_lsn >= statbuf->lsn) 
      vcd_warn ("Segments if ISO 9660 directory out of order lsn %ul >= %ul", 
                (unsigned int) last_lsn, (unsigned int) statbuf->lsn);
    last_lsn = statbuf->lsn;

    i++;
  }

  while(i < num_segments && info->spi_contents[i].item_cont) {
    obj->seg_sizes[i] = VCDINFO_SEGMENT_SECTOR_SIZE;
    i++;
  }

  if (i != num_segments) 
    vcd_warn ("Number of segments found %d is not number of segments %d", 
              i, num_segments);

  _vcd_list_free (entlist, true);

  
#if 0
  /* Figure all of the segment sector sizes */
  for (i=0; i < num_segments; i++) {
    
    obj->seg_sizes[i] = VCDINFO_SEGMENT_SECTOR_SIZE;

    if ( !info->spi_contents[i].item_cont ) {
      /* Should have an entry in the ISO 9660 filesystem. Get and save 
         in statbuf.secsize this size.
       */
      lsn_t lsn = vcdinfo_get_seg_lsn(obj, i);
      iso9660_stat_t *statbuf =iso9660_find_fs_lsn(obj->img, lsn);
      if (NULL != statbuf) {
        obj->seg_sizes[i] = statbuf->secsize;
        free(statbuf);
      } else {
        vcd_warn ("Trouble finding ISO 9660 size for segment %d.", i);
      }
    }
  }
#endif

}

/*!
   Return the number of audio channels implied by "audio_type".
   0 is returned on error.
*/
unsigned int
vcdinfo_audio_type_num_channels(const vcdinfo_obj_t *obj, 
                                 unsigned int audio_type) 
{
  const int audio_types[2][5] =
    {
      { /* VCD 2.0 */
        0,   /* no audio*/
        1,   /* single channel */
        1,   /* stereo */
        2,   /* dual channel */
        0},  /* error */

      { /* SVCD, HQVCD */
        0,   /* no stream */ 
        1,   /*  1 stream */ 
        2,   /*  2 streams */
        1,   /*  1 multi-channel stream (surround sound) */
        0}   /*  error */
    };

  /* We should also check that the second index is in range too. */
  if (audio_type > 4) {
    return 0;
  }
  
  /* Get first index entry into above audio_type array from vcd_type */
  switch (obj->vcd_type) {

  case VCD_TYPE_VCD:
  case VCD_TYPE_VCD11:
    return 1;

  case VCD_TYPE_VCD2:
    return 3;
    break;

  case VCD_TYPE_HQVCD:
  case VCD_TYPE_SVCD:
    return audio_types[1][audio_type];
    break;

  case VCD_TYPE_INVALID:
  default:
    /* We have an invalid entry. Set to handle below. */
    return 0;
  }
}

/*!
  Return a string describing an audio type.
*/
const char * 
vcdinfo_audio_type2str(const vcdinfo_obj_t *obj, unsigned int audio_type)
{
  const char *audio_types[3][5] =
    {
      /* INVALID, VCD 1.0, or VCD 1.1 */
      { "unknown", "invalid", "", "", "" },  
      
      /*VCD 2.0 */
      { "no audio", "single channel", "stereo", "dual channel", "error" }, 
      
      /* SVCD, HQVCD */
      { "no stream", "1 stream", "2 streams", 
        "1 multi-channel stream (surround sound)", "error"}, 
    };

  unsigned int first_index = 0; 

  /* Get first index entry into above audio_type array from vcd_type */
  switch (obj->vcd_type) {

  case VCD_TYPE_VCD:
  case VCD_TYPE_VCD11:
  case VCD_TYPE_VCD2:
    first_index=1;
    break;

  case VCD_TYPE_HQVCD:
  case VCD_TYPE_SVCD:
    first_index=2;
    break;

  case VCD_TYPE_INVALID:
  default:
    /* We have an invalid entry. Set to handle below. */
    audio_type=4;
  }
  
  /* We should also check that the second index is in range too. */
  if (audio_type > 3) {
    first_index=0;
    audio_type=1;
  }
  
  return audio_types[first_index][audio_type];
}

/*!
  Note first seg_num is 0!
*/
const char * 
vcdinfo_ogt2str(const vcdinfo_obj_t *obj, segnum_t seg_num)
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
vcdinfo_video_type2str(const vcdinfo_obj_t *obj, segnum_t seg_num)
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

/*!
  \brief Classify itemid_num into the kind of item it is: track #, entry #, 
  segment #. 
  \param itemid is set to contain this classification an the converted 
  entry number. 
*/
void
vcdinfo_classify_itemid (uint16_t itemid_num, 
                         /*out*/ vcdinfo_itemid_t *itemid)
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
    itemid->num -= (MIN_ENCODED_SEGMENT_NUM);
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
   Return a string containing the VCD album id, or NULL if there is 
   some problem in getting this. 
*/
const char *
vcdinfo_get_album_id(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj ) return (NULL);
  return vcdinf_get_album_id(&obj->info);
}

/*!
  Return the VCD ID.
  NULL is returned if there is some problem in getting this. 
*/
const char *
vcdinfo_get_application_id(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj ) return (NULL);
  return(vcdinf_get_application_id(&obj->pvd));
}

/*!
  Return a pointer to the cdio structure for the CD image opened or
  NULL if error.
*/
CdIo *
vcdinfo_get_cd_image (const vcdinfo_obj_t *vcd_obj) 
{
  if (NULL == vcd_obj) return NULL;
  return vcd_obj->img;
}


/**
 \fn vcdinfo_get_default_offset(const vcdinfo_obj_t *obj, unsinged int lid);
 \brief Get return offset for a given PLD selector descriptor. 
 \return  VCDINFO_INVALID_OFFSET is returned on error or if pld has no 
 "return" entry or pld is NULL. Otherwise the LID offset is returned.
 */
uint16_t
vcdinfo_get_default_offset(const vcdinfo_obj_t *obj, unsigned int lid)
{
  if (NULL != obj) {
    
    PsdListDescriptor pxd;

    vcdinfo_lid_get_pxd(obj, &pxd, lid);
    
    switch (pxd.descriptor_type) {
    case PSD_TYPE_EXT_SELECTION_LIST:
    case PSD_TYPE_SELECTION_LIST:
      return vcdinf_psd_get_default_offset(pxd.psd);
      break;
    case PSD_TYPE_PLAY_LIST:
    case PSD_TYPE_END_LIST:
    case PSD_TYPE_COMMAND_LIST:
      break;
    }
  }
  return VCDINFO_INVALID_OFFSET;
}

/*!
  Return a string containing the default VCD device if none is specified.
  Return NULL we can't get this information.
*/
char *
vcdinfo_get_default_device (const vcdinfo_obj_t *vcd_obj)
{

  /* If device not already open, then we'll open it temporarily and
     let CdIo select a driver, get the default for that and then
     close/destroy the temporary we created.
   */
  CdIo *cdio=NULL;
  if (vcd_obj != NULL && vcd_obj->img != NULL)
    cdio = vcd_obj->img;

  return cdio_get_default_device(cdio);
}

/*!
  Return number of sector units in of an entry. 0 is returned if entry_num
  is out of range.
  The first entry number is 0.
*/
uint32_t
vcdinfo_get_entry_sect_count (const vcdinfo_obj_t *obj, unsigned int entry_num)
{
  const EntriesVcd *entries = &obj->entries;
  const unsigned int entry_count = vcdinf_get_num_entries(entries);
  if (entry_num > entry_count) 
    return 0;
  else {
    const lsn_t this_lsn = vcdinfo_get_entry_lsn(obj, entry_num);
    lsn_t next_lsn;
    if (entry_num < entry_count-1) {
      track_t track=vcdinfo_get_track(obj, entry_num);
      track_t next_track=vcdinfo_get_track(obj, entry_num+1);
      next_lsn = vcdinfo_get_entry_lsn(obj, entry_num+1);
      /* If we've changed tracks, don't include pregap sector between 
         tracks.
       */
      if (track != next_track) next_lsn -= CDIO_PREGAP_SECTORS;
    } else {
      /* entry_num == entry_count -1. Or the last entry.
         This is really really ugly. There's probably a better
         way to do it. 
         Below we get the track of the current entry and then the LBA of the
         beginning of the following (leadout?) track.

         Wait! It's uglier than that! Since VCD's can be created
         *without* a pregap to the leadout track, we try not to use
         that if we can get the entry from the ISO 9660 filesystem.
      */
      track_t track = vcdinfo_get_track(obj, entry_num);
      if (track != VCDINFO_INVALID_TRACK) {
        iso9660_stat_t *statbuf;
        const lsn_t lsn = vcdinfo_get_track_lsn(obj, track);

        /* Try to get the sector count from the ISO 9660 filesystem */
        statbuf = iso9660_find_fs_lsn(obj->img, lsn);
        
        if (NULL != statbuf) {
          next_lsn = lsn + statbuf->secsize;
          free(statbuf);
        } else {
          /* Failed on ISO 9660 filesystem. Use next track or
             LEADOUT track.  */
          next_lsn = vcdinfo_get_track_lsn(obj, track+1);
        }
        if (next_lsn == VCDINFO_NULL_LSN)
          return 0;
      } else {
        /* Something went wrong. Set up size to zero. */
        return 0;
      }
    }
    return (next_lsn - this_lsn);
  }
}

/*!  Return the starting MSF (minutes/secs/frames) for sequence
  entry_num in obj.  NULL is returned if there is no entry.
  The first entry number is 0.
*/
const msf_t *
vcdinfo_get_entry_msf(const vcdinfo_obj_t *obj, unsigned int entry_num)
{
  const EntriesVcd *entries = &obj->entries;
  return vcdinf_get_entry_msf(entries, entry_num);
}

/*!  Return the starting LBA (logical block address) for sequence
  entry_num in obj.  VCDINFO_NULL_LBA is returned if there is no entry.
*/
lba_t
vcdinfo_get_entry_lba(const vcdinfo_obj_t *obj, unsigned int entry_num)
{
  if ( NULL == obj ) return VCDINFO_NULL_LBA;
  else {
    const msf_t *msf = vcdinfo_get_entry_msf(obj, entry_num);
    msf = vcdinfo_get_entry_msf(obj, entry_num);
    return (msf != NULL) ? cdio_msf_to_lba(msf) : VCDINFO_NULL_LBA;
  }
}

/*!  Return the starting LBA (logical block address) for sequence
  entry_num in obj.  VCDINFO_NULL_LBA is returned if there is no entry.
*/
lsn_t
vcdinfo_get_entry_lsn(const vcdinfo_obj_t *obj, unsigned int entry_num)
{
  if ( NULL == obj ) return VCDINFO_NULL_LBA;
  else {
    const msf_t *msf = vcdinfo_get_entry_msf(obj, entry_num);
    return (msf != NULL) ? cdio_msf_to_lsn(msf) : VCDINFO_NULL_LSN;
  }
}

/*!
   Get the VCD format (VCD 1.0 VCD 1.1, SVCD, ... for this object.
   The type is also set inside obj.
*/
vcd_type_t
vcdinfo_get_format_version (vcdinfo_obj_t *obj)
{
  return obj->vcd_type;
}

/*!
   Return a string giving VCD format (VCD 1.0 VCD 1.1, SVCD, ... 
   for this object.
*/
const char *
vcdinfo_get_format_version_str (const vcdinfo_obj_t *obj) 
{
  if (NULL == obj) return "*Uninitialized*";
  return vcdinf_get_format_version_str(obj->vcd_type);
}

EntriesVcd * 
vcdinfo_get_entriesVcd (vcdinfo_obj_t *obj) 
{
  if (NULL == obj) return NULL;
  return &obj->entries;
}
  
InfoVcd * 
vcdinfo_get_infoVcd (vcdinfo_obj_t *obj) 
{
  if (NULL == obj) return NULL;
  return &obj->info;
}
  
void * 
vcdinfo_get_tracksSVD (vcdinfo_obj_t *obj) 
{
  if (NULL == obj) return NULL;
  return obj->tracks_buf;
}
  
void * 
vcdinfo_get_scandata (vcdinfo_obj_t *obj) 
{
  if (NULL == obj) return NULL;
  return obj->scandata_buf;
}
  
void * 
vcdinfo_get_searchDat (vcdinfo_obj_t *obj) 
{
  if (NULL == obj) return NULL;
  return obj->search_buf;
}
  
void * 
vcdinfo_get_pvd (vcdinfo_obj_t *obj) 
{
  if (NULL == obj) return NULL;
  return &obj->pvd;
}
  
/*!
  Get the itemid for a given list ID. 
  VCDINFO_REJECTED_MASK is returned on error or if obj is NULL. 
*/
uint16_t
vcdinfo_lid_get_itemid(const vcdinfo_obj_t *obj, lid_t lid)
{
  PsdListDescriptor pxd;

  if (obj == NULL) return VCDINFO_REJECTED_MASK;
  vcdinfo_lid_get_pxd(obj, &pxd, lid);
  switch (pxd.descriptor_type) {
  case PSD_TYPE_SELECTION_LIST:
  case PSD_TYPE_EXT_SELECTION_LIST:
    if (pxd.psd == NULL) return VCDINFO_REJECTED_MASK;
    return vcdinf_psd_get_itemid(pxd.psd);
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
  Get the LOT pointer. 
*/
LotVcd *
vcdinfo_get_lot(const vcdinfo_obj_t *obj) 
{
  if (NULL == obj) return NULL;
  return obj->lot;
}

/*!
  Get the extended LOT pointer. 
*/
LotVcd *
vcdinfo_get_lot_x(const vcdinfo_obj_t *obj) 
{
  if (NULL == obj) return NULL;
  return obj->lot_x;
}
  
/*!
  Return number of LIDs. 
*/
lid_t
vcdinfo_get_num_LIDs (const vcdinfo_obj_t *obj) 
{
  /* Should probably use _vcd_pbc_max_lid instead? */
  if (NULL==obj) return 0;
  return vcdinf_get_num_LIDs(&obj->info);
}

/*!
  Return the number of entries in the VCD.
*/
unsigned int
vcdinfo_get_num_entries(const vcdinfo_obj_t *obj)
{
  const EntriesVcd *entries = &obj->entries;
  return vcdinf_get_num_entries(entries);
}

/*!
  Return the number of segments in the VCD. Return 0 if there is some
  problem.
*/
segnum_t
vcdinfo_get_num_segments(const vcdinfo_obj_t *obj)
{
  if (NULL==obj) return 0;
  return vcdinf_get_num_segments(&obj->info);
}

/*!
  \fn vcdinfo_get_offset_from_lid(const vcdinfo_obj_t *obj, unsigned int entry_num);
  \brief Get offset entry_num for a given LID. 
  \return VCDINFO_INVALID_OFFSET is returned if obj on error or obj
  is NULL. Otherwise the LID offset is returned.
*/
uint16_t
vcdinfo_lid_get_offset(const vcdinfo_obj_t *obj, lid_t lid,
                       unsigned int entry_num) 
{
  PsdListDescriptor pxd;

  if (obj == NULL) return VCDINFO_INVALID_OFFSET;
  vcdinfo_lid_get_pxd(obj, &pxd, lid);

  switch (pxd.descriptor_type) {
  case PSD_TYPE_SELECTION_LIST:
  case PSD_TYPE_EXT_SELECTION_LIST:
    if (pxd.psd == NULL) return VCDINFO_INVALID_OFFSET;
    return vcdinf_psd_get_offset(pxd.psd, entry_num-1);
    break;
  case PSD_TYPE_PLAY_LIST:
    /* FIXME: There is an array of items */
  case PSD_TYPE_END_LIST:
  case PSD_TYPE_COMMAND_LIST:
    return VCDINFO_INVALID_OFFSET;
  }
  return VCDINFO_INVALID_OFFSET;

}

/*! 
  NULL is returned on error. 
*/
static vcdinfo_offset_t *
_vcdinfo_get_offset_t (const vcdinfo_obj_t *obj, unsigned int offset, bool ext)
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
  Get the VCD info list.
*/
VcdList *
vcdinfo_get_offset_list(const vcdinfo_obj_t *obj)
{
  if (NULL == obj) return NULL;
  return obj->offset_list;
}


/*!
  Get the VCD info extended offset list.
*/
VcdList *
vcdinfo_get_offset_x_list(const vcdinfo_obj_t *obj)
{
  if (NULL == obj) return NULL;
  return obj->offset_x_list;
}

/*!
  Get the VCD info offset multiplier.
*/
unsigned int vcdinfo_get_offset_mult(const vcdinfo_obj_t *obj) 
{
  if (NULL == obj) return 0xFFFF;
  return obj->info.offset_mult;
}

/*! 
  Get entry in offset list for the item that has offset. This entry 
  has for example the LID. NULL is returned on error. 
*/
vcdinfo_offset_t *
vcdinfo_get_offset_t (const vcdinfo_obj_t *obj, unsigned int offset)
{
  vcdinfo_offset_t *off_p= _vcdinfo_get_offset_t (obj, offset, true);
  if (NULL != off_p) 
    return off_p;
  return _vcdinfo_get_offset_t (obj, offset, false);
}

/*!
   Return a string containing the VCD publisher id with trailing
   blanks removed, or NULL if there is some problem in getting this.
*/
const char *
vcdinfo_get_preparer_id(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj ) return (NULL);
  return vcdinf_get_preparer_id(&obj->pvd);
}

/*!
  Get the PSD.
*/
uint8_t *
vcdinfo_get_psd(const vcdinfo_obj_t *obj) 
{
  if ( NULL == obj ) return (NULL);
  return obj->psd;
}

/*!
  Get the extended PSD.
*/
uint8_t *
vcdinfo_get_psd_x(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj ) return (NULL);
  return obj->psd_x;
}

/*!
  Return number of bytes in PSD. Return 0 if there's an error.
*/
uint32_t
vcdinfo_get_psd_size (const vcdinfo_obj_t *obj)
{
  if ( NULL == obj ) return 0;
  return vcdinf_get_psd_size(&obj->info);
}

/*!
  Return number of bytes in the extended PSD. Return 0 if there's an error.
*/
uint32_t
vcdinfo_get_psd_x_size (const vcdinfo_obj_t *obj)
{
  if ( NULL == obj ) return 0;
  return obj->psd_x_size;
}

/*!
   Return a string containing the VCD publisher id with trailing
   blanks removed, or NULL if there is some problem in getting this.
*/
const char *
vcdinfo_get_publisher_id(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj ) return (NULL);
  return vcdinf_get_publisher_id(&obj->pvd);
}

/*!
  Get the PSD Selection List Descriptor for a given lid.
  NULL is returned if error or not found.
*/
static bool
_vcdinfo_lid_get_pxd(const vcdinfo_obj_t *obj, PsdListDescriptor *pxd,
                     uint16_t lid, bool ext) 
{
  VcdListNode *node;
  unsigned mult = obj->info.offset_mult;
  const uint8_t *psd = ext ? obj->psd_x : obj->psd;
  VcdList *offset_list = ext ? obj->offset_x_list : obj->offset_list;

  if (offset_list == NULL) return false;
  
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
            if (vcdinf_pld_get_lid(pxd->pld) == lid) {
              return true;
            }
            break;
          }

        case PSD_TYPE_EXT_SELECTION_LIST:
        case PSD_TYPE_SELECTION_LIST: 
          {
            pxd->psd = (PsdSelectionListDescriptor *) (psd + _rofs);
            if (vcdinf_psd_get_lid(pxd->psd) == lid) {
              return true;
            }
            break;
          }
        default: ;
        }
    }
  return false;
}

/*!
  Get the PSD Selection List Descriptor for a given lid.
  False is returned if not found.
*/
bool
vcdinfo_lid_get_pxd(const vcdinfo_obj_t *obj, PsdListDescriptor *pxd,
                    uint16_t lid)
{
  if (_vcdinfo_lid_get_pxd(obj, pxd, lid, true))
    return true;
  return _vcdinfo_lid_get_pxd(obj, pxd, lid, false);
}

/**
 \fn vcdinfo_get_return_offset(const vcdinfo_obj_t *obj);
 \brief Get return offset for a given LID. 
 \return  VCDINFO_INVALID_OFFSET is returned on error or if LID has no 
 "return" entry. Otherwise the LID offset is returned.
 */
uint16_t
vcdinfo_get_return_offset(const vcdinfo_obj_t *obj, lid_t lid)
{
  if (NULL != obj) {

    PsdListDescriptor pxd;

    vcdinfo_lid_get_pxd(obj, &pxd, lid);
    
    switch (pxd.descriptor_type) {
    case PSD_TYPE_PLAY_LIST:
      return vcdinf_pld_get_return_offset(pxd.pld);
    case PSD_TYPE_SELECTION_LIST:
    case PSD_TYPE_EXT_SELECTION_LIST:
      return vcdinf_psd_get_return_offset(pxd.psd);
      break;
    case PSD_TYPE_END_LIST:
    case PSD_TYPE_COMMAND_LIST:
      break;
    }
  }
  
  return VCDINFO_INVALID_OFFSET;
}

/*!
   Return the audio type for a given segment. 
   VCDINFO_INVALID_AUDIO_TYPE is returned on error.
*/
unsigned int
vcdinfo_get_seg_audio_type(const vcdinfo_obj_t *obj, segnum_t seg_num)
{
  if ( NULL == obj || NULL == &obj->info 
       || seg_num >= vcdinfo_get_num_segments(obj) )
    return VCDINFO_INVALID_AUDIO_TYPE;
  return(obj->info.spi_contents[seg_num].audio_type);
}

/*!
   Return true if this segment is supposed to continue to the next one,
   (is part of an "item" or listing in the ISO 9660 filesystem).
*/
bool
vcdinfo_get_seg_continue(const vcdinfo_obj_t *obj, segnum_t seg_num)
{
  if ( NULL == obj || NULL == &obj->info 
       || seg_num >= vcdinfo_get_num_segments(obj) )
    return false;
  return(obj->info.spi_contents[seg_num].item_cont);
}

/*!  Return the starting LBA (logical block address) for segment
  entry_num in obj.  VCDINFO_LBA_NULL is returned if there is no entry.

  Note first seg_num is 0.
*/
lba_t
vcdinfo_get_seg_lba(const vcdinfo_obj_t *obj, segnum_t seg_num)
{ 
  if (obj == NULL) return VCDINFO_NULL_LBA;
  return cdio_lsn_to_lba(vcdinfo_get_seg_lba(obj, seg_num));
}

/*!  Return the starting LBA (logical block address) for segment
  entry_num in obj.  VCDINFO_LSN_NULL is returned if there is no entry.

  Note first seg_num is 0.
*/
lsn_t
vcdinfo_get_seg_lsn(const vcdinfo_obj_t *obj, segnum_t seg_num)
{ 
  if (obj == NULL || seg_num >= vcdinfo_get_num_segments(obj))
    return VCDINFO_NULL_LSN;
  return obj->first_segment_lsn + (VCDINFO_SEGMENT_SECTOR_SIZE * seg_num);
}

/*!  Return the starting MSF (minutes/secs/frames) for segment
  entry_num in obj.  NULL is returned if there is no entry.

  Note first seg_num is 0!
*/
const msf_t *
vcdinfo_get_seg_msf(const vcdinfo_obj_t *obj, segnum_t seg_num)
{ 
  if (obj == NULL || seg_num >= vcdinfo_get_num_segments(obj))
    return NULL;
  else {
    lsn_t lsn = vcdinfo_get_seg_lsn(obj, seg_num);
    static msf_t msf;
    cdio_lsn_to_msf(lsn, &msf);
    return &msf;
  }
}

/*!  
  Return the number of sectors for segment
  entry_num in obj.  0 is returned if there is no entry.

  Use this routine to figure out the actual number of bytes a physical
  region of a disk or CD takes up for a segment.

  If an item has been broken up into a number of "continued" segments,
  we will report the item size for the first segment and 0 for the
  remaining ones. We may revisit this decision later. 
*/
uint32_t
vcdinfo_get_seg_sector_count(const vcdinfo_obj_t *obj, segnum_t seg_num)
{ 
  if (obj == NULL || seg_num >= vcdinfo_get_num_segments(obj))
    return 0;
  return obj->seg_sizes[seg_num];
}

/*!
   Return a string containing the VCD system id with trailing
   blanks removed, or NULL if there is some problem in getting this.
*/
const char *
vcdinfo_get_system_id(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj || NULL == &obj->pvd ) return (NULL);
  return(vcdinf_get_system_id(&obj->pvd));
}

/*!
  Return the track number for entry n in obj. 
  
  In contrast to libcdio we start numbering at 0 which is the
  ISO9660 and metadata information for the Video CD. Thus track 
  1 is the first track the first complete MPEG track generally.
*/
track_t
vcdinfo_get_track(const vcdinfo_obj_t *obj, const unsigned int entry_num)
{
  const EntriesVcd *entries = &obj->entries;
  const unsigned int entry_count = vcdinf_get_num_entries(entries);
  /* Note entry_num is 0 origin. */
  return entry_num < entry_count ?
    vcdinf_get_track(entries, entry_num)-1: VCDINFO_INVALID_TRACK;
}

/*!
   Return the audio type for a given track. 
   VCDINFO_INVALID_AUDIO_TYPE is returned on error.

   Note: track 1 is usually the first track.
*/
unsigned int
vcdinfo_get_track_audio_type(const vcdinfo_obj_t *obj, track_t track_num)
{
  TracksSVD *tracks;
  TracksSVD2 *tracks2;
  if ( NULL == obj || NULL == &obj->info ) return VCDINFO_INVALID_AUDIO_TYPE;
  tracks = obj->tracks_buf;

  if ( NULL == tracks ) return 0;
  tracks2 = (TracksSVD2 *) &(tracks->playing_time[tracks->tracks]);
  return(tracks2->contents[track_num-1].audio);
}

/*!  
  Return the highest track number in the current medium. 
  
  Because we track start numbering at 0 (which is the ISO 9660 track
  containing Video CD naviagion and disk information), this is one
  less than the number of tracks.
  
    If there are no tracks, we return -1.
*/
unsigned int
vcdinfo_get_num_tracks(const vcdinfo_obj_t *obj)
{
  if (obj == NULL || obj->img == NULL) return 0;

  return cdio_get_num_tracks(obj->img)-1;
}


/*!  
  Return the starting LBA (logical block address) for track number
  track_num in obj.  

  The IS0-9660 filesystem track has number 0. Tracks associated
  with playable entries numbers start at 1.

  The "leadout" track is specified either by
  using track_num LEADOUT_TRACK or the total tracks+1.
  VCDINFO_NULL_LBA is returned on failure.
*/
lba_t 
vcdinfo_get_track_lba(const vcdinfo_obj_t *obj, track_t track_num)
{
  if (NULL == obj || NULL == obj->img)
    return VCDINFO_NULL_LBA;
  

  /* CdIo tracks start at 1 rather than 0. */
  return cdio_get_track_lba(obj->img, track_num+1);
}

/*!  
  Return the starting LSN (logical sector number) for track number
  track_num in obj.  

  The IS0-9660 filesystem track has number 0. Tracks associated
  with playable entries numbers start at 1.

  The "leadout" track is specified either by
  using track_num LEADOUT_TRACK or the total tracks+1.
  VCDINFO_NULL_LBA is returned on failure.
*/
lsn_t 
vcdinfo_get_track_lsn(const vcdinfo_obj_t *obj, track_t track_num)
{
  if (NULL == obj || NULL == obj->img)
    return VCDINFO_NULL_LSN;

  /* CdIo tracks start at 1 rather than 0. */
  return cdio_get_track_lsn(obj->img, track_num+1);
}

/*!  
  Return the starting MSF (minutes/secs/frames) for track number
  track_num in obj.  

  The IS0-9660 filesystem track has number 0. Tracks associated
  with playable entries numbers start at 1.

  The "leadout" track is specified either by
  using track_num LEADOUT_TRACK or the total tracks+1.
  VCDINFO_NULL_LBA is returned on failure.
*/
int
vcdinfo_get_track_msf(const vcdinfo_obj_t *obj, track_t track_num,
                      uint8_t *min, uint8_t *sec, uint8_t *frame)
{
  msf_t msf;

  if (NULL == obj || NULL == obj->img)
    return 1;
  
  /* CdIo tracks start at 1 rather than 0. */
  if (cdio_get_track_msf(obj->img, track_num+1, &msf)) {
    *min   = from_bcd8(msf.m);
    *sec   = from_bcd8(msf.s);
    *frame = from_bcd8(msf.f);
    return 0;
  }
  
  return 1;
}

/*!
  Return the size in sectors for track n. 

  The IS0-9660 filesystem track has number 0. Tracks associated
  with playable entries numbers start at 1.

  FIXME: Whether we count the track pregap sectors is a bit haphazard.
  We should add a parameter to indicate whether this is wanted or not.

*/
unsigned int
vcdinfo_get_track_sect_count(const vcdinfo_obj_t *obj, const track_t track_num)
{
  if (NULL == obj || VCDINFO_INVALID_TRACK == track_num) 
    return 0;
  
  {
    iso9660_stat_t *statbuf;
    const lsn_t lsn = vcdinfo_get_track_lsn(obj, track_num);
    
    /* Try to get the sector count from the ISO 9660 filesystem */
    if (obj->has_xa && (statbuf = iso9660_find_fs_lsn(obj->img, lsn))) {
      unsigned int secsize = statbuf->secsize;
      free(statbuf);
      return secsize;
    } else {
      const lsn_t next_lsn=vcdinfo_get_track_lsn(obj, track_num+1);
      /* Failed on ISO 9660 filesystem. Use track information.  */
      return next_lsn > lsn ? next_lsn - lsn : 0;
    }
  }
  return 0;
}

/*!
  Return size in bytes for track number for entry n in obj.

  The IS0-9660 filesystem track has number 1. Tracks associated
  with playable entries numbers start at 2.

  FIXME: Whether we count the track pregap sectors is a bit haphazard.
  We should add a parameter to indicate whether this is wanted or not.
*/
unsigned int
vcdinfo_get_track_size(const vcdinfo_obj_t *obj, track_t track_num)
{
  if (NULL == obj || VCDINFO_INVALID_TRACK == track_num) 
    return 0;
  
  {
    iso9660_stat_t statbuf;
    const lsn_t lsn = cdio_lba_to_lsn(vcdinfo_get_track_lba(obj, track_num));
    
    /* Try to get the sector count from the ISO 9660 filesystem */
    if (obj->has_xa && iso9660_find_fs_lsn(obj->img, lsn)) {
      return statbuf.size;
    } 
#if 0
    else {
      /* Failed on ISO 9660 filesystem. Use track information.  */
      if (obj->img != NULL) 
        return cdio_get_track_size(obj->img);
    }
#endif 
  }
  return 0;
}

/*!
  \brief Get the kind of video stream segment of segment seg_num in obj.
  \return VCDINFO_FILES_VIDEO_INVALID is returned if  on error or obj is
  null. Otherwise the enumeration type.

  Note first seg_num is 0!
*/
vcdinfo_video_segment_type_t
vcdinfo_get_video_type(const vcdinfo_obj_t *obj, segnum_t seg_num)
{
  const InfoVcd *info;
  if (obj == NULL)  return VCDINFO_FILES_VIDEO_INVALID;
  info = &obj->info;
  if (info == NULL) return VCDINFO_FILES_VIDEO_INVALID;
  return info->spi_contents[seg_num].video_type;
}

/*!
  \brief Get the kind of VCD that obj refers to.
*/
vcd_type_t
vcdinfo_get_VCD_type(const vcdinfo_obj_t *obj) 
{
  if (NULL == obj) return VCD_TYPE_INVALID;
  return obj->vcd_type;
}

  
/*!
  Return the VCD volume count - the number of CD's in the collection.
  O is returned if there is some problem in getting this. 
*/
unsigned int
vcdinfo_get_volume_count(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj ) return 0;
  return vcdinf_get_volume_count(&obj->info);
}

/*!
  Return the VCD ID.
  NULL is returned if there is some problem in getting this. 
*/
const char *
vcdinfo_get_volume_id(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj || NULL == &obj->pvd ) return (NULL);
  return(vcdinf_get_volume_id(&obj->pvd));
}

/*!
  Return the VCD volumeset ID.
  NULL is returned if there is some problem in getting this. 
*/
const char *
vcdinfo_get_volumeset_id(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj || NULL == &obj->pvd ) return (NULL);
  return(vcdinfo_strip_trail(obj->pvd.volume_set_id, MAX_VOLUMESET_ID));
}

/*!
  Return the VCD volume num - the number of the CD in the collection.
  This is a number between 1 and the volume count.
  O is returned if there is some problem in getting this. 
*/
unsigned int
vcdinfo_get_volume_num(const vcdinfo_obj_t *obj)
{
  if ( NULL == obj ) return 0;
  return(uint16_from_be( obj->info.vol_id));
}

int
vcdinfo_get_wait_time (uint16_t wtime)
{
  /* Note: this doesn't agree exactly with _wtime */
  if (wtime < 61)
    return wtime;
  else if (wtime < 255)
    return (wtime - 60) * 10 + 60;
  else
    return -1;
}

/*!
  Return true is there is playback control. 
*/
bool
vcdinfo_has_pbc (const vcdinfo_obj_t *obj) 
{
  return (obj && obj->info.psd_size!=0);
}
  
/*! 
  Return true if VCD has "extended attributes" (XA). Extended attributes
  add meta-data attributes to a entries of file describing the file.
  See also cdio_get_xa_attr_str() which returns a string similar to
  a string you might get on a Unix filesystem listing ("ls").
*/
bool
vcdinfo_has_xa(const vcdinfo_obj_t *obj) 
{
  return obj->has_xa;
}

/*!
  Add one to the MSF.
*/
void
vcdinfo_inc_msf (uint8_t *min, uint8_t *sec, int8_t *frame)
{
  (*frame)++;
  if (*frame>=CDIO_CD_FRAMES_PER_SEC) {
    *frame = 0;
    (*sec)++;
    if (*sec>=CDIO_CD_SECS_PER_MIN) {
      *sec = 0;
      (*min)++;
    }
  }
}

/*!
  Convert minutes, seconds and frame (MSF components) into a
  logical block address (or LBA). 
  See also cdio_msf_to_lba which uses msf_t as its single parameter.
*/
lba_t
vcdinfo_msf2lba (uint8_t min, uint8_t sec, int8_t frame)
{
  return CDIO_CD_FRAMES_PER_SEC*(CDIO_CD_SECS_PER_MIN*min + sec) + frame;
}

/*!
  Convert minutes, seconds and frame (MSF components) into a
  logical block address (or LBA). 
  See also cdio_msf_to_lba which uses msf_t as its single parameter.
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
  if (lba < CDIO_PREGAP_SECTORS) {
    vcd_error ("lba (%u) less than pregap sector (%u)", 
               (unsigned int) lba, CDIO_PREGAP_SECTORS);
    return lba;
  }
  return lba - CDIO_PREGAP_SECTORS;
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
    return "multi-default";
  case PSD_OFS_MULTI_DEF_NO_NUM:
    return "multi_def_no_num";
  default: ;
  }

  buf = _getbuf ();
  ofs = _vcdinfo_get_offset_t(obj, offset, ext);
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
      
      if (cdio_read_mode2_sectors (obj->img, (void *) obj->lot, LOT_VCD_SECTOR,
                                   false, LOT_VCD_SIZE))
        return false;
          
      if (cdio_read_mode2_sectors (obj->img, (void *) obj->psd, PSD_VCD_SECTOR,
                                   false, _vcd_len2blocks (psd_size, 
                                                           ISO_BLOCKSIZE)))
        return false;
  
    } else {
      return false;
    }
  return true;
}

void
vcdinfo_visit_lot (vcdinfo_obj_t *obj, bool extended)
{
  struct _vcdinf_pbc_ctx pbc_ctx;

  pbc_ctx.psd_size      = vcdinfo_get_psd_size (obj);
  pbc_ctx.psd_x_size    = obj->psd_x_size;
  pbc_ctx.offset_mult   = 8;
  pbc_ctx.maximum_lid   = vcdinfo_get_num_LIDs(obj);
  pbc_ctx.offset_x_list = NULL;
  pbc_ctx.offset_list   = NULL;
  pbc_ctx.psd           = obj->psd;
  pbc_ctx.psd_x         = obj->psd_x;
  pbc_ctx.lot           = obj->lot;
  pbc_ctx.lot_x         = obj->lot_x;
  pbc_ctx.extended      = extended;

  vcdinf_visit_lot(&pbc_ctx);
  if (NULL != obj->offset_x_list) 
    _vcd_list_free(obj->offset_x_list, true);
  obj->offset_x_list = pbc_ctx.offset_x_list;
  if (NULL != obj->offset_list) 
    _vcd_list_free(obj->offset_list, true);
  obj->offset_list   = pbc_ctx.offset_list;
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
   Nulls/zeros vcdinfo_obj_t structures; The caller should have 
   ensured that obj != NULL.
   routines using obj are called.
*/
static void
_vcdinfo_zero(vcdinfo_obj_t *obj)
{
  memset(obj, 0, sizeof(vcdinfo_obj_t));
  obj->vcd_type = VCD_TYPE_INVALID;
  obj->img = NULL;
  obj->lot = NULL;
  obj->source_name = NULL;
  obj->seg_sizes = NULL;
}

/*!
   Initialize the vcdinfo structure "obj". Should be done before other
   routines using obj are called.
*/
bool 
vcdinfo_init(vcdinfo_obj_t *obj)
{
  if (NULL == obj) return false;
  _vcdinfo_zero(obj);
  return cdio_init();
}

/*!
   Set up vcdinfo structure "obj" for reading from a particular
   medium. This should be done before after initialization but before
   any routines that need to retrieve data.
 
   source_name is the device or file to use for inspection, and
   source_type indicates what driver to use or class of drivers in the
   case of DRIVER_DEVICE.
   access_mode gives the CD access method for reading should the driver
   allow for more than one kind of access method (e.g. MMC versus ioctl
   on GNU/Linux)
    
   If source_name is NULL we'll fill in the appropriate default device
   name for the given source_type. However if in addtion source_type is
   DRIVER_UNKNOWN, then we'll scan for a drive containing a VCD.
    
   VCDINFO_OPEN_VCD is returned if everything went okay; 
   VCDINFO_OPEN_ERROR if there was an error and VCDINFO_OPEN_OTHER if the
   medium is something other than a VCD.
 */
vcdinfo_open_return_t
vcdinfo_open(vcdinfo_obj_t **obj_p, char *source_name[], 
             driver_id_t source_type, const char access_mode[])
{
  CdIo *img;
  vcdinfo_obj_t *obj = _vcd_malloc(sizeof(vcdinfo_obj_t));
  iso9660_stat_t *statbuf;


  /* If we don't specify a driver_id or a source_name, scan the
     system for a CD that contains a VCD.
   */
  if (NULL == *source_name && source_type == DRIVER_UNKNOWN) {
    char **cd_drives=NULL;
    cd_drives = cdio_get_devices_with_cap(NULL, 
                (CDIO_FS_ANAL_SVCD|CDIO_FS_ANAL_CVD|CDIO_FS_ANAL_VIDEOCD
                |CDIO_FS_UNKNOWN),
                                          true);
    if ( NULL == cd_drives || NULL == cd_drives[0] ) {
      return VCDINFO_OPEN_ERROR;
    }
    *source_name = strdup(cd_drives[0]);
    cdio_free_device_list(cd_drives);
  }

  img = cdio_open(*source_name, source_type);
  if (NULL == img) {
    return VCDINFO_OPEN_ERROR;
  }

  *obj_p = obj;
  
  if (access_mode != NULL) 
    cdio_set_arg (img, "access-mode", access_mode);

  if (NULL == *source_name) {
    *source_name = cdio_get_default_device(img);
    if (NULL == *source_name) return VCDINFO_OPEN_ERROR;
  }
    
  memset (obj, 0, sizeof (vcdinfo_obj_t));
  obj->img = img;  /* Note we do this after the above wipeout! */

  if (!read_pvd(obj->img, &(obj->pvd))) {
    return VCDINFO_OPEN_ERROR;
  }
  
  /* Determine if VCD has XA attributes. */
  {
    
    iso9660_pvd_t const *pvd = &obj->pvd;
    
    obj->has_xa = !strncmp ((char *) pvd + ISO_XA_MARKER_OFFSET, 
                            ISO_XA_MARKER_STRING, 
                           strlen (ISO_XA_MARKER_STRING));
  }
    
  if (!read_info(obj->img, &(obj->info), &(obj->vcd_type)))
    return VCDINFO_OPEN_OTHER;

  if (vcdinfo_get_format_version (obj) == VCD_TYPE_INVALID)
    return VCDINFO_OPEN_OTHER;

  if (!read_entries(obj->img, &(obj->entries)))
    return VCDINFO_OPEN_OTHER;
  
  {
    size_t len = strlen(*source_name)+1;
    obj->source_name = (char *) malloc(len * sizeof(char));
    strncpy(obj->source_name, *source_name, len);
  }

  if (obj->vcd_type == VCD_TYPE_SVCD || obj->vcd_type == VCD_TYPE_HQVCD) {
    statbuf = iso9660_fs_stat (obj->img, "MPEGAV", true);
    
    if (NULL != statbuf) {
      vcd_warn ("non compliant /MPEGAV folder detected!");
      free(statbuf);
    }
    

    statbuf = iso9660_fs_stat (obj->img, "SVCD/TRACKS.SVD;1", true);
    if (NULL != statbuf) {
      lsn_t lsn = statbuf->lsn;
      if (statbuf->size != ISO_BLOCKSIZE)
        vcd_warn ("TRACKS.SVD filesize != %d!", ISO_BLOCKSIZE);
      
      obj->tracks_buf = _vcd_malloc (ISO_BLOCKSIZE);

      free(statbuf);
      if (cdio_read_mode2_sector (obj->img, obj->tracks_buf, lsn, false))
        return VCDINFO_OPEN_ERROR;
    }
  }
      
  _init_segments (obj);

  switch (obj->vcd_type) {
  case VCD_TYPE_VCD2: {
    statbuf = iso9660_fs_stat (img, "EXT/PSD_X.VCD;1", true);
    if (NULL != statbuf) {
      lsn_t lsn        = statbuf->lsn;
      uint32_t secsize = statbuf->secsize;

      obj->psd_x       = _vcd_malloc (ISO_BLOCKSIZE * secsize);
      obj->psd_x_size  = statbuf->size;
      
      vcd_debug ("found /EXT/PSD_X.VCD at sector %lu", 
                 (long unsigned int) lsn);

      free(statbuf);
      if (cdio_read_mode2_sectors (img, obj->psd_x, lsn, false, secsize))
        return VCDINFO_OPEN_ERROR;
    }

    statbuf = iso9660_fs_stat (img, "EXT/LOT_X.VCD;1", true);
    if (NULL != statbuf) {
      lsn_t lsn        = statbuf->lsn;
      uint32_t secsize = statbuf->secsize;
      obj->lot_x       = _vcd_malloc (ISO_BLOCKSIZE * secsize);
      
      vcd_debug ("found /EXT/LOT_X.VCD at sector %lu", 
                 (unsigned long int) lsn);
        
      if (statbuf->size != LOT_VCD_SIZE * ISO_BLOCKSIZE)
        vcd_warn ("LOT_X.VCD size != 65535");

      free(statbuf);
      if (cdio_read_mode2_sectors (img, obj->lot_x, lsn, false, secsize))
        return VCDINFO_OPEN_ERROR;
      
    }
    break;
  }
  case VCD_TYPE_SVCD: 
  case VCD_TYPE_HQVCD: {
    statbuf = iso9660_fs_stat (img, "MPEGAV", true);
    if (NULL != statbuf) {
      vcd_warn ("non compliant /MPEGAV folder detected!");
      free(statbuf);
    }
    
    statbuf = iso9660_fs_stat (img, "SVCD/TRACKS.SVD;1", true);
    if (NULL == statbuf)
      vcd_warn ("mandatory /SVCD/TRACKS.SVD not found!");
    else {
      vcd_debug ("found TRACKS.SVD signature at sector %lu", 
                 (unsigned long int) statbuf->lsn);
      free(statbuf);
    }
    
    statbuf = iso9660_fs_stat (img, "SVCD/SEARCH.DAT;1", true);
    if (NULL == statbuf)
      vcd_warn ("mandatory /SVCD/SEARCH.DAT not found!");
    else {
      lsn_t    lsn       = statbuf->lsn;
      uint32_t secsize   = statbuf->secsize;
      uint32_t stat_size = statbuf->size;
      uint32_t size;

      vcd_debug ("found SEARCH.DAT at sector %lu", (unsigned long int) lsn);
      
      obj->search_buf = _vcd_malloc (ISO_BLOCKSIZE * secsize);
      
      if (cdio_read_mode2_sectors (img, obj->search_buf, lsn, false, secsize))
        return VCDINFO_OPEN_ERROR;
      
      size = (3 * uint16_from_be (((SearchDat *)obj->search_buf)->scan_points))
        + sizeof (SearchDat);

      free(statbuf);
      if (size > stat_size) {
        vcd_warn ("number of scanpoints leads to bigger size than "
                  "file size of SEARCH.DAT! -- rereading");
        
        free (obj->search_buf);
        obj->search_buf = _vcd_malloc (ISO_BLOCKSIZE 
                                       * _vcd_len2blocks(size, ISO_BLOCKSIZE));
        
        if (cdio_read_mode2_sectors (img, obj->search_buf, lsn, false, 
                                     secsize))
          return VCDINFO_OPEN_ERROR;
      }
    }
    break;
    }
  default:
    ;
  }

  statbuf = iso9660_fs_stat (img, "EXT/SCANDATA.DAT;1", true);
  if (statbuf != NULL) {
    lsn_t    lsn       = statbuf->lsn;
    uint32_t secsize   = statbuf->secsize;

    vcd_debug ("found /EXT/SCANDATA.DAT at sector %u", (unsigned int) lsn);
    
    obj->scandata_buf = _vcd_malloc (ISO_BLOCKSIZE * secsize);

    free(statbuf);
    if (cdio_read_mode2_sectors (img, obj->scandata_buf, lsn, false, secsize))
      return VCDINFO_OPEN_ERROR;
  }

  return VCDINFO_OPEN_VCD;
  
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
    if (obj->offset_list != NULL) 
      _vcd_list_free(obj->offset_list, true);
    if (obj->offset_x_list != NULL) 
      _vcd_list_free(obj->offset_x_list, true);
    free(obj->seg_sizes);
    free(obj->lot);
    free(obj->lot_x);
    free(obj->tracks_buf);
    free(obj->search_buf);
    free(obj->source_name);

    if (obj->img != NULL) cdio_destroy (obj->img);
    _vcdinfo_zero(obj);
  }
  
  free(obj);
  return(true);
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
