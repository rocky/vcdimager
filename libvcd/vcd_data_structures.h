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

#ifndef __VCD_DATA_STRUCTURES_H__
#define __VCD_DATA_STRUCTURES_H__

/* opaque... */
typedef struct _VcdList VcdList;
typedef struct _VcdListNode VcdListNode;

typedef int (*_vcd_list_iterfunc) (void *data, void *user_data);

/* methods */
VcdList *_vcd_list_new (void);

void _vcd_list_free (VcdList *list, int free_data);

unsigned _vcd_list_length (VcdList *list);

void _vcd_list_prepend (VcdList *list, void *data);

void _vcd_list_append (VcdList *list, void *data);

void _vcd_list_foreach (VcdList *list, _vcd_list_iterfunc *func, void *user_data);

/* node ops */

VcdListNode *_vcd_list_at (VcdList *list, int idx);

VcdListNode *_vcd_list_begin (VcdList *list);

VcdListNode *_vcd_list_node_next (VcdListNode *node);

void _vcd_list_node_free (VcdListNode *node, int free_data);

void *_vcd_list_node_data (VcdListNode *node);

#endif /* __VCD_DATA_STRUCTURES_H__ */

/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */

