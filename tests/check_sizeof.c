/*
    $Id$

    Copyright (C) 2001 Herbert Valerio Riedel <hvr@gnu.org>

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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <libvcd/vcd.h>
#include <libvcd/vcd_types.h>
#include <libvcd/vcd_pbc.h>
#include <libvcd/vcd_xa.h>
#include <libvcd/vcd_iso9660.h>
#include <libvcd/vcd_iso9660_private.h>
#include <libvcd/vcd_files.h>
#include <libvcd/vcd_files_private.h>
#include <libvcd/vcd_cd_sector.h>
#include <libvcd/vcd_cd_sector_private.h>
#include <libvcd/vcd_mmc_private.h>

#define CHECK_SIZEOF(typnam) { \
  printf ("checking sizeof (%s) ...", #typnam); \
  if (sizeof (typnam) != (typnam##_SIZEOF)) { \
      printf ("failed!\n==> sizeof (%s) == %d (but should be %d)\n", \
              #typnam, (int)sizeof(typnam), (typnam##_SIZEOF)); \
      fail++; \
  } else { pass++; printf ("ok!\n"); } \
}

#define CHECK_SIZEOF_STRUCT(typnam) { \
  printf ("checking sizeof (struct %s) ...", #typnam); \
  if (sizeof (struct typnam) != (struct_##typnam##_SIZEOF)) { \
      printf ("failed!\n==> sizeof (struct %s) == %d (but should be %d)\n", \
              #typnam, (int)sizeof(struct typnam), (struct_##typnam##_SIZEOF)); \
      fail++; \
  } else { pass++; printf ("ok!\n"); } \
}

int main (int argc, const char *argv[])
{
  unsigned fail = 0, pass = 0;

  /* vcd_types.h */
  CHECK_SIZEOF(msf_t);

  /* vcd_pbc.h */
  CHECK_SIZEOF_STRUCT(psd_area_t);
  CHECK_SIZEOF(pbc_area_t);

  /* vcd_mpeg.h */
  CHECK_SIZEOF_STRUCT(vcd_mpeg_scan_data_t);

  /* vcd_xa.h */
  CHECK_SIZEOF(vcd_xa_t);

  /* vcd_iso9660_private.h */
  CHECK_SIZEOF_STRUCT(iso_volume_descriptor);
  CHECK_SIZEOF_STRUCT(iso_primary_descriptor);
  CHECK_SIZEOF_STRUCT(iso_path_table);
  CHECK_SIZEOF_STRUCT(iso_directory_record);

  /* vcd_files_private.h */
  CHECK_SIZEOF(EntriesVcd);
  CHECK_SIZEOF(InfoStatusFlags);
  CHECK_SIZEOF(InfoSpiContents);
  CHECK_SIZEOF(InfoVcd);
  CHECK_SIZEOF(LotVcd);

  CHECK_SIZEOF(PsdEndListDescriptor);
  CHECK_SIZEOF(PsdSelectionListFlags);
  CHECK_SIZEOF(PsdSelectionListDescriptor);
  CHECK_SIZEOF(PsdSelectionListDescriptorExtended);
  CHECK_SIZEOF(PsdCommandListDescriptor);
  CHECK_SIZEOF(PsdPlayListDescriptor);
  CHECK_SIZEOF(SVDTrackContent);
  CHECK_SIZEOF(TracksSVD);
  CHECK_SIZEOF(TracksSVD2);
  CHECK_SIZEOF(TracksSVD_v30);
  CHECK_SIZEOF(SearchDat);
  CHECK_SIZEOF(SpicontxSvd);
  CHECK_SIZEOF(ScandataDat_v2);
  CHECK_SIZEOF(ScandataDat1);
  CHECK_SIZEOF(ScandataDat2);
  CHECK_SIZEOF(ScandataDat3);
  CHECK_SIZEOF(ScandataDat4);

  /* vcd_cd_sector_private.h */
  CHECK_SIZEOF(raw_cd_sector_t);
  CHECK_SIZEOF(sector_header_t);
  CHECK_SIZEOF(mode0_sector_t);
  CHECK_SIZEOF(mode2_form1_sector_t);
  CHECK_SIZEOF(mode2_form2_sector_t);

  /* vcd_mmc_private.h */
  CHECK_SIZEOF_STRUCT(disc_info);
  CHECK_SIZEOF_STRUCT(track_info);
  CHECK_SIZEOF_STRUCT(mode_page_header);
  CHECK_SIZEOF_STRUCT(write_parameters_mode_page);
  CHECK_SIZEOF_STRUCT(cd_capabilities_mode_page);
  CHECK_SIZEOF_STRUCT(opc_table);
  CHECK_SIZEOF_STRUCT(disc_capacity);
  CHECK_SIZEOF_STRUCT(cue_sheet_data);

  if (fail)
    return 1;

  return 0;
}
