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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <popt.h>

#include <libxml/parserInternals.h>
#include <libxml/parser.h>
#include <libxml/valid.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlerror.h>


#include <libvcd/vcd_types.h>
#include <libvcd/vcd_logging.h>

#include "vcdxml.h"
#include "vcd_xml_parse.h"
#include "vcd_xml_master.h"
#include "vcd_xml_dtd.h"
#include "vcd_xml_common.h"

static const char _rcsid[] = "$Id$";

static void
_init_xml (void)
{
  static bool _init_done = false;

  vcd_assert (!_init_done);
  _init_done = true;

  xmlKeepBlanksDefaultValue = false;
  xmlIndentTreeOutput = true; 

  vcd_xml_dtd_init ();
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

  /* ctxt->keepBlanks = false; */
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

#define DEFAULT_CUE_FILE       "videocd.cue"
#define DEFAULT_BIN_FILE       "videocd.bin"

static struct {
  char *xml_fname;
  char *bin_fname;
  char *cue_fname;
  char *cdrdao_base;
  char *nrg_fname;
  int sector_2336_flag;
  int verbose_flag;
  int quiet_flag;
  int progress_flag;
  int gui_flag;
} gl;


static int
_do_cl (int argc, const char *argv[])
{
  const char **args = NULL;
  int n, opt = 0;
  enum { CL_VERSION = 1 };
  poptContext optCon = NULL;
  struct poptOption optionsTable[] = 
    {
      {"cue-file", 'c', POPT_ARG_STRING, &gl.cue_fname, 0,
       "specify cue file for output (default: '" DEFAULT_CUE_FILE "')",
       "FILE"},
      
      {"bin-file", 'b', POPT_ARG_STRING, &gl.bin_fname, 0,
       "specify bin file for output (default: '" DEFAULT_BIN_FILE "')",
       "FILE"},

      {"cdrdao-file", '\0', POPT_ARG_STRING, &gl.cdrdao_base, 0,
       "specify cdrdao-style image filename base", "FILE"},

      {"nrg-file", '\0', POPT_ARG_STRING | POPT_ARGFLAG_DOC_HIDDEN,
       &gl.nrg_fname, 0, "specify nrg-style image filename", "FILE"},

      { "sector-2336", '\0', POPT_ARG_NONE, &gl.sector_2336_flag, 0,
	"use 2336 byte sectors for output"},

      {"progress", 'p', POPT_ARG_NONE, &gl.progress_flag, 0,  
       "show progress"}, 

      {"verbose", 'v', POPT_ARG_NONE, &gl.verbose_flag, 0, 
       "be verbose"},
	
      {"quiet", 'q', POPT_ARG_NONE, &gl.quiet_flag, 0, 
       "show only critical messages"},

      {"gui", '\0', POPT_ARG_NONE, &gl.gui_flag, 0, "enable GUI mode"},

      {"version", 'V', POPT_ARG_NONE, NULL, CL_VERSION,
       "display version and copyright information and exit"},

      POPT_AUTOHELP 

      {NULL, 0, 0, NULL, 0}
    };

  gl.cue_fname = strdup (DEFAULT_CUE_FILE);
  gl.bin_fname = strdup (DEFAULT_BIN_FILE);
  
  optCon = poptGetContext ("vcdimager", argc, argv, optionsTable, 0);
  
  if (poptReadDefaultConfig (optCon, 0)) 
    fprintf (stderr, "warning, reading popt configuration failed\n"); 

  while ((opt = poptGetNextOpt (optCon)) != -1)
    switch (opt)
      {
      case CL_VERSION:
	fputs (vcd_version_string (true), stdout);
	fflush (stdout);
	exit (EXIT_SUCCESS);
	break;

      default:
	vcd_error ("error while parsing command line - try --help");
	break;
      }

  if (gl.verbose_flag && gl.quiet_flag)
    vcd_error ("I can't be both, quiet and verbose... either one or another ;-)");
    
  if ((args = poptGetArgs (optCon)) == NULL)
    vcd_error ("xml input file argument missing -- try --help");

  for (n = 0; args[n]; n++);

  if (n != 1)
    vcd_error ("only one xml input file argument allowed -- try --help");

  gl.xml_fname = strdup (args[0]);

  poptFreeContext (optCon);

  return 0;
}

int 
main (int argc, const char *argv[])
{
  xmlDocPtr vcd_doc;

  _init_xml ();

  vcd_xml_log_init ();

  if (_do_cl (argc, argv))
    return EXIT_FAILURE;

  if (gl.quiet_flag)
    vcd_xml_verbosity = LOG_WARN;
  else if (gl.verbose_flag)
    vcd_xml_verbosity = LOG_DEBUG;
  else
    vcd_xml_verbosity = LOG_INFO;

  if (gl.gui_flag)
    vcd_xml_gui_mode = true;

  if (gl.progress_flag)
    vcd_xml_show_progress = true;

  errno = 0;
  if (!(vcd_doc = _xmlParseFile (gl.xml_fname)))
    {
      if (errno)
	vcd_error ("error while parsing file `%s': %s",
		   gl.xml_fname, strerror (errno));
      else
	vcd_error ("parsing file `%s' failed", gl.xml_fname);
      return EXIT_FAILURE;
    }

  if (vcd_xml_dtd_loaded < 1)
    {
      vcd_error ("doctype declaration missing in `%s'", gl.xml_fname);
      return EXIT_FAILURE;
    }

  {
    xmlNodePtr root;
    xmlNsPtr ns;
    struct vcdxml_t obj;
    
    vcd_xml_init (&obj);
    
    if (!(root = xmlDocGetRootElement (vcd_doc)))
      vcd_error ("XML document seems to be empty (no root node found)");

    if (!(ns = xmlSearchNsByHref (vcd_doc, root, VIDEOCD_DTD_XMLNS)))
      vcd_error ("Namespace not found in document");
    
    if (vcd_xml_parse (&obj, vcd_doc, root, ns))
      vcd_error ("parsing tree failed");

    if (vcd_xml_master (&obj, gl.cue_fname, gl.bin_fname,
			gl.cdrdao_base, 
			gl.nrg_fname, 
			gl.sector_2336_flag))
      vcd_error ("building videocd failed");
  } 

  xmlFreeDoc (vcd_doc);

  return EXIT_SUCCESS;
}
