/*
    $Id$

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>
    Copyright (C) 2002,2003 Rocky Bernstein <rocky@panix.com>

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

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_bytesex.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_image_cd.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_util.h>

static const char _rcsid[] = "$Id$";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef HAVE_SYS_CDIO_H
# include <sys/cdio.h> /* CDIOCALLOW etc... */
#else 
#error "You need <sys/cdio.h> to have CDROM support"
#endif

#include <sys/dkio.h>
#include <sys/scsi/generic/commands.h>
#include <sys/scsi/impl/uscsi.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#define DEFAULT_VCD_DEVICE "/vol/dev/aliases/cdrom0"

/* reader */

typedef struct {
  int fd;

  int ioctls_debugged; /* for debugging */

  enum {
    _AM_NONE,
    _AM_SUN_CTRL_ATAPI,
    _AM_SUN_CTRL_SCSI
#if FINISHED
    _AM_READ_CD,
    _AM_READ_10
#endif
  } access_mode;

  char *device;
  
  bool init;

  /* Track information */
  bool toc_init;                         /* if true, info below is valid. */
  struct cdrom_tochdr    tochdr;
  struct cdrom_tocentry  tocent[100];    /* entry info for each track */
  int                    total_tracks;   /* number of tracks on CD */

} _img_cd_src_t;

/*!
  Initialize CD device.
 */
static void
_vcd_source_init (_img_cd_src_t *_obj)
{

  struct dk_cinfo cinfo;

  if (_obj->init)
    return;

  _obj->fd = open (_obj->device, O_RDONLY, 0);

  if (_obj->fd < 0)
    {
      vcd_error ("open (%s): %s", _obj->device, strerror (errno));
      return;
    }

  if (_obj->access_mode == _AM_NONE) {
    /*
     * CDROMCDXA/CDROMREADMODE2 are broken on IDE/ATAPI devices.
     * Try to send MMC3 SCSI commands via the uscsi interface on
     * ATAPI devices.
     */
    if ( ioctl(_obj->fd, DKIOCINFO, &cinfo) == 0
	 && ((strcmp(cinfo.dki_cname, "ide") == 0) 
	     || (strncmp(cinfo.dki_cname, "pci", 3) == 0)) ) {
      _obj->access_mode = _AM_SUN_CTRL_ATAPI;
    } else {
      _obj->access_mode = _AM_SUN_CTRL_SCSI;    
    }
  }
  
  _obj->init = true;
  _obj->toc_init = false;
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

/*!
   Reads a single mode2 sector from cd device into data starting from lsn.
   Returns 0 if no error. 
 */
static int
_read_mode2_sector (void *user_data, void *data, lsn_t lsn, 
		    bool mode2_form2)
{
  char buf[M2RAW_SECTOR_SIZE] = { 0, };
  struct cdrom_msf *msf = (struct cdrom_msf *) &buf;
  msf_t _msf;

  _img_cd_src_t *_obj = user_data;

  _vcd_source_init (_obj);
  
  lba_to_msf (_vcd_lsn_to_lba(lsn), &_msf);
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
  
  switch (_obj->access_mode)
    {
    case _AM_NONE:
      vcd_error ("No way to read CD mode2.");
      return 1;
      break;
      
    case _AM_SUN_CTRL_SCSI:
      if (ioctl (_obj->fd, CDROMREADMODE2, &buf) == -1) {
	perror ("ioctl(..,CDROMREADMODE2,..)");
	return 1;
	/* exit (EXIT_FAILURE); */
      }
      break;
      
    case _AM_SUN_CTRL_ATAPI:
      {
	struct uscsi_cmd sc;
	union scsi_cdb cdb;
	int blocks = 1;
	int sector_type;
	int sync, header_code, user_data, edc_ecc, error_field;
	int sub_channel;
	
	sector_type = 0;		/* all types */
	/*sector_type = 1;*/	/* CD-DA */
	/*sector_type = 2;*/	/* mode1 */
	/*sector_type = 3;*/	/* mode2 */
	/*sector_type = 4;*/	/* mode2/form1 */
	/*sector_type = 5;*/	/* mode2/form2 */
	sync = 0;
	header_code = 2;
	user_data = 1;
	edc_ecc = 0;
	error_field = 0;
	sub_channel = 0;
	
	memset(&cdb, 0, sizeof(cdb));
	memset(&sc, 0, sizeof(sc));
	cdb.scc_cmd = 0xBE;
	cdb.cdb_opaque[1] = (sector_type) << 2;
	cdb.cdb_opaque[2] = (lsn >> 24) & 0xff;
	cdb.cdb_opaque[3] = (lsn >> 16) & 0xff;
	cdb.cdb_opaque[4] = (lsn >>  8) & 0xff;
	cdb.cdb_opaque[5] =  lsn & 0xff;
	cdb.cdb_opaque[6] = (blocks >> 16) & 0xff;
	cdb.cdb_opaque[7] = (blocks >>  8) & 0xff;
	cdb.cdb_opaque[8] =  blocks & 0xff;
	cdb.cdb_opaque[9] = (sync << 7) |
	  (header_code << 5) |
	  (user_data << 4) |
	  (edc_ecc << 3) |
	  (error_field << 1);
	cdb.cdb_opaque[10] = sub_channel;
	
	sc.uscsi_cdb = (caddr_t)&cdb;
	sc.uscsi_cdblen = 12;
	sc.uscsi_bufaddr = (caddr_t) buf;
	sc.uscsi_buflen = M2RAW_SECTOR_SIZE;
	sc.uscsi_flags = USCSI_ISOLATE | USCSI_READ;
	sc.uscsi_timeout = 20;
	if (ioctl(_obj->fd, USCSICMD, &sc)) {
	  perror("USCSICMD: READ CD");
	  return 1;
	}
	if (sc.uscsi_status) {
	  vcd_error("SCSI command failed with status %d\n", 
		    sc.uscsi_status);
	  return 1;
	}
	break;
      }
    }
  
  if (mode2_form2)
    memcpy (data, buf, M2RAW_SECTOR_SIZE);
  else
    memcpy (((char *)data), buf + 8, M2F1_SECTOR_SIZE);
  
  return 0;
}

/*!
   Reads nblocks of mode2 sectors from cd device into data starting
   from lsn.
   Returns 0 if no error. 
 */
static int
_read_mode2_sectors (void *user_data, void *data, uint32_t lsn, 
		     bool mode2_form2, unsigned nblocks)
{
  _img_cd_src_t *_obj = user_data;
  int i;
  int retval;

  for (i = 0; i < nblocks; i++) {
    if (mode2_form2) {
      if ( (retval = _read_mode2_sector (_obj, 
					  ((char *)data) + (M2RAW_SECTOR_SIZE * i),
					  lsn + i, true)) )
	return retval;
    } else {
      char buf[M2RAW_SECTOR_SIZE] = { 0, };
      if ( (retval = _read_mode2_sector (_obj, buf, lsn + i, true)) )
	return retval;
      
      memcpy (((char *)data) + (M2F1_SECTOR_SIZE * i), buf + 8, 
	      M2F1_SECTOR_SIZE);
    }
  }
  return 0;
}

/*!
   Return the size of the CD in logical block address (LBA) units.
 */
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
      if (!strcmp(value, "ATAPI"))
	_obj->access_mode = _AM_IOCTL;
      else if (!strcmp(value, "SCSI"))
	_obj->access_mode = _AM_READ_CD;
      else
	vcd_error ("unknown access type: %s. ignored.", value);
    }
  else 
    return -1;

  return 0;
}

/*! 
  Read and cache the CD's Track Table of Contents and track info.
  Return 0 if successful or 1 if an error.
*/
static int 
_vcd_read_toc (_img_cd_src_t *_obj) 
{
  int i;

  /* read TOC header */
  if ( ioctl(_obj->fd, CDROMREADTOCHDR, &_obj->tochdr) == -1 ) {
    vcd_error("%s: %s\n", 
            "error in ioctl CDROMREADTOCHDR", strerror(errno));
    return 1;
  }

  /* read individual tracks */
  for (i=_obj->tochdr.cdth_trk0; i<=_obj->tochdr.cdth_trk1; i++) {
    _obj->tocent[i-1].cdte_track = i;
    _obj->tocent[i-1].cdte_format = CDROM_MSF;
    if ( ioctl(_obj->fd, CDROMREADTOCENTRY, &_obj->tocent[i-1]) == -1 ) {
      vcd_error("%s %d: %s\n",
              "error in ioctl CDROMREADTOCENTRY for track", 
              i, strerror(errno));
      return 1;
    }
  }

  /* read the lead-out track */
  _obj->tocent[_obj->tochdr.cdth_trk1].cdte_track = CDROM_LEADOUT;
  _obj->tocent[_obj->tochdr.cdth_trk1].cdte_format = CDROM_MSF;

  if (ioctl(_obj->fd, CDROMREADTOCENTRY, 
	    &_obj->tocent[_obj->tochdr.cdth_trk1]) == -1 ) {
    vcd_error("%s: %s\n", 
	     "error in ioctl CDROMREADTOCENTRY for lead-out",
            strerror(errno));
    return 1;
  }

  _obj->total_tracks = _obj->tochdr.cdth_trk1;
  return 0;
}

/*!
  Eject media in CD drive. If successful, as a side effect we 
  also free obj.
 */
static int 
_vcd_eject_media (void *user_data) {

  _img_cd_src_t *_obj = user_data;
  int ret;

  if (!_obj->init) _vcd_source_init(_obj);

  if (_obj->fd > -1) {
    if ((ret = ioctl(_obj->fd, CDROMEJECT)) != 0) {
      _vcd_source_free((void *) _obj);
      vcd_error ("CDROMEJECT failed: %s\n", strerror(errno));
      return 1;
    } else {
      _vcd_source_free((void *) _obj);
      return 0;
    }
  }
  return 2;
}


static void *
_vcd_malloc_and_zero(size_t size) {
  void *ptr;

  if( !size ) size++;
    
  if((ptr = malloc(size)) == NULL) {
    vcd_error("malloc() failed: %s", strerror(errno));
    return NULL;
  }

  memset(ptr, 0, size);
  return ptr;
}

/*!
  Return a string containing the default VCD device if none is specified.
 */
static char *
_vcd_get_default_device(void)
{
  char *volume_device;
  char *volume_name;
  char *volume_action;
  char *device;
  struct stat stb;

  if ((volume_device = getenv("VOLUME_DEVICE")) != NULL &&
      (volume_name   = getenv("VOLUME_NAME"))   != NULL &&
      (volume_action = getenv("VOLUME_ACTION")) != NULL &&
      strcmp(volume_action, "insert") == 0) {

    device = _vcd_malloc_and_zero(strlen(volume_device) 
				  + strlen(volume_name) + 2);
    if (device == NULL)
      return strdup(DEFAULT_VCD_DEVICE);
    sprintf(device, "%s/%s", volume_device, volume_name);
    if (stat(device, &stb) != 0 || !S_ISCHR(stb.st_mode)) {
      free(device);
      return strdup(DEFAULT_VCD_DEVICE);
    }
    return device;
  }
  return strdup(DEFAULT_VCD_DEVICE);
}

/*!
  Return the number of tracks in the current medium.
*/
static unsigned int
_vcd_get_track_count(void *user_data) 
{
  _img_cd_src_t *_obj = user_data;
  
  if (!_obj->init) _vcd_source_init(_obj);
  if (!_obj->toc_init) _vcd_read_toc (_obj) ;

  return _obj->total_tracks-1;
}

/*!  
  Return the starting MSF (minutes/secs/frames) for track number
  track_num in obj.  Tracks numbers start at 1.
  The "leadout" track is specified either by
  using track_num LEADOUT_TRACK or the total tracks+1.
  NULL is returned if there is no entry.
*/
static int
_vcd_get_track_msf(void *user_data, unsigned int track_num, msf_t *msf)
{
  _img_cd_src_t *_obj = user_data;

  if (NULL == msf) return 1;

  if (!_obj->init) _vcd_source_init(_obj);
  if (!_obj->toc_init) _vcd_read_toc (_obj) ;

  if (track_num == VCD_LEADOUT_TRACK) track_num = _obj->total_tracks+1;

  if (track_num == 0 || track_num > _obj->total_tracks+1) {
    return 1;
  } else {
    struct cdrom_tocentry *msf0 = &_obj->tocent[track_num];
    msf->m = to_bcd8(msf0->cdte_addr.msf.minute);
    msf->s = to_bcd8(msf0->cdte_addr.msf.second);
    msf->f = to_bcd8(msf0->cdte_addr.msf.frame);
    return 0;
  }
}

/*!
  Return the size in bytes of the track that starts at "track".
  The first track is 1.
 */
static uint32_t
_vcd_get_track_size (void *user_data, const unsigned int track) 
{
  _img_cd_src_t *_obj = user_data;

  if (!_obj->init) _vcd_source_init(_obj);
  if (!_obj->toc_init) _vcd_read_toc (_obj) ;

  if (track == 0 || track > _obj->total_tracks) {
    return 0;
  } else {
    struct cdrom_tocentry *start_msf = &_obj->tocent[track];
    struct cdrom_tocentry *end_msf   = &_obj->tocent[track+1];
    uint32_t len = 75 - start_msf->cdte_addr.msf.frame;

    if (start_msf->cdte_addr.msf.second<60)
      len += (59 - start_msf->cdte_addr.msf.second) * 75;

    if (end_msf->cdte_addr.msf.minute > start_msf->cdte_addr.msf.minute) {
      len += (end_msf->cdte_addr.msf.minute - start_msf->cdte_addr.msf.minute-1) * 60 * 75;
      len += end_msf->cdte_addr.msf.second * 60;
      len += end_msf->cdte_addr.msf.frame ;
    }
    return len * M2F2_SECTOR_SIZE;
  }
}

/*!
  Initialization routine. This is the only thing that doesn't
  get called via a function pointer. In fact *we* are the
  ones to set that up.
 */
VcdImageSource *
vcd_image_source_new_cd (void)
{
  _img_cd_src_t *_data;

  vcd_image_source_funcs _funcs = {
    eject_media       : _vcd_eject_media,
    free              : _vcd_source_free,
    get_default_device: _vcd_get_default_device,
    get_track_count   : _vcd_get_track_count,
    get_track_msf     : _vcd_get_track_msf,
    get_track_size    : _vcd_get_track_size,
    read_mode2_sector : _read_mode2_sector,
    read_mode2_sectors: _read_mode2_sectors,
    stat_size         : _vcd_stat_size,
    set_arg           : _vcd_source_set_arg
  };

  _data = _vcd_malloc (sizeof (_img_cd_src_t));
  _data->device = _vcd_get_default_device();
  _data->access_mode = _AM_NONE;
  _data->fd = -1;

  return vcd_image_source_new (_data, &_funcs);
}
