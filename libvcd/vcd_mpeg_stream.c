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
#include <stdio.h>
#include <stdlib.h>

#include <libvcd/vcd_mpeg_stream.h>

#include <libvcd/vcd_util.h>
#include <libvcd/vcd_data_structures.h>
#include <libvcd/vcd_mpeg.h>
#include <libvcd/vcd_logging.h>

static const char _rcsid[] = "$Id$";

struct _VcdMpegSource
{
  VcdDataSource *data_source;

  bool scanned;
  double pts_offset;
  
  /* _get_packet cache */
  unsigned _read_pkt_pos;
  unsigned _read_pkt_no;

  struct vcd_mpeg_source_info info;
};

/*
 * access functions
 */

VcdMpegSource *
vcd_mpeg_source_new (VcdDataSource *mpeg_file)
{
  VcdMpegSource *new_obj;
  
  vcd_assert (mpeg_file != NULL);

  new_obj = _vcd_malloc (sizeof (VcdMpegSource));

  new_obj->data_source = mpeg_file;
  new_obj->scanned = false;

  return new_obj;
}

void
vcd_mpeg_source_destroy (VcdMpegSource *obj, bool destroy_file_obj)
{
  vcd_assert (obj != NULL);

  if (destroy_file_obj)
    vcd_data_source_destroy (obj->data_source);

  _vcd_list_free (obj->info.aps_list, true);

  free (obj);
}

const struct vcd_mpeg_source_info *
vcd_mpeg_source_get_info (VcdMpegSource *obj)
{
  vcd_assert (obj != NULL);

  vcd_assert (obj->scanned);

  return &(obj->info);
}



long
vcd_mpeg_source_stat (VcdMpegSource *obj)
{
  vcd_assert (obj != NULL);
  vcd_assert (!obj->scanned);
  
  return obj->info.packets * 2324;
}

void
vcd_mpeg_source_scan (VcdMpegSource *obj, bool strict_aps)
{
  unsigned length = 0;
  unsigned pos = 0;
  unsigned pno = 0;
  VcdMpegStreamCtx state;
  VcdList *aps_list = 0;
  VcdListNode *n;
  bool _warned_padding = false;
  bool _pal = false;

  vcd_assert (obj != NULL);

  if (obj->scanned)
    {
      vcd_debug ("already scanned... not rescanning");
      return;
    }

  vcd_assert (!obj->scanned);

  memset (&state, 0, sizeof (state));

  vcd_data_source_seek (obj->data_source, 0);
  length = vcd_data_source_stat (obj->data_source);

  aps_list = _vcd_list_new ();

  while (pos < length)
    {
      char buf[2324] = { 0, };
      int read_len = MIN (sizeof (buf), (length - pos));
      int pkt_len;

      vcd_data_source_read (obj->data_source, buf, read_len, 1);

      pkt_len = vcd_mpeg_parse_packet (buf, read_len, true, &state);

      if (!pkt_len)
        {
          vcd_warn ("bad packet at packet #%d (stream byte offset %d)"
                    " -- remaining %d bytes of stream will be ignored",
                    pno, pos, length - pos);

          pos = length; /* don't fall into assert... */
          break;
        }

      switch (state.packet.aps)
        {
        case APS_NONE:
          break;

        case APS_I:
        case APS_GI:
          if (strict_aps)
            break; /* allow only if now strict aps */

        case APS_SGI:
          {
            struct aps_data *_data = _vcd_malloc (sizeof (struct aps_data));
            
            _data->packet_no = pno;
            _data->timestamp = state.packet.aps_pts;

            _vcd_list_append (aps_list, _data);
          }
          break;

        default:
          vcd_assert_not_reached ();
          break;
        }

      pos += pkt_len;
      pno++;

      if (pkt_len != read_len)
        {
          if (!_warned_padding)
            {
              vcd_warn ("mpeg stream will be padded on the fly -- hope that's ok for you!");
              _warned_padding = true;
            }

          vcd_data_source_seek (obj->data_source, pos);
        }
    }

  vcd_data_source_close (obj->data_source);

  vcd_assert (pos == length);

  obj->info.audio_c0 = state.stream.audio[0];
  obj->info.audio_c1 = state.stream.audio[1];
  obj->info.audio_c2 = state.stream.audio[2];

  obj->info.video_e0 = state.stream.video[0];
  obj->info.video_e1 = state.stream.video[1];
  obj->info.video_e2 = state.stream.video[2];

  if (!state.stream.audio[0] && !state.stream.audio[1] && !state.stream.audio[2])
    obj->info.audio_type = MPEG_AUDIO_NOSTREAM;
  else if (state.stream.audio[0] && !state.stream.audio[1] && !state.stream.audio[2])
    obj->info.audio_type = MPEG_AUDIO_1STREAM;
  else if (state.stream.audio[0] && state.stream.audio[1] && !state.stream.audio[2])
    obj->info.audio_type = MPEG_AUDIO_2STREAM;
  else if (state.stream.audio[0] && !state.stream.audio[1] && state.stream.audio[2])
    obj->info.audio_type = MPEG_AUDIO_EXT_STREAM;
  else
    {
      vcd_warn ("unsupported audio stream aggregation encountered! (c0: %d, c1: %d, c2: %d)",
                state.stream.audio[0], state.stream.audio[1], state.stream.audio[2]);
      obj->info.audio_type = MPEG_AUDIO_NOSTREAM;
    }

  obj->info.packets = state.stream.packets;
  obj->scanned = true;

  obj->info.playing_time = state.stream.max_pts - state.stream.min_pts;
  obj->pts_offset = state.stream.min_pts;

  if (state.stream.min_pts)
    vcd_debug ("pts start offset %f (max pts = %f)", 
               state.stream.min_pts, state.stream.max_pts);

  vcd_debug ("playing time %f", obj->info.playing_time);

  for(n = _vcd_list_begin (aps_list);
      n; n = _vcd_list_node_next (n))
    {
      struct aps_data *_data = _vcd_list_node_data (n);
      
      _data->timestamp -= state.stream.min_pts; /* pts offset */
    }

  obj->info.aps_list = aps_list;

  {
    int vid_idx = state.stream.first_shdr;
    
    if (vid_idx)
      {
        vid_idx--;

        obj->info.hsize = state.stream.shdr[vid_idx].hsize;
        obj->info.vsize = state.stream.shdr[vid_idx].vsize;
        obj->info.aratio = state.stream.shdr[vid_idx].aratio;
        obj->info.frate = state.stream.shdr[vid_idx].frate;
        obj->info.bitrate = state.stream.shdr[vid_idx].bitrate;
        obj->info.vbvsize = state.stream.shdr[vid_idx].vbvsize;
        obj->info.constrained_flag = state.stream.shdr[vid_idx].constrained_flag;
      }
  }  

  obj->info.version = state.stream.version;

  obj->info.norm = vcd_mpeg_get_norm (obj->info.hsize,
                                      obj->info.vsize,
                                      obj->info.frate);

  _pal = (obj->info.vsize == 576 || obj->info.vsize == 288);

  switch (state.stream.first_shdr)
    {
    case 0:
      obj->info.video_type = MPEG_VIDEO_NOSTREAM;
      break;
    case 1:
      obj->info.video_type = _pal ? MPEG_VIDEO_PAL_MOTION : MPEG_VIDEO_NTSC_MOTION;
      break;
    case 2:
      obj->info.video_type = _pal ? MPEG_VIDEO_PAL_STILL : MPEG_VIDEO_NTSC_STILL;
      break;
    case 3:
      obj->info.video_type = _pal ? MPEG_VIDEO_PAL_STILL2 : MPEG_VIDEO_NTSC_STILL2;
      break;
    }
}

int
vcd_mpeg_source_get_packet (VcdMpegSource *obj, unsigned long packet_no,
			    void *packet_buf, struct vcd_mpeg_packet_flags *flags)
{
  unsigned length;
  unsigned pos;
  unsigned pno;
  VcdMpegStreamCtx state;

  vcd_assert (obj != NULL);
  vcd_assert (obj->scanned);
  vcd_assert (packet_buf != NULL);

  if (packet_no >= obj->info.packets)
    {
      vcd_error ("invalid argument");
      return -1;
    }

  if (packet_no < obj->_read_pkt_no)
    {
      vcd_warn ("rewinding...");
      obj->_read_pkt_no = 0;
      obj->_read_pkt_pos = 0;
    }

  memset (&state, 0, sizeof (state));
  state.stream.seen_pts = true;
  state.stream.min_pts = obj->pts_offset;

  pos = obj->_read_pkt_pos;
  pno = obj->_read_pkt_no;
  length = vcd_data_source_stat (obj->data_source);

  vcd_data_source_seek (obj->data_source, pos);

  while (pos < length)
    {
      char buf[2324] = { 0, };
      int read_len = MIN (sizeof (buf), (length - pos));
      int pkt_len;
      
      vcd_data_source_read (obj->data_source, buf, read_len, 1);

      pkt_len = vcd_mpeg_parse_packet (buf, read_len, false, &state);

      vcd_assert (pkt_len > 0);

      if (pno == packet_no)
	{
	  obj->_read_pkt_pos = pos;
	  obj->_read_pkt_no = pno;

	  memset (packet_buf, 0, 2324);
	  memcpy (packet_buf, buf, pkt_len);

          if (flags)
            {
              memset (flags, 0, sizeof (struct vcd_mpeg_packet_flags));

              if (state.packet.video[0] 
                  || state.packet.video[1]
                  || state.packet.video[2])
                {
                  flags->type = PKT_TYPE_VIDEO;
                  flags->video_e0 = state.packet.video[0];
                  flags->video_e1 = state.packet.video[1];
                  flags->video_e2 = state.packet.video[2];
                }
              else if (state.packet.audio[0] 
                       || state.packet.audio[1]
                       || state.packet.audio[2])
                {
                  flags->type = PKT_TYPE_AUDIO;
                  flags->audio_c0 = state.packet.audio[0];
                  flags->audio_c1 = state.packet.audio[1];
                  flags->audio_c2 = state.packet.audio[2];
                }
              else if (state.packet.zero)
                flags->type = PKT_TYPE_ZERO;
              else if (state.packet.ogt)
                flags->type = PKT_TYPE_OGT;
              else if (state.packet.system_header || state.packet.padding)
                flags->type = PKT_TYPE_EMPTY;

              if (state.packet.pem)
                flags->pem = true;

              if ((flags->has_pts = state.packet.has_pts))
                flags->pts = state.packet.pts - state.stream.min_pts;
            }

	  return 0;
	}

      pos += pkt_len;
      pno++;

      if (pkt_len != read_len)
	vcd_data_source_seek (obj->data_source, pos);
    }

  vcd_assert (pos == length);

  vcd_error ("shouldnt be reached...");

  return -1;
}

void
vcd_mpeg_source_close (VcdMpegSource *obj)
{
  vcd_assert (obj != NULL);

  vcd_data_source_close (obj->data_source);
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
