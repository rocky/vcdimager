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

#ifndef __VCDXML_H__
#define __VCDXML_H__

#include <libxml/tree.h>

struct vcdxml_t {
  xmlChar *class;
  xmlChar *version;
  xmlChar *options;
  struct {
    xmlChar *album_id;
    xmlChar *volume_count;
    xmlChar *volume_number;
    xmlChar *restriction;
    xmlChar *time_offset;
  } info;
  struct {
    xmlChar *volume_id;
    xmlChar *system_id;
    xmlChar *application_id;
    xmlChar *preparer_id;
    xmlChar *publisher_id;
  } pvd;

  unsigned mpeg_tracks_count;
  struct mpeg_tracks_t {
    xmlChar *id;
    xmlChar *src;
    unsigned entry_points_count;
    struct entry_points_t {
      xmlChar *id;
      xmlChar *timestamp;
    } *entry_points;
  } *mpeg_tracks;
};

#endif /* __VCDXML_H__ */
