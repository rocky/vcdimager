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

#include <libxml/parserInternals.h>
#include <libxml/parser.h>
#include <libxml/valid.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlerror.h>

#include "videocd_dtd.h"

#include "vcd_types.h"

#include "vcdxml.h"
#include "vcd_xml_parse.h"
#include "vcd_xml_master.h"

#define VIDEOCD_DTD_PUBID "-//GNU//DTD VideoCD//EN"
#define VIDEOCD_DTD_SYSID "http://www.gnu.org/software/vcdimager/videocd.dtd"
#define VIDEOCD_DTD_XMLNS "http://www.gnu.org/software/vcdimager/1.0/"

/* static xmlExternalEntityLoader _xmlExternalEntityLoaderDefault = 0; */
static bool videocd_dtd_loaded = false;

static xmlParserInputPtr 
_xmlExternalEntityLoader (const char *URL, const char *ID, xmlParserCtxtPtr context)
{

  videocd_dtd_loaded = true;

  if (ID && !strcmp (ID, VIDEOCD_DTD_PUBID))
    {
      xmlParserInputBufferPtr _input_buf;

      _input_buf = xmlParserInputBufferCreateMem (videocd_dtd, 
						  strlen (videocd_dtd),
						  XML_CHAR_ENCODING_8859_1);

      return xmlNewIOInputStream (context, _input_buf, 
				  XML_CHAR_ENCODING_8859_1);
    }

  printf ("%s (\"%s\", \"%s\", %p);\n", __PRETTY_FUNCTION__, URL, ID, context);
  printf ("unsupported doctype encountered\n");
  exit (EXIT_FAILURE);
  
  /* return _xmlExternalEntityLoaderDefault (URL, ID, context); */
}

static void
_init_xml (void)
{
  static bool _init_done = false;

  assert (!_init_done);
  _init_done = true;

  /* _xmlExternalEntityLoaderDefault = xmlGetExternalEntityLoader (); */
  xmlSetExternalEntityLoader (_xmlExternalEntityLoader);
}

static xmlDocPtr
_xmlParseFile(const char *filename)
{
  xmlDocPtr ret = NULL;
  xmlParserCtxtPtr ctxt = NULL;
  char *directory = NULL;

  /* assert (_init_done == true); */

  ctxt = xmlCreateFileParserCtxt(filename);
  
  if (!ctxt)
    return NULL;

  ctxt->keepBlanks = false;
  ctxt->pedantic = true; 
  ctxt->validate = true;
  ctxt->vctxt.nodeMax = 0;
  ctxt->vctxt.error = xmlParserValidityError;
  ctxt->vctxt.warning = xmlParserValidityWarning;

  if (!ctxt->directory 
      && (directory = xmlParserGetDirectory(filename)))
    ctxt->directory = (char *) xmlStrdup((xmlChar *) directory);
  
  xmlParseDocument(ctxt);

  if (ctxt->wellFormed && ctxt->valid)
    ret = ctxt->myDoc;
  else
    {
      xmlFreeDoc (ctxt->myDoc);
      ctxt->myDoc = NULL;
    }

  xmlFreeParserCtxt(ctxt);
    
  return(ret);
}

int 
main (int argc, const char *argv[])
{
  int rc = EXIT_FAILURE;
  xmlDocPtr vcd_doc;

  _init_xml ();
  
  if (argc != 2)
    {
      printf ("argc != 2\n");
      return EXIT_FAILURE;
    }

  if (!(vcd_doc = _xmlParseFile (argv[1])))
    {
      printf ("parsing file failed\n");
      return EXIT_FAILURE;
    }

  if (!videocd_dtd_loaded)
    {
      printf ("doctype declaration missing\n");
      return EXIT_FAILURE;
    }
    

  do {
    xmlNodePtr root;
    xmlNsPtr ns;
    struct vcdxml_t obj;
    
    if (!(root = xmlDocGetRootElement (vcd_doc)))
      {
	printf ("sorry, doc is empty\n");
	break;
      }

    if (!(ns = xmlSearchNsByHref (vcd_doc, root, VIDEOCD_DTD_XMLNS)))
      {
	printf ("sorry, namespace not found in doc\n");
	break;
      }

    if (vcd_xml_parse (&obj, vcd_doc, root, ns))
      {
	printf ("sorry, parsing failed\n");
	break;
      }

    if (vcd_xml_master (&obj))
      {
	printf ("sorry, mastering vcd failed\n");
	break;
      }

    printf ("everything worked!\n");

    rc = EXIT_SUCCESS;
  } while (false);

  /* xmlDocDump (stdout, vcd_doc); */

  xmlFreeDoc (vcd_doc);

  return rc;
}
