/*
    $Id$

    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>

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

#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <libvcd/vcd_assert.h>

#include "vcd_iso9660.h"
#include "vcd_iso9660_private.h"

#include "vcd_bytesex.h"
#include "vcd_util.h"

static const char _rcsid[] = "$Id$";

/* some parameters... */
#define SYSTEM_ID         "CD-RTOS CD-BRIDGE"
#define VOLUME_SET_ID     ""

static void
_idr_set_time (uint8_t _idr_date[], const struct tm *_tm)
{
  memset (_idr_date, 0, 7);

  if (!_tm)
    return;

  _idr_date[0] = _tm->tm_year;
  _idr_date[1] = _tm->tm_mon + 1;
  _idr_date[2] = _tm->tm_mday;
  _idr_date[3] = _tm->tm_hour;
  _idr_date[4] = _tm->tm_min;
  _idr_date[5] = _tm->tm_sec;
  _idr_date[6] = 0x00; /* tz, GMT -48 +52 in 15min intervals */
}

static void
_pvd_set_time (char _pvd_date[], const struct tm *_tm)
{
  memset (_pvd_date, '0', 16);
  _pvd_date[16] = (int8_t) 0; /* tz */

  if (!_tm)
    return;

  snprintf(_pvd_date, 17, 
           "%4.4d%2.2d%2.2d" "%2.2d%2.2d%2.2d" "%2.2d",
           _tm->tm_year + 1900, _tm->tm_mon + 1, _tm->tm_mday,
           _tm->tm_hour, _tm->tm_min, _tm->tm_sec,
           0 /* 1/100 secs */ );
  
  _pvd_date[16] = (int8_t) 0; /* tz */
}

static void
pathtable_get_size_and_entries(const void *pt, unsigned *size, 
                               unsigned *entries);

void 
set_iso_evd(void *pd)
{
  struct iso_volume_descriptor ied;

  vcd_assert (sizeof(struct iso_volume_descriptor) == ISO_BLOCKSIZE);

  vcd_assert (pd != NULL);
  
  memset(&ied, 0, sizeof(ied));

  ied.type = to_711(ISO_VD_END);
  _vcd_strncpy_pad (ied.id, ISO_STANDARD_ID, sizeof(ied.id), VCD_DCHARS);
  ied.version = to_711(ISO_VERSION);

  memcpy(pd, &ied, sizeof(ied));
}

/* important date to celebrate (for me at least =)
   -- until user customization is implemented... */
static const time_t _vcd_time = 269222400L;
                                       
void
set_iso_pvd(void *pd,
            const char volume_id[],
            const char publisher_id[],
            const char preparer_id[],
            const char application_id[],
            uint32_t iso_size,
            const void *root_dir,
            uint32_t path_table_l_extent,
            uint32_t path_table_m_extent,
            uint32_t path_table_size)
{
  struct iso_primary_descriptor ipd;

  vcd_assert (sizeof(struct iso_primary_descriptor) == ISO_BLOCKSIZE);

  vcd_assert (pd != NULL);
  vcd_assert (volume_id != NULL);
  vcd_assert (application_id != NULL);

  memset(&ipd,0,sizeof(ipd)); /* paranoia? */

  /* magic stuff ... thatis CD XA marker... */
  strcpy(((char*)&ipd)+ISO_XA_MARKER_OFFSET, ISO_XA_MARKER_STRING);

  ipd.type = to_711(ISO_VD_PRIMARY);
  _vcd_strncpy_pad (ipd.id, ISO_STANDARD_ID, 5, VCD_DCHARS);
  ipd.version = to_711(ISO_VERSION);

  _vcd_strncpy_pad (ipd.system_id, SYSTEM_ID, 32, VCD_ACHARS);
  _vcd_strncpy_pad (ipd.volume_id, volume_id, 32, VCD_DCHARS);

  ipd.volume_space_size = to_733(iso_size);

  ipd.volume_set_size = to_723(1);
  ipd.volume_sequence_number = to_723(1);
  ipd.logical_block_size = to_723(ISO_BLOCKSIZE);

  ipd.path_table_size = to_733(path_table_size);
  ipd.type_l_path_table = to_731(path_table_l_extent); 
  ipd.type_m_path_table = to_732(path_table_m_extent); 
  
  vcd_assert (sizeof(ipd.root_directory_record) == 34);
  memcpy(ipd.root_directory_record, root_dir, sizeof(ipd.root_directory_record));
  ipd.root_directory_record[0] = 34;

  _vcd_strncpy_pad (ipd.volume_set_id, VOLUME_SET_ID, 128, VCD_DCHARS);

  _vcd_strncpy_pad (ipd.publisher_id, publisher_id, 128, VCD_ACHARS);
  _vcd_strncpy_pad (ipd.preparer_id, preparer_id, 128, VCD_ACHARS);
  _vcd_strncpy_pad (ipd.application_id, application_id, 128, VCD_ACHARS); 

  _vcd_strncpy_pad (ipd.copyright_file_id    , "", 37, VCD_DCHARS);
  _vcd_strncpy_pad (ipd.abstract_file_id     , "", 37, VCD_DCHARS);
  _vcd_strncpy_pad (ipd.bibliographic_file_id, "", 37, VCD_DCHARS);

  _pvd_set_time (ipd.creation_date, gmtime (&_vcd_time));
  _pvd_set_time (ipd.modification_date, gmtime (&_vcd_time));
  _pvd_set_time (ipd.expiration_date, NULL);
  _pvd_set_time (ipd.effective_date, NULL);

  ipd.file_structure_version = to_711(1);

  /* we leave ipd.application_data = 0 */

  memcpy(pd, &ipd, sizeof(ipd)); /* copy stuff to arg ptr */
}

unsigned
dir_calc_record_size(unsigned namelen, 
                     unsigned su_len)
{
  unsigned length;

  length = sizeof(struct iso_directory_record);
  length += namelen;
  if (length % 2) /* pad to word boundary */
    length++;
  length += su_len;
  if (length % 2) /* pad to word boundary again */
    length++;

  return length;
}

void
dir_add_entry_su(void *dir,
                 const char name[], 
                 uint32_t extent,
                 uint32_t size,
                 uint8_t flags,
                 const void *su_data,
                 unsigned su_size)
{
  struct iso_directory_record *idr = dir;
  uint8_t *dir8 = dir;
  unsigned offset = 0;
  uint32_t dsize = from_733(idr->size);
  int length, su_offset;

  vcd_assert (sizeof(struct iso_directory_record) == 33);

  if (!dsize && !idr->length)
    dsize = ISO_BLOCKSIZE; /* for when dir lacks '.' entry */
  
  vcd_assert (dsize > 0 && !(dsize % ISO_BLOCKSIZE));
  vcd_assert (dir != NULL);
  vcd_assert (extent > 17);
  vcd_assert (name != NULL);
  vcd_assert (strlen(name) <= MAX_ISOPATHNAME);

  length = sizeof(struct iso_directory_record);
  length += strlen(name);
  length = _vcd_ceil2block (length, 2); /* pad to word boundary */
  su_offset = length;
  length += su_size;
  length = _vcd_ceil2block (length, 2); /* pad to word boundary again */

  /* find the last entry's end */
  {
    unsigned ofs_last_rec = 0;

    offset = 0;
    while (offset < dsize)
      {
        if (!dir8[offset])
          {
            offset++;
            continue;
          }

        offset += dir8[offset];
        ofs_last_rec = offset;
      }

    vcd_assert (offset == dsize);

    offset = ofs_last_rec;
  }

  /* be sure we don't cross sectors boundaries */
  offset = _vcd_ofs_add (offset, length, ISO_BLOCKSIZE);
  offset -= length;

  vcd_assert (offset + length <= dsize);

  idr = (struct iso_directory_record*) &dir8[offset];
  
  vcd_assert (offset+length < dsize); 
  
  memset(idr, 0, length);

  idr->length = to_711(length);
  idr->extent = to_733(extent);
  idr->size = to_733(size);
  
  _idr_set_time (idr->date, gmtime (&_vcd_time));
  
  idr->flags = to_711(flags);

  idr->volume_sequence_number = to_723(1);

  idr->name_len = to_711(strlen(name) ? strlen(name) : 1); /* working hack! */

  memcpy(idr->name, name, from_711(idr->name_len));
  memcpy(&dir8[offset] + su_offset, su_data, su_size);
}

void
dir_init_new (void *dir,
              uint32_t self,
              uint32_t ssize,
              uint32_t parent,
              uint32_t psize)
{
  dir_init_new_su (dir, self, ssize, NULL, 0, parent, psize, NULL, 0);
}

void dir_init_new_su (void *dir,
                      uint32_t self,
                      uint32_t ssize,
                      const void *ssu_data,
                      unsigned ssu_size,
                      uint32_t parent,
                      uint32_t psize,
                      const void *psu_data,
                      unsigned psu_size)
{
  vcd_assert (ssize > 0 && !(ssize % ISO_BLOCKSIZE));
  vcd_assert (psize > 0 && !(psize % ISO_BLOCKSIZE));
  vcd_assert (dir != NULL);

  memset (dir, 0, ssize);

  /* "\0" -- working hack due to padding  */
  dir_add_entry_su (dir, "\0", self, ssize, ISO_DIRECTORY, ssu_data, ssu_size); 

  dir_add_entry_su (dir, "\1", parent, psize, ISO_DIRECTORY, psu_data, psu_size);
}

void 
pathtable_init (void *pt)
{
  vcd_assert (sizeof (struct iso_path_table) == 8);

  vcd_assert (pt != NULL);
  
  memset (pt, 0, ISO_BLOCKSIZE); /* fixme */
}

static const struct iso_path_table*
pathtable_get_entry (const void *pt, unsigned entrynum)
{
  const uint8_t *tmp = pt;
  unsigned offset = 0;
  unsigned count = 0;

  vcd_assert (pt != NULL);

  while (from_711 (*tmp)) 
    {
      if (count == entrynum)
        break;

      vcd_assert (count < entrynum);

      offset += sizeof (struct iso_path_table);
      offset += from_711 (*tmp);
      if (offset % 2)
        offset++;
      tmp = (uint8_t *)pt + offset;
      count++;
    }

  if (!from_711 (*tmp))
    return NULL;

  return (const void *) tmp;
}

void
pathtable_get_size_and_entries (const void *pt, 
                                unsigned *size,
                                unsigned *entries)
{
  const uint8_t *tmp = pt;
  unsigned offset = 0;
  unsigned count = 0;

  vcd_assert (pt != NULL);

  while (from_711 (*tmp)) 
    {
      offset += sizeof (struct iso_path_table);
      offset += from_711 (*tmp);
      if (offset % 2)
        offset++;
      tmp = (uint8_t *)pt + offset;
      count++;
    }

  if (size)
    *size = offset;

  if (entries)
    *entries = count;
}

unsigned
pathtable_get_size (const void *pt)
{
  unsigned size = 0;
  pathtable_get_size_and_entries (pt, &size, NULL);
  return size;
}

uint16_t 
pathtable_l_add_entry (void *pt, 
                       const char name[], 
                       uint32_t extent, 
                       uint16_t parent)
{
  struct iso_path_table *ipt = 
    (struct iso_path_table*)((char *)pt + pathtable_get_size (pt));
  size_t name_len = strlen (name) ? strlen (name) : 1;
  unsigned entrynum = 0;

  vcd_assert (pathtable_get_size (pt) < ISO_BLOCKSIZE); /*fixme */

  memset (ipt, 0, sizeof (struct iso_path_table) + name_len); /* paranoia */

  ipt->name_len = to_711 (name_len);
  ipt->extent = to_731 (extent);
  ipt->parent = to_721 (parent);
  memcpy (ipt->name, name, name_len);

  pathtable_get_size_and_entries (pt, NULL, &entrynum);

  if (entrynum > 1)
    {
      const struct iso_path_table *ipt2 
        = pathtable_get_entry (pt, entrynum - 2);

      vcd_assert (ipt2 != NULL);

      vcd_assert (from_721 (ipt2->parent) <= parent);
    }
  
  return entrynum;
}

uint16_t 
pathtable_m_add_entry (void *pt, 
                       const char name[], 
                       uint32_t extent, 
                       uint16_t parent)
{
  struct iso_path_table *ipt =
    (struct iso_path_table*)((char *)pt + pathtable_get_size (pt));
  size_t name_len = strlen (name) ? strlen (name) : 1;
  unsigned entrynum = 0;

  vcd_assert (pathtable_get_size(pt) < ISO_BLOCKSIZE); /* fixme */

  memset(ipt, 0, sizeof (struct iso_path_table) + name_len); /* paranoia */

  ipt->name_len = to_711 (name_len);
  ipt->extent = to_732 (extent);
  ipt->parent = to_722 (parent);
  memcpy (ipt->name, name, name_len);

  pathtable_get_size_and_entries (pt, NULL, &entrynum);

  if (entrynum > 1)
    {
      const struct iso_path_table *ipt2 
        = pathtable_get_entry (pt, entrynum - 2);

      vcd_assert (ipt2 != NULL);

      vcd_assert (from_722 (ipt2->parent) <= parent);
    }

  return entrynum;
}

bool
_vcd_iso_dirname_valid_p (const char pathname[])
{
  const char *p = pathname;
  int len;

  vcd_assert (pathname != NULL);

  if (*p == '/' || *p == '.' || *p == '\0')
    return false;

  if (strlen (pathname) > MAX_ISOPATHNAME)
    return false;
  
  len = 0;
  for (; *p; p++)
    if (_vcd_isdchar (*p))
      {
        len++;
        if (len > 8)
          return false;
      }
    else if (*p == '/')
      {
        if (!len)
          return false;
        len = 0;
      }
    else
      return false; /* unexpected char */

  if (!len)
    return false; /* last char may not be '/' */

  return true;
}

bool
_vcd_iso_pathname_valid_p (const char pathname[])
{
  const char *p = NULL;

  vcd_assert (pathname != NULL);

  if ((p = strrchr (pathname, '/')))
    {
      bool rc;
      char *_tmp = strdup (pathname);
      
      *strrchr (_tmp, '/') = '\0';

      rc = _vcd_iso_dirname_valid_p (_tmp);

      free (_tmp);

      if (!rc)
        return false;

      p++;
    }
  else
    p = pathname;

  if (strlen (pathname) > (MAX_ISOPATHNAME - 6))
    return false;

  {
    int len = 0;
    int dots = 0;

    for (; *p; p++)
      if (_vcd_isdchar (*p))
        {
          len++;
          if (dots == 0 ? len > 8 : len > 3)
            return false;
        }
      else if (*p == '.')
        {
          dots++;
          if (dots > 1)
            return false;
          if (!len)
            return false;
          len = 0;
        }
      else
        return false;

    if (dots != 1)
      return false;
  }

  return true;
}

char *
_vcd_iso_pathname_isofy (const char pathname[], uint16_t version)
{
  char tmpbuf[1024] = { 0, };
    
  vcd_assert (strlen (pathname) < (sizeof (tmpbuf) - sizeof (";65535")));

  snprintf (tmpbuf, sizeof(tmpbuf), "%s;%d", pathname, version);

  return strdup (tmpbuf);
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
