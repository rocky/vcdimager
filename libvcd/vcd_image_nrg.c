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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libvcd/vcd_image_nrg.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_bytesex.h>

#include <stdio.h>
#include <stdlib.h>
#include <libvcd/vcd_assert.h>
#include <string.h>

/* structures used */

/* this ugly image format is typical for lazy win32 programmers... at
   least structure were put big endian, so at least reverse
   engineering wasn't such an headache... */

typedef struct {
  uint32_t start      GNUC_PACKED;
  uint32_t length     GNUC_PACKED;
  uint32_t type       GNUC_PACKED; /* only 0x3 seen so far... -> MIXED_MODE2 2336 blocksize */
  uint32_t start_lsn  GNUC_PACKED; /* cummulated w/o pre-gap! */
  uint32_t _unknown   GNUC_PACKED; /* wtf is this for? -- always zero... */
} _etnf_array_t;

/* finally they realized that 32bit offsets are a bit outdated... */
typedef struct {
  uint64_t start      GNUC_PACKED;
  uint64_t length     GNUC_PACKED;
  uint32_t type       GNUC_PACKED;
  uint32_t start_lsn  GNUC_PACKED;
  uint64_t _unknown   GNUC_PACKED; /* wtf is this for? -- always zero... */
} _etn2_array_t;

typedef struct {
  uint8_t  _unknown1  GNUC_PACKED; /* 0x41 == 'A' */
  uint8_t  track      GNUC_PACKED; /* binary or BCD?? */
  uint8_t  index      GNUC_PACKED; /* makes 0->1 transitions */
  uint8_t  _unknown2  GNUC_PACKED; /* ?? */
  uint32_t lsn        GNUC_PACKED; 
} _cuex_array_t;

/* reader */

typedef struct {
  VcdDataSource *nrg_src;

  uint32_t size;
  VcdList *mapping;
} _img_nrg_src_t;

typedef struct {
  uint32_t start_lsn;
  uint32_t length_lsn;
  long img_offset;
} _mapping_t;

static void
_source_free (void *user_data)
{
  _img_nrg_src_t *_obj = user_data;

  if (_obj->nrg_src)
    vcd_data_source_destroy (_obj->nrg_src);

  free (_obj);
}

static void
_register_mapping (_img_nrg_src_t *_obj, 
		  uint32_t start_lsn, uint32_t length_lsn,
		  long img_offset)
{
  _mapping_t *_map = _vcd_malloc (sizeof (_mapping_t));

  if (!_obj->mapping)
    _obj->mapping = _vcd_list_new ();

  _vcd_list_append (_obj->mapping, _map);
  _map->start_lsn = start_lsn;
  _map->length_lsn = length_lsn;
  _map->img_offset = img_offset;

  _obj->size = MAX (_obj->size, (start_lsn + length_lsn));

  /* vcd_debug ("map: %d +%d -> %ld", start_lsn, length_lsn, img_offset); */
}

static int
_parse_footer (_img_nrg_src_t *_obj)
{
  long size, footer_start;
  char *footer_buf = NULL;

  if (_obj->size)
    return 0;

  size = vcd_data_source_stat (_obj->nrg_src);

  {
    union {
      struct {
	uint32_t __x          GNUC_PACKED;
	uint32_t ID           GNUC_PACKED;
	uint32_t footer_ofs   GNUC_PACKED;
      } v50;
      struct {
	uint32_t ID           GNUC_PACKED;
	uint64_t footer_ofs   GNUC_PACKED;
      } v55;
    } buf;

    vcd_assert (sizeof (buf) == 12);

    vcd_data_source_seek (_obj->nrg_src, size - sizeof (buf));
    vcd_data_source_read (_obj->nrg_src, (void *) &buf, sizeof (buf), 1);
    
    if (buf.v50.ID == UINT32_TO_BE (0x4e45524f)) /* "NERO" */
      {
	vcd_info ("detected v50 (32bit offsets) NRG magic");
	footer_start = UINT32_TO_BE (buf.v50.footer_ofs); 
      }
    else if (buf.v55.ID == UINT32_TO_BE (0x4e455235)) /* "NER5" */
      {
	vcd_info ("detected v55 (64bit offsets) NRG magic");
	footer_start = uint64_from_be (buf.v55.footer_ofs);
      }
    else
      {
	vcd_error ("Image not recognized as either v50 or v55 type NRG");
	return -1;
      }

    vcd_debug ("nrg footer start = %ld, length = %ld", 
	       (long) footer_start, (long) (size - footer_start));

    vcd_assert (IN ((size - footer_start), 0, 4096));

    footer_buf = _vcd_malloc (size - footer_start);

    vcd_data_source_seek (_obj->nrg_src, footer_start);
    vcd_data_source_read (_obj->nrg_src, footer_buf, size - footer_start, 1);
  }

  {
    int pos = 0;

    while (pos < size - footer_start)
      {
	struct {
	  uint32_t id                    GNUC_PACKED;
	  uint32_t len                   GNUC_PACKED;
	  char data[EMPTY_ARRAY_SIZE]    GNUC_PACKED;
	} *chunk = (void *) (footer_buf + pos);

	bool break_out = false;

	switch (UINT32_FROM_BE (chunk->id))
	  {
	  case 0x43554558: /* "CUEX" */
	    {
	      unsigned entries = UINT32_FROM_BE (chunk->len);
	      _cuex_array_t *_entries = (void *) chunk->data;

	      vcd_assert (_obj->mapping == NULL);

	      vcd_assert (sizeof (_cuex_array_t) == 8);
	      vcd_assert (UINT32_FROM_BE (chunk->len) % sizeof (_cuex_array_t) == 0);

	      entries /= sizeof (_cuex_array_t);

	      vcd_info ("DAO type image detected");

	      {
		uint32_t lsn = UINT32_FROM_BE (_entries[0].lsn);
		int idx;

		vcd_assert (lsn == 0xffffff6a);

		for (idx = 2; idx < entries; idx += 2)
		  {
		    uint32_t lsn2;
		    vcd_assert (_entries[idx].index == 1);
		    vcd_assert (_entries[idx].track != _entries[idx + 1].track);
		    
		    lsn = UINT32_FROM_BE (_entries[idx].lsn);
		    lsn2 = UINT32_FROM_BE (_entries[idx + 1].lsn);

		    _register_mapping (_obj, lsn, lsn2 - lsn, (lsn + 150) * 2336);
		  }
	      }	    
	    }
	    break;

	  case 0x44414f58: /* "DAOX" */
	    vcd_debug ("DAOX tag detected...");
	    break;

	  case 0x4e455235: /* "NER5" */
	  case 0x4e45524f: /* "NER0" */
	    vcd_error ("unexpected nrg magic ID detected");
	    return -1;
	    break;

	  case 0x454e4421: /* "END!" */
	    vcd_debug ("nrg end tag detected");
	    break_out = true;
	    break;

	  case 0x45544e46: /* "ETNF" */ 
	    {
	      unsigned entries = UINT32_FROM_BE (chunk->len);
	      _etnf_array_t *_entries = (void *) chunk->data;

	      vcd_assert (_obj->mapping == NULL);

	      vcd_assert (sizeof (_etnf_array_t) == 20);
	      vcd_assert (UINT32_FROM_BE (chunk->len) % sizeof (_etnf_array_t) == 0);

	      entries /= sizeof (_etnf_array_t);

	      vcd_info ("SAO type image detected");

	      {
		int idx;
		for (idx = 0; idx < entries; idx++)
		  {
		    uint32_t _len = UINT32_FROM_BE (_entries[idx].length);
		    uint32_t _start = UINT32_FROM_BE (_entries[idx].start_lsn);
		    uint32_t _start2 = UINT32_FROM_BE (_entries[idx].start);

		    vcd_assert (UINT32_FROM_BE (_entries[idx].type) == 3);

		    vcd_assert (_len % 2336 == 0);

		    _len /= 2336;

		    vcd_assert (_start * 2336 == _start2);

		    _start += idx * 150;

		    _register_mapping (_obj, _start, _len, _start2);
		  }
	      }
	    }
	    break;

	  case 0x45544e32: /* "ETN2", same as above, but with 64bit stuff instead */
	    {
	      unsigned entries = uint32_from_be (chunk->len);
	      _etn2_array_t *_entries = (void *) chunk->data;

	      vcd_assert (_obj->mapping == NULL);

	      vcd_assert (sizeof (_etn2_array_t) == 32);
	      vcd_assert (uint32_from_be (chunk->len) % sizeof (_etn2_array_t) == 0);

	      entries /= sizeof (_etn2_array_t);

	      vcd_info ("SAO type image detected");

	      {
		int idx;
		for (idx = 0; idx < entries; idx++)
		  {
		    uint32_t _len = uint64_from_be (_entries[idx].length);
		    uint32_t _start = uint32_from_be (_entries[idx].start_lsn);
		    uint32_t _start2 = uint64_from_be (_entries[idx].start);

		    vcd_assert (uint32_from_be (_entries[idx].type) == 3);

		    vcd_assert (_len % 2336 == 0);

		    _len /= 2336;

		    vcd_assert (_start * 2336 == _start2);

		    _start += idx * 150;

		    _register_mapping (_obj, _start, _len, _start2);
		  }
	      }
	    }
	    
	    break;

	  case 0x53494e46: /* "SINF" */
	    {
	      uint32_t *_sessions = (void *) chunk->data;

	      vcd_assert (UINT32_FROM_BE (chunk->len) == 4);

	      vcd_debug ("SINF: %d sessions", UINT32_FROM_BE (*_sessions));
	    }
	    break;

	  default:
	    vcd_warn ("unknown tag %8.8x seen", UINT32_FROM_BE (chunk->id));
	    break;
	  }

	if (break_out)
	  break;

	pos += 8;
	pos += UINT32_FROM_BE (chunk->len);
      }
  }

  return 0;
}

  static int
_read_mode2_sector (void *user_data, void *data, uint32_t lsn, bool form2)
{
  _img_nrg_src_t *_obj = user_data;
  char buf[M2RAW_SIZE] = { 0, };
  VcdListNode *node;

  _parse_footer (_obj);

  if (lsn >= _obj->size)
    {
      vcd_warn ("trying to read beyond image size (%d >= %d)", lsn, _obj->size);
      return -1;
    }

  _VCD_LIST_FOREACH (node, _obj->mapping)
    {
      _mapping_t *_map = _vcd_list_node_data (node);

      if (IN (lsn, _map->start_lsn, (_map->start_lsn + _map->length_lsn - 1)))
	{
	  long img_offset = _map->img_offset;

	  img_offset += (lsn - _map->start_lsn) * 2336;
	  
	  vcd_data_source_seek (_obj->nrg_src, img_offset); 
	  vcd_data_source_read (_obj->nrg_src, buf, 2336, 1); 
	  
	  break;
	}
    }

  if (!node)
    vcd_warn ("reading into pre gap (lsn %d)", lsn);

  if (form2)
    memcpy (data, buf, M2RAW_SIZE);
  else
    memcpy (data, buf + 8, ISO_BLOCKSIZE);

  return 0;
}

static uint32_t 
_stat_size (void *user_data)
{
  _img_nrg_src_t *_obj = user_data;

  _parse_footer (_obj);
  return _obj->size;
}

VcdImageSource *
vcd_image_source_new_nrg (VcdDataSource *nrg_source)
{
  _img_nrg_src_t *_data;

  vcd_image_source_funcs _funcs = {
    read_mode2_sector: _read_mode2_sector,
    stat_size: _stat_size,
    free: _source_free
  };

  if (!nrg_source)
    return NULL;

  _data = _vcd_malloc (sizeof (_img_nrg_src_t));
  _data->nrg_src = nrg_source;

  return vcd_image_source_new (_data, &_funcs);
}

/****************************************************************************
 * writer
 */

typedef struct {
  VcdDataSink *nrg_snk;
} _img_nrg_snk_t;

static void
_sink_free (void *user_data)
{
  _img_nrg_snk_t *_obj = user_data;

  vcd_data_sink_destroy (_obj->nrg_snk);
  free (_obj);
}

static int
_set_cuesheet (void *user_data, const VcdList *vcd_cue_list)
{
  _img_nrg_snk_t *_obj = user_data;

  vcd_assert_not_reached ();

  return 0;
}
 
static int
_write (void *user_data, const void *data, uint32_t lsn)
{
  _img_nrg_snk_t *_obj = user_data;

  vcd_assert_not_reached ();

  return 0;
}

VcdImageSink *
vcd_image_sink_new_nrg (VcdDataSink *nrg_sink)
{
  _img_nrg_snk_t *_data;

  vcd_image_sink_funcs _funcs = {
    set_cuesheet: _set_cuesheet,
    write: _write,
    free: _sink_free
  };

  if (!nrg_sink)
    return NULL;

  _data = _vcd_malloc (sizeof (_img_nrg_snk_t));
  _data->nrg_snk = nrg_sink;

  return vcd_image_sink_new (_data, &_funcs);
}

