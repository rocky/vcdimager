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

#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

#include "vcd_types.h"

void directory_init(void);

void directory_done(void);

void directory_mkdir(const char pathname[]);

void directory_mkfile(const char pathname[],
                      uint32_t start, 
                      uint32_t size,
                      bool form2, 
                      uint8_t filenum);

uint32_t directory_get_size(void);

void directory_dump_entries(void * buf, 
                            uint32_t extent);

void directory_dump_pathtables(void * ptl, 
                               void * ptm);

#endif /* _DIRECTORY_H_ */



/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
