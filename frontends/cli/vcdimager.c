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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "vcd_types.h"
#include "vcd.h"
#include "vcd_stream_stdio.h"
#include "vcd_logging.h"
#include "vcd_cd_sector.h"

static const char _rcsid[] = "$Id$";

/* defaults */
#define DEFAULT_CUE_FILE       "videocd.cue"
#define DEFAULT_BIN_FILE       "videocd.bin"
#define DEFAULT_VOLUME_LABEL   "VideoCD"
#define DEFAULT_APPLICATION_ID ""
#define DEFAULT_TYPE           "vcd2"

/* global stuff kept as a singleton makes for less typing effort :-) 
 */
static struct
{
  const char *type;
  const char *image_fname;
  const char *cue_fname;
  char **track_fnames;

  struct add_files_t {
    char *fname;
    char *iso_fname;
    int raw_flag;
    struct add_files_t *next;
  } *add_files;

  char *add_files_path;

  const char *volume_label;
  const char *application_id;

  int sector_2336_flag;
  int broken_svcd_mode_flag;

  int verbose_flag;
  int quiet_flag;
  int progress_flag;

  vcd_log_handler_t default_vcd_log_handler;
}
gl;                             /* global */

/****************************************************************************/

static VcdObj *gl_vcd_obj = NULL;

static void 
_vcd_log_handler (log_level_t level, const char message[])
{
  if (level == LOG_DEBUG && !gl.verbose_flag)
    return;

  if (level == LOG_INFO && gl.quiet_flag)
    return;
  
  gl.default_vcd_log_handler (level, message);
}

static int
_progress_callback (const progress_info_t * info, void *user_data)
{
  fprintf (stdout, "#%d/%d: %ld/%ld (%2.0f%%)\r",
           info->in_track, info->total_tracks, info->sectors_written,
           info->total_sectors,
           (double) info->sectors_written / info->total_sectors * 100);

  return 0;
}

static int
_parse_file_arg (const char *arg, char **fname1, char **fname2)
{
  int rc = 0;
  char *tmp, *arg_cpy = strdup (arg);

  *fname1 = *fname2 = NULL;

  tmp = strtok(arg_cpy, ",");
  if (tmp)
    *fname1 = strdup (tmp);
  else
    rc = -1;
  
  tmp = strtok(NULL, ",");
  if (tmp)
    *fname2 = strdup (tmp);
  else
    rc = -1;
  
  tmp = strtok(NULL, ",");
  if (tmp)
    rc = -1;

  free (tmp);

  if(rc)
    {
      free (*fname1);
      free (*fname2);

      *fname1 = *fname2 = NULL;
    }

  return rc;
}

int
main (int argc, const char *argv[])
{
  int n = 0, sectors;
  vcd_type_t type_id;

  /* g_set_prgname (argv[0]); */

  gl.cue_fname = DEFAULT_CUE_FILE;
  gl.image_fname = DEFAULT_BIN_FILE;
  gl.track_fnames = NULL;

  gl.type = DEFAULT_TYPE;

  gl.volume_label = DEFAULT_VOLUME_LABEL;
  gl.application_id = DEFAULT_APPLICATION_ID;

  gl.default_vcd_log_handler = vcd_log_set_handler (_vcd_log_handler);

  {
    const char **args = NULL;
    int opt = 0;

    enum {
      CL_VERSION = 1,
      CL_ADD_FILE,
      CL_ADD_FILE_RAW
    };

    struct poptOption optionsTable[] = 
      {
        {"type", 't', POPT_ARG_STRING, &gl.type, 0,
         "select VideoCD type ('vcd11', 'vcd2' or 'svcd') (default: '" DEFAULT_TYPE "')", 
         "TYPE"},

        {"broken-svcd-mode", '\0', POPT_ARG_NONE, &gl.broken_svcd_mode_flag, 0,
         "enable non-compliant compatibility mode for broken devices"},
        
        {"iso-volume-label", 'l', POPT_ARG_STRING, &gl.volume_label, 0,
         "specify ISO volume label for video cd (default: '" DEFAULT_VOLUME_LABEL
         "')", "LABEL"},

        {"iso-application-id", '\0', POPT_ARG_STRING, &gl.application_id, 0,
         "specify ISO application id for video cd (default: '" DEFAULT_APPLICATION_ID
         "')", "LABEL"},

        {"cue-file", 'c', POPT_ARG_STRING, &gl.cue_fname, 0,
         "specify cue file for output (default: '" DEFAULT_CUE_FILE "')",
         "FILE"},
      
        {"bin-file", 'b', POPT_ARG_STRING, &gl.image_fname, 0,
         "specify bin file for output (default: '" DEFAULT_BIN_FILE "')",
         "FILE"},

        {"sector-2336", '\0', POPT_ARG_NONE, &gl.sector_2336_flag, 0,
         "use 2336 byte sectors for output"},

        {"add-file", '\0', POPT_ARG_STRING, NULL, CL_ADD_FILE, "add file to ISO fs",
         "FILE,ISO_FILENAME"},

        {"add-file-2336", '\0', POPT_ARG_STRING, NULL, CL_ADD_FILE_RAW, 
         "add file containing full 2336 byte sectors to ISO fs",
         "FILE,ISO_FILENAME"},

        {"progress", 'p', POPT_ARG_NONE, &gl.progress_flag, 0, "show progress"},
        {"verbose", 'v', POPT_ARG_NONE, &gl.verbose_flag, 0, "be verbose"},

        {"quiet", 'q', POPT_ARG_NONE, &gl.quiet_flag, 0, "show only critical messages"},

        {"version", 'V', POPT_ARG_NONE, NULL, CL_VERSION,
         "display version and copyright information and exit"},

        POPT_AUTOHELP 

        {NULL, 0, 0, NULL, 0}
      };
    
    poptContext optCon = poptGetContext ("vcdimager", argc, argv, optionsTable, 0);

    if (poptReadDefaultConfig (optCon, 0)) 
      fprintf (stderr, "warning, reading popt configuration failed\n"); 

    while ((opt = poptGetNextOpt (optCon)) != -1)
      switch (opt)
        {
        case CL_VERSION:
          fprintf (stdout, "GNU VCDImager " VERSION "\n\n"
                   "Copyright (c) 2001 Herbert Valerio Riedel <hvr@gnu.org>\n\n"
                   "VCDImager may be distributed under the terms of the GNU General Public Licence;\n"
                   "For details, see the file `COPYING', which is included in the VCDImager\n"
                   "distribution. There is no warranty, to the extent permitted by law.\n");
          exit (EXIT_SUCCESS);
          break;

        case CL_ADD_FILE:
        case CL_ADD_FILE_RAW:
          {
            const char *arg = poptGetOptArg (optCon);
            char *fname1 = NULL, *fname2 = NULL;

            assert (arg != NULL);

            if(!_parse_file_arg (arg, &fname1, &fname2)) 
              {
                struct add_files_t *tmp = malloc (sizeof (struct add_files_t));

                tmp->next = gl.add_files;
                gl.add_files = tmp;

                tmp->fname = fname1;
                tmp->iso_fname = fname2;
                tmp->raw_flag = (opt == CL_ADD_FILE_RAW);
              }
            else
              {
                fprintf (stderr, "file parsing of `%s' failed\n", arg);
                exit (EXIT_FAILURE);
              }
          }
          break;

        default:
          vcd_error ("error while parsing command line - try --help");
          break;
        }

    if (gl.verbose_flag && gl.quiet_flag)
      vcd_error ("I can't be both, quiet and verbose... either one or another ;-)");
    
    if ((args = poptGetArgs (optCon)) == NULL)
      vcd_error ("error: need at least one data track as argument "
                 "-- try --help");

    for (n = 0; args[n]; n++);

    if (n > CD_MAX_TRACKS - 1)
      vcd_error ("error: maximal number of supported mpeg tracks (%d) reached",
                 CD_MAX_TRACKS - 1);

    gl.track_fnames = malloc (sizeof (char *) * (n + 1));
    memset (gl.track_fnames, 0, sizeof (char *) * (n + 1));

    for (n = 0; args[n]; n++)
      gl.track_fnames[n] = strdup (args[n]);

    {
      struct {
        const char *str;
        vcd_type_t id;
      } type_str[] = 
        {
          { "vcd11", VCD_TYPE_VCD11 },
          { "vcd2", VCD_TYPE_VCD2 },
          { "svcd", VCD_TYPE_SVCD },
          { NULL, }
        };
      
      int i = 0;

      while (type_str[i].str) 
        if (strcasecmp(gl.type, type_str[i].str))
          i++;
        else
          break;

      if (!type_str[i].str)
        vcd_error ("invalid type given");
        
      type_id = type_str[i].id;
    }

    poptFreeContext (optCon);
  }

  /* done with argument processing */

  gl_vcd_obj = vcd_obj_new (type_id);

  vcd_obj_set_param (gl_vcd_obj, VCD_PARM_VOLUME_LABEL, gl.volume_label);
  vcd_obj_set_param (gl_vcd_obj, VCD_PARM_APPLICATION_ID, gl.application_id);

  if (type_id == VCD_TYPE_SVCD)
    {
      bool __tmp = gl.broken_svcd_mode_flag;
      vcd_obj_set_param (gl_vcd_obj, VCD_PARM_BROKEN_SVCD_MODE, &__tmp);
    }

  {
    struct add_files_t *p = gl.add_files;

    while(p)
      {
        fprintf (stdout, "debug: adding [%s] as [%s] (raw=%d)\n", p->fname, p->iso_fname, p->raw_flag);
        
        if(vcd_obj_add_file(gl_vcd_obj, p->iso_fname,
                            vcd_data_source_new_stdio (p->fname),
                            p->raw_flag))
          {
            fprintf (stderr, "error while adding file `%s' as `%s' to (S)VCD\n", p->fname, p->iso_fname);
            exit (EXIT_FAILURE);
          }

        p = p->next;
      }
  }

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

  fprintf (stdout, "finished ok, image created with %d sectors (%d bytes)\n",
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
