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
_get_node (xmlDocPtr doc, xmlNodePtr cur, xmlNsPtr ns, 
	   const char nodename[], bool folder)
{
  xmlNodePtr n = NULL;
  const xmlChar *_node_id = folder ? "folder" : "file";

  FOR_EACH (n, cur)
    {
      char *tmp;

      if (xmlStrcmp (n->name, _node_id))
	continue;

      vcd_assert (!xmlStrcmp (n->children->name, "name"));
      
      tmp = xmlNodeListGetString (doc, n->children->children, 1);
      
      if (!xmlStrcmp (tmp, nodename))
	break;
    }
  
  if (!n)
    {
      n = xmlNewNode (ns, _node_id);
      xmlNewChild (n, ns, "name", nodename);

      if (!folder || !cur->xmlChildrenNode) /* file or first entry */
	xmlAddChild (cur, n);
      else /* folder */
	{
	  if (!xmlStrcmp (cur->children->name, "name"))
	    xmlAddNextSibling (cur->xmlChildrenNode, n);
	  else
	    {
	      vcd_assert (!xmlStrcmp (cur->name, "filesystem"));
	      xmlAddPrevSibling (cur->xmlChildrenNode, n); /* special case for <filesystem> */
	    }
	}
    }

  return n;
}

static xmlNodePtr 
_get_node_pathname (xmlDocPtr doc, xmlNodePtr cur, xmlNsPtr ns, const char pathname[], bool folder)
{
  char *_dir, *c;
  xmlNodePtr retval = NULL;

  vcd_assert (pathname != NULL);

  if (pathname[0] == '/')
    return _get_node_pathname (doc, cur, ns, pathname + 1, folder);

  if (pathname[0] == '\0')
    return retval;

  _dir = _vcd_strdup_upper (pathname);
  c = strchr (_dir, '/');

  if (c)
    { /* subdir... */
      xmlNodePtr n;

      *c++ = '\0';

      n = _get_node (doc, cur, ns, _dir, true);

      retval = _get_node_pathname (doc, n, ns, c, folder);
    }
  else /* leaf */
    retval = _get_node (doc, cur, ns, _dir, folder);

  free (_dir);

  return retval;
}

static void
_ref_area_helper (xmlNodePtr cur, xmlNsPtr ns, const char tag_id[], const char pbc_id[], const pbc_area_t *_area)
{
  xmlNodePtr node;

  if (!pbc_id)
    return;
  
  node = xmlNewChild (cur, ns, tag_id, NULL);
  
  xmlSetProp (node, "ref", pbc_id);

  if (_area)
    {
      char buf[16];

      snprintf (buf, sizeof (buf), "%d", _area->x1);
      xmlSetProp (node, "x1", buf);

      snprintf (buf, sizeof (buf), "%d", _area->y1);
      xmlSetProp (node, "y1", buf);

      snprintf (buf, sizeof (buf), "%d", _area->x2);
      xmlSetProp (node, "x2", buf);

      snprintf (buf, sizeof (buf), "%d", _area->y2);
      xmlSetProp (node, "y2", buf);
    }
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
    case VCD_TYPE_VCD:
      xmlSetProp (vcd_node, "class", "vcd");
      xmlSetProp (vcd_node, "version", "1.0");
      break;

    case VCD_TYPE_VCD11:
      xmlSetProp (vcd_node, "class", "vcd");
      xmlSetProp (vcd_node, "version", "1.1");
      break;

    case VCD_TYPE_VCD2:
      xmlSetProp (vcd_node, "class", "vcd");
      xmlSetProp (vcd_node, "version", "2.0");
      break;

    case VCD_TYPE_SVCD:
      xmlSetProp (vcd_node, "class", "svcd");
      xmlSetProp (vcd_node, "version", "1.0");
      break;

    case VCD_TYPE_HQVCD:
      xmlSetProp (vcd_node, "class", "hqvcd");
      xmlSetProp (vcd_node, "version", "1.0");
      break;

    default:
      vcd_assert_not_reached ();
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

  /* filesystem */

  if (_vcd_list_length (obj->filesystem))
    {
      section = xmlNewChild (vcd_node, ns, "filesystem", NULL);

      _VCD_LIST_FOREACH (node, obj->filesystem)
	{
	  struct filesystem_t *p = _vcd_list_node_data (node);
	  
	  if (p->file_src)
	    { /* file */
	      xmlNodePtr filenode = _get_node_pathname (doc, section, ns, p->name, false);

	      xmlSetProp (filenode, "src", p->file_src);

	      if (p->file_raw)
		xmlSetProp (filenode, "format", "mixed");
	    }
	  else /* folder */
	    _get_node_pathname (doc, section, ns, p->name, true);
	}
    }

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

      if (_sequence->default_entry_id)
	{
	  xmlNodePtr ent_node;

	  ent_node = xmlNewChild (seq_node, ns, "default-entry", NULL);
	  xmlSetProp (ent_node, "id", _sequence->default_entry_id);
	}

      _VCD_LIST_FOREACH (node2, _sequence->entry_point_list)
	{
	  struct entry_point_t *_entry = _vcd_list_node_data (node2);
	  xmlNodePtr ent_node;
	  char buf[80];

	  snprintf (buf, sizeof (buf), "%f", _entry->timestamp);
	  ent_node = xmlNewChild (seq_node, ns, "entry", buf);
	  xmlSetProp (ent_node, "id", _entry->id);
	}

      _VCD_LIST_FOREACH (node2, _sequence->autopause_list)
	{
	  double *_ap_ts = _vcd_list_node_data (node2);
	  char buf[80];

	  snprintf (buf, sizeof (buf), "%f", *_ap_ts);
	  xmlNewChild (seq_node, ns, "auto-pause", buf);
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

	      _ref_area_helper (pl, ns, "prev", _pbc->prev_id, _pbc->prev_area);
	      _ref_area_helper (pl, ns, "next", _pbc->next_id, _pbc->next_area);
	      _ref_area_helper (pl, ns, "return", _pbc->retn_id, _pbc->return_area);

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
		  const char *_id = _vcd_list_node_data (node2);
		  
		  if (_id)
		    xmlSetProp (xmlNewChild (pl, ns, "play-item", NULL), "ref", _id);
		  else
		    xmlNewChild (pl, ns, "play-item", NULL);
		}

	      break;

	    case PBC_SELECTION:
	      pl = xmlNewChild (section, ns, "selection", NULL);

	      snprintf (buf, sizeof (buf), "%d", _pbc->bsn);
	      xmlNewChild (pl, ns, "bsn", buf);

	      _ref_area_helper (pl, ns, "prev", _pbc->prev_id, _pbc->prev_area);
	      _ref_area_helper (pl, ns, "next", _pbc->next_id, _pbc->next_area);
	      _ref_area_helper (pl, ns, "return", _pbc->retn_id, _pbc->return_area);
	      switch (_pbc->selection_type)
		{
		case _SEL_NORMAL:
		  _ref_area_helper (pl, ns, "default",
				    _pbc->default_id, _pbc->default_area);
		  break;

		case _SEL_MULTI_DEF:
		  xmlSetProp (xmlNewChild (pl, ns, "multi-default", NULL), 
			      "numeric", "enabled");
		  break;

		case _SEL_MULTI_DEF_NO_NUM:
		  xmlSetProp (xmlNewChild (pl, ns, "multi-default", NULL), 
			      "numeric", "disabled");
		  break;
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

	      {
		VcdListNode *node3 = _vcd_list_begin (_pbc->select_area_list);

		_VCD_LIST_FOREACH (node2, _pbc->select_id_list)
		  {
		    char *_id = _vcd_list_node_data (node2);
		    pbc_area_t *_area = node3 ? _vcd_list_node_data (node3) : NULL;

		    if (_id)
		      _ref_area_helper (pl, ns, "select", _id, _area);
		    else
		      xmlNewChild (pl, ns, "select", NULL);

		    if (_vcd_list_length (_pbc->select_area_list))
		      node3 = _vcd_list_node_next (node3);
		  }
	      }
	      break;
	      
	    case PBC_END:
	      pl = xmlNewChild (section, ns, "endlist", NULL);

	      if (_pbc->next_disc)
		{
		  snprintf (buf, sizeof (buf), "%d", _pbc->next_disc);
		  xmlNewChild (pl, ns, "next-volume", buf);
		}

	      if (_pbc->image_id)
		xmlSetProp (xmlNewChild (pl, ns, "play-item", NULL),
			    "ref", _pbc->image_id);
	      break;

	    default:
	      vcd_assert_not_reached ();
	    }
	  
	  xmlSetProp (pl, "id", _pbc->id);
	  if (_pbc->rejected)
	    xmlSetProp (pl, "rejected", "true");
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

