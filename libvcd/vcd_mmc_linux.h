/*
    $Id$

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>
    Copyright (C) 2001 Arnd Bergmann <arnd@itreff.de>

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

#ifndef __VCD_MMC_LINUX_H__
#define __VCD_MMC_LINUX_H__

#include <libvcd/vcd_mmc_private.h>

int _vcd_mmc_linux_generic_packet (void *device, _vcd_mmc_command_t * cmd);

void *_vcd_mmc_linux_new_device (const char *device);

void _vcd_mmc_linux_destroy_device (void *user_data);

#endif /* __MMC_LINUX_H__ */
