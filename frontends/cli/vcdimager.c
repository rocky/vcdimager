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

#include <glib.h>

#include <errno.h>
#include <fcntl.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "vcd.h"
#include "vcd_stream_stdio.h"
#include "vcd_logging.h"
#include "vcd_cd_sector.h"

/* defaults */
#define DEFAULT_CUE_FILE      "videocd.cue"
#define DEFAULT_BIN_FILE      "videocd.bin"
#define DEFAULT_VOLUME_LABEL  "VideoCD"

/* global stuff kept as a singleton makes for less typing effort :-) 
 */
static struct
{
  const gchar *image_fname;
  const gchar *cue_fname;
  gchar **track_fnames;

  gchar *add_files_path;

  const gchar *volume_label;

  gboolean sector_2336_flag;

  gboolean verbose_flag;
  gboolean progress_flag;
}
gl;                             /* global */

/* short cut's */
#define TRACK(n) (g_array_index(gl.track_infos, track_info_t, n))
#define NTRACKS  (gl.track_infos->len)

/****************************************************************************/

static VcdObj *gl_vcd_obj = NULL;

static int
_progress_callback (const progress_info_t * info, void *user_data)
{
  g_print ("#%d/%d: %ld/%ld (%2.0f%%)\r",
           info->in_track, info->total_tracks, info->sectors_written,
           info->total_sectors,
           (double) info->sectors_written / info->total_sectors * 100);

  return 0;
}

int
main (int argc, const char *argv[])
{
  gint n = 0, sectors;

  g_set_prgname (argv[0]);

  gl.cue_fname = DEFAULT_CUE_FILE;
  gl.image_fname = DEFAULT_BIN_FILE;
  gl.track_fnames = NULL;

  gl.volume_label = DEFAULT_VOLUME_LABEL;

  {
    const gchar **args = NULL;
    gint opt = 0;

    struct poptOption optionsTable[] = {

      {"volume-label", 'l', POPT_ARG_STRING, &gl.volume_label, 0,
       "specify volume label for video cd (default: '" DEFAULT_VOLUME_LABEL
       "')", "LABEL"},

      {"cue-file", 'c', POPT_ARG_STRING, &gl.cue_fname, 0,
       "specify cue file for output (default: '" DEFAULT_CUE_FILE "')",
       "FILE"},
      {"bin-file", 'b', POPT_ARG_STRING, &gl.image_fname, 0,
       "specify bin file for output (default: '" DEFAULT_BIN_FILE "')",
       "FILE"},

      {"sector-2336", 0, POPT_ARG_NONE, &gl.sector_2336_flag, 0,
       "use 2336 byte sectors for output"},

      {"progress", 'p', POPT_ARG_NONE, &gl.progress_flag, 0, "show progress"},
      {"verbose", 'v', POPT_ARG_NONE, &gl.verbose_flag, 0, "be verbose"},

      {"version", 'V', POPT_ARG_NONE, NULL, 1,
       "display version and copyright information and exit"},

      POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
    };

    poptContext optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);

    while ((opt = poptGetNextOpt (optCon)) != -1)
      switch (opt)
        {
        case 1:
          g_print ("GNU VCDImager " VERSION "\n\n"
                   "Copyright (c) 2000 Herbert Valerio Riedel <hvr@gnu.org>\n\n"
                   "VCDImager may be distributed under the terms of the GNU General Public Licence;\n"
                   "For details, see the file `COPYING', which is included in the VCDImager\n"
                   "distribution. There is no warranty, to the extent permitted by law.\n");
          exit (EXIT_SUCCESS);
          break;
        default:
          vcd_error ("error while parsing command line - try --help");
          break;
        }

    if ((args = poptGetArgs (optCon)) == NULL)
      vcd_error ("error: need at least one data track as argument "
                 "-- try --help");

    for (n = 0; args[n]; n++);

    if (n > CD_MAX_TRACKS - 1)
      vcd_error ("error: maximal number of supported mpeg tracks (%d) reached",
                 CD_MAX_TRACKS - 1);

    gl.track_fnames = g_new0 (char *, n + 1);

    for (n = 0; args[n]; n++)
      gl.track_fnames[n] = g_strdup (args[n]);

    poptFreeContext (optCon);
  }

  /* done with argument processing */

  gl_vcd_obj = vcd_obj_new (VCD_TYPE_VCD2);

  vcd_obj_set_param (gl_vcd_obj, VCD_PARM_VOLUME_LABEL, gl.volume_label);

  for (n = 0; gl.track_fnames[n] != NULL; n++)
    vcd_obj_append_mpeg_track (gl_vcd_obj,
                               vcd_data_source_new_stdio (gl.track_fnames[n]));

  sectors = vcd_obj_begin_output (gl_vcd_obj);

  vcd_obj_write_image (gl_vcd_obj,
                       vcd_data_sink_new_stdio (gl.image_fname),
                       gl.progress_flag ? _progress_callback : NULL, NULL);

  vcd_obj_write_cuefile (gl_vcd_obj, vcd_data_sink_new_stdio (gl.cue_fname),
                         gl.image_fname);

  vcd_obj_end_output (gl_vcd_obj);

  g_print ("finished ok, image created with %d sectors (%d bytes)\n",
           sectors, sectors * (gl.sector_2336_flag ? M2RAW_SIZE : CDDA_SIZE));

  return EXIT_SUCCESS;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
