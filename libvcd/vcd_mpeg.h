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

#ifndef __VCD_MPEG_H__
#define __VCD_MPEG_H__

#include <string.h>

#include <libvcd/vcd_types.h>
#include <libvcd/vcd_mpeg_stream.h>
#include <libvcd/vcd_logging.h>

struct vcd_mpeg_scan_data_t {
  uint8_t tag        GNUC_PACKED;
  uint8_t len        GNUC_PACKED;
  msf_t prev_ofs     GNUC_PACKED;
  msf_t next_ofs     GNUC_PACKED;
  msf_t back_ofs     GNUC_PACKED;
  msf_t forw_ofs     GNUC_PACKED;
};

#define VCD_MPEG_SCAN_DATA_WARNS 8

typedef struct {
  struct {
    bool video[3];
    bool audio[3];

    bool ogt;
    bool padding;
    bool pem;
    bool zero;
    bool system_header;

    struct vcd_mpeg_scan_data_t *scan_data_ptr; /* points into actual packet memory! */

    enum aps_t {
      APS_NONE = 0,
      APS_I,    /* iframe */
      APS_GI,   /* gop + iframe */
      APS_SGI   /* sequence + gop + iframe */
    } aps;
    double aps_pts;

    bool has_pts;
    double pts;

    uint64_t scr;
    uint32_t muxrate;

    bool gop;
    struct {
      uint8_t h, m, s, f;
    } gop_timecode;
  } packet;

  struct {
    unsigned packets;

    int first_shdr;
    struct {
      bool seen;
      unsigned hsize;
      unsigned vsize;
      double aratio;
      double frate;
      unsigned bitrate;
      unsigned vbvsize;
      bool constrained_flag;
    } shdr[3];

    struct {
      bool seen;
      unsigned layer;
      unsigned bitrate;
      unsigned sampfreq;
      enum {
        MPEG_STEREO,
        MPEG_JOINT_STEREO,
        MPEG_DUAL_CHANNEL,
        MPEG_SINGLE_CHANNEL
      } mode;
    } ahdr[3];

    bool video[3];
    bool audio[3];

    mpeg_vers_t version;

    bool seen_pts;
    double min_pts;
    double max_pts;

    double last_aps_pts[3];

    unsigned scan_data;
    unsigned scan_data_warnings;
  } stream;
} VcdMpegStreamCtx;

int
vcd_mpeg_parse_packet (const void *buf, unsigned buflen, bool parse_pes,
                       VcdMpegStreamCtx *ctx);

mpeg_norm_t 
vcd_mpeg_get_norm (unsigned hsize, unsigned vsize, double frate);

#endif /* __VCD_MPEG_H__ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
