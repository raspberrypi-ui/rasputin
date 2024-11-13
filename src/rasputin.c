/*
 *      lxinput.c
 *
 *      Copyright 2009-2011 PCMan <pcman.tw@gmail.com>
 *      Copyright 2009 martyj19 <martyj19@comcast.net>
 *      Copyright 2011-2013 Julien Lavergne <julien.lavergne@gmail.com>
 *      Copyright 2014 Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <wordexp.h>
#include <locale.h>

#include "rasputin.h"


static GtkWidget *dlg;
static GtkRange *mouse_accel;
static GtkRange *mouse_dclick;
static GtkSwitch *mouse_left_handed;
static GtkRange *kb_delay;
static GtkRange *kb_interval;
static GtkButton* kb_layout;

int accel = 20, old_accel = 20;
int dclick = 250, old_dclick = 250;
gboolean left_handed = FALSE, old_left_handed = FALSE;
float facc = 0.0, old_facc = 0.0;
char fstr[16];

int threshold = 10, old_threshold = 10;


int delay = 500, old_delay = 500;
int interval = 30, old_interval = 30;
static guint dctimer = 0, matimer = 0, kbtimer = 0;

GSettings *mouse_settings, *keyboard_settings;


extern km_functions_t labwc_functions;
extern km_functions_t openbox_functions;
extern km_functions_t wayfire_functions;

km_functions_t km_fn;

/* Window manager in use */

typedef enum {
    WM_OPENBOX,
    WM_WAYFIRE,
    WM_LABWC } wm_type;
static wm_type wm;


char *update_facc_str (void)
{
    char *oldloc = setlocale (LC_NUMERIC, NULL);
    setlocale (LC_NUMERIC, "POSIX");
    sprintf (fstr, "%f", facc);
    setlocale (LC_NUMERIC, oldloc);
    return fstr;
}


static gboolean dclick_handler (gpointer data)
{
    km_fn.set_doubleclick ();
    dctimer = 0;
    return FALSE;
}

static gboolean on_mouse_dclick_changed (GtkRange* range, GdkEventButton *event, gpointer user_data)
{
    if (dctimer) g_source_remove (dctimer);
    dclick = gtk_range_get_value (range);
    dctimer = g_timeout_add (500, dclick_handler, NULL);
    return FALSE;
}

static gboolean accel_handler (gpointer data)
{
    km_fn.set_acceleration ();
    matimer = 0;
    return FALSE;
}

static gboolean on_mouse_accel_changed (GtkRange* range, GdkEventButton *event, gpointer user_data)
{
    if (matimer) g_source_remove (matimer);
    facc = (gtk_range_get_value (range) / 5.0) - 1.0;
    matimer = g_timeout_add (500, accel_handler, NULL);
    return FALSE;
}

static gboolean kbd_handler (gpointer data)
{
    km_fn.set_keyboard ();
    kbtimer = 0;
    return FALSE;
}

static gboolean on_kb_range_changed (GtkRange* range, GdkEventButton *event, int *val)
{
    if (kbtimer) g_source_remove (kbtimer);
    *val = (int) gtk_range_get_value (range);
    kbtimer = g_timeout_add (500, kbd_handler, NULL);
    return FALSE;
}

static void on_left_handed_toggle(GtkSwitch* btn, gboolean state, gpointer user_data)
{
    left_handed = state;
    km_fn.set_lefthanded ();
}

static void on_set_keyboard_ext (GtkButton* btn, gpointer ptr)
{
    g_spawn_command_line_async ("rc_gui -k", NULL);
}


int main(int argc, char** argv)
{
    GtkBuilder* builder;

    // check window manager
    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE"))
        {
            wm = WM_WAYFIRE;
            km_fn = wayfire_functions;
        }
        else
        {
            wm = WM_LABWC;
            km_fn = labwc_functions;
        }
    }
    else 
    {
        wm = WM_OPENBOX;
        km_fn = openbox_functions;
    }


    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );

    gtk_init(&argc, &argv);

    gtk_icon_theme_prepend_search_path(gtk_icon_theme_get_default(), PACKAGE_DATA_DIR);

    /* build the UI */
    builder = gtk_builder_new_from_file( PACKAGE_DATA_DIR "/ui/rasputin.ui" );
    dlg = (GtkWidget*)gtk_builder_get_object( builder, "dlg" );

    mouse_accel = (GtkRange*)gtk_builder_get_object(builder,"mouse_accel");
    mouse_left_handed = (GtkSwitch*)gtk_builder_get_object(builder,"left_handed");
    mouse_dclick = (GtkRange*)gtk_builder_get_object(builder, "mouse_dclick");

    kb_delay = (GtkRange*)gtk_builder_get_object(builder,"kb_delay");
    kb_interval = (GtkRange*)gtk_builder_get_object(builder,"kb_interval");
    kb_layout = (GtkButton*)gtk_builder_get_object(builder,"keyboard_layout");

    g_object_unref( builder );

    /* read the config file */
    km_fn.load_config ();
    
    old_accel = accel;
    old_threshold = threshold;
    old_left_handed = left_handed;
    old_facc = facc;
    old_dclick = dclick;
    
    

    /* init the UI */
    gtk_range_set_value(mouse_accel, (facc + 1) * 5.0);
    gtk_range_set_value(mouse_dclick, dclick);
    gtk_switch_set_active(mouse_left_handed, left_handed);

    gtk_range_set_value(kb_delay, delay);
    gtk_range_set_value(kb_interval, interval);

    g_signal_connect(mouse_accel, "button-release-event", G_CALLBACK(on_mouse_accel_changed), NULL);
    g_signal_connect(mouse_dclick, "button-release-event", G_CALLBACK(on_mouse_dclick_changed), NULL);
    g_signal_connect(mouse_left_handed, "state-set", G_CALLBACK(on_left_handed_toggle), NULL);

    g_signal_connect(kb_delay, "button-release-event", G_CALLBACK(on_kb_range_changed), &delay);
    g_signal_connect(kb_interval, "button-release-event", G_CALLBACK(on_kb_range_changed), &interval);
    g_signal_connect(kb_layout, "clicked", G_CALLBACK(on_set_keyboard_ext), NULL);

    if( gtk_dialog_run( (GtkDialog*)dlg ) == GTK_RESPONSE_OK )
    {
        km_fn.save_config ();
    }
    else
    {
        /* restore to original settings */

        /* keyboard */
        delay = old_delay;
        interval = old_interval;

        /* mouse */
        accel = old_accel;
        threshold = old_threshold;
        left_handed = old_left_handed;
        facc = old_facc;
        dclick = old_dclick;

        km_fn.set_acceleration ();
        km_fn.set_doubleclick ();
        km_fn.set_keyboard ();
        km_fn.set_lefthanded ();
    }

    gtk_widget_destroy( dlg );


    return 0;
}
