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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <libvcd/vcd_types.h>
#include <libvcd/vcd_mpeg_stream.h>
#include <libvcd/vcd_stream_stdio.h>

int 
main (int argc, const char *argv[])
{
  VcdMpegSource *src;
  VcdListNode *n;
  double t = 0;

  if (argc != 2)
    return 1;

  src = vcd_mpeg_source_new (vcd_data_source_new_stdio (argv[1]));

  vcd_mpeg_source_scan (src, true, NULL, NULL);

  printf ("packets: %d\n", vcd_mpeg_source_get_info (src)->packets);

  _VCD_LIST_FOREACH (n, vcd_mpeg_source_get_info (src)->shdr[0].aps_list)
    {
      struct aps_data *_data = _vcd_list_node_data (n);
      
      printf ("aps: %d %f\n", _data->packet_no, _data->timestamp);
    }

  {
    VcdListNode *aps_node = _vcd_list_begin (vcd_mpeg_source_get_info (src)->shdr[0].aps_list);
    struct aps_data *_data;
    double aps_time;
    int aps_packet;

    _data = _vcd_list_node_data (aps_node);
    aps_time = _data->timestamp;
    aps_packet = _data->packet_no;


    for (t = 0; t <= vcd_mpeg_source_get_info (src)->playing_time; t += 0.5)
      {
        for(n = _vcd_list_node_next (aps_node); n; n = _vcd_list_node_next (n))
          {
            _data = _vcd_list_node_data (n);

            if (fabs (_data->timestamp - t) < fabs (aps_time - t))
              {
                aps_node = n;
                aps_time = _data->timestamp;
                aps_packet = _data->packet_no;
              }
            else 
              break;
          }

        printf ("%f %f %d\n", t, aps_time, aps_packet);
      }

  }

  {
    const struct vcd_mpeg_stream_info *_info = vcd_mpeg_source_get_info (src);
    printf ("mpeg info\n");
  
    printf (" %d x %d (%f:1) @%f v%d\n", _info->shdr[0].hsize, _info->shdr[0].vsize,
	    _info->shdr[0].aratio, _info->shdr[0].frate, _info->version);
  }

  vcd_mpeg_source_destroy (src, true);


  return 0;
}
