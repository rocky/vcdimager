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

struct _VcdList
{
  unsigned length;

  VcdListNode *begin;
  VcdListNode *end;
};

struct _VcdListNode
{
  VcdList *list;

  VcdListNode *next;

  void *data;
};

/* impl */

VcdList *
_vcd_list_new (void)
{
  VcdList *new_obj = _vcd_malloc (sizeof (VcdList));

  return new_obj;
}

void
_vcd_list_free (VcdList *list, int free_data)
{
  while (_vcd_list_length (list))
    _vcd_list_node_free (_vcd_list_begin (list), free_data);

  free (list);
}

unsigned
_vcd_list_length (const VcdList *list)
{
  vcd_assert (list != NULL);

  return list->length;
}

static bool
_bubble_sort_iteration (VcdList *list, _vcd_list_cmp_func cmp_func)
{
  VcdListNode **pnode;
  bool changed = false;
  
  for (pnode = &(list->begin);
       (*pnode) != NULL && (*pnode)->next != NULL;
       pnode = &((*pnode)->next))
    {
      VcdListNode *node = *pnode;
      
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

void _vcd_list_sort (VcdList *list, _vcd_list_cmp_func cmp_func)
{
  /* fixme -- this is bubble sort -- worst sorting algo... */

  vcd_assert (list != NULL);
  vcd_assert (cmp_func != 0);
  
  while (_bubble_sort_iteration (list, cmp_func));
}

void
_vcd_list_prepend (VcdList *list, void *data)
{
  VcdListNode *new_node;

  vcd_assert (list != NULL);

  new_node = _vcd_malloc (sizeof (VcdListNode));
  
  new_node->list = list;
  new_node->next = list->begin;
  new_node->data = data;

  list->begin = new_node;
  if (list->length == 0)
    list->end = new_node;

  list->length++;
}

void
_vcd_list_append (VcdList *list, void *data)
{
  vcd_assert (list != NULL);

  if (list->length == 0)
    {
      _vcd_list_prepend (list, data);
    }
  else
    {
      VcdListNode *new_node = _vcd_malloc (sizeof (VcdListNode));
      
      new_node->list = list;
      new_node->next = NULL;
      new_node->data = data;

      list->end->next = new_node;
      list->end = new_node;

      list->length++;
    }
}

void 
_vcd_list_foreach (VcdList *list, _vcd_list_iterfunc func, void *user_data)
{
  VcdListNode *node;

  vcd_assert (list != NULL);
  vcd_assert (func != 0);
  
  for (node = _vcd_list_begin (list);
       node != NULL;
       node = _vcd_list_node_next (node))
    func (_vcd_list_node_data (node), user_data);
}

VcdListNode *
_vcd_list_find (VcdList *list, _vcd_list_iterfunc cmp_func, void *user_data)
{
  VcdListNode *node;

  vcd_assert (list != NULL);
  vcd_assert (cmp_func != 0);
  
  for (node = _vcd_list_begin (list);
       node != NULL;
       node = _vcd_list_node_next (node))
    if (cmp_func (_vcd_list_node_data (node), user_data))
      break;

  return node;
}

/* node ops */

VcdListNode *
_vcd_list_at (VcdList *list, int idx)
{
  VcdListNode *node = _vcd_list_begin (list);

  if (idx < 0)
    return _vcd_list_at (list, _vcd_list_length (list) + idx);

  vcd_assert (idx >= 0);

  while (node && idx)
    {
      node = _vcd_list_node_next (node);
      idx--;
    }

  return node;
}

VcdListNode *
_vcd_list_begin (const VcdList *list)
{
  vcd_assert (list != NULL);

  return list->begin;
}

VcdListNode *
_vcd_list_end (VcdList *list)
{
  vcd_assert (list != NULL);

  return list->end;
}

VcdListNode *
_vcd_list_node_next (VcdListNode *node)
{
  if (node)
    return node->next;

  return NULL;
}

void 
_vcd_list_node_free (VcdListNode *node, int free_data)
{
  VcdList *list;
  VcdListNode *prev_node;

  vcd_assert (node != NULL);
  
  list = node->list;

  vcd_assert (_vcd_list_length (list) > 0);

  if (free_data)
    free (_vcd_list_node_data (node));

  if (_vcd_list_length (list) == 1)
    {
      vcd_assert (list->begin == list->end);

      list->end = list->begin = NULL;
      list->length = 0;
      free (node);
      return;
    }

  vcd_assert (list->begin != list->end);

  if (list->begin == node)
    {
      list->begin = node->next;
      free (node);
      list->length--;
      return;
    }

  for (prev_node = list->begin; prev_node->next; prev_node = prev_node->next)
    if (prev_node->next == node)
      break;

  vcd_assert (prev_node->next != NULL);

  if (list->end == node)
    list->end = prev_node;

  prev_node->next = node->next;

  list->length--;

  free (node);
}

void *
_vcd_list_node_data (VcdListNode *node)
{
  if (node)
    return node->data;

  return NULL;
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

  VcdListNode *listnode;
  VcdTree *tree;
  VcdTreeNode *parent;
  VcdList *children;
};

VcdTree *
_vcd_tree_new (void *root_data)
{
  VcdTree *new_tree;

  new_tree = _vcd_malloc (sizeof (VcdTree));

  new_tree->root = _vcd_malloc (sizeof (VcdTreeNode));

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
      vcd_assert (_vcd_list_length (node->children) == 0);
      _vcd_list_free (node->children, true);
      node->children = NULL;
    }

  if (free_data)
    free (_vcd_tree_node_set_data (node, NULL));

  if (node->parent)
    _vcd_list_node_free (node->listnode, true);
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
    pnode->children = _vcd_list_new ();

  nnode = _vcd_malloc (sizeof (VcdTreeNode));

  _vcd_list_append (pnode->children, nnode);

  nnode->data = cdata;
  nnode->parent = pnode;
  nnode->tree = pnode->tree;
  nnode->listnode = _vcd_list_end (pnode->children);

  return nnode;
}

VcdTreeNode *
_vcd_tree_node_first_child (VcdTreeNode *node)
{
  vcd_assert (node != NULL);

  if (!node->children)
    return NULL;

  return _vcd_list_node_data (_vcd_list_begin (node->children));
}

VcdTreeNode *
_vcd_tree_node_next_sibling (VcdTreeNode *node)
{
  vcd_assert (node != NULL);

  return _vcd_list_node_data (_vcd_list_node_next (node->listnode));
}

void
_vcd_tree_node_sort_children (VcdTreeNode *node, _vcd_tree_node_cmp_func cmp_func)
{
  vcd_assert (node != NULL);

  if (node->children)
    _vcd_list_sort (node->children, (_vcd_list_cmp_func) cmp_func);
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
  VcdList *queue;

  vcd_assert (node != NULL);

  queue = _vcd_list_new ();

  _vcd_list_prepend (queue, node);

  while (_vcd_list_length (queue))
    {
      VcdListNode *lastnode = _vcd_list_end (queue);
      VcdTreeNode *treenode = _vcd_list_node_data (lastnode);
      VcdTreeNode *childnode;

      _vcd_list_node_free (lastnode, false);

      trav_func (treenode, user_data);
      
      _VCD_CHILD_FOREACH (childnode, treenode)
        {
          _vcd_list_prepend (queue, childnode);
        }
    }

  _vcd_list_free (queue, false);
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

