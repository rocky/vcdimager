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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <libvcd/vcd_assert.h>

#include "vcd_logging.h"

static const char _rcsid[] = "$Id$";

static void
default_vcd_log_handler (log_level_t level, const char message[])
{
  switch (level)
    {
    case LOG_ERROR:
      fprintf (stderr, "**ERROR: %s\n", message);
      fflush (stderr);
      exit (EXIT_FAILURE);
      break;
    case LOG_DEBUG:
      fprintf (stdout, "--DEBUG: %s\n", message);
      break;
    case LOG_WARN:
      fprintf (stdout, "++ WARN: %s\n", message);
      break;
    case LOG_INFO:
      fprintf (stdout, "   INFO: %s\n", message);
      break;
    case LOG_ASSERT:
      fprintf (stderr, "!ASSERT: %s\n", message);
      fflush (stderr);
      abort ();
      break;
    default:
      vcd_assert_not_reached ();
      break;
    }

  fflush (stdout);
}

static vcd_log_handler_t _handler = default_vcd_log_handler;

vcd_log_handler_t
vcd_log_set_handler (vcd_log_handler_t new_handler)
{
  vcd_log_handler_t old_handler = _handler;

  _handler = new_handler;

  return old_handler;
}

static void
vcd_logv (log_level_t level, const char format[], va_list args)
{
  char buf[1024] = { 0, };
  static int in_recursion = 0;

  if (in_recursion)
    vcd_assert_not_reached ();

  in_recursion = 1;
  
  vsnprintf(buf, sizeof(buf)-1, format, args);

  _handler(level, buf);

  in_recursion = 0;
}

void
vcd_log (log_level_t level, const char format[], ...)
{
  va_list args;
  va_start (args, format);
  vcd_logv (level, format, args);
  va_end (args);
}

#define VCD_LOG_TEMPLATE(level, LEVEL) \
void \
vcd_ ## level (const char format[], ...) \
{ \
  va_list args; \
  va_start (args, format); \
  vcd_logv (LOG_ ## LEVEL, format, args); \
  va_end (args); \
} 

VCD_LOG_TEMPLATE(debug, DEBUG)
VCD_LOG_TEMPLATE(info, INFO)
VCD_LOG_TEMPLATE(warn, WARN)
VCD_LOG_TEMPLATE(error, ERROR)

#undef VCD_LOG_TEMPLATE


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
