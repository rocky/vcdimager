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

#ifndef __VCD_IMAGE_CD_H__
#define __VCD_IMAGE_CD_H__

#include <libvcd/vcd_image.h>
#include <libvcd/vcd_image_linuxcd.h>
#include <libvcd/vcd_image_bsdicd.h>

static VcdImageSource *
vcd_image_source_new_cd (void)
{
#if defined(__linux__)
  return vcd_image_source_new_linuxcd ();
#elif defined(__bsdi__)
  return vcd_image_source_new_bsdcd ();
#else
  vcd_error ("no CD-ROM image driver available for this architecture (%s)",
	     HOST_ARCH);
  return NULL;
#endif
}

#endif /* __VCD_IMAGE_CD_H__ */
