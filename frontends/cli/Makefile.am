# Copyright (C) 2003, 2017 Rocky Bernstein <rocky@gnu.org>
# Copyright (C) 2003 Herbert Valerio Riedel <hvr@gnu.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.
bin_PROGRAMS = vcdimager cdxa2mpeg vcd-info

AM_CPPFLAGS = -I$(top_srcdir) $(LIBPOPT_CFLAGS) $(LIBVCD_CFLAGS) $(LIBCDIO_CFLAGS) $(LIBISO9660_CFLAGS)

vcdimager_SOURCES = vcdimager.c
vcdimager_LDADD =  $(LIBISO9660_LIBS) $(LIBVCD_LIBS) $(LIBPOPT_LIBS) $(LIBCDIO_LIBS)

cdxa2mpeg_SOURCES = cdxa2mpeg.c
cdxa2mpeg_LDADD =  $(LIBISO9660_LIBS) $(LIBVCD_LIBS) $(LIBPOPT_LIBS)

vcd_info_SOURCES = vcd-info.c
vcd_info_LDADD = $(LIBISO9660_LIBS) $(LIBVCDINFO_LIBS) $(LIBVCD_LIBS) $(LIBPOPT_LIBS) $(LIBCDIO_LIBS) $(LIBISO9660_LIBS)

man_MANS = vcdimager.1 cdxa2mpeg.1 vcd-info.1

if MAINTAINER_MODE
vcdimager.1: vcdimager$(EXEEXT)
	-$(HELP2MAN) --name "Generates simple pbc-less VCD and SVCD disc images directly" --no-info --libtool -o $@ ./$<

cdxa2mpeg.1: cdxa2mpeg$(EXEEXT)
	-$(HELP2MAN) --name "Simple tool for converting RIFF CDXA file to plain mpeg" --no-info --libtool -o $@ ./$<

vcd-info.1: vcd-info$(EXEEXT)
	-$(HELP2MAN) --name "Display (selectively) the contents of a Video CD or CD image" --no-info --libtool -o $@ ./$<
endif

MAINTAINERCLEANFILES = $(man_MANS)

EXTRA_DIST = $(man_MANS)
