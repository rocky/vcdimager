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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <popt.h>
#include <errno.h>
#include <stdint.h>

#ifdef __linux__
#warning expect missing prototype warnings from swab.h
#include <linux/cdrom.h>
#endif

#include "vcd_bytesex.h"
#include "vcd_cd_sector.h"
#include "vcd_files.h"
#include "vcd_files_private.h"
#include "vcd_iso9660.h"
#include "vcd_logging.h"
#include "vcd_transfer.h"

static const char DELIM[] = \
"----------------------------------------" \
"---------------------------------------\n";

static uint32_t
_get_psd_size (const void *info_p)
{
  InfoVcd info;

  memcpy (&info, info_p, ISO_BLOCKSIZE);

  return UINT32_FROM_BE (info.psd_size);
}

static void
_hexdump (const void *data, unsigned len)
{
  unsigned n;
  const uint8_t *bytes = data;

  for (n = 0; n < len; n++)
    {
      if (n % 8 == 0)
        fprintf (stdout, " ");
      fprintf (stdout, "%2.2x ", bytes[n]);
    }
}

static vcd_type_t gl_vcd_type = VCD_TYPE_INVALID;

static void
dump_lot_and_psd_vcd (const void *data, const void *data2,
                      uint32_t psd_size)
{
  const LotVcd *lot = data;
  const uint8_t *psd_data = data2;
  uint32_t n, tmp;

  fprintf (stdout, "VCD/LOT.VCD\n");

  n = 0;
  while ((tmp = UINT16_FROM_BE (lot->offset[n])) != 0xFFFF)
    fprintf (stdout, " offset[%d]: 0x%4.4x (real offset = %d)\n", n++, tmp, tmp << 3);

  fprintf (stdout, DELIM);

  fprintf (stdout, "VCD/PSD.VCD\n");

  assert (psd_size % 8 == 0);

  n = 0;
  while ((tmp = UINT16_FROM_BE (lot->offset[n])) != 0xFFFF)
    {
      uint8_t type;

      n++;

      tmp <<= 3;
      assert (tmp < psd_size);

      type = psd_data[tmp];

      switch (type)
        {
        case PSD_TYPE_PLAY_LIST:
          {
            const PsdPlayListDescriptor *d = (const void *) (psd_data + tmp);
            int i;

            fprintf (stdout, " [%2d]: play list descriptor ( NOI: %d | LID#: %d "
                     "| prev: %4.4x | next: %4.4x | retn: %4.4x | ptime: %d "
                     "| wait: %d | await: %d",
                     n - 1,
                     d->noi,
                     UINT16_FROM_BE (d->lid),
                     UINT16_FROM_BE (d->prev_ofs),
                     UINT16_FROM_BE (d->next_ofs),
                     UINT16_FROM_BE (d->retn_ofs),
                     UINT16_FROM_BE (d->ptime), d->wtime, d->atime);

            for (i = 0; i < d->noi; i++)
              fprintf (stdout, " | itemid[%d]: %d",
                       i, UINT16_FROM_BE (d->itemid[i]));
            fprintf (stdout, ")\n");
          }
          break;
        case PSD_TYPE_END_OF_LIST:
          {
            const PsdEndOfListDescriptor *d =
              (const void *) (psd_data + tmp);
            fprintf (stdout, " [%2d]: end of list descriptor (", n - 1);
            _hexdump (d->unknown, 7);
            fprintf (stdout, ")\n");
          }
          break;
        default:
          fprintf (stdout, " [%2d] unkown descriptor type (0x%2.2x) at %d\n", n - 1,
                   type, tmp);

          fprintf (stdout, "  hexdump: ");
          _hexdump (&psd_data[tmp], 24);
          fprintf (stdout, "\n");
          break;
        }
    }


}

static void
dump_info_vcd (const void *data)
{
  InfoVcd info;
  int n;
  const char *bool_str[] = { "no", "yes" };

  memcpy (&info, data, 2048);

  gl_vcd_type = VCD_TYPE_INVALID;

  if (!strncmp (info.ID, INFO_ID_VCD, sizeof (info.ID)))
    gl_vcd_type = VCD_TYPE_VCD2;
  else if (!strncmp (info.ID, INFO_ID_SVCD, sizeof (info.ID)))
    gl_vcd_type = VCD_TYPE_SVCD;

  switch (gl_vcd_type)
    {
    case VCD_TYPE_VCD2:
      fprintf (stdout, "VCD detected\n");
      break;
    case VCD_TYPE_SVCD:
      fprintf (stdout, "SVCD detected\n");
      break;
    case VCD_TYPE_INVALID:
      fprintf (stderr, "unknown ID encountered -- maybe not a proper (S)VCD?\n");
      exit (EXIT_FAILURE);
      break;
    }

  fprintf (stdout, "VCD/INFO.VCD\n");

  fprintf (stdout, " ID: `%.8s'\n", info.ID);
  fprintf (stdout, " version: 0x%2.2x\n", info.version);
  fprintf (stdout, " system profile tag: 0x%2.2x\n", info.sys_prof_tag);
  fprintf (stdout, " album desc: `%.16s'\n", info.album_desc);
  fprintf (stdout, " volume count: %d\n", UINT16_FROM_BE (info.vol_count));
  fprintf (stdout, " volume number: %d\n", UINT16_FROM_BE (info.vol_id));

  fprintf (stdout, " pal flags: 0x");
  for (n = 0; n < sizeof (info.pal_flags); n++)
    fprintf (stdout, "%2.2x ", info.pal_flags[n]);
  fprintf (stdout, "\n");

  fprintf (stdout, " flags:\n");
  fprintf (stdout, "  restriction: %d\n", info.flags.restriction);
  fprintf (stdout, "  special info: %s\n", bool_str[info.flags.special_info]);
  fprintf (stdout, "  user data cc: %s\n", bool_str[info.flags.user_data_cc]);
  fprintf (stdout, "  start lid#1: %s\n", bool_str[info.flags.use_lid1]);
  fprintf (stdout, "  start track#3: %s\n", bool_str[info.flags.use_track3]);

  fprintf (stdout, " psd size: %d\n", UINT32_FROM_BE (info.psd_size));
  fprintf (stdout, " first segment addr: %2.2x:%2.2x:%2.2x\n",
           info.first_seg_addr[0],
           info.first_seg_addr[1], info.first_seg_addr[2]);

  fprintf (stdout, " offset multiplier: 0x%2.2x (must be 0x08)\n", info.offset_mult);

  fprintf (stdout, " highest psd offset: 0x%4.4x (real offset = %d)\n",
           UINT16_FROM_BE (info.last_psd_ofs),
           UINT16_FROM_BE (info.last_psd_ofs) << 3);

  fprintf (stdout, " item count: %d\n", UINT16_FROM_BE (info.item_count));

  for (n = 0; n < UINT16_FROM_BE (info.item_count); n++)
    fprintf (stdout, "  Item contents[%d]: audio type %d,"
             " videotype %d, continuation %s\n",
             n,
             info.spi_contents[n].audio_type,
             info.spi_contents[n].video_type,
             bool_str[info.spi_contents[n].item_cont]);
}

static void
dump_entries_vcd (const void *data)
{
  EntriesVcd entries;
  int ntracks, n;

  memcpy (&entries, data, 2048);

  ntracks = UINT16_FROM_BE (entries.tracks);

  fprintf (stdout, "VCD/ENTRIES.VCD\n");
  fprintf (stdout, " ID: `%.8s'\n", entries.ID);
  fprintf (stdout, " version: 0x%2.2x\n", entries.version);

  fprintf (stdout, " entries: %d\n", ntracks);

  for (n = 0; n < ntracks; n++)
    {
      const msf_t *msf = &(entries.entry[n].msf);
      uint32_t extent = msf_to_lba(msf);
      extent -= 150;

      fprintf (stdout, " entry %d starts in track %d at (lba) extent %d "
               "(msf: %2.2x:%2.2x:%2.2x)\n",
               n + 1, from_bcd8 (entries.entry[n].n), extent,
               msf->m, msf->s, msf->f);
    }

}

static void
dump_all (const void *info_p, const void *entries_p,
          const void *lot_p, const void *psd_p)
{
  fprintf (stdout, DELIM);
  dump_info_vcd (info_p);
  fprintf (stdout, DELIM);
  dump_entries_vcd (entries_p);
  if (lot_p)
    {
      fprintf (stdout, DELIM);
      dump_lot_and_psd_vcd (lot_p, psd_p, _get_psd_size (info_p));
    }
  fprintf (stdout, DELIM);
}

static void
dump_device (const char device_fname[])
{
  FILE *fd = fopen (device_fname, "rb");
  char info_buf[ISO_BLOCKSIZE] = { 0, };
  char entries_buf[ISO_BLOCKSIZE] = { 0, };
  char *lot_buf = NULL, *psd_buf = NULL;
  uint32_t psd_size;
  uint32_t size = -1;

  if (!fd)
    {
      perror ("fopen()");
      exit (EXIT_FAILURE);
    }

  {
    struct cdrom_tocentry tocent;

    tocent.cdte_track = CDROM_LEADOUT;
    tocent.cdte_format = CDROM_LBA;
    if (ioctl (fileno (fd), CDROMREADTOCENTRY, &tocent) == -1)
      {
        perror ("ioctl(CDROMREADTOCENTRY)");
        exit (EXIT_FAILURE);
      }
    size = tocent.cdte_addr.lba;
  }

  fprintf (stdout, DELIM);

  fprintf (stdout, "Source: `%s'\n", device_fname);
  fprintf (stdout, "Image size: %d sectors\n", size);

  fseek (fd, INFO_VCD_SECTOR * ISO_BLOCKSIZE, SEEK_SET);
  fread (info_buf, ISO_BLOCKSIZE, 1, fd);
  psd_size = _get_psd_size (info_buf);

  fseek (fd, ENTRIES_VCD_SECTOR * ISO_BLOCKSIZE, SEEK_SET);
  fread (entries_buf, ISO_BLOCKSIZE, 1, fd);
  
  if (psd_size)
    {
      int n;

      lot_buf = malloc (ISO_BLOCKSIZE * 32);
      psd_buf =
        malloc (ISO_BLOCKSIZE * (((psd_size - 1) / ISO_BLOCKSIZE) + 1));
      
      for (n = LOT_VCD_SECTOR; n < PSD_VCD_SECTOR; n++)
        {
          fseek (fd, n * ISO_BLOCKSIZE, SEEK_SET);
          fread (lot_buf + (ISO_BLOCKSIZE * (n - LOT_VCD_SECTOR)),
                 ISO_BLOCKSIZE, 1, fd);
        }

      for (n = PSD_VCD_SECTOR;
           n < PSD_VCD_SECTOR + ((psd_size - 1) / ISO_BLOCKSIZE) + 1; n++)
        {
          fseek (fd, n * ISO_BLOCKSIZE, SEEK_SET);
          fread (psd_buf + (ISO_BLOCKSIZE * (n - PSD_VCD_SECTOR)),
                 ISO_BLOCKSIZE, 1, fd);
        }
    }

  dump_all (info_buf, entries_buf, lot_buf, psd_buf);

  free (lot_buf);
  free (psd_buf);

  fclose (fd);
}

static void
read_raw_mode2_sector (FILE *fd, void *data, uint32_t extent)
{
  char buf[CDDA_SIZE] = { 0, };

  assert (fd != NULL);

  if (fseek (fd, extent * CDDA_SIZE, SEEK_SET))
    vcd_error ("fseek(): %s", strerror (errno));

  fread (buf, CDDA_SIZE, 1, fd);

  if (ferror (fd))
    vcd_error ("fwrite(): %s", strerror (errno));

  if (feof (fd))
    vcd_error ("fread(): unexpected EOF");

  memcpy (data, buf + 16, M2RAW_SIZE);
}

static void
dump_file (const char image_fname[])
{
  FILE *fd = fopen (image_fname, "rb");
  char buf[M2RAW_SIZE] = { 0, };
  char info_buf[ISO_BLOCKSIZE] = { 0, };
  char entries_buf[ISO_BLOCKSIZE] = { 0, };
  char *lot_buf = NULL, *psd_buf = NULL;
  struct stat statbuf;
  uint32_t size, psd_size;

  if (!fd)
    {
      perror ("fopen()");
      exit (EXIT_FAILURE);
    }

  if (fstat (fileno (fd), &statbuf))
    {
      perror ("fstat()");
      exit (EXIT_FAILURE);
    }

  size = statbuf.st_size;

  if (size % CDDA_SIZE)
    {
      fprintf (stderr, "file not multiple of blocksize\n");
      exit (EXIT_FAILURE);
    }

  size /= CDDA_SIZE;

  read_raw_mode2_sector (fd, buf, INFO_VCD_SECTOR);
  memcpy (info_buf, buf + 8, ISO_BLOCKSIZE);
  psd_size = _get_psd_size (info_buf);

  read_raw_mode2_sector (fd, buf, ENTRIES_VCD_SECTOR);
  memcpy (entries_buf, buf + 8, ISO_BLOCKSIZE);

  if (psd_size)
    {
      int n;

      lot_buf = malloc (ISO_BLOCKSIZE * 32);
      psd_buf = malloc (ISO_BLOCKSIZE * (((psd_size - 1) / ISO_BLOCKSIZE) + 1));

      for (n = LOT_VCD_SECTOR; n < PSD_VCD_SECTOR; n++)
        {
          read_raw_mode2_sector (fd, buf, n);
          memcpy (lot_buf + (ISO_BLOCKSIZE * (n - LOT_VCD_SECTOR)), buf + 8,
                  ISO_BLOCKSIZE);
        }

      for (n = PSD_VCD_SECTOR;
           n < PSD_VCD_SECTOR + ((psd_size - 1) / ISO_BLOCKSIZE) + 1; n++)
        {
          read_raw_mode2_sector (fd, buf, n);
          memcpy (psd_buf + (ISO_BLOCKSIZE * (n - PSD_VCD_SECTOR)), buf + 8,
                  ISO_BLOCKSIZE);
        }
    }

  fprintf (stdout, DELIM);

  fprintf (stdout, "Source: image file `%s'\n", image_fname);
  fprintf (stdout, "Image size: %d sectors\n", size);

  dump_all (info_buf, entries_buf, lot_buf, psd_buf);

  free (lot_buf);
  free (psd_buf);

  fclose (fd);
}

static void
rip_file (const char device_fname[])
{
  fprintf (stderr, "ripping file image not supported yet\n");
}

static void
rip_device (const char device_fname[])
{
  FILE *fd = fopen (device_fname, "rb");
  EntriesVcd entries;
  uint32_t size = -1;
  int i;

  if (!fd)
    {
      perror ("fopen()");
      exit (EXIT_FAILURE);
    }

  {
    struct cdrom_tocentry tocent;

    tocent.cdte_track = CDROM_LEADOUT;
    tocent.cdte_format = CDROM_LBA;
    if (ioctl (fileno (fd), CDROMREADTOCENTRY, &tocent) == -1)
      {
        perror ("ioctl(CDROMREADTOCENTRY)");
        exit (EXIT_FAILURE);
      }
    size = tocent.cdte_addr.lba;
  }

  memset (&entries, 0, sizeof (EntriesVcd));

  assert (sizeof (EntriesVcd) == ISO_BLOCKSIZE);

  fseek (fd, ENTRIES_VCD_SECTOR * ISO_BLOCKSIZE, SEEK_SET);
  fread (&entries, ISO_BLOCKSIZE, 1, fd);

  for (i = 0; i < UINT16_FROM_BE (entries.tracks); i++)
    {
      uint32_t startlba = msf_to_lba (&(entries.entry[i].msf)) - 150;

      uint32_t endlba = (i + 1 == UINT16_FROM_BE (entries.tracks))
        ? size : (msf_to_lba (&(entries.entry[i + 1].msf)) - 150);

      uint32_t pos = -1;

      FILE *outfd = NULL;

      char fname[80] = { 0, };

      snprintf (fname, sizeof (fname), "track_%2.2d.mpg", i);

      fprintf (stdout, "%s: %d -> %d\n", fname, startlba, endlba);

      if (!(outfd = fopen (fname, "wb")))
        {
          perror ("fopen()");
          exit (EXIT_FAILURE);
        }

      if (ioctl (fileno (fd), CDROM_SELECT_SPEED, 8) == -1)
        {
          perror ("ioctl()");
          exit (EXIT_FAILURE);
        }

      for (pos = startlba; pos < endlba; pos++)
        {
          struct m2f2sector
          {
            uint8_t subheader[8];
            uint8_t data[2324];
            uint8_t spare[4];
          }
          buf;

          struct cdrom_msf *msf = (struct cdrom_msf *) &buf;

          memset (&buf, 0, sizeof (buf));

          msf->cdmsf_min0 = pos / (60 * 75);
          msf->cdmsf_sec0 = ((pos / 75) % 60 + 2);
          msf->cdmsf_frame0 = pos % 75;

          if (!msf->cdmsf_frame0)
            fprintf (stdout, "debug: %2.2d:%2.2d:%2.2d\n",
                     msf->cdmsf_min0, msf->cdmsf_sec0, msf->cdmsf_frame0);

          if (ioctl (fileno (fd), CDROMREADMODE2, &buf) == -1)
            {
              perror ("ioctl()");
              exit (EXIT_FAILURE);
            }

          if (!buf.subheader[1])
            {
              fprintf (stdout, "skipping...\n");
              continue;
            }

          fwrite (buf.data, 2324, 1, outfd);
        }

      fclose (outfd);
    }

  fclose (fd);
}


/* global static vars */

static char *gl_source_name = NULL;

static enum
{
  SOURCE_NONE = 0,
  SOURCE_FILE = 1,
  SOURCE_DIRECTORY = 2,
  SOURCE_DEVICE = 3
}
gl_source_type = SOURCE_NONE;

static enum
{
  OP_NOOP = 0,
  OP_DUMP = 1 << 4,
  OP_RIP = 1 << 5,
}
gl_operation = OP_NOOP;

/* end of vars */

int
main (int argc, const char *argv[])
{
  int opt;

  struct poptOption optionsTable[] = {

    {"bin-file", '\0', POPT_ARG_STRING, &gl_source_name, SOURCE_FILE,
     "set image file as source", "FILE"},

    {"vcd-dir", '\0', POPT_ARG_STRING, &gl_source_name, SOURCE_DIRECTORY,
     "set VCD directory as source (only dump)", "DIR"},

#ifdef __linux__
    {"cdrom-device", '\0', POPT_ARG_STRING, &gl_source_name, SOURCE_DEVICE,
     "set CDROM device as source (only linux)", "DEVICE"},
#endif

    {"dump", '\0', POPT_ARG_NONE, NULL, OP_DUMP,
     "dump information about VideoCD"},

    {"rip", '\0', POPT_ARG_NONE, NULL, OP_RIP,
     "rip data tracks"},

    {"version", 'V', POPT_ARG_NONE, NULL, 4,
     "display version and copyright information and exit"},
    POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
  };

  poptContext optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);

  /* end of local declarations */

  while ((opt = poptGetNextOpt (optCon)) != -1)
    switch (opt)
      {
      case 0:
        fprintf (stdout, "GNU VCDRip " VERSION "\n\n"
                 "Copyright (c) 2000 Herbert Valerio Riedel <hvr@gnu.org>\n\n"
                 "VCDImager may be distributed under the terms of the GNU General Public Licence;\n"
                 "For details, see the file `COPYING', which is included in the VCDImager\n"
                 "distribution. There is no warranty, to the extent permitted by law.\n");
        exit (EXIT_SUCCESS);
        break;
      case SOURCE_FILE:
      case SOURCE_DIRECTORY:
      case SOURCE_DEVICE:
        if (gl_source_type != SOURCE_NONE)
          {
            fprintf (stderr, "only one source allowed! - try --help\n");
            exit (EXIT_FAILURE);
          }

        gl_source_type = opt;
        break;

      case OP_RIP:
      case OP_DUMP:
        gl_operation |= opt;
        break;
      default:
        fprintf (stderr, "error while parsing command line - try --help\n");
        exit (EXIT_FAILURE);
      }

  if (poptGetArgs (optCon) != NULL)
    {
      fprintf (stderr, "error - no arguments expected!\n");
      exit (EXIT_FAILURE);
    }

  if (gl_operation == OP_RIP && gl_source_type == SOURCE_DIRECTORY)
    {
      fprintf (stderr, "ripping from directory source not supported\n");
      exit (EXIT_FAILURE);
    }

  if (gl_operation == OP_NOOP)
    {
      fprintf (stderr, "no operation requested -- nothing to do\n");
      exit (EXIT_FAILURE);
    }

  if (gl_operation & OP_DUMP)
    switch (gl_source_type)
      {
      case SOURCE_DIRECTORY:
        fprintf (stderr, "source type not implemented yet!\n");
        exit (EXIT_FAILURE);
        break;
      case SOURCE_DEVICE:
        dump_device (gl_source_name);
        break;
      case SOURCE_FILE:
        dump_file (gl_source_name);
        break;
      default:
        fprintf (stderr, "no source given -- can't do anything...\n");
        exit (EXIT_FAILURE);
      }

  if (gl_operation & OP_RIP)
    switch (gl_source_type)
      {
      case SOURCE_DIRECTORY:
        fprintf (stderr, "source type not implemented yet!\n");
        exit (EXIT_FAILURE);
        break;
      case SOURCE_DEVICE:
        rip_device (gl_source_name);
        break;
      case SOURCE_FILE:
        rip_file (gl_source_name);
        break;
      default:
        fprintf (stderr, "no source given -- can't do anything...\n");
        exit (EXIT_FAILURE);
      }

  return EXIT_SUCCESS;
}

/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
