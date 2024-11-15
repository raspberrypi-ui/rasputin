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
/* Global data */
/*----------------------------------------------------------------------------*/

static GSettings *mouse_settings;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static void load_config (void);
static void set_doubleclick (void);
static void set_acceleration (void);
static void set_keyboard (void);
static void set_lefthanded (void);

/*----------------------------------------------------------------------------*/
/* Exported API */
/*----------------------------------------------------------------------------*/

static void load_config (void)
{
    GError *err;
    char *user_config_file;
    GKeyFile *kfu, *kfs;

    mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");

    dclick = g_settings_get_int (mouse_settings, "double-click");

    /* open user and system config files */
    user_config_file = g_build_filename (g_get_user_config_dir (), "wayfire.ini", NULL);
    kfu = g_key_file_new ();
    g_key_file_load_from_file (kfu, user_config_file, G_KEY_FILE_NONE, NULL);

    kfs = g_key_file_new ();
    g_key_file_load_from_file (kfs, "/etc/wayfire/defaults.ini", G_KEY_FILE_NONE, NULL);

    err = NULL;
    delay = g_key_file_get_integer (kfu, "input", "kb_repeat_delay", &err);
    if (err)
    {
        err = NULL;
        delay = g_key_file_get_integer (kfs, "input", "kb_repeat_delay", &err);
        if (err) delay = 400;
    }

    err = NULL;
    interval = g_key_file_get_integer (kfu, "input", "kb_repeat_rate", &err);
    if (err)
    {
        err = NULL;
        interval = g_key_file_get_integer (kfs, "input", "kb_repeat_rate", &err);
        if (err) interval = 40;
    }
    interval = 1000 / interval;

    err = NULL;
    accel = g_key_file_get_double (kfu, "input", "mouse_cursor_speed", &err);
    if (err)
    {
        err = NULL;
        accel = g_key_file_get_double (kfs, "input", "mouse_cursor_speed", &err);
        if (err) accel = 0.0;
    }

    err = NULL;
    left_handed = g_key_file_get_boolean (kfu, "input", "left_handed_mode", &err);
    if (err)
    {
        err = NULL;
        left_handed = g_key_file_get_boolean (kfs, "input", "left_handed_mode", &err);
        if (err) left_handed = FALSE;
    }

    g_key_file_free (kfu);
    g_key_file_free (kfs);
    g_free (user_config_file);
}

static void set_doubleclick (void)
{
    g_settings_set_int (mouse_settings, "double-click", dclick);
}

static void set_acceleration (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = g_build_filename (g_get_user_config_dir (), "wayfire.ini", NULL);

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_double (kf, "input", "mouse_cursor_speed", accel);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void set_keyboard (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = g_build_filename (g_get_user_config_dir (), "wayfire.ini", NULL);

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_integer (kf, "input", "kb_repeat_delay", delay);
    g_key_file_set_integer (kf, "input", "kb_repeat_rate", 1000 / interval);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);
}

static void set_lefthanded (void)
{
    char *user_config_file, *str;
    GKeyFile *kf;
    gsize len;

    user_config_file = g_build_filename (g_get_user_config_dir (), "wayfire.ini", NULL);

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_boolean (kf, "input", "left_handed_mode", left_handed);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (user_config_file, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (user_config_file);
}

/*----------------------------------------------------------------------------*/
/* Function table */
/*----------------------------------------------------------------------------*/

km_functions_t wayfire_functions = {
    .load_config = load_config,
    .set_doubleclick = set_doubleclick,
    .set_acceleration = set_acceleration,
    .set_keyboard = set_keyboard,
    .set_lefthanded = set_lefthanded,
};

/* End of file */
/*============================================================================*/
