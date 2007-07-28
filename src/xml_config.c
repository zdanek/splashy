/************************************************************************
 *                              xml_config.c                            *
 *                                                                      *
 *  2005-10-01 11:21 EDT                                                *
 *  Copyright  2005  Luis Mondesi <lemsx1@gmail.com>                    *
 ************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.
 */

/*
 * A very basic implementation of an XML configuration parser using xml_parser
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "common_macros.h"
#include "xml_parser.h"
#include "splashycnf.h"

static xml_parser_t *splashy_xml_config;      /** global: default config file db */

const char *
splashy_get_config_string(const char *xpath)
{
	return xml_parser_get_text(xpath);
}
int
splashy_get_config_int(const char *xpath, int base)
{
	return xml_parser_get_int(xpath, base);
}
int
splashy_change_config_file (const char* filename)
{
	 xml_parser_set_xml_file (filename);
	 return xml_parser_read (splashy_xml_config);

}

/**
 * @desc gets current theme full path
 */
const gchar *
splashy_get_theme_dir ()
{
        GString *theme_path = g_string_new ("");
        const gchar *ret = NULL;
        /*
         * the current theme could be given to us in a full path or a 
         * simple string name. We determine how to get the theme
         * config file from this path 
         */
        const gchar *_current_theme = xml_parser_get_text (SPL_CURRENT_THEME);
        /*
         * is this a relative path ? 
         */
        if (g_ascii_strncasecmp (_current_theme, G_DIR_SEPARATOR_S, 1) != 0)
        {
                /*
                 * build path 
                 */
                g_string_append (theme_path,
                                 xml_parser_get_text (SPL_THEMES_DIR));
                g_string_append (theme_path, G_DIR_SEPARATOR_S);
                g_string_append (theme_path, _current_theme);
                /*
                 * FIXME g_free(_current_theme); 
                 */
        }
        else
        {
                /*
                 * use full path 
                 */
                g_string_append (theme_path, _current_theme);
	}
	g_string_append (theme_path, G_DIR_SEPARATOR_S);

        ret = theme_path->str;
        /*
         * do not leak GString struct. free struct but not data
         * pointer/allocated mem 
         */
        g_string_free (theme_path, FALSE);
        return ret;
}

/**
 * @desc a helper function to construct a filename path from a given xpath
 * It reads config.xml for a given theme and builds the path accordingly
 * @param theme_xpath use this theme
 * @param image_xpath path representing string to retrieve from xml file
 * @return full path to image
 */
/* Was static inline, FXIME */
inline const gchar *
splashy_image_path (const gchar * image_xpath)
{
        DEBUG_PRINT ("splashy_image_path(%s) called",
                     image_xpath);
        GString *_ret_path = g_string_new (xml_parser_get_text (image_xpath));

        /*
         * is this a relative path? Else assume full
         */
        if (g_ascii_strncasecmp (_ret_path->str, G_DIR_SEPARATOR_S, 1) != 0)
		g_string_prepend (_ret_path, splashy_get_theme_dir());

        return _ret_path->str;
}


/**
 * @ingroup core
 * Initializes a our XML db using _filename as main config file.
 * It will determine our theme config file from this file.
 * @param _filename filename path to read
 * @return true on success
 */
gboolean
splashy_init_config (const gchar * _filename)
{
        DEBUG_PRINT ("ENTERING %s (%s)",  __FUNCTION__, _filename);
        /*
         * sanity check 
         */
        if (splashy_xml_config != NULL) /* splashy_xml_config has been
                                         * initialized */
                return TRUE;
        /*
         * slurp our config file to splashy_xml_config 
         */
        gboolean ret = xml_parser_init (_filename);
        if (ret == TRUE)
        {
                splashy_xml_config = xml_parser_get_db ();
                /*
                 * set default theme path now 
                 */
                GString *theme_path = g_string_new (splashy_get_theme_dir ());
                g_string_append (theme_path,
                                 G_DIR_SEPARATOR_S
                                 SPL_THEME_CONFIG_FILE_NAME);

                DEBUG_PRINT ("%s: constructed theme path <<%s>>",
                             __FUNCTION__, theme_path->str);
                /*
                 * now the good part, if this SPL_THEME_CONFIG_FILE_NAME file doesn't 
                 * exist at the current path, we need to fall back to our default file
                 * SPL_DEFAULT_THEME in SPL_CONFIG_DIR+SPL_CONFIG_FILE_NAME 
                 * (old: /etc/splashy/config.xml) must have
                 * a full path to the fallback theme folder
                 */
                if (!g_file_test (theme_path->str, G_FILE_TEST_IS_REGULAR))
                {
                        g_string_free (theme_path, TRUE);
                        theme_path = g_string_new ("");
                        /*
                         * assumes SPL_DEFAULT_THEME has a full path to the
                         * theme 
                         */
                        g_string_append (theme_path,
                                         xml_parser_get_text
                                         (SPL_DEFAULT_THEME));
                        g_string_append (theme_path,
                                         G_DIR_SEPARATOR_S
                                         SPL_THEME_CONFIG_FILE_NAME);
                }
                DEBUG_PRINT ("%s: attempting to read theme <<%s>>",
                             __FUNCTION__, theme_path->str);

                /*
                 * append tags from our theme file to our main XML db 
                 */
                xml_parser_set_xml_file (theme_path->str);
                if (!xml_parser_read (splashy_xml_config))
                {
                        DEBUG_PRINT("%s: failed to init themes", __FUNCTION__);
                        return FALSE;
                }
        }
        return ret;
}


