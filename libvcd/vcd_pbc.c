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
#include <stddef.h>

#include <libvcd/vcd_util.h>
#include <libvcd/vcd_pbc.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_obj.h>
#include <libvcd/vcd_files_private.h>
#include <libvcd/vcd_bytesex.h>

unsigned
_vcd_pbc_lid_lookup (const VcdObj *obj, const char item_id[])
{
  VcdListNode *node;
  unsigned n = 1;

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);

      assert (n < 0x8000);

      if (_pbc->id && !strcmp (item_id, _pbc->id))
	return n;

      n++;
    }

  /* not found */
  return 0;
}

enum item_type_t
_vcd_pbc_lookup (const VcdObj *obj, const char item_id[])
{
  unsigned id;

  if ((id = _vcd_pbc_pin_lookup (obj, item_id)))
    {
      if (id < 100)
	return ITEM_TYPE_TRACK;
      else if (id < 600)
	return ITEM_TYPE_ENTRY;
      else if (id < 2980)
	return ITEM_TYPE_SEGMENT;
      else 
	assert (1);
    }
  else if (_vcd_pbc_lid_lookup (obj, item_id))
    return ITEM_TYPE_PBC;

  return ITEM_TYPE_NOTFOUND;
}

uint32_t
_vcd_pbc_pin_lookup (const VcdObj *obj, const char item_id[])
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

  return 0;
}

bool
_vcd_pbc_available (const VcdObj *obj)
{
  assert (obj != NULL);
  assert (obj->pbc_list != NULL);

  if (!_vcd_list_length (obj->pbc_list))
    return false;

  switch (obj->type) 
    {
    case VCD_TYPE_VCD2:
    case VCD_TYPE_SVCD:
      return true;
      break;

    case VCD_TYPE_VCD11:
      vcd_warn ("PBC list not empty but VCD type not capable of PBC!");
      return false;
      break;

    default:
      assert (1);
      break;
    }

  return false;
}

uint32_t
_vcd_pbc_max_lid (const VcdObj *obj)
{
  if (!_vcd_pbc_available (obj))
    return 0;

  return (_vcd_list_length (obj->pbc_list));
}

static size_t
_vcd_pbc_node_length (const pbc_t *_pbc, bool extended)
{
  size_t retval = 0;

  switch (_pbc->type)
    {
      int n;

    case PBC_PLAYLIST:
      n = _vcd_list_length (_pbc->item_id_list);
      retval = offsetof (PsdPlayListDescriptor, itemid[n]);
      break;

    case PBC_SELECTION:
      n = _vcd_list_length (_pbc->default_id_list);
      if (n > 1)
	{ /* multi defaults */
	  if (_vcd_list_length (_pbc->select_id_list))
	    { /* if numeric selects are available they must be as many as 
		 multi default targets exists */
	      assert (_vcd_list_length (_pbc->select_id_list) == n);
	      n *= 2;
	    }
	}
      else /* only numeric targets */
	n = _vcd_list_length (_pbc->select_id_list);

      retval = offsetof (PsdSelectionListDescriptor, ofs[n]);

      n = _vcd_list_length (_pbc->select_id_list);

      if (extended)
	retval += offsetof (PsdSelectionListDescriptorExtended, area[n]);

      break;
      
    case PBC_END:
      retval = sizeof (PsdEndOfListDescriptor);
      break;

    default:
      assert (1);
      break;
    }

  return retval;
}

void
_vcd_pbc_node_write (const pbc_t *_pbc, void *buf, bool extended)
{
  switch (_pbc->type)
    {
    case PBC_PLAYLIST:
      {
	PsdPlayListDescriptor *_md = buf;

	_md->type = PSD_TYPE_PLAY_LIST;
	_md->noi = 0;
	_md->lid = UINT16_TO_BE (_pbc->lid);
	_md->prev_ofs = UINT16_TO_BE (0xffff);
	_md->next_ofs = UINT16_TO_BE (0xffff);
	_md->return_ofs = UINT16_TO_BE (0xffff);
	_md->ptime = UINT16_TO_BE (0x0000);
	_md->wtime = 0x00;
	_md->atime = 0x00;
	/* _md->itemid[0] = UINT16_TO_BE (0); */
      }
      break;

    case PBC_SELECTION:
      {
	PsdSelectionListDescriptor *_md = buf;

	_md->type = PSD_TYPE_SELECTION_LIST;

	_md->lid = UINT16_TO_BE (_pbc->lid);
	_md->prev_ofs = UINT16_TO_BE (0xffff);
	_md->next_ofs = UINT16_TO_BE (0xffff);
	_md->return_ofs = UINT16_TO_BE (0xffff);
      }
      break;
      
    case PBC_END:
      {
	PsdEndOfListDescriptor *_md = buf;

	_md->type = PSD_TYPE_END_OF_LIST;
      }
      break;

    default:
      assert (1);
      break;
    }
}

pbc_t *
vcd_pbc_new (enum pbc_type_t type)
{
  pbc_t *_pbc;

  _pbc = _vcd_malloc (sizeof (pbc_t));
  _pbc->type = type;

  switch (type)
    {
    case PBC_PLAYLIST:
      _pbc->item_id_list = _vcd_list_new ();
      break;

    case PBC_SELECTION:
      _pbc->default_id_list = _vcd_list_new ();
      _pbc->select_id_list = _vcd_list_new ();
      break;
      
    case PBC_END:
      break;

    default:
      assert (1);
      break;
    }

  return _pbc;
}

/* 
 */

bool
_vcd_pbc_finalize (VcdObj *obj)
{
  VcdListNode *node;
  unsigned offset = 0, offset_ext = 0;
  unsigned lid;

  lid = 1;
  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);
      unsigned length, length_ext;

      length = _vcd_pbc_node_length (_pbc, false);
      length_ext = _vcd_pbc_node_length (_pbc, true);

      /* node may not cross sector boundary! */
      if (ISO_BLOCKSIZE - (offset % ISO_BLOCKSIZE) < length)
	offset = ISO_BLOCKSIZE * _vcd_len2blocks (offset, ISO_BLOCKSIZE);

      if (ISO_BLOCKSIZE - (offset_ext % ISO_BLOCKSIZE) < length_ext)
	offset_ext = ISO_BLOCKSIZE * _vcd_len2blocks (offset_ext, ISO_BLOCKSIZE);

      _pbc->offset = offset;
      _pbc->offset_ext = offset_ext;
      _pbc->lid = lid;
      vcd_debug ("pbc %d+%d (%d+%d)", offset, length, offset_ext, length_ext);

      offset += _vcd_len2blocks (length, INFO_OFFSET_MULT) * INFO_OFFSET_MULT;
      offset_ext += _vcd_len2blocks (length_ext, INFO_OFFSET_MULT) * INFO_OFFSET_MULT;
      lid++;
    }

  obj->psd_size = offset;
  obj->psdx_size = offset_ext;

  vcd_debug ("pbc size %d (%d)", offset, offset_ext);

  vcd_warn ("%s not implemented yet", __PRETTY_FUNCTION__);

  

  return true;
}
