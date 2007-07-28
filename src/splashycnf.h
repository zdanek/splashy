/*
 * This file is part of Splashy.
 * 
 * Splashy is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any
 * later version.
 * 
 * Splashy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty 
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with Splashy; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301 USA 
 */
/*
 * This uses xml_parser to get configuration options from our configuration
 * and theme files. Leaving xml_parser as an all purpose, pristine, xml parser.
 */

#ifndef _XML_CONFIG_H
#define _XML_CONFIG_H

/*
 * default location for our main config file. @see doc/ for examples 
 */
/* TODO use this name and instead of SPL_CONFIG_FILE, do 
  * SPL_CONFIG_DIR+SPL_CONFIG_FILE_NAME and better yet, use a .in
  * file to generate this file with the right path from automake Makefile.am
  */
#define SPL_CONFIG_DIR "/etc/splashy"
#define SPL_CONFIG_FILE_NAME "config.xml"
#define SPL_CONFIG_FILE "/etc/splashy/config.xml"	/* DEPRECATED */
/*
 * our themes are simple directories with a config.xml file pointing to
 * pictures and setting other progressbar and theme related variables 
 */
#define SPL_THEME_CONFIG_FILE_NAME "theme.xml"
/** xpaths for common xml strings in config files */
#define SPL_AUTO_VERBOSE_ON_ERROR       "/splashy/autoverboseonerror"
#define SPL_CURRENT_THEME               "/splashy/current_theme"
#define SPL_DEFAULT_THEME               "/splashy/default_theme"
#define SPL_THEMES_DIR                  "/splashy/themes"
#define SPL_PID_FILE                    "/splashy/pid"


/** public functions */
int splashy_init_config (const char * _filename);
const char * splashy_get_theme_dir ();
const char * splashy_image_path (const char * image_xpath);
const char * splashy_get_config_string (const char * xpath);
int splashy_get_config_int (const char * xpath, int base);
int splashy_change_config_file (const char* filename);


#endif /* _XML_CONFIG_H */
