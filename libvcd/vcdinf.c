/*
    $Id$

    Copyright (C) 2003 Rocky Bernstein <rocky@panix.com>

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
   Like vcdinfo but exposes more of the internal structure. It is probably
   better to use vcdinfo, when possible.
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
#include <libvcd/vcd_image_nrg.h>
#include <libvcd/vcd_image_cd.h>
#include <libvcd/vcd_image_fs.h>
#include <libvcd/vcd_xa.h>
#include <libvcd/vcd_cd_sector.h>

/* Eventually move above libvcd includes but having vcdinfo including. */
#include "vcdinfo.h"
#include "vcdinf.h"

static const char _rcsid[] = "$Id$";

/* Comparison routine used in sorting. We compare LIDs and if those are 
   equal, use the offset.
   Note: we assume an unnassigned LID is 0 and this compares as a high value.
*/
static int
_lid_t_cmp (vcdinfo_offset_t *a, vcdinfo_offset_t *b)
{
  if (a->lid && b->lid)
    {
      if (a->lid > b->lid) return +1;
      if (a->lid < b->lid) return -1;
      vcd_warn ("LID %d at offset %d has same nunber as LID of offset %d", 
                a->lid, a->offset, b->offset);
    } 
  else if (a->lid) return -1;
  else if (b->lid) return +1;

  /* Failed to sort on LID, try offset now. */

  if (a->offset > b->offset) return +1;
  if (a->offset < b->offset) return -1;
  
  /* LIDS and offsets are equal. */
  return 0;
}

/*
  This fills in unassigned LIDs in the offset table.  Due to
  "rejected" LOT entries, some of these might not have gotten filled
  in while scanning PBC (if in fact there even was a PBC).

  Note: We assume that an unassigned LID is one whose value is 0.
 */
static void
vcdinf_update_offset_list(struct _vcdinf_pbc_ctx *obj, bool extended)
{
  if (NULL==obj) return;
  {
    VcdListNode *node;
    VcdList *unused_lids = _vcd_list_new();
    VcdListNode *next_unused_node = _vcd_list_begin(unused_lids);
    
    unsigned int last_lid=0;
    VcdList *offset_list = extended ? obj->offset_x_list : obj->offset_list;
    
    lid_t max_seen_lid=0;

    _VCD_LIST_FOREACH (node, offset_list)
      {
        vcdinfo_offset_t *ofs = _vcd_list_node_data (node);
        if (!ofs->lid) {
          /* We have a customer! Assign a LID from the free pool
             or take one from the end if no skipped LIDs.
          */
          VcdListNode *node=_vcd_list_node_next(next_unused_node);
          if (node != NULL) {
            lid_t *next_unused_lid=_vcd_list_node_data(node);
            ofs->lid = *next_unused_lid;
            next_unused_node=node;
          } else {
            max_seen_lid++;
            ofs->lid = max_seen_lid;
          }
        } else {
          /* See if we've skipped any LID numbers. */
          last_lid++;
          while (last_lid != ofs->lid ) {
            lid_t * lid=_vcd_malloc (sizeof(lid_t));
            *lid = last_lid;
            _vcd_list_append(unused_lids, lid);
          }
          if (last_lid > max_seen_lid) max_seen_lid=last_lid;
        }
      }
    _vcd_list_free(unused_lids, true);
  }
}

/*!
   Calls recursive routine to populate obj->offset_list or obj->offset_x_list
   by going through LOT.
*/
void
vcdinf_visit_lot (struct _vcdinf_pbc_ctx *obj)
{
  const LotVcd *lot = obj->extended ? obj->lot_x : obj->lot;
  unsigned int n, tmp;

  if (obj->extended) {
    if (!obj->psd_x_size) return;
  } else if (!obj->psd_size) return;

  for (n = 0; n < LOT_VCD_OFFSETS; n++)
    if ((tmp = uint16_from_be (lot->offset[n])) != PSD_OFS_DISABLED)
      vcdinf_visit_pbc (obj, n + 1, tmp, true);

  _vcd_list_sort (obj->extended ? obj->offset_x_list : obj->offset_list, 
                  (_vcd_list_cmp_func) _lid_t_cmp);

  /* Now really complete the offset table with LIDs.  This routine
     might obviate the need for vcdinf_visit_pbc() or some of it which is
     more complex. */
  vcdinf_update_offset_list(obj, obj->extended);
}

/*!
   Recursive routine to populate obj->offset_list or obj->offset_x_list
   by reading playback control entries referred to via lid.
*/
void
vcdinf_visit_pbc (struct _vcdinf_pbc_ctx *obj, lid_t lid, unsigned int offset, 
                  bool in_lot)
{
  VcdListNode *node;
  vcdinfo_offset_t *ofs;
  unsigned psd_size  = obj->extended ? obj->psd_x_size : obj->psd_size;
  const uint8_t *psd = obj->extended ? obj->psd_x : obj->psd;
  unsigned int _rofs = offset * obj->offset_mult;
  VcdList *offset_list;

  vcd_assert (psd_size % 8 == 0);

  switch (offset)
    {
    case PSD_OFS_DISABLED:
    case PSD_OFS_MULTI_DEF:
    case PSD_OFS_MULTI_DEF_NO_NUM:
      return;

    default:
      break;
    }

  if (_rofs >= psd_size)
    {
      if (obj->extended)
	vcd_error ("psd offset out of range in extended PSD"
		   " (try --no-ext-psd option)");
      else
        vcd_warn ("psd offset out of range (%d >= %d)", _rofs, psd_size);
      return;
    }

  vcd_assert (_rofs < psd_size);

  if (!obj->offset_list)
    obj->offset_list = _vcd_list_new ();

  if (!obj->offset_x_list)
    obj->offset_x_list = _vcd_list_new ();

  if (obj->extended) {
    offset_list = obj->offset_x_list;
  } else 
    offset_list = obj->offset_list;

  _VCD_LIST_FOREACH (node, offset_list)
    {
      ofs = _vcd_list_node_data (node);

      if (offset == ofs->offset)
        {
          if (in_lot)
            ofs->in_lot = true;

          if (lid) {
            /* Our caller thinks she knows what our LID is.
               This should help out getting the LID for end descriptors
               if not other things as well.
             */
            ofs->lid = lid;
          }
          

          ofs->ext = obj->extended;

          return; /* already been there... */
        }
    }

  ofs = _vcd_malloc (sizeof (vcdinfo_offset_t));

  ofs->ext    = obj->extended;
  ofs->in_lot = in_lot;
  ofs->lid    = lid;
  ofs->offset = offset;

  switch (psd[_rofs])
    {
    case PSD_TYPE_PLAY_LIST:
      _vcd_list_append (offset_list, ofs);
      {
        const PsdPlayListDescriptor *d = (const void *) (psd + _rofs);
        const lid_t lid = vcdinfo_get_lid_from_pld(d);

        if (!ofs->lid)
          ofs->lid = lid;
        else 
          if (ofs->lid != lid)
            vcd_warn ("LOT entry assigned LID %d, but descriptor has LID %d",
                      ofs->lid, lid);

        vcdinf_visit_pbc (obj, 0, vcdinf_get_prev_from_pld(d), false);
        vcdinf_visit_pbc (obj, 0, vcdinf_get_next_from_pld(d), false);
        vcdinf_visit_pbc (obj, 0, vcdinf_get_return_from_pld(d), false);
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

        vcdinf_visit_pbc (obj, 0, vcdinf_get_prev_from_psd(d), false);
        vcdinf_visit_pbc (obj, 0, vcdinf_get_next_from_psd(d), false);
        vcdinf_visit_pbc (obj, 0, vcdinf_get_return_from_psd(d), false);
        vcdinf_visit_pbc (obj, 0, vcdinf_get_default_from_psd(d), false);
        vcdinf_visit_pbc (obj, 0, uint16_from_be (d->timeout_ofs), false);

        for (idx = 0; idx < d->nos; idx++)
          vcdinf_visit_pbc (obj, 0, uint16_from_be (d->ofs[idx]), false);
        
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
   Return a string containing the VCD album id, or NULL if there is 
   some problem in getting this. 
*/
const char *
vcdinf_get_album_id(const InfoVcd *info)
{
  if (NULL==info) return NULL;
  return vcdinfo_strip_trail (info->album_desc, MAX_ALBUM_LEN);
}

/*!
  Return the VCD application ID.
  NULL is returned if there is some problem in getting this. 
*/
const char * 
vcdinf_get_application_id(const pvd_t *pvd)
{
  if (NULL==pvd) return NULL;
  return(vcdinfo_strip_trail(pvd->application_id, MAX_APPLICATION_ID));
}

/*!
  Return the base selection number. VCD_INVALID_BSN is returned if there
  is an error.
*/
unsigned int
vcdinf_get_bsn(const PsdSelectionListDescriptor *psd)
{
  if (NULL==psd) return VCDINFO_INVALID_BSN;
  return(psd->bsn);
}

/**
 * \fn vcdinfo_get_default_from_psd(const PsdSelectionListDescriptor *psd);
 * \brief Get next offset for a given PSD selector descriptor. 
 * \return VCDINFO_INVALID_OFFSET is returned on error or if psd is
 * NULL. Otherwise the LID offset is returned.
 */
lid_t
vcdinf_get_default_from_psd(const PsdSelectionListDescriptor *psd)
{
  if (NULL == psd) return VCDINFO_INVALID_OFFSET;
  return uint16_from_be (psd->default_ofs);
}

/*!  Return the starting LBA (logical block address) for sequence
  entry_num in obj.  VCDINFO_NULL_LBA is returned if there is no entry.
*/
lba_t
vcdinf_get_entry_lba(const EntriesVcd *entries, unsigned int entry_num)
{
  const msf_t *msf = vcdinf_get_entry_msf(entries, entry_num);
  return (msf != NULL) ? msf_to_lba(msf) : VCDINFO_NULL_LBA;
}

/*!  Return the starting MSF (minutes/secs/frames) for sequence
  entry_num in obj.  NULL is returned if there is no entry.
  The first entry number is 0.
*/
const msf_t *
vcdinf_get_entry_msf(const EntriesVcd *entries, unsigned int entry_num)
{
  const unsigned int entry_count = uint16_from_be (entries->entry_count);
  return entry_num < entry_count ?
    &(entries->entry[entry_num].msf)
    : NULL;
}

/*!
   Return a string giving VCD format (VCD 1.0 VCD 1.1, SVCD, ... 
   for this object.
*/
const char *
vcdinf_get_format_version_str (vcd_type_t vcd_type) 
{
  switch (vcd_type)
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
  Return number of times PLD loops.
*/
uint16_t
vcdinf_get_loop_count (const PsdSelectionListDescriptor *d) 
{
  return 0x7f & d->loop;
}

/**
 \fn vcdinfo_get_next_from_pld(const PsdPlayListDescriptor *pld); 
 \brief  Get next offset for a given PSD selector descriptor.  
 \return  VCDINFO_INVALID_OFFSET is returned on error or if pld has no "next"
 entry or pld is NULL. Otherwise the LID offset is returned.
 */
lid_t
vcdinf_get_next_from_pld(const PsdPlayListDescriptor *pld)
{
  if (NULL == pld) return VCDINFO_INVALID_OFFSET;
  return uint16_from_be (pld->next_ofs);
}

/**
 * \fn vcdinfo_get_next_from_psd(const PsdSelectionListDescriptor *psd);
 * \brief Get next offset for a given PSD selector descriptor. 
 * \return VCDINFO_INVALID_OFFSET is returned on error or if psd is
 * NULL. Otherwise the LID offset is returned.
 */
lid_t
vcdinf_get_next_from_psd(const PsdSelectionListDescriptor *psd)
{
  if (NULL == psd) return VCDINFO_INVALID_OFFSET;
  return uint16_from_be (psd->next_ofs);
}

/*!
  Return the number of entries in the VCD.
*/
unsigned int
vcdinf_get_num_entries(const EntriesVcd *entries)
{
  if (NULL==entries) return 0;
  return (uint16_from_be (entries->entry_count));
}

/*!
  Return the number of segments in the VCD. 
*/
unsigned int
vcdinf_get_num_segments(const InfoVcd *info)
{
  if (NULL==info) return 0;
  return (uint16_from_be (info->item_count));
}

/*!
  Return number of LIDs. 
*/
lid_t
vcdinf_get_num_LIDs (const InfoVcd *info) 
{
  if (NULL==info) return 0;
  /* Should probably use _vcd_pbc_max_lid instead? */
  return uint16_from_be (info->lot_entries);
}

/*!
   Return a string containing the VCD publisher id with trailing
   blanks removed.
*/
const char *
vcdinf_get_preparer_id(const pvd_t *pvd)
{
  if (NULL==pvd) return NULL;
  return(vcdinfo_strip_trail(pvd->preparer_id, MAX_PREPARER_ID));
}

/**
 \fn vcdinf_get_prev_from_pld(const PsdPlayListDescriptor *pld);
 \brief Get prev offset for a given PSD selector descriptor. 
 \return  VCDINFO_INVALID_OFFSET is returned on error or if pld has no "prev"
 entry or pld is NULL. Otherwise the LID offset is returned.
 */
lid_t
vcdinf_get_prev_from_pld(const PsdPlayListDescriptor *pld)
{
  return (pld != NULL) ? 
    uint16_from_be (pld->prev_ofs) : VCDINFO_INVALID_OFFSET;
}

/**
 \fn vcdinf_get_prev_from_psd(const PsdSelectionListDescriptor *psd);
 \brief Get prev offset for a given PSD selector descriptor. 
 \return  VCDINFO_INVALID_OFFSET is returned on error or if psd has no "prev"
 entry or psd is NULL. Otherwise the LID offset is returned.
 */
lid_t
vcdinf_get_prev_from_psd(const PsdSelectionListDescriptor *psd)
{
  return (psd != NULL) ? 
    uint16_from_be (psd->prev_ofs) : VCDINFO_INVALID_OFFSET;
}

/*!
  Return number of bytes in PSD. 
*/
uint32_t
vcdinf_get_psd_size (const InfoVcd *info)
{
  if (NULL==info) return 0;
  return uint32_from_be (info->psd_size);
}

/*!
   Return a string containing the VCD publisher id with trailing
   blanks removed.
*/
const char *
vcdinf_get_publisher_id(const pvd_t *pvd)
{
  if (NULL==pvd) return NULL;
  return(vcdinfo_strip_trail(pvd->publisher_id, MAX_PUBLISHER_ID));
}

/**
 \fn vcdinf_get_return_from_pld(const PsdPlayListDescriptor *pld);
 \brief Get return offset for a given PLD selector descriptor. 
 \return  VCDINFO_INVALID_OFFSET is returned on error or if pld has no 
 "return" entry or pld is NULL. Otherwise the LID offset is returned.
 */
lid_t
vcdinf_get_return_from_pld(const PsdPlayListDescriptor *pld)
{
  return (pld != NULL) ? 
    uint16_from_be (pld->return_ofs) : VCDINFO_INVALID_OFFSET;
}

/**
 * \fn vcdinfo_get_return_from_psd(const PsdSelectionListDescriptor *psd);
 * \brief Get return offset for a given PSD selector descriptor. 
 \return  VCDINFO_INVALID_OFFSET is returned on error or if psd has no 
 "return" entry or psd is NULL. Otherwise the LID offset is returned.
 */
uint16_t
vcdinf_get_return_from_psd(const PsdSelectionListDescriptor *psd)
{
  return (psd != NULL) ? 
    uint16_from_be (psd->return_ofs) : VCDINFO_INVALID_OFFSET;
}

/*!
   Return a string containing the VCD system id with trailing
   blanks removed.
*/
const char *
vcdinf_get_system_id(const pvd_t *pvd)
{
  if (NULL==pvd) return NULL;
  return(vcdinfo_strip_trail(pvd->system_id, MAX_SYSTEM_ID));
}

/*!
  Return the track number for entry n in obj. The first track starts
  at 1. Note this is one less than the track number reported in vcddump.
  (We don't count the header track?)
*/
track_t
vcdinf_get_track(const EntriesVcd *entries, const unsigned int entry_num)
{
  const unsigned int entry_count = uint16_from_be (entries->entry_count);
  /* Note entry_num is 0 origin. */
  return entry_num < entry_count ?
    from_bcd8 (entries->entry[entry_num].n):
    VCDINFO_INVALID_TRACK;
}

/*!
  Return the VCD volume count - the number of CD's in the collection.
*/
unsigned int 
vcdinf_get_volume_count(const InfoVcd *info) 
{
  if (NULL==info) return 0;
  return(uint16_from_be( info->vol_count));
}

/*!
  Return the VCD ID.
*/
const char *
vcdinf_get_volume_id(const pvd_t *pvd) 
{
  if (NULL == pvd) return NULL;
  return(vcdinfo_strip_trail(pvd->volume_id, MAX_VOLUME_ID));
}

/*!
  Return the VCD volumeset ID.
  NULL is returned if there is some problem in getting this. 
*/
const char *
vcdinf_get_volumeset_id(const pvd_t *pvd)
{
  if ( NULL == pvd ) return NULL;
  return vcdinfo_strip_trail(pvd->volume_set_id, MAX_VOLUMESET_ID);
}

/*!
  Return the VCD volume num - the number of the CD in the collection.
  This is a number between 1 and the volume count.
*/
unsigned int
vcdinf_get_volume_num(const InfoVcd *info)
{
  if (NULL == info) return 0;
  return uint16_from_be(info->vol_id);
}

int
vcdinf_get_wait_time (uint16_t wtime)
{
  /* Note: this doesn't agree exactly with _wtime */
  if (wtime < 61)
    return wtime;
  else if (wtime < 255)
    return (wtime - 60) * 10 + 60;
  else
    return -1;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
