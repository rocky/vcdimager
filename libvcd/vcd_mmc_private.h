/*
    $Id$

    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>
    Copyright (C) 2001 Arnd Bergmann <arnd@itreff.de>
    Copyright (C) 1999, 2000 Jens Axboe <axboe@suse.com>

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

#ifndef __VCD_MMC_PRIVATE_H__
#define __VCD_MMC_PRIVATE_H__

#include <libvcd/vcd_types.h>

/* Multimedia Commands, see [mmc], table N.1 */
enum mmc_command 
{
  CMD_TEST_UNIT_READY	= 0x00,
  CMD_REQUEST_SENSE	= 0x03,
  CMD_FORMAT_UNIT	= 0x04,
  CMD_INQUIRY		= 0x12,
  CMD_START_STOP_UNIT	= 0x1b,
  CMD_PREVENT_REMOVAL	= 0x1e,
  CMD_READ_CAPACITY	= 0x25,
  CMD_READ_10		= 0x28,
  CMD_WRITE_10		= 0x2a,
  CMD_SEEK		= 0x2b,
  CMD_SYNC_CACHE	= 0x35,
  CMD_READ_SUBCHANNEL	= 0x42,
  CMD_READ_TOC		= 0x43,
  CMD_READ_HEADER	= 0x44,
  CMD_STOP_PLAY_SCAN	= 0x4e,
  CMD_READ_DISC_INFO	= 0x51,
  CMD_READ_TRACK_INFO	= 0x52,
  CMD_RESERVE_TRACK	= 0x53,
  CMD_SEND_OPC_INFO	= 0x54,
  CMD_MODE_SELECT_10	= 0x55,
  CMD_REPAIR_TRACK	= 0x58,
  CMD_READ_MASTER_CUE	= 0x59,
  CMD_MODE_SENSE_10	= 0x5a,
  CMD_CLOSE_TRACK	= 0x5b,
  CMD_READ_BUFFER_CAPACITY = 0x5c,
  CMD_SEND_CUE_SHEET	= 0x5d,
  CMD_BLANK		= 0xa1,
  CMD_LOAD_UNLOAD_CD	= 0xa6,
  CMD_READ_12		= 0xa8,
  CMD_WRITE_12		= 0xaa,
  CMD_READ_CD_MSF	= 0xb9,
  CMD_SCAN		= 0xba,
  CMD_SET_CD_SPEED	= 0xbb,
  CMD_MECHANISM_STATUS	= 0xbd,
  CMD_READ_CD		= 0xbe
};

enum mode_pages
{
  MP_READ_ERROR_RECOVERY	= 0x01,
  MP_DISCONNECT_RECONNECT	= 0x02,
  MP_WRITE_PARAMETERS		= 0x05,
  MP_VERIFY_ERROR_PARAMETER	= 0x07,
  MP_CACHING			= 0x08,
  MP_PERIPHERAL_DEVICE   	= 0x09,
  MP_CONTROL			= 0x0a,
  MP_MEDIUM_TYPES_SUPPORTED	= 0x0b,
  MP_COMPACT_DISC		= 0x0d,
  MP_CD_AUDIO			= 0x0e,
  MP_POWER_CONDITION		= 0x1a,
  MP_INFORMATIONAL_EXCEPTIONS	= 0x1c,
  MP_CD_CAPABILITIES		= 0x2a,
  MP_ALL			= 0x3f,
};

enum mode_sense_type
{
  PAGE_CURRENT			= 0x00,
  PAGE_CHANGE			= 0x01,
  PAGE_DEFAULT			= 0x02,
  PAGE_SAVE			= 0x03
};

enum data_block_type
{
  DB_RAW	= 0,  /* 2352 bytes, raw data */
  DB_RAW_PQ	= 1,  /* 2368 bytes, raw data plus P and Q subchannel */
  DB_RAW_PW_I	= 2,  /* 2448 bytes, raw data plus P through W 
			 subchannels interleaved */
  DB_RAW_PW	= 3,  /* 2448 bytes, raw data plus P through W
			 subchannels sequential */
  DB_MODE1	= 8,  /* 2048 bytes, cd-rom mode 1 */
  DB_MODE2	= 9,  /* 2336 bytes, cd-rom mode 2 */
  DB_XA_FORM1	= 10, /* 2048 bytes, cd-rom mode 2 XA, form 1 */
  DB_XA_FORM1_S	= 11, /* 2056 bytes, cd-rom mode 2 XA, form 1 plus sub-header */
  DB_XA_FORM2	= 12, /* 2324 bytes, cd-rom mode 2 XA, form 2 */
  DB_XA_FORM2_S	= 13, /* 2332 bytes, cd-rom mode 2 XA, form 2 plus sub-header */
};

enum session_format
{
  SF_CDDA	= 0x00, /* CD audio or CD-ROM */
  SF_CDI	= 0x10, /* CD-I */
  SF_CD_XA	= 0x20  /* CD-XA */
};

/*
static int data_block_size[16] = {
  2352, 2368, 2448, 2448,   -1,   -1, -1, -1,
  2048, 2336, 2048, 2056, 2324, 2332, -1, -1
};
*/


PRAGMA_BEGIN_PACKED

struct disc_info 
{
  uint16_t length;
#if defined(BITFIELD_MSBF)
  bitfield_t  reserved1		: 3;
  bitfield_t  erasable		: 1;
  bitfield_t  border		: 2;		/* state of last session */
  bitfield_t  status		: 2;		/* disc status */
#else
  bitfield_t  status		: 2;
  bitfield_t  border		: 2;
  bitfield_t  erasable		: 1;
  bitfield_t  reserved1		: 3;
#endif
  uint8_t  n_first_track;	/* number of first track on d */
  uint8_t  n_sessions_l;	/* number of sessions */
  uint8_t  first_track_l;	/* first track in last session*/
  uint8_t  last_track_l;	/* last track in last session */
#if defined(BITFIELD_MSBF)
  bitfield_t  did_v		: 1;
  bitfield_t  dbc_v		: 1;
  bitfield_t  uru			: 1;
  bitfield_t  reserved2		: 5;
#else
  bitfield_t  reserved2		: 5;
  bitfield_t  uru 			: 1;
  bitfield_t  dbc_v 		: 1;
  bitfield_t  did_v 		: 1;
#endif
  enum session_format disc_type : 8;	/* disc type */
  uint8_t  n_sessions_m;
  uint8_t  first_track_m;
  uint8_t  last_track_m;
  uint32_t disc_id;
  uint32_t lead_in;
  uint32_t lead_out;
  char     disc_bar_code[8];
  uint8_t  reserved3;
  uint8_t  opc_entries;
} GNUC_PACKED;

#define struct_disc_info_SIZEOF 34

struct track_info 
{
  uint16_t info_length;
  uint8_t  track_number_l;
  uint8_t  session_number_l;
  uint8_t  reserved1;
#if defined(BITFIELD_MSBF)
  bitfield_t  reserved2		: 2;
  bitfield_t  damage		: 1;
  bitfield_t  copy			: 1;
  bitfield_t  track_mode		: 4;
  bitfield_t  rt			: 1;
  bitfield_t  blank 		: 1;
  bitfield_t  packet		: 1;
  bitfield_t  fp			: 1;
  bitfield_t  data_mode		: 4;
  bitfield_t  reserved3		: 6;
  bitfield_t  lra_v		: 1;
  bitfield_t  nwa_v		: 1;
#else
  bitfield_t  track_mode		: 4;
  bitfield_t  copy			: 1;
  bitfield_t  damage		: 1;
  bitfield_t  reserved2		: 2;
  bitfield_t  data_mode		: 4;
  bitfield_t  fp			: 1;
  bitfield_t  packet		: 1;
  bitfield_t  blank		: 1;
  bitfield_t  rt			: 1;
  bitfield_t  nwa_v		: 1;
  bitfield_t  lra_v		: 1;
  bitfield_t  reserved3		: 6;
#endif
  uint32_t track_start;
  uint32_t next_writable;
  uint32_t free_blocks;
  uint32_t packet_size;
  uint32_t track_size;
  uint32_t last_recorded;
  uint8_t  track_number_m;
  uint8_t  session_number_m;
  uint8_t  reserved4;
  uint8_t  reserved5;
} GNUC_PACKED;

#define struct_track_info_SIZEOF 36

enum write_type
{
  WT_PACKET	= 0x00,	/* fixed or variable length packet writing */
  WT_TAO	= 0x01, /* track at once */
  WT_SAO	= 0x02, /* session/disk at once */
  WT_RAW	= 0x03  /* raw write */
};

enum multi_session_field
{
  MS_NO_B0	= 0x00, /* no b0 pointer, no next session allowed */
  MS_FFFFFF	= 0x01, /* b0 = 0xffffff, no next session allowed */
  MS_MULTI	= 0x03  /* b0 points to next possible session	  */
};

struct mode_page_header
{
  uint16_t mode_data_length;
  uint8_t  medium_typer;
  uint8_t  device_specific;
  uint8_t  long_lba;
  uint8_t  reserved1;
  uint16_t block_desc_len;
  uint8_t  reserved2[8];
} GNUC_PACKED;

#define struct_mode_page_header_SIZEOF 16

struct write_parameters_mode_page
{
#if defined(BITFIELD_MSBF)
  bitfield_t ps 			: 1;		/* page savable */
  bitfield_t reserved1		: 1;
  bitfield_t page_code		: 6;		/* always 0x05h */
#else
  bitfield_t page_code		: 6;
  bitfield_t reserved1		: 1;
  bitfield_t ps 			: 1;
#endif
  uint8_t page_length;				/* 0x32 or 0x36 */
#if defined(BITFIELD_MSBF)
  bitfield_t reserved2		: 3;
  bitfield_t test_write		: 1;		/* 1 => dummy write */
  enum write_type write_type	: 4;		/* write types */
  enum multi_session_field multi_session
	  			: 2;		/* multi-session */
  bitfield_t fixed_packet		: 1;		/* fixed or variable pkts */
  bitfield_t copy			: 1;		/* copy protected medium */
  bitfield_t track_mode		: 4;		/* control nibble in all 
  						   1Q sub-channel, only for
						   raw writing */
  bitfield_t reserved3		: 4;
  enum data_block_type data_block_type : 4;	/* see above */
#else
  enum write_type write_type	: 4;
  bitfield_t test_write		: 1;
  bitfield_t reserved2		: 3;
  bitfield_t track_mode		: 4;
  bitfield_t copy			: 1;
  bitfield_t fixed_packet		: 1;
  enum multi_session_field multi_session
				: 2;
  enum data_block_type data_block_type : 4;
  bitfield_t reserved3		: 4;
#endif
  uint16_t reserved4;
#if defined(BITFIELD_MSBF)
  bitfield_t reserved5		: 2;
  bitfield_t host_appl_code	: 6;		/* host application code */
#else
  bitfield_t host_appl_code	: 6;
  bitfield_t reserved5		: 2;
#endif
  enum session_format session_format : 8;	/* session format */
  uint8_t reserved6;
  uint32_t packet_size;		/* for fixed length packets*/
  uint16_t audio_pause_len;	/* */
  char media_catalog_no[16];	/* text with catalog number */
  char isrc[16];		/* international stardard
  						   recording code */
  uint8_t sub_header[4];	/* four sub header bytes */
  uint32_t vendor_specific;	/* may be omitted */
} GNUC_PACKED;

#define struct_write_parameters_mode_page_SIZEOF 56

struct cd_capabilities_mode_page
{
  uint8_t page_header[16];			/* XXX have to look this up */
#if defined(BITFIELD_MSBF)
  bitfield_t ps 			: 1;		/* page savable */
  bitfield_t reserved1		: 1;
  bitfield_t page_code		: 6;		/* always 0x05h */
#else
  bitfield_t page_code		: 6;
  bitfield_t reserved1		: 1;
  bitfield_t ps 			: 1;
#endif
  uint8_t page_length;				/* 0x32 or 0x36 */
#if defined(BITFIELD_MSBF)
  bitfield_t reserved2		: 5;
  bool    method_2		: 1;	/* can read fixed packet
					   addressing mode 2 disks*/
  bool    cdrw_read		: 1;
  bool    cdr_read		: 1;
  
  bitfield_t reserved3		: 5;
  bool    test_write		: 1;
  bool    cdrw_write		: 1;
  bool    cdr_write		: 1;
  
  bitfield_t reserved4		: 1;
  bool    multi_session		: 1;
  bool    mode2_form2		: 1;
  bool    mode2_form1		: 1;
  bool    digital_port_2	: 1; /* supports digital output on port 2*/
  bool    digital_port_1	: 1; /* supports digital output on port 1*/
  bool    composite		: 1; /* can deliver composite A/V stream */
  bool    audio_play		: 1;

  bool    read_bar_code		: 1; /* can read disk bar codes */
  bool    UPC			: 1; /* can read media catalog number */
  bool    ISRC			: 1; /* can read ISRC information */
  bool    C2_pointers_support   : 1; /* can use C2 error pointers */
  bool    R_W_deinterleaved	: 1; /* subchannel data is deinterleaved */
  bool    R_W_supported		: 1; /* can read R to W subchannels */
  bool    CDDA_stream_accurate	: 1; /* can continue after loss of stream*/
  bool    CDDA_commands_support	: 1; /* can extract digital audio */

  bitfield_t loading_mech_type	: 3; /* 0: caddy, 1: tray, 2: pop-up
					4: changer, 5: cardridge changer */
  bitfield_t reserved5		: 1;
  bool    eject			: 1; /* can eject through software */
  bool    prevent_jumper	: 1; /* disk is locked though jumper */
  bool    lock_state		: 1; /* drive is in prevent (lock) mode */
  bool    lock			: 1; /* prevent/allow command will lock */

  bitfield_t reserved6		: 4;
  bool    sss			: 1; /* software slot selection               */
  bool    db_report_support	: 1; /* changer supports disc present reporting*/
  bool    seperate_mute		: 1; /* seperate audio channel mute supported */
  bool    seperate_vol		: 1; /* seperate audio volume settings        */
#else
  bool    cdr_read		: 1;
  bool    cdrw_read		: 1;
  bool    method_2		: 1;
  bitfield_t reserved2		: 5;
  
  bool    cdr_write		: 1;
  bool    cdrw_write		: 1;
  bool    test_write		: 1;
  bitfield_t reserved3		: 5;
  
  bool    audio_play		: 1;
  bool    composite		: 1;
  bool    digital_port_1	: 1;
  bool    digital_port_2	: 1;
  bool    mode2_form1		: 1;
  bool    mode2_form2		: 1;
  bool    multi_session		: 1;
  bitfield_t reserved4		: 1;

  bool    CDDA_commands_support	: 1;
  bool    CDDA_stream_accurate	: 1;
  bool    RW_supported		: 1;
  bool    RW_deinterleaved	: 1;
  bool    C2_pointers_support   : 1;
  bool    ISRC			: 1;
  bool    UPC			: 1;
  bool    read_bar_code		: 1;

  bool    lock			: 1;
  bool    lock_state		: 1;
  bool    prevent_jumper	: 1;
  bool    eject			: 1;
  bitfield_t reserved5		: 1;
  bitfield_t loading_mech_type	: 3; /* tray or caddy */

  bool    seperate_vol		: 1; /* seperate audio volume settings        */
  bool    seperate_mute		: 1; /* seperate audio channel mute supported */
  bool    db_report_support	: 1; /* changer supports disc present reporting*/
  bool    sss			: 1; /* software slot selection               */
  bitfield_t reserved6		: 4;
#endif

  uint16_t max_read_speed; /* maximum speed in kBps   */
  uint16_t vol_levels; /* number of volume levels */
  uint16_t bufsize; /* buffer size in kbytes   */
  uint16_t cur_read_speed; /* current speed in kBps   */

  uint8_t reserved7;
  
#if defined(BITFIELD_MSBF)
 /* digital audio output */
  bitfield_t reserved8		: 2;
  bitfield_t length		: 2;
  bool    lsbf			: 1;
  bool    rck			: 1;
  bool    bck			: 1;
  bitfield_t reserved9		: 1;
#else
  bitfield_t reserved9		: 1;
  bool    bck			: 1;
  bool    rck			: 1;
  bool    lsbf			: 1;
  bitfield_t length		: 2;
  bitfield_t reserved8		: 2;
#endif
  
  uint16_t max_write_speed; /* maximum write speed in kBps */
  uint16_t cur_write_speed; /* current write speed in kBps */
} GNUC_PACKED;

#define struct_cd_capabilities_mode_page_SIZEOF 38

struct opc_table
{
  uint16_t speed;
  uint8_t  opc_value[6];
} GNUC_PACKED;

#define struct_opc_table_SIZEOF 8
 
struct disc_capacity
{
  uint32_t lba;
  uint32_t block_length;
} GNUC_PACKED;

#define struct_disc_capacity_SIZEOF 8

enum cue_sheet_adr
{
  ADR_TNO_IDX      	= 0x01, /* Track or index identifier */
  ADR_CATALOG_CODE	= 0x02,
  ADR_ISRC_CODE		= 0x03 
};

enum data_form_sub
{
  FORM_SUB_ZERO		= 0x00, /* generate zero subchannel data */
  FORM_SUB_RAW		= 0x01, /* read subchannel data from buffer */
  FORM_SUB_PACK		= 0x03  /* read packed subchannel data from buffer */
};

enum data_form_main
{
  FORM_CDDA		= 0x00,	/* 2352 bytes audio 	   */
  FORM_CDROM		= 0x01, /* 2048 bytes mode1 data   */
  FORM_CDROM_XA		= 0x02, /* 2336 bytes xa/cd-1 form */
  FORM_CDROM_MODE2	= 0x03  /* 2336 bytes mode2 data   */
};

enum data_form_transfer
{
  FORM_READ_DATA	= 0x00, /* read sector from buffer */
  FORM_READ_DATA_SYNC	= 0x01, /* read sector and sync data from buffer */
  FORM_READ_AUDIO	= 0x00, /* read red book sector from buffer */
  FORM_GENERATE_AUDIO	= 0x01, /* generate empty red book data sectors */
  FORM_IGNORE_DATA	= 0x02, /* read sector but ignore and generate empty*/
  FORM_IGNORE_DATA_SYNC = 0x03, /* read sector and sync but ignore */
  FORM_GENERATE_DATA	= 0x04  /* generate empty data sectors */
};

struct cue_sheet_data
{
#if defined(BITFIELD_MSBF)
  bitfield_t channels 	: 1;	/* number of audio channels: 
  				   0 => stereo / data
				   1 => 4 channel audio (invalid)         */
  bitfield_t data 		: 1;	/* 0 => music; 1 => data                  */
  bitfield_t copy 		: 1;	/* 0 => copy prohibited; 1 => allowed     */
  bitfield_t preemph 	: 1;	/* 0 => no premphasis; 1 => 50/15µs emph. */
  enum cue_sheet_adr cue_sheet_adr
	  		: 4;	/* adr field; XXX endian correct?         */
#else
  enum cue_sheet_adr cue_sheet_adr
	  		: 4;
  bitfield_t preemph 	: 1;
  bitfield_t copy 		: 1;
  bitfield_t data 		: 1;
  bitfield_t channels 	: 1;
#endif
  uint8_t tno;		/* track number */
  uint8_t index;	/* index; 0 to 0x63 */
#if defined(BITFIELD_MSBF)
  enum data_form_sub data_form_sub
	  		: 2;	/* Data form of sub channel */
  enum data_form_main data_form_main
	  		: 2;	/* data form of main data   */
  enum data_form_transfer data_form_transfer
	  		: 4;	/* where to get the data    */
  bitfield_t alt_copy	: 1;	/* Alternate copy bit       */
  bitfield_t reserved	: 7;
#else
  enum data_form_transfer data_form_transfer
	  		: 4;
  enum data_form_main data_form_main  
	  		: 2;
  enum data_form_sub data_form_sub 
	  		: 2;
  bitfield_t reserved	: 7;
  bitfield_t alt_copy	: 1;
#endif 
  uint8_t min;	/* absolute time: minute  */
  uint8_t sec;	/* absolute time: second  */
  uint8_t frame;	/* absolute time: frame	  */
} GNUC_PACKED;

#define struct_cue_sheet_data_SIZEOF 8

PRAGMA_END_PACKED

typedef struct
{
  uint8_t  cdb[12]; /* command descriptor block */
  const void *buffer; 
  uint32_t buflen;
  int      stat;
  void     *sense;
  enum _vcd_mmc_data_direction
  {
    _VCD_MMC_DIR_WRITE = 1,
    _VCD_MMC_DIR_READ,
    _VCD_MMC_DIR_NONE
  } data_direction;
  uint32_t timeout;
} _vcd_mmc_command_t;

/* helper for low-level driver */
void
_vcd_recorder_mmc_dump_sense (const char *name, _vcd_mmc_command_t *cmd,
                              uint32_t sense_key);

#endif /* IMAGE_PRIVATE_H__ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
