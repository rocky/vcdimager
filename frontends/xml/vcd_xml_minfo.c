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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <popt.h>

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_data_structures.h>
#include <libvcd/vcd_types.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_mpeg_stream.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_stream_stdio.h>

static const char _rcsid[] = "$Id$";

static int gl_quiet_flag = 0;
static int gl_verbose_flag = 0;

static vcd_log_handler_t gl_default_vcd_log_handler = NULL;

static void 
_vcd_log_handler (log_level_t level, const char message[])
{
  if (level == LOG_DEBUG && !gl_verbose_flag)
    return;

  if (level == LOG_INFO && gl_quiet_flag)
    return;
  
  gl_default_vcd_log_handler (level, message);
}

enum {
  OP_NONE = 0,
  OP_VERSION = 1 << 1
};

#define _TAG_PRINT(fd, tag, tp, val) \
 fprintf (fd, "  <" tag ">" tp "</" tag ">\n", val)

int
main (int argc, const char *argv[])
{
  char *_mpeg_fname = NULL;
  int _generic_info = 0;
  int _relaxed_aps = 0;
  int _dump_aps = 0;

  /* command line processing */
  {
    int opt = 0;

    struct poptOption optionsTable[] = {
      {"generic-info", 'i', POPT_ARG_NONE, &_generic_info, OP_NONE,
       "show generic information"},

      {"dump-aps", 'a', POPT_ARG_NONE, &_dump_aps, OP_NONE,
       "dump APS data"},

      {"relaxed-aps", '\0', POPT_ARG_NONE, &_relaxed_aps, OP_NONE,
       "relax APS constraints"},

      /* */
      
      {"verbose", 'v', POPT_ARG_NONE, &gl_verbose_flag, OP_NONE, 
       "be verbose"},
    
      {"quiet", 'q', POPT_ARG_NONE, &gl_quiet_flag, OP_NONE, 
       "show only critical messages"},

      {"version", 'V', POPT_ARG_NONE, NULL, OP_VERSION,
       "display version and copyright information and exit"},

      POPT_AUTOHELP

      {NULL, 0, 0, NULL, 0}
    };

    poptContext optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);

    /* end of local declarations */

    while ((opt = poptGetNextOpt (optCon)) != -1)
      switch (opt)
        {
        case OP_VERSION:
          fprintf (stdout, "GNU VCDImager " VERSION " [" HOST_ARCH "]\n\n"
                   "Copyright (c) 2001 Herbert Valerio Riedel <hvr@gnu.org>\n\n"
                   "GNU VCDRip may be distributed under the terms of the GNU General Public Licence;\n"
                   "For details, see the file `COPYING', which is included in the GNU VCDImager\n"
                   "distribution. There is no warranty, to the extent permitted by law.\n");
          exit (EXIT_SUCCESS);
          break;

        default:
          fprintf (stderr, "error while parsing command line - try --help\n");
          exit (EXIT_FAILURE);
          break;
        }

    {
      const char **args = NULL;
      int n;

      if ((args = poptGetArgs (optCon)) == NULL)
        vcd_error ("mpeg input file argument missing -- try --help");

      for (n = 0; args[n]; n++);

      if (n != 1)
        vcd_error ("only one xml input file argument allowed -- try --help");

      _mpeg_fname = strdup (args[0]);
    }

    poptFreeContext (optCon);
  } // command line processing

  gl_default_vcd_log_handler = vcd_log_set_handler (_vcd_log_handler);

  {
    VcdMpegSource *src;
    VcdListNode *n;

    vcd_debug ("trying to open mpeg stream...");

    src = vcd_mpeg_source_new (vcd_data_source_new_stdio (_mpeg_fname));

    vcd_mpeg_source_scan (src, _relaxed_aps ? false : true);

    vcd_debug ("stream scan completed");

    fprintf (stdout, "<mpeg-info>\n");

    if (_generic_info)
      { 
        const struct vcd_mpeg_source_info *_info = vcd_mpeg_source_get_info (src);

        fprintf (stdout, "<mpeg-properties>\n");

        _TAG_PRINT (stdout, "version", "%d", _info->version);

        _TAG_PRINT (stdout, "hsize", "%d", _info->hsize);
        _TAG_PRINT (stdout, "vsize", "%d", _info->vsize);
        _TAG_PRINT (stdout, "frame-rate", "%f", _info->frate);

        _TAG_PRINT (stdout, "playing-time", "%f", _info->playing_time);
        _TAG_PRINT (stdout, "packets", "%d", _info->packets);

        _TAG_PRINT (stdout, "video-bitrate", "%d", _info->bitrate);

        if (_info->video_e0)
          fprintf (stdout, "  <video-e0 /> <!-- motion -->\n");
        if (_info->video_e1)
          fprintf (stdout, "  <video-e1 /> <!-- still -->\n");
        if (_info->video_e2)
          fprintf (stdout, "  <video-e2 /> <!-- hi-res still -->\n");

        if (_info->audio_c0)
          fprintf (stdout, "  <audio-c0 /> <!-- standard audio -->\n");
        if (_info->audio_c1)
          fprintf (stdout, "  <audio-c1 /> <!-- 2nd audio -->\n");
        if (_info->audio_c2)
          fprintf (stdout, "  <audio-c2 /> <!-- MC5.1 surround -->\n");

        /* fprintf (stdout, " v: %d a: %d\n", _info->video_type, _info->audio_type); */

        fprintf (stdout, "</mpeg-properties>\n");
      }

    if (_dump_aps)
      {
        fprintf (stdout, "<aps-list>\n");
        if (_relaxed_aps)
          fprintf (stdout, "  <!-- relaxed aps -->\n");

        _VCD_LIST_FOREACH (n, vcd_mpeg_source_get_info (src)->aps_list)
          {
            struct aps_data *_data = _vcd_list_node_data (n);
            
            fprintf (stdout, "  <aps packet=\"%d\">%f</aps>\n",
                     _data->packet_no, _data->timestamp);
          }

        fprintf (stdout, "</aps-list>\n");
      }

    fprintf (stdout, "</mpeg-info>\n");

    fflush (stdout);

    vcd_mpeg_source_destroy (src, true);
  }

  free (_mpeg_fname);

  return EXIT_SUCCESS;
}

/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
