/*!
   \file vcdinfo.h

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

#ifndef _VCDINFO_H
#define _VCDINFO_H

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

/* From vcd_cd_sector.h */
#define VCDINFO_CDDA_SECTOR_SIZE  2352
#define VCDINFO_M2F1_SECTOR_SIZE  2048

/*! \def Bytes in mode 2 Format 2 sector size. */
#define VCDINFO_M2F2_SECTOR_SIZE  2324

#define VCDINFO_M2SUB_SECTOR_SIZE 2332
#define VCDINFO_M2RAW_SECTOR_SIZE 2336

#define VCDINFO_ISO_BLOCK_SIZE    2048

#define VCDINFO_NULL_LBA          0xFFFFFFFF

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
#define VCDINFO_INVALID_TRACK   0xFFFF

/*! 
  Constant for invalid offset.
*/
#define VCDINFO_INVALID_OFFSET  0xFFFF

/*! 
  Constant for ending or "leadout" track.
*/
#define VCDINFO_LEADOUT_TRACK  0xaa

/*! 
  Constant for invalid entry.
*/
#define VCDINFO_INVALID_ENTRY  0xFFFF

/*! 
  Constant for invalid LID. 
  FIXME: player needs these to be the same. rest of VCDimager
  uses 0 for INVALID LID.
  
*/
#define VCDINFO_INVALID_LID  VCDINFO_INVALID_ENTRY

/*! 
  Constant for invalid itemid
*/
#define VCDINFO_INVALID_ITEMID  0xFFFF

/*! 
  Constant for invalid audio type
*/
#define VCDINFO_INVALID_AUDIO_TYPE  4

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
  
  typedef enum {
    VCDINFO_SOURCE_UNDEF,          /* none of the below, e.g. uninitialized */
    VCDINFO_SOURCE_AUTO,           /* figure out whether a device or a flie */
    VCDINFO_SOURCE_BIN,            /* bin disk image; default is 2352 bytes */
    VCDINFO_SOURCE_CUE,            /* Cue-sheet CD-ROM disk image */
    VCDINFO_SOURCE_NRG,            /* Nero CD-ROM disk image */
    VCDINFO_SOURCE_DEVICE,         /* a device like /dev/cdrom */
    VCDINFO_SOURCE_SECTOR_2336,    /* bin file emulating 2336 bytes/sector */
  } vcdinfo_source_t;
  
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
  
  
  /* Move somewhere else? */
  typedef struct iso_primary_descriptor pvd_t;
  
  typedef struct {
    vcd_type_t vcd_type;
    
    VcdImageSource *img;
    
    struct iso_primary_descriptor pvd;
    
    InfoVcd info;
    EntriesVcd entries;
    
    uint8_t *psd;
    VcdList *offset_list;
    VcdList *offset_x_list;
    VcdList *segment_list; /* list data entry is a vcd_image_stat_t */
    
    LotVcd *lot;
    LotVcd *lot_x;
    uint8_t *psd_x;
    unsigned int psd_x_size;
    bool extended;

    bool has_xa;           /* True if has extended attributes (XA) */
    
    void *tracks_buf;
    void *search_buf;
    void *scandata_buf;
    
    char *source_name; /* VCD device or file currently open */
    
  } vcdinfo_obj_t;
  
  /*
   * VcdImageSource 
   */
  
  struct _VcdImageSource {
    void *user_data;
    vcd_image_source_funcs op;
  };
  
  /*!
    Used in working with LOT - list of offsets and lid's 
  */
  typedef struct {
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
    VCDINFO_ITEM_TYPE_NOTFOUND,
  } vcdinfo_item_enum_t;
  
  typedef struct {
    uint16_t num;
    vcdinfo_item_enum_t type;
  } vcdinfo_itemid_t;
  
  typedef enum {
    VCDINFO_OPEN_ERROR,          /* Error */
    VCDINFO_OPEN_VCD,            /* Is VCD of some sort */
    VCDINFO_OPEN_OTHER,          /* Is not VCD but something else */
  } vcdinfo_open_return_t;
  
  const char *
  vcdinfo_area_str (const struct psd_area_t *_area);
  
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
  vcdinfo_ogt2str(const vcdinfo_obj_t *obj, unsigned int seg_num);
  
  /*!
    Note first seg_num is 0!
  */
  const char * 
  vcdinfo_video_type2str(const vcdinfo_obj_t *obj, unsigned int seg_num);
  
  const char *
  vcdinfo_pin2str (uint16_t itemid);
  
  int
  vcdinfo_calc_psd_wait_time (uint16_t wtime);
  
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
    Get autowait time value for PsdPlayListDescriptor *d.
    Time is in seconds unless it is -1 (unlimited).
  */
  int
  vcdinfo_get_autowait_time (const PsdPlayListDescriptor *d);
  
  /*!
    Return the base selection number.
  */
  unsigned int
  vcdinfo_get_bsn(const PsdSelectionListDescriptor *psd);
  
  /*!
    Return a string containing the default VCD device if none is specified.
    This might be something like "/dev/cdrom" on Linux or 
    "/vol/dev/aliases/cdrom0" on Solaris,  or maybe "VIDEOCD.CUE" for 
    if bin/cue I/O routines are in effect. 
    
    Return "" if we can't get this information.
  */
  char *
  vcdinfo_get_default_device (const vcdinfo_obj_t *obj);
  
  /*!
    \brief Get default offset. 
    \return  VCDINFO_INVALID_OFFSET is returned on error or if
    no "return"  entry. Otherwise the LID offset is returned.
  */
  uint16_t
  vcdinfo_get_default(const vcdinfo_obj_t *obj, unsigned int lid);
  
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
  
  /*!
    Get the item id for a given list ID. 
    VCDINFO_REJECTED_MASK is returned on error or if obj is NULL. 
  */
  uint16_t
  vcdinfo_get_itemid_from_lid(const vcdinfo_obj_t *obj, lid_t lid);
  
  /*!
    Get the item id for a given PSD selector descriptor. 
    VCDINFO_REJECTED_MASK is returned on error or if d is NULL. 
  */
  uint16_t
  vcdinfo_get_itemid_from_psd(const PsdSelectionListDescriptor *d);

  /*!
    Get the LID from a given PSD play-list descriptor. 
    VCDINFO_REJECTED_MASK is returned d on error or d is NULL. 
  */
  uint16_t
  vcdinfo_get_lid_from_pld(const PsdPlayListDescriptor *d);
  
  /*!
    Get the LID from a given PSD selector descriptor. 
    VCDINFO_REJECTED_MASK is returned d on error or d is NULL. 
  */
  uint16_t
  vcdinfo_get_lid_from_psd(const PsdSelectionListDescriptor *d);
  
  /*!
    Get the LID rejected status for a given PSD selector descriptor. 
  true is also returned d is NULL. 
  */
  bool
  vcdinfo_get_lid_rejected_from_psd(const PsdSelectionListDescriptor *d);
  
  /*!
    Return true if item is to be looped. 
  */
  uint16_t
  vcdinfo_get_loop_count (const PsdSelectionListDescriptor *d);
  
  /*!
    Return Number of LIDs. 
  */
  lid_t
  vcdinfo_get_num_LIDs (const vcdinfo_obj_t *obj);
  
  /**
     \fn vcdinfo_get_next_from_pld(const PsdPlayListDescriptor *pld); 
     \brief  Get next offset for a given PSD selector descriptor.  
     \return  VCDINFO_INVALID_OFFSET is returned on error or if pld has no 
     "next" entry or pld is NULL. Otherwise the LID offset is returned.
  */
  lid_t
  vcdinfo_get_next_from_pld(const PsdPlayListDescriptor *pld);
  
  /**
     \fn vcdinfo_get_next_from_psd(const PsdSelectionListDescriptor *psd);
     \brief Get next offset for a given PSD selector descriptor. 
     \return  VCDINFO_INVALID_OFFSET is returned on error or if psd has no 
     "next" entry or psd is NULL. Otherwise the LID offset is returned.
  */
  lid_t
  vcdinfo_get_next_from_psd(const PsdSelectionListDescriptor *psd);
  
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
  unsigned int
  vcdinfo_get_num_segments(const vcdinfo_obj_t *obj);
  
  /*!  
    Return the number of tracks in the current medium. Note this is
    one less than the track number reported in vcddump.  We don't count
    the track that contains ISO9660 and metadata information.
  */
  unsigned int
  vcdinfo_get_num_tracks(const vcdinfo_obj_t *obj);
  
  /*!
    \fn vcdinfo_get_offset_from_lid(const vcdinfo_obj_t *obj, 
                                    unsigned int entry_num);
    \brief Get offset entry_num for a given LID. 
    \return VCDINFO_INVALID_OFFSET is returned if obj on error or obj
    is NULL. Otherwise the LID offset is returned.
  */
  uint16_t
  vcdinfo_get_offset_from_lid(const vcdinfo_obj_t *obj, lid_t lid,
			      unsigned int entry_num);
  
  /*!
    \brief Get offset entry_num for a given PSD selector descriptor. 
    \param d PSD selector containing the entry_num we query
    \param entry_num entry number that we want the LID offset for.
    \return VCDINFO_INVALID_OFFSET is returned if d on error or d is
  NULL. Otherwise the LID offset is returned.
  */
  uint16_t
  vcdinfo_get_offset_from_psd(const PsdSelectionListDescriptor *d, 
			      unsigned int entry_num);
  
  
  /*! 
    Get entry in offset list for the item that has offset. This entry 
    has for example the LID. NULL is returned on error. 
  */
  vcdinfo_offset_t *
  vcdinfo_get_offset_t (const vcdinfo_obj_t *obj, unsigned int offset);
  
  /*!
    Get play-time value for PsdPlayListDescriptor *d.
    Time is in seconds.
  */
  uint16_t
  vcdinfo_get_play_time (const PsdPlayListDescriptor *d);
  
  /*!
    Return a string containing the VCD preparer id with trailing
    blanks removed, or NULL if there is some problem in getting this.
  */
  const char *
  vcdinfo_get_preparer_id(const vcdinfo_obj_t *obj);
  
  /**
     \fn vcdinfo_get_prev_from_psd(const PsdSelectionListDescriptor *pld);
     \brief Get prev offset for a given PSD selector descriptor. 
     \return  VCDINFO_INVALID_OFFSET is returned on error or if pld has no 
     "prev" entry or pld is NULL. Otherwise the LID offset is returned.
  */
  lid_t
  vcdinfo_get_prev_from_pld(const PsdPlayListDescriptor *pld);
  
  /**
     \fn vcdinfo_get_prev_from_psd(const PsdPlayListDescriptor *psd);
     \brief Get prev offset for a given PSD selector descriptor. 
     \return  VCDINFO_INVALID_OFFSET is returned on error or if psd has no 
     "prev"
     entry or psd is NULL. Otherwise the LID offset is returned.
  */
  lid_t
  vcdinfo_get_prev_from_psd(const PsdSelectionListDescriptor *psd);
  
  /*!
    Return number of bytes in PSD.
  */
  uint32_t
  vcdinfo_get_psd_size (const vcdinfo_obj_t *obj);
  
  /*!
    Return a string containing the VCD publisher id with trailing
    blanks removed, or NULL if there is some problem in getting this.
  */
  const char *
  vcdinfo_get_publisher_id(const vcdinfo_obj_t *obj);
  
  /*!
    Get the PSD Selection List Descriptor for a given lid.
    False is returned if not found.
  */
  bool
  vcdinfo_get_pxd_from_lid(const vcdinfo_obj_t *obj, 
			   PsdListDescriptor *pxd, uint16_t lid);
  
  /*!
    \brief Get return offset. 
    \return  VCDINFO_INVALID_OFFSET is returned on error or if
    no "return"  entry. Otherwise the LID offset is returned.
  */
  lid_t
  vcdinfo_get_return(const vcdinfo_obj_t *obj, unsigned int lid);
  
  /*!
    \brief Get return offset for a given PSD selector descriptor. 
    \return  VCDINFO_INVALID_OFFSET is returned on error or if pld has 
    no "return"  entry or pld is NULL. Otherwise the LID offset is returned.
  */
  lid_t
  vcdinfo_get_return_from_pld(const PsdPlayListDescriptor *pld);
  
/*!
  \brief Get return offset for a given PSD selector descriptor. 
  \return  VCDINFO_INVALID_OFFSET is returned on error or if psd has 
  no "return" entry or psd is NULL. Otherwise the LID offset is returned.
*/
  lid_t
  vcdinfo_get_return_from_psd(const PsdSelectionListDescriptor *psd);
  
  /*!
    Return the audio type for a given segment. 
    VCDINFO_INVALID_AUDIO_TYPE is returned on error.
  */
  unsigned int 
  vcdinfo_get_seg_audio_type(const vcdinfo_obj_t *obj, unsigned int seg_num);
  
  /*!  Return the starting LBA (logical block address) for segment
    entry_num in obj.  VCDINFO_NULL_LBA is returned if there is no entry.
    
    Note first seg_num is 0.
  */
  lba_t
  vcdinfo_get_seg_lba(const vcdinfo_obj_t *obj, const unsigned int seg_num);
  
  /*!  Return the starting MSF (minutes/secs/frames) for segment
    entry_num in obj.  NULL is returned if there is no entry.
    
    Note first seg_num is 0.
  */
  const msf_t *
  vcdinfo_get_seg_msf(const vcdinfo_obj_t *obj, const unsigned int seg_num);
  
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
vcdinfo_get_seg_size(const vcdinfo_obj_t *obj, const unsigned int seg_num);

  /*!  
    Return the number of sectors for segment
    entry_num in obj.  0 is returned if there is no entry.
    
    Use this routine to figure out the actual number of bytes a physical
    region of a disk or CD takes up for a segment.
  */
  uint32_t
  vcdinfo_get_seg_sector_count(const vcdinfo_obj_t *obj, 
			       const unsigned int seg_num);
  
  /*!
    Return a string containing the VCD system id with trailing
    blanks removed, or NULL if there is some problem in getting this.
  */
  const char *
  vcdinfo_get_system_id(const vcdinfo_obj_t *obj);
  
  /*!
    Get timeout LID for PsdPlayListDescriptor *d.
  */
  int
  vcdinfo_get_timeout_LID (const PsdSelectionListDescriptor *d);
  
  /*!
    Get timeout wait value for PsdPlayListDescriptor *d.
    Time is in seconds unless it is -1 (unlimited).
  */
  int
  vcdinfo_get_timeout_time (const PsdSelectionListDescriptor *d);
  
  /*!
    Return the track number for entry n in obj. The first track starts
    at 1. Note this is one less than the track number reported in vcddump.
    We don't count the track that contains ISO9660 and metadata information.
  */
  track_t
  vcdinfo_get_track(const vcdinfo_obj_t *obj, const unsigned int entry_num);
  
  /*!
    Return the audio type for a given track. 
    VCDINFO_INVALID_AUDIO_TYPE is returned on error.
    
    Note: track 1 is usually the first track.
  */
  unsigned int
  vcdinfo_get_track_audio_type(const vcdinfo_obj_t *obj, 
			       unsigned int track_num);
  
  /*!  
    Return the starting LBA (logical block address) for track number
    track_num in obj.  Tracks numbers start at 1.
    The "leadout" track is specified either by
    using track_num LEADOUT_TRACK or the total tracks+1.
    VCDINFO_NULL_LBA is returned on failure.
  */
  lba_t
  vcdinfo_get_track_lba(const vcdinfo_obj_t *obj, unsigned int track_num);
  
  /*!  
    Return the starting MSF (minutes/secs/frames) for track number
    track_num in obj.  Tracks numbers start at 1.
    The "leadout" track is specified either by
    using track_num VCD_LEADOUT_TRACK or the total tracks+1.
    1 is returned on error, 0.
  */
  int
  vcdinfo_get_track_msf(const vcdinfo_obj_t *obj, unsigned int track_num,
			uint8_t *min, uint8_t *sec, uint8_t *frame);
  
  /*!
    Return the size in sectors for track n. The first track is 1.
  */
  unsigned int
  vcdinfo_get_track_sect_count(const vcdinfo_obj_t *obj, 
			       const unsigned int track_num);
  
  /*!
    Return size in bytes for track number for entry n in obj.
  */
  unsigned int
  vcdinfo_get_track_size(const vcdinfo_obj_t *obj, unsigned int track_num);
  
  /*!
    \brief Get the kind of video stream segment of segment seg_num in obj.
    \return VCDINFO_FILES_VIDEO_INVALD is returned if  on error or obj is
    null. Otherwise the enumeration type.
    
    Note first seg_num is 0!
  */
  vcdinfo_video_segment_type_t
  vcdinfo_get_video_type(const vcdinfo_obj_t *obj, unsigned int seg_num);
  
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
  
  /*!
    Get wait time value for PsdPlayListDescriptor *d.
    Time is in seconds unless it is -1 (unlimited).
  */
  int
  vcdinfo_get_wait_time (const PsdPlayListDescriptor *d);
  
  /*!
    Returns a string which interpreting the extended attribute xa_attr. 
    For example:
    \verbatim
    d---1xrxrxr
    ---2--r-r-r
    -a--1xrxrxr
    \endverbatim
    
    A description of the characters in the string follows
    The 1st character is either "d" if the entry is a directory, or "-" if not
    The 2nd character is either "a" if the entry is CDDA (audio), or "-" if not
    The 3rd character is either "i" if the entry is interleaved, or "-" if not
    The 4th character is either "2" if the entry is mode2 form2 or "-" if not
    The 5th character is either "1" if the entry is mode2 form1 or "-" if not
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
  vcdinfo_get_xa_attr_str (uint16_t xa_attr);
  
  /*!
    Return true is there is playback control. 
  */
  bool vcdinfo_has_pbc (const vcdinfo_obj_t *obj);
  
  /*! 
    Return true if VCD has "extended attributes" (XA). Extended attributes
    add meta-data attributes to a entries of file describing the file.
    See also vcdinfo_get_xa_attr_str() which returns a string similar to
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
  lba_t vcdinfo_msf2lba (uint8_t min, uint8_t sec, int8_t frame);
  
  /*!
    Convert minutes, seconds and frame (MSF components) into a
    logical sector number (or LSN). 
  */
  lsn_t vcdinfo_msf2lsn (uint8_t min, uint8_t sec, int8_t frame);
  
  /*!
    Convert minutes, seconds and frame (MSF components) into a
    logical block address (or LBA). 
    See also msf_to_lba which uses msf_t as its single parameter.
  */
  void 
  vcdinfo_lba2msf (lba_t lba, uint8_t *min, uint8_t *sec, uint8_t *frame);
  
  /*!
    Convert logical block address (LBA), to a logical sector number (LSN).
  */
  lsn_t vcdinfo_lba2lsn(lba_t lba) ;
  
  /*!
    Convert logical sector number (LSN), to a logical block address (LBA).
  */
  lba_t vcdinfo_lsn2lba(lsn_t lsn);
  
  const char *
  vcdinfo_ofs2str (const vcdinfo_obj_t *obj, unsigned int offset, bool ext);
  
  void vcdinfo_visit_lot (vcdinfo_obj_t *obj, bool extended);
  
  bool vcdinfo_read_psd (vcdinfo_obj_t *obj);
  
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
    Set up vcdinfo structure "obj" for reading and writing from a
    particular medium. This should be done before after initialization
    but before any routines that need to retrieve or write data.
    
    source_name is the device or file to use for inspection, and
    source_type indicates if this is a device or a file.
    access_mode gives special access options for reading.
    
    VCDINFO_OPEN_VCD is returned if everything went okay; 
    VCDINFO_OPEN_ERROR if there was an error and VCDINFO_OPEN_OTHER if the
    medium is something other than a VCD.
  */
  vcdinfo_open_return_t
  vcdinfo_open(vcdinfo_obj_t *obj, char source_name[], 
	       vcdinfo_source_t source_type, const char access_mode[]);
  
  
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
  
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_VCDINFO_H*/
