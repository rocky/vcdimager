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

#include <string.h>

#include <libvcd/vcd_assert.h>
#include <libvcd/vcd_util.h>
#include <libvcd/vcd_image_fs.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_iso9660_private.h>

static const char _rcsid[] = "$Id$";

static void
_idr2statbuf (const struct iso_directory_record *idr, vcd_image_stat_t *buf)
{
  vcd_xa_t *xa_data = NULL;
  int su_length = 0;

  memset ((void *) buf, 0, sizeof (vcd_image_stat_t));

  if (!idr->length)
    return;

  vcd_assert (idr->length >= sizeof (struct iso_directory_record));

  buf->type = (idr->flags & ISO_DIRECTORY) ? _STAT_DIR : _STAT_FILE;
  buf->lsn = from_733 (idr->extent);
  buf->size = from_733 (idr->size);
  buf->secsize = _vcd_len2blocks (buf->size, ISO_BLOCKSIZE);

  su_length = idr->length - sizeof (struct iso_directory_record);
  vcd_debug ("%d", su_length);
  su_length -= idr->name_len;

  if (su_length % 2)
    su_length--;

  if (su_length < 0 || su_length < sizeof (vcd_xa_t))
    return;

  xa_data = ((char *) idr) + (idr->length - su_length);

  if (xa_data->signature[0] != 'X' 
      || xa_data->signature[1] != 'A')
    {
      vcd_debug ("%d %d %d,  %c %c", idr->length, idr->name_len, su_length,
		 xa_data->signature[0], xa_data->signature[1]);
      vcd_assert_not_reached ();
    }

  buf->xa = *xa_data;
}

static char *
_idr2name (const struct iso_directory_record *idr)
{
  char namebuf[256] = { 0, };

  if (!idr->length)
    return NULL;

  vcd_assert (idr->length >= sizeof (struct iso_directory_record));

  /* (idr->flags & ISO_DIRECTORY) */
  
  if (idr->name[0] == '\0')
    strcpy (namebuf, ".");
  else if (idr->name[0] == '\1')
    strcpy (namebuf, "..");
  else
    strncpy (namebuf, idr->name, idr->name_len);

  return strdup (namebuf);
}


static void
_fs_stat_root (VcdImageSource *obj, vcd_image_stat_t *buf)
{
  char block[ISO_BLOCKSIZE] = { 0, };
  struct iso_primary_descriptor const *pvd = (void *) &block;
  struct iso_directory_record *idr = (void *) pvd->root_directory_record;

  if (vcd_image_source_read_mode2_sector (obj, &block, 
					  ISO_PVD_SECTOR, false))
    vcd_assert_not_reached ();

  _idr2statbuf (idr, buf);
}

static int
_fs_stat_traverse (VcdImageSource *obj, 
		   const vcd_image_stat_t *_root,
		   char **splitpath,
		   vcd_image_stat_t *buf)
{
  unsigned offset = 0;
  uint8_t _dirbuf[_root->size];

  if (!splitpath[0])
    {
      *buf = *_root;
      return 0;
    }

  if (_root->type == _STAT_FILE)
    return -1;

  vcd_assert (_root->type == _STAT_DIR);
  vcd_assert (_root->size == ISO_BLOCKSIZE * _root->secsize);

  if (vcd_image_source_read_mode2_sectors (obj, _dirbuf, _root->lsn, 
					   false, _root->secsize))
    vcd_assert_not_reached ();

  while (offset < _root->size)
    {
      struct iso_directory_record const *idr 
	= (void *) &_dirbuf[offset];
      vcd_image_stat_t _stat;

      char *_name;

      if (!idr->length)
	{
	  offset++;
	  continue;
	}
      
      _name = _idr2name (idr);
      _idr2statbuf (idr, &_stat);

      if (!strcmp (splitpath[0], _name))
	{
	  int retval = _fs_stat_traverse (obj, &_stat, &splitpath[1], buf);
	  free (_name);
	  return retval;
	}

      free (_name);

      offset += idr->length;
    }

  vcd_assert (offset == _root->size);
  
  /* not found */
  return -1;
}

int
vcd_image_source_fs_stat (VcdImageSource *obj,
			  const char pathname[], vcd_image_stat_t *buf)
{
  vcd_image_stat_t _root;
  int retval;
  char **splitpath;

  vcd_assert (obj != NULL);
  vcd_assert (pathname != NULL);
  vcd_assert (buf != NULL);

  _fs_stat_root (obj, &_root);

  splitpath = _vcd_strsplit (pathname, '/');
  retval = _fs_stat_traverse (obj, &_root, splitpath, buf);
  _vcd_strfreev (splitpath);

  return retval;
}

VcdList * /* list of char* -- caller must free it */
vcd_image_source_fs_readdir (VcdImageSource *obj,
			     const char pathname[])
{
  vcd_image_stat_t _stat;

  vcd_assert (obj != NULL);
  vcd_assert (pathname != NULL);

  if (vcd_image_source_fs_stat (obj, pathname, &_stat))
    return NULL;

  if (_stat.type != _STAT_DIR)
    return NULL;

  {
    unsigned offset = 0;
    uint8_t _dirbuf[_stat.size];
    VcdList *retval = _vcd_list_new ();

    vcd_assert (_stat.size == ISO_BLOCKSIZE * _stat.secsize);

    if (vcd_image_source_read_mode2_sectors (obj, _dirbuf, _stat.lsn, 
					     false, _stat.secsize))
      vcd_assert_not_reached ();

    while (offset < _stat.size)
      {
	struct iso_directory_record const *idr 
	  = (void *) &_dirbuf[offset];

	if (!idr->length)
	  {
	    offset++;
	    continue;
	  }

	_vcd_list_append (retval, _idr2name (idr));

	offset += idr->length;
      }

    vcd_assert (offset == _stat.size);

    return retval;
  }
}
