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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "vcd_directory.h"
#include "vcd_iso9660.h"
#include "vcd_logging.h"
#include "vcd_util.h"

static const char _rcsid[] = "$Id$";

/* CD-ROM XA */

#define XA_FORM1_DIR    0x8d
#define XA_FORM1_FILE   0x0d
#define XA_FORM2_FILE   0x15

typedef struct 
{
  uint8_t unknown1[4]; /* zero */
  uint8_t type;        /* XA_... */
  uint8_t magic[3];    /* { 'U', 'X', 'A' } */
  uint8_t filenum;     /* filenum */
  uint8_t unknown2[5]; /* zero */
} xa_t;

/* tree data structure */

typedef struct 
{
  bool is_dir;
  char *name;
  uint16_t version;
  uint8_t xa_type;
  uint8_t xa_filenum;
  uint32_t extent;
  uint32_t size;
  unsigned pt_id;
} data_t;

typedef VcdTreeNode VcdDirNode;

#define EXTENT(anode) (DATAP(anode)->extent)
#define SIZE(anode)   (DATAP(anode)->size)
#define PT_ID(anode)  (DATAP(anode)->pt_id)

#define DATAP(anode) ((data_t*) _vcd_tree_node_data (anode))

/* implementation */

static void
traverse_get_dirsizes(VcdDirNode *node, void *data)
{
  data_t *d = DATAP(node);
  unsigned *sum = data;

  if(d->is_dir)
    *sum += (d->size / ISO_BLOCKSIZE);
}

static unsigned
get_dirsizes (VcdDirNode* dirnode)
{
  unsigned result = 0;

  _vcd_tree_node_traverse (dirnode, traverse_get_dirsizes, &result);

  return result;
}

static void
traverse_update_dirextents (VcdDirNode *dirnode, void *data)
{
  data_t *d = DATAP(dirnode);

  if (d->is_dir) 
    {
      VcdDirNode* child = _vcd_tree_node_first_child (dirnode);
      unsigned dirextent = d->extent;
      
      dirextent += d->size / ISO_BLOCKSIZE;
      
      while (child) 
        {
          data_t *d = DATAP(child);
          
          assert (d != NULL);
          
          if (d->is_dir) 
            {
              d->extent = dirextent;
              dirextent += get_dirsizes (child);
            }
          
          child = _vcd_tree_node_next_sibling (child);
        }
    }
}

static void 
update_dirextents (VcdDirectory *dir, uint32_t extent)
{
  data_t *dirdata = DATAP(_vcd_tree_root (dir));
  
  dirdata->extent = extent;
  _vcd_tree_node_traverse (_vcd_tree_root (dir),
                           traverse_update_dirextents, NULL);
}

static void
traverse_update_sizes(VcdDirNode *node, void *data)
{
  data_t *dirdata = DATAP(node);

  if (dirdata->is_dir)
    {
      VcdDirNode* child = _vcd_tree_node_first_child(node);
      unsigned blocks = 1;
      unsigned offset = 0;
      
      offset += dir_calc_record_size (1, sizeof(xa_t)); /* '.' */
      offset += dir_calc_record_size (1, sizeof(xa_t)); /* '..' */
      
      while (child) 
        {
          data_t *d = DATAP(child);
          unsigned reclen;
          char *pathname = (d->is_dir 
                            ? strdup (d->name)
                            : _vcd_iso_pathname_isofy (d->name, d->version));
          
          assert(d != NULL);
          
          reclen = dir_calc_record_size (strlen (pathname), sizeof (xa_t));

          free (pathname);
          
          if (offset + reclen > ISO_BLOCKSIZE) 
            {
              blocks++;
              offset = reclen;
            } 
          else
            offset += reclen;
          
          child = _vcd_tree_node_next_sibling (child);
        }
      dirdata->size = blocks * ISO_BLOCKSIZE;
    }
}

static void 
update_sizes (VcdDirectory *dir)
{
  _vcd_tree_node_traverse (_vcd_tree_root(dir), traverse_update_sizes, NULL);
}


/* exported stuff: */

VcdDirectory *
_vcd_directory_new (void)
{
  data_t *data;
  VcdDirectory *dir = NULL;

  assert(sizeof(xa_t) == 14);

  data = _vcd_malloc (sizeof (data_t));
  dir = _vcd_tree_new (data);

  data->is_dir = true;
  data->name = _vcd_memdup("\0", 2);
  data->xa_type = XA_FORM1_DIR;
  data->xa_filenum = 0x00;

  return dir;
}

static void
traverse_vcd_directory_done (VcdDirNode *node, void *data)
{
  data_t *dirdata = DATAP (node);

  free (dirdata->name);
}

void
_vcd_directory_destroy (VcdDirectory *dir)
{
  assert (dir != NULL);

  _vcd_tree_node_traverse (_vcd_tree_root (dir),
                           traverse_vcd_directory_done, NULL);

  _vcd_tree_destroy (dir, true);
}

static VcdDirNode* 
lookup_child (VcdDirNode* node, const char name[])
{
  VcdDirNode* child = _vcd_tree_node_first_child (node);

  while (child)
    {
      data_t *d = DATAP(child);
      
      if (!strcmp (d->name, name))
        return child;

      child = _vcd_tree_node_next_sibling (child);
    }
  
  return child; /* NULL */
}

static int
_iso_dir_cmp (VcdDirNode *node1, VcdDirNode *node2)
{
  data_t *d1 = DATAP(node1);
  data_t *d2 = DATAP(node2);
  int result = 0;

  result = strcmp (d1->name, d2->name);

  return result;
}

int
_vcd_directory_mkdir (VcdDirectory *dir, const char pathname[])
{
  char **splitpath;
  unsigned level, n;
  VcdDirNode* pdir = _vcd_tree_root (dir);

  assert (dir != NULL);
  assert (pathname != NULL);

  splitpath = _vcd_strsplit (pathname, '/');

  level = _vcd_strlenv (splitpath);

  for (n = 0; n < level-1; n++) 
    if (!(pdir = lookup_child(pdir, splitpath[n]))) 
      {
        vcd_error("mkdir: parent dir `%s' (level=%d) for `%s' missing!",
                  splitpath[n], n, pathname);
        assert(0);
      }

  if (lookup_child (pdir, splitpath[level-1])) 
    {
      vcd_error ("mkdir: `%s' already exists", pathname);
      assert (0);
    }

  {
    data_t *data = _vcd_malloc (sizeof (data_t));

    _vcd_tree_node_append_child (pdir, data);

    data->is_dir = true;
    data->name = strdup(splitpath[level-1]);
    data->xa_type = XA_FORM1_DIR;
    data->xa_filenum = 0x00;
    /* .. */
  }

  _vcd_tree_node_sort_children (pdir, _iso_dir_cmp);
  
  _vcd_strfreev (splitpath);

  return 0;
}

int
_vcd_directory_mkfile (VcdDirectory *dir, const char pathname[], 
                       uint32_t start, uint32_t size,
                       bool form2_flag, uint8_t filenum)
{
  char **splitpath;
  unsigned level, n;
  const int file_version = 1;

  VcdDirNode* pdir = NULL;

  assert (dir != NULL);
  assert (pathname != NULL);

  splitpath = _vcd_strsplit (pathname, '/');

  level = _vcd_strlenv (splitpath);

  while (!pdir)
    {
      pdir = _vcd_tree_root (dir);
      
      for (n = 0; n < level-1; n++) 
        if (!(pdir = lookup_child (pdir, splitpath[n]))) 
          {
            char *newdir = _vcd_strjoin (splitpath, n+1, "/");

            vcd_info ("autocreating directory `%s' for file `%s'",
                      newdir, pathname);
            
            _vcd_directory_mkdir (dir, newdir);
            
            free (newdir);
        
            assert (pdir == NULL);

            break;
          }
        else if (!DATAP(pdir)->is_dir)
          {
            char *newdir = _vcd_strjoin (splitpath, n+1, "/");

            vcd_error ("mkfile: `%s' not a directory", newdir);

            free (newdir);

            return -1;
          }

    }

  if (lookup_child (pdir, splitpath[level-1])) 
    {
      vcd_error ("mkfile: `%s' already exists", pathname);
      return -1;
    }
  
  {
    data_t *data = _vcd_malloc (sizeof (data_t));

    _vcd_tree_node_append_child (pdir, data);

    data->is_dir = false;
    data->name = strdup (splitpath[level-1]);
    data->version = file_version;
    data->xa_type = form2_flag ? XA_FORM2_FILE : XA_FORM1_FILE;
    data->xa_filenum = filenum;
    data->size = size;
    data->extent = start;
    /* .. */
  }

  _vcd_tree_node_sort_children (pdir, _iso_dir_cmp);

  _vcd_strfreev (splitpath);

  return 0;
}

uint32_t
_vcd_directory_get_size (VcdDirectory *dir)
{
  assert (dir != NULL);

  update_sizes (dir);
  return get_dirsizes (_vcd_tree_root (dir));
}

static void
traverse_vcd_directory_dump_entries (VcdDirNode *node, void *data)
{
  data_t *d = DATAP(node);
  xa_t tmp = { { 0, }, 0, { 'U', 'X', 'A' }, 0, { 0, } };
  
  uint32_t root_extent = EXTENT(_vcd_tree_node_root (node));
  uint32_t parent_extent = 
    (!_vcd_tree_node_is_root (node))
    ? EXTENT(_vcd_tree_node_parent (node))
    : EXTENT(node);
  uint32_t parent_size = 
    (!_vcd_tree_node_is_root (node))
    ? SIZE(_vcd_tree_node_parent (node))
    : SIZE(node);

  void *dirbufp = (char*) data + ISO_BLOCKSIZE * (parent_extent - root_extent);

  tmp.type = d->xa_type;
  tmp.filenum = d->xa_filenum;

  if (!_vcd_tree_node_is_root (node))
    {
      char *pathname = (d->is_dir 
                        ? strdup (d->name)
                        : _vcd_iso_pathname_isofy (d->name, d->version));

      dir_add_entry_su (dirbufp, pathname, d->extent, d->size, 
                        d->is_dir ? ISO_DIRECTORY : ISO_FILE,
                        &tmp, sizeof (tmp));

      free (pathname);
    }  

  if (d->is_dir) 
    {
      void *dirbuf = (char*)data + ISO_BLOCKSIZE * (d->extent - root_extent);
      xa_t tmp2 = {{ 0, }, XA_FORM1_DIR, { 'U', 'X', 'A' }, 0, { 0, }};
      
      dir_init_new_su (dirbuf, 
                       d->extent, d->size, &tmp2, sizeof (tmp2),
                       parent_extent, parent_size, &tmp2, sizeof (tmp2));
    }
}

void
_vcd_directory_dump_entries (VcdDirectory *dir, void *buf, uint32_t extent)
{
  assert (dir != NULL);

  update_sizes (dir); /* better call it one time more than one less */
  update_dirextents (dir, extent);

  _vcd_tree_node_traverse (_vcd_tree_root (dir), 
                           traverse_vcd_directory_dump_entries, buf); 
}

typedef struct 
{
  void *ptl;
  void *ptm;
} _vcd_directory_dump_pathtables_t;

static void
traverse_vcd_directory_dump_pathtables (VcdDirNode *node, void *data)
{
  data_t *d = DATAP(node);
  _vcd_directory_dump_pathtables_t *args = data;

  if (d->is_dir)
    {
      unsigned parent_id = (!_vcd_tree_node_is_root (node)
                            ? PT_ID(_vcd_tree_node_parent (node)) 
                            : 1);

      pathtable_l_add_entry (args->ptl, d->name, d->extent, parent_id);
      d->pt_id =
        pathtable_m_add_entry (args->ptm, d->name, d->extent, parent_id);
    }
}

void
_vcd_directory_dump_pathtables (VcdDirectory *dir, void *ptl, void *ptm)
{
  _vcd_directory_dump_pathtables_t args;

  assert (dir != NULL);

  pathtable_init (ptl);
  pathtable_init (ptm);

  args.ptl = ptl;
  args.ptm = ptm;

  _vcd_tree_node_traverse (_vcd_tree_root (dir),
                           traverse_vcd_directory_dump_pathtables, &args); 
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
