/*
    $Id$

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>
    Copyright (C) 2001 Arnd Bergmann <arnd@itreff.de>

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

#include <string.h>
#include <stdio.h>

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_bytesex.h>
#include <libvcd/vcd_recorder.h>
#include <libvcd/vcd_image.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_recorder_mmc.h>
#include <libvcd/vcd_mmc_linux.h>
#include <libvcd/vcd_mmc_private.h>

static const char _rcsid[] =
  "$Id$";

typedef struct
{
  void *device;
  int (*wait_command) (void *device, _vcd_mmc_command_t * cmd);
  void (*destroy_device) (void *device);

  struct mode_page_header *write_parameters_header;
  struct write_parameters_mode_page *write_parameters;
  struct mode_page_header *capabilities_header;
  struct cd_capabilities_mode_page *capabilities;
}
_mmc_recorder_t;

static void
_print_write_parameters (_mmc_recorder_t * obj)
{
  struct write_parameters_mode_page *wp;
  const int tblen = 1024;
  char textbuf[tblen];
  char *s = textbuf;

  vcd_assert ((wp = obj->write_parameters));
  /* vcd_assert (wp->page_code == MP_WRITE_PARAMETERS); */

  s += snprintf (s, tblen, "write parameter page\n");
  s += snprintf (s, tblen, "\tsave page:\t%s\n", _vcd_bool_str (wp->ps));
  s += snprintf (s, tblen, "\tpage code:\t%02x\n", wp->page_code);
  s += snprintf (s, tblen, "\ttest write:\t%s\n", _vcd_bool_str (wp->test_write));
  s += snprintf (s, tblen, "\twrite type:\t%02x\n", wp->write_type);
  s += snprintf (s, tblen, "\tmulti session:\t%02x\n", wp->multi_session);
  s += snprintf (s, tblen, "\tfixed packet:\t%s\n", _vcd_bool_str (wp->fixed_packet));
  s += snprintf (s, tblen, "\tcopy:\t\t%s\n", _vcd_bool_str (wp->copy));
  s += snprintf (s, tblen, "\tdatablock type:\t%02x\n", wp->data_block_type);
  s += snprintf (s, tblen, "\thost app code:\t%02x\n", wp->host_appl_code);
  s += snprintf (s, tblen, "\tsession format:\t%02x\n", wp->session_format);
  s += snprintf (s, tblen, "\tpacket size:\t%02x\n", wp->packet_size);
  s += snprintf (s, tblen, "\taudio pause:\t%02x\n", wp->audio_pause_len);
  s += snprintf (s, tblen, "\tmedia catalog:\t\"%s\"\n", wp->media_catalog_no);
  s += snprintf (s, tblen, "\tisrc:\t\t\"%s\"\n", wp->isrc);

  vcd_debug (textbuf);
}

static int
_mode_sense (_mmc_recorder_t * obj, void *buffer, int buflen,
	     enum mode_sense_type mode_sense_type, int page)
{
  _vcd_mmc_command_t cmd = {
    cdb:{
	 CMD_MODE_SENSE_10, 0, mode_sense_type << 6 | (page & 0x3f),
	 0, 0, 0, 0, (buflen >> 8) & 0xff, buflen & 0xff, 0, 0},
  data_direction:_VCD_MMC_DIR_READ,
  buffer:buffer,
  buflen:buflen,
  timeout:500
  };

  return obj->wait_command (obj->device, &cmd);
}

#if 0
static void
hexdump (void *buf, int len)
{
  int i;
  uint8_t *buffer = buf;
  for (i = 0; i < len; i++, buffer++)
    {
      if (i % 16 == 0)
	printf ("%04x ", i);
      printf ("%02x ", *buffer);
      if (i % 16 == 15)
	printf ("\n");
    }
  printf ("\n");
}
#endif

static int
_mode_select (_mmc_recorder_t * obj, void *buffer, int buflen)
{
  _vcd_mmc_command_t cmd = {
    cdb:{
	 CMD_MODE_SELECT_10, 1 << 4, 0, 0, 0,
	 0, 0, (buflen >> 8) & 0xff, buflen & 0xff, 0, 0},
  data_direction:_VCD_MMC_DIR_WRITE,
  buffer:buffer,
  buflen:buflen,
  timeout:100
  };

  return obj->wait_command (obj->device, &cmd);
}

static int
_set_write_parameters_mode_page (_mmc_recorder_t * obj)
{
  vcd_assert (obj);
  vcd_assert (obj->write_parameters);
  return _mode_select (obj, (uint8_t *) obj->write_parameters_header,
		       UINT16_FROM_BE (obj->write_parameters_header->
				       mode_data_length) + 2);
}

static int
_get_mode_page (_mmc_recorder_t * obj, struct mode_page_header **page,
		enum mode_pages page_number,
		enum mode_sense_type mode_sense_type)
{
  struct mode_page_header header;
  int len = sizeof (header);
  int ret;

  vcd_assert (page);
  memset (&header, 0, len);
  if ((ret = _mode_sense (obj, &header, len, mode_sense_type, page_number)))
    {
      vcd_warn ("error reading mode page header %d", page_number);
      perror ("_get_mode_page");
      return ret;
    }

  len = 2 + UINT16_FROM_BE (header.mode_data_length);

  if (!(*page))
    {
      *page = _vcd_malloc (len);
    }

  if ((ret = _mode_sense (obj, *page, len, mode_sense_type, page_number)))
    {
      vcd_warn ("error reading mode page %d", page_number);
      perror ("_get_mode_page2");
    }

  return ret;
}


static int
_get_write_parameters_mode_page (_mmc_recorder_t * obj,
				 enum mode_sense_type mode_sense_type)
{
  struct mode_page_header **header = &obj->write_parameters_header;
  int ret =
    _get_mode_page (obj, header, MP_WRITE_PARAMETERS, mode_sense_type);

  /* the write parameters page starts after the block descriptor */
  obj->write_parameters = (struct write_parameters_mode_page *)
    ((uint8_t *) * header + 8 + UINT16_FROM_BE ((*header)->block_desc_len));

  return ret;
}

static int
_get_capabilities_mode_page (_mmc_recorder_t * obj,
			     enum mode_sense_type mode_sense_type)
{
  struct mode_page_header **header = &obj->capabilities_header;
  int ret = _get_mode_page (obj, header, MP_CD_CAPABILITIES, mode_sense_type);

  obj->capabilities = (struct cd_capabilities_mode_page *)
    ((uint8_t *) * header + 8 + UINT16_FROM_BE ((*header)->block_desc_len));

  return ret;
}

static int
_get_track_information (_mmc_recorder_t * obj, struct track_info *track_info,
			int track)
{
  int buflen = sizeof (struct track_info);
  _vcd_mmc_command_t cmd = {
    cdb:{CMD_READ_TRACK_INFO,},
  data_direction:_VCD_MMC_DIR_READ,
  buffer:track_info,
  buflen:buflen,
  timeout:500,
  };

  cmd.cdb[1] = 1; /* information for track number not sector number */
  cmd.cdb[5] = track;
  cmd.cdb[7] = (buflen >> 8) & 0xff;
  cmd.cdb[8] = (buflen >> 0) & 0xff;

  return obj->wait_command (obj->device, &cmd);
}

static int
_get_disc_information (_mmc_recorder_t * obj, struct disc_info *disc_info)
{
  int buflen = sizeof (struct disc_info);
  _vcd_mmc_command_t cmd = {
    cdb:{CMD_READ_DISC_INFO,},
  data_direction:_VCD_MMC_DIR_READ,
  buffer:disc_info,
  buflen:buflen,
  timeout:500,
  };

  cmd.cdb[7] = (buflen >> 8) & 0xff;
  cmd.cdb[8] = (buflen) & 0xff;

  return obj->wait_command (obj->device, &cmd);
}

static int
_synchronize_cache (_mmc_recorder_t * obj)
{
  _vcd_mmc_command_t cmd = {
    cdb:{CMD_SYNC_CACHE,},
  data_direction:_VCD_MMC_DIR_WRITE,
  timeout:2000,
  };

  return obj->wait_command (obj->device, &cmd);
}

static int
_reserve_track (void *user_data, unsigned int sectors)
{
  _mmc_recorder_t *obj = user_data;
  _vcd_mmc_command_t cmd = {
    cdb:{
         CMD_RESERVE_TRACK, 0, 0, 0, 0,
         (sectors >> 24) & 0xff,
         (sectors >> 16) & 0xff,
         (sectors >>  8) & 0xff,
         (sectors      ) & 0xff,
         },
    data_direction:_VCD_MMC_DIR_NONE,
    timeout:10000,
  };

  return obj->wait_command (obj->device, &cmd);
}

static int
_close_track (void *user_data, int track)
{
  _mmc_recorder_t *obj = user_data;
  _vcd_mmc_command_t cmd = {
    cdb:{CMD_CLOSE_TRACK,},
  data_direction:_VCD_MMC_DIR_NONE,
  timeout:track ? 2000 : 20000,
  };

  int ret = _synchronize_cache (obj);
  if (ret)
    {
      vcd_error ("could not synchronize the write cache");
      return ret;
    }

  cmd.cdb[1] = 0x00 & 0x01;		/* return after completion */
  if (track != 0)
    {
      cmd.cdb[2] = 0x01 & 0x03;		/* close track */
      cmd.cdb[5] = track & 0xff;	/* track number */
    }
  else
    {
      cmd.cdb[2] = 0x02 & 0x03;		/* close session */
    }

  return obj->wait_command (obj->device, &cmd);
}

static int
_cue_vcd_to_mmc (const vcd_cue_t * vcd_cue,
		 struct cue_sheet_data *mmc_cue, int track)
{
  msf_t _msf;
  const int VCD_CUE_OFFSET = 150;	/* Data area starts at 00:02:00 */

  if (track == 0)
    {
      lba_to_msf (vcd_cue->lsn, &_msf);
    }
  else
    {
      lba_to_msf (vcd_cue->lsn + VCD_CUE_OFFSET, &_msf);
    }

  mmc_cue->channels = 0;			/* data track */
  mmc_cue->data = 1;				/* data track */
  mmc_cue->copy = 0;				/* copy permitted */
  mmc_cue->preemph = 0;				/* data track */
  mmc_cue->cue_sheet_adr = ADR_TNO_IDX;		/* track/index address */
  mmc_cue->data_form_sub = FORM_SUB_ZERO;	/* let drive handle sub channels */
  mmc_cue->alt_copy = 0;			/* what is this? */
  mmc_cue->reserved = 0;
  mmc_cue->min = _msf.m;			/* minute */
  mmc_cue->sec = _msf.s;			/* second */
  mmc_cue->frame = _msf.f;			/* frame */

  switch (vcd_cue->type)
    {
    case VCD_CUE_LEADIN:
      vcd_assert (track == 0);
      mmc_cue->tno = track;				/* track number */
      mmc_cue->index = 0;				/* pre-gap is index 0 */
      mmc_cue->data_form_transfer = FORM_GENERATE_AUDIO;/* ignore sent data */
      mmc_cue->data_form_main = FORM_CDDA;		/* sector type */
      track++;
      break;
    case VCD_CUE_PREGAP_START:
      mmc_cue->tno = track;				/* track number */
      mmc_cue->index = 0; 				/* pre-gap is index 0 */
      mmc_cue->data_form_transfer = FORM_READ_DATA;	/* cdrdao does so too */
      mmc_cue->data_form_main = FORM_CDROM_XA;		/* sector type */
      break;
    case VCD_CUE_TRACK_START:
      mmc_cue->tno = track;				/* track number */
      mmc_cue->index = 1;				/* only have one index */
      mmc_cue->data_form_transfer = FORM_READ_DATA;	/* read data but not sync */
      mmc_cue->data_form_main = FORM_CDROM_XA;		/* sector type */
      track++;			/* manually keep track of the track number */
      break;
    case VCD_CUE_END:
      mmc_cue->tno = 0xAA;				/* magic number */
      mmc_cue->index = 1;				/* always index 1 */
      mmc_cue->data_form_transfer = FORM_GENERATE_AUDIO;/* generate empty
							 * red book sectors */
      mmc_cue->data_form_main = FORM_CDDA;		/* sector type */
      break;
    default:
      vcd_assert_not_reached ();
    }

  vcd_debug ("ctladr:0x%02x, tno:%02x, index:%02x, form:0x%02x "
	     "scms:0x%02x m:%02x s:%02x f:%02x",
	     mmc_cue->channels << 7 | mmc_cue->data << 6 | mmc_cue->copy << 5
	     | mmc_cue->preemph << 4 | mmc_cue->cue_sheet_adr,
	     mmc_cue->tno,
	     mmc_cue->index,
	     mmc_cue->data_form_sub << 6 | mmc_cue->data_form_main << 4
	     | mmc_cue->data_form_transfer,
	     mmc_cue->alt_copy << 7 | mmc_cue->reserved,
	     mmc_cue->min, mmc_cue->sec, mmc_cue->frame);
  return track;
}

static int
_send_cue_sheet (void *user_data, const VcdList * vcd_cue_list)
{
  /* this is ugly. it would be much better to get a valid cue
   * sheet from vcd_cue_list directly instead of having to
   * do all the strange fixups
   */

  _mmc_recorder_t *obj = user_data;
  int cue_sheet_size = _vcd_list_length (vcd_cue_list) + 2;
  int buflen = cue_sheet_size * sizeof (struct cue_sheet_data);
  struct cue_sheet_data cue_sheet[cue_sheet_size];

  _vcd_mmc_command_t cmd = {
    cdb:{CMD_SEND_CUE_SHEET,},
  data_direction:_VCD_MMC_DIR_WRITE,
  buffer:cue_sheet,
  buflen:buflen,
  timeout:1000,
  };

  VcdListNode *node = _vcd_list_begin (vcd_cue_list);
  int track = 1;
  int i;

  cmd.cdb[5] = (buflen >> 24) & 0xff;
  cmd.cdb[6] = (buflen >> 16) & 0xff;
  cmd.cdb[7] = (buflen >> 8) & 0xff;
  cmd.cdb[8] = (buflen >> 0) & 0xff;

  vcd_debug ("building cue sheet");
  {
    vcd_cue_t cue_start = { 0, VCD_CUE_LEADIN };
    _cue_vcd_to_mmc (&cue_start, &cue_sheet[0], 0);
    cue_start.type = VCD_CUE_PREGAP_START;
    _cue_vcd_to_mmc (&cue_start, &cue_sheet[1], 1);
  }

  for (i = 2; i < cue_sheet_size; i++)
    {
      vcd_cue_t *cue_node = _vcd_list_node_data (node);
      vcd_assert (cue_node);
      track = _cue_vcd_to_mmc (cue_node, &cue_sheet[i], track);
      node = _vcd_list_node_next (node);
    }
  vcd_assert (!node);

  obj->wait_command (obj->device, &cmd);

  return 0;
};

static int
_set_simulate (void *user_data, bool simulate)
{
  _mmc_recorder_t *obj = user_data;
  int ret;

  ret = _get_write_parameters_mode_page (obj, PAGE_CURRENT);
  if (ret)
    {
      return ret;
    }

  vcd_debug ("set simulate: %s", _vcd_bool_str (simulate));
  obj->write_parameters->test_write = simulate;
  return _set_write_parameters_mode_page (obj);
}

static int
_set_write_type (void *user_data, vcd_write_type_t write_type)
{
  _mmc_recorder_t *obj = user_data;
  int ret;

  ret = _get_write_parameters_mode_page (obj, PAGE_CURRENT);
  if (ret)
    {
      return ret;
    }

  switch (write_type)
    {
    case _VCD_WT_DAO:
      obj->write_parameters->write_type = WT_SAO;
      break;
    case _VCD_WT_VPKT:
      obj->write_parameters->write_type = WT_PACKET;
      obj->write_parameters->fixed_packet = false;
      break;
    default:
      vcd_assert_not_reached ();
    }
  vcd_debug ("set write type: %d", write_type);
  return _set_write_parameters_mode_page (obj);
}

static int
_set_data_block_type (void *user_data, vcd_data_block_type_t data_block_type)
{
  _mmc_recorder_t *obj = user_data;
  int ret;

  ret = _get_write_parameters_mode_page (obj, PAGE_CURRENT);
  if (ret)
    {
      return ret;
    }

  switch (data_block_type)
    {
    case _VCD_DB_RAW:
      obj->write_parameters->data_block_type = DB_RAW;
      break;
    case _VCD_DB_MODE2:
      obj->write_parameters->data_block_type = DB_MODE2;
      break;
    case _VCD_DB_XA_FORM2_S:
      obj->write_parameters->data_block_type = DB_XA_FORM2_S;
      break;
    default:
      vcd_assert_not_reached ();
    }
  ret = _set_write_parameters_mode_page (obj);
  if (ret)
    {
      return ret;
    }
  ret = _get_write_parameters_mode_page (obj, PAGE_CURRENT);
  /* _print_write_parameters (obj); */

  return ret;
}

/* a speed value of -1 will be interpreted as maximum speed by the drive */
static int
_set_speed (void *user_data, int read_speed, int write_speed)
{
  _mmc_recorder_t *obj = user_data;

  int rspeed = (1764 * read_speed + 5) / 10;
  int wspeed = (1764 * write_speed + 5) / 10;

  _vcd_mmc_command_t cmd = {
    cdb:{
	 CMD_SET_CD_SPEED,
	 0,
	 (rspeed >> 8) & 0xff,
	 rspeed & 0xff,
	 (wspeed >> 8) & 0xff,
	 wspeed & 0xff,
	 },
  data_direction:_VCD_MMC_DIR_NONE,
  timeout:100,
  };

  return obj->wait_command (obj->device, &cmd);
}

static int
_get_write_speed (void *user_data)
{
  _mmc_recorder_t *obj = user_data;
  if (!obj->capabilities)
    _get_capabilities_mode_page (obj, PAGE_CURRENT);

  return UINT16_FROM_BE (obj->capabilities->cur_write_speed);
}

static int
_get_next_track (void *user_data)
{
  _mmc_recorder_t *obj = user_data;
  struct disc_info disc_info;
  int ret;

  if ((ret = _get_disc_information (obj, &disc_info)))
    {
      vcd_warn ("cannot get disc information");
      return ret;
    }

  return disc_info.last_track_l;
}

static int
_get_next_writable (void *user_data, int track)
{
  _mmc_recorder_t *obj = user_data;
  struct track_info track_info;
  int ret;

  if ((ret = _get_track_information (obj, &track_info, track)))
    {
      vcd_warn ("cannot get information for track %d", track);
      return 0;
    }

  ret = UINT32_FROM_BE (track_info.next_writable);
#if 0
  vcd_debug ("nwa_v = %d, next_writable = %d", track_info.nwa_v, ret);
#endif
  if (track_info.nwa_v)
    {
      return ret;
    }
  else
    {
      vcd_warn ("next writable address for track %d is invalid", track);
      return 0;
    }
}

static int
_get_size (void *user_data, int track)
{
  _mmc_recorder_t *obj = user_data;
  struct track_info track_info;
  int start;
  int ret;

  if ((ret = _get_track_information (obj, &track_info, track)))
    {
      vcd_warn ("cannot get information for track %d", track);
      return 0;
    }

  start = UINT32_FROM_BE (track_info.track_start);

  if ((ret = _get_track_information (obj, &track_info, track + 1)))
    {
      vcd_warn ("cannot get information for track %d", track + 1);
      return 0;
    }

  return UINT32_FROM_BE (track_info.track_start) - start;
}

static int
_set_default_write_parameters (_mmc_recorder_t * obj)
{
  struct write_parameters_mode_page *wp;
  int ret;

  vcd_assert (obj);

  ret = _get_write_parameters_mode_page (obj, PAGE_CURRENT);
  if (ret)
    {
      return ret;
    }
  vcd_assert ((wp = obj->write_parameters));
  vcd_assert (wp->page_code == MP_WRITE_PARAMETERS);
  vcd_assert (wp->page_code == MP_WRITE_PARAMETERS);

  wp->ps = false;		/* don't save page */
  wp->test_write = false;	/* don't simulate write */
  wp->write_type = WT_SAO;	/* session at once */
  wp->multi_session = MS_NO_B0;	/* don't do multi session */
  wp->fixed_packet = false;	/* variable packet size */
  wp->copy = false;		/* copy permitted (?) */
  wp->data_block_type = DB_XA_FORM2_S;	/* mode 2 form 2 + subheader (2332 B) */
  wp->session_format = SF_CD_XA;	/* XA, not CDI (?) */
  wp->packet_size = 32768;	/* for fixed packets (byteorder ?) */
  return _set_write_parameters_mode_page (obj);
}

static int
_write_sectors (void *user_data, const uint8_t * buffer,
		int lsn, unsigned int buflen, int count)
{
  _mmc_recorder_t *obj = user_data;
  _vcd_mmc_command_t cmd = {
    cdb:{
	 CMD_WRITE_10,
	 0,
	 (lsn >> 24) & 0xff,
	 (lsn >> 16) & 0xff,
	 (lsn >> 8) & 0xff,
	 lsn & 0xff,
	 0,
	 0,
	 count,
	 },
  data_direction:_VCD_MMC_DIR_WRITE,
  buffer:buffer,
  buflen:buflen,
  timeout:10000,
  };

  return obj->wait_command (obj->device, &cmd);
}

static void
_free (void *user_data)
{
  _mmc_recorder_t *obj = user_data;
  obj->destroy_device (obj->device);
  if (obj->write_parameters_header)
    free (obj->write_parameters_header);
  if (obj->capabilities_header)
    free (obj->capabilities_header);
  free (obj);
}

void
_vcd_recorder_mmc_dump_sense (const char *name, _vcd_mmc_command_t * cmd,
			      uint32_t sense_key)
{
  uint8_t *cdb = cmd->cdb;

  vcd_info ("%s: command failed: %02x %02x %02x %02x %02x %02x "
	    "%02x %02x %02x %02x %02x %02x", name,
	    cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5],
	    cdb[6], cdb[7], cdb[8], cdb[9], cdb[10], cdb[11]);

  vcd_info ("buffer: %p len: %d", cmd->buffer, cmd->buflen);
  vcd_info ("timeout: %d", cmd->timeout);

  if (sense_key)
    {
      const char *msg;
      switch (sense_key)
	{
	  /* incomplete list, just add any codes you encounter */
	case 0x000000: msg = ""; break;
	case 0x020408: msg = "Logical Unit not Ready" 
			     " -- long write in progress"; break;
	case 0x023005: msg = "Cannot write medium"
			     " -- incompatible format"; break;
	case 0x023a00: msg = "Medium not present"; break;
	case 0x023a01: msg = "Medium not present -- tray closed"; break;
	case 0x023a02: msg = "Medium not present -- tray open"; break;
	case 0x052100: msg = "Logical Block Address out of range"; break;
	case 0x052400: msg = "Invalid field in CDB"; break;
	case 0x052600: msg = "Invalid field in parameter list"; break;
	case 0x052602: msg = "Parameter value invalid"; break;
	case 0x052c00: msg = "Command sequence error"; break;
	case 0x056300: msg = "End of user area encounterd on this track"; break;
	case 0x056400: msg = "Illegal mode for this track"; break;
	default: msg = "Unknown sense code"; break;
	}
      vcd_info ("sense %02x, asc %02x, ascq %02x: %s",
		sense_key >> 16 & 0xff, sense_key >> 8 & 0xff,
		sense_key & 0xff, msg);
    }
  else
    {
      vcd_info ("no sense information");
    };
}


VcdRecorder *
vcd_recorder_new_mmc (const char *device)
{
  _mmc_recorder_t *obj;
  void *mmc_device;

  vcd_recorder_funcs funcs = {
    set_simulate:_set_simulate,
    set_write_type:_set_write_type,
    set_data_block_type:_set_data_block_type,
    set_speed:_set_speed,
    get_next_writable:_get_next_writable,
    get_next_track:_get_next_track,
    get_size:_get_size,
    send_cue_sheet:_send_cue_sheet,
    write_sectors:_write_sectors,
    reserve_track:_reserve_track,
    close_track:_close_track,
    free:_free
  };

  obj = _vcd_malloc (sizeof (_mmc_recorder_t));
  /* scan low-level drivers, currently only linux 2.4 generic_packet is
   * supported, but others could be added here. */
  mmc_device = _vcd_mmc_linux_new_device (device);
  if (mmc_device)
    {
      obj->device = mmc_device;
      obj->wait_command = _vcd_mmc_linux_generic_packet;
      obj->destroy_device = _vcd_mmc_linux_destroy_device;
    }
  else
    {
      vcd_warn ("device %s could not be opened.\n", device);
      return NULL;
    }

  /* only cd writers have the write parameters mode page */
  if (_set_default_write_parameters (obj))
    {
      vcd_warn ("%s does not seem to be a cd-r device", device);
      _vcd_mmc_linux_destroy_device (mmc_device);
      return NULL;
    }

  _print_write_parameters (obj);
  _set_speed (obj, 1, 1);
  vcd_debug ("write speed: %d", _get_write_speed (obj));

  return vcd_recorder_new (obj, &funcs);
}
