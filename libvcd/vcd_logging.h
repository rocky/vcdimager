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

#ifndef __VCD_LOGGING_H__
#define __VCD_LOGGING_H__

#include "vcd_types.h"

typedef enum {
  LOG_DEBUG = 1,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR
} log_level_t;

void
vcd_log(log_level_t level, const char format[], ...) GNUC_PRINTF(2, 3);
    
typedef void(*vcd_log_handler_t)(log_level_t level, const char message[]);

vcd_log_handler_t
vcd_log_set_handler(vcd_log_handler_t new_handler);

void
vcd_debug(const char format[], ...) GNUC_PRINTF(1,2);

void
vcd_info(const char format[], ...) GNUC_PRINTF(1,2);

void
vcd_warn(const char format[], ...) GNUC_PRINTF(1,2);

void
vcd_error(const char format[], ...) GNUC_PRINTF(1,2);

#endif /* __VCD_LOGGING_H__ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
