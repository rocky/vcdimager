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

#include <libvcd/vcd_types.h>
#include <libvcd/vcd_data_structures.h>
#include <libvcd/vcd_pbc.h>

struct vcdxml_t {
  char *comment; /* just a xml comment... */

  vcd_type_t vcd_type;
  VcdList *option_list;

  struct {
    char *album_id;
    unsigned volume_count;
    unsigned volume_number;
    unsigned restriction;
    bool use_sequence2;
    bool use_lid2;
    double time_offset;

    /* used for restoring vcd */
    unsigned psd_size;
    unsigned max_lid;
    unsigned segments_start;
  } info;

  struct {
    char *volume_id;
    char *system_id;
    char *application_id;
    char *preparer_id;
    char *publisher_id;
  } pvd;

  VcdList *pbc_list;

  VcdList *sequence_list;

  VcdList *segment_list;

  VcdList *filesystem;
};

struct option_t {
  char *name;
  char *value;
};

struct sequence_t {
  char *id;
  char *src;
  VcdList *entry_point_list; /* entry_point_t */
  VcdList *autopause_list; /* double * */

  /* used for restoring */
  uint32_t start_extent;
};

struct entry_point_t {
  char *id;
  double timestamp;
  
  /* used for restoring */
  uint32_t extent;
};

struct segment_t
{
  char *id;
  char *src;
  
  /* used for restoring vcds */
  unsigned segments_count;
};

struct filesystem_t
{
  char *name;
  char *file_src; /* if NULL then dir */
  bool file_raw;
};

#endif /* __VCDXML_H__ */
