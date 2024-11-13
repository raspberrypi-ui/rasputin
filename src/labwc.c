#include <locale.h>
#include <gtk/gtk.h>
#include <sys/stat.h>
#include <libxml/xpathInternals.h>

#include "rasputin.h"

#define XC(str) ((xmlChar *) str)

static GSettings *mouse_settings;
static char fstr[16];
static char *update_facc_str (void)
{
    char *oldloc = setlocale (LC_NUMERIC, NULL);
    setlocale (LC_NUMERIC, "POSIX");
    sprintf (fstr, "%f", accel);
    setlocale (LC_NUMERIC, oldloc);
    return fstr;
}

static void set_xml_value (const char *lvl1, const char *lvl2, const char *l2attr, const char *l2atval, const char *name, const char *val)
{
    char *cptr, *attr, *user_config_file = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);

    xmlDocPtr xDoc;
    xmlNodePtr root, cur_node, node;
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;

    if (l2attr) attr = g_strdup_printf ("[@%s=\"%s\"]", l2attr, l2atval);

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    if (g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        xDoc = xmlParseFile (user_config_file);
        if (!xDoc) xDoc = xmlNewDoc (XC ("1.0"));
    }
    else xDoc = xmlNewDoc (XC ("1.0"));
    xpathCtx = xmlXPathNewContext (xDoc);

    // check that the nodes exist in the document - create them if not
    xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']"), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        root = xmlNewNode (NULL, XC ("openbox_config"));
        xmlDocSetRootElement (xDoc, root);
        xmlNewNs (root, XC ("http://openbox.org/3.4/rc"), NULL);
        xmlXPathRegisterNs (xpathCtx, XC ("openbox_config"), XC ("http://openbox.org/3.4/rc"));
    }
    else root = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);

    cptr = g_strdup_printf ("/*[local-name()='openbox_config']/*[local-name()='%s']", lvl1);
    xpathObj = xmlXPathEvalExpression (XC (cptr), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval)) cur_node = xmlNewChild (root, NULL, XC (lvl1), NULL);
    else cur_node = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);
    g_free (cptr);

    if (lvl2)
    {
        cptr = g_strdup_printf ("/*[local-name()='openbox_config']/*[local-name()='%s']/*[local-name()='%s']%s", lvl1, lvl2, l2attr ? attr : "");
        xpathObj = xmlXPathEvalExpression (XC (cptr), xpathCtx);
        if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
        {
            node = xmlNewChild (cur_node, NULL, XC (lvl2), NULL);
            if (l2attr) xmlSetProp (node, XC (l2attr), XC (l2atval));
        }
        xmlXPathFreeObject (xpathObj);
        g_free (cptr);
        cptr = g_strdup_printf ("/*[local-name()='openbox_config']/*[local-name()='%s']/*[local-name()='%s']%s/*[local-name()='%s']", lvl1, lvl2, l2attr ? attr : "", name);
    }
    else cptr = g_strdup_printf ("/*[local-name()='openbox_config']/*[local-name()='%s']/*[local-name()='%s']", lvl1, name);

    xpathObj = xmlXPathEvalExpression (XC (cptr), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        xmlXPathFreeObject (xpathObj);
        g_free (cptr);
        if (lvl2) cptr = g_strdup_printf ("/*[local-name()='openbox_config']/*[local-name()='%s']/*[local-name()='%s']%s", lvl1, lvl2, l2attr ? attr : "");
        else cptr = g_strdup_printf ("/*[local-name()='openbox_config']/*[local-name()='%s']", lvl1);
        xpathObj = xmlXPathEvalExpression (XC (cptr), xpathCtx);
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNewChild (cur_node, NULL, XC (name), XC (val));
    }
    else
    {
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNodeSetContent (cur_node, XC (val));
    }
    g_free (cptr);

    // cleanup XML
    xmlXPathFreeObject (xpathObj);
    xmlXPathFreeContext (xpathCtx);
    xmlSaveFile (user_config_file, xDoc);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    if (l2attr) g_free (attr);
    g_free (user_config_file);
}



static void load_config (void)
{
    char *user_config_file = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);;
    char *dir = g_path_get_dirname (user_config_file);
    int val;
    float fval;
    xmlXPathObjectPtr xpathObj;
    xmlNode *node;

    mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
    dclick = g_settings_get_int (mouse_settings, "double-click");

    // labwc default values if nothing set in rc.xml
    interval = 40;
    delay = 600;
    accel = 0.0;
    left_handed = FALSE;

    // create the directory if needed
    g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (dir);

    if (!g_file_test (user_config_file, G_FILE_TEST_IS_REGULAR))
    {
        g_free (user_config_file);
        return;
    }

    // read in data from XML file
    xmlInitParser ();
    LIBXML_TEST_VERSION
    xmlDocPtr xDoc = xmlParseFile (user_config_file);
    if (xDoc == NULL)
    {
        g_free (user_config_file);
        return;
    }

    xmlXPathContextPtr xpathCtx = xmlXPathNewContext (xDoc);

    xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='keyboard']/*[local-name()='repeatRate']"), xpathCtx);
    if (xpathObj)
    {
        if (xpathObj->nodesetval)
        {
            node = xpathObj->nodesetval->nodeTab[0];
            if (node && sscanf ((const char *) xmlNodeGetContent (node), "%d", &val) == 1 && val > 0) interval = 1000 / val;
        }
        xmlXPathFreeObject (xpathObj);
    }

    xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='keyboard']/*[local-name()='repeatDelay']"), xpathCtx);
    if (xpathObj)
    {
        if (xpathObj->nodesetval)
        {
            node = xpathObj->nodesetval->nodeTab[0];
            if (node && sscanf ((const char *) xmlNodeGetContent (node), "%d", &val) == 1 && val > 0) delay = val;
        }
        xmlXPathFreeObject (xpathObj);
    }

    xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='libinput']/*[local-name()='device'][@category=\"default\"]/*[local-name()='pointerSpeed']"), xpathCtx);
    if (xpathObj)
    {
        if (xpathObj->nodesetval)
        {
            node = xpathObj->nodesetval->nodeTab[0];
            if (node && sscanf ((const char *) xmlNodeGetContent (node), "%f", &fval) == 1) accel = fval;
        }
        xmlXPathFreeObject (xpathObj);
    }

    xpathObj = xmlXPathEvalExpression (XC ("/*[local-name()='openbox_config']/*[local-name()='libinput']/*[local-name()='device'][@category=\"default\"]/*[local-name()='leftHanded']"), xpathCtx);
    if (xpathObj)
    {
        if (xpathObj->nodesetval)
        {
            node = xpathObj->nodesetval->nodeTab[0];
            if (node && xmlNodeGetContent (node) && !strcmp ((const char *) xmlNodeGetContent (node), "yes")) left_handed = TRUE;
        }
        xmlXPathFreeObject (xpathObj);
    }

    xmlXPathFreeContext (xpathCtx);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    g_free (user_config_file);
}

static void set_doubleclick (void)
{
    g_settings_set_int (mouse_settings, "double-click", dclick);
    char *str = g_strdup_printf ("%d", dclick);
    set_xml_value ("mouse", NULL, NULL, NULL, "doubleClickTime", str);
    g_free (str);
    system ("labwc -r");
}

static void set_acceleration (void)
{
    update_facc_str ();
    set_xml_value ("libinput", "device", "category", "default", "pointerSpeed", fstr);
    system ("labwc -r");
}

static void set_keyboard (void)
{
    char *str;

    str = g_strdup_printf ("%d", 1000 / interval);
    set_xml_value ("keyboard", NULL, NULL, NULL, "repeatRate", str);
    g_free (str);

    str = g_strdup_printf ("%d", delay);
    set_xml_value ("keyboard", NULL, NULL, NULL, "repeatDelay", str);
    g_free (str);

    system ("labwc -r");
}

static void set_lefthanded (void)
{
    set_xml_value ("libinput", "device", "category", "default", "leftHanded", left_handed ? "yes" : "no");
    system ("labwc -r");
}

static void save_config (void)
{
}



/*----------------------------------------------------------------------------*/
/* Function table */
/*----------------------------------------------------------------------------*/

km_functions_t labwc_functions = {
    .load_config = load_config,
    .set_doubleclick = set_doubleclick,
    .set_acceleration = set_acceleration,
    .set_keyboard = set_keyboard,
    .set_lefthanded = set_lefthanded,
    .save_config = save_config,
};
