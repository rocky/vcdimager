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

#include <libvcd/vcd_image_bincue.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_bytesex.h>

#include <stdio.h>
#include <stdlib.h>
#include <libvcd/vcd_assert.h>
#include <string.h>

/* reader */

typedef struct {
  bool sector_2336_flag;
  VcdDataSource *bin_src;
  VcdDataSource *cue_src;
} _img_bincue_src_t;

static void
_source_free (void *user_data)
{
  _img_bincue_src_t *_obj = user_data;

  if (_obj->bin_src)
    vcd_data_source_destroy (_obj->bin_src);

  if (_obj->cue_src)
    vcd_data_source_destroy (_obj->cue_src);
  free (_obj);
}

static int
_read_mode2_sector (void *user_data, void *data, uint32_t lsn, bool form2)
{
  _img_bincue_src_t *_obj = user_data;
  char buf[CDDA_SIZE] = { 0, };
  int blocksize = _obj->sector_2336_flag ? 2336 : CDDA_SIZE;

  vcd_data_source_seek (_obj->bin_src, lsn * blocksize);

  vcd_data_source_read (_obj->bin_src,
			_obj->sector_2336_flag ? (buf + 12 + 4) : buf,
			blocksize, 1);

  if (form2)
    memcpy (data, buf + 12 + 4, M2RAW_SIZE);
  else
    memcpy (data, buf + 12 + 4 + 8, ISO_BLOCKSIZE);

  return 0;
}

static uint32_t 
_stat_size (void *user_data)
{
  _img_bincue_src_t *_obj = user_data;
  long size;
  int blocksize = _obj->sector_2336_flag ? 2336 : CDDA_SIZE;
  
  size = vcd_data_source_stat (_obj->bin_src);

  if (size % blocksize)
    {
      vcd_warn ("image file not multiple of blocksize (%d)", 
                blocksize);
      if (size % 2336 == 0)
	vcd_warn ("this may be a 2336-type disc image");
      else if (size % CDDA_SIZE == 0)
	vcd_warn ("this may be a 2352-type disc image");
      /* exit (EXIT_FAILURE); */
    }

  size /= blocksize;

  return size;
}

VcdImageSource *
vcd_image_source_new_bincue (VcdDataSource *bin_source,
			     VcdDataSource *cue_source, bool sector_2336)
{
  _img_bincue_src_t *_data;

  vcd_image_source_funcs _funcs = {
    read_mode2_sector: _read_mode2_sector,
    stat_size: _stat_size,
    free: _source_free
  };

  if (!bin_source)
    return NULL;

  _data = _vcd_malloc (sizeof (_img_bincue_src_t));
  _data->bin_src = bin_source;
  _data->cue_src = cue_source;
  _data->sector_2336_flag = sector_2336;

  return vcd_image_source_new (_data, &_funcs);
}

/****************************************************************************
 * writer
 */

typedef struct {
  bool sector_2336_flag;
  VcdDataSink *bin_snk;
  VcdDataSink *cue_snk;
  char *cue_fname;
} _img_bincue_snk_t;

static void
_sink_free (void *user_data)
{
  _img_bincue_snk_t *_obj = user_data;

  vcd_data_sink_destroy (_obj->bin_snk);
  vcd_data_sink_destroy (_obj->cue_snk);
  free (_obj->cue_fname);
  free (_obj);
}

static int
_set_cuesheet (void *user_data, const VcdList *vcd_cue_list)
{
  _img_bincue_snk_t *_obj = user_data;
  char buf[1024] = { 0, };
  VcdListNode *node;
  int num;

  snprintf (buf, sizeof (buf), "FILE \"%s\" BINARY\r\n", _obj->cue_fname);

  vcd_data_sink_write (_obj->cue_snk, buf, 1, strlen (buf));

  num = 1;
  _VCD_LIST_FOREACH (node, (VcdList *) vcd_cue_list)
    {
      const vcd_cue_t *_cue = _vcd_list_node_data (node);
      
      msf_t _msf = { 0, 0, 0 };
      
      if (_cue->type != VCD_CUE_TRACK_START)
	continue;

      lba_to_msf (_cue->lsn, &_msf);

      snprintf (buf, sizeof (buf),
		"  TRACK %2.2d MODE2/%d\r\n"
		"    INDEX %2.2d %2.2x:%2.2x:%2.2x\r\n",
		num, (_obj->sector_2336_flag ? 2336 : 2352), 
		1, _msf.m, _msf.s, _msf.f);

      num++;

      vcd_data_sink_write (_obj->cue_snk, buf, 1, strlen (buf));
    }

  vcd_data_sink_close (_obj->cue_snk);

  return 0;
}
 
static int
_write (void *user_data, const void *data, uint32_t lsn)
{
  const char *buf = data;
  _img_bincue_snk_t *_obj = user_data;
  long offset = lsn;

  offset *= _obj->sector_2336_flag ? M2RAW_SIZE : CDDA_SIZE;

  vcd_data_sink_seek(_obj->bin_snk, offset);
  
  if (_obj->sector_2336_flag)
    vcd_data_sink_write(_obj->bin_snk, buf + 12 + 4, M2RAW_SIZE, 1);
  else
    vcd_data_sink_write(_obj->bin_snk, buf, CDDA_SIZE, 1);

  return 0;
}

VcdImageSink *
vcd_image_sink_new_bincue (VcdDataSink *bin_sink, VcdDataSink *cue_sink,
			   const char cue_fname[], bool sector_2336)
{
  _img_bincue_snk_t *_data;

  vcd_image_sink_funcs _funcs = {
    set_cuesheet: _set_cuesheet,
    write: _write,
    free: _sink_free
  };

  if (!bin_sink || !cue_fname)
    return NULL;

  _data = _vcd_malloc (sizeof (_img_bincue_snk_t));
  _data->bin_snk = bin_sink;
  _data->cue_snk = cue_sink;
  _data->cue_fname = strdup (cue_fname);
  _data->sector_2336_flag = sector_2336;

  return vcd_image_sink_new (_data, &_funcs);
}

