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
#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_recorder.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_cd_sector.h>

static const char _rcsid[] =
  "$Id$";

struct _VcdRecorder
{
  uint8_t *buffer;	/* store sectors before they are written */
  int max_sector;	/* maximum amount of sectors in buffer */
  int next_sector;	/* number of next available sector in buffer */
  int sector_size;	/* size of a single sector */
  int next_writable;	/* nwa of the recordable cd */
  int current_track;	/* track that is used for next write command */
  bool closed;		/* disk has been closed */

  void *user_data;
  vcd_recorder_funcs op;
};

static void
_set_sector_size (VcdRecorder * obj, int sector_size)
{
  int bufsize = 128000;	/* op.get_bufsize(obj->user_data); XXX not yet implemented */
  vcd_assert (obj->next_sector == 0);	/* no sectors to be written */
  obj->sector_size = sector_size;
  obj->max_sector = bufsize / sector_size;
  if (!obj->buffer)
    obj->buffer = _vcd_malloc (bufsize);
}

VcdRecorder *
vcd_recorder_new (void *user_data, const vcd_recorder_funcs * funcs)
{
  VcdRecorder *new_obj;

  new_obj = _vcd_malloc (sizeof (VcdRecorder));

  new_obj->user_data = user_data;
  new_obj->op = *funcs;
  new_obj->current_track = 1;

  return new_obj;
}

void
vcd_recorder_destroy (VcdRecorder * obj)
{
  vcd_assert (obj != NULL);

  if (!obj->closed
      && (obj->op.get_next_writable (obj->user_data, obj->current_track) > 0))
    {
      vcd_info ("closing track %d", obj->current_track);
      obj->op.close_track (obj->user_data, obj->current_track);
      vcd_info ("closing session");
      obj->op.close_track (obj->user_data, 0);
    }
  obj->op.free (obj->user_data);
  if (obj->buffer)
    free (obj->buffer);
  free (obj);
}

static int
_vcd_recorder_flush_write (VcdRecorder * obj)
{
  int ret;
  int next_writable;
  int off = 0;

  if (obj->next_sector == 0)
    {
      /* nothing to do */
      return 0;
    }

  vcd_assert (obj->sector_size != 0);

  if (obj->next_writable == -150)
    {
      vcd_info ("writing lead-in");
      obj->op.write_sectors (obj->user_data, 0, -150, 0, 150);
      obj->next_writable = 0;
    }

  next_writable =
    obj->op.get_next_writable (obj->user_data, obj->current_track);

  /* if this happens, it is most likely a bug in the sector counting code */
  if (next_writable != obj->next_writable)
    {
      off = next_writable - obj->next_writable;
      vcd_warn ("nwa of cd is %d, expected %d", next_writable,
		obj->next_writable);
      if (off < obj->next_sector && off > 0)
	{
	  /* at least in variable packet writing mode, a buffer underrun
	   * is not fatal. The drive automatically creates link sectors
	   * so it can restart the writing. Unfortunately, we must give up
	   * a few sectors of the stream for the link blocks. */
	  vcd_warn ("buffer underrun? trying to work around");
	  obj->next_writable += off;
	  obj->next_sector -= off;
	}
      else
	{
	  obj->next_writable = next_writable;
	  /* vcd_assert_not_reached (); */
	}
    }

  /* do actual write */
  ret = obj->op.write_sectors (obj->user_data,
			       obj->buffer,
			       obj->next_writable,
			       obj->sector_size * obj->next_sector,
			       obj->next_sector);

  if (ret == _VCD_ERR_BUR)
    {
      /* attempt to recover from buffer underrun */
      if (
	  (ret =
	   obj->op.get_next_writable (obj->user_data,
				      obj->current_track)) >
	  obj->next_writable)
	{
	  vcd_warn ("Buffer underrun occured, skipping %d sectors",
		    ret - obj->next_writable);

	  obj->next_writable = ret;
	  ret = obj->op.write_sectors (obj->user_data,
				       obj->buffer,
				       obj->next_writable,
				       obj->sector_size * obj->next_sector,
				       obj->next_sector);
	}
      else
	{
	  vcd_error ("cannot recover from buffer underrun at sector %d", ret);
	}
    }

  /* reset buffer */
  obj->next_writable += obj->next_sector;
  if (off == 0)
    {
      obj->next_sector = 0;
    }
  else
    {
      /* put remaining sectors to beginning of buffer */
      memmove (obj->buffer,
	       obj->buffer + obj->sector_size * obj->next_sector,
	       obj->sector_size * off);
      obj->next_sector = off;
    }

  return ret;
}

int
vcd_recorder_write_sector (VcdRecorder * obj, const uint8_t * buffer, int lsn)
{
  vcd_assert (obj != NULL);

  /* append sector data to buffer */
  switch (obj->sector_size)
    {
    case M2SUB_SECTOR_SIZE:
      /* 2332 bytes: sub-header plus 2324 data bytes */
      memcpy (obj->buffer + M2SUB_SECTOR_SIZE * obj->next_sector,
	      buffer + 12 + 4, M2SUB_SECTOR_SIZE);
      break;
    case M2RAW_SECTOR_SIZE:
      /* seems to be broken. My Yamaha 6416S writes 2048 byte FORM1 sectors
       * instead of 2336 byte FORM1/2 sectors. */
      memcpy (obj->buffer + M2RAW_SECTOR_SIZE * obj->next_sector,
	      buffer + 12 + 4, M2RAW_SECTOR_SIZE);
      break;
    case CDDA_SECTOR_SIZE:
      /* behaviour is not defined for packet writing */
      memcpy (obj->buffer + CDDA_SECTOR_SIZE * obj->next_sector, buffer, CDDA_SECTOR_SIZE);
      break;
    case 0:
      vcd_error ("sector size not set");
      break;
    default:
      vcd_assert_not_reached ();
    }
  ++obj->next_sector;

  /* write if buffer full */
  if (obj->next_sector == obj->max_sector)
    return _vcd_recorder_flush_write (obj);
  else				/* write deferred */
    return 0;
}

int
vcd_recorder_set_current_track (VcdRecorder * obj, int track)
{
  int ret;

  vcd_assert (obj != NULL);
  vcd_assert (track >= 1 && track <= 99);

  ret = obj->op.get_next_writable (obj->user_data, track);

  if (ret == _VCD_ERR_NWA_INV)
    return 1;

  obj->next_writable = ret;
  obj->current_track = track;

  return 0;
}


/*
 * virtual functions
 */

int
vcd_recorder_set_simulate (VcdRecorder * obj, bool simulate)
{
  vcd_assert (obj != NULL);

  return obj->op.set_simulate (obj->user_data, simulate);
}

int
vcd_recorder_set_write_type (VcdRecorder * obj, vcd_write_type_t write_type)
{
  int ret;
  vcd_assert (obj != NULL);

  ret = obj->op.set_write_type (obj->user_data, write_type);

  if (write_type == _VCD_WT_DAO)
    obj->next_writable = obj->op.get_next_writable (obj->user_data, 1);

  if (obj->next_writable == _VCD_ERR_NWA_INV)
    return 1;

  return ret;
}

int
vcd_recorder_set_data_block_type (VcdRecorder * obj,
				  vcd_data_block_type_t data_block_type)
{
  vcd_assert (obj != NULL);

  switch (data_block_type)
    {
    case _VCD_DB_RAW:
      _set_sector_size (obj, CDDA_SECTOR_SIZE);
      break;
    case _VCD_DB_MODE2:
      _set_sector_size (obj, M2RAW_SECTOR_SIZE);
      break;
    case _VCD_DB_XA_FORM2_S:
      _set_sector_size (obj, M2SUB_SECTOR_SIZE);
      break;
    }

  return obj->op.set_data_block_type (obj->user_data, data_block_type);
}

int
vcd_recorder_set_speed (VcdRecorder * obj, int read_speed, int write_speed)
{
  vcd_assert (obj != NULL);

  return obj->op.set_speed (obj->user_data, read_speed, write_speed);
}

int
vcd_recorder_get_next_writable (VcdRecorder * obj, int track)
{
  vcd_assert (obj != NULL);

  return obj->op.get_next_writable (obj->user_data, track);
}

int
vcd_recorder_get_next_track (VcdRecorder * obj)
{
  vcd_assert (obj != NULL);

  return obj->op.get_next_track (obj->user_data);
}

int
vcd_recorder_get_size (VcdRecorder * obj, int track)
{
  vcd_assert (obj != NULL);

  return obj->op.get_size (obj->user_data, track);
}

int
vcd_recorder_send_cue_sheet (VcdRecorder * obj, const VcdList * vcd_cue_list)
{
  vcd_assert (obj != NULL);

  return obj->op.send_cue_sheet (obj->user_data, vcd_cue_list);
}

int
vcd_recorder_write_sectors (VcdRecorder * obj, const uint8_t * buffer,
			    int lsn, unsigned int buflen, int count)
{
  vcd_assert (obj != NULL);

  return obj->op.write_sectors (obj->user_data, buffer, lsn, buflen, count);
}

int
vcd_recorder_reserve_track (VcdRecorder *obj, unsigned int sectors)
{
  vcd_assert (obj != NULL);

  return obj->op.reserve_track (obj->user_data, sectors);
}

int
vcd_recorder_close_track (VcdRecorder * obj, int track)
{
  vcd_assert (obj != NULL);

  _vcd_recorder_flush_write (obj);

  if (track == 0)
    {
      obj->closed = true;
    }
  else
    {
      obj->next_writable += (150 + 2);	/* is this always correct? */
      obj->current_track++;
    }
  return obj->op.close_track (obj->user_data, track);
}
