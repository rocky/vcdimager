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
#include <stdlib.h>
#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlmemory.h>
#include <libxml/uri.h>

#define FOR_EACH(iter, parent) for(iter = parent->xmlChildrenNode; iter != NULL; iter = iter->next)

#include "vcd_xml_dump.h"
#include "vcd_xml_dtd.h"

static const char _rcsid[] = "$Id$";

static xmlNodePtr 
_get_file_node (xmlDocPtr doc, xmlNodePtr cur, xmlNsPtr ns, const char pathname[])
{
  char *_dir, *c;
  xmlNodePtr retval = NULL;

  _dir = strdup (pathname);
  c = strchr (_dir, '/');

  if (c)
    { /* subdir... */
      xmlNodePtr n;

      *c++ = '\0';

      FOR_EACH (n, cur)
        {
          char *tmp;

          if (xmlStrcmp (n->name, (const xmlChar *) "folder"))
            continue;

          assert (!xmlStrcmp (n->children->name, "name"));

          tmp = xmlNodeListGetString (doc, n->children->children, 1);

          if (!xmlStrcmp (tmp, _dir))
            break;
        }

      if (!n)
        {
          n = xmlNewChild (cur, ns, "folder", NULL);
          xmlNewChild (n, ns, "name", _dir);
        }

      retval = _get_file_node (doc, n, ns, c);
    }
  else
    { /* finally there! */
      retval = xmlNewChild (cur, ns, "file", NULL);
      xmlNewChild (retval, ns, "name", pathname);
    }

  free (_dir);

  return retval;
}

static void
_make_xml (struct vcdxml_t *obj, const char xml_fname[])
{
  xmlDtdPtr dtd;
  xmlDocPtr doc;
  xmlNodePtr vcd_node, section;
  xmlNsPtr ns = NULL;
  char buf[1024];
  VcdListNode *node;

  xmlKeepBlanksDefault(0);

  doc = xmlNewDoc ("1.0");
  
  dtd = xmlNewDtd (doc, "videocd", VIDEOCD_DTD_PUBID, VIDEOCD_DTD_SYSID);
  xmlAddChild ((xmlNodePtr) doc, (xmlNodePtr) dtd);

  if (obj->comment)
    xmlAddChild ((xmlNodePtr) doc, xmlNewComment (obj->comment));

  vcd_node = xmlNewDocNode (doc, ns, "videocd", NULL);
  xmlAddChild ((xmlNodePtr) doc, vcd_node);

  ns = xmlNewNs (vcd_node, VIDEOCD_DTD_XMLNS, NULL);
  xmlSetNs (vcd_node, ns);

  switch (obj->vcd_type) 
    {
    case VCD_TYPE_SVCD:
      xmlSetProp (vcd_node, "class", "svcd");
      xmlSetProp (vcd_node, "version", "1.0");
      break;
    case VCD_TYPE_VCD2:
      xmlSetProp (vcd_node, "class", "vcd");
      xmlSetProp (vcd_node, "version", "2.0");
      break;
    case VCD_TYPE_VCD11:
      xmlSetProp (vcd_node, "class", "vcd");
      xmlSetProp (vcd_node, "version", "1.1");
      break;
    default:
      assert (0);
      break;
    }


  /* options */

  _VCD_LIST_FOREACH (node, obj->option_list)
    {
      struct option_t *_option = _vcd_list_node_data (node);
      
      section = xmlNewChild (vcd_node, ns, "option", NULL);
      xmlSetProp (section, "name", _option->name);
      xmlSetProp (section, "value", _option->value);
    }

  /* INFO */

  section = xmlNewChild (vcd_node, ns, "info", NULL);

  xmlNewChild (section, ns, "album-id", obj->info.album_id);
  
  snprintf (buf, sizeof (buf), "%d", obj->info.volume_count);
  xmlNewChild (section, ns, "volume-count", buf);

  snprintf (buf, sizeof (buf), "%d", obj->info.volume_number);
  xmlNewChild (section, ns, "volume-number", buf);

  if (obj->info.use_sequence2)
    xmlNewChild (section, ns, "next-volume-use-sequence2", NULL);

  if (obj->info.use_lid2)
    xmlNewChild (section, ns, "next-volume-use-lid2", NULL);

  snprintf (buf, sizeof (buf), "%d", obj->info.restriction);
  xmlNewChild (section, ns, "restriction", buf);

  /* PVD */

  section = xmlNewChild (vcd_node, ns, "pvd", NULL);

  xmlNewChild (section, ns, "volume-id", obj->pvd.volume_id);
  xmlNewChild (section, ns, "system-id", obj->pvd.system_id);
  xmlNewChild (section, ns, "application-id", obj->pvd.application_id);
  xmlNewChild (section, ns, "preparer-id", obj->pvd.preparer_id);
  xmlNewChild (section, ns, "publisher-id", obj->pvd.publisher_id);

  /* segments */

  if (_vcd_list_length (obj->segment_list))
    {
      section = xmlNewChild (vcd_node, ns, "segment-items", NULL);

      _VCD_LIST_FOREACH (node, obj->segment_list)
	{
	  struct segment_t *_segment =  _vcd_list_node_data (node);
	  xmlNodePtr seg_node;
	  
	  seg_node = xmlNewChild (section, ns, "segment-item", NULL);
	  xmlSetProp (seg_node, "src", _segment->src);
	  xmlSetProp (seg_node, "id", _segment->id);
	}
    }

  /* filesystem */

  if (_vcd_list_length (obj->filesystem))
    {
      section = xmlNewChild (vcd_node, ns, "filesystem", NULL);

      _VCD_LIST_FOREACH (node, obj->filesystem)
	{
	  struct filesystem_t *p = _vcd_list_node_data (node);
	  xmlNodePtr filenode;

	  filenode = _get_file_node (doc, section, ns, p->name);

	  xmlSetProp (filenode, "src", p->file_src);
	  if (p->file_raw)
	    xmlSetProp (filenode, "format", "mixed");
	}
    }

  /* sequences */
    
  section = xmlNewChild (vcd_node, ns, "sequence-items", NULL);

  _VCD_LIST_FOREACH (node, obj->sequence_list)
    {
      struct sequence_t *_sequence =  _vcd_list_node_data (node);
      xmlNodePtr seq_node;
      VcdListNode *node2;

      seq_node = xmlNewChild (section, ns, "sequence-item", NULL);
      xmlSetProp (seq_node, "src", _sequence->src);
      xmlSetProp (seq_node, "id", _sequence->id);

      _VCD_LIST_FOREACH (node2, _sequence->entry_point_list)
	{
	  struct entry_point_t *_entry = _vcd_list_node_data (node2);
	  xmlNodePtr ent_node;
	  char buf[80];

	  snprintf (buf, sizeof (buf), "%f", _entry->timestamp);
	  ent_node = xmlNewChild (seq_node, ns, "entry", buf);
	  xmlSetProp (ent_node, "id", _entry->id);
	}
    }

  /* PBC */

  if (_vcd_list_length (obj->pbc_list))
    {
      section = xmlNewChild (vcd_node, ns, "pbc", NULL);

      _VCD_LIST_FOREACH (node, obj->pbc_list)
	{
	  pbc_t *_pbc = _vcd_list_node_data (node);
	  xmlNodePtr pl = NULL;
	  
	  switch (_pbc->type)
	    {
	      char buf[80];
	      VcdListNode *node2;

	    case PBC_PLAYLIST:
	      pl = xmlNewChild (section, ns, "playlist", NULL);
	      if (_pbc->prev_id)
		xmlSetProp (xmlNewChild (pl, ns, "prev", NULL), "ref", _pbc->prev_id);
	      if (_pbc->next_id)
		xmlSetProp (xmlNewChild (pl, ns, "next", NULL), "ref", _pbc->next_id);
	      if (_pbc->retn_id)
		xmlSetProp (xmlNewChild (pl, ns, "return", NULL), "ref", _pbc->retn_id);

	      if (_pbc->playing_time)
		{
		  snprintf (buf, sizeof (buf), "%f", _pbc->playing_time);
		  xmlNewChild (pl, ns, "playtime", buf);
		}

	      snprintf (buf, sizeof (buf), "%d", _pbc->wait_time);
	      xmlNewChild (pl, ns, "wait", buf);

	      snprintf (buf, sizeof (buf), "%d", _pbc->auto_pause_time);
	      xmlNewChild (pl, ns, "autowait", buf);

	      _VCD_LIST_FOREACH (node2, _pbc->item_id_list)
		{
		  char *_id = _vcd_list_node_data (node2);
		  
		  xmlSetProp (xmlNewChild (pl, ns, "play-item", NULL), "ref", _id);
		}

	      break;

	    case PBC_SELECTION:
	      pl = xmlNewChild (section, ns, "selection", NULL);

	      snprintf (buf, sizeof (buf), "%d", _pbc->bsn);
	      xmlNewChild (pl, ns, "bsn", buf);

	      if (_pbc->prev_id)
		xmlSetProp (xmlNewChild (pl, ns, "prev", NULL), "ref", _pbc->prev_id);
	      if (_pbc->next_id)
		xmlSetProp (xmlNewChild (pl, ns, "next", NULL), "ref", _pbc->next_id);
	      if (_pbc->retn_id)
		xmlSetProp (xmlNewChild (pl, ns, "return", NULL), "ref", _pbc->retn_id);

	      _VCD_LIST_FOREACH (node2, _pbc->default_id_list)
		{
		  char *_id = _vcd_list_node_data (node2);

		  xmlSetProp (xmlNewChild (pl, ns, "default", NULL), "ref", _id);
		}

	      if (_pbc->timeout_id)
		xmlSetProp (xmlNewChild (pl, ns, "timeout", NULL), "ref", _pbc->timeout_id);

	      snprintf (buf, sizeof (buf), "%d", _pbc->timeout_time);
	      xmlNewChild (pl, ns, "wait", buf);

	      snprintf (buf, sizeof (buf), "%d", _pbc->loop_count);
	      xmlSetProp (xmlNewChild (pl, ns, "loop", buf), "jump-timing", 
			  (_pbc->jump_delayed ? "delayed" : "immediate"));

	      if (_pbc->item_id)
		xmlSetProp (xmlNewChild (pl, ns, "play-item", NULL), "ref", _pbc->item_id);

	      _VCD_LIST_FOREACH (node2, _pbc->select_id_list)
		{
		  char *_id = _vcd_list_node_data (node2);

		  xmlSetProp (xmlNewChild (pl, ns, "select", NULL), "ref", _id);
		}
	      break;
	      
	    case PBC_END:
	      pl = xmlNewChild (section, ns, "endlist", NULL);
	      break;

	    default:
	      assert (0);
	    }

	  xmlSetProp (pl, "id", _pbc->id);
	}
    }

  xmlSaveFormatFile (xml_fname, doc, true);

  xmlFreeDoc (doc);
}

int
vcd_xml_dump (struct vcdxml_t *obj, const char xml_fname[])
{
  _make_xml (obj, xml_fname);
  
  return 0;
}

char *
vcd_xml_dump_cl_comment (int argc, const char *argv[])
{
  int idx;
  char *retval;
  size_t len = 0;

  retval = strdup (" commandline used: ");

  len = strlen (retval);

  for (idx = 0; idx < argc; idx++)
    {
      len += strlen (argv[idx]) + 1;
      
      retval = realloc (retval, len + 1);

      strcat (retval, argv[idx]);
      strcat (retval, " ");
    }

  /* scramble hyphen's */
  for (idx = 0; retval[idx]; idx++)
    if (!strncmp (retval + idx, "--", 2))
      retval[idx + 1] = '=';

  return retval;
}

