#include "rasputin.h"


static void load_config (void)
{
    GError *err;
    char *user_config_file;
    GKeyFile *kfu, *kfs;

    /* open user and system config files */
    user_config_file = g_build_filename (g_get_user_config_dir (), "wayfire.ini", NULL);
    kfu = g_key_file_new ();
    g_key_file_load_from_file (kfu, user_config_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    kfs = g_key_file_new ();
    g_key_file_load_from_file (kfs, "/etc/wayfire/defaults.ini", G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

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
    facc = g_key_file_get_double (kfu, "input", "mouse_cursor_speed", &err);
    if (err)
    {
        err = NULL;
        facc = g_key_file_get_double (kfs, "input", "mouse_cursor_speed", &err);
        if (err) facc = 0;
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

    mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
    dclick = g_settings_get_int (mouse_settings, "double-click");
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

    g_key_file_set_double (kf, "input", "mouse_cursor_speed", facc);

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
