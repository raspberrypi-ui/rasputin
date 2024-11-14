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

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

static GList *devs = NULL;
static GSettings *mouse_settings, *keyboard_settings;
static char fstr[16];

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static void load_config (void);
static void set_doubleclick (void);
static void set_acceleration (void);
static void set_keyboard (void);
static void set_lefthanded (void);
static void save_config (void);

/*----------------------------------------------------------------------------*/
/* Helper functions */
/*----------------------------------------------------------------------------*/

static char *update_facc_str (void)
{
    char *oldloc = setlocale (LC_NUMERIC, NULL);
    setlocale (LC_NUMERIC, "POSIX");
    sprintf (fstr, "%f", accel);
    setlocale (LC_NUMERIC, oldloc);
    return fstr;
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

    const char *session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;

    char *rel_path = g_strconcat ("lxsession/", session_name, "/desktop.conf", NULL);
    char *user_config_file = g_build_filename (g_get_user_config_dir(), rel_path, NULL);

    kfu = g_key_file_new ();
    g_key_file_load_from_file (kfu, user_config_file, G_KEY_FILE_NONE, NULL);

    kfs = g_key_file_new ();
    g_key_file_load_from_dirs (kfs, rel_path, (const char **) g_get_system_config_dirs (), NULL, G_KEY_FILE_NONE, NULL);

    g_free (rel_path);
    g_free (user_config_file);

    delay = read_key_file_int (kfu, kfs, "Keyboard", "Delay", 400);
    interval = read_key_file_int (kfu, kfs, "Keyboard", "Interval", 250);
    dclick = read_key_file_int (kfu, kfs, "GTK", "iNet/DoubleClickTime", 250);
    left_handed = read_key_file_int (kfu, kfs, "Mouse", "LeftHanded", 0);

    g_key_file_free (kfu);
    g_key_file_free (kfs);
}

#if GTK_CHECK_VERSION(3, 0, 0)

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

#endif


static void reload_all_programs (void)
{
    GdkEventClient event;
    event.type = GDK_CLIENT_EVENT;
    event.send_event = TRUE;
    event.window = NULL;
    event.message_type = gdk_atom_intern("_GTK_READ_RCFILES", FALSE);
    event.data_format = 8;
    gdk_event_send_clientmessage_toall((GdkEvent *)&event);
}



/*----------------------------------------------------------------------------*/
/* Exported API */
/*----------------------------------------------------------------------------*/

static void load_config (void)
{
    read_acceleration ();
    load_settings ();

    mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
    keyboard_settings = g_settings_new ("org.gnome.desktop.peripherals.keyboard");
}

static void set_doubleclick (void)
{
    const char *session_name;
    char *user_config_file, *str, *scf;
    GKeyFile *kf;
    gsize len;
    
    // construct the file path
    session_name = g_getenv ("DESKTOP_SESSION");
    if (!session_name) session_name = DEFAULT_SES;
    user_config_file = g_build_filename (g_get_user_config_dir (), "lxsession/", session_name, "/desktop.conf", NULL);

    // read in data from file to a key file
    kf = g_key_file_new ();
    if (!g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        // create the local config directory
        scf = g_path_get_dirname (user_config_file);
        g_mkdir_with_parents (scf, 0700);
        g_free (scf);
        // load the global config
        scf = g_build_filename ("/etc/xdg/lxsession/", session_name, "/desktop.conf", NULL);
        g_key_file_load_from_file (kf, scf, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
        g_free (scf);
    }

    // update changed values in the key file
    g_key_file_set_integer (kf, "GTK", "iNet/DoubleClickTime", dclick);

    // write the modified key file out
    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);

    g_free (user_config_file);
    g_free (str);

    reload_all_programs ();
}

static void set_acceleration (void)
{
    char buf[256];
    update_facc_str ();

    GList *l;
    for (l = devs; l != NULL; l = l->next)
    {
        sprintf (buf, "xinput set-prop %s \"libinput Accel Speed\" %s", (char *) l->data, fstr);
        system (buf);
    }

    g_settings_set_double (mouse_settings, "speed", accel);
}

static void set_keyboard (void)
{
    /* apply keyboard values */
    XkbSetAutoRepeatRate(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), XkbUseCoreKbd, delay, interval);
    g_settings_set_uint (keyboard_settings, "repeat-interval", interval);
    g_settings_set_uint (keyboard_settings, "delay", delay);
}

static void set_lefthanded (void)
{
/* This function is taken from Gnome's control-center 2.6.0.3 (gnome-settings-mouse.c) and was modified*/
#define DEFAULT_PTR_MAP_SIZE 128
    unsigned char *buttons;
    gint n_buttons, i;
    gint idx_1 = 0, idx_3 = 1;

    buttons = g_alloca (DEFAULT_PTR_MAP_SIZE);
    n_buttons = XGetPointerMapping (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), buttons, DEFAULT_PTR_MAP_SIZE);
    if (n_buttons > DEFAULT_PTR_MAP_SIZE)
    {
        buttons = g_alloca (n_buttons);
        n_buttons = XGetPointerMapping (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), buttons, n_buttons);
    }

    for (i = 0; i < n_buttons; i++)
    {
        if (buttons[i] == 1)
            idx_1 = i;
        else if (buttons[i] == ((n_buttons < 3) ? 2 : 3))
            idx_3 = i;
    }

    if ((left_handed && idx_1 < idx_3) ||
        (!left_handed && idx_1 > idx_3))
    {
        buttons[idx_1] = ((n_buttons < 3) ? 2 : 3);
        buttons[idx_3] = 1;
        XSetPointerMapping (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), buttons, n_buttons);
    }
}

static void save_config (void)
{
    char* str = NULL, *user_config_file;
    GKeyFile* kf = g_key_file_new();
    gsize len;
    const char* session_name = g_getenv("DESKTOP_SESSION");
    if(!session_name) session_name = "LXDE";

    char *rel_path = g_strconcat("lxsession/", session_name, "/desktop.conf", NULL);
    user_config_file = g_build_filename(g_get_user_config_dir(), rel_path, NULL);

    if(!g_key_file_load_from_file(kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        /* the user config file doesn't exist, create its parent dir */
        len = strlen(user_config_file) - strlen("/desktop.conf");
        user_config_file[len] = '\0';
        g_debug("user_config_file = %s", user_config_file);
        g_mkdir_with_parents(user_config_file, 0700);
        user_config_file[len] = '/';

        g_key_file_load_from_dirs(kf, rel_path, (const char**)g_get_system_config_dirs(), NULL,
                                  G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
    }

    g_free(rel_path);

    g_key_file_set_integer(kf, "Mouse", "LeftHanded", !!left_handed);

    g_key_file_set_integer(kf, "Keyboard", "Delay", delay);
    g_key_file_set_integer(kf, "Keyboard", "Interval", interval);

    str = g_key_file_to_data(kf, &len, NULL);
    g_file_set_contents(user_config_file, str, len, NULL);
    g_free(str);

    /* ask the settigns daemon to reload */
    /* FIXME: is this needed? */
    /* g_spawn_command_line_sync("lxde-settings-daemon reload", NULL, NULL, NULL, NULL); */

    /* also save settings into autostart file for non-lxsession sessions */
    g_free(user_config_file);
    update_facc_str ();
    rel_path = g_build_filename(g_get_user_config_dir(), "autostart", NULL);
    user_config_file = g_build_filename(rel_path, "LXinput-setup.desktop", NULL);
    if (g_mkdir_with_parents(rel_path, 0755) == 0)
    {
        str = g_strdup_printf("[Desktop Entry]\n"
                              "Type=Application\n"
                              "Name=%s\n"
                              "Comment=%s\n"
                              "NoDisplay=true\n"
                              "Exec=sh -c 'xset r rate %d %d %s; for id in $(xinput list | grep pointer | grep slave | cut -f 2 | cut -d = -f 2 ) ; do xinput set-prop $id \"libinput Accel Speed\" %s 2> /dev/null ; done'\n"
                              "NotShowIn=GNOME;KDE;XFCE;\n",
                              _("LXInput autostart"),
                              _("Setup keyboard and mouse using settings done in LXInput"),
                              /* FIXME: how to setup left-handed mouse? */
                              delay, 1000 / interval,
                              left_handed ? ";xmodmap -e \"pointer = 3 2 1\"" : "",
                              fstr);
        g_file_set_contents(user_config_file, str, -1, NULL);
        g_free(str);
    }
    g_free(user_config_file);
    g_key_file_free( kf );
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
    .save_config = save_config,
};

/* End of file */
/*============================================================================*/
