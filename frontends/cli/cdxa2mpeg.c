/* -*- c -*- 
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

/* quick'n'dirty RIFF CDXA 2 MPEG converter */

#define _GNU_SOURCE

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_types.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_bytesex.h>
#include <libvcd/vcd_util.h>

typedef struct 
{
  FILE *fd;
  FILE *fd_out;
  uint32_t size;
  uint32_t lsize;
} riff_context;

static int next_id (riff_context *ctxt);

static uint32_t
read_le_u32 (riff_context *ctxt)
{
  uint32_t tmp;

  if (fread (&tmp, sizeof (uint32_t), 1, ctxt->fd) != 1)
    vcd_error ("fread () failed");

  return uint32_from_le (tmp);
}

static int 
handler_RIFF (riff_context *ctxt)
{
  const uint32_t size = read_le_u32 (ctxt);

  vcd_info ("RIFF data[%d]", size);
  
  ctxt->lsize = ctxt->size = size;

  return next_id (ctxt);
}

static int 
handler_CDXA (riff_context *ctxt)
{
  vcd_info ("CDXA RIFF detected");

  next_id (ctxt); /* fmt */
  next_id (ctxt); /* data */

  return 0;
}

static int 
handler_data (riff_context *ctxt)
{
  const uint32_t size = read_le_u32 (ctxt);
  uint32_t sectors;

  if (size % 2352)
    vcd_warn ("size not a multiple of 2352 bytes!!");
  sectors = size / 2352;

  vcd_info ("CDXA data[%d] (%d sectors)", size, sectors);

  if (ctxt->fd_out)
    {
      long first_nzero = -1, last_nzero = -1, s;
      struct {
	uint8_t sync[12];
	uint8_t header[4];
	uint8_t subheader[8];
	uint8_t data[2324];
	uint8_t edc[4];
      } sbuf;
      
      vcd_assert (sizeof (sbuf) == 2352);

      vcd_info ("...converting...");

      for (s = 0; s < sectors; s++)
	{
	  int r = fread (&sbuf, 2352, 1, ctxt->fd);
	  bool empty = true;

	  {
	    int i;
	    for (i = 0; (i < 2324) && !sbuf.data[i]; i++);
	    empty = i == 2324;
	  }

	  if (!r)
	    {
	      if (ferror (ctxt->fd))
		vcd_error ("fread (): %s", strerror (errno));

	      if (feof (ctxt->fd))
		vcd_warn ("premature end of file encountered after %ld sectors", s);
	      
	      fclose (ctxt->fd);
	      fclose (ctxt->fd_out);
	      exit (EXIT_FAILURE);
	    }

	  if (empty)
	    {
	      if (first_nzero == -1)
		continue;
	    }
	  else
	    {
	      last_nzero = s;

	      if (first_nzero == -1)
		first_nzero = s;
	    }

	  fwrite (&sbuf.data, 2324, 1, ctxt->fd_out);
	}

      fflush (ctxt->fd_out);
      
      {
	const long allsecs = (last_nzero - first_nzero + 1);
	ftruncate (fileno (ctxt->fd_out), allsecs * 2324);
	
	vcd_info ("...stripped %ld leading and %ld trailing empty sectors...",
		first_nzero, (sectors - last_nzero - 1));
	vcd_info ("...extraction done (%ld sectors extracted to file)!", allsecs);
      }
    }
  else
    vcd_warn ("no extraction done, since no output file was given");

  return 0;
}

static int 
handler_fmt (riff_context *ctxt)
{
  uint8_t buf[1024] = { 0, };
  const uint32_t size = read_le_u32 (ctxt);
  int i;

  vcd_assert (size < sizeof (buf));
  fread (buf, 1, (size % 2) ? size + 1 : size, ctxt->fd);

  {
    char *strbuf = _vcd_malloc (1 + size * 6);
    strbuf[0] = '\0';

    for (i = 0; i < size; i++)
      {
        char _buf[7] = { 0, };
        snprintf (_buf, sizeof (_buf) - 1, "%.2x ", buf[i]);
        strcat (strbuf, _buf);
      }

    vcd_info ("CDXA fmt[%d] = 0x%s", size, strbuf);

    free (strbuf);
  }

  return 0;
}

static int
handle (riff_context *ctxt, char id[4])
{
  struct {
    char id[4];
    int (*handler) (riff_context *);
  } handlers[] = {
    { "RIFF", handler_RIFF},
    { "CDXA", handler_CDXA},
    { "fmt ", handler_fmt},
    { "data", handler_data},
    { "", 0}
  }, *p = handlers;

  for (; p->id[0]; p++)
    if (!strncmp (p->id, id, 4))
      return p->handler (ctxt);

  vcd_warn ("unknown chunk id [%.4s] encountered", id);

  return -1;
}

static int
next_id (riff_context *ctxt)
{
  char id[4] = { 0, };

  fread (id, 1, 4, ctxt->fd);

  return handle (ctxt, id);
}

static void
usage (void)
{
  printf ("usage: cdxa2mpg infile [outfile]\n\n"
	  "description: \n"
	  "  Converts a Video CD RIFF CDXA file to plain mpeg streams\n\n"
	  "copyright: \n"
	  "  Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>\n"
	  "  This is free software; see the source for copying conditions.  There is NO\n"
	  "  warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");

  exit (EXIT_FAILURE);
}

int 
main (int argc, char *argv[])
{
  FILE *in = NULL, *out = NULL;
  riff_context ctxt = { 0, };

  if (argc == 2 || argc == 3)
    {
      in = fopen (argv[1], "rb");
      if (!in) 
	{
	  vcd_error ("fopen (): %s", strerror (errno));
	  exit (EXIT_FAILURE);
	}
    }
  else
    usage ();

  if (argc == 3)
    {
      out = fopen (argv[2], "wb");
      if (!out) 
	{
	  vcd_error ("fopen (): %s", strerror (errno));
	  exit (EXIT_FAILURE);
	}
    }
  
  ctxt.fd = in;
  ctxt.fd_out = out;
  
  next_id (&ctxt);

  if (in)
    fclose (in);

  if (out)
    fclose (out);
 
  return 0;
}

/*
  Local Variables:
  c-file-style: "gnu"
  tab-width: 8
  indent-tabs-mode: nil
  End:
*/ 
