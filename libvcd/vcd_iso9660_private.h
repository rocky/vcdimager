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

#ifndef __VCD_ISO9660_PRIVATE_H__
#define __VCD_ISO9660_PRIVATE_H__

#define ISO_VD_PRIMARY          1
#define ISO_VD_END              255

#define ISO_STANDARD_ID         "CD001"
#define ISO_VERSION             1

#define ISO_XA_MARKER_STRING    "CD-XA001"
#define ISO_XA_MARKER_OFFSET    1024

#ifdef __MWERKS__
#pragma options align=packed
#endif

struct iso_volume_descriptor {
  uint8_t  type                        GNUC_PACKED; /* 711 */
  char     id[5]                       GNUC_PACKED;
  uint8_t  version                     GNUC_PACKED; /* 711 */
  char     data[2041]                  GNUC_PACKED;
};

struct iso_primary_descriptor {
  uint8_t  type                        GNUC_PACKED; /* 711 */
  char     id[5]                       GNUC_PACKED;
  uint8_t  version                     GNUC_PACKED; /* 711 */
  char     unused1[1]                  GNUC_PACKED;
  char     system_id[32]               GNUC_PACKED; /* achars */
  char     volume_id[32]               GNUC_PACKED; /* dchars */
  char     unused2[8]                  GNUC_PACKED;
  uint64_t volume_space_size           GNUC_PACKED; /* 733 */
  char     escape_sequences[32]        GNUC_PACKED;
  uint32_t volume_set_size             GNUC_PACKED; /* 723 */
  uint32_t volume_sequence_number      GNUC_PACKED; /* 723 */
  uint32_t logical_block_size          GNUC_PACKED; /* 723 */
  uint64_t path_table_size             GNUC_PACKED; /* 733 */
  uint32_t type_l_path_table           GNUC_PACKED; /* 731 */
  uint32_t opt_type_l_path_table       GNUC_PACKED; /* 731 */
  uint32_t type_m_path_table           GNUC_PACKED; /* 732 */
  uint32_t opt_type_m_path_table       GNUC_PACKED; /* 732 */
  char     root_directory_record[34]   GNUC_PACKED; /* 9.1 */
  char     volume_set_id[128]          GNUC_PACKED; /* dchars */
  char     publisher_id[128]           GNUC_PACKED; /* achars */
  char     preparer_id[128]            GNUC_PACKED; /* achars */
  char     application_id[128]         GNUC_PACKED; /* achars */
  char     copyright_file_id[37]       GNUC_PACKED; /* 7.5 dchars */
  char     abstract_file_id[37]        GNUC_PACKED; /* 7.5 dchars */
  char     bibliographic_file_id[37]   GNUC_PACKED; /* 7.5 dchars */
  char     creation_date[17]           GNUC_PACKED; /* 8.4.26.1 */
  char     modification_date[17]       GNUC_PACKED; /* 8.4.26.1 */
  char     expiration_date[17]         GNUC_PACKED; /* 8.4.26.1 */
  char     effective_date[17]          GNUC_PACKED; /* 8.4.26.1 */
  uint8_t  file_structure_version      GNUC_PACKED; /* 711 */
  char     unused4[1]                  GNUC_PACKED;
  char     application_data[512]       GNUC_PACKED;
  char     unused5[653]                GNUC_PACKED;
};

struct iso_path_table {
  uint8_t  name_len                    GNUC_PACKED; /* 711 */
  uint8_t  xa_len                      GNUC_PACKED; /* 711 */
  uint32_t extent                      GNUC_PACKED; /* 731/732 */
  uint16_t parent                      GNUC_PACKED; /* 721/722 */
  char     name[EMPTY_ARRAY_SIZE]      GNUC_PACKED;
};

struct iso_directory_record {
  uint8_t  length                      GNUC_PACKED; /* 711 */
  uint8_t  ext_attr_length             GNUC_PACKED; /* 711 */
  uint64_t extent                      GNUC_PACKED; /* 733 */
  uint64_t size                        GNUC_PACKED; /* 733 */
  uint8_t  date[7]                     GNUC_PACKED; /* 7 by 711 */
  uint8_t  flags                       GNUC_PACKED;
  uint8_t  file_unit_size              GNUC_PACKED; /* 711 */
  uint8_t  interleave                  GNUC_PACKED; /* 711 */
  uint32_t volume_sequence_number      GNUC_PACKED; /* 723 */
  uint8_t  name_len                    GNUC_PACKED; /* 711 */
  char     name[EMPTY_ARRAY_SIZE]      GNUC_PACKED;
};

#ifdef __MWERKS__
#pragma options align=reset
#endif

#endif /* __VCD_ISO9660_PRIVATE_H__ */


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
