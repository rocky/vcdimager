/*
    $Id$

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>
    Copyright (C) 2001 Arnd Bergmann <arnd@itreff.de>

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

#include <libvcd/vcd_image_vpkt.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_assert.h>

#include <string.h>

static const char _rcsid[] =
  "$Id$";

typedef struct
{
  VcdRecorder *recorder;
  int next_writable;	/* nwa of the recordable cd */
  int current_track;	/* number of track that is being written with next 
			 * write_sector command */
  int *track_offset;	/* sector start numbers for tracks */
}
_img_vpkt_snk_t;

static int
_set_cuesheet (void *user_data, const VcdList * vcd_cue_list)
{
  int track = 0;
  int track_offset[99];
  int last_sector = -1;
  _img_vpkt_snk_t *obj = user_data;
  VcdListNode *node;
  track_offset[track++] = 0;
  /* save positions for end of track marks */
  _VCD_LIST_FOREACH (node, (VcdList *) vcd_cue_list)
  {
    vcd_cue_t *_cue = _vcd_list_node_data (node);

    switch (_cue->type)
      {
      case VCD_CUE_END:
	vcd_info ("leadout at offset %d", _cue->lsn);
	track_offset[track++] = last_sector = _cue->lsn;
	break;
      case VCD_CUE_TRACK_START:
	vcd_info ("track %d, offset %d", track, _cue->lsn);
	break;
      case VCD_CUE_LEADIN:
	vcd_info ("leadin at %d", _cue->lsn);
	break;
      case VCD_CUE_PREGAP_START:
	vcd_info ("pregap at %d", _cue->lsn);
	track_offset[track++] = _cue->lsn;
	break;
      case VCD_CUE_SUBINDEX:
	/* index marks are supported only for DAO mode */
	vcd_info ("index mark at %d (ignored)", _cue->lsn);
	break;
      }
  }
  track_offset[track++] = -1;	/* marker for end of disc */
  obj->track_offset = _vcd_malloc (sizeof (int) * (track + 1));
  memcpy (obj->track_offset, track_offset, sizeof (int) * (track + 1));

  return 0;
}

static int
_close_track (_img_vpkt_snk_t * obj)
{
  int next_writable;
  int ret;

  /* close current track */
  vcd_info ("closing track %d", obj->current_track);
  ret = vcd_recorder_close_track (obj->recorder, obj->current_track);
  if (ret)
    return ret;
  ++obj->current_track;

  if (obj->track_offset[obj->current_track] == -1)
    {
      /* close session after last track is closed */
      vcd_info ("closing session");
      ret = vcd_recorder_close_track (obj->recorder, 0);
    }
  else
    {
      /* see if the sector number was predicted correctly */
      next_writable = vcd_recorder_get_next_writable (obj->recorder,
						      obj->current_track);
      obj->next_writable += 152;
      vcd_debug ("next_writable = %d - %d, offset %d",
		 next_writable, obj->next_writable,
		 next_writable - obj->next_writable);
    }
  return ret;
}

static int
_write (void *user_data, const void *data, uint32_t lsn)
{
  int ret;

  _img_vpkt_snk_t *obj = user_data;

  /* don't write inside pre-gap */
  if (lsn < obj->next_writable)
    {
#if 0
      vcd_debug ("sector %d is inside pre-gap", lsn);
#endif
      return 0;
    }

  /* wrong sector count? */
  if (obj->track_offset[obj->current_track] == -1)
    {
      vcd_warn ("refusing to write sector %d after session close", lsn);
      return -1;
    }

  /* write sector to buffer */
  ret = vcd_recorder_write_sector (obj->recorder, data, lsn);
  if (ret)
    return ret;

  obj->next_writable++;

  /* end of track, will also flush any sectors in the buffer */
  if (obj->next_writable == obj->track_offset[obj->current_track])
    {
      return _close_track (obj);
    }

  return 0;
}

static void
_sink_free (void *user_data)
{
  _img_vpkt_snk_t *obj = user_data;
  vcd_recorder_destroy (obj->recorder);
  if (obj->track_offset)
    free (obj->track_offset);
  free (obj);
}

static int
_sink_set_arg (void *user_data, const char key[], const char value[])
{
  _img_vpkt_snk_t *obj = user_data;

if (!strcmp (key, "simulate"))
    {
      bool simulate;
      if (!strcmp (value, "true"))
        simulate = true;
      else if (!strcmp (value, "false"))
        simulate = false;
      else return -2;

      if (vcd_recorder_set_simulate (obj->recorder, simulate))
        {
          vcd_error ("cannot set simulation mode to %s", 
            _vcd_bool_str(simulate));
          return -3;
        }
      return 0;
    }
  /* no valid arguments */
  else
    return -1;
}

VcdImageSink *
vcd_image_sink_new_vpkt (VcdRecorder * recorder)
{
  _img_vpkt_snk_t *obj;

  vcd_image_sink_funcs funcs = {
    set_cuesheet:_set_cuesheet,
    write:_write,
    free:_sink_free,
    set_arg: _sink_set_arg
  };

  if (!recorder)
    return NULL;

  if (vcd_recorder_set_simulate (recorder, false))
    vcd_error ("cannot disable simulation mode"); 
  if (vcd_recorder_set_write_type (recorder, _VCD_WT_VPKT))
    vcd_error ("cannot set variable packet mode");
  if (vcd_recorder_set_data_block_type (recorder, _VCD_DB_XA_FORM2_S))
    vcd_error ("cannot set FORM 2 data block type");
  if (vcd_recorder_set_speed (recorder, -1, -1))
    vcd_error ("cannot set speed");

  obj = _vcd_malloc (sizeof (_img_vpkt_snk_t));
  if (!obj)
    return NULL;

  obj->recorder = recorder;
  obj->current_track = 1;
  obj->next_writable = vcd_recorder_get_next_writable (recorder, 1);
  vcd_assert (obj->next_writable == 0);

  return vcd_image_sink_new (obj, &funcs);
}
