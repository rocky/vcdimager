/*
    $Id$

    Copyright (C) 2003 Herbert Valerio Riedel <hvr@gnu.org>

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

/* This file contains Linux-specific code and impliments low-level 
   control of the CD drive.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_bytesex.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_image_cd.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_util.h>

static const char _rcsid[] = "$Id$";

#if defined(__linux__) && defined(HAVE_LINUX_VERSION_H)
#include <linux/version.h>
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,16)
#  define __VCD_IMAGE_LINUXCD_BUILD
# endif
#endif

#if defined(__VCD_IMAGE_LINUXCD_BUILD) && defined(HAVE_LINUX_CDROM_H)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <linux/cdrom.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/* FIXME!!!!!
   The below two functions will be in libcdio's cdio_sector.h
   REMOVE THEM FROM HERE.
*/

#define CDIO_PREGAP_SECTORS 150

static void
cdio_lba_to_msf (uint32_t lba, msf_t *msf)
{
  vcd_assert (msf != 0);

  msf->m = to_bcd8 (lba / (60 * 75));
  msf->s = to_bcd8 ((lba / 75) % 60);
  msf->f = to_bcd8 (lba % 75);
}

static inline lba_t
cdio_lsn_to_lba (lsn_t lsn)
{
  return lsn + CDIO_PREGAP_SECTORS; 
}

/* END FIXME HACK. */

/* reader */

#define DEFAULT_VCD_DEVICE "/dev/cdrom"

typedef struct {
  int fd;

  int ioctls_debugged; /* for debugging */

  enum {
    _AM_NONE,
    _AM_IOCTL,
    _AM_READ_CD,
    _AM_READ_10
  } access_mode;

  char *device;
  
  bool init;
} _img_cd_src_t;

static void
_vcd_source_init (_img_cd_src_t *_obj)
{
  if (_obj->init)
    return;

  _obj->fd = open (_obj->device, O_RDONLY, 0);

  if (_obj->fd < 0)
    {
      vcd_error ("open (%s): %s", _obj->device, strerror (errno));
      return;
    }

  _obj->init = true;
}

/*!
  Release and free resources associated with cd. 
 */
static void
_vcd_source_free (void *user_data)
{
  _img_cd_src_t *_obj = user_data;

  if (NULL == _obj) return;
  free (_obj->device);

  if (_obj->fd >= 0)
    close (_obj->fd);

  free (_obj);
}

static int 
_set_bsize (int fd, unsigned bsize)
{
  struct cdrom_generic_command cgc;

  struct
  {
    uint8_t reserved1;
    uint8_t medium;
    uint8_t reserved2;
    uint8_t block_desc_length;
    uint8_t density;
    uint8_t number_of_blocks_hi;
    uint8_t number_of_blocks_med;
    uint8_t number_of_blocks_lo;
    uint8_t reserved3;
    uint8_t block_length_hi;
    uint8_t block_length_med;
    uint8_t block_length_lo;
  } mh;

  memset (&mh, 0, sizeof (mh));
  memset (&cgc, 0, sizeof (struct cdrom_generic_command));
  
  cgc.cmd[0] = 0x15;
  cgc.cmd[1] = 1 << 4;
  cgc.cmd[4] = 12;
  
  cgc.buflen = sizeof (mh);
  cgc.buffer = (void *) &mh;

  cgc.data_direction = CGC_DATA_WRITE;

  mh.block_desc_length = 0x08;
  mh.block_length_hi = (bsize >> 16) & 0xff;
  mh.block_length_med = (bsize >> 8) & 0xff;
  mh.block_length_lo = (bsize >> 0) & 0xff;

  return ioctl (fd, CDROM_SEND_PACKET, &cgc);
}

static int
__read_mode2 (int fd, void *buf, lba_t lba, unsigned nblocks, 
		 bool _workaround)
{
  struct cdrom_generic_command cgc;

  memset (&cgc, 0, sizeof (struct cdrom_generic_command));

  cgc.cmd[0] = _workaround ? GPCMD_READ_10 : GPCMD_READ_CD;
  
  cgc.cmd[2] = (lba >> 24) & 0xff;
  cgc.cmd[3] = (lba >> 16) & 0xff;
  cgc.cmd[4] = (lba >> 8) & 0xff;
  cgc.cmd[5] = (lba >> 0) & 0xff;

  cgc.cmd[6] = (nblocks >> 16) & 0xff;
  cgc.cmd[7] = (nblocks >> 8) & 0xff;
  cgc.cmd[8] = (nblocks >> 0) & 0xff;

  if (!_workaround)
    {
      cgc.cmd[1] = 0; /* sector size mode2 */

      cgc.cmd[9] = 0x58; /* 2336 mode2 */
    }

  cgc.buflen = 2336 * nblocks;
  cgc.buffer = buf;

#ifdef HAVE_LINUX_CDROM_TIMEOUT
  cgc.timeout = 500;
#endif
  cgc.data_direction = CGC_DATA_READ;

  if (_workaround)
    {
      int retval;

      if ((retval = _set_bsize (fd, 2336)))
	return retval;

      if ((retval = ioctl (fd, CDROM_SEND_PACKET, &cgc)))
	{
	  _set_bsize (fd, 2048);
	  return retval;
	}

      if ((retval = _set_bsize (fd, 2048)))
	return retval;
    }
  else
    return ioctl (fd, CDROM_SEND_PACKET, &cgc);

  return 0;
}

static int
_read_mode2 (int fd, void *buf, lba_t lba, unsigned nblocks, 
	     bool _workaround)
{
  unsigned l = 0;
  int retval = 0;

  while (nblocks > 0)
    {
      const unsigned nblocks2 = (nblocks > 25) ? 25 : nblocks;
      void *buf2 = ((char *)buf ) + (l * 2336);
      
      retval |= __read_mode2 (fd, buf2, lba + l, nblocks2, _workaround);

      if (retval)
	break;

      nblocks -= nblocks2;
      l += nblocks2;
    }

  return retval;
}

/*!
   Reads a single mode2 sector from cd device into data starting
   from lsn. Returns 0 if no error. 
 */
static int
_read_mode2_sectors (void *user_data, void *data, lsn_t lsn, bool form2,
		     unsigned nblocks)
{
  _img_cd_src_t *_obj = user_data;

  _vcd_source_init (_obj);

  if (form2)
    {
      char buf[M2RAW_SECTOR_SIZE] = { 0, };
      struct cdrom_msf *msf = (struct cdrom_msf *) &buf;
      msf_t _msf;
      
      cdio_lba_to_msf (cdio_lsn_to_lba(lsn), &_msf);
      msf->cdmsf_min0 = from_bcd8(_msf.m);
      msf->cdmsf_sec0 = from_bcd8(_msf.s);
      msf->cdmsf_frame0 = from_bcd8(_msf.f);

      if (_obj->ioctls_debugged == 75)
	vcd_debug ("only displaying every 75th ioctl from now on");

      if (_obj->ioctls_debugged == 30 * 75)
	vcd_debug ("only displaying every 30*75th ioctl from now on");

      if (_obj->ioctls_debugged < 75 
	  || (_obj->ioctls_debugged < (30 * 75)  
	      && _obj->ioctls_debugged % 75 == 0)
	  || _obj->ioctls_debugged % (30 * 75) == 0)
	vcd_debug ("reading %2.2d:%2.2d:%2.2d",
		   msf->cdmsf_min0, msf->cdmsf_sec0, msf->cdmsf_frame0);
     
      _obj->ioctls_debugged++;
 
    retry:
	switch (_obj->access_mode)
	  {
	  case _AM_NONE:
	    vcd_error ("no way to read mode2");
	    return 1;
	    break;

	  case _AM_IOCTL:
	    if (nblocks != 1)
	      {
		int i;
		
		for (i = 0; i < nblocks; i++)
		  if (_read_mode2_sectors (_obj, 
					   ((char *)data) + (M2RAW_SECTOR_SIZE * i),
					   lsn + i, form2, 1))
		    return 1;
	      }
	    else if (ioctl (_obj->fd, CDROMREADMODE2, &buf) == -1)
	      {
		perror ("ioctl()");
		return 1;
		/* exit (EXIT_FAILURE); */
	      }
	    else
	      memcpy (data, buf, M2RAW_SECTOR_SIZE);
	    break;

	  case _AM_READ_CD:
	  case _AM_READ_10:
	    if (_read_mode2 (_obj->fd, data, lsn, nblocks, 
			     (_obj->access_mode == _AM_READ_10)))
	      {
		perror ("ioctl()");
		if (_obj->access_mode == _AM_READ_CD)
		  {
		    vcd_info ("READ_CD failed; switching to READ_10 mode...");
		    _obj->access_mode = _AM_READ_10;
		    goto retry;
		  }
		else
		  {
		    vcd_info ("READ_10 failed; switching to ioctl(CDROMREADMODE2) mode...");
		    _obj->access_mode = _AM_IOCTL;
		    goto retry;
		  }
		return 1;
	      }
	    break;
	  }
    }
  else
    {
      int i;

      for (i = 0; i < nblocks; i++)
	{
	  char buf[M2RAW_SECTOR_SIZE] = { 0, };
	  int retval;

	  if ((retval = _read_mode2_sectors (_obj, buf, lsn + i, true, 1)))
	    return retval;

	  memcpy (((char *)data) + (M2F1_SECTOR_SIZE * i), buf + 8, 
		  M2F1_SECTOR_SIZE);
	}
    }

  return 0;
}

static uint32_t 
_vcd_stat_size (void *user_data)
{
  _img_cd_src_t *_obj = user_data;

  struct cdrom_tocentry tocent;
  uint32_t size;

  _vcd_source_init (_obj);

  tocent.cdte_track = CDROM_LEADOUT;
  tocent.cdte_format = CDROM_LBA;
  if (ioctl (_obj->fd, CDROMREADTOCENTRY, &tocent) == -1)
    {
      perror ("ioctl(CDROMREADTOCENTRY)");
      exit (EXIT_FAILURE);
    }

  size = tocent.cdte_addr.lba;

  return size;
}

/*!
  Set the key "arg" to "value" in source device.
*/
static int
_vcd_source_set_arg (void *user_data, const char key[], const char value[])
{
  _img_cd_src_t *_obj = user_data;

  if (!strcmp (key, "device"))
    {
      if (!value)
	return -2;

      free (_obj->device);
      
      _obj->device = strdup (value);
    }
  else if (!strcmp (key, "access-mode"))
    {
      if (!strcmp(value, "IOCTL"))
	_obj->access_mode = _AM_IOCTL;
      else if (!strcmp(value, "READ_CD"))
	_obj->access_mode = _AM_READ_CD;
      else if (!strcmp(value, "READ_10"))
	_obj->access_mode = _AM_READ_10;
      else
	vcd_error ("unknown access type: %s. ignored.", value);
    }
  else 
    return -1;

  return 0;
}

/*!
  Eject media in CD drive. If successful, as a side effect we 
  also free obj.
 */
static int 
_vcd_eject_media (void *user_data) {

  _img_cd_src_t *_obj = user_data;
  int ret, status;

  if (!_obj->init) _vcd_source_init(_obj);

  if (_obj->fd > -1) {
    if((status = ioctl(_obj->fd, CDROM_DRIVE_STATUS, CDSL_CURRENT)) > 0) {
      switch(status) {
      case CDS_TRAY_OPEN:
	if((ret = ioctl(_obj->fd, CDROMCLOSETRAY)) != 0) {
	  vcd_error ("CDROMCLOSETRAY failed: %s\n", strerror(errno));  
	}
	break;
      case CDS_DISC_OK:
	if((ret = ioctl(_obj->fd, CDROMEJECT)) != 0) {
	  vcd_error("CDROMEJECT failed: %s\n", strerror(errno));  
	}
	break;
      }
      _vcd_source_free((void *) _obj);
      return 0;
    } else {
      vcd_error ("CDROM_DRIVE_STATUS failed: %s\n", strerror(errno));
      _vcd_source_free((void *) _obj);
      return 1;
    }
  }
  return 2;
}

/*!
  Return a string containing the default VCD device if none is specified.
 */
static char *
_vcd_get_default_device()
{
  return strdup(DEFAULT_VCD_DEVICE);
}

#endif /* __VCD_IMAGE_LINUXCD_BUILD) && defined(HAVE_LINUX_CDROM_H)) */

VcdImageSource *
vcd_image_source_new_cd (void)
{
#if defined(__VCD_IMAGE_LINUXCD_BUILD) && defined(HAVE_LINUX_CDROM_H)
  _img_cd_src_t *_data = _vcd_malloc (sizeof *_data);

  vcd_image_source_funcs _funcs = {
    .eject_media       = _vcd_eject_media,
    .free              = _vcd_source_free,
    .get_default_device= _vcd_get_default_device,
    .read_mode2_sectors= _read_mode2_sectors,
    .set_arg           = _vcd_source_set_arg,
    .stat_size         = _vcd_stat_size
  };

  _data->device = _vcd_get_default_device();
  _data->access_mode = _AM_READ_CD;
  _data->init   = false;
  _data->fd = -1;

  return vcd_image_source_new (_data, &_funcs);

#elif defined(__linux__)
#if !defined(HAVE_LINUX_CDROM_H)
  vcd_error ("GNU/Linux CDd image source driver was not enabled in build due to old or missing linux/cdrom.h header at compile time.");
#else   
  vcd_error ("GNU/Linux cd image source driver was not enabled in build due to old or missing GNU/Linux kernel headers at compile time.");
#endif
  return NULL;
#else
# error GNU/Linux CD image source only supported under GNU/Linux
  vcd_assert_not_reached ();
  return NULL;
#endif
}


