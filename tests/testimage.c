/*
    $Id$

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>

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

#include <stdio.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_assert.h>

#include <libvcd/vcd_image_nrg.h>
#include <libvcd/vcd_image_linuxcd.h>
#include <libvcd/vcd_image_bincue.h>
#include <libvcd/vcd_stream_stdio.h>

int
main (int argc, const char *argv[])
{
  VcdImageSource *img;
  VcdDataSource *src;

  src = vcd_data_source_new_stdio (argv[1]);

  vcd_assert (src != NULL);

  img = vcd_image_source_new_nrg (src);

  {
    uint32_t n = vcd_image_source_stat_size (img);
    char buf[2336];

    vcd_debug ("size = %d", n);

    while (n > 0)
      vcd_image_source_read_mode2_sector (img, buf, --n, true);
  }

  return 0;
}
