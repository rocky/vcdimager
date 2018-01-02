#   $Id$
#
#    Copyright (C) 2005 Rocky Bernstein <rocky@gnu.org>
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
# Sample programs
####################################################

noinst_PROGRAMS = info1 info2

AM_CPPFLAGS   = -I$(top_srcdir) $(LIBVCD_CFLAGS) $(LIBCDIO_CFLAGS)

info1_LDADD   = $(LIBVCDINFO_LIBS) $(LIBVCD_LIBS) $(LIBISO9660_LIBS)
info2_SOURCES = info2.cpp
info2_LDADD   = $(LIBVCDINFO_LIBS) $(LIBVCD_LIBS) $(LIBISO9660_LIBS)
