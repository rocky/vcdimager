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

#ifndef __VCD_IMAGE_BINCUE_H__
#define __VCD_IMAGE_BINCUE_H__

#include <libvcd/vcd_stream.h>
#include <libvcd/vcd_image.h>

VcdImageSink *
vcd_image_sink_new_bincue (VcdDataSink *bin_sink, 
			   VcdDataSink *cue_sink,
			   const char cue_fname[],
			   bool sector_2336);

VcdImageSource *
vcd_image_source_new_bincue (VcdDataSource *bin_source,
			     VcdDataSource *cue_source,
			     bool sector_2336);

#endif /* __VCD_IMAGE_BINCUE_H__ */
