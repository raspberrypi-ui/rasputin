/*============================================================================
Copyright (c) 2024 Raspberry Pi Holdings Ltd.
Some code based on lxinput from the LXDE project :
Copyright (c) 2009-2014 PCMan, martyj19, Julien Lavergne, Andri Grytsenko
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <locale.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "rasputin.h"

extern km_functions_t labwc_functions;
extern km_functions_t openbox_functions;
extern km_functions_t wayfire_functions;

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

#define TIMEOUT_MS 500

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

GtkWidget *dlg, *mouse_speed, *mouse_dclick, *mouse_left_handed,
    *kb_delay, *kb_interval, *kb_layout, *dclick_btn, *dclick_ind;
int dclick, old_dclick, delay, old_delay, interval, old_interval;
float speed, old_speed;
gboolean left_handed, old_left_handed;

guint dctimer = 0, matimer = 0, kbtimer = 0;

km_functions_t km_fn;

GtkGesture *gesture;
GdkPixbuf *black, *white;
gboolean ind_state;

GtkBuilder *builder;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static gboolean dclick_handler (gpointer data);
static gboolean speed_handler (gpointer data);
static gboolean kbd_handler (gpointer data);
static gboolean on_mouse_dclick_changed (GtkRange *range, GdkEventButton *event, gpointer user_data);
static gboolean on_mouse_speed_changed (GtkRange *range, GdkEventButton *event, gpointer user_data);
static gboolean on_kb_range_changed (GtkRange *range, GdkEventButton *event, int *val);
static gboolean on_left_handed_toggle (GtkSwitch *btn, gboolean state, gpointer user_data);
static void on_set_keyboard_ext (GtkButton *btn, gpointer ptr);
static gboolean reset_indicator (gpointer ptr);
static void on_gpress (GtkGestureMultiPress *self, gint n_press, gdouble x, gdouble y, gpointer ptr);

/*----------------------------------------------------------------------------*/
/* Timer handlers */
/*----------------------------------------------------------------------------*/

static gboolean dclick_handler (gpointer data)
{
    km_fn.set_doubleclick ();
    dctimer = 0;
    return FALSE;
}

static gboolean speed_handler (gpointer data)
{
    km_fn.set_speed ();
    matimer = 0;
    return FALSE;
}

static gboolean kbd_handler (gpointer data)
{
    km_fn.set_keyboard ();
    kbtimer = 0;
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Widget handlers */
/*----------------------------------------------------------------------------*/

static gboolean on_mouse_dclick_changed (GtkRange *range, GdkEventButton *event, gpointer user_data)
{
    if (dctimer) g_source_remove (dctimer);
    dclick = gtk_range_get_value (range);
    dctimer = g_timeout_add (TIMEOUT_MS, dclick_handler, NULL);
    return FALSE;
}

static gboolean on_mouse_speed_changed (GtkRange *range, GdkEventButton *event, gpointer user_data)
{
    if (matimer) g_source_remove (matimer);
    speed = (gtk_range_get_value (range) / 5.0) - 1.0;
    matimer = g_timeout_add (TIMEOUT_MS, speed_handler, NULL);
    return FALSE;
}

static gboolean on_kb_range_changed (GtkRange *range, GdkEventButton *event, int *val)
{
    if (kbtimer) g_source_remove (kbtimer);
    *val = (int) gtk_range_get_value (range);
    kbtimer = g_timeout_add (TIMEOUT_MS, kbd_handler, NULL);
    return FALSE;
}

static gboolean on_left_handed_toggle (GtkSwitch *btn, gboolean state, gpointer user_data)
{
    left_handed = state;
    km_fn.set_lefthanded ();
    return FALSE;
}

static void on_set_keyboard_ext (GtkButton *btn, gpointer ptr)
{
    g_spawn_command_line_async ("rc_gui -k", NULL);
}

static gboolean reset_indicator (gpointer ptr)
{
    gtk_image_set_from_pixbuf (GTK_IMAGE (dclick_ind), black);
    return FALSE;
}

static void on_gpress (GtkGestureMultiPress *self, gint n_press, gdouble x, gdouble y, gpointer ptr)
{
    if (n_press == 2)
    {
        g_object_unref (gesture);
        gesture = gtk_gesture_multi_press_new (dclick_btn);
        g_signal_connect (gesture, "pressed", G_CALLBACK (on_gpress), NULL);
        gtk_image_set_from_pixbuf (GTK_IMAGE (dclick_ind), white);
        g_timeout_add (250, G_SOURCE_FUNC (reset_indicator), NULL);
    }
}

/*----------------------------------------------------------------------------*/
/* Main function */
/*----------------------------------------------------------------------------*/

void init_plugin (void)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE")) km_fn = wayfire_functions;
        else km_fn = labwc_functions;
    }
    else km_fn = openbox_functions;

    /* load the current state */
    km_fn.load_config ();

    /* backup the existing state */
    old_left_handed = left_handed;
    old_speed = speed;
    old_dclick = dclick;
    old_delay = delay;
    old_interval = interval;

    /* create the dialog */
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/rasputin.ui");

    dlg = (GtkWidget *) gtk_builder_get_object (builder, "dlg");

    mouse_speed = (GtkWidget *) gtk_builder_get_object (builder, "mouse_speed");
    gtk_range_set_value (GTK_RANGE (mouse_speed), (speed + 1) * 5.0);
    g_signal_connect (mouse_speed, "button-release-event", G_CALLBACK (on_mouse_speed_changed), NULL);

    mouse_dclick = (GtkWidget *) gtk_builder_get_object (builder, "mouse_dclick");
    gtk_range_set_value (GTK_RANGE (mouse_dclick), dclick);
    g_signal_connect (mouse_dclick, "button-release-event", G_CALLBACK (on_mouse_dclick_changed), NULL);

    mouse_left_handed = (GtkWidget *) gtk_builder_get_object (builder, "left_handed");
    gtk_switch_set_active (GTK_SWITCH (mouse_left_handed), left_handed);
    g_signal_connect (mouse_left_handed, "state-set", G_CALLBACK (on_left_handed_toggle), NULL);

    kb_delay = (GtkWidget *) gtk_builder_get_object (builder, "kb_delay");
    gtk_range_set_value (GTK_RANGE (kb_delay), delay);
    g_signal_connect (kb_delay, "button-release-event", G_CALLBACK (on_kb_range_changed), &delay);

    kb_interval = (GtkWidget *) gtk_builder_get_object (builder, "kb_interval");
    gtk_range_set_value (GTK_RANGE (kb_interval), interval);
    g_signal_connect (kb_interval, "button-release-event", G_CALLBACK (on_kb_range_changed), &interval);

    kb_layout = (GtkWidget *) gtk_builder_get_object (builder, "keyboard_layout");
    g_signal_connect (kb_layout, "clicked", G_CALLBACK (on_set_keyboard_ext), NULL);

    dclick_btn = (GtkWidget *) gtk_builder_get_object (builder, "dclick");
    gesture = gtk_gesture_multi_press_new (dclick_btn);
    g_signal_connect (gesture, "pressed", G_CALLBACK (on_gpress), NULL);
    dclick_ind = (GtkWidget *) gtk_builder_get_object (builder, "dclick_ind");

    black = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 32, 32);
    gdk_pixbuf_fill (black, 0x707070ff);
    white = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 32, 32);
    gdk_pixbuf_fill (white, 0xffffffff);
    gtk_image_set_from_pixbuf (GTK_IMAGE (dclick_ind), black);
}

int plugin_tabs (void)
{
    return 2;
}

const char *tab_name (int tab)
{
    if (tab == 0) return "Mouse";
    else return "Keyboard";
}

GtkWidget *get_tab (int tab)
{
    GtkWidget *window, *plugin;

    window = (GtkWidget *) gtk_builder_get_object (builder, tab ? "kbd_wd" : "mouse_wd");
    plugin = (GtkWidget *) gtk_builder_get_object (builder, tab ? "kbd_page" : "mouse_page");

    gtk_container_remove (GTK_CONTAINER (window), plugin);

    return plugin;
}

void free_plugin (void)
{
    if (dctimer) g_source_remove (dctimer);
    if (matimer) g_source_remove (matimer);
    if (kbtimer) g_source_remove (kbtimer);
    g_object_unref (builder);
}

/* End of file */
/*============================================================================*/
