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

#include <gtk/gtk.h>

#include "rasputin.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

#define DEFAULT_KB_DELAY 400
#define DEFAULT_KB_INTERVAL 40
#define DEFAULT_MOUSE_SPEED 0.0
#define DEFAULT_MOUSE_DCLICK 400

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

static GSettings *mouse_settings;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static int read_key_file_int (GKeyFile *user, GKeyFile *sys, const char *section, const char *item, int fallback);
static float read_key_file_float (GKeyFile *user, GKeyFile *sys, const char *section, const char *item, float fallback);
static void write_key_file (const char *section, const char *item, int value, float fval);
static void load_config (void);
static void set_doubleclick (void);
static void set_speed (void);
static void set_keyboard (void);
static void set_lefthanded (void);

/*----------------------------------------------------------------------------*/
/* Helper functions */
/*----------------------------------------------------------------------------*/

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

static float read_key_file_float (GKeyFile *user, GKeyFile *sys, const char *section, const char *item, float fallback)
{
    GError *err;
    float val;

    err = NULL;
    val = g_key_file_get_double (user, section, item, &err);
    if (!err) return val;

    err = NULL;
    val = g_key_file_get_double (sys, section, item, &err);
    if (!err) return val;

    return fallback;
}

static void write_key_file (const char *section, const char *item, int value, float fval)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = g_build_filename (g_get_user_config_dir (), "wayfire.ini", NULL);

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    if (value == -1) g_key_file_set_double (kf, section, item, fval);
    else g_key_file_set_integer (kf, section, item, value);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);
}

/*----------------------------------------------------------------------------*/
/* Exported API */
/*----------------------------------------------------------------------------*/

static void load_config (void)
{
    char *user_config_file;
    GKeyFile *kfu, *kfs;

    mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
    dclick = g_settings_get_int (mouse_settings, "double-click");
    if (!dclick) dclick = DEFAULT_MOUSE_DCLICK;

    user_config_file = g_build_filename (g_get_user_config_dir (), "wayfire.ini", NULL);
    kfu = g_key_file_new ();
    g_key_file_load_from_file (kfu, user_config_file, G_KEY_FILE_NONE, NULL);
    g_free (user_config_file);

    kfs = g_key_file_new ();
    g_key_file_load_from_file (kfs, "/etc/wayfire/defaults.ini", G_KEY_FILE_NONE, NULL);

    left_handed = read_key_file_int (kfu, kfs, "input", "left_handed_mode", 0);
    delay = read_key_file_int (kfu, kfs, "input", "kb_repeat_delay", DEFAULT_KB_DELAY);
    interval = 1000 / read_key_file_int (kfu, kfs, "input", "kb_repeat_rate", DEFAULT_KB_INTERVAL);
    speed = read_key_file_float (kfu, kfs, "input", "mouse_cursor_speed", DEFAULT_MOUSE_SPEED);

    g_key_file_free (kfu);
    g_key_file_free (kfs);
}

static void set_doubleclick (void)
{
    g_settings_set_int (mouse_settings, "double-click", dclick);
}

static void set_speed (void)
{
    write_key_file ("input", "mouse_cursor_speed", -1, speed);
}

static void set_keyboard (void)
{
    write_key_file ("input", "kb_repeat_delay", delay, 0.0);
    write_key_file ("input", "kb_repeat_rate", 1000 / interval, 0.0);
}

static void set_lefthanded (void)
{
    write_key_file ("input", "left_handed_mode", left_handed, 0.0);
}

/*----------------------------------------------------------------------------*/
/* Function table */
/*----------------------------------------------------------------------------*/

km_functions_t wayfire_functions = {
    .load_config = load_config,
    .set_doubleclick = set_doubleclick,
    .set_speed = set_speed,
    .set_keyboard = set_keyboard,
    .set_lefthanded = set_lefthanded,
};

/* End of file */
/*============================================================================*/
