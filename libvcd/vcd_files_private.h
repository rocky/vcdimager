/*
    $Id$

    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>
              (C) 2000 Jens B. Jorgensen <jbj1@ultraemail.net>

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

#ifndef __VCD_FILES_PRIVATE_H__
#define __VCD_FILES_PRIVATE_H__

#include "vcd_types.h"

/* random note: most stuff is big endian here */

#define ENTRIES_ID_VCD  "ENTRYVCD"
#define ENTRIES_ID_SVCD "ENTRYVCD" /* not ENTRYSVD! */

#define ENTRIES_VERSION_VCD11 0x01
#define ENTRIES_SPTAG_VCD11   0x01   

#define ENTRIES_VERSION_VCD2  0x02
#define ENTRIES_SPTAG_VCD2    0x00

#define ENTRIES_VERSION_SVCD  0x01
#define ENTRIES_SPTAG_SVCD    0x00

#ifdef __MWERKS__
#pragma options align=packed
#endif

typedef struct {
  char ID[8]                 GNUC_PACKED; /* "ENTRYVCD" */
  uint8_t version            GNUC_PACKED; /* 0x02 --- VCD2.0
                                             0x01 --- SVCD, should be
                                             same as version in
                                             INFO.SVD */
  uint8_t sys_prof_tag       GNUC_PACKED; /* 0x01 if VCD1.1
                                             0x00 else */
  uint16_t entry_count       GNUC_PACKED; /* 1 <= tracks <= 500 */
  struct { /* all fields are BCD */
    uint8_t n                GNUC_PACKED; /* cd track no 2 <= n <= 99 */
    msf_t msf                GNUC_PACKED;
  } entry[500]               GNUC_PACKED;
  uint8_t reserved2[36]      GNUC_PACKED; /* RESERVED, must be 0x00 */
} EntriesVcd; /* sector 00:04:01 */

#define INFO_ID_VCD   "VIDEO_CD"
#define INFO_ID_SVCD  "SUPERVCD"
#define INFO_ID_HQVCD "HQ-VCD  "

#define INFO_VERSION_VCD11 0x01
#define INFO_SPTAG_VCD11   0x01   

#define INFO_VERSION_VCD2  0x02
#define INFO_SPTAG_VCD2    0x00   

#define INFO_VERSION_SVCD  0x01
#define INFO_SPTAG_SVCD    0x00

#define INFO_VERSION_HQVCD 0x01
#define INFO_SPTAG_HQVCD   0x01

#define INFO_OFFSET_MULT   0x08

/* This one-byte field describes certain characteristics of the disc */
typedef struct {
#if defined(BITFIELD_LSBF)
  uint8_t reserved1 : 1;                   /* Reserved, must be zero */
  uint8_t restriction : 2;                 /* restriction, eg. "unsuitable
                                              for kids":
                                              0x0 ==> unrestricted,
                                              0x1 ==> restricted category 1,
                                              0x2 ==> restricted category 2,
                                              0x3 ==> restricted category 3 */
  uint8_t special_info : 1;                /* Special Information is encoded 
                                              in the pictures */
  uint8_t user_data_cc : 1;                /* MPEG User Data is used
                                              for Closed Caption */
  uint8_t use_lid2 : 1;                    /* If == 1 and the PSD is
                                              interpreted and the next
                                              disc has the same album
                                              id then start the next
                                              disc at List ID #2,
                                              otherwise List ID #1 */ 
  uint8_t use_track3 : 1;                  /* If == 1 and the PSD is
                                              not interpreted  and
                                              next disc has same album
                                              id, then start next disc
                                              with track 3, otherwise
                                              start with track 2 */ 
  uint8_t reserved2 : 1;                   /* Reserved, must be zero */
#else
  uint8_t reserved2 : 1;
  uint8_t use_track3 : 1;
  uint8_t use_lid2 : 1;
  uint8_t user_data_cc : 1;
  uint8_t special_info : 1;
  uint8_t restriction : 2;
  uint8_t reserved1 : 1;
#endif
} InfoStatusFlags;

typedef struct 
{
#if defined(BITFIELD_LSBF)
  uint8_t audio_type : 2;                /* Audio characteristics:
                                            0x0 - No MPEG audio stream
                                            0x1 - One MPEG1 or MPEG2 audio
                                                  stream without extension
                                            0x2 - Two MPEG1 or MPEG2 audio
                                                  streams without extension
                                            0x3 - One MPEG2 multi-channel 
                                                    audio stream w/ extension*/
  uint8_t video_type : 3;                /* Video characteristics:
                                            0x0 - No MPEG video data
                                            0x1 - NTSC still picture
                                            0x2 - Reserved (NTSC hires?)
                                            0x3 - NTSC motion picture
                                            0x4 - Reserved
                                            0x5 - PAL still picture
                                            0x6 - Reserved (PAL hires?)
                                            0x7 - PAL motion picture */
  uint8_t item_cont : 1;                 /* Indicates segment is continuation
                                            0x0 - 1st or only segment of item
                                            0x1 - 2nd or later
                                                  segment of item */
  uint8_t reserved2 : 2;                 /* Reserved, must be zero */
#else
  uint8_t reserved2 : 2;
  uint8_t item_cont : 1;
  uint8_t video_type : 3;
  uint8_t audio_type : 2;
#endif
}
InfoSpiContents;

typedef struct {
  char   ID[8]               GNUC_PACKED;  /* const "VIDEO_CD" for
                                              VCD, "SUPERVCD" or
                                              "HQ-VCD  " for SVCD */
  uint8_t version            GNUC_PACKED;  /* 0x02 -- VCD2.0,
                                              0x01 for SVCD and VCD1.x */ 
  uint8_t sys_prof_tag       GNUC_PACKED;  /* System Profile Tag, used
                                              to define the set of
                                              mandatory parts to be
                                              applied for compatibility;
                                              0x00 for "SUPERVCD",
                                              0x01 for "HQ-VCD  ",
                                              0x0n for VCDx.n */ 
  char album_desc[16]        GNUC_PACKED;  /* album identification/desc. */

  uint16_t vol_count         GNUC_PACKED;  /* number of volumes in album */
  uint16_t vol_id            GNUC_PACKED;  /* number id of this volume
                                              in album */
  uint8_t  pal_flags[13]     GNUC_PACKED;  /* bitset of 98
                                              PAL(=set)/NTSC flags */
  InfoStatusFlags flags      GNUC_PACKED;  /* status flags bit field */
  uint32_t psd_size          GNUC_PACKED;  /* size of PSD.VCD file */
  uint8_t  first_seg_addr[3] GNUC_PACKED;  /* first segment addresses,
                                              coded BCD The location
                                              of the first sector of
                                              the Segment Play Item
                                              Area, in the form
                                              mm:ss:00. Must be
                                              00:00:00 if the PSD size
                                              is 0. */
  uint8_t  offset_mult       GNUC_PACKED;  /* offset multiplier, must be 8 */
  uint16_t lot_entries       GNUC_PACKED;  /* offsets in lot */
  uint16_t item_count        GNUC_PACKED;  /* segments used for segmentitems */
  InfoSpiContents spi_contents[1980]
                             GNUC_PACKED;  /* The next 1980 bytes contain one
                                              byte for each possible segment
                                              play item. Each byte indicates
                                              contents. */
  char reserved1[12]         GNUC_PACKED;  /* Reserved, must be zero */
} InfoVcd;

/* LOT.VCD
   This optional file is only necessary if the PSD size is not zero.
   This List ID Offset Table allows you to start playing the PSD from
   lists other than the default List ID number. This table has a fixed length
   of 32 sectors and maps List ID numbers into List Offsets. It's got
   an entry for each List ID Number with the 16-bit offset. Note that
   List ID 1 has an offset of 0x0000. All unused or non-user-accessible
   entries must be 0xffff. */

#define LOT_VCD_OFFSETS ((1 << 15)-1)

typedef struct {
  uint16_t reserved                 GNUC_PACKED;  /* Reserved, must be zero */
  uint16_t offset[LOT_VCD_OFFSETS]  GNUC_PACKED;  /* offset given in 8
                                                     byte units */
} LotVcd;

/* PSD.VCD
   The PSD controls the "user interaction" mode which can be used to make
   menus, etc. The PSD contains a set of Lists. Each List defines a set of
   Items which are played in sequence. An Item can be an mpeg track (in whole
   or part) or a Segment Play Item which can subsequently be mpeg video
   with or without audio, one more more mpeg still pictures (with or without
   audio) or mpeg audio only.

   The Selection List defines the action to be taken in response to a set
   of defined user actions: Next, Previous, Default Select, Numeric, Return.

   The End List terminates the control flow.

   Each list has a unique list id number. The first must be 1, the others can
   be anything (up to 32767).

   References to PSD list addresses are expressed as an offset into the PSD
   file. The offset indicated in the file must be multiplied by the Offset
   Multiplier found in the info file (although this seems to always have to
   be 8). Unused areas are filled with zeros. List ID 1 starts at offset 0.
*/

/* ...difficult to represent as monolithic C struct... */

typedef enum {
  PSD_TYPE_PLAY_LIST = 0x10,        /* Play List */
  PSD_TYPE_SELECTION_LIST = 0x18,   /* Selection List -- (jbj1: more
                                       on list later) */
  PSD_TYPE_END_OF_LIST = 0x1f       /* End of List */
} psd_descriptor_types;

typedef struct {
  uint8_t type               GNUC_PACKED;
  uint8_t reserved[7]        GNUC_PACKED;
} PsdEndOfListDescriptor;

struct psd_area_t
{
  uint8_t x1;
  uint8_t y1;
  uint8_t x2;
  uint8_t y2;
};

typedef struct {
  uint8_t type               GNUC_PACKED;
  uint8_t reserved           GNUC_PACKED;
  uint8_t nos                GNUC_PACKED;
  uint8_t bsn                GNUC_PACKED;
  uint16_t lid               GNUC_PACKED;
  uint16_t prev_ofs          GNUC_PACKED;
  uint16_t next_ofs          GNUC_PACKED;
  uint16_t return_ofs        GNUC_PACKED;
  uint16_t default_ofs       GNUC_PACKED;
  uint16_t timeout_ofs       GNUC_PACKED;
  uint8_t wtime              GNUC_PACKED;
  uint8_t loop               GNUC_PACKED;
  uint16_t itemid            GNUC_PACKED;
  uint16_t ofs[EMPTY_ARRAY_SIZE] GNUC_PACKED; /* variable length */
  /* PsdSelectionListDescriptorExtended */
} PsdSelectionListDescriptor;

typedef struct {
  struct psd_area_t prev_area      GNUC_PACKED;
  struct psd_area_t next_area      GNUC_PACKED;
  struct psd_area_t return_area    GNUC_PACKED;
  struct psd_area_t default_area   GNUC_PACKED;
  struct psd_area_t area[EMPTY_ARRAY_SIZE] GNUC_PACKED; /* variable length */
} PsdSelectionListDescriptorExtended;

typedef struct {
  uint8_t type               GNUC_PACKED;
  uint8_t noi                GNUC_PACKED; /* number of items */
  uint16_t lid               GNUC_PACKED; /* list id: high-bit means this 
                                             list is rejected in the LOT
                                             (also, can't use 0) */
  uint16_t prev_ofs          GNUC_PACKED; /* previous list offset 
                                             (0xffff disables) */
  uint16_t next_ofs          GNUC_PACKED; /* next list offset
                                             (0xffff disables) */
  uint16_t return_ofs        GNUC_PACKED; /* return list offset
                                             (0xffff disables) */
  uint16_t ptime             GNUC_PACKED; /* play time in 1/15 s,
                                             0x0000 meaning full item */
  uint8_t  wtime             GNUC_PACKED; /* delay after, in seconds,
                                             if 1 <= wtime <= 60
                                                wait is wtime
                                             else if 61 <= wtime <= 254 
                                                wait is (wtime-60) * 10 + 60
                                             else wtime == 255
                                                wait is infinite  */
  uint8_t  atime             GNUC_PACKED; /* auto pause wait time
                                             calculated same as wtime, used
                                             for each item in list if the auto
                                             pause flag in a sector is true */
  uint16_t itemid[EMPTY_ARRAY_SIZE] GNUC_PACKED; /* item number
                                                0 <= n <= 1      - play nothing
                                                2 <= n <= 99     - play track n
                                              100 <= n <= 599    - play entry
                                                          (n - 99) from entries
                                                          table to end of track
                                              600 <= n <= 999    - reserved
                                             1000 <= n <= 2979   - play segment
                                                          play item (n - 999)
                                             2980 <= n <= 0xffff - reserved */
} PsdPlayListDescriptor;

/* TRACKS.SVD
   SVCD\TRACKS.SVD is a mandatory file which describes the numbers and types
   of MPEG tracks on the disc. */

/* SVDTrackContent indicates the audio/video content of an MPEG Track */

typedef struct {
#if defined(BITFIELD_LSBF)
  uint8_t audio : 2;                      /* Audio Content
                                             0x00 : No MPEG audio stream
                                             0x01 : One MPEG{1|2} audio stream
                                             0x02 : Two MPEG{1|2} streams
                                             0x03 : One MPEG2 multi-channel
                                                    audio stream with
                                                    extension */
  uint8_t video : 3;                      /* Video Content
                                             0x00 : No MPEG video
                                             0x03 : NTSC video
                                             0x07 : PAL video */
  uint8_t reserved1 : 1;                  /* Reserved, must be zero */
  uint8_t reserved2 : 2;                  /* Reserved, must be zero */
#else
  uint8_t reserved2 : 2;
  uint8_t reserved1 : 1;
  uint8_t video : 3;
  uint8_t audio : 2;
#endif
} SVDTrackContent;

/* The file contains a series of structures, one for each
   track, which indicates the track's playing time (in sectors, not actually
   real time) and contents. */

#define TRACKS_SVD_FILE_ID  "TRACKSVD"
#define TRACKS_SVD_VERSION  0x01

typedef struct {
  char file_id[8]               GNUC_PACKED; /* == "TRACKSVD" */
  uint8_t version               GNUC_PACKED; /* == 0x01 */
  uint8_t reserved              GNUC_PACKED; /* Reserved, must be zero */
  uint8_t tracks                GNUC_PACKED; /* number of MPEG tracks */
  msf_t playing_time[EMPTY_ARRAY_SIZE] GNUC_PACKED; /* per track, BCD coded
                                                       mm:ss:ff */
} TracksSVD;

typedef struct {
  /* TracksSVD tracks_svd; */
  SVDTrackContent contents[EMPTY_ARRAY_SIZE] GNUC_PACKED; /* indicates track
                                                             contents */
} TracksSVD2;

/* SEARCH.DAT
   This file defines where the scan points are. It covers all mpeg tracks
   together. A scan point at time T is the nearest I-picture in the MPEG
   stream to the given time T. Scan points are given at every half-second
   for the entire duration of the disc. */

#define SEARCH_FILE_ID        "SEARCHSV"
#define SEARCH_VERSION        0x01
#define SEARCH_TIME_INTERVAL  0x01

typedef struct {
  char file_id[8]             GNUC_PACKED; /* = "SEARCHSV" */
  uint8_t version             GNUC_PACKED; /* = 0x01 */
  uint8_t reserved            GNUC_PACKED; /* Reserved, must be zero */
  uint16_t scan_points        GNUC_PACKED; /* the number of scan points */
  uint8_t time_interval       GNUC_PACKED; /* The interval of time in
                                              between scan points, in units
                                              of 0.5 seconds, must be 0x01 */
  msf_t points[EMPTY_ARRAY_SIZE] GNUC_PACKED; /* The series of scan points */
} SearchDat;

/* SPICONTX.SVD 
 */

#define SPICONTX_FILE_ID      "SPICONSV"
#define SPICONTX_VERSION      0x01

typedef struct {
  char file_id[8]             GNUC_PACKED; /* = "SPICONSV" */
  uint8_t version             GNUC_PACKED; /* = 0x01 */
  uint8_t reserved            GNUC_PACKED; /* Reserved, must be zero */
  struct {
    uint8_t ogt_info          GNUC_PACKED;
    uint8_t audio_info        GNUC_PACKED;
  } spi[1980]                 GNUC_PACKED;
  uint8_t reserved2[126]      GNUC_PACKED; /* 0x00 */
} SpicontxSvd;

/* SCANDATA.DAT for VCD 2.0 */

#define SCANDATA_FILE_ID "SCAN_VCD"
#define SCANDATA_VERSION 0x02

typedef struct {
  char file_id[8]             GNUC_PACKED; /* = "SCAN_VCD" */
  uint8_t version             GNUC_PACKED; /* = 0x02 */
  uint8_t reserved            GNUC_PACKED; /* Reserved, must be zero */
  uint16_t scan_points        GNUC_PACKED; /* the number of scan points */
  msf_t points[EMPTY_ARRAY_SIZE] GNUC_PACKED; /* actual scan points 
                                              points[time(iframe)/0.5] */
} ScandataDat_v2;

/* SCANDATA.DAT for SVCD
   This file fulfills much the same purpose of the SEARCH.DAT file except
   that this file is mandatory only if the System Profile Tag of the
   INFO.SVD file is 0x01 (HQ-VCD) and also that it contains sector addresses
   also for each video Segment Play Items in addition to the regular MPEG
   tracks. */

typedef struct {
  char file_id[8]             GNUC_PACKED; /* = "SCAN_VCD" */
  uint8_t version             GNUC_PACKED; /* = 0x01 */
  uint8_t reserved            GNUC_PACKED; /* Reserved, must be zero */
  uint16_t scandata_count     GNUC_PACKED; /* number of 3-byte entries in
                                              the table */
  uint16_t track_count        GNUC_PACKED; /* number of mpeg tracks on disc */
  uint16_t spi_count          GNUC_PACKED; /* number of consecutively recorded
                                              play item segments (as opposed
                                              to the number of segment play
                                              items). */
  msf_t cum_playtimes[EMPTY_ARRAY_SIZE] GNUC_PACKED; /* cumulative playing
                                              time up to track
                                              N. Track time just wraps
                                              at 99:59:74 */
} ScandataDat1;

typedef struct {
  /* ScandataDat head; */
  uint16_t spi_indexes[EMPTY_ARRAY_SIZE] GNUC_PACKED; /* Indexes into
                                              the following scandata
                                              table */
} ScandataDat2;

typedef struct {
  /* ScandataDat2 head; */
  uint16_t mpegtrack_start_index GNUC_PACKED; /* Index into the
                                              following scandata table
                                              where the MPEG track
                                              scan points start */
  
  /* The scandata table starts here */
  struct {
    uint8_t track_num          GNUC_PACKED;   /* Track number as in TOC */
    uint16_t table_offset      GNUC_PACKED;   /* Index into scandata table */
  } mpeg_track_offsets[EMPTY_ARRAY_SIZE] GNUC_PACKED;
} ScandataDat3;

typedef struct {
  /* ScandataDat3 head; */
  msf_t scandata_table[EMPTY_ARRAY_SIZE];
} ScandataDat4;


#ifdef __MWERKS__
#pragma options align=reset
#endif

#endif /* __VCD_FILES_PRIVATE_H__ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
