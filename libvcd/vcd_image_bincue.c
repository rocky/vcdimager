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

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_bytesex.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_image_bincue.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_stream_stdio.h>

#include <stdlib.h>
#include <string.h>

static const char _rcsid[] = "$Id$";

/* reader */

typedef struct {
  bool sector_2336_flag;

  VcdDataSource *bin_src;
  char *bin_name;

  bool init;
} _img_bincue_src_t;

static void
_source_init (_img_bincue_src_t *_obj)
{
  if (_obj->init)
    return;

  if (!(_obj->bin_src = vcd_data_source_new_stdio (_obj->bin_name)))
    vcd_error ("init failed");

  _obj->init = true;  
}

static void
_source_free (void *user_data)
{
  _img_bincue_src_t *_obj = user_data;

  free (_obj->bin_name);

  if (_obj->bin_src)
    vcd_data_source_destroy (_obj->bin_src);

  free (_obj);
}

static int
_read_mode2_sector (void *user_data, void *data, uint32_t lsn, bool form2)
{
  _img_bincue_src_t *_obj = user_data;
  char buf[CDDA_SECTOR_SIZE] = { 0, };
  int blocksize = _obj->sector_2336_flag ? 2336 : CDDA_SECTOR_SIZE;

  _source_init (_obj);

  vcd_data_source_seek (_obj->bin_src, lsn * blocksize);

  vcd_data_source_read (_obj->bin_src,
			_obj->sector_2336_flag ? (buf + 12 + 4) : buf,
			blocksize, 1);

  if (form2)
    memcpy (data, buf + 12 + 4, M2RAW_SECTOR_SIZE);
  else
    memcpy (data, buf + 12 + 4 + 8, ISO_BLOCKSIZE);

  return 0;
}

static uint32_t 
_stat_size (void *user_data)
{
  _img_bincue_src_t *_obj = user_data;
  long size;
  int blocksize = _obj->sector_2336_flag ? 2336 : CDDA_SECTOR_SIZE;

  _source_init (_obj);
  
  size = vcd_data_source_stat (_obj->bin_src);

  if (size % blocksize)
    {
      vcd_warn ("image file not multiple of blocksize (%d)", 
                blocksize);
      if (size % 2336 == 0)
	vcd_warn ("this may be a 2336-type disc image");
      else if (size % CDDA_SECTOR_SIZE == 0)
	vcd_warn ("this may be a 2352-type disc image");
      /* exit (EXIT_FAILURE); */
    }

  size /= blocksize;

  return size;
}

static int
_source_set_arg (void *user_data, const char key[], const char value[])
{
  _img_bincue_src_t *_obj = user_data;

  if (!strcmp (key, "bin"))
    {
      free (_obj->bin_name);

      if (!value)
	return -2;

      _obj->bin_name = strdup (value);
    }
  else if (!strcmp (key, "sector"))
    {
      if (!strcmp (value, "2336"))
	_obj->sector_2336_flag = true;
      else if (!strcmp (value, "2352"))
	_obj->sector_2336_flag = false;
      else
	return -2;
    }
  else
    return -1;

  return 0;
}

VcdImageSource *
vcd_image_source_new_bincue (void)
{
  _img_bincue_src_t *_data;

  vcd_image_source_funcs _funcs = {
    read_mode2_sector: _read_mode2_sector,
    stat_size: _stat_size,
    free: _source_free,
    set_arg: _source_set_arg  
  };

  _data = _vcd_malloc (sizeof (_img_bincue_src_t));
  _data->bin_name = strdup ("videocd.bin");

  return vcd_image_source_new (_data, &_funcs);
}

/****************************************************************************
 * writer
 */

typedef struct {
  bool sector_2336_flag;
  VcdDataSink *bin_snk;
  VcdDataSink *cue_snk;
  char *bin_fname;
  char *cue_fname;

  bool init;
} _img_bincue_snk_t;

static void
_sink_init (_img_bincue_snk_t *_obj)
{
  if (_obj->init)
    return;

  if (!(_obj->bin_snk = vcd_data_sink_new_stdio (_obj->bin_fname)))
    vcd_error ("init failed");

  if (!(_obj->cue_snk = vcd_data_sink_new_stdio (_obj->cue_fname)))
    vcd_error ("init failed");

  _obj->init = true;  
}

static void
_sink_free (void *user_data)
{
  _img_bincue_snk_t *_obj = user_data;

  vcd_data_sink_destroy (_obj->bin_snk);
  vcd_data_sink_destroy (_obj->cue_snk);
  free (_obj->bin_fname);
  free (_obj->cue_fname);
  free (_obj);
}

static int
_set_cuesheet (void *user_data, const VcdList *vcd_cue_list)
{
  _img_bincue_snk_t *_obj = user_data;
  VcdListNode *node;
  int track_no, index_no;
  const vcd_cue_t *_last_cue = 0;
  
  _sink_init (_obj);

  vcd_data_sink_printf (_obj->cue_snk, "FILE \"%s\" BINARY\r\n",
			_obj->bin_fname);

  track_no = 0;
  index_no = 0;
  _VCD_LIST_FOREACH (node, (VcdList *) vcd_cue_list)
    {
      const vcd_cue_t *_cue = _vcd_list_node_data (node);
      
      msf_t _msf = { 0, 0, 0 };
      
      switch (_cue->type)
	{
	case VCD_CUE_TRACK_START:
	  track_no++;
	  index_no = 0;

	  vcd_data_sink_printf (_obj->cue_snk, 
				"  TRACK %2.2d MODE2/%d\r\n"
				"    FLAGS DCP\r\n",
				track_no, (_obj->sector_2336_flag ? 2336 : 2352));

	  if (_last_cue && _last_cue->type == VCD_CUE_PREGAP_START)
	    {
	      lba_to_msf (_last_cue->lsn, &_msf);

	      vcd_data_sink_printf (_obj->cue_snk, 
				    "    INDEX %2.2d %2.2x:%2.2x:%2.2x\r\n",
				    index_no, _msf.m, _msf.s, _msf.f);
	    }

	  index_no++;

	  lba_to_msf (_cue->lsn, &_msf);

	  vcd_data_sink_printf (_obj->cue_snk, 
				"    INDEX %2.2d %2.2x:%2.2x:%2.2x\r\n",
				index_no, _msf.m, _msf.s, _msf.f);
	  break;

	case VCD_CUE_PREGAP_START:
	  /* handled in next iteration */
	  break;

	case VCD_CUE_SUBINDEX:
	  vcd_assert (_last_cue != 0);

	  index_no++;
	  vcd_assert (index_no < 100);

	  lba_to_msf (_cue->lsn, &_msf);

	  vcd_data_sink_printf (_obj->cue_snk, 
				"    INDEX %2.2d %2.2x:%2.2x:%2.2x\r\n",
				index_no, _msf.m, _msf.s, _msf.f);
	  break;
	  
	case VCD_CUE_END:
	  vcd_data_sink_close (_obj->cue_snk);
	  return 0;
	  break;

	case VCD_CUE_LEADIN:
	  break;
	}

      _last_cue = _cue;
    }

  vcd_assert_not_reached ();

  return -1;
}
 
static int
_write (void *user_data, const void *data, uint32_t lsn)
{
  const char *buf = data;
  _img_bincue_snk_t *_obj = user_data;
  long offset = lsn;

  _sink_init (_obj);

  offset *= _obj->sector_2336_flag ? M2RAW_SECTOR_SIZE : CDDA_SECTOR_SIZE;

  vcd_data_sink_seek(_obj->bin_snk, offset);
  
  if (_obj->sector_2336_flag)
    vcd_data_sink_write(_obj->bin_snk, buf + 12 + 4, M2RAW_SECTOR_SIZE, 1);
  else
    vcd_data_sink_write(_obj->bin_snk, buf, CDDA_SECTOR_SIZE, 1);

  return 0;
}

static int
_sink_set_arg (void *user_data, const char key[], const char value[])
{
  _img_bincue_snk_t *_obj = user_data;

  if (!strcmp (key, "bin"))
    {
      free (_obj->bin_fname);

      if (!value)
	return -2;

      _obj->bin_fname = strdup (value);
    }
  else if (!strcmp (key, "cue"))
    {
      free (_obj->cue_fname);

      if (!value)
	return -2;

      _obj->cue_fname = strdup (value);
    }
  else if (!strcmp (key, "sector"))
    {
      if (!strcmp (value, "2336"))
	_obj->sector_2336_flag = true;
      else if (!strcmp (value, "2352"))
	_obj->sector_2336_flag = false;
      else
	return -2;
    }
  else
    return -1;

  return 0;
}

VcdImageSink *
vcd_image_sink_new_bincue (void)
{
  _img_bincue_snk_t *_data;

  vcd_image_sink_funcs _funcs = {
    set_cuesheet: _set_cuesheet,
    write: _write,
    free: _sink_free,
    set_arg: _sink_set_arg
  };

  _data = _vcd_malloc (sizeof (_img_bincue_snk_t));

  _data->bin_fname = strdup ("videocd.bin");
  _data->cue_fname = strdup ("videocd.cue");

  return vcd_image_sink_new (_data, &_funcs);
}

