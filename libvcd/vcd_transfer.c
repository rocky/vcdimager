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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "vcd_transfer.h"
#include "vcd_cd_sector.h"
#include "vcd_directory.h"
#include "vcd_logging.h"
#include "vcd_salloc.h"

static const char _rcsid[] = "$Id$";

void
write_mode2_sector(VcdDataSink *sink, int sect2336, const void *data,
                   uint32_t extent, uint8_t fnum, uint8_t cnum, 
                   uint8_t sm, uint8_t ci)
{
  char buf[CDDA_SIZE] = { 0, };
  

  assert(sink != NULL);
  
  make_mode2(buf, data, extent, fnum, cnum, sm, ci);

/*   if(fseek(fd, extent*CDDA_SIZE, SEEK_SET)) */
/*     vcd_error("fseek(): %s", g_strerror(errno)); */

  vcd_data_sink_seek(sink, extent*(sect2336 ? M2RAW_SIZE : CDDA_SIZE));

  if(sect2336) 
    vcd_data_sink_write(sink, buf+12, M2RAW_SIZE, 1);
  else
    vcd_data_sink_write(sink, buf, CDDA_SIZE, 1);

/*   fwrite(buf, CDDA_SIZE, 1, fd); */
  
/*   if(ferror(fd))  */
/*     vcd_error("fwrite(): %s", g_strerror(errno)); */
}

void
write_mode2_raw_sector(VcdDataSink *sink, int sect2336,
                       const void *data, uint32_t extent)
{
  char buf[CDDA_SIZE] = { 0, };

  assert(sink != NULL);
  
  make_raw_mode2(buf, data, extent);

  vcd_data_sink_seek(sink, extent*(sect2336 ? M2RAW_SIZE : CDDA_SIZE));

  if(sect2336) 
    vcd_data_sink_write(sink, buf+12, M2RAW_SIZE, 1);
  else
    vcd_data_sink_write(sink, buf, CDDA_SIZE, 1);

/*   if(fseek(fd, extent*CDDA_SIZE, SEEK_SET))  */
/*     vcd_error("fseek(): %s", g_strerror(errno)); */

/*   fwrite(buf, CDDA_SIZE, 1, fd); */

/*   if(ferror(fd))  */
/*     vcd_error("fwrite(): %s", g_strerror(errno)); */
}


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
