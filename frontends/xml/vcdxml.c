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

#define VIDEOCD_DTD_PUBID "-//GNU//DTD VideoCD//EN"
#define VIDEOCD_DTD_SYSID "http://www.gnu.org/software/vcdimager/videocd.dtd"
#define VIDEOCD_DTD_XMLNS "http://www.gnu.org/software/vcdimager/1.0/"

static xmlExternalEntityLoader _xmlEEL = 0;

#define FOR_EACH(iter, parent) for(iter = parent->xmlChildrenNode; iter != NULL; iter = iter->next)

#define GET_ELEM_STR(str, id, doc, node, ns) \
 if ((!xmlStrcmp (node->name, (const xmlChar *) id)) && (node->ns == ns)) \
   free (str), str = xmlNodeListGetString (doc, node->xmlChildrenNode, 1)

static xmlParserInputPtr 
_myEEL (const char *URL, const char *ID, xmlParserCtxtPtr context)
{
  printf ("%s (\"%s\", \"%s\", %p);\n", __PRETTY_FUNCTION__, URL, ID, context);

  if (ID && !strcmp (ID, VIDEOCD_DTD_PUBID))
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

typedef bool (*_parse_node_func) (xmlDocPtr, xmlNodePtr, xmlNsPtr);

struct {
  xmlChar *class;
  xmlChar *version;
  xmlChar *options;
  struct {
    xmlChar *album_id;
    xmlChar *volume_count;
    xmlChar *volume_number;
    xmlChar *restriction;
    xmlChar *time_offset;
  } info;
  struct {
    xmlChar *volume_id;
    xmlChar *system_id;
    xmlChar *application_id;
    xmlChar *preparer_id;
    xmlChar *publisher_id;
  } pvd;

  unsigned mpeg_tracks_count;
  struct {
    xmlChar *id;
    unsigned entry_point_count;
    struct {
      xmlChar *id;
      xmlChar *timestamp;
    } *entry_points;
  } *mpeg_tracks;

} gl_data;

static bool
_parse_info (xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  FOR_EACH (cur, node)
    {
      GET_ELEM_STR (gl_data.info.album_id, "album-id", doc, cur, ns);
      else GET_ELEM_STR (gl_data.info.volume_count, "volume-count", doc, cur, ns);
      else GET_ELEM_STR (gl_data.info.volume_number, "volume-number", doc, cur, ns);
      else GET_ELEM_STR (gl_data.info.restriction, "restriction", doc, cur, ns);
      else printf ("??? %s\n", cur->name); 
    }

  return false;
}

static bool
_parse_pvd (xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  FOR_EACH (cur, node)
    {
      GET_ELEM_STR (gl_data.pvd.volume_id, "volume-id", doc, cur, ns);
      else GET_ELEM_STR (gl_data.pvd.system_id, "system-id", doc, cur, ns);
      else GET_ELEM_STR (gl_data.pvd.application_id, "application-id", doc, cur, ns);
      else GET_ELEM_STR (gl_data.pvd.preparer_id, "preparer-id", doc, cur, ns);
      else GET_ELEM_STR (gl_data.pvd.publisher_id, "publisher-id", doc, cur, ns);
      else printf ("??? %s\n", cur->name); 
    }

  return false;
}

static bool
_parse_mpeg_track (xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  printf ("%s\n", __PRETTY_FUNCTION__);

  if (gl_data.mpeg_tracks_count >= 98)
    {
      printf "too many tracks\n";
      return true;
    }

  FOR_EACH (cur, node)
    {
      xmlChar *_tmp = NULL;

      GET_ELEM_STR (_tmp, "entry", doc, cur, ns);
      else printf ("??? %s\n", cur->name); 
    }  

  return false;
}

static bool
_parse_tracks (xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  FOR_EACH (cur, node)
    {
      bool rc = false;

      if (!xmlStrcmp (cur->name, "mpeg-track")) rc = _parse_mpeg_track (doc, cur, ns);
      else printf ("unexpected element: %s\n", cur->name);

      if (rc)
	return rc;
    }

  return false;
}

static bool
_traverse_doc (xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  if (xmlStrcmp (node->name, "videocd") || (node->ns != ns))
    {
      printf ("root element not videocd...\n");
      return true;
    }

  

  for (cur = node->children; cur; cur = cur->next)
    {
      bool rc = false;
      if (cur->ns != ns)
	{
	  printf ("foreign namespace ignored (%s)\n", cur->name);
	  continue;
	}

      gl_data.class = xmlGetProp (cur, "class");
      gl_data.version = xmlGetProp (cur, "version");
      gl_data.options = xmlGetProp (cur, "options");

      if (!xmlStrcmp (cur->name, "info")) rc = _parse_info (doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "pvd")) rc = _parse_pvd (doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "tracks")) rc = _parse_tracks (doc, cur, ns);
      else printf ("unexpected element: %s\n", cur->name);

      if (rc)
	return rc;
    }

  return false;
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

  if (!(vcd_doc = xmlParseFile (argv[1])))
    {
      printf ("parsing file failed\n");
      return EXIT_FAILURE;
    }

  do {
    xmlNodePtr root;
    xmlNsPtr ns;

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

    if (_traverse_doc (vcd_doc, root, ns))
      {
	printf ("sorry, parsing failed\n");
	break;
      }

    rc = EXIT_SUCCESS;
  } while (false);

  xmlFreeDoc (vcd_doc);

  //xmlDocDump (stdout, vcd_doc);

  return rc;
}
