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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <libvcd/vcd_assert.h>

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

  vcd_assert (str_array != NULL);

  while(str_array[n])
    n++;

  return n;
}

void
_vcd_strfreev(char **strv)
{
  int n;
  
  vcd_assert (strv != NULL);

  for(n = 0; strv[n]; n++)
    free(strv[n]);

  free(strv);
}

char *
_vcd_strjoin (char *strv[], unsigned count, const char delim[])
{
  size_t len;
  char *new_str;
  unsigned n;

  vcd_assert (strv != NULL);
  vcd_assert (delim != NULL);

  len = (count-1) * strlen (delim);

  for (n = 0;n < count;n++)
    len += strlen (strv[n]);

  len++;

  new_str = _vcd_malloc (len);
  new_str[0] = '\0';

  for (n = 0;n < count;n++)
    {
      if (n)
        strcat (new_str, delim);
      strcat (new_str, strv[n]);
    }
  
  return new_str;
}

char **
_vcd_strsplit(const char str[], char delim) /* fixme -- non-reentrant */
{
  int n;
  char **strv = NULL;
  char *_str, *p;
  char _delim[2] = { 0, 0 };

  vcd_assert (str != NULL);

  _str = strdup(str);
  _delim[0] = delim;

  vcd_assert (_str != NULL);

  n = 1;
  p = _str;
  while(*p) 
    if (*(p++) == delim)
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

  vcd_assert (new_mem != NULL);

  memset (new_mem, 0, size);

  return new_mem;
}

void *
_vcd_memdup (const void *mem, size_t count)
{
  void *new_mem = NULL;

  if (mem)
    {
      new_mem = _vcd_malloc (count);
      memcpy (new_mem, mem, count);
    }
  
  return new_mem;
}

char *
_vcd_strdup_upper (const char str[])
{
  char *new_str = NULL;

  if (str)
    {
      char *p;

      p = new_str = strdup (str);

      while (*p)
        {
          *p = toupper (*p);
          p++;
        }
    }

  return new_str;
}

char *
_vcd_strncpy_pad(char dst[], const char src[], size_t len,
                 enum strncpy_pad_check _check)
{
  size_t rlen;

  vcd_assert (dst != NULL);
  vcd_assert (src != NULL);
  vcd_assert (len > 0);

  switch (_check)
    {
      int idx;
    case VCD_NOCHECK:
      break;

    case VCD_7BIT:
      for (idx = 0; src[idx]; idx++)
        if (src[idx] < 0)
          {
            vcd_warn ("string '%s' fails 7bit constraint (pos = %d)", 
                      src, idx);
            break;
          }
      break;

    case VCD_ACHARS:
      for (idx = 0; src[idx]; idx++)
        if (!_vcd_isachar (src[idx]))
          {
            vcd_warn ("string '%s' fails a-character constraint (pos = %d)",
                      src, idx);
            break;
          }
      break;

    case VCD_DCHARS:
      for (idx = 0; src[idx]; idx++)
        if (!_vcd_isdchar (src[idx]))
          {
            vcd_warn ("string '%s' fails d-character constraint (pos = %d)",
                      src, idx);
            break;
          }
      break;

    default:
      vcd_assert_not_reached ();
      break;
    }

  rlen = strlen (src);

  if (rlen > len)
    vcd_warn ("string '%s' is getting truncated to %d characters",  src, len);

  strncpy (dst, src, len);
  if (rlen < len)
    memset(dst+rlen, ' ', len-rlen);
  return dst;
}

int
_vcd_isdchar (int c)
{
  if (!IN (c, 0x30, 0x5f)
      || IN (c, 0x3a, 0x40)
      || IN (c, 0x5b, 0x5e))
    return false;

  return true;
}

int
_vcd_isachar (int c)
{
  if (!IN (c, 0x20, 0x5f)
      || IN (c, 0x23, 0x24)
      || c == 0x40
      || IN (c, 0x5b, 0x5e))
    return false;

  return true;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
