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

#include "vcd_stream_stdio.h"
#include "vcd_logging.h"
#include "vcd_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/stat.h>
#include <errno.h>

static const char _rcsid[] = "$Id$";

typedef struct {
  char *pathname;
  FILE *fd;
} _UserData;

static int
_stdio_open_source(void *user_data) 
{
  _UserData *ud = user_data;
  
  ud->fd = fopen(ud->pathname, "rb");

  return (ud->fd == NULL);
}

static int
_stdio_open_sink(void *user_data) 
{
  _UserData *ud = user_data;
  
  ud->fd = fopen(ud->pathname, "wb");

  return (ud->fd == NULL);
}

static int
_stdio_close(void *user_data)
{
  _UserData *ud = user_data;

  fclose(ud->fd);

  ud->fd = NULL;

  return 0;
}

static void
_stdio_free(void *user_data)
{
  _UserData *ud = user_data;

  if(ud->pathname)
    free(ud->pathname);

  if(ud->fd) /* should be NULL anyway... */
    _stdio_close(user_data); 

  free(ud);
}

static long
_stdio_seek(void *user_data, long offset)
{
  _UserData *ud = user_data;
  
  fseek(ud->fd, offset, SEEK_SET);

  return offset;
}

static long
_stdio_stat(void *user_data)
{
  _UserData *ud = user_data;
  struct stat statbuf;
  
  if(fstat(fileno(ud->fd), &statbuf))
    return -1;

  return statbuf.st_size;
}

static long
_stdio_read(void *user_data, void *buf, long count)
{
  _UserData *ud = user_data;

  return fread(buf, 1, count, ud->fd);
}

static long
_stdio_write(void *user_data, const void *buf, long count)
{
  _UserData *ud = user_data;

  return fwrite(buf, 1, count, ud->fd);
}

VcdDataSource*
vcd_data_source_new_stdio(const char pathname[])
{
  VcdDataSource *new_obj = NULL;
  vcd_data_source_io_functions funcs;
  _UserData *ud = NULL;
  struct stat statbuf;
  
  if(stat(pathname, &statbuf) == -1) {
    vcd_error("could not stat() file `%s': %s", pathname, strerror(errno));
    return NULL;
  }


  ud = _vcd_malloc (sizeof (_UserData));

  memset(&funcs, 0, sizeof(funcs));

  ud->pathname = strdup(pathname);

  funcs.open = _stdio_open_source;
  funcs.seek = _stdio_seek;
  funcs.stat = _stdio_stat;
  funcs.read = _stdio_read;
  funcs.close = _stdio_close;
  funcs.free = _stdio_free;

  new_obj = vcd_data_source_new(ud, &funcs);


  return new_obj;
}


VcdDataSink*
vcd_data_sink_new_stdio(const char pathname[])
{
  VcdDataSink *new_obj = NULL;
  vcd_data_sink_io_functions funcs;
  _UserData *ud = NULL;

  ud = _vcd_malloc (sizeof (_UserData));

  memset(&funcs, 0, sizeof(funcs));

  ud->pathname = strdup(pathname);

  funcs.open = _stdio_open_sink;
  funcs.seek = _stdio_seek;
  funcs.write = _stdio_write;
  funcs.close = _stdio_close;
  funcs.free = _stdio_free;

  new_obj = vcd_data_sink_new(ud, &funcs);

  return new_obj;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
