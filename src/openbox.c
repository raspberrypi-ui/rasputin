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
#include <gdk/gdkx.h>
#include <X11/XKBlib.h>
#include <glib/gi18n.h>

#include "rasputin.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

#define DEFAULT_SES "LXDE-pi"
#define DEFAULT_PTR_MAP_SIZE 128

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

static GList *devs = NULL;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static void load_config (void);
static void set_doubleclick (void);
static void set_acceleration (void);
static void set_keyboard (void);
static void set_lefthanded (void);

/*----------------------------------------------------------------------------*/
/* Helper functions */
/*----------------------------------------------------------------------------*/

/* Client message code copied from GTK+2 */

typedef struct _GdkEventClient GdkEventClient;

struct _GdkEventClient
{
    GdkEventType type;
    GdkWindow *window;
    gint8 send_event;
    GdkAtom message_type;
    gushort data_format;
    union {
        char b[20];
        short s[10];
        long l[5];
    } data;
};

gint _gdk_send_xevent (GdkDisplay *display, Window window, gboolean propagate, glong event_mask, XEvent *event_send)
{
    gboolean result;

    if (gdk_display_is_closed (display)) return FALSE;

    gdk_x11_display_error_trap_push (display);
    result = XSendEvent (GDK_DISPLAY_XDISPLAY (display), window, propagate, event_mask, event_send);
    XSync (GDK_DISPLAY_XDISPLAY (display), False);
    if (gdk_x11_display_error_trap_pop (display)) return FALSE;

    return result;
}

/* Sends a ClientMessage to all toplevel client windows */
static gboolean gdk_event_send_client_message_to_all_recurse (GdkDisplay *display, XEvent *xev, guint32 xid, guint level)
{
    Atom type = None;
    int format;
    unsigned long nitems, after;
    unsigned char *data;
    Window *ret_children, ret_root, ret_parent;
    unsigned int ret_nchildren;
    gboolean send = FALSE;
    gboolean found = FALSE;
    gboolean result = FALSE;
    int i;

    gdk_x11_display_error_trap_push (display);

    if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), xid, 
        gdk_x11_get_xatom_by_name_for_display (display, "WM_STATE"),
        0, 0, False, AnyPropertyType, &type, &format, &nitems, &after, &data) != Success)
            goto out;

    if (type)
    {
        send = TRUE;
        XFree (data);
    }
    else
    {
        /* OK, we're all set, now let's find some windows to send this to */
        if (!XQueryTree (GDK_DISPLAY_XDISPLAY (display), xid, &ret_root, &ret_parent, &ret_children, &ret_nchildren))
            goto out;

        for (i = 0; i < ret_nchildren; i++)
            if (gdk_event_send_client_message_to_all_recurse (display, xev, ret_children[i], level + 1))
                found = TRUE;

        XFree (ret_children);
    }

    if (send || (!found && (level == 1)))
    {
        xev->xclient.window = xid;
        _gdk_send_xevent (display, xid, False, NoEventMask, xev);
    }

    result = send || found;

    out:
        gdk_x11_display_error_trap_pop (display);

    return result;
}

void gdk_screen_broadcast_client_message (GdkScreen *screen, GdkEventClient *event)
{
    XEvent sev;
    GdkWindow *root_window;

    g_return_if_fail (event != NULL);

    root_window = gdk_screen_get_root_window (screen);

    /* Set up our event to send, with the exception of its target window */
    sev.xclient.type = ClientMessage;
    sev.xclient.display = GDK_WINDOW_XDISPLAY (root_window);
    sev.xclient.format = event->data_format;
    memcpy(&sev.xclient.data, &event->data, sizeof (sev.xclient.data));
    sev.xclient.message_type = gdk_x11_atom_to_xatom_for_display (gdk_screen_get_display (screen), event->message_type);

    gdk_event_send_client_message_to_all_recurse (gdk_screen_get_display (screen), &sev, GDK_WINDOW_XID (root_window), 0);
}

void gdk_event_send_clientmessage_toall (GdkEvent *event)
{
    g_return_if_fail (event != NULL);
    gdk_screen_broadcast_client_message (gdk_screen_get_default (), (GdkEventClient *) event);
}

static void reload_all_programs (void)
{
    GdkEventClient event;
    event.type = GDK_CLIENT_EVENT;
    event.send_event = TRUE;
    event.window = NULL;
    event.message_type = gdk_atom_intern("_GTK_READ_RCFILES", FALSE);
    event.data_format = 8;
    gdk_event_send_clientmessage_toall ((GdkEvent *) &event);
}

void read_acceleration (void)
{
    FILE *fp_dev, *fp_acc;
    char *cmd, dev[16], acc[32];
    char *oldloc = setlocale (LC_NUMERIC, NULL);
    float fval;

    setlocale (LC_NUMERIC, "POSIX");
    accel = 0.0;

    // query xinput for list of slave pointer devices - returned as ids, one per line
    fp_dev = popen ("xinput list | grep pointer | grep slave | cut -f 2 | cut -d = -f 2", "r");
    if (fp_dev)
    {
        // loop through devices
        while (fgets (dev, sizeof (dev) - 1, fp_dev))
        {
            g_strstrip (dev);

            // query xinput for acceleration value for each device
            cmd = g_strdup_printf ("xinput list-props %s | grep \"Accel Speed\" | head -n 1 | cut -f 3", dev);
            fp_acc = popen (cmd, "r");
            if (fp_acc)
            {
                if (fgets (acc, sizeof (acc) - 1, fp_acc))
                {
                    if (sscanf (acc, "%f", &fval) == 1)
                    {
                        accel = fval;
                        devs = g_list_append (devs, g_strdup (dev));
                    }
                }
                pclose (fp_acc);
            }
            g_free (cmd);
        }
        pclose (fp_dev);
    }

    setlocale (LC_NUMERIC, oldloc);
}

int read_key_file_int (GKeyFile *user, GKeyFile *sys, const char *section, const char *item, int fallback)
{
    GError *err;
    int val;

    err = NULL;
    val = g_key_file_get_integer (user, section, item, &err);
    if (!err && val > 0) return val;

    err = NULL;
    val = g_key_file_get_integer (sys, section, item, &err);
    if (!err && val > 0) return val;

    return fallback;
}

static void load_settings (void)
{
    GKeyFile *kfu, *kfs;
    char *config_file;

    const char *session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;

    kfu = g_key_file_new ();
    config_file = g_build_filename (g_get_user_config_dir(), "lxsession", session_name, "desktop.conf", NULL);
    g_key_file_load_from_file (kfu, config_file, G_KEY_FILE_NONE, NULL);
    g_free (config_file);

    kfs = g_key_file_new ();
    config_file = g_build_filename ("/etc", "xdg", "lxsession", session_name, "desktop.conf", NULL);
    g_key_file_load_from_file (kfs, config_file, G_KEY_FILE_NONE, NULL);
    g_free (config_file);

    delay = read_key_file_int (kfu, kfs, "Keyboard", "Delay", 400);
    interval = read_key_file_int (kfu, kfs, "Keyboard", "Interval", 250);
    dclick = read_key_file_int (kfu, kfs, "GTK", "iNet/DoubleClickTime", 250);
    left_handed = read_key_file_int (kfu, kfs, "Mouse", "LeftHanded", 0);

    g_key_file_free (kfu);
    g_key_file_free (kfs);
}

static void write_lxsession (const char *section, const char *param, int value)
{
    char *config_file, *sysconf_file, *str;
    GKeyFile *kf;
    gsize len;

    const char *session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;

    // try to open the user config file
    kf = g_key_file_new ();
    config_file = g_build_filename (g_get_user_config_dir(), "lxsession", session_name, "desktop.conf", NULL);
    if (!g_key_file_load_from_file (kf, config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // no user config - create the local config directory
        str = g_path_get_dirname (config_file);
        g_mkdir_with_parents (str, 0700);
        g_free (str);

        // load the global config
        sysconf_file = g_build_filename ("/etc", "xdg", "lxsession", session_name, "desktop.conf", NULL);
        g_key_file_load_from_file (kf, sysconf_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
        g_free (sysconf_file);
    }

    // update value in the key file
    g_key_file_set_integer (kf, section, param, value);

    // write the modified key file out
    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (config_file, str, len, NULL);

    g_free (config_file);
    g_free (str);
    g_key_file_free (kf);
}

/*----------------------------------------------------------------------------*/
/* Exported API */
/*----------------------------------------------------------------------------*/

static void load_config (void)
{
    read_acceleration ();
    load_settings ();
}

static void set_doubleclick (void)
{
    write_lxsession ("GTK", "iNet/DoubleClickTime", dclick);
    reload_all_programs ();
}

static void set_acceleration (void)
{
    char *cmd, *config_file, *dir, *str;
    char *oldloc = setlocale (LC_NUMERIC, NULL);
    GList *dev;

    setlocale (LC_NUMERIC, "POSIX");

    for (dev = devs; dev != NULL; dev = dev->next)
    {
        cmd = g_strdup_printf ("xinput set-prop %s \"libinput Accel Speed\" %f", (char *) dev->data, accel);
        system (cmd);
        g_free (cmd);
    }

    // clean up old autostart
    config_file = g_build_filename (g_get_user_config_dir(), "autostart", "LXinput-setup.desktop", NULL);
    remove (config_file);
    g_free (config_file);

    // save pointer acceleration into autostart
    config_file = g_build_filename (g_get_user_config_dir(), "autostart", "rasputin-mouse-accel.desktop", NULL);
    dir = g_path_get_dirname (config_file);
    g_mkdir_with_parents (dir, 0755);
    g_free (dir);

    str = g_strdup_printf ("[Desktop Entry]\nType=Application\nName=rasputin-mouse-accel\nComment=Set mouse acceleration\nNoDisplay=true\nNotShowIn=GNOME;KDE;XFCE;\n"
        "Exec=sh -c 'for id in $(xinput list | grep pointer | grep slave | cut -f 2 | cut -d = -f 2) ; do xinput set-prop $id \"libinput Accel Speed\" %f ; done'\n", accel);
    g_file_set_contents (config_file, str, -1, NULL);
    g_free (str);

    g_free (config_file);

    setlocale (LC_NUMERIC, oldloc);
}

static void set_keyboard (void)
{
    write_lxsession ("Keyboard", "Delay", delay);
    write_lxsession ("Keyboard", "Interval", interval);
    reload_all_programs ();
}

static void set_lefthanded (void)
{
    write_lxsession ("Mouse", "LeftHanded", left_handed);
    reload_all_programs ();
}

/*----------------------------------------------------------------------------*/
/* Function table */
/*----------------------------------------------------------------------------*/

km_functions_t openbox_functions = {
    .load_config = load_config,
    .set_doubleclick = set_doubleclick,
    .set_acceleration = set_acceleration,
    .set_keyboard = set_keyboard,
    .set_lefthanded = set_lefthanded,
};

/* End of file */
/*============================================================================*/
