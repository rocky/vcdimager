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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "vcd_util.h"

static const char _rcsid[] = "$Id$";

unsigned
_vcd_len2blocks (unsigned len, int blocksize)
{
  unsigned blocks;

  blocks = len / blocksize;
  if (len % blocksize)
    blocks++;

  return blocks;
}

size_t
_vcd_strlenv(char **str_array)
{
  size_t n = 0;

  assert(str_array != NULL);

  while(str_array[n])
    n++;

  return n;
}

void
_vcd_strfreev(char **strv)
{
  int n;
  
  assert(strv != NULL);

  for(n = 0; strv[n]; n++)
    free(strv[n]);

  free(strv);
}

char **
_vcd_strsplit(const char str[], char delim)
{
  int n;
  char **strv = NULL;
  char *_str, *p;
  char _delim[2] = { 0, 0 };

  assert(str != NULL);

  _str = strdup(str);
  _delim[0] = delim;

  assert(_str != NULL);

  n = 1;
  p = _str;
  while(*p) 
    if(*(p++) == delim)
      n++;

  strv = _vcd_malloc (sizeof (char *) * (n+1));
  
  n = 0;
  while((p = strtok(n ? NULL : _str, _delim)) != NULL) 
    strv[n++] = strdup(p);

  free(_str);

  return strv;
}

void *
_vcd_malloc (size_t size)
{
  void *new_mem = malloc (size);

  assert (new_mem != NULL);

  memset (new_mem, 0, size);

  return new_mem;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
