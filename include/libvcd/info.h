/*!
   \file info.h

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
 \endverbatim
*/
/* 
   Things here refer to higher-level structures usually accessed via
   vcdinfo_t. For lower-level access which generally use 
   structures other than vcdinfo_t, see inf.h
*/


#ifndef _VCD_INFO_H
#define _VCD_INFO_H

#include <libvcd/types.h>
#include <libvcd/files.h>
#include <cdio/cdio.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*========== Move somewhere else? ================*/

/*! \def Max # characters in an album id. */
#define MAX_ALBUM_LEN 16   
#define MAX_APPLICATION_ID 128
#define MAX_PREPARER_ID 128
#define MAX_PUBLISHER_ID 128
#define MAX_SYSTEM_ID 32
#define MAX_VOLUME_ID 32
#define MAX_VOLUMESET_ID 128

/*!
  Invalid LBA, Note: VCD player uses the fact that this is a very high
  value.
 */
#define VCDINFO_NULL_LBA          CDIO_INVALID_LBA

/*!
  Invalid LSN, Note: VCD player uses the fact that this is a very high
  value.
 */
#define VCDINFO_NULL_LSN          VCDINFO_NULL_LBA

/*========== End move somewhere else? ================*/

/*! 
  Portion of uint16_t which determines whether offset is
  rejected or not. 
*/
#define VCDINFO_REJECTED_MASK (0x8000)

/*!
  Portion of uint16_t which contains the offset.
*/
#define VCDINFO_OFFSET_MASK (VCDINFO_REJECTED_MASK-1)

/*! 
  Portion of uint16_t which contains the lid.
*/
#define VCDINFO_LID_MASK    (VCDINFO_REJECTED_MASK-1)

/*! 
  Constant for invalid track number
*/
#define VCDINFO_INVALID_TRACK   0xFF

/*! 
  Constant for invalid LID offset.
*/
#define VCDINFO_INVALID_OFFSET  0xFFFF

/*! 
  Constant for ending or "leadout" track.
*/
#define VCDINFO_LEADOUT_TRACK  0xaa

/*! 
  Constant for invalid sequence entry.
*/
#define VCDINFO_INVALID_ENTRY  0xFFFF

/*! 
  Constant for invalid LID. 
  FIXME: player needs these to be the same. 
  VCDimager code requres 0 for an UNINITIALIZED LID.
  
*/
#define VCDINFO_INVALID_LID  VCDINFO_INVALID_ENTRY
#define VCDINFO_UNINIT_LID   0

/*! 
  Constant for invalid itemid
*/
#define VCDINFO_INVALID_ITEMID  0xFFFF

/*! 
  Constant for invalid audio type
*/
#define VCDINFO_INVALID_AUDIO_TYPE  4

/*! 
  Constant for invalid base selection number (BSN)
*/
#define VCDINFO_INVALID_BSN  200

/* The number of sectors allocated in a Video CD segment is a fixed: 150.
   
   NOTE: The actual number of sectors used is often less and can sometimes
   be gleaned by looking at the correspoinding ISO 9660 file entry (or
   by scanning the MPEG segment which may be slow).
   Some media players get confused by or complain about padding at the end
   a segment.
*/
#define VCDINFO_SEGMENT_SECTOR_SIZE 150

  /* Opaque type used in most routines below. */
  typedef struct _VcdInfo vcdinfo_obj_t;

  /* See enum in vcd_files_private.h */
  typedef enum {
    VCDINFO_FILES_VIDEO_NOSTREAM = 0,   
    VCDINFO_FILES_VIDEO_NTSC_STILL = 1,   
    VCDINFO_FILES_VIDEO_NTSC_STILL2 = 2,  /* lo+hires*/
    VCDINFO_FILES_VIDEO_NTSC_MOTION = 3,
    VCDINFO_FILES_VIDEO_PAL_STILL = 5,    
    VCDINFO_FILES_VIDEO_PAL_STILL2 = 6,   /* lo+hires*/
    VCDINFO_FILES_VIDEO_PAL_MOTION = 7,
    VCDINFO_FILES_VIDEO_INVALID = 8
  } vcdinfo_video_segment_type_t;
  
  /*!
    Used in working with LOT - list of offsets and lid's 
  */
  typedef struct {
    uint8_t type;
    lid_t lid;
    uint16_t offset;
    bool in_lot;   /* Is listed in LOT. */
    bool ext;      /* True if entry comes from offset_x_list. */
  } vcdinfo_offset_t;
  
  /*!
    The kind of entry associated with an selection-item id 
  */
  /* See corresponding enum in vcd_pbc.h. */
  typedef enum {
    VCDINFO_ITEM_TYPE_TRACK,
    VCDINFO_ITEM_TYPE_ENTRY,
    VCDINFO_ITEM_TYPE_SEGMENT,
    VCDINFO_ITEM_TYPE_LID,
    VCDINFO_ITEM_TYPE_SPAREID2,
    VCDINFO_ITEM_TYPE_NOTFOUND
  } vcdinfo_item_enum_t;
  
  typedef struct {
    uint16_t num;
    vcdinfo_item_enum_t type;
  } vcdinfo_itemid_t;
  
  typedef enum {
    VCDINFO_OPEN_ERROR,          /* Error */
    VCDINFO_OPEN_VCD,            /* Is VCD of some sort */
    VCDINFO_OPEN_OTHER           /* Is not VCD but something else */
  } vcdinfo_open_return_t;
  
  typedef struct 
  {
    
    psd_descriptor_types descriptor_type;
    /* Only one of pld or psd is used below. Not all
       C compiler accept the anonymous unions commented out below. */
    /* union  { */
    PsdPlayListDescriptor *pld;
    PsdSelectionListDescriptor *psd;
    /* }; */
    
  } PsdListDescriptor;
  
  /*!
    Return the number of audio channels implied by "audio_type".
    0 is returned on error.
  */
  unsigned int
  vcdinfo_audio_type_num_channels(const vcdinfo_obj_t *obj, 
				  unsigned int audio_type);
  
  /*!
    Return a string describing an audio type.
  */
  const char * vcdinfo_audio_type2str(const vcdinfo_obj_t *obj,
				      unsigned int audio_type);
  
  /*!
    Note first seg_num is 0!
  */
  const char * 
  vcdinfo_ogt2str(const vcdinfo_obj_t *obj, segnum_t seg_num);
  
  /*!
    Note first seg_num is 0!
  */
  const char * 
  vcdinfo_video_type2str(const vcdinfo_obj_t *obj, segnum_t seg_num);
  
  const char *
  vcdinfo_pin2str (uint16_t itemid);
  
  /*!
    \brief Classify itemid_num into the kind of item it is: track #, entry #, 
    segment #. 
    \param itemid is set to contain this classifcation an the converted 
    entry number. 
  */
  void
  vcdinfo_classify_itemid (uint16_t itemid_num, 
			   /*out*/ vcdinfo_itemid_t *itemid);
  
  /*!
    Return a string containing the VCD album id, or NULL if there is 
    some problem in getting this. 
  */
  const char *
  vcdinfo_get_album_id(const vcdinfo_obj_t *obj);
  
  /*!
    Return the VCD application ID.
    NULL is returned if there is some problem in getting this. 
  */
  const char *
  vcdinfo_get_application_id(const vcdinfo_obj_t *obj);
  
  /*!
    Return a pointer to the cdio structure for the CD image opened or
    NULL if error.
  */
  CdIo *
  vcdinfo_get_cd_image (const vcdinfo_obj_t *vcd_obj);
  
  /*!
    Return a string containing the default VCD device if none is specified.
    This might be something like "/dev/cdrom" on Linux or 
    "/vol/dev/aliases/cdrom0" on Solaris,  or maybe "VIDEOCD.CUE" for 
    if bin/cue I/O routines are in effect. 
    
    Return NULL we can't get this information.
  */
  char *
  vcdinfo_get_default_device (const vcdinfo_obj_t *vcd_obj);
  
  /*!
    \brief Get default LID offset. 

    Return the LID offset associated with a the "default" entry of the
    passed-in LID parameter. Note "default" entries are associated with
    PSDs that are (extended) selection lists.

    \return VCDINFO_INVALID_OFFSET is returned on error, or if the LID
    is not a selection list or no "default" entry. Otherwise the LID
    offset is returned.
  */
  uint16_t
  vcdinfo_get_default_offset(const vcdinfo_obj_t *obj, lid_t lid);
  
  /*!
    Return number of sector units in of an entry. 0 is returned if
    entry_num is invalid.
  */
  uint32_t
  vcdinfo_get_entry_sect_count (const vcdinfo_obj_t *obj, 
				unsigned int entry_num);
  
  /*!  Return the starting LBA (logical block address) for sequence
    entry_num in obj.  VCDINFO_NULL_LBA is returned if there is no entry.
    The first entry number is 0.
  */
  lba_t
  vcdinfo_get_entry_lba(const vcdinfo_obj_t *obj, unsigned int entry_num);
  
  /*!  Return the starting LSN (logical sector number) for sequence
    entry_num in obj.  VCDINFO_NULL_LSN is returned if there is no entry.
    The first entry number is 0.
  */
  lsn_t
  vcdinfo_get_entry_lsn(const vcdinfo_obj_t *obj, unsigned int entry_num);
  
  /*!  Return the starting MSF (minutes/secs/frames) for sequence
    entry_num in obj.  NULL is returned if there is no entry.
    The first entry number is 0.
  */
  const msf_t *
  vcdinfo_get_entry_msf(const vcdinfo_obj_t *obj, unsigned int entry_num);

  /*!
    Get the VCD format (VCD 1.0 VCD 1.1, SVCD, ... for this object.
    The type is also set inside obj.
    The first entry number is 0.
  */
  vcd_type_t 
  vcdinfo_get_format_version (vcdinfo_obj_t *obj);
  
  /*!
    Return a string giving VCD format (VCD 1.0 VCD 1.1, SVCD, ... 
    for this object.
  */
  const char * 
  vcdinfo_get_format_version_str (const vcdinfo_obj_t *obj);
  
  EntriesVcd * vcdinfo_get_entriesVcd (vcdinfo_obj_t *obj);
  
  InfoVcd * vcdinfo_get_infoVcd (vcdinfo_obj_t *obj);

  /*!
    \brief Get default or multi-default LID. 

    Return the LID offset associated with a the "default" entry of the
    passed-in LID parameter. Note "default" entries are associated
    with PSDs that are (extended) selection lists. If the "default"
    is a multi-default, we use entry_num to find the proper
    "default" LID. Otherwise this routine is exactly like
    vcdinfo_get_default_offset with the exception of requiring an
    additional "entry_num" parameter.

    \return VCDINFO_INVALID_LID is returned on error, or if the LID
    is not a selection list or no "default" entry. Otherwise the LID
    offset is returned.
  */
  lid_t
  vcdinfo_get_multi_default_lid(const vcdinfo_obj_t *obj, lid_t lid,
				unsigned int selection);
  
  /*!
    \brief Get default or multi-default LID offset. 

    Return the LID offset associated with a the "default" entry of the
    passed-in LID parameter. Note "default" entries are associated
    with PSDs that are (extended) selection lists. If the "default"
    is a multi-default, we use entry_num to find the proper
    "default" offset. Otherwise this routine is exactly like
    vcdinfo_get_default_offset with the exception of requiring an
    additional "entry_num" parameter.

    \return VCDINFO_INVALID_OFFSET is returned on error, or if the LID
    is not a selection list or no "default" entry. Otherwise the LID
    offset is returned.
  */
  uint16_t
  vcdinfo_get_multi_default_offset(const vcdinfo_obj_t *obj, lid_t lid,
				   unsigned int selection);
  
  void * vcdinfo_get_pvd (vcdinfo_obj_t *obj);
  
  void * vcdinfo_get_scandata (vcdinfo_obj_t *obj);

  void * vcdinfo_get_searchDat (vcdinfo_obj_t *obj);
  
  void * vcdinfo_get_tracksSVD (vcdinfo_obj_t *obj);
  
  /*!
    Get the LOT pointer. 
  */
  LotVcd *
  vcdinfo_get_lot(const vcdinfo_obj_t *obj);
  
  /*!
    Get the extended LOT pointer. 
  */
  LotVcd *
  vcdinfo_get_lot_x(const vcdinfo_obj_t *obj);

  /*!
    Return Number of LIDs. 
  */
  lid_t
  vcdinfo_get_num_LIDs (const vcdinfo_obj_t *obj);
  
  /*!
    Return the audio type for a given track. 
    VCDINFO_INVALID_AUDIO_TYPE is returned on error.
  */
  unsigned int
  vcdinfo_get_num_audio_channels(unsigned int audio_type);
  
  /*!
    Return the number of entries in the VCD.
  */
  unsigned int
  vcdinfo_get_num_entries(const vcdinfo_obj_t *obj);
  
  /*!
    Return the number of segments in the VCD. 
  */
  segnum_t
  vcdinfo_get_num_segments(const vcdinfo_obj_t *obj);
  
  /*!  
    Return the highest track number in the current medium. 

    Because we track start numbering at 0 (which is the ISO 9660 track
    containing Video CD naviagion and disk information), this is one
    less than the number of tracks. 

    If there are no tracks, we return -1.
  */
  unsigned int
  vcdinfo_get_num_tracks(const vcdinfo_obj_t *obj);
  
  /*!
    Get the VCD info list.
  */
  VcdList *vcdinfo_get_offset_list(const vcdinfo_obj_t *obj);

  /*!
    Get the VCD info extended offset list.
  */
  VcdList *vcdinfo_get_offset_x_list(const vcdinfo_obj_t *obj);

  /*!
    Get the VCD info offset multiplier.
  */
  unsigned int vcdinfo_get_offset_mult(const vcdinfo_obj_t *obj);

  /*! 
    Get entry in offset list for the item that has offset. This entry 
    has for example the LID. NULL is returned on error. 
  */
  vcdinfo_offset_t *
  vcdinfo_get_offset_t (const vcdinfo_obj_t *obj, unsigned int offset);
  
  /*!
    Return a string containing the VCD preparer id with trailing
    blanks removed, or NULL if there is some problem in getting this.
  */
  const char *
  vcdinfo_get_preparer_id(const vcdinfo_obj_t *obj);
  
  /*!
    Get the PSD.
  */
  uint8_t *vcdinfo_get_psd(const vcdinfo_obj_t *obj);

  /*!
    Get the extended PSD.
  */
  uint8_t *vcdinfo_get_psd_x(const vcdinfo_obj_t *obj);

  /*!
    Return number of bytes in PSD.
  */
  uint32_t vcdinfo_get_psd_size (const vcdinfo_obj_t *obj);
  
  /*!
    Return number of bytes in the extended PSD.
  */
  uint32_t vcdinfo_get_psd_x_size (const vcdinfo_obj_t *obj);
  
  /*!
    Return a string containing the VCD publisher id with trailing
    blanks removed, or NULL if there is some problem in getting this.
  */
  const char * vcdinfo_get_publisher_id(const vcdinfo_obj_t *obj);
  
  /**
   \fn vcdinfo_get_return_offset(const vcdinfo_obj_t *obj);
   \brief Get return offset for a given LID. 
   \return  VCDINFO_INVALID_OFFSET is returned on error or if LID has no 
   "return" entry. Otherwise the LID offset is returned.
   */
  lid_t
  vcdinfo_get_return_offset(const vcdinfo_obj_t *obj, lid_t lid);
  
  /*!
    Return the audio type for a given segment. 
    VCDINFO_INVALID_AUDIO_TYPE is returned on error.
  */
  unsigned int 
  vcdinfo_get_seg_audio_type(const vcdinfo_obj_t *obj, segnum_t seg_num);
  
  /*!
    Return true if this segment is supposed to continue to the next one,
    (is part of an "item" or listing in the ISO 9660 filesystem).
  */
  bool vcdinfo_get_seg_continue(const vcdinfo_obj_t *obj, segnum_t seg_num);

  /*!  Return the starting LBA (logical block address) for segment
    entry_num in obj.  VCDINFO_NULL_LBA is returned if there is no entry.
    
    Note first seg_num is 0.
  */
  lba_t
  vcdinfo_get_seg_lba(const vcdinfo_obj_t *obj, segnum_t seg_num);
  
  /*!  Return the starting LSN (logical sector number) for segment
    entry_num in obj.  VCDINFO_NULL_LBA is returned if there is no entry.
    
    Note first seg_num is 0.
  */
  lsn_t
  vcdinfo_get_seg_lsn(const vcdinfo_obj_t *obj, segnum_t seg_num);
  
  /*!  Return the starting MSF (minutes/secs/frames) for segment
    entry_num in obj.  NULL is returned if there is no entry.
    
    Note first seg_num is 0.
  */
  const msf_t *
  vcdinfo_get_seg_msf(const vcdinfo_obj_t *obj, segnum_t seg_num);
  
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
  vcdinfo_get_seg_sector_count(const vcdinfo_obj_t *obj, segnum_t seg_num);
  
  /*!
    Return a string containing the VCD system id with trailing
    blanks removed, or NULL if there is some problem in getting this.
  */
  const char *
  vcdinfo_get_system_id(const vcdinfo_obj_t *obj);
  
  /*!
    Return the track number for entry n in obj. 

    In contrast to libcdio we start numbering at 0 which is the
    ISO9660 and metadata information for the Video CD. Thus track 
    1 is the first track the first complete MPEG track generally.
  */
  track_t
  vcdinfo_get_track(const vcdinfo_obj_t *obj, const unsigned int entry_num);
  
  /*!
    Return the audio type for a given track. 
    VCDINFO_INVALID_AUDIO_TYPE is returned on error.
    
    Note: track 1 is usually the first track.
  */
  unsigned int
  vcdinfo_get_track_audio_type(const vcdinfo_obj_t *obj, track_t track_num);
  
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
  vcdinfo_get_track_lba(const vcdinfo_obj_t *obj, track_t track_num);
  
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
  vcdinfo_get_track_lsn(const vcdinfo_obj_t *obj, track_t track_num);
  
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
			uint8_t *min, uint8_t *sec, uint8_t *frame);
  
  /*!
    Return the size in sectors for track n. 

    The IS0-9660 filesystem track has number 1. Tracks associated
    with playable entries numbers start at 2.
    
    FIXME: Whether we count the track pregap sectors is a bit haphazard.
    We should add a parameter to indicate whether this is wanted or not.
  */
  unsigned int
  vcdinfo_get_track_sect_count(const vcdinfo_obj_t *obj, 
			       const track_t track_num);
  
  /*!
    Return size in bytes for track number for entry n in obj.

    The IS0-9660 filesystem track has number 0. Tracks associated
    with playable entries numbers start at 1.

    FIXME: Do we count the track pregap sectors is a bit haphazard.
    We should add a parameter to indicate whether this is wanted or not.
  */
  unsigned int
  vcdinfo_get_track_size(const vcdinfo_obj_t *obj, track_t track_num);
  
  /*!
    \brief Get the kind of video stream segment of segment seg_num in obj.
    \return VCDINFO_FILES_VIDEO_INVALID is returned if  on error or obj is
    null. Otherwise the enumeration type.
    
    Note first seg_num is 0!
  */
  vcdinfo_video_segment_type_t
  vcdinfo_get_video_type(const vcdinfo_obj_t *obj, segnum_t seg_num);
  
  /*!
    \brief Get the kind of VCD that obj refers to.
  */
  vcd_type_t
  vcdinfo_get_VCD_type(const vcdinfo_obj_t *obj);
  
  /*!
    Return the VCD volume count - the number of CD's in the collection.
    O is returned if there is some problem in getting this. 
  */
  unsigned int
  vcdinfo_get_volume_count(const vcdinfo_obj_t *obj);
  
  /*!
    Return the VCD ID.
    NULL is returned if there is some problem in getting this. 
  */
  const char *
  vcdinfo_get_volume_id(const vcdinfo_obj_t *obj);
  
  /*!
    Return the VCD volumeset ID.
    NULL is returned if there is some problem in getting this. 
  */
  const char *
  vcdinfo_get_volumeset_id(const vcdinfo_obj_t *obj);
  
  /*!
    Return the VCD volume num - the number of the CD in the collection.
    This is a number between 1 and the volume count.
    O is returned if there is some problem in getting this. 
  */
  unsigned int
  vcdinfo_get_volume_num(const vcdinfo_obj_t *obj);
  
  int vcdinfo_get_wait_time (uint16_t wtime);

  /*!
    Return true if there is playback control. 
  */
  bool vcdinfo_has_pbc (const vcdinfo_obj_t *obj);
  
  /*! 
    Return true if VCD has "extended attributes" (XA). Extended attributes
    add meta-data attributes to a entries of file describing the file.
    See also cdio_get_xa_attr_str() which returns a string similar to
    a string you might get on a Unix filesystem listing ("ls").
  */
  bool vcdinfo_has_xa(const vcdinfo_obj_t *obj);
  
  /*!
    Add one to the MSF.
  */
  void vcdinfo_inc_msf (uint8_t *min, uint8_t *sec, int8_t *frame);
  
  /*!
    Convert minutes, seconds and frame (MSF components) into a
    logical block address (or LBA). 
    See also msf_to_lba which uses msf_t as its single parameter.
  */
  void 
  vcdinfo_lba2msf (lba_t lba, uint8_t *min, uint8_t *sec, uint8_t *frame);
  
  /*!
    Get the item id for a given list ID. 
    VCDINFO_REJECTED_MASK is returned on error or if obj is NULL. 
  */
  uint16_t
  vcdinfo_lid_get_itemid(const vcdinfo_obj_t *obj, lid_t lid);
  
  /*!
    \fn vcdinfo_lid_get_offset(const vcdinfo_obj_t *obj, 
                                    unsigned int entry_num);
    \brief Get offset entry_num for a given LID. 
    \return VCDINFO_INVALID_OFFSET is returned if obj on error or obj
    is NULL. Otherwise the LID offset is returned.
  */
  uint16_t vcdinfo_lid_get_offset(const vcdinfo_obj_t *obj, lid_t lid,
				  unsigned int entry_num);
  
  /*!
    Get the PSD Selection List Descriptor for a given lid.
    False is returned if not found.
  */
  bool vcdinfo_lid_get_pxd(const vcdinfo_obj_t *obj, PsdListDescriptor *pxd,
			   uint16_t lid);
  
  /*!
    Convert minutes, seconds and frame (MSF components) into a
    logical block address (or LBA). 
    See also msf_to_lba which uses msf_t as its single parameter.
  */
  lba_t vcdinfo_msf2lba (uint8_t min, uint8_t sec, int8_t frame);
  
  /*!
    Convert minutes, seconds and frame (MSF components) into a
    logical sector number (or LSN). 
  */
  lsn_t vcdinfo_msf2lsn (uint8_t min, uint8_t sec, int8_t frame);
  
  const char *
  vcdinfo_ofs2str (const vcdinfo_obj_t *obj, unsigned int offset, bool ext);
  
  /*!
    Calls recursive routine to populate obj->offset_list or obj->offset_x_list
    by going through LOT.
    
    Returns false if there was some error.
  */
  bool vcdinfo_visit_lot (vcdinfo_obj_t *obj, bool extended);
  
  bool vcdinfo_read_psd (vcdinfo_obj_t *obj);
  
  /*!
    \fn vcdinfo_selection_get_lid(const vcdinfo_obj_t *obj, lid_t lid,
                                     unsigned int selection);
    \brief Get the "default" lid of a selection for a given lid. 

    Return the LID offset associated with a the selection number of the
    passed-in LID parameter. 

    \return VCDINFO_INVALID_LID is returned if obj on error or obj
    is NULL. Otherwise the LID offset is returned.
  */
  lid_t vcdinfo_selection_get_lid(const vcdinfo_obj_t *obj, lid_t lid,
				  unsigned int selection);
  
  /*!
    \fn vcdinfo_selection_get_offset(const vcdinfo_obj_t *obj, lid_t lid,
                                     unsigned int selection);
    \brief Get offset of a selection for a given LID. 

    Return the LID offset associated with a the selection number of the
    passed-in LID parameter. 

    \return VCDINFO_INVALID_OFFSET is returned if obj on error or obj
    is NULL. Otherwise the LID offset is returned.
  */
  uint16_t vcdinfo_selection_get_offset(const vcdinfo_obj_t *obj, lid_t lid,
					unsigned int selection);
  
  /*!
    Change trailing blanks in str to nulls.  Str has a maximum size of
    n characters.
  */
  const char * vcdinfo_strip_trail (const char str[], size_t n);
  
  /*!
    Initialize the vcdinfo structure "obj". Should be done before other
    routines using obj are called.
  */
  bool vcdinfo_init(vcdinfo_obj_t *obj);
  
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
  vcdinfo_open(vcdinfo_obj_t **p_obj, char *source_name[], 
	       driver_id_t source_type, const char access_mode[]);
  
  
  /*!
    Dispose of any resources associated with vcdinfo structure "obj".
    Call this when "obj" it isn't needed anymore. 
    
    True is returned is everything went okay, and false if not.
  */
  bool vcdinfo_close(vcdinfo_obj_t *obj);
  
  /*!
    Return true if offset is "rejected". That is shouldn't be displayed
    in a list of entries.
  */
  bool vcdinfo_is_rejected(uint16_t offset);

/* Include lower-level access as well. */
#include <libvcd/inf.h>

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_VCD_INFO_H*/
