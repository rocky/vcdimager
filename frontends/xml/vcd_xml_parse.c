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

#define GET_ELEM_STR(str, id, doc, node, ns) \
 if ((!xmlStrcmp (node->name, (const xmlChar *) id)) && (node->ns == ns)) \
   str = xmlNodeListGetString (doc, node->xmlChildrenNode, 1)

#define GET_PROP_STR(str, id, doc, node, ns) \
 if (xmlHasProp (node, id)) \
   str = xmlGetProp (node, id)

#define GET_ELSE else

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
      GET_ELSE GET_ELEM_STR (obj->info.volume_count, "volume-count", doc, cur, ns);
      GET_ELSE GET_ELEM_STR (obj->info.volume_number, "volume-number", doc, cur, ns);
      GET_ELSE GET_ELEM_STR (obj->info.restriction, "restriction", doc, cur, ns);
      GET_ELSE printf ("??? %s\n", cur->name); 
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
      GET_ELSE printf ("??? %s\n", cur->name); 
    }

  return false;
}

/*
 * segment items block 
 */

static bool
_parse_items (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  printf ("sorry, segment items not supported yet\n");
  return false;
}

/*
 * PBC block 
 */

static bool
_parse_pbc (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  printf ("sorry, PBC not supported yet\n");
  return false;
}

/*
 * sequence items block
 */

static bool
_parse_cdda_track (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  printf ("sorry, CDDA tracks not supported yet\n");
  return true;
}

static bool
_parse_mpeg_track (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  struct mpeg_tracks_t *track;
  xmlNodePtr cur;

  if (obj->mpeg_tracks_count >= 98) /* fixme -- check should be done in backend */
    {
      printf ("too many tracks\n");
      return true;
    }

  obj->mpeg_tracks_count++;
  obj->mpeg_tracks = realloc (obj->mpeg_tracks,
				 obj->mpeg_tracks_count 
				 * sizeof (struct mpeg_tracks_t));

  track = &obj->mpeg_tracks[obj->mpeg_tracks_count - 1];
  memset (track, 0, sizeof (struct mpeg_tracks_t));

  GET_PROP_STR (track->id, "id", doc, node, ns);
  GET_PROP_STR (track->src, "src", doc, node, ns);

  FOR_EACH (cur, node)
    if (!xmlStrcmp (cur->name, "entry"))
      {
	struct entry_points_t *entry;
	
	track->entry_points_count++;

	track->entry_points = realloc (track->entry_points,
				       track->entry_points_count
				       * sizeof (struct entry_points_t));

	entry = &track->entry_points[track->entry_points_count - 1];
	memset (entry, 0, sizeof (struct entry_points_t));

	GET_PROP_STR (entry->id, "id", doc, cur, ns);
	GET_ELEM_STR (entry->timestamp, "entry", doc, cur, ns);
      }
    else
      printf ("??? %s\n", cur->name);

  return false;
}

static bool
_parse_tracks (struct vcdxml_t *obj, xmlDocPtr doc, xmlNodePtr node, xmlNsPtr ns)
{
  xmlNodePtr cur;

  FOR_EACH (cur, node)
    {
      bool rc = false;

      if (!xmlStrcmp (cur->name, "mpeg-track")) 
	rc = _parse_mpeg_track (obj, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "cdda-track")) 
	rc = _parse_cdda_track (obj, doc, cur, ns);
      else
	{
	  printf ("unexpected element: %s\n", cur->name);
	  rc = false;
	}

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

  printf ("%s\n", __PRETTY_FUNCTION__);
  assert (path != NULL);

  GET_PROP_STR (_src, "src", doc, node, ns);
  assert (_src != NULL);

  GET_PROP_STR (_format, "format", doc, node, ns);

  FOR_EACH (cur, node)
    {
      GET_ELEM_STR (_name, "name", doc, cur, ns);
      GET_ELSE printf ("??? %s\n", cur->name); 
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

  printf ("%s\n", __PRETTY_FUNCTION__);

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
	printf ("?????\n");

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

  printf ("%s\n", __PRETTY_FUNCTION__);

  obj->filesystem = _vcd_list_new ();

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
	printf ("?????\n");

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

  printf ("%s\n", __PRETTY_FUNCTION__);

  GET_PROP_STR (obj->class, "class", doc, node, ns);
  GET_PROP_STR (obj->version, "version", doc, node, ns);

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
      else if (!xmlStrcmp (cur->name, "items")) 
	rc = _parse_items (obj, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "filesystem")) 
	rc = _parse_filesystem (obj, doc, cur, ns);
      else if (!xmlStrcmp (cur->name, "tracks")) 
	rc = _parse_tracks (obj, doc, cur, ns);
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

