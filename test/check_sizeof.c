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

#include <cdio/iso9660.h>
#include <libvcd/types.h>
#include <libvcd/files.h>
#include <libvcd/files_private.h>
#include <libvcd/sector.h>

/* Private headers */
#include "pbc.h"
#include "sector_private.h"
#include "vcd.h"

#define CHECK_SIZEOF(typnam) { \
  printf ("checking sizeof (%s) ...", #typnam); \
  if (sizeof (typnam) != (typnam##_SIZEOF)) { \
      printf ("failed!\n==> sizeof (%s) == %d (but should be %d)\n", \
              #typnam, (int)sizeof(typnam), (int)(typnam##_SIZEOF)); \
      fail++; \
  } else { pass++; printf ("ok!\n"); } \
}

#define CHECK_SIZEOF_STRUCT(typnam) { \
  printf ("checking sizeof (struct %s) ...", #typnam); \
  if (sizeof (struct typnam) != (struct_##typnam##_SIZEOF)) { \
      printf ("failed!\n==> sizeof (struct %s) == %d (but should be %d)\n", \
              #typnam, (int)sizeof(struct typnam), (int)(struct_##typnam##_SIZEOF)); \
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

  if (fail)
    return 1;

  return 0;
}
