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

#ifndef __GVCDIMAGERAPP_HH__
#define __GVCDIMAGERAPP_HH__

#include "GVCDImager.hh"

using Glade::GladeXML;

class GVCDImagerApp
  : public SigC::Object
{
public:
  static GVCDImagerApp *app;

  GVCDImagerApp(int argc, const char *argv[]);
  ~GVCDImagerApp();
  
  GladeXML* loadFromGlade(const std::string &name);

  void run(void);
  void quit(void);

private:
  void updateSensitivity(void);
  int getTrackHighlighted(void) const;
  void renumberList(void);
  
  // events
  gint onWindowDelete(GdkEventAny* e);
  void onSelectRow(gint p0,gint p1,GdkEvent* p2);
  void onUnselectRow(gint p0,gint p1,GdkEvent* p2);

  // actions
  void onAbort(void);
  void onExit(void);
  void onHelpAbout(void);
  void onHelpHomepage(void);
  void onImageWrite(void);
  void onSettings(void);
  void onTrackAdd(void);
  void onTrackDel(void);
  void onFileAdd(void);
  void onFileCancel(void);

  void onVcdType(libvcd::vcd_type_t new_type);

private:
  Gnome::Main _main;
  std::string _glade_filename;

  libvcd::vcd_type_t _current_vcd_type;

  // model...
  //VcdObj *_vcdobj;
  bool _writing;

  GladeXML* _main_win_xml;
  // UI elements
  Gnome::App *_main_win;

  Gtk::Button *_track_add_button;
  Gtk::Button *_track_del_button;
  Gtk::Button *_image_write_button;
  Gtk::Button *_abort_button;

  Gtk::MenuItem *_image_write_menuitem;
  Gtk::MenuItem *_exit_menuitem;
  Gtk::MenuItem *_track_add_menuitem;
  Gtk::MenuItem *_track_del_menuitem;
  Gtk::Menu *_vcd_type_menu;
  Gtk::MenuItem *_settings_menuitem;
  Gtk::MenuItem *_help_about_menuitem;
  Gtk::MenuItem *_help_homepage_menuitem;

  Gtk::Viewport *_config_viewport;
  Gtk::CList *_tracks_clist;

  // FileSelection...
  Gtk::FileSelection *_file_selection;
};

#endif // __GVCDIMAGERAPP_HH__
