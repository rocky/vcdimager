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
#include <stdio.h>
#include <unistd.h>

#include <popt.h>

#include <libvcd/vcd_types.h>
#include <libvcd/vcd_files.h>
#include <libvcd/vcd_files_private.h>
#include <libvcd/vcd_image_bincue.h>
#include <libvcd/vcd_stream_stdio.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_iso9660_private.h>
#include <libvcd/vcd_bytesex.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_mpeg.h>

#include "vcdxml.h"
#include "vcd_xml_dtd.h"
#include "vcd_xml_dump.h"

static const char _rcsid[] = "$Id$";

static int _verbose_flag = 0;
static int _quiet_flag = 0;

static const char *
_strip_trail (const char str[], size_t n)
{
  static char buf[1024];
  int j;

  assert (n < 1024);

  strncpy (buf, str, n);
  buf[n] = '\0';

  for (j = strlen (buf) - 1; j >= 0; j--)
    {
      if (buf[j] != ' ')
        break;

      buf[j] = '\0';
    }

  return buf;
}

static int
_parse_pvd (struct vcdxml_t *obj, VcdImageSource *img)
{
  struct iso_primary_descriptor pvd;

  memset (&pvd, 0, sizeof (struct iso_primary_descriptor));
  assert (sizeof (struct iso_primary_descriptor) == ISO_BLOCKSIZE);

  vcd_image_source_read_mode2_sector (img, &pvd, ISO_PVD_SECTOR, false);

  if (pvd.type != ISO_VD_PRIMARY)
    {
      vcd_error ("unexcected descriptor type");
      return -1;
    }
  
  if (strncmp (pvd.id, ISO_STANDARD_ID, strlen (ISO_STANDARD_ID)))
    {
      vcd_error ("unexpected ID encountered (expected `"
		 ISO_STANDARD_ID "', got `%.5s'", pvd.id);
      return -1;
    }
 
  obj->pvd.volume_id = strdup (_strip_trail (pvd.volume_id, 32));
  obj->pvd.system_id = strdup (_strip_trail (pvd.system_id, 32));

  obj->pvd.publisher_id = strdup (_strip_trail (pvd.publisher_id, 128));
  obj->pvd.preparer_id = strdup (_strip_trail (pvd.preparer_id, 128));
  obj->pvd.application_id = strdup (_strip_trail (pvd.application_id, 128));

  return 0;
}

static int
_parse_info (struct vcdxml_t *obj, VcdImageSource *img)
{
  InfoVcd info;
  vcd_type_t _vcd_type = VCD_TYPE_INVALID;

  memset (&info, 0, sizeof (InfoVcd));
  assert (sizeof (InfoVcd) == ISO_BLOCKSIZE);

  vcd_image_source_read_mode2_sector (img, &info, INFO_VCD_SECTOR, false);

  /* analyze signature/type */

  if (!strncmp (info.ID, INFO_ID_VCD, sizeof (info.ID)))
    switch (info.version)
      {
      case INFO_VERSION_VCD2:
        if (info.sys_prof_tag != INFO_SPTAG_VCD2)
          vcd_warn ("unexpected system profile tag encountered");
        _vcd_type = VCD_TYPE_VCD2;
        break;

      case INFO_VERSION_VCD11:
        if (info.sys_prof_tag != INFO_SPTAG_VCD11)
          vcd_warn ("unexpected system profile tag encountered");
        _vcd_type = VCD_TYPE_VCD11;
        break;

      default:
        vcd_warn ("unexpected vcd version encountered -- assuming vcd 2.0");
        break;
      }
  else if (!strncmp (info.ID, INFO_ID_SVCD, sizeof (info.ID)))
    switch (info.version) 
      {
      case INFO_VERSION_SVCD:
        if (info.sys_prof_tag != INFO_SPTAG_VCD2)
          vcd_warn ("unexpected system profile tag value -- assuming svcd");
        _vcd_type = VCD_TYPE_SVCD;
        break;
        
      default:
        vcd_warn ("unexpected svcd version...");
        _vcd_type = VCD_TYPE_SVCD;
        break;
      }
  else
    vcd_error ("INFO signature not found");

  obj->vcd_type = _vcd_type;
  switch (_vcd_type)
    {
    case VCD_TYPE_VCD11:
      vcd_debug ("VCD 1.1 detected");
      break;
    case VCD_TYPE_VCD2:
      vcd_debug ("VCD 2.0 detected");
      break;
    case VCD_TYPE_SVCD:
      vcd_debug ("SVCD detected");
      break;
    case VCD_TYPE_INVALID:
      vcd_error ("unknown ID encountered -- maybe not a proper (S)VCD?");
      return -1;
      break;
    default:
      assert (0);
      break;
    }

  obj->info.album_id = strdup (_strip_trail (info.album_desc, 16));
  obj->info.volume_count = UINT16_FROM_BE (info.vol_count);
  obj->info.volume_number = UINT16_FROM_BE (info.vol_id);

  obj->info.restriction = info.flags.restriction;
  obj->info.use_lid2 = info.flags.use_lid2;
  obj->info.use_sequence2 = info.flags.use_track3;

  obj->info.psd_size = UINT32_FROM_BE (info.psd_size);
  obj->info.max_lid = UINT16_FROM_BE (info.lot_entries);

  {
    unsigned segment_start;
    unsigned max_seg_num;
    int idx, n;
    struct segment_t *_segment = NULL;

    segment_start = msf_to_lba (&info.first_seg_addr);

    max_seg_num = UINT16_FROM_BE (info.item_count);

    if (segment_start < 150)
      return 0;

    segment_start -= 150;

    assert (segment_start % 75 == 0);

    obj->info.segments_start = segment_start;

    if (!max_seg_num)
      return 0;

    n = 0;
    for (idx = 0; idx < max_seg_num; idx++)
      {
	if (!info.spi_contents[idx].item_cont)
	  { /* new segment */
	    char buf[80];
	    _segment = _vcd_malloc (sizeof (struct segment_t));

	    snprintf (buf, sizeof (buf), "segment-%4.4d", idx);
	    _segment->id = strdup (buf);

	    snprintf (buf, sizeof (buf), "item%4.4d.mpg", n);
	    _segment->src = strdup (buf);

	    _vcd_list_append (obj->segment_list, _segment);
	    n++;
	  }

	assert (_segment != NULL);

	_segment->segments_count++;
      }
  }

  return 0;
}

static int
_parse_entries (struct vcdxml_t *obj, VcdImageSource *img)
{
  EntriesVcd entries;
  int idx;
  uint8_t ltrack;

  memset (&entries, 0, sizeof (EntriesVcd));
  assert (sizeof (EntriesVcd) == ISO_BLOCKSIZE);

  vcd_image_source_read_mode2_sector (img, &entries, ENTRIES_VCD_SECTOR, false);

  /* analyze signature/type */

  if (!strncmp (entries.ID, ENTRIES_ID_VCD, sizeof (entries.ID)))
    {}
  else if (!strncmp (entries.ID, "ENTRYSVD", sizeof (entries.ID)))
    vcd_warn ("found (non-compliant) SVCD ENTRIES.SVD signature");
  else
    {
      vcd_error ("unexpected ID signature encountered `%.8s'", entries.ID);
      return -1;
    }

  ltrack = 0;
  for (idx = 0; idx < UINT16_FROM_BE (entries.entry_count); idx++)
    {
      uint32_t extent = msf_to_lba(&(entries.entry[idx].msf));
      uint8_t track = from_bcd8 (entries.entry[idx].n);
      bool newtrack = (track != ltrack);
      ltrack = track;
      
      assert (extent >= 150);
      extent -= 150;

      assert (track >= 2);
      track -= 2;

      if (newtrack)
	{
	  char buf[80];
	  struct sequence_t *_new_sequence = _vcd_malloc (sizeof (struct sequence_t));

	  snprintf (buf, sizeof (buf), "sequence-%2.2d", track);
	  _new_sequence->id = strdup (buf);

	  snprintf (buf, sizeof (buf), "avseq%2.2d.mpg", track);
	  _new_sequence->src = strdup (buf);

	  _new_sequence->entry_point_list = _vcd_list_new ();
	  _new_sequence->autopause_list = _vcd_list_new ();
	  _new_sequence->start_extent = extent;

	  _vcd_list_append (obj->sequence_list, _new_sequence);
	}
      else
	{
	  char buf[80];
	  struct sequence_t *_seq =
	    _vcd_list_node_data (_vcd_list_end (obj->sequence_list));

	  struct entry_point_t *_entry = _vcd_malloc (sizeof (struct entry_point_t));

	  snprintf (buf, sizeof (buf), "entry-%3.3d", idx);
	  _entry->id = strdup (buf);

	  _entry->extent = extent;

	  _vcd_list_append (_seq->entry_point_list, _entry);
	}

      /* vcd_debug ("%d %d %d %d", idx, track, extent, newtrack); */
    }

  return 0;
}

struct _pbc_ctx {
  
};

static char *
_xstrdup(const char *s)
{
  if (s)
    return strdup (s);
  
  return NULL;
}

static const char *
_pin2id (unsigned pin, const struct _pbc_ctx *_ctx)
{
  static char buf[80];

  if (pin < 2)
    return NULL;

  if (pin < 100)
    snprintf (buf, sizeof (buf), "sequence-%2.2d", pin - 2);
  else if (pin < 600)
    snprintf (buf, sizeof (buf), "entry-%3.3d", pin - 100);
  else if (pin < 1000)
    return NULL;
  else if (pin < 2980)
    snprintf (buf, sizeof (buf), "segment-%4.4d", pin - 1000);
  else 
    return NULL;

  return buf;
}

static const char *
_ofs2id (unsigned ofs, const struct _pbc_ctx *_ctx)
{
  static char buf[80];

  if (ofs == 0xffff)
    return NULL;

  snprintf (buf, sizeof (buf), "ofs-%4.4x", ofs);

  return buf;
}

static int
_calc_time (uint16_t wtime)
{
  if (wtime < 61)
    return wtime;
  else if (wtime < 255)
    return (wtime - 60) * 10 + 60;
  else
    return -1;
}

static pbc_t *
_pbc_node_read (const void *buf, const struct _pbc_ctx *_ctx)
{
  pbc_t *_pbc = NULL;
  const uint8_t *_buf = buf;

  switch (*_buf)
    {
      int n;

    case PSD_TYPE_PLAY_LIST:
      _pbc = vcd_pbc_new (PBC_PLAYLIST);
      {
	const PsdPlayListDescriptor *d = buf;
	_pbc->prev_id = _xstrdup (_ofs2id (UINT16_FROM_BE (d->prev_ofs), _ctx));
	_pbc->next_id = _xstrdup (_ofs2id (UINT16_FROM_BE (d->next_ofs), _ctx));
	_pbc->retn_id = _xstrdup (_ofs2id (UINT16_FROM_BE (d->return_ofs), _ctx));
	
	_pbc->playing_time = (double) (UINT16_FROM_BE (d->ptime)) / 15.0;
	_pbc->wait_time = _calc_time (d->wtime);
	_pbc->auto_pause_time =  _calc_time(d->atime);

	for (n = 0; n < d->noi; n++)
	  {
	    _vcd_list_append (_pbc->item_id_list, 
			      _xstrdup (_pin2id (UINT16_FROM_BE (d->itemid[n]), _ctx)));
	  }
      }
      break;

    case PSD_TYPE_SELECTION_LIST:
      _pbc = vcd_pbc_new (PBC_SELECTION);
      {
	const PsdSelectionListDescriptor *d = buf;
	_pbc->bsn = d->bsn;
	_pbc->prev_id = _xstrdup (_ofs2id (UINT16_FROM_BE (d->prev_ofs), _ctx));
	_pbc->next_id = _xstrdup (_ofs2id (UINT16_FROM_BE (d->next_ofs), _ctx));
	_pbc->retn_id = _xstrdup (_ofs2id (UINT16_FROM_BE (d->return_ofs), _ctx));

	assert (UINT16_FROM_BE (d->default_ofs) != 0xfffe);
	assert (UINT16_FROM_BE (d->default_ofs) != 0xfffd); /* fixme -- multidef lists
							       not supported yet */
	if (_ofs2id (UINT16_FROM_BE (d->default_ofs), _ctx))
	  _vcd_list_append (_pbc->default_id_list,
			    _xstrdup (_ofs2id (UINT16_FROM_BE (d->default_ofs), _ctx)));

	_pbc->timeout_id = _xstrdup (_ofs2id (UINT16_FROM_BE (d->timeout_ofs), _ctx));
	_pbc->timeout_time = _calc_time (d->totime);
	_pbc->jump_delayed = (0x80 & d->loop) != 0;
	_pbc->loop_count = (0x7f & d->loop);
	_pbc->item_id = _xstrdup (_pin2id (UINT16_FROM_BE (d->itemid), _ctx));

	for (n = 0; n < d->nos; n++)
	  {
	    _vcd_list_append (_pbc->select_id_list, 
			      _xstrdup (_ofs2id (UINT16_FROM_BE (d->ofs[n]), _ctx)));
	  }
      }
      break;

    case PSD_TYPE_END_OF_LIST:
      _pbc = vcd_pbc_new (PBC_END);
      break;

    default:
      vcd_warn ("unknown psd type %d", *_buf);
      break;
    }

  return _pbc;
}

static int
_parse_pbc (struct vcdxml_t *obj, VcdImageSource *img)
{
  /* VcdList *_offsets; -- fixme -- detect offsets better */
  LotVcd *lot = NULL;
  uint8_t *psd = NULL;
  int n;
  struct _pbc_ctx _pctx;

  if (!obj->info.psd_size)
    {
      vcd_debug ("no pbc info");
      return 0;
    }

  lot = _vcd_malloc (ISO_BLOCKSIZE * LOT_VCD_SIZE);
  
  for (n = 0; n < LOT_VCD_SIZE; n++)
    vcd_image_source_read_mode2_sector (img, ((char *) lot) + (n * ISO_BLOCKSIZE),
					LOT_VCD_SECTOR + n, false);

  psd = _vcd_malloc (ISO_BLOCKSIZE * _vcd_len2blocks (obj->info.psd_size, ISO_BLOCKSIZE));

  for (n = 0; n < _vcd_len2blocks (obj->info.psd_size, ISO_BLOCKSIZE); n++)
    vcd_image_source_read_mode2_sector (img, psd + (n * ISO_BLOCKSIZE),
					PSD_VCD_SECTOR + n, false);

  for (n = 0; n < obj->info.max_lid + 2; n++)
    {
      unsigned _ofs = UINT16_FROM_BE (lot->offset[n]);
      pbc_t *_pbc;

      if (_ofs == 0xffff)
	continue;

      if ((_ofs << 3) >= obj->info.psd_size)
	vcd_error ("offset out of range :-(");

      if ((_pbc = _pbc_node_read (&psd[_ofs << 3], &_pctx)))
	{
	  char buf[80];

	  snprintf (buf, sizeof (buf), "ofs-%4.4x", _ofs);
	  _pbc->id = _xstrdup (buf);
	  _vcd_list_append (obj->pbc_list, _pbc);
	}
    }

  /* fixme -- what about 'rejected' PBC lists... */

  return 0;
}

static int
_rip_segments (struct vcdxml_t *obj, VcdImageSource *img)
{
  VcdListNode *node;
  uint32_t start_extent;

  start_extent = obj->info.segments_start;

  assert (start_extent % 75 == 0);

  _VCD_LIST_FOREACH (node, obj->segment_list)
    {
      struct segment_t *_seg = _vcd_list_node_data (node);
      uint32_t n;
      FILE *outfd = NULL;

      assert (_seg->segments_count > 0);

      vcd_info ("extracting %s... (start lsn %d, %d segments)",
		_seg->src, start_extent, _seg->segments_count);

      if (!(outfd = fopen (_seg->src, "wb")))
        {
          perror ("fopen()");
          exit (EXIT_FAILURE);
        }

      for (n = 0; n < _seg->segments_count * 150; n++)
	{
	  struct m2f2sector
          {
            uint8_t subheader[8];
            uint8_t data[2324];
            uint8_t spare[4];
          }
          buf;
	  memset (&buf, 0, sizeof (buf));
	  vcd_image_source_read_mode2_sector (img, &buf, start_extent + n, true);
	  
	  if (!buf.subheader[0] 
              && !buf.subheader[1]
              && (buf.subheader[2] | SM_FORM2) == SM_FORM2
              && !buf.subheader[3])
            {
              vcd_warn ("no EOF seen, but stream ended");
              break;
            }

	  fwrite (buf.data, 2324, 1, outfd);

	  if (buf.subheader[2] & SM_EOF)
            break;
	}

      fclose (outfd);

      start_extent += _seg->segments_count * 150;
    }

  return 0;
}

static int
_rip_sequences (struct vcdxml_t *obj, VcdImageSource *img)
{
  VcdListNode *node;

  vcd_warn ("NIY");

  _VCD_LIST_FOREACH (node, obj->sequence_list)
    {
      struct sequence_t *_seq = _vcd_list_node_data (node);
      VcdListNode *nnode = _vcd_list_node_next (node);
      struct sequence_t *_nseq = nnode ? _vcd_list_node_data (nnode) : NULL;
      FILE *outfd = NULL;
      bool in_data = false;
      VcdMpegStreamCtx mpeg_ctx;
      uint32_t start_lsn, end_lsn, n, last_nonzero, first_data;
      double last_pts = 0;

      memset (&mpeg_ctx, 0, sizeof (VcdMpegStreamCtx));

      start_lsn = _seq->start_extent;
      end_lsn = _nseq ? _nseq->start_extent : vcd_image_source_stat_size (img);

      vcd_info ("extracting %s... (start lsn %d (+%d))",
		_seq->src, start_lsn, end_lsn - start_lsn);

      if (!(outfd = fopen (_seq->src, "wb")))
        {
          perror ("fopen()");
          exit (EXIT_FAILURE);
        }

      last_nonzero = start_lsn - 1;
      first_data = 0;
      for (n = start_lsn; n < end_lsn; n++)
	{
	  struct m2f2sector
          {
            uint8_t subheader[8];
            uint8_t data[2324];
            uint8_t spare[4];
          }
          buf;

	  memset (&buf, 0, sizeof (buf));
	  vcd_image_source_read_mode2_sector (img, &buf, n, true);

	  if (_nseq && n + 150 == end_lsn + 1)
	    vcd_warn ("reading into gap @%d... :-(", n);

	  if (!(buf.subheader[2] & SM_FORM2))
	    {
	      vcd_warn ("encountered non-form2 sector -- leaving loop");
	      break;
	    }
	  
	  if (in_data)
	    { /* end conditions... */
	      if (!buf.subheader[0])
		{
		  vcd_debug ("fn -edge @%d", n);
		  break;
		}

	      if (!(buf.subheader[2] & SM_REALT))
		{
		  vcd_debug ("subheader: no realtime data anymore @%d", n);
		  break;
		}
	    }

	  if (buf.subheader[1] && !in_data)
	    {
	      vcd_debug ("cn +edge @%d", n);
	      in_data = true;
	    }


#ifdef DEBUG	 
	  if (!in_data)
	    vcd_debug ("%2.2x %2.2x %2.2x %2.2x",
		       buf.subheader[0],
		       buf.subheader[1],
		       buf.subheader[2],
		       buf.subheader[3]);
#endif

	  if (in_data)
	    {
	      VcdListNode *_node;

	      vcd_mpeg_parse_packet (buf.data, 2324, false, &mpeg_ctx);

	      if (!mpeg_ctx.packet.zero)
		last_nonzero = n;

	      if (!first_data && !mpeg_ctx.packet.zero)
		first_data = n;
	      
	      if (mpeg_ctx.packet.has_pts)
		{
		  last_pts = mpeg_ctx.packet.pts;
		  if (mpeg_ctx.stream.seen_pts)
		    last_pts -= mpeg_ctx.stream.min_pts;
		  if (last_pts < 0)
		    last_pts = 0;
		  /* vcd_debug ("pts %f @%d", mpeg_ctx.packet.pts, n); */
		}

	      if (buf.subheader[2] & SM_TRIG)
		{
		  double *_ap_ts = _vcd_malloc (sizeof (double));

		  vcd_debug ("autopause @%d (%f)", n, last_pts);
		  *_ap_ts = last_pts;

		  _vcd_list_append (_seq->autopause_list, _ap_ts);
		}
	      
	      _VCD_LIST_FOREACH (_node, _seq->entry_point_list)
		{
		  struct entry_point_t *_ep = _vcd_list_node_data (_node);

		  if (_ep->extent == n)
		    {
		      vcd_debug ("entry point @%d (%f)", n, last_pts);
		      _ep->timestamp = last_pts;
		    }
		}
	      
	      if (first_data)
		fwrite (buf.data, 2324, 1, outfd);
	    }
	  
	  if (buf.subheader[2] & SM_EOF)
	    {
	      vcd_debug ("encountered subheader EOF @%d", n);
	      break;
	    }
	}
      if (in_data)
	{
	  uint32_t length;

	  if (n == end_lsn)
	    vcd_debug ("stream till end of track");
	  
	  length = (1 + last_nonzero) - first_data;

	  vcd_debug ("truncating file to %d packets", length);

	  fflush (outfd);
	  if (ftruncate (fileno (outfd), length * 2324))
	    perror ("ftruncate()");
	}

      fclose (outfd);
    }

  return 0;
}

#define DEFAULT_XML_FNAME      "videocd.xml"
#define DEFAULT_IMG_FNAME      "videocd.bin"

int
main (int argc, const char *argv[])
{
  VcdDataSource *bin_source;
  VcdImageSource *img_src;
  struct vcdxml_t obj;

  /* cl params */
  char *xml_fname = NULL;
  char *img_fname = NULL;
  char *img_dname = NULL;
  int norip_flag = 0;
  int sector_2336_flag = 0;

  memset (&obj, 0, sizeof (struct vcdxml_t));

  obj.comment = vcd_xml_dump_cl_comment (argc, argv);

  {
    enum { CL_VERSION = 1 };
    poptContext optCon = NULL;
    int opt;

    struct poptOption optionsTable[] = {
      {"output-file", 'o', POPT_ARG_STRING, &xml_fname, 0,
       "specify xml file for output (default: '" DEFAULT_XML_FNAME "')",
       "FILE"},

      {"bin-file", 'b', POPT_ARG_STRING, &img_fname, 0,
       "set image file as source (default: '" DEFAULT_IMG_FNAME "')", 
       "FILE"},

      {"sector-2336", '\0', POPT_ARG_NONE, &sector_2336_flag, 0,
       "use 2336 byte sector mode for image file"},

#if defined(__linux__)
      {"cdrom-device", '\0', POPT_ARG_STRING, &img_dname, 0,
       "set CDROM device as source (linux only)", "DEVICE"},
#endif

      {"norip", '\0', POPT_ARG_NONE, &norip_flag, 0,
       "dont rip mpeg streams"},

      {"verbose", 'v', POPT_ARG_NONE, &_verbose_flag, 0, 
       "be verbose"},
    
      {"quiet", 'q', POPT_ARG_NONE, &_quiet_flag, 0, 
       "show only critical messages"},

      {"version", 'V', POPT_ARG_NONE, NULL, CL_VERSION,
       "display version and copyright information and exit"},
      
      POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
    };

    optCon = poptGetContext ("vcdimager", argc, argv, optionsTable, 0);
      
    if (poptReadDefaultConfig (optCon, 0)) 
      vcd_warn ("reading popt configuration failed"); 

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

	default:
	  vcd_error ("error while parsing command line - try --help");
	  exit (EXIT_FAILURE);
	  break;
      }

    if (_verbose_flag && _quiet_flag)
      vcd_error ("I can't be both, quiet and verbose... either one or another ;-)");
    
    if (poptGetArgs (optCon) != NULL)
      vcd_error ("why are you giving me non-option arguments? -- try --help");

    if (img_fname && img_dname)
      vcd_error ("file name and device name given at the same time...");

    poptFreeContext (optCon);
  }

  if (!xml_fname)
    xml_fname = strdup (DEFAULT_XML_FNAME);

  if (!img_dname && !img_fname)
    img_fname = strdup (DEFAULT_IMG_FNAME);

  if (!img_fname)
    vcd_error ("bug: only file sources supported so far...");

  obj.segment_list = _vcd_list_new ();
  obj.sequence_list = _vcd_list_new ();
  obj.pbc_list = _vcd_list_new ();
  obj.filesystem = _vcd_list_new ();
  obj.option_list = _vcd_list_new ();

  bin_source = vcd_data_source_new_stdio (img_fname);
  assert (bin_source != NULL);

  img_src = vcd_image_source_new_bincue (bin_source, NULL, false);
  assert (img_src != NULL);

  /* start with ISO9660 PVD */
  _parse_pvd (&obj, img_src);

  /* _parse_iso_fs (&obj, img_src); */
  
  /* needs to be parsed in order */
  _parse_info (&obj, img_src);
  _parse_entries (&obj, img_src);

  /* needs to be parsed last! */
  _parse_pbc (&obj, img_src);

  if (norip_flag)
    vcd_warn ("fyi, entry point and auto pause locations cannot be "
	      "determined without mpeg extraction -- hope that's ok");
  else
    {
      /* _rip_files (&obj, img_src); */
      _rip_segments (&obj, img_src);
      _rip_sequences (&obj, img_src);
    }

  vcd_info ("writing xml description to `%s'...", xml_fname);
  vcd_xml_dump (&obj, xml_fname);

  vcd_image_source_destroy (img_src);

  vcd_info ("done");
  return 0;
}
