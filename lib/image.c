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

#include <cdio/cdio.h>

/* Public headers */
#include <libvcd/sector.h>
#include <cdio/iso9660.h>

/* Private headers */
#include "vcd_assert.h"
#include "image_sink.h"
#include "util.h"

static const char _rcsid[] = "$Id$";

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
