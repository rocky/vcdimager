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

#include "GVCDImager.hh"
#include "GVCDImagerApp.hh"

#include <libgnome/gnome-url.h>
#include <libgnome/libgnome.h>

// main controller

#define GLADE_ID "MainAppWindow"

#define GPACKAGE "GVCDImager"

GVCDImagerApp* GVCDImagerApp::app = 0;

GVCDImagerApp::GVCDImagerApp(int argc, const char *argv[])
  : _main(GPACKAGE, VERSION, argc, const_cast<gchar **>(argv)),
    _glade_filename("GVCDImager.glade")
{
  assert(GVCDImagerApp::app == 0);

  gnome_app_id = GPACKAGE; // fixme ...
  gnome_app_version = VERSION; // fixme bug workaround...

  GVCDImagerApp::app = this;

  Glade::glade_gnome_init();

  GladeXML *__mwx = _main_win_xml = GVCDImagerApp::app->loadFromGlade(GLADE_ID);

  _main_win = Glade::getWidgetPtr<Gnome::App>(__mwx, GLADE_ID);
  _main_win->delete_event.connect(slot(this, &GVCDImagerApp::onWindowDelete));

  // toolbar
  _track_add_button = Glade::getWidgetPtr<Gtk::Button>(__mwx, "track_add_button");
  _track_add_button->clicked.connect(slot(this, &GVCDImagerApp::onTrackAdd));

  _track_del_button = Glade::getWidgetPtr<Gtk::Button>(__mwx, "track_del_button");
  _track_del_button->clicked.connect(slot(this, &GVCDImagerApp::onTrackDel));

  _image_write_button = Glade::getWidgetPtr<Gtk::Button>(__mwx, "image_write_button");
  _image_write_button->clicked.connect(slot(this, &GVCDImagerApp::onImageWrite));

  _abort_button = Glade::getWidgetPtr<Gtk::Button>(__mwx, "abort_button");
  _abort_button->clicked.connect(slot(this, &GVCDImagerApp::onAbort));

  // menubar
  _image_write_menuitem = Glade::getWidgetPtr<Gtk::MenuItem>(__mwx, "image_write_menuitem");
  _image_write_menuitem->activate.connect(slot(this, &GVCDImagerApp::onImageWrite));

  _exit_menuitem = Glade::getWidgetPtr<Gtk::MenuItem>(__mwx, "exit_menuitem");
  _exit_menuitem->activate.connect(slot(this, &GVCDImagerApp::onExit));

  _track_add_menuitem = Glade::getWidgetPtr<Gtk::MenuItem>(__mwx, "track_add_menuitem");
  _track_add_menuitem->activate.connect(slot(this, &GVCDImagerApp::onTrackAdd));

  _track_del_menuitem = Glade::getWidgetPtr<Gtk::MenuItem>(__mwx, "track_del_menuitem");
  _track_del_menuitem->activate.connect(slot(this, &GVCDImagerApp::onTrackDel));

  _settings_menuitem = Glade::getWidgetPtr<Gtk::MenuItem>(__mwx, "settings_menuitem");
  _settings_menuitem->activate.connect(slot(this, &GVCDImagerApp::onSettings));

  _help_homepage_menuitem = Glade::getWidgetPtr<Gtk::MenuItem>(__mwx, "help_homepage_menuitem");
  _help_homepage_menuitem->activate.connect(slot(this, &GVCDImagerApp::onHelpHomepage));

  _help_about_menuitem = Glade::getWidgetPtr<Gtk::MenuItem>(__mwx, "help_about_menuitem");
  _help_about_menuitem->activate.connect(slot(this, &GVCDImagerApp::onHelpAbout));

  // ...
  _tracks_clist = Glade::getWidgetPtr<Gtk::CList>(__mwx, "tracks_clist");
  _tracks_clist->select_row.connect(slot(this, &GVCDImagerApp::onSelectRow));
  _tracks_clist->unselect_row.connect(slot(this, &GVCDImagerApp::onUnselectRow));
  
  _config_viewport = Glade::getWidgetPtr<Gtk::Viewport>(__mwx, "config_viewport");

  // !!!

  _file_selection = Glade::getWidgetPtr<Gtk::FileSelection>(loadFromGlade("AddTrackDialog"), "AddTrackDialog");
  _file_selection->get_ok_button()->clicked.connect(slot(this, &GVCDImagerApp::onFileAdd));
  _file_selection->get_cancel_button()->clicked.connect(slot(this, &GVCDImagerApp::onFileCancel));

  _file_selection->hide_fileop_buttons();

  updateSensitivity();
}

void 
GVCDImagerApp::updateSensitivity(void)
{
  bool tracks_avail = _tracks_clist->get_rows() > 0;
  bool track_selected = getTrackHighlighted() >= 0;
  bool writing = _writing;
  
  /* this one is only active if writing image */
  _abort_button->set_sensitive(writing);

  /* stuff active when not writing image */
  _tracks_clist->set_sensitive(!writing);
  _config_viewport->set_sensitive(!writing);

  _track_add_menuitem->set_sensitive(!writing);
  _track_add_button->set_sensitive(!writing);
  
  _file_selection->set_sensitive(!writing);

  /* this one is only active if not writing and there are tracks */
  _image_write_menuitem->set_sensitive(tracks_avail && !writing);
  _image_write_button->set_sensitive(tracks_avail && !writing);

  //_track_info_button
  _track_del_menuitem->set_sensitive(tracks_avail && track_selected && !writing);
  _track_del_button->set_sensitive(tracks_avail && track_selected && !writing);
}

GVCDImagerApp::~GVCDImagerApp()
{
  cerr << "~GVCDImagerApp() called..." << endl;

  //delete _main_win;
  _main_win->destroy();
  gtk_object_unref(GTK_OBJECT(_main_win_xml));
}

gint GVCDImagerApp::onWindowDelete(GdkEventAny* e)
{
  cerr << "delete..." << endl;
  quit();
  return 1; // fixme
}

void 
GVCDImagerApp::onExit(void)
{
  cerr << "exit..." << endl;
  quit();
}

void 
GVCDImagerApp::onSelectRow(gint p0,gint p1,GdkEvent* p2)
{
  //assert(_track_highlighted == -1);
  //_track_highlighted = p0;

  updateSensitivity();

  cerr << "+p0: " << p0 << " p1: " << p1 << endl;
}

void 
GVCDImagerApp::onUnselectRow(gint p0,gint p1,GdkEvent* p2)
{
  //assert(_track_highlighted == p0);
  //_track_highlighted = -1;
  
  updateSensitivity();

  cerr << "-p0: " << p0 << " p1: " << p1 << endl;
}

void 
GVCDImagerApp::run(void)
{
  _main.run();
}

void
GVCDImagerApp::quit(void)
{
  _main.quit();
}

void 
GVCDImagerApp::onFileCancel(void)
{
  _file_selection->hide();
}

void 
GVCDImagerApp::onFileAdd(void)
{
  string fname = _file_selection->get_filename();
  
  cerr << fname << endl;

  const char *text[6] = { 0, };

  //text[0] = "a";
  text[1] = "b";
  text[2] = "c";
  text[3] = "d";
  text[4] = fname.c_str();

  _tracks_clist->rows().push_back(text);

  renumberList();

  updateSensitivity();

}

void 
GVCDImagerApp::onTrackAdd(void)
{
  _file_selection->show();
}

void 
GVCDImagerApp::onAbort(void)
{
  cerr << "abort!!!" << endl;

  updateSensitivity();
}

void 
GVCDImagerApp::onSettings(void)
{
  cerr << "settings" << endl;
}

void 
GVCDImagerApp::onHelpHomepage(void)
{
  gnome_url_show("http://www.hvrlab.org/~hvr/vcdimager/");
}

void 
GVCDImagerApp::onHelpAbout(void)
{
  Glade::GladeXML *_tmp_xml = loadFromGlade("AboutDialog");

  Gnome::About* about =
    Glade::getWidgetPtr<Gnome::About>(_tmp_xml, "AboutDialog");

  about->run();
}

using Gtk::CList_Helpers::RowList;

void 
GVCDImagerApp::onImageWrite(void)
{
  _writing = true;
  updateSensitivity();

  int save_track_highlighted = getTrackHighlighted();

  for(RowList::iterator p = _tracks_clist->rows().begin();
      p != _tracks_clist->rows().end(); p++) {

    p->select();
    (*p)[0].moveto();

    while(_main.events_pending())
      _main.iteration();

    sleep(1);
    
  }

  sleep(5);

  _writing = false;

  cerr << "image write" << endl;

  _tracks_clist->selection().clear();
  if(save_track_highlighted >= 0)
    _tracks_clist->row(save_track_highlighted).select();

  updateSensitivity();
}


int
GVCDImagerApp::getTrackHighlighted(void) const
{
  if(!_tracks_clist->selection().size()) 
    return -1;

  return _tracks_clist->selection().begin()->get_row_num();
}

void 
GVCDImagerApp::renumberList(void)
{
  for(RowList::iterator p = _tracks_clist->rows().begin();
      p != _tracks_clist->rows().end(); p++) {
    char buf[16] = { 0, };
    snprintf(buf, sizeof(buf), "%2.2d", p->get_row_num()+2);
    (*p)[0].set_text(buf);
  }
}

void 
GVCDImagerApp::onTrackDel(void)
{
  assert(getTrackHighlighted() >= 0);

  RowList::iterator p = _tracks_clist->rows().begin();

  for(int n = 0;n < getTrackHighlighted();n++, p++);

  _tracks_clist->rows().erase(p);
  renumberList();

  updateSensitivity();
}

GladeXML* 
GVCDImagerApp::loadFromGlade(const std::string &name)
{
  return Glade::glade_xml_new(_glade_filename.c_str(), name.c_str());
}
