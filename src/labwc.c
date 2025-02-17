/*============================================================================
Copyright (c) 2024 Raspberry Pi
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
#include <sys/stat.h>
#include <libxml/xpathInternals.h>

#include "rasputin.h"

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

#define DEFAULT_KB_DELAY 600
#define DEFAULT_KB_INTERVAL 40
#define DEFAULT_MOUSE_SPEED 0.0
#define DEFAULT_MOUSE_DCLICK 400

#define XC(str) ((xmlChar *) str)
#define LN(str) "/*[local-name()='"str"']"

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

static GSettings *mouse_settings;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static void set_xml_value (const char *lvl1, const char *lvl2, const char *name, const char *val);
static void load_config (void);
static void set_doubleclick (void);
static void set_speed (void);
static void set_keyboard (void);
static void set_lefthanded (void);

/*----------------------------------------------------------------------------*/
/* Helper functions */
/*----------------------------------------------------------------------------*/

static void set_xml_value (const char *lvl1, const char *lvl2, const char *name, const char *val)
{
    char *cptr, *user_config_file = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);

    xmlDocPtr xDoc;
    xmlNodePtr cur_node;
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;

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

    // check that the root node exists - create if not
    xpathObj = xmlXPathEvalExpression (XC (LN ("openbox_config")), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
    {
        cur_node = xmlNewNode (NULL, XC ("openbox_config"));
        xmlDocSetRootElement (xDoc, cur_node);
        xmlNewNs (cur_node, XC ("http://openbox.org/3.4/rc"), NULL);
        xmlXPathRegisterNs (xpathCtx, XC ("openbox_config"), XC ("http://openbox.org/3.4/rc"));
    }
    else
        cur_node = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);

    // check that the top level node (keyboard / mouse / libinput) exists - create if not
    cptr = g_strdup_printf ("./" LN ("%s"), lvl1);
    xpathObj = xmlXPathNodeEval (cur_node, XC (cptr), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
        cur_node = xmlNewChild (cur_node, NULL, XC (lvl1), NULL);
    else
        cur_node = xpathObj->nodesetval->nodeTab[0];
    xmlXPathFreeObject (xpathObj);
    g_free (cptr);

    // if a second level is required (libinput device), check it exists - create if not
    if (lvl2)
    {
        cptr = g_strdup_printf ("./" LN ("%s"), lvl2);
        xpathObj = xmlXPathNodeEval (cur_node, XC (cptr), xpathCtx);
        if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
        {
            cur_node = xmlNewChild (cur_node, NULL, XC (lvl2), NULL);
            // libinput device nodes require the category property to be set...
            xmlSetProp (cur_node, XC ("category"), XC ("default"));
        }
        else
            cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlXPathFreeObject (xpathObj);
        g_free (cptr);
    }

    // add or edit the desired element at the current node
    cptr = g_strdup_printf ("./" LN ("%s"), name);
    xpathObj = xmlXPathNodeEval (cur_node, XC (cptr), xpathCtx);
    if (xmlXPathNodeSetIsEmpty (xpathObj->nodesetval))
        cur_node = xmlNewChild (cur_node, NULL, XC (name), XC (val));
    else
    {
        cur_node = xpathObj->nodesetval->nodeTab[0];
        xmlNodeSetContent (cur_node, XC (val));
    }
    xmlXPathFreeObject (xpathObj);
    g_free (cptr);

    // cleanup XML
    xmlXPathFreeContext (xpathCtx);
    xmlSaveFile (user_config_file, xDoc);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();

    g_free (user_config_file);
}

/*----------------------------------------------------------------------------*/
/* Exported API */
/*----------------------------------------------------------------------------*/

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
    if (!dclick) dclick = DEFAULT_MOUSE_DCLICK;

    // labwc default values if nothing set in rc.xml
    interval = DEFAULT_KB_INTERVAL;
    delay = DEFAULT_KB_DELAY;
    speed = DEFAULT_MOUSE_SPEED;
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

    xpathObj = xmlXPathEvalExpression (XC (LN ("openbox_config") LN ("keyboard") LN ("repeatRate")), xpathCtx);
    if (xpathObj)
    {
        if (xpathObj->nodesetval)
        {
            node = xpathObj->nodesetval->nodeTab[0];
            if (node && sscanf ((const char *) xmlNodeGetContent (node), "%d", &val) == 1 && val > 0) interval = 1000 / val;
        }
        xmlXPathFreeObject (xpathObj);
    }

    xpathObj = xmlXPathEvalExpression (XC (LN ("openbox_config") LN ("keyboard") LN ("repeatDelay")), xpathCtx);
    if (xpathObj)
    {
        if (xpathObj->nodesetval)
        {
            node = xpathObj->nodesetval->nodeTab[0];
            if (node && sscanf ((const char *) xmlNodeGetContent (node), "%d", &val) == 1 && val > 0) delay = val;
        }
        xmlXPathFreeObject (xpathObj);
    }

    xpathObj = xmlXPathEvalExpression (XC (LN ("openbox_config") LN ("libinput") LN ("device") LN("pointerSpeed")), xpathCtx);
    if (xpathObj)
    {
        if (xpathObj->nodesetval)
        {
            node = xpathObj->nodesetval->nodeTab[0];
            if (node && sscanf ((const char *) xmlNodeGetContent (node), "%f", &fval) == 1) speed = fval;
        }
        xmlXPathFreeObject (xpathObj);
    }

    xpathObj = xmlXPathEvalExpression (XC (LN ("openbox_config") LN ("libinput") LN ("device") LN("leftHanded")), xpathCtx);
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
    char *str;

    g_settings_set_int (mouse_settings, "double-click", dclick);

    str = g_strdup_printf ("%d", dclick);
    set_xml_value ("mouse", NULL, "doubleClickTime", str);
    g_free (str);

    system ("labwc -r");
}

static void set_speed (void)
{
    char *str;

    str = g_strdup_printf ("%f", speed);
    set_xml_value ("libinput", "device", "pointerSpeed", str);
    g_free (str);

    system ("labwc -r");
}

static void set_keyboard (void)
{
    char *str;

    str = g_strdup_printf ("%d", 1000 / interval);
    set_xml_value ("keyboard", NULL, "repeatRate", str);
    g_free (str);

    str = g_strdup_printf ("%d", delay);
    set_xml_value ("keyboard", NULL, "repeatDelay", str);
    g_free (str);

    system ("labwc -r");
}

static void set_lefthanded (void)
{
    set_xml_value ("libinput", "device", "leftHanded", left_handed ? "yes" : "no");

    system ("labwc -r");
}

/*----------------------------------------------------------------------------*/
/* Function table */
/*----------------------------------------------------------------------------*/

km_functions_t labwc_functions = {
    .load_config = load_config,
    .set_doubleclick = set_doubleclick,
    .set_speed = set_speed,
    .set_keyboard = set_keyboard,
    .set_lefthanded = set_lefthanded,
};

/* End of file */
/*============================================================================*/
