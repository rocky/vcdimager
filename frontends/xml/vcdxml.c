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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/valid.h>
#include "videocd_dtd.h"

#include "vcd_types.h"

static xmlExternalEntityLoader _xmlEEL = 0;

static xmlParserInputPtr 
_myEEL (const char *URL, const char *ID, xmlParserCtxtPtr context)
{
  printf ("%s (\"%s\", \"%s\", %p);\n", __PRETTY_FUNCTION__, URL, ID, context);

  if (ID && !strcmp (ID, "-//GNU//DTD VideoCD//EN"))
    {
      xmlParserInputBufferPtr _input_buf;

      _input_buf = xmlParserInputBufferCreateMem (videocd_dtd, 
						  strlen (videocd_dtd),
						  XML_CHAR_ENCODING_8859_1);

      return xmlNewIOInputStream (context, _input_buf, 
				  XML_CHAR_ENCODING_8859_1);
    }

  printf ("unsupported doctype encountered\n");
  exit (EXIT_FAILURE);
  
  return _xmlEEL (URL, ID, context);
}

static void
_init_xml (void)
{
  extern int xmlDoValidityCheckingDefaultValue;
  static bool _init_done = false;

  assert (_init_done == false);
  _init_done = true;

  xmlDoValidityCheckingDefaultValue = 1;

  xmlKeepBlanksDefault (0);

  _xmlEEL = xmlGetExternalEntityLoader ();
  xmlSetExternalEntityLoader (_myEEL);
}

int 
main (int argc, const char *argv[])
{
  xmlDocPtr vcd_doc;

  _init_xml ();
  
  if (argc != 2)
    {
      printf ("argc != 2\n");
      return 1;
    }

  if (!(vcd_doc = xmlParseFile (argv[1])))
    {
      printf ("parsing file failed\n");
      return 1;
    }

  //xmlDocDump (stdout, vcd_doc);

  return 0;
}

/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
