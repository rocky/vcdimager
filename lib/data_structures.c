/*
    $Id$

    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>
    Copyright (C) 2004 Rocky Bernstein <rocky@panix.com>

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

#include <stdlib.h>
#include <string.h>

#include <cdio/cdio.h>

/* Public headers */
#include <libvcd/types.h>

/* Private headers */
#include "vcd_assert.h"
#include "data_structures.h"
#include "util.h"

static const char _rcsid[] = "$Id$";

struct _CdioList
{
  unsigned length;

  CdioListNode_t *begin;
  CdioListNode_t *end;
};

struct _CdioListNode
{
  CdioList_t *list;

  CdioListNode_t *next;

  void *data;
};

/* impl */

static bool
_bubble_sort_iteration (CdioList *list, _cdio_list_cmp_func cmp_func)
{
  CdioListNode_t **pnode;
  bool changed = false;
  
  for (pnode = &(list->begin);
       (*pnode) != NULL && (*pnode)->next != NULL;
       pnode = &((*pnode)->next))
    {
      CdioListNode_t *node = *pnode;
      
      if (cmp_func (node->data, node->next->data) <= 0)
        continue; /* n <= n->next */
      
      /* exch n n->next */
      *pnode = node->next;
      node->next = node->next->next;
      (*pnode)->next = node;
      
      changed = true;

      if (node->next == NULL)
        list->end = node;
    }

  return changed;
}

void _vcd_list_sort (CdioList *list, _cdio_list_cmp_func cmp_func)
{
  /* fixme -- this is bubble sort -- worst sorting algo... */

  vcd_assert (list != NULL);
  vcd_assert (cmp_func != 0);
  
  while (_bubble_sort_iteration (list, cmp_func));
}

/* node ops */

CdioListNode_t *
_vcd_list_at (CdioList *list, int idx)
{
  CdioListNode_t *node = _cdio_list_begin (list);

  if (idx < 0)
    return _vcd_list_at (list, _cdio_list_length (list) + idx);

  vcd_assert (idx >= 0);

  while (node && idx)
    {
      node = _cdio_list_node_next (node);
      idx--;
    }

  return node;
}

/*
 * n-way tree based on list -- somewhat inefficent 
 */

struct _VcdTree
{
  VcdTreeNode *root;
};

struct _VcdTreeNode
{
  void *data;

  CdioListNode_t *listnode;
  VcdTree *tree;
  VcdTreeNode *parent;
  CdioList *children;
};

VcdTree *
_vcd_tree_new (void *root_data)
{
  VcdTree *new_tree;

  new_tree = calloc(1, sizeof (VcdTree));

  new_tree->root = calloc(1, sizeof (VcdTreeNode));

  new_tree->root->data = root_data;
  new_tree->root->tree = new_tree;
  new_tree->root->parent = NULL;
  new_tree->root->children = NULL;
  new_tree->root->listnode = NULL;
  
  return new_tree;
}

void
_vcd_tree_destroy (VcdTree *tree, bool free_data)
{
  _vcd_tree_node_destroy (tree->root, free_data);
  
  free (tree->root);
  free (tree);
}

void
_vcd_tree_node_destroy (VcdTreeNode *node, bool free_data)
{
  VcdTreeNode *child, *nxt_child;
  
  vcd_assert (node != NULL);

  child = _vcd_tree_node_first_child (node);
  while(child) {
    nxt_child = _vcd_tree_node_next_sibling (child);
    _vcd_tree_node_destroy (child, free_data);
    child = nxt_child;
  }

  if (node->children)
    {
      vcd_assert (_cdio_list_length (node->children) == 0);
      _cdio_list_free (node->children, true);
      node->children = NULL;
    }

  if (free_data)
    free (_vcd_tree_node_set_data (node, NULL));

  if (node->parent)
    _cdio_list_node_free (node->listnode, true);
  else
    _vcd_tree_node_set_data (node, NULL);
}

VcdTreeNode *
_vcd_tree_root (VcdTree *tree)
{
  return tree->root;
}

void *
_vcd_tree_node_data (VcdTreeNode *node)
{
  return node->data;
}

void *
_vcd_tree_node_set_data (VcdTreeNode *node, void *new_data)
{
  void *old_data = node->data;

  node->data = new_data;

  return old_data;
}

VcdTreeNode *
_vcd_tree_node_append_child (VcdTreeNode *pnode, void *cdata)
{
  VcdTreeNode *nnode;

  vcd_assert (pnode != NULL);

  if (!pnode->children)
    pnode->children = _cdio_list_new ();

  nnode = calloc(1, sizeof (VcdTreeNode));

  _cdio_list_append (pnode->children, nnode);

  nnode->data = cdata;
  nnode->parent = pnode;
  nnode->tree = pnode->tree;
  nnode->listnode = _cdio_list_end (pnode->children);

  return nnode;
}

VcdTreeNode *
_vcd_tree_node_first_child (VcdTreeNode *node)
{
  vcd_assert (node != NULL);

  if (!node->children)
    return NULL;

  return _cdio_list_node_data (_cdio_list_begin (node->children));
}

VcdTreeNode *
_vcd_tree_node_next_sibling (VcdTreeNode *node)
{
  vcd_assert (node != NULL);

  return _cdio_list_node_data (_cdio_list_node_next (node->listnode));
}

void
_vcd_tree_node_sort_children (VcdTreeNode *node, _vcd_tree_node_cmp_func cmp_func)
{
  vcd_assert (node != NULL);

  if (node->children)
    _vcd_list_sort (node->children, (_cdio_list_cmp_func) cmp_func);
}

void
_vcd_tree_node_traverse (VcdTreeNode *node, 
                         _vcd_tree_node_traversal_func trav_func,
                         void *user_data) /* pre-order */
{
  VcdTreeNode *child;

  vcd_assert (node != NULL);

  trav_func (node, user_data);

  _VCD_CHILD_FOREACH (child, node)
    {
      _vcd_tree_node_traverse (child, trav_func, user_data);
    }
}

void
_vcd_tree_node_traverse_bf (VcdTreeNode *node, 
                            _vcd_tree_node_traversal_func trav_func,
                            void *user_data) /* breath-first */
{
  CdioList *queue;

  vcd_assert (node != NULL);

  queue = _cdio_list_new ();

  _cdio_list_prepend (queue, node);

  while (_cdio_list_length (queue))
    {
      CdioListNode_t *lastnode = _cdio_list_end (queue);
      VcdTreeNode  *treenode = _cdio_list_node_data (lastnode);
      VcdTreeNode  *childnode;

      _cdio_list_node_free (lastnode, false);

      trav_func (treenode, user_data);
      
      _VCD_CHILD_FOREACH (childnode, treenode)
        {
          _cdio_list_prepend (queue, childnode);
        }
    }

  _cdio_list_free (queue, false);
}

VcdTreeNode *_vcd_tree_node_parent (VcdTreeNode *node)
{
  return node->parent;
}

VcdTreeNode *_vcd_tree_node_root (VcdTreeNode *node)
{
  return node->tree->root;
}

bool _vcd_tree_node_is_root (VcdTreeNode *node)
{
  return (node->parent == NULL);
}

/* eof */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */

