/*
    $Id$

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>
                  2002 Rocky Bernstein <rocky@panix.com>

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

#include <libvcd/vcd_util.h>
#include <libvcd/vcd_image.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_iso9660.h>

static const char _rcsid[] = "$Id$";

/*
 * VcdImageSource routines next.
 */

struct _VcdImageSource {
  void *user_data;
  vcd_image_source_funcs op;
};

/*!
  Eject media in CD drive if there is a routine to do so. 
  Return 0 if success and 1 for failure, and 2 if no routine.
 */
int
vcd_image_source_eject_media (VcdImageSource *obj)
{
  vcd_assert (obj != NULL);

  if (obj->op.eject_media) {
    return obj->op.eject_media (obj->user_data);
  } else {
    return 2;
  }
}

/*!
  Return a string containing the default VCD device if none is specified.
 */
char *
vcd_image_source_get_default_device (VcdImageSource *obj)
{
  vcd_assert (obj != NULL);

  if (obj->op.get_default_device) {
    return obj->op.get_default_device ();
  } else {
    return NULL;
  }
}

VcdImageSource *
vcd_image_source_new (void *user_data, const vcd_image_source_funcs *funcs)
{
  VcdImageSource *new_obj;

  new_obj = _vcd_malloc (sizeof (VcdImageSource));

  new_obj->user_data = user_data;
  new_obj->op = *funcs;

  return new_obj;
}

void
vcd_image_source_destroy (VcdImageSource *obj)
{
  vcd_assert (obj != NULL);
  
  obj->op.free (obj->user_data);
  free (obj);
}

int
vcd_image_source_read_mode2_sectors (VcdImageSource *obj, void *buf, 
				     uint32_t lsn, bool mode2raw, 
				     unsigned num_sectors)
{
  char *_buf = buf;
  const int blocksize = mode2raw ? M2RAW_SECTOR_SIZE : ISO_BLOCKSIZE;
  int n;

  vcd_assert (obj != NULL);
  vcd_assert (buf != NULL);
  vcd_assert (obj->op.read_mode2_sector != NULL 
	      || obj->op.read_mode2_sectors != NULL);
  
  if (obj->op.read_mode2_sectors)
    return obj->op.read_mode2_sectors (obj->user_data, buf, lsn,
				       mode2raw, num_sectors);

  /* fallback */
  for (n = 0; n < num_sectors; n++)
    {
      int rc = vcd_image_source_read_mode2_sector (obj, &_buf[n * blocksize],
						   lsn + n, mode2raw);
      
      if (rc)
	return rc;
    }
  
  return 0;
}

/*!
   Reads a single mode2 sector from cd device into data starting
   from lsn. Returns 0 if no error. 
 */
int
vcd_image_source_read_mode2_sector (VcdImageSource *obj, void *buf, 
				    uint32_t lsn, bool mode2raw)
{
  vcd_assert (obj != NULL);
  vcd_assert (buf != NULL);
  vcd_assert (obj->op.read_mode2_sector != NULL 
	      || obj->op.read_mode2_sectors != NULL);

  if (obj->op.read_mode2_sector)
    return obj->op.read_mode2_sector (obj->user_data, buf, lsn, mode2raw);

  /* fallback */
  return vcd_image_source_read_mode2_sectors (obj, buf, lsn, mode2raw, 1);

  /* not reached */
}

uint32_t
vcd_image_source_stat_size (VcdImageSource *obj)
{
  vcd_assert (obj != NULL);

  return obj->op.stat_size (obj->user_data);
}

/*!
  Set the arg "key" with "value" in the source device.
*/
int
vcd_image_source_set_arg (VcdImageSource *obj, const char key[],
			  const char value[])
{
  vcd_assert (obj != NULL);
  vcd_assert (obj->op.set_arg != NULL);
  vcd_assert (key != NULL);
  
  return obj->op.set_arg (obj->user_data, key, value);
}

/*
 * VcdImageSink routines next.
 */

struct _VcdImageSink {
  void *user_data;
  vcd_image_sink_funcs op;
};

VcdImageSink *
vcd_image_sink_new (void *user_data, const vcd_image_sink_funcs *funcs)
{
  VcdImageSink *new_obj;

  new_obj = _vcd_malloc (sizeof (VcdImageSink));

  new_obj->user_data = user_data;
  new_obj->op = *funcs;

  return new_obj;
}

void
vcd_image_sink_destroy (VcdImageSink *obj)
{
  vcd_assert (obj != NULL);
  
  obj->op.free (obj->user_data);
  free (obj);
}

int
vcd_image_sink_set_cuesheet (VcdImageSink *obj, const VcdList *vcd_cue_list)
{
  vcd_assert (obj != NULL);

  return obj->op.set_cuesheet (obj->user_data, vcd_cue_list);
}

int
vcd_image_sink_write (VcdImageSink *obj, void *buf, uint32_t lsn)
{
  vcd_assert (obj != NULL);

  return obj->op.write (obj->user_data, buf, lsn);
}

/*!
  Set the arg "key" with "value" in the target device.
*/

int
vcd_image_sink_set_arg (VcdImageSink *obj, const char key[],
			const char value[])
{
  vcd_assert (obj != NULL);
  vcd_assert (obj->op.set_arg != NULL);
  vcd_assert (key != NULL);

  return obj->op.set_arg (obj->user_data, key, value);
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
