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

#ifndef __VCD_ISO9660_H__
#define __VCD_ISO9660_H__

#include "vcd_types.h"

#define MIN_TRACK_SIZE 4*75

#define MIN_ISO_SIZE MIN_TRACK_SIZE

#define ISO_BLOCKSIZE 2048

#define LEN_ISONAME     31
#define MAX_ISONAME     37

#define ISO_FILE        0       
#define ISO_DIRECTORY   2

/* file/dirname's */
int
_vcd_iso_pathname_valid_p (const char pathname[]);

char *
_vcd_iso_pathname_isofy (const char pathname[]);

/* volume descriptors */

void
set_iso_pvd (void *pd, const char volume_id[], const char application_id[],
             uint32_t iso_size, const void *root_dir, 
             uint32_t path_table_l_extent, uint32_t path_table_m_extent,
             uint32_t path_table_size);

void 
set_iso_evd (void *pd);

/* directory tree */

void
dir_init_new (void *dir, uint32_t self, uint32_t ssize, uint32_t parent, 
              uint32_t psize);

void
dir_init_new_su (void *dir, uint32_t self, uint32_t ssize, 
                 const void *ssu_data, unsigned ssu_size, uint32_t parent,
                 uint32_t psize, const void *psu_data, unsigned psu_size);

void
dir_add_entry_su (void *dir, const char name[], uint32_t extent,
                  uint32_t size, uint8_t flags, const void *su_data,
                  unsigned su_size);

unsigned
dir_calc_record_size (unsigned namelen, unsigned su_len);

/* pathtable */

void
pathtable_init (void *pt);

unsigned
pathtable_get_size (const void *pt);

uint16_t
pathtable_l_add_entry (void *pt, const char name[], uint32_t extent,
                       uint16_t parent);

uint16_t
pathtable_m_add_entry (void *pt, const char name[], uint32_t extent,
                       uint16_t parent);

#endif /* __VCD_ISO9660_H__ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
