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

#include <sys/types.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libvcd/vcd.h>
#include <libvcd/vcd_types.h>
#include <libvcd/vcd_data_structures.h>
#include <libvcd/vcd_cd_sector.h>

#include "videocd_dtd.h"

static const char _rcsid[] = "$Id$";

/* defaults */
#define DEFAULT_VOLUME_ID      "VideoCD"
#define DEFAULT_APPLICATION_ID ""
#define DEFAULT_ALBUM_ID       ""
#define DEFAULT_TYPE           "vcd2"
#define DEFAULT_XML_FNAME      "videocd.xml"

/* global stuff kept as a singleton makes for less typing effort :-) 
 */
static struct
{
  const char *type;
  vcd_type_t type_id;

  VcdList *file_list;
  VcdList *tracks_list;

  const char *xml_fname;

  const char *volume_label;
  const char *application_id;
  const char *album_id;

  int volume_number;
  int volume_count;

  int broken_svcd_mode_flag;

  int verbose_flag;
  int quiet_flag;

  int nopbc_flag;
}
gl;                             /* global */

struct add_files_t {
  char *fname;
  char *iso_fname;
  bool raw_flag;
};

static void
gl_add_file (char *fname, char *iso_fname, int raw_flag)
{
  struct add_files_t *tmp = malloc (sizeof (struct add_files_t));

  _vcd_list_append (gl.file_list, tmp);

  tmp->fname = fname;
  tmp->iso_fname = iso_fname;
  tmp->raw_flag = raw_flag;
}

/****************************************************************************/

static vcd_type_t
_parse_type_arg (const char arg[])
{
  struct {
    const char *str;
    vcd_type_t id;
  } type_str[] = 
    {
      { "vcd11", VCD_TYPE_VCD11 },
      { "vcd2", VCD_TYPE_VCD2 },
      { "svcd", VCD_TYPE_SVCD },
      { NULL, VCD_TYPE_INVALID }
    };
      
  int i = 0;

  while (type_str[i].str) 
    if (strcasecmp(gl.type, type_str[i].str))
      i++;
    else
      break;

  if (!type_str[i].str)
    fprintf (stderr, "invalid type given\n");
        
  return type_str[i].id;
}

static int
_parse_file_arg (const char *arg, char **fname1, char **fname2)
{
  int rc = 0;
  char *tmp, *arg_cpy = strdup (arg);

  *fname1 = *fname2 = NULL;

  tmp = strtok(arg_cpy, ",");
  if (tmp)
    *fname1 = strdup (tmp);
  else
    rc = -1;
  
  tmp = strtok(NULL, ",");
  if (tmp)
    *fname2 = strdup (tmp);
  else
    rc = -1;
  
  tmp = strtok(NULL, ",");
  if (tmp)
    rc = -1;

  free (tmp);

  if(rc)
    {
      free (*fname1);
      free (*fname2);

      *fname1 = *fname2 = NULL;
    }

  return rc;
}

static void
_add_dir (const char pathname[], const char iso_pathname[])
{
  DIR *dir = NULL;
  struct dirent *dentry = NULL;

  assert (pathname != NULL);
  assert (iso_pathname != NULL);

  dir = opendir (pathname);

  if (!dir)
    {
      perror ("--add-dir: opendir()");
      exit (EXIT_FAILURE);
    }

  while ((dentry = readdir (dir)))
    {
      char buf[1024] = { 0, };
      char iso_name[1024] = { 0, };
      struct stat st;

      if (!strcmp (dentry->d_name, "."))
        continue;

      if (!strcmp (dentry->d_name, ".."))
        continue;

      strcat (buf, pathname);
      strcat (buf, "/");
      strcat (buf, dentry->d_name);

      strcat (iso_name, dentry->d_name);

      if (stat (buf, &st))
        perror ("stat()");

      if (S_ISDIR(st.st_mode))
        {
          strcat (iso_name, "/");
          _add_dir (buf, iso_name);
        }
      else if (S_ISREG(st.st_mode))
        {
          gl_add_file (strdup (buf), strdup (iso_name), false);
        }
      else
        fprintf (stdout, "ignoring %s\n", buf);
    }

  closedir (dir);
}

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlmemory.h>

#define FOR_EACH(iter, parent) for(iter = parent->xmlChildrenNode; iter != NULL; iter = iter->next)

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

static const char *
_lid_str (int n)
{
  static char buf[16];

  snprintf (buf, sizeof (buf), "lid-%3.3d", n);

  return buf;
}

static const char *
_sequence_str (int n)
{
  static char buf[16];

  snprintf (buf, sizeof (buf), "sequence-%2.2d", n);

  return buf;
}


static void
_make_xml (void)
{
  xmlDtdPtr dtd;
  xmlDocPtr doc;
  xmlNodePtr vcd_node, section;
  xmlNsPtr ns = NULL;
  char buf[1024];
  VcdListNode *node;
  int n;

  xmlKeepBlanksDefault(0);

  doc = xmlNewDoc ("1.0");
  
  dtd = xmlNewDtd (doc, "videocd", VIDEOCD_DTD_PUBID, VIDEOCD_DTD_SYSID);
  xmlAddChild ((xmlNodePtr) doc, (xmlNodePtr) dtd);

  vcd_node = xmlNewDocNode (doc, ns, "videocd", NULL);
  xmlAddChild ((xmlNodePtr) doc, vcd_node);

  ns = xmlNewNs (vcd_node, VIDEOCD_DTD_XMLNS, NULL);
  xmlSetNs (vcd_node, ns);

  switch (gl.type_id) 
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

  if (gl.type_id == VCD_TYPE_SVCD && gl.broken_svcd_mode_flag)
    {
      section = xmlNewChild (vcd_node, ns, "option", NULL);
      xmlSetProp (section, "name", "broken svcd mode");
      xmlSetProp (section, "value", "true");
    }

  /* INFO */

  section = xmlNewChild (vcd_node, ns, "info", NULL);

  xmlNewChild (section, ns, "album-id", gl.album_id);
  
  snprintf (buf, sizeof (buf), "%d", gl.volume_count);
  xmlNewChild (section, ns, "volume-count", buf);

  snprintf (buf, sizeof (buf), "%d", gl.volume_number);
  xmlNewChild (section, ns, "volume-number", buf);

  /* PVD */

  section = xmlNewChild (vcd_node, ns, "pvd", NULL);

  xmlNewChild (section, ns, "volume-id", gl.volume_label);

  xmlNewChild (section, ns, "application-id", gl.application_id);

  /* filesystem */

  section = xmlNewChild (vcd_node, ns, "filesystem", NULL);

  _VCD_LIST_FOREACH (node, gl.file_list)
    {
      struct add_files_t *p = _vcd_list_node_data (node);
      xmlNodePtr filenode;

      filenode = _get_file_node (doc, section, ns, p->iso_fname);

      xmlSetProp (filenode, "src", p->fname);
      if (p->raw_flag)
        xmlSetProp (filenode, "format", "mixed");
    }

  /* sequences */
    
  section = xmlNewChild (vcd_node, ns, "sequence-items", NULL);

  n = 0;
  _VCD_LIST_FOREACH (node, gl.tracks_list)
    {
      char *track_fname =  _vcd_list_node_data (node);
      xmlNodePtr seq_node;

      seq_node = xmlNewChild (section, ns, "sequence-item", NULL);
      xmlSetProp (seq_node, "src", track_fname);

      snprintf (buf, sizeof (buf), _sequence_str (n), n);
      xmlSetProp (seq_node, "id", buf);
        
      n++;
    }

  if (!gl.nopbc_flag) 
    {
      xmlNodePtr pl;

      section = xmlNewChild (vcd_node, ns, "pbc", NULL);

      for (n = 0; n < _vcd_list_length (gl.tracks_list); n++)
        {
          pl = xmlNewChild (section, ns, "playlist", NULL);
          xmlSetProp (pl, "id", _lid_str (n));

          if (n)
            xmlSetProp (xmlNewChild (pl, ns, "prev", NULL), "ref", _lid_str (n - 1));

          if (n + 1 != _vcd_list_length (gl.tracks_list))
            xmlSetProp (xmlNewChild (pl, ns, "next", NULL), "ref", _lid_str (n + 1));
          else
            xmlSetProp (xmlNewChild (pl, ns, "next", NULL), "ref", "lid-end");

          xmlSetProp (xmlNewChild (pl, ns, "return", NULL), "ref", "lid-end");

          xmlNewChild (pl, ns, "wait", "5");

          xmlSetProp (xmlNewChild (pl, ns, "play-item", NULL), "ref", _sequence_str (n));
        }

      pl = xmlNewChild (section, ns, "endlist", NULL);
      xmlSetProp (pl, "id", "lid-end");
    }

  xmlSaveFormatFile (gl.xml_fname, doc, true);

  xmlFreeDoc (doc);
}

int
main (int argc, const char *argv[])
{
  int n = 0;

  gl.xml_fname = DEFAULT_XML_FNAME;

  gl.type = DEFAULT_TYPE;

  gl.file_list = _vcd_list_new ();

  gl.volume_label = DEFAULT_VOLUME_ID;
  gl.application_id = DEFAULT_APPLICATION_ID;
  gl.album_id = DEFAULT_ALBUM_ID;
  
  gl.volume_count = 1;
  gl.volume_number = 1;

  {
    const char **args = NULL;
    int opt = 0;

    enum {
      CL_VERSION = 1,
      CL_ADD_DIR,
      CL_ADD_FILE,
      CL_ADD_FILE_RAW
    };

    struct poptOption optionsTable[] = 
      {
        {"output-file", 'o', POPT_ARG_STRING, &gl.xml_fname, 0,
         "specify xml file for output (default: '" DEFAULT_XML_FNAME "')",
         "FILE"},

        {"type", 't', POPT_ARG_STRING, &gl.type, 0,
         "select VideoCD type ('vcd11', 'vcd2' or 'svcd') (default: '" DEFAULT_TYPE "')", 
         "TYPE"},

        {"iso-volume-label", 'l', POPT_ARG_STRING, &gl.volume_label, 0,
         "specify ISO volume label for video cd (default: '" DEFAULT_VOLUME_ID
         "')", "LABEL"},

        {"iso-application-id", '\0', POPT_ARG_STRING, &gl.application_id, 0,
         "specify ISO application id for video cd (default: '" DEFAULT_APPLICATION_ID
         "')", "LABEL"},

        {"info-album-id", '\0', POPT_ARG_STRING, &gl.album_id, 0,
         "specify album id for video cd set (default: '" DEFAULT_ALBUM_ID
         "')", "LABEL"},

        {"volume-count", '\0', POPT_ARG_INT, &gl.volume_count, 0,
         "specify number of volumes in album set", "NUMBER"},

        {"volume-number", '\0', POPT_ARG_INT, &gl.volume_number, 0,
         "specify album set sequence number (< volume-count)", "NUMBER"},

        {"broken-svcd-mode", '\0', POPT_ARG_NONE, &gl.broken_svcd_mode_flag, 0,
         "enable non-compliant compatibility mode for broken devices"},

        {"nopbc", '\0', POPT_ARG_NONE, &gl.nopbc_flag, 0, "don't create PBC"},
        
        {"add-dir", '\0', POPT_ARG_STRING, NULL, CL_ADD_DIR,
         "add directory contents recursively to ISO fs root", "DIR"},

        {"add-file", '\0', POPT_ARG_STRING, NULL, CL_ADD_FILE, 
         "add single file to ISO fs", "FILE,ISO_FILENAME"},

        {"add-file-2336", '\0', POPT_ARG_STRING, NULL, CL_ADD_FILE_RAW, 
         "add file containing full 2336 byte sectors to ISO fs",
         "FILE,ISO_FILENAME"},

        {"verbose", 'v', POPT_ARG_NONE, &gl.verbose_flag, 0, "be verbose"},

        {"quiet", 'q', POPT_ARG_NONE, &gl.quiet_flag, 0, "show only critical messages"},

        {"version", 'V', POPT_ARG_NONE, NULL, CL_VERSION,
         "display version and copyright information and exit"},

        POPT_AUTOHELP 

        {NULL, 0, 0, NULL, 0}
      };
    
    poptContext optCon = poptGetContext ("vcdimager", argc, argv, optionsTable, 0);

    if (poptReadDefaultConfig (optCon, 0)) 
      fprintf (stderr, "warning, reading popt configuration failed\n"); 

    while ((opt = poptGetNextOpt (optCon)) != -1)
      switch (opt)
        {
        case CL_VERSION:
          fprintf (stdout, "GNU VCDImager " VERSION " [" HOST_ARCH "]\n\n"
                   "Copyright (c) 2001 Herbert Valerio Riedel <hvr@gnu.org>\n\n"
                   "GNU VCDImager may be distributed under the terms of the GNU General Public\n"
                   "Licence; For details, see the file `COPYING', which is included in the GNU\n"
                   "VCDImager distribution. There is no warranty, to the extent permitted by law.\n");
          exit (EXIT_SUCCESS);
          break;

        case CL_ADD_DIR:
          {
            const char *arg = poptGetOptArg (optCon);

            assert (arg != NULL);
            
            _add_dir (arg, "");
          }
          break;

        case CL_ADD_FILE:
        case CL_ADD_FILE_RAW:
          {
            const char *arg = poptGetOptArg (optCon);
            char *fname1 = NULL, *fname2 = NULL;

            assert (arg != NULL);

            if(!_parse_file_arg (arg, &fname1, &fname2)) 
              gl_add_file (fname1, fname2, (opt == CL_ADD_FILE_RAW));
            else
              {
                fprintf (stderr, "file parsing of `%s' failed\n", arg);
                exit (EXIT_FAILURE);
              }
          }
          break;

        default:
          fprintf (stderr, "error while parsing command line - try --help\n");
          exit (EXIT_FAILURE);
          break;
        }

    if (gl.verbose_flag && gl.quiet_flag)
      fprintf (stderr, "I can't be both, quiet and verbose... either one or another ;-)");
    
    if ((args = poptGetArgs (optCon)) == NULL)
      {
        fprintf (stderr, "error: need at least one data track as argument "
                 "-- try --help\n");
        exit (EXIT_FAILURE);
      }

    gl.tracks_list = _vcd_list_new ();

    for (n = 0; args[n]; n++)
      _vcd_list_append (gl.tracks_list, strdup (args[n]));

    if (_vcd_list_length (gl.tracks_list) > CD_MAX_TRACKS - 1)
      {
        fprintf (stderr, "error: maximal number of supported mpeg tracks (%d) reached",
                 CD_MAX_TRACKS - 1);
        exit (EXIT_FAILURE);
      }
                        
    if ((gl.type_id = _parse_type_arg (gl.type)) == VCD_TYPE_INVALID)
      exit (EXIT_FAILURE);

    poptFreeContext (optCon);
  }

  /* done with argument processing */

  _make_xml ();

  fprintf (stdout, "(Super) VideoCD xml description created successfully as `%s'\n",
           gl.xml_fname);

  return EXIT_SUCCESS;
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
