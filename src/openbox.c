/*============================================================================
Copyright (c) 2024 Raspberry Pi Holdings Ltd.
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

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

#define DEFAULT_SES "LXDE-pi"

#define DEFAULT_KB_DELAY 400
#define DEFAULT_KB_INTERVAL 250
#define DEFAULT_MOUSE_SPEED 0.0
#define DEFAULT_MOUSE_DCLICK 250

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

static GList *devs = NULL;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static void read_acceleration (void);
static int read_key_file_int (GKeyFile *user, GKeyFile *sys, const char *section, const char *item, int fallback);
static void read_lxsession (void);
static void write_lxsession (const char *section, const char *param, int value);
static void load_config (void);
static void set_doubleclick (void);
static void set_acceleration (void);
static void set_keyboard (void);
static void set_lefthanded (void);

/*----------------------------------------------------------------------------*/
/* Helper functions */
/*----------------------------------------------------------------------------*/

static void read_acceleration (void)
{
    FILE *fp_dev, *fp_acc;
    char *cmd, dev[16], acc[32];
    char *oldloc = setlocale (LC_NUMERIC, NULL);
    float fval;

    setlocale (LC_NUMERIC, "POSIX");
    accel = DEFAULT_MOUSE_SPEED;

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

static int read_key_file_int (GKeyFile *user, GKeyFile *sys, const char *section, const char *item, int fallback)
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

static void read_lxsession (void)
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

    delay = read_key_file_int (kfu, kfs, "Keyboard", "Delay", DEFAULT_KB_DELAY);
    interval = read_key_file_int (kfu, kfs, "Keyboard", "Interval", DEFAULT_KB_INTERVAL);
    dclick = read_key_file_int (kfu, kfs, "GTK", "iNet/DoubleClickTime", DEFAULT_MOUSE_DCLICK);
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
    read_lxsession ();
}

static void set_doubleclick (void)
{
    write_lxsession ("GTK", "iNet/DoubleClickTime", dclick);
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

    str = g_strdup_printf ("[Desktop Entry]\nType=Application\nName=rasputin-mouse-accel\nComment=Set mouse acceleration\nNoDisplay=true\n"
        "Exec=sh -c 'if pgrep openbox ; then for id in $(xinput list | grep pointer | grep slave | cut -f 2 | cut -d = -f 2) ; do xinput set-prop $id \"libinput Accel Speed\" %f ; done ; fi'\n", accel);
    g_file_set_contents (config_file, str, -1, NULL);
    g_free (str);

    g_free (config_file);

    setlocale (LC_NUMERIC, oldloc);
}

static void set_keyboard (void)
{
    write_lxsession ("Keyboard", "Delay", delay);
    write_lxsession ("Keyboard", "Interval", interval);
}

static void set_lefthanded (void)
{
    write_lxsession ("Mouse", "LeftHanded", left_handed);
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
