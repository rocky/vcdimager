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
#include <libvcd/vcd_assert.h>
#include <stddef.h>
#include <math.h>

#include <libvcd/vcd_util.h>
#include <libvcd/vcd_pbc.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_obj.h>
#include <libvcd/vcd_files_private.h>
#include <libvcd/vcd_bytesex.h>

static uint8_t
_wtime (int seconds)
{
  if (seconds < 0)
    return 255;

  if (seconds <= 60)
    return seconds;

  if (seconds <= 2000)
    {
      double _tmp;

      _tmp = seconds;
      _tmp -= 60;
      _tmp /= 10;
      _tmp += 60;

      return rint (_tmp);
    }

  vcd_warn ("wait time of %ds clipped to 2000s", seconds);

  return 254;
}

enum _sel_mode_t {
  _SEL_MODE_NORMAL = 0,
  _SEL_MODE_MULTI,
  _SEL_MODE_MULTI_ONLY
};

static enum _sel_mode_t
_get_sel_mode (const pbc_t *_pbc)
{
  enum _sel_mode_t _mode = _SEL_MODE_NORMAL;

  if (_vcd_list_length (_pbc->default_id_list) > 1)
    _mode = _SEL_MODE_MULTI_ONLY;
  
  if (_mode == _SEL_MODE_MULTI_ONLY 
      && _vcd_list_length (_pbc->select_id_list))
    {
      vcd_assert (_vcd_list_length (_pbc->default_id_list)
	      == _vcd_list_length (_pbc->select_id_list));

      _mode = _SEL_MODE_MULTI;
    }

  return _mode;
}

unsigned
_vcd_pbc_lid_lookup (const VcdObj *obj, const char item_id[])
{
  VcdListNode *node;
  unsigned n = 1;

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);

      vcd_assert (n < 0x8000);

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

  vcd_assert (item_id != NULL);

  if ((id = _vcd_pbc_pin_lookup (obj, item_id)))
    {
      if (id < 2)
	return ITEM_TYPE_NOTFOUND;
      else if (id < 100)
	return ITEM_TYPE_TRACK;
      else if (id < 600)
	return ITEM_TYPE_ENTRY;
      else if (id < 2980)
	return ITEM_TYPE_SEGMENT;
      else 
	vcd_assert_not_reached ();
    }
  else if (_vcd_pbc_lid_lookup (obj, item_id))
    return ITEM_TYPE_PBC;

  return ITEM_TYPE_NOTFOUND;
}

uint16_t
_vcd_pbc_pin_lookup (const VcdObj *obj, const char item_id[])
{
  int n;
  VcdListNode *node;

  if (!item_id)
    return 0;

  /* check sequence items */

  n = 0;
  _VCD_LIST_FOREACH (node, obj->mpeg_sequence_list)
    {
      mpeg_sequence_t *_sequence = _vcd_list_node_data (node);

      vcd_assert (n < 98);

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
	  entry_t *_entry = _vcd_list_node_data (node2);

	  vcd_assert (n < 500);

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

      vcd_assert (n < 1980);

      if (_segment->id && !strcmp (item_id, _segment->id))
	return n + 1000;

      n += _segment->segment_count;
    }

  return 0;
}

bool
_vcd_pbc_available (const VcdObj *obj)
{
  vcd_assert (obj != NULL);
  vcd_assert (obj->pbc_list != NULL);

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
      vcd_assert_not_reached ();
      break;
    }

  return false;
}

uint16_t
_vcd_pbc_max_lid (const VcdObj *obj)
{
  uint16_t retval = 0;
  
  if (_vcd_pbc_available (obj))
    retval = _vcd_list_length (obj->pbc_list);

  return retval;
}

static size_t
_vcd_pbc_node_length (const VcdObj *obj, const pbc_t *_pbc, bool extended)
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
      n = 0;

      {
	enum _sel_mode_t _mode;

	_mode = _get_sel_mode (_pbc);
	
	switch (_mode)
	  {
	  case _SEL_MODE_MULTI:
	    n = _vcd_list_length (_pbc->select_id_list) + _vcd_list_length (_pbc->default_id_list);
	    break;

	  case _SEL_MODE_MULTI_ONLY:
	    n = _vcd_list_length (_pbc->default_id_list);
	    vcd_assert (_vcd_list_length (_pbc->select_id_list) == 0);
	    break;

	  case _SEL_MODE_NORMAL:
	    n = _vcd_list_length (_pbc->select_id_list);
	    vcd_assert (_vcd_list_length (_pbc->default_id_list) <= 1);
	    break;

	  default:
	    vcd_assert_not_reached ();
	    break;
	  }
      }

      retval = offsetof (PsdSelectionListDescriptor, ofs[n]);

      n = _vcd_list_length (_pbc->select_id_list);

      if (extended || obj->type == VCD_TYPE_SVCD)
	retval += offsetof (PsdSelectionListDescriptorExtended, area[n]);

      break;
      
    case PBC_END:
      retval = sizeof (PsdEndOfListDescriptor);
      break;

    default:
      vcd_assert_not_reached ();
      break;
    }

  return retval;
}

static uint16_t 
_lookup_psd_offset (const VcdObj *obj, const char item_id[], bool extended)
{
  VcdListNode *node;

  /* disable it */
  if (!item_id)
    return 0xffff;

  _VCD_LIST_FOREACH (node, obj->pbc_list)
    {
      pbc_t *_pbc = _vcd_list_node_data (node);

      if (!_pbc->id || strcmp (item_id, _pbc->id))
	continue;
	
      return (extended ? _pbc->offset_ext : _pbc->offset) / INFO_OFFSET_MULT;
    }

  vcd_warn ("PSD: referenced PSD '%s' not found", item_id);
	    
  /* not found */
  return 0xffff;
}


void
_vcd_pbc_node_write (const VcdObj *obj, const pbc_t *_pbc, void *buf,
		     bool extended)
{
  if (extended)
    vcd_assert (obj->type == VCD_TYPE_VCD2);

  switch (_pbc->type)
    {
    case PBC_PLAYLIST:
      {
	PsdPlayListDescriptor *_md = buf;
	VcdListNode *node;
	int n;
	
	_md->type = PSD_TYPE_PLAY_LIST;
	_md->noi = _vcd_list_length (_pbc->item_id_list);
	
	vcd_assert (_pbc->lid < 0x8000);
	_md->lid = UINT16_TO_BE (_pbc->lid | (_pbc->rejected ? 0x8000 : 0));
	
	_md->prev_ofs = 
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->prev_id, extended));
	_md->next_ofs = 
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->next_id, extended));
	_md->return_ofs =
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->retn_id, extended));
	_md->ptime = uint16_to_be (rint (_pbc->playing_time * 15.0));
	_md->wtime = _wtime (_pbc->wait_time);
	_md->atime = _wtime (_pbc->auto_pause_time);
	
	n = 0;
	_VCD_LIST_FOREACH (node, _pbc->item_id_list)
	  {
	    char *_id = _vcd_list_node_data (node);
	    uint16_t _pin;

	    vcd_assert (_id != NULL);

	    _pin = _vcd_pbc_pin_lookup (obj, _id);

	    if (!_pin)
	      vcd_warn ("PSD: referenced PIN '%s' not found", _id);

	    _md->itemid[n] = UINT16_TO_BE (_pin);
	    n++;
	  }
      }
      break;

    case PBC_SELECTION:
      {
	PsdSelectionListDescriptor *_md = buf;

	enum _sel_mode_t _mode;

	_mode = _get_sel_mode (_pbc);

	if (obj->type != VCD_TYPE_SVCD)
	  vcd_assert (_mode == _SEL_MODE_NORMAL);

	if (extended)
	  _md->type = PSD_TYPE_EXT_SELECTION_LIST;
	else
	  _md->type = PSD_TYPE_SELECTION_LIST;
	
	if (obj->type == VCD_TYPE_SVCD)
	  _md->flags.SelectionAreaFlag = true;

	_md->bsn = _pbc->bsn;
	_md->nos = _vcd_list_length (_pbc->select_id_list);
	_md->default_ofs = UINT16_TO_BE (0xffff);

	vcd_assert (sizeof (PsdSelectionListFlags) == 1);
	_md->flags.SelectionAreaFlag = false;
	_md->flags.CommandListFlag = false;

	vcd_assert (_pbc->lid < 0x8000);
	_md->lid = UINT16_TO_BE (_pbc->lid | (_pbc->rejected ? 0x8000 : 0));

	_md->prev_ofs = 
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->prev_id, extended));
	_md->next_ofs = 
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->next_id, extended));
	_md->return_ofs =
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->retn_id, extended));
	_md->timeout_ofs =
	  uint16_to_be (_lookup_psd_offset (obj, _pbc->timeout_id, extended));
	_md->totime = _wtime (_pbc->timeout_time);

	if (_pbc->loop_count > 0x7f)
	  vcd_warn ("loop count %d > 127", _pbc->loop_count);

	_md->loop = (_pbc->loop_count > 0x7f) ? 0x7f : _pbc->loop_count;
	
	if (_pbc->jump_delayed)
	  _md->loop |= 0x80;

	{
	  uint16_t _pin;

	  _pin = _vcd_pbc_pin_lookup (obj, _pbc->item_id);

	  if (!_pin)
	    vcd_warn ("PSD: referenced PIN '%s' not found", _pbc->item_id);

	  _md->itemid = UINT16_TO_BE (_pin);
	}

	switch (_mode)
	  {
	    VcdListNode *node;
	    int idx;
	    
	  case _SEL_MODE_MULTI:
	    vcd_assert (_md->bsn == 1);
	    _md->default_ofs = UINT16_TO_BE (0xfffd);

	    idx = 0;
	    _VCD_LIST_FOREACH (node, _pbc->select_id_list)
	      {
		char *_id = _vcd_list_node_data (node);
		
		_md->ofs[idx] = uint16_to_be (_lookup_psd_offset (obj, _id, extended));

		idx += 2;
	      }

	    idx = 1;
	    _VCD_LIST_FOREACH (node, _pbc->default_id_list)
	      {
		char *_id = _vcd_list_node_data (node);
		
		_md->ofs[idx] = uint16_to_be (_lookup_psd_offset (obj, _id, extended));

		idx += 2;
	      }
	    break;

	  case _SEL_MODE_MULTI_ONLY:
	    vcd_assert (_md->bsn == 1);
	    _md->default_ofs = UINT16_TO_BE (0xfffe);
	    _md->nos = _vcd_list_length (_pbc->default_id_list);
	    /* fixme -- assert entry nums */

	    idx = 0;
	    _VCD_LIST_FOREACH (node, _pbc->default_id_list)
	      {
		char *_id = _vcd_list_node_data (node);
		
		_md->ofs[idx] = uint16_to_be (_lookup_psd_offset (obj, _id, extended));

		idx++;
	      }

	    break;

	  case _SEL_MODE_NORMAL:
	    if (_vcd_list_length (_pbc->default_id_list))
	      {
		char *_default_id = _vcd_list_node_data (_vcd_list_begin (_pbc->default_id_list));
		_md->default_ofs = uint16_to_be (_lookup_psd_offset (obj, _default_id, extended));
	      }

	    idx = 0;
	    _VCD_LIST_FOREACH (node, _pbc->select_id_list)
	      {
		char *_id = _vcd_list_node_data (node);
		
		_md->ofs[idx] = 
		  uint16_to_be (_lookup_psd_offset (obj, _id, extended));

		idx++;
	      }
	    break;

	  default:
	    vcd_assert_not_reached ();
	  }
      }
      break;
      
    case PBC_END:
      {
	PsdEndOfListDescriptor *_md = buf;

	_md->type = PSD_TYPE_END_LIST;
      }
      break;

    default:
      vcd_assert_not_reached ();
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
      vcd_assert_not_reached ();
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

      length = _vcd_pbc_node_length (obj, _pbc, false);
      length_ext = _vcd_pbc_node_length (obj, _pbc, true);

      /* round them up to... */
      length = _vcd_ceil2block (length, INFO_OFFSET_MULT);
      length_ext = _vcd_ceil2block (length_ext, INFO_OFFSET_MULT);

      /* node may not cross sector boundary! */
      offset = _vcd_ofs_add (offset, length, ISO_BLOCKSIZE);
      offset_ext = _vcd_ofs_add (offset_ext, length_ext, ISO_BLOCKSIZE);

      /* save start offsets */
      _pbc->offset = offset - length;
      _pbc->offset_ext = offset_ext - length_ext;

      _pbc->lid = lid;

      lid++;
    }

  obj->psd_size = offset;
  obj->psdx_size = offset_ext;

  vcd_debug ("pbc: psd size %d (extended psd %d)", offset, offset_ext);

  return true;
}
