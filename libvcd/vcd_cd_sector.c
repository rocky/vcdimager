/*
    $Id$

    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>
              (C) 1998 Heiko Eissfeldt <heiko@colossus.escape.de>
                  portions used& Chris Smith

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

#include <string.h>
#include <assert.h>

#include "vcd_types.h"

#include "vcd_cd_sector.h"
#include "vcd_cd_sector_private.h"
#include "vcd_bytesex.h"
#include "vcd_salloc.h"

const static uint8_t sync_pattern[] = {
  0x00, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0x00
};

static void
build_address(void * buf, sectortype_t sectortype, uint32_t address)
{
  raw_cd_sector_t *sector = buf;
  
  assert(sizeof(raw_cd_sector_t) == CDDA_SIZE-DATA_LEN);

  sector->msf[0] = to_bcd8(address / (60*75));
  sector->msf[1] = to_bcd8((address / 75) % 60);
  sector->msf[2] = to_bcd8(address % 75);

  switch(sectortype) {
  case MODE_0:
    sector->mode = 0;
    break;
  case MODE_2:
  case MODE_2_FORM_1:
  case MODE_2_FORM_2:
    sector->mode = 2;
    break;
  default:
    assert(0);
    break;
  }
}

static uint32_t
build_edc(const void *in, unsigned from, unsigned upto)
{
  const uint8_t *p = (uint8_t*)in+from;
  uint32_t result = 0;

  for (; from <= upto; from++)
    result = EDC_crctable[(result ^ *p++) & 0xffL] ^ (result >> 8);

  return result;
}

static void
encode_L2_Q(uint8_t inout[4 + L2_RAW + 4 + 8 + L2_P + L2_Q])
{
  uint8_t *Q;
  int i,j;

  Q = inout + 4 + L2_RAW + 4 + 8 + L2_P;
  memset(Q, 0, L2_Q);
  for (j = 0; j < 26; j++) {
    for (i = 0; i < 43; i++) {
      uint8_t data;

      /* LSB */
      data = inout[(j*43*2+i*2*44) % (4 + L2_RAW + 4 + 8 + L2_P)];
      if (data != 0) {
        uint32_t base = rs_l12_log[data];

        uint32_t sum = base + DQ[0][i];
        if (sum >= ((1 << RS_L12_BITS)-1))
          sum -= (1 << RS_L12_BITS)-1;
                
        Q[0] ^= rs_l12_alog[sum];

        sum = base + DQ[1][i];
        if (sum >= ((1 << RS_L12_BITS)-1))
          sum -= (1 << RS_L12_BITS)-1;
                
        Q[26*2] ^= rs_l12_alog[sum];
      }
      /* MSB */
      data = inout[(j*43*2+i*2*44+1) % (4 + L2_RAW + 4 + 8 + L2_P)];
      if (data != 0) {
        uint32_t base = rs_l12_log[data];

        uint32_t sum = base+DQ[0][i];
        if (sum >= ((1 << RS_L12_BITS)-1))
          sum -= (1 << RS_L12_BITS)-1;
                
        Q[1] ^= rs_l12_alog[sum];

        sum = base + DQ[1][i];
        if (sum >= ((1 << RS_L12_BITS)-1))
          sum -= (1 << RS_L12_BITS)-1;
                
        Q[26*2+1] ^= rs_l12_alog[sum];
      }
    }
    Q += 2;
  }
}

static void
encode_L2_P(uint8_t inout[4 + L2_RAW + 4 + 8 + L2_P])
{
  uint8_t *P;
  int i,j;

  P = inout + 4 + L2_RAW + 4 + 8;
  memset(P, 0, L2_P);
  for (j = 0; j < 43; j++) {
    for (i = 0; i < 24; i++) {
      uint8_t data;

      /* LSB */
      data = inout[i*2*43];
      if (data != 0) {
        uint32_t base = rs_l12_log[data];
                
        uint32_t sum = base + DP[0][i];
        if (sum >= ((1 << RS_L12_BITS)-1))
          sum -= (1 << RS_L12_BITS)-1;

        P[0] ^= rs_l12_alog[sum];

        sum = base + DP[1][i];
        if (sum >= ((1 << RS_L12_BITS)-1))
          sum -= (1 << RS_L12_BITS)-1;
                
        P[43*2] ^= rs_l12_alog[sum];
      }
      /* MSB */
      data = inout[i*2*43+1];
      if (data != 0) {
        uint32_t base = rs_l12_log[data];

        uint32_t sum = base + DP[0][i];
        if (sum >= ((1 << RS_L12_BITS)-1))
          sum -= (1 << RS_L12_BITS)-1;
                
        P[1] ^= rs_l12_alog[sum];

        sum = base + DP[1][i];
        if (sum >= ((1 << RS_L12_BITS)-1))
          sum -= (1 << RS_L12_BITS)-1;
                
        P[43*2+1] ^= rs_l12_alog[sum];
      }
    }
    P += 2;
    inout += 2;
  }
}

/* Layer 2 Product code en/decoder */
static void
do_encode_L2(void *buf, sectortype_t sectortype, uint32_t address)
{
  raw_cd_sector_t *raw_sector = buf;

  assert(buf != NULL);

  assert(sizeof(sync_pattern) == SYNC_LEN);
  assert(sizeof(mode2_form1_sector_t) == CDDA_SIZE);
  assert(sizeof(mode2_form2_sector_t) == CDDA_SIZE);
  assert(sizeof(mode0_sector_t) == CDDA_SIZE);
  assert(sizeof(raw_cd_sector_t) == SYNC_LEN+HEADER_LEN);

  memset(raw_sector, 0, SYNC_LEN+HEADER_LEN);
  memcpy(raw_sector->sync, sync_pattern, sizeof(sync_pattern));

  switch (sectortype) {
  case MODE_0:
    {
      mode0_sector_t *sector = buf;

      memset(sector->data, 0, sizeof(sector->data));
    }
    break;
  case MODE_2:
    break;
  case MODE_2_FORM_1:
    {
      mode2_form1_sector_t *sector = buf;

      sector->edc = UINT32_TO_LE(build_edc(buf, 16, 16+8+2048-1));

      encode_L2_P((uint8_t*)buf+SYNC_LEN);
      encode_L2_Q((uint8_t*)buf+SYNC_LEN);
    }
    break;
  case MODE_2_FORM_2:
    {
      mode2_form2_sector_t *sector = buf;

      sector->edc = UINT32_TO_LE(build_edc(buf, 16, 16+8+2324-1));
    }
    break;
  default:
    assert(0);
  }

  build_address(buf, sectortype, address);
}

void
make_mode2(void *raw_sector, const void *data, uint32_t extent,
           uint8_t fnum, uint8_t cnum, uint8_t sm, uint8_t ci)
{
  uint8_t *subhdr = (uint8_t*)raw_sector+16;

  assert(raw_sector != NULL);
  assert(data != NULL);
  assert(extent != SECTOR_NIL);

  memset(raw_sector, 0, CDDA_SIZE);

  subhdr[0] = subhdr[4] = fnum;
  subhdr[1] = subhdr[5] = cnum;
  subhdr[2] = subhdr[6] = sm;
  subhdr[3] = subhdr[7] = ci;

  if(sm & SM_FORM2) {
    memcpy((char*)raw_sector+12+4+8, data, M2F2_SIZE);
    do_encode_L2(raw_sector, MODE_2_FORM_2, extent+2*75);
  } else {
    memcpy((char*)raw_sector+12+4+8, data, M2F1_SIZE);
    do_encode_L2(raw_sector, MODE_2_FORM_1, extent+2*75);
  } 
}

void
make_raw_mode2(void * raw_sector, const void * data, uint32_t extent)
{
  assert(raw_sector != NULL);
  assert(data != NULL);
  assert(extent != SECTOR_NIL);

  memset(raw_sector, 0, CDDA_SIZE);

  memcpy((char*)raw_sector+12+4, data, M2RAW_SIZE);
  do_encode_L2(raw_sector, MODE_2, extent+2*75);
}



/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
