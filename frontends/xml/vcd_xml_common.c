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

#include <stdio.h>
#include <stdlib.h>

#include <libvcd/vcd.h>
#include <libvcd/vcd_assert.h>

#include "vcd_xml_common.h"

static const char _rcsid[] = "$Id$";

bool vcd_xml_gui_mode = false;

log_level_t vcd_xml_verbosity = LOG_INFO;

bool vcd_xml_show_progress = false;

static vcd_log_handler_t __default_vcd_log_handler = 0;

static void 
_vcd_xml_log_handler (log_level_t level, const char message[])
{

  if (level < vcd_xml_verbosity)
    return;

  if (vcd_xml_gui_mode)
    {
      const char *_level_str = "unknown";

      switch (level)
	{
	case LOG_DEBUG:
	  _level_str = "debug";
	  break;
	case LOG_WARN:
	  _level_str = "warning";
	  break;
	case LOG_INFO:
	  _level_str = "information";
	  break;
	case LOG_ASSERT:
	  _level_str = "assertion";
	  break;
	case LOG_ERROR:
	  _level_str = "error";
	  break;
	}

      fprintf (stdout, "<log level=\"%s\">%s</log>\n", _level_str, message);
      fflush (stdout);
    }
  else
    __default_vcd_log_handler (level, message);

  if (level == LOG_ERROR
      || level == LOG_ASSERT)
    exit (EXIT_FAILURE);
}

void
vcd_xml_log_init (void)
{
  vcd_assert (__default_vcd_log_handler == 0);
  __default_vcd_log_handler = vcd_log_set_handler (_vcd_xml_log_handler);
}

int 
vcd_xml_scan_progress_cb (const vcd_mpeg_prog_info_t *info, void *user_data)
{
  if (!vcd_xml_show_progress)
    return 0;

  if (vcd_xml_gui_mode)
    fprintf (stdout, "<progress operation=\"scan\" id=\"%s\" position=\"%ld\" size=\"%ld\" />\n",
	     (char *) user_data, info->current_pos, info->length);
  else
    fprintf (stdout, "#scan[%s]: %ld/%ld (%2.0f%%)          \r",
	     (char *) user_data, info->current_pos, info->length,
	     (double) info->current_pos / info->length * 100);

  fflush (stdout);
}

int
vcd_xml_write_progress_cb (const progress_info_t *info, void *user_data)
{
  if (!vcd_xml_show_progress)
    return 0;

  if (vcd_xml_gui_mode)
    fprintf (stdout, "<progress operation=\"write\" position=\"%ld\" size=\"%ld\" />\n",
	     info->sectors_written, info->total_sectors);
  else
    fprintf (stdout, "#write[%d/%d]: %ld/%ld (%2.0f%%)          \r",
	     info->in_track, info->total_tracks, info->sectors_written,
	     info->total_sectors,
	     (double) info->sectors_written / info->total_sectors * 100);

  fflush (stdout);

  return 0;
}
