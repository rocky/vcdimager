/*!
   \file vcdinf.h

    Copyright (C) 2002,2003 Rocky Bernstein <rocky@panix.com>

 \verbatim
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Foundation
    Software, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Like vcdinfo but exposes more of the internal structure. It is probably
    better to use vcdinfo, when possible.
 \endverbatim
*/

#ifndef _VCDINF_H
#define _VCDINF_H

#include <libvcd/vcd_iso9660.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

  /*!
    Return a string containing the VCD album id.
  */
  const char * vcdinf_get_album_id(const InfoVcd *info);

  /*!
    Return the VCD application ID.
    NULL is returned if there is some problem in getting this. 
  */
  const char * vcdinf_get_application_id(const pvd_t *pvd);

  const char * vcdinf_get_format_version_str (vcd_type_t vcd_type);

  /*!
    Return number of LIDs. 
  */
  lid_t vcdinf_get_num_LIDs (const InfoVcd *info);

  /*!
    Return a string containing the VCD publisher id with trailing
    blanks removed.
  */
  const char * vcdinf_get_preparer_id(const pvd_t *pvd);

  /*!
    Return a string containing the VCD publisher id with trailing
    blanks removed.
  */
  const char * vcdinf_get_publisher_id(const pvd_t *pvd);
  
  /*!
    Return number of bytes in PSD. 
  */
  uint32_t vcdinf_get_psd_size (const InfoVcd *info);

  /*!
    Return a string containing the VCD system id with trailing
    blanks removed.
  */
  const char * vcdinf_get_system_id(const pvd_t *pvd);
  
  /*!
    Return the VCD volume count - the number of CD's in the collection.
  */
  unsigned int vcdinf_get_volume_count(const InfoVcd *info);
  
  /*!
    Return the VCD ID.
  */
  const char * vcdinf_get_volume_id(const pvd_t *pvd);

  /*!
    Return the VCD volume num - the number of the CD in the collection.
    This is a number between 1 and the volume count.
  */
  unsigned int vcdinf_get_volume_num(const InfoVcd *info);
  
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_VCDINF_H*/
