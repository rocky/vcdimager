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
  uint8_t xa_type;
  uint8_t xa_filenum;
  uint32_t extent;
  uint32_t size;
  unsigned pt_id;
} data_t;

typedef struct _VcdDirNode VcdDirNode;

struct _VcdDirNode 
{
  data_t     data;
  VcdDirNode *next;
  VcdDirNode *prev;
  VcdDirNode *parent;
  VcdDirNode *children;
};

struct _VcdDirectory 
{
  VcdDirNode *root;
};

#define EXTENT(anode) ((anode)->data.extent)
#define SIZE(anode)   ((anode)->data.size)
#define PT_ID(anode)  ((anode)->data.pt_id)

#define DATAP(anode) (&((anode)->data))

/* n-way tree stuff */

typedef void (*_node_traversal_func_t) (VcdDirNode *node, void *user_data);

static VcdDirNode* 
_node_next_sibling(VcdDirNode *node)    
{
  assert(node != NULL);
  return node->next;
}

static VcdDirNode* 
_node_first_child (VcdDirNode *node)    
{
  assert(node != NULL);
  return node->children;
}

static void
_node_traverse (VcdDirNode *root, _node_traversal_func_t travfunc, void *user_data)
{
  VcdDirNode *child;

  assert(root != NULL);

  travfunc(root, user_data);

  child = _node_first_child(root);
  while(child) {
    _node_traverse(child, travfunc, user_data);
    child = _node_next_sibling(child);
  }
}

static VcdDirNode*
_node_new(void)
{
  VcdDirNode *newnode;
  
  newnode = _vcd_malloc(sizeof(VcdDirNode));

  return newnode;
}


static VcdDirNode* 
_node_get_root(VcdDirNode *node)    
{
  assert(node != NULL);

  while (node->parent)
    node = node->parent;
  
  return node;
}

static void
_node_append_node(VcdDirNode *pnode, VcdDirNode *nnode)
{
  assert(pnode != NULL);
  assert(nnode != NULL);
  
  if(_node_first_child(pnode)) {
    VcdDirNode *child = _node_first_child(pnode);
    while(child->next) 
      child = _node_next_sibling(child);

    child->next = nnode;
    nnode->parent = pnode;
    nnode->next = NULL;
    nnode->prev = child;

  } else {
    pnode->children = nnode;
    nnode->parent = pnode;
    nnode->prev = nnode->next = NULL;
  }
}

static void
_node_destroy(VcdDirNode *node)
{
  VcdDirNode *child;

  assert(node != NULL);

  child = _node_first_child(node);
  while(child) {
    VcdDirNode *current = child;
    child = _node_next_sibling(child);
    _node_destroy(current);
  }

  free(node);
}

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
get_dirsizes(VcdDirNode* dirnode)
{
  unsigned result = 0;

  _node_traverse(dirnode, traverse_get_dirsizes, &result);

  return result;
}

static void
traverse_update_dirextents(VcdDirNode *dirnode, void *data)
{
  data_t *d = DATAP(dirnode);

  if(d->is_dir) {
    VcdDirNode* child = _node_first_child(dirnode);
    unsigned dirextent = d->extent;

    dirextent += d->size / ISO_BLOCKSIZE;

    while(child) {
      data_t *d = DATAP(child);

      assert(d != NULL);

      if(d->is_dir) {
        d->extent = dirextent;
        dirextent += get_dirsizes(child);
      }

      child = _node_next_sibling(child);
    }
    
  }
}

static void 
update_dirextents (VcdDirectory *dir, uint32_t extent)
{
  EXTENT(dir->root) = extent;

  _node_traverse(dir->root, traverse_update_dirextents, NULL);
}

static void
traverse_update_sizes(VcdDirNode *node, void *data)
{
  data_t *dirdata = DATAP(node);

  if(dirdata->is_dir) {
    VcdDirNode* child = _node_first_child(node);
    unsigned blocks = 1;
    unsigned offset = 0;

    offset += dir_calc_record_size(1, sizeof(xa_t)); /* '.' */
    offset += dir_calc_record_size(1, sizeof(xa_t)); /* '..' */

    while(child) {
      data_t *d = DATAP(child);
      unsigned reclen;

      assert(d != NULL);

      reclen = dir_calc_record_size(strlen(d->name), sizeof(xa_t));
    
      if(offset + reclen > ISO_BLOCKSIZE) {
        blocks++;
        offset = reclen;
      } else
        offset += reclen;

      child = _node_next_sibling(child);
    }
    dirdata->size = blocks*ISO_BLOCKSIZE;
  }
}

static void 
update_sizes(VcdDirectory *dir)
{
  _node_traverse(dir->root, traverse_update_sizes, NULL);
}

static void*
_memdup (const void *mem, size_t count)
{
  void *new_mem = NULL;

  if (mem) {
    new_mem = _vcd_malloc (count);
    memcpy (new_mem, mem, count);
  }

  return new_mem;
}

/* exported stuff: */

VcdDirectory *
_vcd_directory_new (void)
{
  data_t *data;
  VcdDirectory *dir = NULL;

  assert(sizeof(xa_t) == 14);

  dir = _vcd_malloc (sizeof (VcdDirectory));

  dir->root = _node_new();
  data = &(dir->root->data);

  data->is_dir = true;
  data->name = _memdup("\0", 2);
  data->xa_type = XA_FORM1_DIR;
  data->xa_filenum = 0x00;

  return dir;
}

static void
traverse_vcd_directory_done(VcdDirNode *node, void *data)
{
  free(node->data.name);
}

void
_vcd_directory_destroy(VcdDirectory *dir)
{
  assert(dir != NULL);
  assert(dir->root != NULL);

  _node_traverse(dir->root, traverse_vcd_directory_done, NULL);

  _node_destroy(dir->root);
  dir->root = NULL;

  free(dir);
}

static VcdDirNode* 
lookup_child(VcdDirNode* node, const char name[])
{
  VcdDirNode* child = _node_first_child(node);

  while(child) {
    data_t *d = DATAP(child);

    if(!strcmp(d->name, name))
      return child;

    child = _node_next_sibling(child);
  }

  return child; /* NULL */
}

int
_vcd_directory_mkdir (VcdDirectory *dir, const char pathname[])
{
  char **splitpath;
  unsigned level, n;
  VcdDirNode* pdir = dir->root;

  assert(pathname != NULL);
  assert(dir->root != NULL);

  splitpath = _strsplit(pathname, '/');

  level = _strlenv(splitpath);

  for(n = 0; n < level-1; n++) 
    if(!(pdir = lookup_child(pdir, splitpath[n]))) {
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
    VcdDirNode *newnode = _node_new();

    _node_append_node(pdir, newnode);

    newnode->data.is_dir = true;
    newnode->data.name = strdup(splitpath[level-1]);
    newnode->data.xa_type = XA_FORM1_DIR;
    newnode->data.xa_filenum = 0x00;
    /* .. */
  }

  _strfreev (splitpath);

  return 0;
}

int
_vcd_directory_mkfile (VcdDirectory *dir, const char pathname[], 
                       uint32_t start, uint32_t size,
                       bool form2_flag, uint8_t filenum)
{
  char **splitpath;
  unsigned level, n;
  VcdDirNode* pdir = dir->root;

  assert (pathname != NULL);
  assert (dir->root != NULL);

  splitpath = _strsplit (pathname, '/');

  level = _strlenv (splitpath);

  for (n = 0; n < level-1; n++) 
    if (!(pdir = lookup_child (pdir, splitpath[n]))) 
      {
        vcd_error ("mkdfile: parent dir `%s' (level=%d) for `%s' missing!", 
                   splitpath[n], n, pathname);
        assert (0);
      }

  if (lookup_child (pdir, splitpath[level-1])) 
    {
      vcd_error ("mkfile: `%s' already exists", pathname);
      assert (0);
    }
  
  {
    VcdDirNode *newnode = _node_new();

    _node_append_node (pdir, newnode);

    newnode->data.is_dir = false;
    newnode->data.name = strdup (splitpath[level-1]);
    newnode->data.xa_type = form2_flag ? XA_FORM2_FILE : XA_FORM1_FILE;
    newnode->data.xa_filenum = filenum;
    newnode->data.size = size;
    newnode->data.extent = start;
    /* .. */
  }

  _strfreev (splitpath);

  return 0;
}

uint32_t
_vcd_directory_get_size (VcdDirectory *dir)
{
  assert (dir != NULL);
  assert (dir->root != NULL);

  update_sizes (dir);
  return get_dirsizes (dir->root);
}

static void
traverse_vcd_directory_dump_entries (VcdDirNode *node, void *data)
{
  data_t *d = DATAP(node);
  xa_t tmp = { { 0, }, 0, { 'U', 'X', 'A' }, 0, { 0, } };
  
  uint32_t root_extent = EXTENT(_node_get_root(node));
  uint32_t parent_extent = 
    (node->parent)
    ? EXTENT(node->parent)
    : EXTENT(node);
  uint32_t parent_size = 
    (node->parent)
    ? SIZE(node->parent)
    : SIZE(node);

  void *dirbufp = (char*) data + ISO_BLOCKSIZE * (parent_extent - root_extent);

  tmp.type = d->xa_type;
  tmp.filenum = d->xa_filenum;

  if (node->parent)
    dir_add_entry_su (dirbufp, d->name, d->extent, d->size, 
                      d->is_dir ? ISO_DIRECTORY : ISO_FILE,
                      &tmp, sizeof (tmp));
  
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
  assert(dir != NULL);
  assert(dir->root != NULL);

  update_sizes(dir); /* better call it one time more than one less */
  update_dirextents(dir, extent);

  _node_traverse(dir->root, traverse_vcd_directory_dump_entries, buf); 
}

typedef struct {
  void *ptl;
  void *ptm;
} _vcd_directory_dump_pathtables_t;

static void
traverse_vcd_directory_dump_pathtables(VcdDirNode *node, void * data)
{
  data_t *d = DATAP(node);
  _vcd_directory_dump_pathtables_t *args = data;

  if(d->is_dir) {
    unsigned parent_id = node->parent ? PT_ID(node->parent) : 1;

    pathtable_l_add_entry(args->ptl, d->name, d->extent, parent_id);
    d->pt_id =
      pathtable_m_add_entry(args->ptm, d->name, d->extent, parent_id);
  }
}

void
_vcd_directory_dump_pathtables(VcdDirectory *dir, void *ptl, void *ptm)
{
  _vcd_directory_dump_pathtables_t args;

  assert(dir != NULL);

  pathtable_init(ptl);
  pathtable_init(ptm);

  args.ptl = ptl;
  args.ptm = ptm;

  _node_traverse(dir->root, traverse_vcd_directory_dump_pathtables, &args); 
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
