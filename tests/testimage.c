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
#include <stdlib.h>
#include <string.h>

#include <libvcd/vcd_util.h>
#include <libvcd/vcd_assert.h>

#include <libvcd/vcd_image_nrg.h>
#include <libvcd/vcd_image_linuxcd.h>
#include <libvcd/vcd_image_bincue.h>
#include <libvcd/vcd_stream_stdio.h>

int
main (int argc, const char *argv[])
{
  VcdImageSource *img = NULL;
  uint32_t lsn = 0;

  vcd_assert (argc == 4);

  if (!strcmp ("nrg", argv[1]))
    {
      VcdDataSource *src = vcd_data_source_new_stdio (argv[2]);
      vcd_assert (src != NULL);

      img = vcd_image_source_new_nrg (src); 
    }
  else if (!strcmp ("bincue", argv[1]))
    {
      VcdDataSource *src = vcd_data_source_new_stdio (argv[2]);
      vcd_assert (src != NULL);

      img = vcd_image_source_new_bincue (src, NULL, false); 
    }
  else if (!strcmp ("linuxcd", argv[1]))
    img = vcd_image_source_new_linuxcd (argv[2]);
  else 
    vcd_error ("unrecognized img type");

  vcd_assert (img != NULL);

  {
    uint32_t n = vcd_image_source_stat_size (img);
    char buf[2336];
    lsn = atoi (argv[3]);

    vcd_debug ("size = %d", n);

    vcd_debug ("reading sector %d to testimage.out", lsn);
    
    if (!vcd_image_source_read_mode2_sector (img, buf, lsn, true))
      {
	struct m2f2sector
	{
	  uint8_t subheader[8];
	  uint8_t data[2324];
	  uint8_t spare[4];
	}
	*_sect = (void *) buf;
	FILE *fd;

	vcd_debug ("fn = %d, cn = %d, sm = 0x%x, ci = 0x%x",
		   _sect->subheader[0],
		   _sect->subheader[1],
		   _sect->subheader[2],
		   _sect->subheader[3]);

	fd = fopen ("testimage.out", "wb");
	fwrite (buf, sizeof (buf), 1, fd);
	fclose (fd);

	/* vcd_assert_not_reached (); */
      }
    else
      vcd_error ("failed...");

    
  }

  return 0;
}
