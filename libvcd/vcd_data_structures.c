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

#include "vcd_data_structures.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

VcdList *_vcd_list_new (void)
{
  VcdList *new_obj = malloc (sizeof (VcdList));

  assert (new_obj != NULL);

  memset (new_obj, 0, sizeof (VcdList));

  return new_obj;
}

void _vcd_list_free (VcdList *list, int free_data)
{
  free (list);

  assert (!free_data);
  /* fixme */
}

unsigned _vcd_list_length (VcdList *list)
{
  assert (list != NULL);

  return list->length;
}

void _vcd_list_prepend (VcdList *list, void *data)
{
  VcdListNode *new_node;

  assert (list != NULL);

  new_node = malloc (sizeof (VcdListNode));
  assert (new_node != NULL);

  new_node->list = list;
  new_node->next = list->begin;
  new_node->data = data;

  list->begin = new_node;
  if (list->length == 0)
    list->end = new_node;

  list->length++;
}

void _vcd_list_append (VcdList *list, void *data)
{
  assert (list != NULL);

  if (list->length == 0)
    {
      _vcd_list_prepend (list, data);
    }
  else
    {
      VcdListNode *new_node = malloc (sizeof (VcdListNode));
      
      new_node->list = list;
      new_node->next = NULL;
      new_node->data = data;

      list->end->next = new_node;
      list->end = new_node;

      list->length++;
    }
}

void _vcd_list_foreach (VcdList *list, _vcd_list_iterfunc *func, void *user_data)
{
  assert (list != NULL);

  assert (0);
  /* fixme */
}

/* node ops */

VcdListNode *_vcd_list_at (VcdList *list, int idx)
{
  VcdListNode *node = _vcd_list_begin (list);

  if (idx < 0)
    return _vcd_list_at (list, _vcd_list_length (list) + idx);

  assert (idx >= 0);

  while (node && idx)
    {
      node = _vcd_list_node_next (node);
      idx--;
    }

  return node;
}

VcdListNode *_vcd_list_begin (VcdList *list)
{
  assert (list != NULL);

  return list->begin;
}

VcdListNode *_vcd_list_node_next (VcdListNode *node)
{
  if (node)
    return node->next;

  return NULL;
}

void _vcd_list_node_free (VcdListNode *node, int free_data)
{
  assert (0);
  /* fixme */
}

void *_vcd_list_node_data (VcdListNode *node)
{
  if (node)
    return node->data;

  return NULL;
}

/* eof */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */

