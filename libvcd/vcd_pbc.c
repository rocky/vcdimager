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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <assert.h>

#include <libvcd/vcd_pbc.h>

uint32_t 
_vcd_pbc_lookup (const VcdObj *obj, const char item_id[])
{
  int n;
  VcdListNode *node;

  /* check sequence items */

  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      mpeg_sequence_t *_sequence = _vcd_list_node_data (node);

      assert (n < 98);

      if (_sequence->id && !strcmp (item_id, _sequence->id))
	return n + 2;

      n++;
    }

  /* check entry points */

  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      mpeg_sequence_t *_sequence = _vcd_list_node_data (node);
      VcdListNode *node2;

      /* skip over virtual 0-entry */
      n++;

      _VCD_LIST_FOREACH (node2, _sequence->entry_list)
	{
	  entry_t *_entry = _vcd_list_node_data (node);

	  assert (n < 500);

	  if (_entry->id && !strcmp (item_id, _entry->id))
	    return n + 100;

	  n++;
	}
    }

  /* check sequence items */

  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_segment_list)
    {
      mpeg_segment_t *_segment = _vcd_list_node_data (node);

      assert (n < 1980);

      if (_segment->id && !strcmp (item_id, _segment->id))
	return n + 1000;

      n += _segment->segment_count;
    }

  /* check lid's */

  return 0;
}

