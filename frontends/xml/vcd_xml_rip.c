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

#include "vcdxml.h"
#include "vcd_xml_dtd.h"
#include "vcd_xml_dump.h"


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
      obj->class = strdup ("vcd");
      obj->version = strdup ("1.1");
      break;
    case VCD_TYPE_VCD2:
      vcd_debug ("VCD 2.0 detected");
      obj->class = strdup ("vcd");
      obj->version = strdup ("2.0");
      break;
    case VCD_TYPE_SVCD:
      vcd_debug ("SVCD detected");
      obj->class = strdup ("svcd");
      obj->version = strdup ("1.0");
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

	    snprintf (buf, sizeof (buf), "segment-%3.3d", n);
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

	  snprintf (buf, sizeof (buf), "entry-%3.3d", idx - track - 1);
	  _entry->id = strdup (buf);

	  _entry->extent = extent;

	  _vcd_list_append (_seq->entry_point_list, _entry);
	}

      vcd_debug ("%d %d %d %d", idx, track, extent, newtrack);
    }

  return 0;
}

int main (int argc, const char *argv[])
{
  VcdDataSource *bin_source;
  VcdImageSource *img_src;
  struct vcdxml_t obj;

  memset (&obj, 0, sizeof (struct vcdxml_t));

  obj.segment_list = _vcd_list_new ();
  obj.sequence_list = _vcd_list_new ();
  obj.pbc_list = _vcd_list_new ();
  obj.filesystem = _vcd_list_new ();

  bin_source = vcd_data_source_new_stdio (argc == 2 ? argv[1] : "videocd.bin");
  assert (bin_source != NULL);

  img_src = vcd_image_source_new_bincue (bin_source, NULL, false);
  assert (img_src != NULL);

  /* start with ISO9660 PVD */
  _parse_pvd (&obj, img_src);

  /* needs to be parsed in order */
  _parse_info (&obj, img_src);
  _parse_entries (&obj, img_src);

  vcd_xml_dump (&obj, "videocd.xml");

  vcd_image_source_destroy (img_src);

  return 1;
}
