# $Id$
#   $Id$
#
#    Copyright (C) 2004 Rocky Bernstein <rocky@panix.com>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
####################################################
# Things to make the libvcd and libvcdinfo librares
####################################################

# From libtool documentation:
#  1. Start with version information of `0:0:0' for each libtool library.
#
#  2. Update the version information only immediately before a public
#     release of your software.  More frequent updates are unnecessary,
#     and only guarantee that the current interface number gets larger
#     faster.
#
#  3. If the library source code has changed at all since the last
#     update, then increment REVISION (`C:R:A' becomes `C:r+1:A').
#
#  4. If any interfaces have been added, removed, or changed since the
#     last update, increment CURRENT, and set REVISION to 0.
#
#  5. If any interfaces have been added since the last public release,
#     then increment AGE.
#
#  6. If any interfaces have been removed since the last public release,
#     then set AGE to 0.

libvcdinfo_la_CURRENT := 0
libvcdinfo_la_REVISION := 1
libvcdinfo_la_AGE := 0

lib_LTLIBRARIES = libvcd.la libvcdinfo.la

INCLUDES = $(LIBVCD_CFLAGS) $(LIBCDIO_CFLAGS)

libvcd_la_LIBADD = $(LIBCDIO_LIBS) $(LIBISO9660_LIBS)
libvcd_la_SOURCES = \
	vcd_assert.h \
	bitvec.h \
	bytesex_asm.h \
	bytesex.h \
	data_structures.h \
	dict.h \
	directory.h \
	image_sink.h \
	mpeg.h \
	mpeg_stream.h \
	obj.h \
	pbc.h \
	salloc.h \
	sector_private.h \
	stream.h \
	stream_stdio.h \
	util.h \
	vcd.h \
	vcd.c \
	data_structures.c \
	directory.c \
	files.c \
	image.c \
	image_bincue.c \
	image_cdrdao.c \
	image_nrg.c \
	logging.c \
	mpeg.c \
	mpeg_stream.c \
	pbc.c \
	salloc.c \
	sector.c \
	stream.c \
	stream_stdio.c \
	util.c 

libvcdinfo_la_SOURCES = \
	info.c \
	inf.c \
	info_private.h \
	info_private.c \
	vcd_read.c \
	vcd_read.h 

libvcdinfo_la_LIBADD = $(LIBCDIO_LIBS) $(LIBISO9660_LIBS) $(LIBVCD_LIBS)
libvcdinfo_la_LDFLAGS = -version-info $(libvcdinfo_la_CURRENT):$(libvcdinfo_la_REVISION):$(libvcdinfo_la_AGE)

# EOF
