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

#include <libvcd/vcd_mmc_linux.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_assert.h>

#if defined(__linux__)

#include <libvcd/vcd_util.h>

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/cdrom.h>

#define NAME_MAX (256 - sizeof (int))

static const char _rcsid[] =
  "$Id$";

typedef struct
{
  int fd;			/* file descriptor */
  char name[NAME_MAX];		/* file name */
}
_gpkt_device_t;

/* convert enum cmd->data_direction to linux CGC_DATA_* */
static const uint8_t convert_data_direction[4] = {
  CGC_DATA_UNKNOWN,
  CGC_DATA_WRITE,
  CGC_DATA_READ,
  CGC_DATA_NONE
};

#if 0
static void
hexdump (uint8_t * buffer, int len)
{
  int i;
  for (i = 0; i < len; i++, buffer++)
    {
      if (i % 16 == 0)
	printf ("0x%04x ", i);
      printf ("%02x ", *buffer);
      if (i % 16 == 15)
	printf ("\n");
    }
  printf ("\n");
}
#endif

int
_vcd_mmc_linux_generic_packet (void *device, _vcd_mmc_command_t * cmd)
{
  _gpkt_device_t *_dev = device;
  int retry = 3;
  int ret = 0;
  struct request_sense sense;

  struct cdrom_generic_command cgc = {
    buffer:(cmd->buflen) ? (void *) cmd->buffer : 0,
    buflen:cmd->buflen,
    stat:0,
    sense:&sense,
    data_direction:CGC_DATA_UNKNOWN,
    quiet:0,
    timeout:cmd->timeout,
  };

  memcpy (&cgc.cmd, &cmd->cdb, 12);
  memset (&sense, 0, sizeof (sense));

  vcd_assert (cmd->data_direction <= _VCD_MMC_DIR_NONE
	      || cmd->data_direction >= _VCD_MMC_DIR_WRITE);

  cgc.data_direction = convert_data_direction[cmd->data_direction];

  vcd_assert (_dev);

  /* you probably don't want to see all the WRITE_10 commands */
  if (cmd->cdb[0]    != 0x2a /* CMD_WRITE_10 */ 
      && cmd->cdb[0] != 0x52 /* CMD_READ_TRACK_INFO */ ) 
  {
    uint8_t *cdb = cmd->cdb;
    vcd_debug ("%s: sending command: %02x %02x %02x %02x %02x %02x "
	       "%02x %02x %02x %02x %02x %02x", _dev->name,
	       cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5],
	       cdb[6], cdb[7], cdb[8], cdb[9], cdb[10], cdb[11]);
  }

  while (retry--)
    {				/* retry loop */
      if ((ret = ioctl (_dev->fd, CDROM_SEND_PACKET, &cgc)))
	{
	  uint32_t sense_key =
	    sense.sense_key << 16 | sense.asc << 8 | sense.ascq;

	  switch (sense_key)
	    {
	    case 0x020407:
	      vcd_debug ("got 'operation in progress'");
	      continue;
	    case 0x020408:
	      vcd_debug ("got 'long write in progress'");
	      continue;
	    case 0x030c09:
	      vcd_debug ("got 'write error -- loss of streaming'");
	      return 1;
	    case 0x000000:
	      vcd_debug ("got zero error");
	      perror ("generic packet");
	      /* continue; */
	      return 0;
	    }

	  perror ("CDROM_SEND_PACKET");
	  _vcd_recorder_mmc_dump_sense (_dev->name, cmd, sense_key);
	  vcd_error ("unexpected scsi problem");
	}
      else
	{
	  break;
	}
    }

  cmd->stat = cgc.stat;
  return ret;
}

void *
_vcd_mmc_linux_new_device (const char *device)
{
  _gpkt_device_t *_dev;
  int fd;

  if (!device)
    return NULL;

  fd = open (device, O_RDONLY | O_NONBLOCK);
  if (-1 == fd)
    {
      perror ("open ()");
      return 0;
    }

  _dev = _vcd_malloc (sizeof (_gpkt_device_t));
  _dev->fd = fd;
  strncpy (_dev->name, device, NAME_MAX);

  vcd_debug ("opened device %s as file descriptor %x", _dev->name, fd);

  return _dev;
}

void
_vcd_mmc_linux_destroy_device (void *user_data)
{
  _gpkt_device_t *_dev = user_data;

  if (_dev)
    {
      free (_dev);
    }
}


#else /* !defined (__linux__) */

int
_vcd_mmc_linux_generic_packet (void *device, _vcd_mmc_command_t * cmd)
{
  vcd_error ("Tried to access linux packed interface on non-linux system.\n");
  vcd_assert_not_reached ();
  return -1;
};

void *
_vcd_mmc_linux_new_device (const char *device)
{
  vcd_info ("Generic packet interface only supported on linux.\n");
  return NULL;
}

void
_vcd_mmc_linux_destroy_device (void *user_data)
{
  vcd_error ("Tried to access linux packed interface on non-linux system.\n");
  vcd_assert_not_reached ();
}

#endif /* defined (__linux__) */
