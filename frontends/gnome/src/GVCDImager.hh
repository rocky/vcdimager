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

#ifndef __GVCDIMAGER_HH__
#define __GVCDIMAGER_HH__

#include <cassert>
using std::assert;

#include <string>

// Gnome--

#include <gnome--.h>
#include <gtk/gtk.h>
#include <gtk--/main.h>

// libsigc++

#include <sigc++/object.h>
#include <sigc++/slot.h>
#include <sigc++/bind.h>

//using SigC::slot;
using SigC::bind;
using SigC::slot;

#include "GladeHelper.hh"

#endif // __GVCDIMAGER_HH__
