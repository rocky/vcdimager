/*
    $Id$

    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>

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

#ifndef _TRANSFER_H_
#define _TRANSFER_H_

#include <stdio.h>

#include "vcd_types.h"
#include "vcd_stream.h"
#include "vcd_salloc.h"
#include "vcd_directory.h"

void
write_mode2_sector(VcdDataSink *sink, bool sect2336, const void *data, 
                   uint32_t extent, uint8_t fnum, uint8_t cnum, 
                   uint8_t sm, uint8_t ci);

void
write_mode2_raw_sector(VcdDataSink *sink, bool sect2336, 
                       const void *data, uint32_t extent);

uint32_t
mknod_source_mode2_raw(VcdDirectory *dir, VcdDataSink *sink, VcdDataSource *source,
                       VcdSalloc *iso_bitmap, const char iso_pathname[]);

uint32_t
mknod_source_mode2_form1(VcdDirectory *dir, VcdDataSink *sink, VcdDataSource *source,
                         VcdSalloc *iso_bitmap, const char iso_pathname[]);

#endif /* _TRANSFER_H_ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
