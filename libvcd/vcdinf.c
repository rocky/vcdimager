/*
    $Id$

    Copyright (C) 2003 Rocky Bernstein <rocky@panix.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Foundation
    Software, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/* 
   Like vcdinfo but exposes more of the internal structure. It is probably
   better to use vcdinfo, when possible.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stddef.h>
#include <errno.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_types.h>
#include <libvcd/vcd_files.h>
#include <libvcd/vcd_files_private.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_iso9660_private.h>
#include <libvcd/vcd_logging.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_salloc.h>
#include <libvcd/vcd_stream_stdio.h>
#include <libvcd/vcd_image_bincue.h>
#include <libvcd/vcd_image_nrg.h>
#include <libvcd/vcd_image_cd.h>
#include <libvcd/vcd_image_fs.h>
#include <libvcd/vcd_xa.h>
#include <libvcd/vcd_cd_sector.h>

/* Eventually move above libvcd includes but having vcdinfo including. */
#include "vcdinfo.h"
#include "vcdinf.h"

static const char _rcsid[] = "$Id$";

/*!
   Return a string containing the VCD album id, or NULL if there is 
   some problem in getting this. 
*/
const char *
vcdinf_get_album_id(const InfoVcd *info)
{
  return vcdinfo_strip_trail (info->album_desc, MAX_ALBUM_LEN);
}

/*!
  Return the VCD ID.
  NULL is returned if there is some problem in getting this. 
*/
const char * 
vcdinf_get_application_id(const pvd_t *pvd)
{
  return(vcdinfo_strip_trail(pvd->application_id, MAX_APPLICATION_ID));
}

/*!  Return the starting LBA (logical block address) for sequence
  entry_num in obj.  VCDINFO_NULL_LBA is returned if there is no entry.
*/
lba_t
vcdinf_get_entry_lba(const EntriesVcd *entries, unsigned int entry_num)
{
  const msf_t *msf = vcdinf_get_entry_msf(entries, entry_num);
  return (msf != NULL) ? msf_to_lba(msf) : VCDINFO_NULL_LBA;
}

/*!  Return the starting MSF (minutes/secs/frames) for sequence
  entry_num in obj.  NULL is returned if there is no entry.
  The first entry number is 0.
*/
const msf_t *
vcdinf_get_entry_msf(const EntriesVcd *entries, unsigned int entry_num)
{
  const unsigned int entry_count = vcdinf_get_num_entries(entries);
  return entry_num < entry_count ?
    &(entries->entry[entry_num].msf)
    : NULL;
}

/*!
   Return a string giving VCD format (VCD 1.0 VCD 1.1, SVCD, ... 
   for this object.
*/
const char *
vcdinf_get_format_version_str (vcd_type_t vcd_type) 
{
  switch (vcd_type)
    {
    case VCD_TYPE_VCD:
      return ("VCD 1.0");
      break;
    case VCD_TYPE_VCD11:
      return ("VCD 1.1");
      break;
    case VCD_TYPE_VCD2:
      return ("VCD 2.0");
      break;
    case VCD_TYPE_SVCD:
      return ("SVCD");
      break;
    case VCD_TYPE_HQVCD:
      return ("HQVCD");
      break;
    case VCD_TYPE_INVALID:
      return ("INVALID");
      break;
    default:
      return ( "????");
    }
}

/*!
  Return the number of segments in the VCD. 
*/
unsigned int
vcdinf_get_num_entries(const EntriesVcd *entries)
{
  return (uint16_from_be (entries->entry_count));
}

/*!
  Return number of LIDs. 
*/
lid_t
vcdinf_get_num_LIDs (const InfoVcd *info) 
{
  /* Should probably use _vcd_pbc_max_lid instead? */
  return uint16_from_be (info->lot_entries);
}

/*!
  Return the number of segments in the VCD. 
*/
unsigned int
vcdinf_get_num_segments(const InfoVcd *info)
{
  return (uint16_from_be (info->item_count));
}

/*!
   Return a string containing the VCD publisher id with trailing
   blanks removed.
*/
const char *
vcdinf_get_preparer_id(const pvd_t *pvd)
{
  return(vcdinfo_strip_trail(pvd->preparer_id, MAX_PREPARER_ID));
}

/*!
  Return number of bytes in PSD. 
*/
uint32_t
vcdinf_get_psd_size (const InfoVcd *info)
{
  return uint32_from_be (info->psd_size);
}

/*!
   Return a string containing the VCD publisher id with trailing
   blanks removed.
*/
const char *
vcdinf_get_publisher_id(const pvd_t *pvd)
{
  return(vcdinfo_strip_trail(pvd->publisher_id, MAX_PUBLISHER_ID));
}

/*!
   Return a string containing the VCD system id with trailing
   blanks removed.
*/
const char *
vcdinf_get_system_id(const pvd_t *pvd)
{
  return(vcdinfo_strip_trail(pvd->system_id, MAX_SYSTEM_ID));
}

/*!
  Return the track number for entry n in obj. The first track starts
  at 1. 
*/
track_t
vcdinf_get_track(const EntriesVcd *entries, const unsigned int entry_num)
{
  const unsigned int entry_count = vcdinf_get_num_entries(entries);
  /* Note entry_num is 0 origin. */
  return entry_num < entry_count ?
    from_bcd8 (entries->entry[entry_num].n): VCDINFO_INVALID_TRACK;
}

/*!
  Return the VCD volume count - the number of CD's in the collection.
*/
unsigned int 
vcdinf_get_volume_count(const InfoVcd *info) 
{
  return(uint16_from_be( info->vol_count));
}

/*!
  Return the VCD ID.
*/
const char *
vcdinf_get_volume_id(const pvd_t *pvd) 
{
  return(vcdinfo_strip_trail(pvd->volume_id, MAX_VOLUME_ID));
}

/*!
  Return the VCD volume num - the number of the CD in the collection.
  This is a number between 1 and the volume count.
*/
unsigned int
vcdinf_get_volume_num(const InfoVcd *info)
{
  return uint16_from_be(info->vol_id);
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
