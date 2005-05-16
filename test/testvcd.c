/*
 * Copyright (C) 2005 Rocky Bernstein <rocky@panix.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
/* Test the operation of vcd-info and vcdxrip on a real CD-ROM - not a 
   CD-ROM image. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <libvcd/info.h>
#include <libvcd/logging.h>

#define SKIP_TEST_RC 77

int
main(int argc, const char *argv[])
{
  int i_rc=0;
  vcdinfo_obj_t *p_vcdinfo;
  char *psz_source = NULL;
  char cmd[1024];
  struct stat statbuf;

  /* Set to give only errors on open, not warnings. */
  vcd_loglevel_default = VCD_LOG_ERROR; 

  if ( vcdinfo_open(&p_vcdinfo, &psz_source, DRIVER_DEVICE,
                    NULL) != VCDINFO_OPEN_VCD) {
    printf("Unable to find a CD-ROM with a (S)VCD in it; skipping test\n");
    return SKIP_TEST_RC;
  }
  vcd_loglevel_default = VCD_LOG_WARN;
  vcdinfo_close(p_vcdinfo);

  if (0 != stat("../frontends/cli/vcd-info", &statbuf)) {
    printf("Unable to find vcd-info program; skipping test\n");
    i_rc = SKIP_TEST_RC;
  } else {
    snprintf(cmd, sizeof(cmd), "../frontends/cli/vcd-info -i %s", psz_source);
    i_rc = system(cmd);
  }
  
  if (0 != stat("../frontends/xml/vcdxrip", &statbuf)) {
    printf("Unable to find vcdxrip program; skipping test\n");
    if (SKIP_TEST_RC == i_rc) return SKIP_TEST_RC;
  } else {
    snprintf(cmd, sizeof(cmd), 
	     "../frontends/xml/vcdxrip --norip --input=%s", psz_source);
    i_rc = system(cmd);
  }
  
  return i_rc;
}
