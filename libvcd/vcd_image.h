/*
    $Id$

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>
                  2002 Rocky Bernstein <rocky@panix.com>

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

#ifndef __VCD_IMAGE_H__
#define __VCD_IMAGE_H__

#include <libvcd/vcd_types.h>
#include <libvcd/vcd_data_structures.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* VcdImageSink ( --> image reader) */

/* opaque structure */
typedef struct _VcdImageSource VcdImageSource; 

typedef struct {

/*!
  Eject media in CD drive. If successful, as a side effect we 
  also free obj. Return 0 if success and 1 for failure.
 */
  int (*eject_media) (void *user_data);

  /*!
    Release and free resources associated with cd. 
  */
  void (*free) (void *user_data);

  /*!
    Return a string containing the default VCD device if none is specified.
  */
  char * (*get_default_device)(void);

  /*! Return the size in bytes of the track that starts at "track."
     The first track is 1. 0 is returned on error.
   */
  unsigned int (*get_track_count) (void *user_data);

  /*!  
    Return the starting MSF (minutes/secs/frames) for track number
    track_num in obj.  Tracks numbers start at 1.
    The "leadout" track is specified either by
    using track_num LEADOUT_TRACK or the total tracks+1.
    1 is returned on error.
  */
  uint32_t (*get_track_lba) (void *user_data, unsigned int track_num);

  /*!  
    Return the starting MSF (minutes/secs/frames) for track number
    track_num in obj.  Tracks numbers start at 1.
    The "leadout" track is specified either by
    using track_num LEADOUT_TRACK or the total tracks+1.
    1 is returned on error.
  */
  int (*get_track_msf) (void *user_data, unsigned int track_num, msf_t *msf);

  /*! Return the size in bytes of the track that starts at "track."
     The first track is 1.
   */
  uint32_t (*get_track_size) (void *user_data, unsigned int track);

  /*!
    Reads a single mode2 sector from cd device into data starting
    from lsn. Returns 0 if no error. 
  */
  int (*read_mode2_sector) (void *user_data, void *buf, lsn_t lsn, 
			    bool mode2raw);

  /*!
    Reads nblocks of mode2 sectors from cd device into data starting
    from lsn.
    Returns 0 if no error. 
  */
  int (*read_mode2_sectors) (void *user_data, void *buf, lsn_t lsn, 
			     bool mode2raw, unsigned nblocks);

  /*!
    Set the arg "key" with "value" in the source device.
  */
  int (*set_arg) (void *user_data, const char key[], const char value[]);

  /*!
    Return the size of the CD in logical block address (LBA) units.
  */
  uint32_t (*stat_size) (void *user_data);

} vcd_image_source_funcs;

/*!
  Eject media in CD drive if there is a routine to do so. 
  Return 0 if success and 1 for failure, and 2 if no routine.
 */
int
vcd_image_source_eject_media (VcdImageSource *obj);

void
vcd_image_source_destroy (VcdImageSource *obj);

/*!
  Return a string containing the default VCD device if none is specified.
 */
char *
vcd_image_source_get_default_device (VcdImageSource *obj);

VcdImageSource *
vcd_image_source_new (void *user_data, const vcd_image_source_funcs *funcs);

/*!
   Reads a single mode2 sector from cd device into data starting
   from lsn. Returns 0 if no error. 
 */
int
vcd_image_source_read_mode2_sector (VcdImageSource *obj, void *buf, 
				    lsn_t lsn, bool mode2raw);

int
vcd_image_source_read_mode2_sectors (VcdImageSource *obj, void *buf, 
				     lsn_t lsn, bool mode2raw, 
				     unsigned num_sectors);

int
vcd_image_source_set_arg (VcdImageSource *obj, const char key[],
			  const char value[]);

uint32_t
vcd_image_source_stat_size (VcdImageSource *obj);

/* VcdImageSink ( --> image writer) */

typedef struct _VcdImageSink VcdImageSink;

typedef struct {
  uint32_t lsn;
  enum {
    VCD_CUE_TRACK_START = 1, /* INDEX 0 -> 1 transition, TOC entry */
    VCD_CUE_PREGAP_START,    /* INDEX = 0 start */
    VCD_CUE_SUBINDEX,        /* INDEX++; sub-index */
    VCD_CUE_END,             /* lead-out start */
    VCD_CUE_LEADIN,          /* lead-in start */
  } type;
} vcd_cue_t;

typedef struct {
  int (*set_cuesheet) (void *user_data, const VcdList *vcd_cue_list);
  int (*write) (void *user_data, const void *buf, lsn_t lsn);
  void (*free) (void *user_data);
  int (*set_arg) (void *user_data, const char key[], const char value[]);
} vcd_image_sink_funcs;

VcdImageSink *
vcd_image_sink_new (void *user_data, const vcd_image_sink_funcs *funcs);

void
vcd_image_sink_destroy (VcdImageSink *obj);

int
vcd_image_sink_set_cuesheet (VcdImageSink *obj, const VcdList *vcd_cue_list);

int
vcd_image_sink_write (VcdImageSink *obj, void *buf, lsn_t lsn);

/*!
  Set the arg "key" with "value" in the target device.
*/
int
vcd_image_sink_set_arg (VcdImageSink *obj, const char key[], 
			const char value[]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VCD_IMAGE_H__ */
