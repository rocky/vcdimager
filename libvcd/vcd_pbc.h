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

#ifndef __VCD_PBC_H__
#define __VCD_PBC_H__

#include <libvcd/vcd_types.h>
#include <libvcd/vcd_obj.h>

typedef struct {
  enum {
    PBC_INVALID = 0,
    PBC_PLAY,
    PBC_SELECTION,
    PBC_END
  } type;

  char *id;

  /* used for play/selection lists */
  char *prev_id;
  char *next_id;
  char *retn_id;

  /* used for play lists */
  double playing_time;
  unsigned wait_time;
  unsigned auto_pause_time;
  VcdList *item_id_list; /* char */

  /* used for selection lists */
  unsigned bsn;
  VcdList *default_id_list; /* char */
  char *timeout_id;
  unsigned timeout_time;
  unsigned loop_count;
  bool jump_delayed;
  char *item_id;
  VcdList *select_id_list; /* char */

  /* used for end lists */
  char *image_id;
  unsigned next_disc_no;

  /* computed... */
  unsigned lid;
  unsigned offset;
  unsigned offset_ext;
} pbc_t;

enum item_type_t {
  ITEM_TYPE_NOTFOUND = 0,
  ITEM_TYPE_TRACK,
  ITEM_TYPE_ENTRY,
  ITEM_TYPE_SEGMENT,
  ITEM_TYPE_PBC
};

/* functions */

unsigned
_vcd_pbc_lid_lookup (const VcdObj *obj, const char item_id[]);

enum item_type_t
_vcd_pbc_lookup (const VcdObj *obj, const char item_id[]);

uint32_t
_vcd_pbc_pin_lookup (const VcdObj *obj, const char item_id[]);

unsigned 
_vcd_pbc_list_calc_size (const pbc_t *_pbc, bool extended);

bool
_vcd_pbc_link (VcdObj *obj);

bool
_vcd_pbc_available (const VcdObj *obj);

uint32_t
_vcd_pbc_max_lid (const VcdObj *obj);

#endif /* __VCD_PBC_H__ */
