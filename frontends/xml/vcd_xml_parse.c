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

#include "vcd_xml_parse.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

/*
 * shortcut templates...
 */

#define FOR_EACH(iter, parent) for(iter = parent->xmlChildrenNode; iter != NULL; iter = iter->next)

typedef struct {
  xmlChar *name; /* node name */
} handlers_t;

static bool
_generic_do_children (void)
{
  /* todo */
  return false;
}

#define GET_ELEM_(id, doc, node, ns) \
 if ((!xmlStrcmp (node->name, (const xmlChar *) id)) && (node->ns == ns)) 

#define GET_ELEM_STR(str, id, doc, node, ns) \
 if ((!xmlStrcmp (node->name, (const xmlChar *) id)) && (node->ns == ns)) \
   str = xmlNodeListGetString (doc, node->xmlChildrenNode, 1)

#define GET_ELEM_LONG(val, id, doc, node, ns) \
 if ((!xmlStrcmp (node->name, (const xmlChar *) id)) && (node->ns == ns)) \
   val = _get_elem_long (id, doc, node, ns)

#define GET_ELEM_DOUBLE(val, id, doc, node, ns) \
 if ((!xmlStrcmp (node->name, (const xmlChar *) id)) && (node->ns == ns)) \
   val = _get_elem_double (id, doc, node, ns)

#define GET_PROP_STR(str, id, doc, node, ns) \
 if (xmlHasProp (node, id)) \
   str = xmlGetProp (node, id)

#define GET_ELSE else

static long
_get_elem_long (const char id[], xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  long retval = 0;
  xmlChar *_tmp = NULL;
  char *endptr;
  
  GET_ELEM_STR (_tmp, id, doc, node, ns);

  if (!_tmp)
    {
      printf ("empty element??\n");
      return retval;
    }

  retval = strtol (_tmp, &endptr, 10);

  if (*endptr)
    printf ("error while converting string to integer?\n");

  return retval;
}

static double
_get_elem_double (const char id[], xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  double retval = 0;
  xmlChar *_tmp = NULL;
  char *endptr;
  
  GET_ELEM_STR (_tmp, id, doc, node, ns);

  if (!_tmp)
    {
      printf ("empty element??\n");
      return retval;
    }

  retval = strtod (_tmp, &endptr);

  if (*endptr)
    printf ("error while converting string to floating point?\n");

  return retval;
}


/*
 * options 
 */

static bool
_parse_option (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  printf ("sorry, option element not supported yet\n");
  return false;
}

/*
 * info block
 */

static bool
_parse_info (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  FOR_EACH (cur, node)
    {
      GET_ELEM_STR (obj->info.album_id, "album-id", doc, cur, ns);
      GET_ELSE GET_ELEM_LONG (obj->info.volume_count, "volume-count", doc, cur, ns);
      GET_ELSE GET_ELEM_LONG (obj->info.volume_number, "volume-number", doc, cur, ns);
      GET_ELSE GET_ELEM_LONG (obj->info.restriction, "restriction", doc, cur, ns);
      GET_ELSE GET_ELEM_DOUBLE (obj->info.time_offset, "time-offset", doc, cur, ns);
      GET_ELSE GET_ELEM_ ("next-volume-use-sequence2", doc, cur, ns)
	obj->info.use_sequence2 = true;
      GET_ELSE GET_ELEM_ ("next-volume-use-lid2", doc, cur, ns)
	obj->info.use_lid2 = true;
      GET_ELSE assert (0);
    }

  return false;
}

/*
 * iso9660 pvd block
 */

static bool
_parse_pvd (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  FOR_EACH (cur, node)
    {
      GET_ELEM_STR (obj->pvd.volume_id, "volume-id", doc, cur, ns);
      GET_ELSE GET_ELEM_STR (obj->pvd.system_id, "system-id", doc, cur, ns);
      GET_ELSE GET_ELEM_STR (obj->pvd.application_id, "application-id", doc, cur, ns);
      GET_ELSE GET_ELEM_STR (obj->pvd.preparer_id, "preparer-id", doc, cur, ns);
      GET_ELSE GET_ELEM_STR (obj->pvd.publisher_id, "publisher-id", doc, cur, ns);
      GET_ELSE assert (0);
    }

  return false;
}

/*
 * segment items block 
 */

static bool
_parse_segments (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  FOR_EACH (cur, node)
    {
      bool rc = false;

      if (!xmlStrcmp (cur->name, "segment-item")) 
	{
	  struct segment_t *_item = malloc (sizeof (struct segment_t));
	  memset (_item, 0, sizeof (sizeof (struct segment_t)));
	  
	  GET_PROP_STR (_item->id, "id", doc, cur, ns);
	  GET_PROP_STR (_item->src, "src", doc, cur, ns);

	  _vcd_list_append (obj->segment_list, _item);
	}
      else
	assert (0);

      if (rc)
	return rc;
      
    }  

  return false;
}

/*
 * PBC block 
 */

static bool
_parse_pbc_selection (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;
  pbc_t *_pbc;

  _pbc = vcd_pbc_new (PBC_SELECTION);

  GET_PROP_STR (_pbc->id, "id", doc, node, ns);

  FOR_EACH (cur, node)
    {
      if (cur->ns != ns) 
	continue; 
      
      if (!xmlStrcmp (cur->name, "prev"))
	{ GET_PROP_STR (_pbc->prev_id, "ref", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "next"))
	{ GET_PROP_STR (_pbc->next_id, "ref", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "return"))
	{ GET_PROP_STR (_pbc->retn_id, "ref", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "timeout"))
	{ GET_PROP_STR (_pbc->timeout_id, "ref", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "bsn"))
	{ _pbc->bsn = _get_elem_long ("bsn", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "loop"))
	{ 
	  xmlChar *_tmp = NULL;
	  
	  _pbc->loop_count = _get_elem_long ("loop", doc, cur, ns); 

	  GET_PROP_STR (_tmp, "jump-timing", doc, cur, ns);
	  
	  if (_tmp && !xmlStrcmp (_tmp, "delayed"))
	    _pbc->jump_delayed = true;
	}
      else if (!xmlStrcmp (cur->name, "wait"))
	{ _pbc->timeout_time = _get_elem_long ("wait", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "play-item"))
	{ GET_PROP_STR (_pbc->item_id, "ref", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "default"))
	{
	  xmlChar *_default_ref = NULL;

	  GET_PROP_STR (_default_ref, "ref", doc, cur, ns); 
	  assert (_default_ref != NULL);
	  _vcd_list_append (_pbc->default_id_list, _default_ref);
	}
      else if (!xmlStrcmp (cur->name, "select"))
	{
	  xmlChar *_select_ref = NULL;

	  GET_PROP_STR (_select_ref, "ref", doc, cur, ns); 
	  assert (_select_ref != NULL);
	  _vcd_list_append (_pbc->select_id_list, _select_ref);
	}
      else
	assert (0);
    }

  _vcd_list_append (obj->pbc_list, _pbc);

  return false;
}

static bool
_parse_pbc_playlist (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;
  pbc_t *_pbc;

  _pbc = vcd_pbc_new (PBC_PLAYLIST);

  GET_PROP_STR (_pbc->id, "id", doc, node, ns);

  FOR_EACH (cur, node)
    {
      if (cur->ns != ns) 
	continue; 
      
      if (!xmlStrcmp (cur->name, "prev"))
	{ GET_PROP_STR (_pbc->prev_id, "ref", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "next"))
	{ GET_PROP_STR (_pbc->next_id, "ref", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "return"))
	{ GET_PROP_STR (_pbc->retn_id, "ref", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "wait"))
	{ _pbc->wait_time = _get_elem_long ("wait", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "autowait"))
	{ _pbc->auto_pause_time = _get_elem_long ("autowait", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "playtime"))
	{ _pbc->playing_time = _get_elem_double ("playtime", doc, cur, ns); }
      else if (!xmlStrcmp (cur->name, "play-item"))
	{
	  xmlChar *_item_ref = NULL;

	  GET_PROP_STR (_item_ref, "ref", doc, cur, ns); 
	  assert (_item_ref != NULL);
	  _vcd_list_append (_pbc->item_id_list, _item_ref);
	}
      else
	assert (0);
    }

  _vcd_list_append (obj->pbc_list, _pbc);

  return false;
}

static bool
_parse_pbc_endlist (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;
  pbc_t *_pbc;

  _pbc = vcd_pbc_new (PBC_END);

  GET_PROP_STR (_pbc->id, "id", doc, node, ns);

  FOR_EACH (cur, node)
    {
      if (cur->ns != ns) 
	continue; 
      
      printf ("%s %s -- sorry, not fully implemented yet\n", __PRETTY_FUNCTION__, cur->name);
      assert (0);
    }

  _vcd_list_append (obj->pbc_list, _pbc);

  return false;
}

static bool
_parse_pbc (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  FOR_EACH (cur, node)
    {
      bool rc = true;

      if (cur->ns != ns) 
	continue; 

      if (!xmlStrcmp (cur->name, "selection")) 
	rc = _parse_pbc_selection (obj, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "playlist")) 
	rc = _parse_pbc_playlist (obj, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "endlist")) 
	rc = _parse_pbc_endlist (obj, doc, cur, ns);
      else 
	assert (0);

      if (rc)
	return rc;
    }

  return false;
}

/*
 * sequence items block
 */

static bool
_parse_mpeg_sequence (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  struct sequence_t *sequence;
  xmlNodePtr cur;

  sequence = malloc (sizeof (struct sequence_t));
  memset (sequence, 0, sizeof (struct sequence_t));

  _vcd_list_append (obj->sequence_list, sequence);

  GET_PROP_STR (sequence->id, "id", doc, node, ns);
  GET_PROP_STR (sequence->src, "src", doc, node, ns);

  sequence->entry_point_list = _vcd_list_new ();

  FOR_EACH (cur, node)
    if (!xmlStrcmp (cur->name, "entry"))
      {
	struct entry_point_t *entry = malloc (sizeof (struct entry_point_t));
	memset (entry, 0, sizeof (struct entry_point_t));

	GET_PROP_STR (entry->id, "id", doc, cur, ns);
	GET_ELEM_DOUBLE (entry->timestamp, "entry", doc, cur, ns);

	_vcd_list_append (sequence->entry_point_list, entry);
      }
    else
      assert (0);

  return false;
}

static bool
_parse_sequences (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  FOR_EACH (cur, node)
    {
      bool rc = false;

      if (!xmlStrcmp (cur->name, "sequence-item")) 
	rc = _parse_mpeg_sequence (obj, doc, cur, ns);
      else
	assert (0);

      if (rc)
	return rc;
    }

  return false;
}

/*
 * filesystem block 
 */

static bool
_parse_file (struct vcdxml_t *obj, const char path[], xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;
  xmlChar *_name = NULL;
  xmlChar *_src = NULL;
  xmlChar *_format = NULL;

  assert (path != NULL);

  GET_PROP_STR (_src, "src", doc, node, ns);
  assert (_src != NULL);

  GET_PROP_STR (_format, "format", doc, node, ns);

  FOR_EACH (cur, node)
    {
      GET_ELEM_STR (_name, "name", doc, cur, ns);
      GET_ELSE assert (0);
    }

  if (!_name)
    return true;

  {
    struct filesystem_t *_data;
    char *_tmp;

    _tmp = malloc (strlen (path) + strlen (_name) + 1);
    
    strcpy (_tmp, path);
    strcat (_tmp, _name);
	    
    _data = malloc (sizeof (struct filesystem_t));
    _data->name = _tmp;
    _data->file_src = strdup (_src);
    _data->file_raw = (_format && !xmlStrcmp (_format, "mixed"));

    _vcd_list_append (obj->filesystem, _data);
  }

  return false;
}

static bool
_parse_folder (struct vcdxml_t *obj, const char path[], xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;
  char *new_path = NULL;

  assert (path != NULL);

  FOR_EACH (cur, node)
    {
      bool rc = true;

      if (cur->ns != ns) 
	continue; 

      if (!xmlStrcmp (cur->name, "name")) 
	{
	  xmlChar *_tmp;

	  assert (new_path == NULL);

	  _tmp = xmlNodeListGetString (doc, cur->xmlChildrenNode, 1);

	  assert (_tmp != NULL);

	  new_path = malloc (strlen (path) + strlen (_tmp) + 1 + 1);
	  strcpy (new_path, path);
	  strcat (new_path, _tmp);

	  {
	    struct filesystem_t *_data;
	    
	    _data = malloc (sizeof (struct filesystem_t));
	    _data->name = strdup (new_path);
	    _data->file_src = NULL;

	    _vcd_list_append (obj->filesystem, _data);
	  }
	  

	  strcat (new_path, "/");
    
	  rc = false;
	  
	  /* fixme, free _tmp?? */
	}
      else if (!xmlStrcmp (cur->name, "folder")) 
	rc = _parse_folder (obj, new_path, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "file"))
	rc = _parse_file (obj, new_path, doc, cur, ns);
      else 
	assert (0);
      
      if (new_path == NULL)
	rc = true;

      if (rc)
	return rc;
    }

  return false;
}

static bool
_parse_filesystem (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  FOR_EACH (cur, node)
    {
      bool rc = true;

      if (cur->ns != ns) 
	continue; 

      if (!xmlStrcmp (cur->name, "folder")) 
	rc = _parse_folder (obj, "", doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "file"))
	rc = _parse_file (obj, "", doc, cur, ns);
      else 
	assert (0);

      if (rc)
	return rc;
    }

  return false;
}

/*
 * top videocd block
 */

static bool
_parse_videocd (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  assert (obj != NULL);

  GET_PROP_STR (obj->class, "class", doc, node, ns);
  GET_PROP_STR (obj->version, "version", doc, node, ns);

  obj->segment_list = _vcd_list_new ();
  obj->pbc_list = _vcd_list_new ();
  obj->sequence_list = _vcd_list_new ();
  obj->filesystem = _vcd_list_new ();
  obj->option_list = _vcd_list_new ();

  FOR_EACH (cur, node)
    {
      bool rc = false;

      if (cur->ns != ns)
	{
	  printf ("foreign namespace ignored (%s)\n", cur->name);
	  continue;
	}

      if (!xmlStrcmp (cur->name, "option")) 
	rc = _parse_option (obj, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "info")) 
	rc = _parse_info (obj, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "pvd")) 
	rc = _parse_pvd (obj, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "pbc")) 
	rc = _parse_pbc (obj, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "segment-items")) 
	rc = _parse_segments (obj, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "filesystem")) 
	rc = _parse_filesystem (obj, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "sequence-items")) 
	rc = _parse_sequences (obj, doc, cur, ns);
      else printf ("unexpected element: %s\n", cur->name);

      if (rc)
	return rc;
    }

  return false;
}




/* exported entry point
 */

bool
vcd_xml_parse (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  assert (obj != NULL);
  assert (node != NULL);
  assert (doc != NULL);

  if (xmlStrcmp (node->name, "videocd") || (node->ns != ns))
    {
      printf ("root element not videocd...\n");
      return true;
    }

  memset (obj, 0, sizeof (struct vcdxml_t));

  return _parse_videocd (obj, doc, node, ns);
}

