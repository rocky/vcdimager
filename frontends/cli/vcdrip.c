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

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <popt.h>
#include <errno.h>

#ifdef __linux__
#warning expect missing prototype warnings from swab.h
#include <linux/cdrom.h>
#endif

#include "vcd_cd_sector.h"
#include "vcd_transfer.h"
#include "vcd_files.h"
#include "vcd_files_private.h"
#include "vcd_bytesex.h"
#include "vcd_iso9660.h"
#include "vcd_logging.h"

static const gchar DELIM[] =
"----------------------------------------"
"---------------------------------------\n";

static inline guint32
_get_psd_size (gconstpointer info_p)
{
  InfoVcd info;

  memcpy (&info, info_p, ISO_BLOCKSIZE);

  return GUINT32_FROM_BE (info.psd_size);
}

static void
_hexdump (gconstpointer data, guint len)
{
  guint n;
  const guint8 *bytes = data;

  for (n = 0; n < len; n++)
    {
      if (n % 8 == 0)
        g_print (" ");
      g_print ("%2.2x ", bytes[n]);
    }
}

static vcd_type_t gl_vcd_type = VCD_TYPE_INVALID;

static void
dump_lot_and_psd_vcd (gconstpointer data, gconstpointer data2,
                      guint32 psd_size)
{
  const LotVcd *lot = data;
  const guint8 *psd_data = data2;
  guint32 n, tmp;

  g_print ("VCD/LOT.VCD\n");

  n = 0;
  while ((tmp = GUINT16_FROM_BE (lot->offset[n])) != 0xFFFF)
    g_print (" offset[%d]: 0x%4.4x (real offset = %d)\n", n++, tmp, tmp << 3);

  g_print (DELIM);

  g_print ("VCD/PSD.VCD\n");

  g_assert (psd_size % 8 == 0);

  n = 0;
  while ((tmp = GUINT16_FROM_BE (lot->offset[n++])) != 0xFFFF)
    {
      guint8 type;

      tmp <<= 3;
      g_assert (tmp < psd_size);

      type = psd_data[tmp];

      switch (type)
        {
        case PSD_TYPE_PLAY_LIST:
          {
            const PsdPlayListDescriptor *d = (gconstpointer) (psd_data + tmp);
            int i;

            g_print (" [%2d]: play list descriptor ( NOI: %d | LID#: %d "
                     "| prev: %4.4x | next: %4.4x | retn: %4.4x | ptime: %d "
                     "| wait: %d | await: %d",
                     n - 1,
                     d->noi,
                     GUINT16_FROM_BE (d->lid),
                     GUINT16_FROM_BE (d->prev_ofs),
                     GUINT16_FROM_BE (d->next_ofs),
                     GUINT16_FROM_BE (d->retn_ofs),
                     GUINT16_FROM_BE (d->ptime), d->wtime, d->atime);

            for (i = 0; i < d->noi; i++)
              g_print (" | itemid[%d]: %d",
                       i, GUINT16_FROM_BE (d->itemid[i]));
            g_print (")\n");
          }
          break;
        case PSD_TYPE_END_OF_LIST:
          {
            const PsdEndOfListDescriptor *d =
              (gconstpointer) (psd_data + tmp);
            g_print (" [%2d]: end of list descriptor (", n - 1);
            _hexdump (d->unknown, 7);
            g_print (")\n");
          }
          break;
        default:
          g_print (" [%2d] unkown descriptor type (0x%2.2x) at %d\n", n - 1,
                   type, tmp);

          g_print ("  hexdump: ");
          _hexdump (&psd_data[tmp], 24);
          g_print ("\n");
          break;
        }
    }


}

static void
dump_info_vcd (gconstpointer data)
{
  InfoVcd info;
  gint n;
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
      g_print ("VCD detected\n");
      break;
    case VCD_TYPE_SVCD:
      g_print ("SVCD detected\n");
      break;
    case VCD_TYPE_INVALID:
      g_printerr ("unknown ID encountered -- maybe not a proper (S)VCD?\n");
      exit (EXIT_FAILURE);
      break;
    }

  g_print ("VCD/INFO.VCD\n");

  g_print (" ID: `%.8s'\n", info.ID);
  g_print (" version: 0x%2.2x\n", info.version);
  g_print (" system profile tag: 0x%2.2x\n", info.sys_prof_tag);
  g_print (" album desc: `%.16s'\n", info.album_desc);
  g_print (" volume count: %d\n", GUINT16_FROM_BE (info.vol_count));
  g_print (" volume number: %d\n", GUINT16_FROM_BE (info.vol_id));

  g_print (" pal flags: 0x");
  for (n = 0; n < sizeof (info.pal_flags); n++)
    g_print ("%2.2x ", info.pal_flags[n]);
  g_print ("\n");

  g_print (" flags:\n");
  g_print ("  restriction: %d\n", info.flags.restriction);
  g_print ("  special info: %s\n", bool_str[info.flags.special_info]);
  g_print ("  user data cc: %s\n", bool_str[info.flags.user_data_cc]);
  g_print ("  start lid#1: %s\n", bool_str[info.flags.use_lid1]);
  g_print ("  start track#3: %s\n", bool_str[info.flags.use_track3]);

  g_print (" psd size: %d\n", GUINT32_FROM_BE (info.psd_size));
  g_print (" first segment addr: %2.2x:%2.2x:%2.2x\n",
           info.first_seg_addr[0],
           info.first_seg_addr[1], info.first_seg_addr[2]);

  g_print (" offset multiplier: 0x%2.2x (must be 0x08)\n", info.offset_mult);

  g_print (" highest psd offset: 0x%4.4x (real offset = %d)\n",
           GUINT16_FROM_BE (info.last_psd_ofs),
           GUINT16_FROM_BE (info.last_psd_ofs) << 3);

  g_print (" item count: %d\n", GUINT16_FROM_BE (info.item_count));

  for (n = 0; n < GUINT16_FROM_BE (info.item_count); n++)
    g_print ("  Item contents[%d]: audio type %d,"
             " videotype %d, continuation %s\n",
             n,
             info.spi_contents[n].audio_type,
             info.spi_contents[n].video_type,
             bool_str[info.spi_contents[n].item_cont]);
}

static void
dump_entries_vcd (gconstpointer data)
{
  EntriesVcd entries;
  gint ntracks, n;

  memcpy (&entries, data, 2048);

  ntracks = GUINT16_FROM_BE (entries.tracks);

  g_print ("VCD/ENTRIES.VCD\n");
  g_print (" ID: `%.8s'\n", entries.ID);
  g_print (" version: 0x%2.2x\n", entries.version);

  g_print (" entries: %d\n", ntracks);

  for (n = 0; n < ntracks; n++)
    {
      const msf_t *msf = &(entries.entry[n].msf);
      uint32_t extent = msf_to_lba(msf);
      extent -= 150;

      g_print (" entry %d starts in track %d at (lba) extent %d "
               "(msf: %2.2x:%2.2x:%2.2x)\n",
               n + 1, from_bcd8 (entries.entry[n].n), extent,
               msf->m, msf->s, msf->f);
    }

}

static void
dump_all (gconstpointer info_p, gconstpointer entries_p,
          gconstpointer lot_p, gconstpointer psd_p)
{
  g_print (DELIM);
  dump_info_vcd (info_p);
  g_print (DELIM);
  dump_entries_vcd (entries_p);
  if (lot_p)
    {
      g_print (DELIM);
      dump_lot_and_psd_vcd (lot_p, psd_p, _get_psd_size (info_p));
    }
  g_print (DELIM);
}

static void
dump_device (const gchar device_fname[])
{
  FILE *fd = fopen (device_fname, "rb");
  gchar info_buf[ISO_BLOCKSIZE] = { 0, };
  gchar entries_buf[ISO_BLOCKSIZE] = { 0, };
  gchar *lot_buf = NULL, *psd_buf = NULL;
  guint32 psd_size;
  guint32 size = -1;

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

  g_print (DELIM);

  g_print ("Source: `%s'\n", device_fname);
  g_print ("Image size: %d sectors\n", size);

  fseek (fd, INFO_VCD_SECTOR * ISO_BLOCKSIZE, SEEK_SET);
  fread (info_buf, ISO_BLOCKSIZE, 1, fd);
  psd_size = _get_psd_size (info_buf);

  fseek (fd, ENTRIES_VCD_SECTOR * ISO_BLOCKSIZE, SEEK_SET);
  fread (entries_buf, ISO_BLOCKSIZE, 1, fd);
  
  if (psd_size)
    {
      gint n;

      lot_buf = g_malloc0 (ISO_BLOCKSIZE * 32);
      psd_buf =
        g_malloc0 (ISO_BLOCKSIZE * (((psd_size - 1) / ISO_BLOCKSIZE) + 1));
      
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

  g_free (lot_buf);
  g_free (psd_buf);

  fclose (fd);
}

static void
read_raw_mode2_sector (FILE * fd, gpointer data, guint32 extent)
{
  gchar buf[CDDA_SIZE] = { 0, };

  g_assert (fd != NULL);

  if (fseek (fd, extent * CDDA_SIZE, SEEK_SET))
    vcd_error ("fseek(): %s", g_strerror (errno));

  fread (buf, CDDA_SIZE, 1, fd);

  if (ferror (fd))
    vcd_error ("fwrite(): %s", g_strerror (errno));

  if (feof (fd))
    vcd_error ("fread(): unexpected EOF");

  memcpy (data, buf + 16, M2RAW_SIZE);
}

static void
dump_file (const gchar image_fname[])
{
  FILE *fd = fopen (image_fname, "rb");
  gchar buf[M2RAW_SIZE] = { 0, };
  gchar info_buf[ISO_BLOCKSIZE] = { 0, };
  gchar entries_buf[ISO_BLOCKSIZE] = { 0, };
  gchar *lot_buf = NULL, *psd_buf = NULL;
  struct stat statbuf;
  guint32 size, psd_size;

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
      g_printerr ("file not multiple of blocksize\n");
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
      gint n;

      lot_buf = g_malloc0 (ISO_BLOCKSIZE * 32);
      psd_buf =
        g_malloc0 (ISO_BLOCKSIZE * (((psd_size - 1) / ISO_BLOCKSIZE) + 1));

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

  g_print (DELIM);

  g_print ("Source: image file `%s'\n", image_fname);
  g_print ("Image size: %d sectors\n", size);

  dump_all (info_buf, entries_buf, lot_buf, psd_buf);

  g_free (lot_buf);
  g_free (psd_buf);

  fclose (fd);
}

static void
rip_file (const gchar device_fname[])
{
  g_printerr ("ripping file image not supported yet\n");
}

static void
rip_device (const gchar device_fname[])
{
  FILE *fd = fopen (device_fname, "rb");
  EntriesVcd entries;
  guint32 size = -1;
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

  g_assert (sizeof (EntriesVcd) == ISO_BLOCKSIZE);

  fseek (fd, ENTRIES_VCD_SECTOR * ISO_BLOCKSIZE, SEEK_SET);
  fread (&entries, ISO_BLOCKSIZE, 1, fd);

  for (i = 0; i < GUINT16_FROM_BE (entries.tracks); i++)
    {
      guint32 startlba = msf_to_lba (&(entries.entry[i].msf)) - 150;

      guint32 endlba = (i + 1 == GUINT16_FROM_BE (entries.tracks))
        ? size : (msf_to_lba (&(entries.entry[i + 1].msf)) - 150);

      guint32 pos = -1;

      FILE *outfd = NULL;

      char fname[80] = { 0, };

      snprintf (fname, sizeof (fname), "track_%2.2d.mpg", i);

      g_print ("%s: %d -> %d\n", fname, startlba, endlba);

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
            guint8 subheader[8];
            guint8 data[2324];
            guint8 spare[4];
          }
          buf;

          struct cdrom_msf *msf = (struct cdrom_msf *) &buf;

          memset (&buf, 0, sizeof (buf));

          msf->cdmsf_min0 = pos / (60 * 75);
          msf->cdmsf_sec0 = ((pos / 75) % 60 + 2);
          msf->cdmsf_frame0 = pos % 75;

          if (!msf->cdmsf_frame0)
            g_print ("debug: %2.2d:%2.2d:%2.2d\n",
                     msf->cdmsf_min0, msf->cdmsf_sec0, msf->cdmsf_frame0);

          if (ioctl (fileno (fd), CDROMREADMODE2, &buf) == -1)
            {
              perror ("ioctl()");
              exit (EXIT_FAILURE);
            }

          if (!buf.subheader[1])
            {
              g_print ("skipping...\n");
              continue;
            }

          fwrite (buf.data, 2324, 1, outfd);
        }

      fclose (outfd);
    }

  fclose (fd);
}


/* global static vars */

static gchar *gl_source_name = NULL;

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
  gint opt;

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
        g_print ("GNU VCDRip " VERSION "\n\n"
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
            g_printerr ("only one source allowed! - try --help\n");
            exit (EXIT_FAILURE);
          }

        gl_source_type = opt;
        break;

      case OP_RIP:
      case OP_DUMP:
        gl_operation |= opt;
        break;
      default:
        g_printerr ("error while parsing command line - try --help\n");
        exit (EXIT_FAILURE);
      }

  if (poptGetArgs (optCon) != NULL)
    {
      g_printerr ("error - no arguments expected!\n");
      exit (EXIT_FAILURE);
    }

  if (gl_operation == OP_RIP && gl_source_type == SOURCE_DIRECTORY)
    {
      g_printerr ("ripping from directory source not supported\n");
      exit (EXIT_FAILURE);
    }

  if (gl_operation == OP_NOOP)
    {
      g_printerr ("no operation requested -- nothing to do\n");
      exit (EXIT_FAILURE);
    }

  if (gl_operation & OP_DUMP)
    switch (gl_source_type)
      {
      case SOURCE_DIRECTORY:
        g_printerr ("source type not implemented yet!\n");
        exit (EXIT_FAILURE);
        break;
      case SOURCE_DEVICE:
        dump_device (gl_source_name);
        break;
      case SOURCE_FILE:
        dump_file (gl_source_name);
        break;
      default:
        g_printerr ("no source given -- can't do anything...\n");
        exit (EXIT_FAILURE);
      }

  if (gl_operation & OP_RIP)
    switch (gl_source_type)
      {
      case SOURCE_DIRECTORY:
        g_printerr ("source type not implemented yet!\n");
        exit (EXIT_FAILURE);
        break;
      case SOURCE_DEVICE:
        rip_device (gl_source_name);
        break;
      case SOURCE_FILE:
        rip_file (gl_source_name);
        break;
      default:
        g_printerr ("no source given -- can't do anything...\n");
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
