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

#ifndef __VCD_RECORDER_H__
#define __VCD_RECORDER_H__

#include <libvcd/vcd_types.h>
#include <libvcd/vcd_data_structures.h>

typedef struct _VcdRecorder VcdRecorder;

typedef enum
{
  _VCD_WT_DAO,
  _VCD_WT_VPKT
}
vcd_write_type_t;

typedef enum
{
  _VCD_DB_RAW,
  _VCD_DB_MODE2,
  _VCD_DB_XA_FORM2_S
}
vcd_data_block_type_t;

enum _VCD_RECORDER_ERRORS
{
  _VCD_ERR_NWA_INV = -151,
  _VCD_ERR_BUR = 1		/* buffer underrun */
};

typedef struct
{
  int (*set_simulate) (void *user_data, bool simulate);
  int (*set_write_type) (void *user_data, vcd_write_type_t write_type);
  int (*set_data_block_type) (void *user_data,
			      vcd_data_block_type_t data_block_type);
  int (*set_speed) (void *user_data, int read_speed, int write_speed);
  int (*get_next_writable) (void *user_data, int track);
  int (*get_next_track) (void *user_data);
  int (*get_size) (void *user_data, int track);
  int (*send_cue_sheet) (void *user_data, const VcdList * vcd_cue_list);
  int (*write_sectors) (void *user_data, const uint8_t * buffer, int lsn,
			unsigned int buflen, int count);
  int (*reserve_track) (void *user_data, unsigned int sectors);
  int (*close_track) (void *user_data, int track);
  void (*free) (void *user_data);
}
vcd_recorder_funcs;

VcdRecorder *vcd_recorder_new (void *user_data,
			       const vcd_recorder_funcs * funcs);

void vcd_recorder_destroy (VcdRecorder * obj);

int
vcd_recorder_write_sector (VcdRecorder * obj, const uint8_t * buffer,
			   int lsn);

int vcd_recorder_flush_write (VcdRecorder * obj);

int vcd_recorder_set_simulate (VcdRecorder * obj, bool simulate);

int
vcd_recorder_set_write_type (VcdRecorder * obj, vcd_write_type_t write_type);

int
vcd_recorder_set_data_block_type (VcdRecorder * obj,
				  vcd_data_block_type_t data_block_type);

int
vcd_recorder_set_speed (VcdRecorder * obj, int read_speed, int write_speed);

int vcd_recorder_set_current_track (VcdRecorder * obj, int track);

int vcd_recorder_get_next_writable (VcdRecorder * obj, int track);

int vcd_recorder_get_next_track (VcdRecorder * obj);

int vcd_recorder_get_size (VcdRecorder * obj, int track);

int
vcd_recorder_send_cue_sheet (VcdRecorder * obj, const VcdList * vcd_cue_list);

int
vcd_recorder_write_sectors (VcdRecorder * obj, const uint8_t * buffer,
			    int lsn, unsigned int buflen, int count);
int
vcd_recorder_reserve_track (VcdRecorder * obj, unsigned int sectors);

int vcd_recorder_close_track (VcdRecorder * obj, int track);

#endif /* __VCD_RECORDER_H__ */
