/*
    $Id$

    Copyright (C) 2001,2003 Herbert Valerio Riedel <hvr@gnu.org>

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
#include <errno.h>
#include <ctype.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <popt.h>

#include <libvcd/vcd_bytesex.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_files.h>
#include <libvcd/vcd_files_private.h>
#include <libvcd/vcd_image_fs.h>
#include <libvcd/vcd_image_bincue.h>
#include <libvcd/vcd_image_cd.h>
#include <libvcd/vcd_image_nrg.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_iso9660_private.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_mpeg.h>
#include <libvcd/vcd_stream_stdio.h>
#include <libvcd/vcd_types.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_xa.h>
#include <libvcd/vcdinfo.h>
#include <libvcd/vcdinf.h>

#include "vcdxml.h"
#include "vcd_xml_dtd.h"
#include "vcd_xml_dump.h"
#include "vcd_xml_common.h"

static const char _rcsid[] = "$Id$";

static int _verbose_flag = 0;
static int _quiet_flag = 0;

static void
_register_file (struct vcdxml_t *obj, const char *pathname,
		vcd_image_stat_t const *statbuf)
{
  uint16_t xa_attr = uint16_from_be (statbuf->xa.attributes);

  while (*pathname == '/')
    pathname++;

  switch (statbuf->type)
    {
    case _STAT_DIR:
	{
	  struct filesystem_t *_fs 
	    = _vcd_malloc (sizeof (struct filesystem_t));
	  _vcd_list_append (obj->filesystem, _fs);
	  
	  _fs->name = strdup (pathname);
	}
      break;

    case _STAT_FILE:
      {
	char *namebuf = strdup (pathname);
	struct filesystem_t *_fs = _vcd_malloc (sizeof (struct filesystem_t));
	_vcd_list_append (obj->filesystem, _fs);

	if (strrchr (namebuf, ';'))
	  *strrchr (namebuf, ';') = '\0';

	_fs->name = namebuf;

	{
	  char namebuf2[strlen (namebuf) + 2];
	  int i;
		
	  namebuf2[0] = '_';

	  for (i = 0; namebuf[i]; i++)
	    namebuf2[i + 1] = (namebuf[i] == '/') ? '_' : tolower (namebuf[i]);

	  namebuf2[i + 1] = '\0';

	  _fs->file_src = strdup (namebuf2);
	}

	_fs->lsn = statbuf->lsn;
	_fs->size = statbuf->size;
	_fs->file_raw = (xa_attr & XA_ATTR_MODE2FORM2) != 0;

	vcd_debug ("file %s", namebuf);

	if (_fs->file_raw)
	  {
	    if (_fs->size % ISO_BLOCKSIZE == 0)
	      { 
		_fs->size /= ISO_BLOCKSIZE;
		_fs->size *= M2RAW_SECTOR_SIZE;
	      }
	    else if (_fs->size % M2RAW_SECTOR_SIZE == 0)
	      vcd_warn ("detected wrong size calculation for form2 file `%s'; fixing up", namebuf);
	    else 
	      vcd_error ("form2 file has invalid file size");
	  }
      }
      break;
      
    default:
      vcd_assert_not_reached ();
      break;
    }
}

static int
_parse_isofs_r (struct vcdxml_t *obj, VcdImageSource *img, const char pathname[])
{ 
  VcdList *entlist = vcd_image_source_fs_readdir (img, pathname);
  VcdListNode *entnode;

  if (entlist == NULL)
    return -1;

  _VCD_LIST_FOREACH (entnode, entlist)
    {
      char *_name = _vcd_list_node_data (entnode);
      char _fullname[4096] = { 0, };
      vcd_image_stat_t statbuf;

      strncpy (_fullname, pathname, sizeof (_fullname));
      strncat (_fullname, _name, sizeof (_fullname));

      if (vcd_image_source_fs_stat (img, _fullname, &statbuf))
	return -1;
      
      if (!strcmp (_name, ".") 
	  || !strcmp (_name, "..")
	  || !strcmp (_name, "MPEGAV")
	  || !strcmp (_name, "MPEG2")
	  || !strcmp (_name, "VCD")
	  || !strcmp (_name, "SVCD")
	  || !strcmp (_name, "SEGMENT")
	  || !strcmp (_name, "EXT")
	  || !strcmp (_name, "CDDA"))
	continue;

      _register_file (obj, _fullname, &statbuf);

      if (statbuf.type == _STAT_DIR)
	{
	  strncat (_fullname, "/", sizeof (_fullname));
	  if (_parse_isofs_r (obj, img, _fullname))
	    return -1;
	}
    }

  _vcd_list_free (entlist, true);

  return 0;
}

static int
_parse_isofs (struct vcdxml_t *obj, VcdImageSource *img)
{
  return _parse_isofs_r (obj, img, "/");
}

static int
_parse_pvd (struct vcdxml_t *obj, VcdImageSource *img)
{
  struct iso_primary_descriptor pvd;

  memset (&pvd, 0, sizeof (struct iso_primary_descriptor));
  vcd_assert (sizeof (struct iso_primary_descriptor) == ISO_BLOCKSIZE);

  if (vcd_image_source_read_mode2_sector (img, &pvd, ISO_PVD_SECTOR, false) != 0) 
    {
      vcd_error ("error reading PVD sector (%d)", ISO_PVD_SECTOR);
      return -1;
    }
  

  if (pvd.type != ISO_VD_PRIMARY)
    {
      vcd_error ("unexpected descriptor type");
      return -1;
    }
  
  if (strncmp (pvd.id, ISO_STANDARD_ID, strlen (ISO_STANDARD_ID)))
    {
      vcd_error ("unexpected ID encountered (expected `"
		 ISO_STANDARD_ID "', got `%.5s'", pvd.id);
      return -1;
    }
 
  obj->pvd.volume_id      = strdup (vcdinf_get_volume_id(&pvd));
  obj->pvd.system_id      = strdup (vcdinf_get_system_id(&pvd));
  obj->pvd.publisher_id   = strdup (vcdinf_get_publisher_id(&pvd));
  obj->pvd.preparer_id    = strdup (vcdinf_get_preparer_id(&pvd));
  obj->pvd.application_id = strdup (vcdinf_get_application_id(&pvd));

  return 0;
}

static int
_parse_info (struct vcdxml_t *obj, VcdImageSource *img)
{
  InfoVcd info;
  vcd_type_t _vcd_type;

  memset (&info, 0, sizeof (InfoVcd));
  vcd_assert (sizeof (InfoVcd) == ISO_BLOCKSIZE);

  vcd_image_source_read_mode2_sector (img, &info, INFO_VCD_SECTOR, false);

  /* analyze signature/type */

  obj->vcd_type = _vcd_type = vcd_files_info_detect_type (&info);

  switch (_vcd_type)
    {
    case VCD_TYPE_VCD:
    case VCD_TYPE_VCD11:
    case VCD_TYPE_VCD2:
    case VCD_TYPE_SVCD:
    case VCD_TYPE_HQVCD:
      vcd_debug ("%s detected", vcdinf_get_format_version_str(_vcd_type));
      break;
    case VCD_TYPE_INVALID:
      vcd_error ("unknown ID encountered -- maybe not a proper (S)VCD?");
      return -1;
      break;
    default:
      vcd_assert_not_reached ();
      break;
    }

  obj->info.album_id = strdup (vcdinf_get_album_id(&info));
  obj->info.volume_count = vcdinf_get_volume_count(&info);
  obj->info.volume_number = vcdinf_get_volume_num(&info);

  obj->info.restriction = info.flags.restriction;
  obj->info.use_lid2 = info.flags.use_lid2;
  obj->info.use_sequence2 = info.flags.use_track3;

  obj->info.psd_size = vcdinf_get_psd_size(&info);
  obj->info.max_lid = vcdinf_get_num_LIDs(&info);

  {
    lba_t segment_start;
    unsigned max_seg_num;
    int idx, n;
    struct segment_t *_segment = NULL;

    segment_start = msf_to_lba (&info.first_seg_addr);

    max_seg_num = vcdinf_get_num_segments(&info);

    if (segment_start < PREGAP_SECTORS)
      return 0;

    segment_start = vcdinfo_lba2lsn(segment_start);

    vcd_assert (segment_start % CD_FRAMES_PER_SECOND == 0);

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

	    snprintf (buf, sizeof (buf), "item%4.4d.mpg", idx + 1);
	    _segment->src = strdup (buf);

	    _segment->autopause_list = _vcd_list_new ();

	    _vcd_list_append (obj->segment_list, _segment);
	    n++;
	  }

	vcd_assert (_segment != NULL);

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
  vcd_assert (sizeof (EntriesVcd) == ISO_BLOCKSIZE);

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
  for (idx = 0; idx < vcdinf_get_num_entries(&entries); idx++)
    {
      lba_t extent = vcdinf_get_entry_lba(&entries, idx);
      track_t track = vcdinf_get_track(&entries, idx);
      bool newtrack = (track != ltrack);
      ltrack = track;
      
      vcd_assert (extent >= PREGAP_SECTORS);
      extent = vcdinfo_lba2lsn(extent);

      vcd_assert (track >= 2);
      track -= 2;

      if (newtrack)
	{
	  char buf[80];
	  struct sequence_t *_new_sequence;

	  _new_sequence = _vcd_malloc (sizeof (struct sequence_t));

	  snprintf (buf, sizeof (buf), "sequence-%2.2d", track);
	  _new_sequence->id = strdup (buf);

	  snprintf (buf, sizeof (buf), "avseq%2.2d.mpg", track + 1);
	  _new_sequence->src = strdup (buf);

	  _new_sequence->entry_point_list = _vcd_list_new ();
	  _new_sequence->autopause_list = _vcd_list_new ();
	  _new_sequence->start_extent = extent;

	  snprintf (buf, sizeof (buf), "entry-%3.3d", idx);
	  _new_sequence->default_entry_id = strdup (buf);
	  
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

static char *
_xstrdup(const char *s)
{
  if (s)
    return strdup (s);
  
  return NULL;
}

struct _pbc_ctx {
  unsigned psd_size;
  unsigned maximum_lid;
  unsigned offset_mult;
  VcdList *offset_list;

  uint8_t *psd;
  LotVcd *lot;
  bool extended;
};

static const char *
_pin2id (unsigned pin, const struct _pbc_ctx *_ctx)
{
  static char buf[80];
  vcdinfo_itemid_t itemid;
  
  vcdinfo_classify_itemid (pin, &itemid);

  switch(itemid.type) {
  case VCDINFO_ITEM_TYPE_NOTFOUND:
    return NULL;
  case VCDINFO_ITEM_TYPE_TRACK:
    snprintf (buf, sizeof (buf), "sequence-%2.2d", itemid.num-1);
    break;
  case VCDINFO_ITEM_TYPE_ENTRY:
    snprintf (buf, sizeof (buf), "entry-%3.3d", itemid.num);
    break;
  case VCDINFO_ITEM_TYPE_SEGMENT:
    snprintf (buf, sizeof (buf), "segment-%4.4d", itemid.num+1);
    break;
  case VCDINFO_ITEM_TYPE_SPAREID2:
  default:
    return NULL;
  }
  return buf;
}

static const char *
_ofs2id (unsigned offset, const struct _pbc_ctx *_ctx)
{
  VcdListNode *node;
  static char buf[80];
  unsigned sl_num = 0, el_num = 0, pl_num = 0;
  vcdinfo_offset_t *ofs = NULL;
  
  if (offset == PSD_OFS_DISABLED)
    return NULL;

  _VCD_LIST_FOREACH (node, _ctx->offset_list)
    {
      ofs = _vcd_list_node_data (node);
	
      switch (ofs->type)
	{
	case PSD_TYPE_PLAY_LIST:
	  pl_num++;
	  break;

	case PSD_TYPE_EXT_SELECTION_LIST:
	case PSD_TYPE_SELECTION_LIST:
	  sl_num++;
	  break;

	case PSD_TYPE_END_LIST:
	  el_num++;
	  break;

	default:
	  vcd_assert_not_reached ();
	  break;
	}

      if (ofs->offset == offset)
	break;
    }

  if (node)
    {
      switch (ofs->type)
	{
	case PSD_TYPE_PLAY_LIST:
	  snprintf (buf, sizeof (buf), "playlist-%.2d", pl_num);
	  break;

	case PSD_TYPE_EXT_SELECTION_LIST:
	case PSD_TYPE_SELECTION_LIST:
	  snprintf (buf, sizeof (buf), "selection-%.2d", sl_num);
	  break;

	case PSD_TYPE_END_LIST:
	  snprintf (buf, sizeof (buf), "end-%d", el_num);
	  break;

	default:
	  snprintf (buf, sizeof (buf), "unknown-type-%4.4x", offset);
	  break;
	}
    }
  else
    snprintf (buf, sizeof (buf), "unknown-offset-%4.4x", offset);

  return buf;
}

static pbc_t *
_pbc_node_read (const struct _pbc_ctx *_ctx, unsigned offset)
{
  pbc_t *_pbc = NULL;
  const uint8_t *_buf = &_ctx->psd[offset * _ctx->offset_mult];
  vcdinfo_offset_t *ofs = NULL;

  {
    VcdListNode *node;
    _VCD_LIST_FOREACH (node, _ctx->offset_list)
      {
	ofs = _vcd_list_node_data (node);

	if (ofs->offset == offset)
	  break;
      }
    vcd_assert (node);
  }

  switch (*_buf)
    {
      int n;

    case PSD_TYPE_PLAY_LIST:
      _pbc = vcd_pbc_new (PBC_PLAYLIST);
      {
	const PsdPlayListDescriptor *d = (const void *) _buf;
	_pbc->prev_id = _xstrdup (_ofs2id (vcdinf_get_prev_from_pld(d), 
					   _ctx));
	_pbc->next_id = _xstrdup (_ofs2id (vcdinf_get_next_from_pld(d),
					   _ctx));
	_pbc->retn_id = _xstrdup (_ofs2id (vcdinf_get_return_from_pld(d), 
					   _ctx));
	
	_pbc->playing_time = (double) (vcdinf_get_play_time(d)) / 15.0;
	_pbc->wait_time       = vcdinfo_get_wait_time(d);
	_pbc->auto_pause_time = vcdinfo_get_autowait_time(d);

	for (n = 0; n < d->noi; n++) {
	  _vcd_list_append (_pbc->item_id_list, 
			    _xstrdup(_pin2id(vcdinf_get_play_item_from_pld(d, n),
					     _ctx)));
	  }
      }
      break;

    case PSD_TYPE_EXT_SELECTION_LIST:
    case PSD_TYPE_SELECTION_LIST:
      _pbc = vcd_pbc_new (PBC_SELECTION);
      {
	const PsdSelectionListDescriptor *d = (const void *) _buf;
	_pbc->bsn = vcdinf_get_bsn(d);
	_pbc->prev_id = _xstrdup (_ofs2id (vcdinf_get_prev_from_psd(d), 
					   _ctx));
	_pbc->next_id = _xstrdup (_ofs2id (vcdinf_get_next_from_psd(d), 
					   _ctx));
	_pbc->retn_id = _xstrdup (_ofs2id (vcdinf_get_return_from_psd(d), 
					   _ctx));

	switch (vcdinf_get_default_from_psd(d))
	  {
	  case PSD_OFS_DISABLED:
	    _pbc->default_id = NULL;
	    _pbc->selection_type = _SEL_NORMAL;
	    break;

	  case PSD_OFS_MULTI_DEF:
	    _pbc->default_id = NULL;
	    _pbc->selection_type = _SEL_MULTI_DEF;
	    break;

	  case PSD_OFS_MULTI_DEF_NO_NUM:
	    _pbc->default_id = NULL;
	    _pbc->selection_type = _SEL_MULTI_DEF_NO_NUM;
	    break;

	  default:
	    _pbc->default_id = _xstrdup (_ofs2id (uint16_from_be (d->default_ofs), _ctx));
	    _pbc->selection_type = _SEL_NORMAL;
	    break;
	  }

	_pbc->timeout_id = 
	  _xstrdup (_ofs2id (uint16_from_be (d->timeout_ofs), _ctx));
	_pbc->timeout_time = vcdinfo_get_timeout_time (d);
	_pbc->jump_delayed = vcdinf_has_jump_delay(d);
	_pbc->loop_count = vcdinf_get_loop_count(d);
	_pbc->item_id = _xstrdup (_pin2id (uint16_from_be (d->itemid), _ctx));

	for (n = 0; n < d->nos; n++)
	  {
	    _vcd_list_append (_pbc->select_id_list, 
			      _xstrdup (_ofs2id (uint16_from_be (d->ofs[n]), _ctx)));
	  }

	if (d->type == PSD_TYPE_EXT_SELECTION_LIST
	    || d->flags.SelectionAreaFlag)
	  {
	    PsdSelectionListDescriptorExtended *d2 = (void *) &d->ofs[d->nos];

	    _pbc->prev_area = _vcd_malloc (sizeof (pbc_area_t));
	    _pbc->next_area = _vcd_malloc (sizeof (pbc_area_t));
	    _pbc->return_area = _vcd_malloc (sizeof (pbc_area_t));
	    _pbc->default_area = _vcd_malloc (sizeof (pbc_area_t));

	    *_pbc->prev_area = d2->prev_area;
	    *_pbc->next_area = d2->next_area;
	    *_pbc->return_area = d2->return_area;
	    *_pbc->default_area = d2->default_area;

	    for (n = 0; n < d->nos; n++)
	      {
		pbc_area_t *_area = _vcd_malloc (sizeof (pbc_area_t));

		*_area = d2->area[n];

		_vcd_list_append (_pbc->select_area_list, _area);
	      }

	    vcd_assert (_vcd_list_length (_pbc->select_area_list) 
			== _vcd_list_length (_pbc->select_id_list));
	  }
      }
      break;

    case PSD_TYPE_END_LIST:
      _pbc = vcd_pbc_new (PBC_END);
      {
	const PsdEndListDescriptor *d = (const void *) _buf;
	
	_pbc->next_disc = d->next_disc;
	_pbc->image_id = _xstrdup (_pin2id (uint16_from_be (d->change_pic), _ctx));
      }      
      break;

    default:
      vcd_warn ("Unknown PSD type %d", *_buf);
      break;
    }

  if (_pbc)
    {
      _pbc->id = _xstrdup (_ofs2id (offset, _ctx));
      _pbc->rejected = !ofs->in_lot;
    }

  return _pbc;
}

static void
_visit_pbc (struct _pbc_ctx *obj, unsigned lid, unsigned offset, bool in_lot)
{
  VcdListNode *node;
  vcdinfo_offset_t *ofs;
  unsigned _rofs = offset * obj->offset_mult;

  vcd_assert (obj->psd_size % 8 == 0);

  switch (offset)
    {
    case PSD_OFS_DISABLED:
    case PSD_OFS_MULTI_DEF:
    case PSD_OFS_MULTI_DEF_NO_NUM:
      return;

    default:
      break;
    }

  if (_rofs >= obj->psd_size)
    {
      if (obj->extended)
	vcd_error ("psd offset out of range in extended PSD"
		   " (try --no-ext-psd option)");
      else
	vcd_warn ("psd offset out of range, ignoring...");
      return;
    }

  vcd_assert (_rofs < obj->psd_size);

  if (!obj->offset_list)
    obj->offset_list = _vcd_list_new ();

  _VCD_LIST_FOREACH (node, obj->offset_list)
    {
      ofs = _vcd_list_node_data (node);

      if (offset == ofs->offset)
        {
          if (in_lot)
            ofs->in_lot = true;

          return; /* already been there... */
        }
    }

  ofs = _vcd_malloc (sizeof (vcdinfo_offset_t));

  ofs->offset = offset;
  ofs->lid = lid;
  ofs->in_lot = in_lot;
  ofs->type = obj->psd[_rofs];

  switch (ofs->type)
    {
    case PSD_TYPE_PLAY_LIST:
      _vcd_list_append (obj->offset_list, ofs);
      {
        const PsdPlayListDescriptor *d = 
          (const void *) (obj->psd + _rofs);

        if (!ofs->lid)
          ofs->lid = uint16_from_be (d->lid) & 0x7fff;
        else 
          if (ofs->lid != (uint16_from_be (d->lid) & 0x7fff))
            vcd_warn ("LOT entry assigned LID %d, but descriptor has LID %d",
                      ofs->lid, uint16_from_be (d->lid) & 0x7fff);

        _visit_pbc (obj, 0, vcdinf_get_prev_from_pld(d), false);
        _visit_pbc (obj, 0, vcdinf_get_next_from_pld(d), false);
        _visit_pbc (obj, 0, vcdinf_get_return_from_pld(d), false);
      }
      break;

    case PSD_TYPE_EXT_SELECTION_LIST:
      
    case PSD_TYPE_SELECTION_LIST:
      _vcd_list_append (obj->offset_list, ofs);
      {
        const PsdSelectionListDescriptor *d =
          (const void *) (obj->psd + _rofs);

        int idx;

        if (!ofs->lid)
          ofs->lid = uint16_from_be (d->lid) & 0x7fff;
        else 
          if (ofs->lid != (uint16_from_be (d->lid) & 0x7fff))
            vcd_warn ("LOT entry assigned LID %d, but descriptor has LID %d",
                      ofs->lid, uint16_from_be (d->lid) & 0x7fff);

        _visit_pbc (obj, 0, vcdinf_get_prev_from_psd(d), false);
        _visit_pbc (obj, 0, vcdinf_get_next_from_psd(d), false);
        _visit_pbc (obj, 0, vcdinf_get_return_from_psd(d), false);
        _visit_pbc (obj, 0, vcdinf_get_default_from_psd(d), false);
        _visit_pbc (obj, 0, uint16_from_be (d->timeout_ofs), false);

        for (idx = 0; idx < d->nos; idx++)
          _visit_pbc (obj, 0, uint16_from_be (d->ofs[idx]), false);
        
      }
      break;

    case PSD_TYPE_END_LIST:
      _vcd_list_append (obj->offset_list, ofs);
      break;

    default:
      vcd_warn ("corrupt PSD???????");
      free (ofs);
      return;
      break;
    }
}

static void
_visit_lot (struct _pbc_ctx *obj)
{
  const LotVcd *lot = obj->lot;
  unsigned n, tmp;

  for (n = 0; n < LOT_VCD_OFFSETS; n++)
    if ((tmp = uint16_from_be (lot->offset[n])) != PSD_OFS_DISABLED)
      _visit_pbc (obj, n + 1, tmp, true);

  _vcd_list_sort (obj->offset_list, (_vcd_list_cmp_func) vcdinf_lid_t_cmp);
}

static int
_parse_pbc (struct vcdxml_t *obj, VcdImageSource *img, bool no_ext_psd)
{
  int n;
  struct _pbc_ctx _pctx;
  VcdListNode *node;
  bool extended = false;
  uint32_t _lot_vcd_sector = -1;
  uint32_t _psd_vcd_sector = -1;
  unsigned _psd_size = -1;
  vcd_image_stat_t statbuf;

  if (!obj->info.psd_size)
    {
      vcd_debug ("No PBC info");
      return 0;
    }

  if (obj->vcd_type == VCD_TYPE_VCD2)
    {
      if (!vcd_image_source_fs_stat (img, "EXT/LOT_X.VCD;1", &statbuf))
	{
	  extended = true;
	  _lot_vcd_sector = statbuf.lsn;
	  vcd_assert (statbuf.size == ISO_BLOCKSIZE * LOT_VCD_SIZE);
	}  

      if (extended &&
	  !vcd_image_source_fs_stat (img, "EXT/PSD_X.VCD;1", &statbuf))
	{
	  _psd_vcd_sector = statbuf.lsn;
	  _psd_size = statbuf.size;
	}
      else
	extended = false;
    }

  _pctx.extended = extended;

  if (extended && !no_ext_psd)
    vcd_info ("detected extended VCD2.0 PBC files");
  else
    {
      if (extended)
	vcd_info ("ignoring detected extended VCD2.0 PBC files");

      _pctx.extended = false;

      _lot_vcd_sector = LOT_VCD_SECTOR;
      _psd_vcd_sector = PSD_VCD_SECTOR;
      _psd_size = obj->info.psd_size;
    }

  _pctx.psd_size = _psd_size;
  _pctx.offset_mult = 8;
  _pctx.maximum_lid = obj->info.max_lid;

  _pctx.offset_list = _vcd_list_new ();

  /* read in LOT */
  
  _pctx.lot = _vcd_malloc (ISO_BLOCKSIZE * LOT_VCD_SIZE);
  
  if (vcd_image_source_read_mode2_sectors (img, _pctx.lot, _lot_vcd_sector, false, LOT_VCD_SIZE))
    return -1;

  /* read in PSD */

  n = _vcd_len2blocks (_psd_size, ISO_BLOCKSIZE);

  _pctx.psd = _vcd_malloc (ISO_BLOCKSIZE * n);

  if (vcd_image_source_read_mode2_sectors (img, _pctx.psd,
					   _psd_vcd_sector, false, n))
    return -1;

  /* traverse it */

  _visit_lot (&_pctx);

  _VCD_LIST_FOREACH (node, _pctx.offset_list)
    {
      vcdinfo_offset_t *ofs = _vcd_list_node_data (node);
      pbc_t *_pbc;

      vcd_assert (ofs->offset != PSD_OFS_DISABLED);
      
      if ((_pbc = _pbc_node_read (&_pctx, ofs->offset)))
	_vcd_list_append (obj->pbc_list, _pbc);
    }

  return 0;
}

static int
_rip_isofs (struct vcdxml_t *obj, VcdImageSource *img)
{
  VcdListNode *node;
  
  _VCD_LIST_FOREACH (node, obj->filesystem)
    {
      struct filesystem_t *_fs = _vcd_list_node_data (node);
      int idx;
      FILE *outfd;
      const int blocksize = _fs->file_raw ? M2RAW_SECTOR_SIZE : ISO_BLOCKSIZE;

      if (!_fs->file_src)
	continue;

      vcd_info ("extracting %s to %s (lsn %d, size %d, raw %d)", 
		_fs->name, _fs->file_src,
		_fs->lsn, _fs->size, _fs->file_raw);

      if (!(outfd = fopen (_fs->file_src, "wb")))
        {
          perror ("fopen()");
          exit (EXIT_FAILURE);
        }

      for (idx = 0; idx < _fs->size; idx += blocksize)
	{
	  char buf[blocksize];
	  /* some EDG based compilers segfault on the commented out code */
	  /* memset (buf, 0, sizeof (buf)); */
	  memset (buf, 0, blocksize);

	  vcd_image_source_read_mode2_sector (img, buf, 
					      _fs->lsn + (idx / blocksize),
					      _fs->file_raw);

	  fwrite (buf, blocksize, 1, outfd);

	  if (ferror (outfd))
	    {
	      perror ("fwrite()");
	      exit (EXIT_FAILURE);
	    }
	}

      fflush (outfd);
      if (ftruncate (fileno (outfd), _fs->size))
	perror ("ftruncate()");

      fclose (outfd);
      
    }

  return 0;
}

static int
_rip_segments (struct vcdxml_t *obj, VcdImageSource *img)
{
  VcdListNode *node;
  uint32_t start_extent;

  start_extent = obj->info.segments_start;

  vcd_assert (start_extent % CD_FRAMES_PER_SECOND == 0);

  _VCD_LIST_FOREACH (node, obj->segment_list)
    {
      struct segment_t *_seg = _vcd_list_node_data (node);
      uint32_t n;
      FILE *outfd = NULL;
      VcdMpegStreamCtx mpeg_ctx;
      double last_pts = 0;

      vcd_assert (_seg->segments_count > 0);

      memset (&mpeg_ctx, 0, sizeof (VcdMpegStreamCtx));

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
            uint8_t data[M2F2_SECTOR_SIZE];
            uint8_t spare[4];
          }
          buf;

	  memset (&buf, 0, sizeof (buf));
	  vcd_image_source_read_mode2_sector (img, &buf, start_extent + n,
					       true);
	  
	  if (!buf.subheader[0] 
              && !buf.subheader[1]
              && (buf.subheader[2] | SM_FORM2) == SM_FORM2
              && !buf.subheader[3])
            {
              vcd_warn ("no EOF seen, but stream ended");
              break;
            }

	  vcd_mpeg_parse_packet (buf.data, M2F2_SECTOR_SIZE, false, &mpeg_ctx);

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

	      _vcd_list_append (_seg->autopause_list, _ap_ts);
	    }

	  fwrite (buf.data, M2F2_SECTOR_SIZE, 1, outfd);

	  if (ferror (outfd))
            {
              perror ("fwrite()");
              exit (EXIT_FAILURE);
            }

	  if (buf.subheader[2] & SM_EOF)
            break;
	}

      fclose (outfd);

      start_extent += _seg->segments_count * 150;
    }

  return 0;
}

static int
_rip_sequences (struct vcdxml_t *obj, VcdImageSource *img, int tracknum)
{
  VcdListNode *node;
  int counter=1;

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

      _read_progress_t _progress;

      struct m2f2sector
      {
	uint8_t subheader[8];
	uint8_t data[M2F2_SECTOR_SIZE];
	uint8_t spare[4];
      }
      buf[15];

      if (tracknum && tracknum!=counter++) {
	vcd_info("Track %d selected, skipping track %d", tracknum,counter-1);
	continue;
      }
	  
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

      _progress.total = end_lsn;

      for (n = start_lsn; n < end_lsn; n++)
	{
	  const int buf_idx = (n - start_lsn) % 15;

	  if (n - _progress.done > (end_lsn / 100))
	    {
	      _progress.done = n;
	      vcd_xml_read_progress_cb (&_progress, _seq->src);
	    }

	  if (!buf_idx)
	    {
	      const int secs_left = end_lsn - n;

	      memset (buf, 0, sizeof (buf));
	      vcd_image_source_read_mode2_sectors (img, buf, n, true, 
						   (secs_left > 15 
						    ? 15 : secs_left));
	    }

	  if (_nseq && n + 150 == end_lsn + 1)
	    vcd_warn ("reading into gap @%d... :-(", n);

	  if (!(buf[buf_idx].subheader[2] & SM_FORM2))
	    {
	      vcd_warn ("encountered non-form2 sector -- leaving loop");
	      break;
	    }
	  
	  if (in_data)
	    { /* end conditions... */
	      if (!buf[buf_idx].subheader[0])
		{
		  vcd_debug ("fn -edge @%d", n);
		  break;
		}

	      if (!(buf[buf_idx].subheader[2] & SM_REALT))
		{
		  vcd_debug ("subheader: no realtime data anymore @%d", n);
		  break;
		}
	    }

	  if (buf[buf_idx].subheader[1] && !in_data)
	    {
	      vcd_debug ("cn +edge @%d", n);
	      in_data = true;
	    }


#if defined(DEBUG)
	  if (!in_data)
	    vcd_debug ("%2.2x %2.2x %2.2x %2.2x",
		       buf[buf_idx].subheader[0],
		       buf[buf_idx].subheader[1],
		       buf[buf_idx].subheader[2],
		       buf[buf_idx].subheader[3]);
#endif

	  if (in_data)
	    {
	      VcdListNode *_node;

	      vcd_mpeg_parse_packet (buf[buf_idx].data, M2F2_SECTOR_SIZE, 
				     false, &mpeg_ctx);

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

	      if (buf[buf_idx].subheader[2] & SM_TRIG)
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
		{
		  fwrite (buf[buf_idx].data, M2F2_SECTOR_SIZE, 1, outfd);

		  if (ferror (outfd))
		    {
		      perror ("fwrite()");
		      exit (EXIT_FAILURE);
		    }
		}

	    } /* if (in_data) */
	  
	  if (buf[buf_idx].subheader[2] & SM_EOF)
	    {
	      vcd_debug ("encountered subheader EOF @%d", n);
	      break;
	    }
	} /* for */
      
      _progress.done = _progress.total;
      vcd_xml_read_progress_cb (&_progress, _seq->src);

      if (in_data)
	{
	  uint32_t length;

	  if (n == end_lsn)
	    vcd_debug ("stream till end of track");
	  
	  length = (1 + last_nonzero) - first_data;

	  vcd_debug ("truncating file to %d packets", length);

	  fflush (outfd);
	  if (ftruncate (fileno (outfd), length * M2F2_SECTOR_SIZE))
	    perror ("ftruncate()");
	}

      fclose (outfd);
    } /* _VCD_LIST_FOREACH */

  return 0;
}

#define DEFAULT_XML_FNAME      "videocd.xml"
#define DEFAULT_IMG_FNAME      "videocd.bin"

int
main (int argc, const char *argv[])
{
  VcdImageSource *img_src = NULL;
  struct vcdxml_t obj;

  /* cl params */
  char *xml_fname = NULL;
  char *img_fname = NULL;
  int norip_flag = 0;
  int nofile_flag = 0;
  int noseq_flag = 0;
  int noseg_flag = 0;
  int no_ext_psd_flag = 0;
  int sector_2336_flag = 0;
  int _progress_flag = 0;
  int _gui_flag = 0;
  int _track_flag=0;

  enum { 
    CL_SOURCE_UNDEF = VCDINFO_SOURCE_UNDEF,
    CL_SOURCE_AUTO  = VCDINFO_SOURCE_AUTO,
    CL_SOURCE_BIN   = VCDINFO_SOURCE_BIN,
    CL_SOURCE_CDROM = VCDINFO_SOURCE_DEVICE,
    CL_SOURCE_NRG   = VCDINFO_SOURCE_NRG,
    CL_VERSION      = 20
  } _img_type = CL_SOURCE_UNDEF;

  vcd_xml_progname = "vcdxrip";

  vcd_xml_init (&obj);

  vcd_xml_log_init ();

  obj.comment = vcd_xml_dump_cl_comment (argc, argv);

  {
    poptContext optCon = NULL;
    int opt;

    struct poptOption optionsTable[] = {
      {"output-file", 'o', POPT_ARG_STRING, &xml_fname, 0,
       "specify xml file for output (default: '" DEFAULT_XML_FNAME "')",
       "FILE"},

      {"bin-file", 'b', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, &img_fname, 
       CL_SOURCE_BIN,
       "set image file as source (default: '" DEFAULT_IMG_FNAME "')", 
       "FILE"},

      {"sector-2336", '\0', POPT_ARG_NONE, &sector_2336_flag, 0,
       "use 2336 byte sector mode for image file"},

      {"cdrom-device", 'C', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, &img_fname,
       CL_SOURCE_CDROM,"set CDROM device as source", "DEVICE"},

      {"input", 'i', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, &img_fname, 
       CL_SOURCE_AUTO, 
       "set source and determine if \"bin\" image or device", "FILE"},

      {"nrg-file", 'N', POPT_ARG_STRING|POPT_ARGFLAG_OPTIONAL, &img_fname, 
       CL_SOURCE_NRG, "set Nero CD-ROM disk image image file as source",
       "FILE"},

      {"no-ext-psd", '\0', POPT_ARG_NONE, &no_ext_psd_flag, 0,
       "ignore /EXT/PSD_X.VCD"},

      {"norip", '\0', POPT_ARG_NONE, &norip_flag, 0,
       "only extract XML structure"},

      {"nofiles", '\0', POPT_ARG_NONE, &nofile_flag, 0,
       "don't extract files"},

      {"nosequences", '\0', POPT_ARG_NONE, &noseq_flag, 0,
       "don't extract sequences"},

      {"nosegments", '\0', POPT_ARG_NONE, &noseg_flag, 0,
       "don't extract segment play items"},

      {"progress", 'p', POPT_ARG_NONE, &_progress_flag, 0,  
       "show progress"}, 

      {"verbose", 'v', POPT_ARG_NONE, &_verbose_flag, 0, 
       "be verbose"},
    
      {"quiet", 'q', POPT_ARG_NONE, &_quiet_flag, 0, 
       "show only critical messages"},

      {"gui", '\0', POPT_ARG_NONE, &_gui_flag, 0, "enable GUI mode"},

      {"version", 'V', POPT_ARG_NONE, NULL, CL_VERSION,
       "display version and copyright information and exit"},
	   
      { "track", 't', POPT_ARG_INT, &_track_flag, 0,
	"rip only this track"},
      
      POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
    };

    optCon = poptGetContext ("vcdimager", argc, argv, optionsTable, 0);
      
    if (poptReadDefaultConfig (optCon, 0)) 
      vcd_warn ("reading popt configuration failed"); 

    while ((opt = poptGetNextOpt (optCon)) != -1)
      switch (opt)
	{
	case CL_VERSION:
          vcd_xml_gui_mode = _gui_flag;
          vcd_xml_print_version ();
	  exit (EXIT_SUCCESS);
	  break;

	case CL_SOURCE_AUTO:
	case CL_SOURCE_NRG:
	case CL_SOURCE_BIN:
	case CL_SOURCE_CDROM:
	  if (_img_type)
	    {
	      vcd_error ("Only one image (type) supported at once - try --help");
	      exit (EXIT_FAILURE);
	    }
	  _img_type = opt;
	  break;

	default:
	  fprintf (stderr, "%s: %s\n", 
		   poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
		   poptStrerror(opt));
	  fprintf (stderr, "error while parsing command line - try --help\n");
	  exit (EXIT_FAILURE);
	  break;
      }

    if (_verbose_flag && _quiet_flag)
      vcd_error ("I can't be both, quiet and verbose... either one or another ;-)");
    
    if (poptGetArgs (optCon) != NULL)
      vcd_error ("Why are you giving me non-option arguments? -- try --help");

    poptFreeContext (optCon);
  }

  if (_quiet_flag)
    vcd_xml_verbosity = LOG_WARN;
  else if (_verbose_flag)
    vcd_xml_verbosity = LOG_DEBUG;
  else
    vcd_xml_verbosity = LOG_INFO;

  if (_gui_flag)
    vcd_xml_gui_mode = true;

  if (_progress_flag)
    vcd_xml_show_progress = true;

  if (!xml_fname)
    xml_fname = strdup (DEFAULT_XML_FNAME);

  if (CL_SOURCE_UNDEF == _img_type) 
    _img_type = CL_SOURCE_AUTO;

  if (!vcdinf_open(&img_src, img_fname, _img_type, NULL)) {
    vcd_error ("Error determining place to read from.");
    exit (EXIT_FAILURE);
  }
  
  vcd_assert (img_src != NULL);

  /* start with ISO9660 PVD */
  _parse_pvd (&obj, img_src);

  _parse_isofs (&obj, img_src); 
  
  /* needs to be parsed in order */
  _parse_info (&obj, img_src);
  _parse_entries (&obj, img_src);

  /* needs to be parsed last! */
  _parse_pbc (&obj, img_src, no_ext_psd_flag);

  if (norip_flag || noseq_flag || noseg_flag || _track_flag)
    vcd_warn ("Some entry point and auto pause locations might not be "
	      "checked.");

  if (!norip_flag)
    {
      if (!nofile_flag)
	_rip_isofs (&obj, img_src);

      if (!noseg_flag)
	_rip_segments (&obj, img_src);

      if (!noseq_flag)
	_rip_sequences (&obj, img_src, _track_flag);
    }

  vcd_info ("Writing XML description to `%s'...", xml_fname);
  vcd_xml_dump (&obj, xml_fname);

  vcd_image_source_destroy (img_src);

  vcd_info ("done");
  return 0;
}
