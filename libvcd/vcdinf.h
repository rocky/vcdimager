/*!
   \file vcdinf.h

    Copyright (C) 2002,2003 Rocky Bernstein <rocky@panix.com>

 \verbatim
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

    Like vcdinfo but exposes more of the internal structure. It is probably
    better to use vcdinfo, when possible.
 \endverbatim
*/

#ifndef _VCDINF_H
#define _VCDINF_H

#include <libvcd/vcd_iso9660.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

  /*!
    Comparison routine used in sorting. We compare LIDs and if those are 
    equal, use the offset.
    Note: we assume an unassigned LID is 0 and this compares as a high value.

    NOTE: Consider making static.
  */
  int vcdinf_lid_t_cmp (vcdinfo_offset_t *a, vcdinfo_offset_t *b);

  /*!
    Return a string containing the VCD album id.
  */
  const char * vcdinf_get_album_id(const InfoVcd *info);

  /*!
    Return the VCD application ID.
    NULL is returned if there is some problem in getting this. 
  */
  const char * vcdinf_get_application_id(const pvd_t *pvd);

  /*!
    Return the base selection number. VCD_INVALID_BSN is returned if there
    is an error.
  */
  unsigned int vcdinf_get_bsn(const PsdSelectionListDescriptor *psd);
  
  /**
   * \fn vcdinfo_get_default_from_psd(const PsdSelectionListDescriptor *psd);
   * \brief Get next offset for a given PSD selector descriptor. 
   * \return VCDINFO_INVALID_OFFSET is returned on error or if psd is
   * NULL. Otherwise the LID offset is returned.
   */
  lid_t vcdinf_get_default_from_psd(const PsdSelectionListDescriptor *psd);

  /*!  Return the starting LBA (logical block address) for sequence
    entry_num in obj.  VCDINFO_NULL_LBA is returned if there is no entry.
    The first entry number is 0.
  */
  lba_t vcdinf_get_entry_lba(const EntriesVcd *entries, 
			     unsigned int entry_num);

  /*!  Return the starting MSF (minutes/secs/frames) for sequence
    entry_num in obj.  NULL is returned if there is no entry.
    The first entry number is 0.
  */
  const msf_t * vcdinf_get_entry_msf(const EntriesVcd *entries, 
				     unsigned int entry_num);

  const char * vcdinf_get_format_version_str (vcd_type_t vcd_type);

  /*!
    Return true if item is to be looped. 
  */
  uint16_t vcdinf_get_loop_count (const PsdSelectionListDescriptor *d);
  
  /**
     \fn vcdinfo_get_next_from_pld(const PsdPlayListDescriptor *pld); 
     \brief  Get next offset for a given PSD selector descriptor.  
     \return  VCDINFO_INVALID_OFFSET is returned on error or if pld has no 
     "next" entry or pld is NULL. Otherwise the LID offset is returned.
  */
  lid_t vcdinf_get_next_from_pld(const PsdPlayListDescriptor *pld);
  
  /**
     \fn vcdinfo_get_next_from_psd(const PsdSelectionListDescriptor *psd);
     \brief Get next offset for a given PSD selector descriptor. 
     \return  VCDINFO_INVALID_OFFSET is returned on error or if psd has no 
     "next" entry or psd is NULL. Otherwise the LID offset is returned.
  */
  lid_t vcdinf_get_next_from_psd(const PsdSelectionListDescriptor *psd);
  
  /*!
    Return the number of segments in the VCD. 
  */
  unsigned int vcdinf_get_num_entries(const EntriesVcd *entries);

  /*!
    Return number of LIDs. 
  */
  lid_t vcdinf_get_num_LIDs (const InfoVcd *info);

  /*!
    Return the number of segments in the VCD. 
  */
  unsigned int vcdinf_get_num_segments(const InfoVcd *info);

  /*!
    Return the playlist item i in d. 
  */
  uint16_t vcdinf_get_play_item_from_pld(const PsdPlayListDescriptor *pld, 
					 unsigned int i);

  /*!
    Get play-time value for PsdPlayListDescriptor *d.
    Time is in 1/15-second units.
  */
  uint16_t vcdinf_get_play_time (const PsdPlayListDescriptor *d);
  
  /*!
    Return a string containing the VCD preparer id with trailing
    blanks removed.
    NULL is returned if there is some problem in getting this. 
  */
  const char * vcdinf_get_preparer_id(const pvd_t *pvd);

  /**
     \fn vcdinf_get_prev_from_psd(const PsdSelectionListDescriptor *pld);
     \brief Get prev offset for a given PSD selector descriptor. 
     \return  VCDINFO_INVALID_OFFSET is returned on error or if pld has no 
     "prev" entry or pld is NULL. Otherwise the LID offset is returned.
  */
  lid_t vcdinf_get_prev_from_pld(const PsdPlayListDescriptor *pld);
  
  /**
     \fn vcdinf_get_prev_from_psd(const PsdPlayListDescriptor *psd);
     \brief Get prev offset for a given PSD selector descriptor. 
     \return  VCDINFO_INVALID_OFFSET is returned on error or if psd has no 
     "prev"
     entry or psd is NULL. Otherwise the LID offset is returned.
  */
  lid_t vcdinf_get_prev_from_psd(const PsdSelectionListDescriptor *psd);
  
  /*!
    Return a string containing the VCD publisher id with trailing
    blanks removed.
    NULL is returned if there is some problem in getting this. 
  */
  const char * vcdinf_get_publisher_id(const pvd_t *pvd);
    
  /*!
    Return number of bytes in PSD. 
  */
  uint32_t vcdinf_get_psd_size (const InfoVcd *info);
       

  /**
     \fn vcdinfo_get_return_from_pld(const PsdPlayListDescriptor *pld);
     \brief Get return offset for a given PLD selector descriptor. 
     \return  VCDINFO_INVALID_OFFSET is returned on error or if pld has no 
     "return" entry or pld is NULL. Otherwise the LID offset is returned.
  */
  lid_t vcdinf_get_return_from_pld(const PsdPlayListDescriptor *pld);

  /**
   * \fn vcdinfo_get_return_from_psd(const PsdSelectionListDescriptor *psd);
   * \brief Get return offset for a given PSD selector descriptor. 
   \return  VCDINFO_INVALID_OFFSET is returned on error or if psd has no 
   "return" entry or psd is NULL. Otherwise the LID offset is returned.
  */
  uint16_t vcdinf_get_return_from_psd(const PsdSelectionListDescriptor *psd);

  /*!
    Return a string containing the VCD system id with trailing
    blanks removed.
    NULL is returned if there is some problem in getting this. 
  */
  const char * vcdinf_get_system_id(const pvd_t *pvd);
  
  /*!
    Return the track number for entry n in obj. The first track starts
    at 1. 
  */
  track_t vcdinf_get_track(const EntriesVcd *entries, 
			   const unsigned int entry_num);

  /*!
    Return the VCD volume count - the number of CD's in the collection.
  */
  unsigned int vcdinf_get_volume_count(const InfoVcd *info);

  /*!
    Return the VCD ID.
    NULL is returned if there is some problem in getting this. 
  */
  const char * vcdinf_get_volume_id(const pvd_t *pvd);

  /*!
    Return the VCD volumeset ID.
    NULL is returned if there is some problem in getting this. 
  */
  const char * vcdinf_get_volumeset_id(const pvd_t *pvd);

  /*!
    Return the VCD volume num - the number of the CD in the collection.
    This is a number between 1 and the volume count.
  */
  unsigned int vcdinf_get_volume_num(const InfoVcd *info);
  
  int vcdinf_get_wait_time (uint16_t wtime);

  struct _vcdinf_pbc_ctx {
    unsigned int psd_size;
    lid_t maximum_lid;
    unsigned offset_mult;
    VcdList *offset_x_list;
    VcdList *offset_list;
    
    LotVcd *lot;
    LotVcd *lot_x;
    uint8_t *psd;
    uint8_t *psd_x;
    unsigned int psd_x_size;
    bool extended;
  };

  /*!
     Calls recursive routine to populate obj->offset_list or obj->offset_x_list
     by going through LOT.
  */
  void vcdinf_visit_lot (struct _vcdinf_pbc_ctx *obj);
  
  /*! 
     Recursive routine to populate obj->offset_list or obj->offset_x_list
     by reading playback control entries referred to via lid.
  */
  void vcdinf_visit_pbc (struct _vcdinf_pbc_ctx *obj, lid_t lid, 
			 unsigned int offset, bool in_lot);

  /*!
    Return true if loop has a jump delay
  */
  bool vcdinf_has_jump_delay (const PsdSelectionListDescriptor *psd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_VCDINF_H*/
