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

#ifndef __VCD_IMAGE_FS_H__
#define __VCD_IMAGE_FS_H__

#include <libvcd/vcd_image.h>
#include <libvcd/vcd_types.h>
#include <libvcd/vcd_data_structures.h>
#include <libvcd/vcd_xa.h>

typedef struct {
  enum { _STAT_FILE = 1, _STAT_DIR = 2 } type;
  uint32_t lsn; /* start logical sector number */
  uint32_t size; /* total size in bytes */
  uint32_t secsize; /* number of sectors allocated */
  vcd_xa_t xa; /* XA attributes */
} vcd_image_stat_t;

int
vcd_image_source_fs_stat (VcdImageSource *obj,
			  const char pathname[], vcd_image_stat_t *buf);

VcdList * /* list of char* -- caller must free it */
vcd_image_source_fs_readdir (VcdImageSource *obj,
			     const char pathname[]);

#endif /* __VCD_IMAGE_FS_H__ */
