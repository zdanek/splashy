/***************************************************************************
 *            xml_functions.c
 *
 *  Sat Aug 13 19:56:58 2005
 *  Copyright  2005  Vincent Amouret
 *  vincent.amouret@gmail.com
 ****************************************************************************/

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

#include "xml_functions.h"
#include "xml_format.h"
#include "splashycnf.h"
#include <glib/gi18n.h>
/*
 ** gen_config_xml
 ** desc: generate the SPL_CONFIG_FILE_NAME file
 ** args: spl_theme_path
 **       spl_pid_path
 ** the xml_fields
 */
        void
gen_config_xml (gchar * spl_theme_path,
                gchar * spl_current_path,
                gchar * spl_pid_path)
{
        gchar* config_xml = g_markup_printf_escaped (CONFIG_XML_FORMAT,
                spl_theme_path,
                spl_current_path, spl_pid_path);
        FILE *file = fopen (SPL_CONFIG_FILE, "w+");
        fprintf (file,"%s",config_xml);
        fclose (file);
        g_free(config_xml);
}

/*
 ** gen_theme_xml
 ** desc: generate the SPL_THEME_CONFIG_FILE_NAME file of the themes 
 ** args: theme_file -> the complete filename path
 **       struct of xml fields
 */

        int
gen_theme_xml (gchar *theme_file, XmlFields *spl_theme)
{
       gchar* theme_xml = g_markup_printf_escaped (
                        THEME_XML_FORMAT,
                        spl_theme->name,
                        spl_theme->version,
                        spl_theme->description,
                        spl_theme->urls,
                        spl_theme->author,
                        spl_theme->pb_x,
                        spl_theme->pb_y,
                        spl_theme->pb_width,
                        spl_theme->pb_height,
                        spl_theme->pb_red,
                        spl_theme->pb_green,
                        spl_theme->pb_blue,               
                        spl_theme->pb_alpha,
                        spl_theme->pb_border_show,
                        spl_theme->pb_border_red,
                        spl_theme->pb_border_green,
                        spl_theme->pb_border_blue,
                        spl_theme->pb_border_alpha,
                        spl_theme->pb_bg_red,
                        spl_theme->pb_bg_green,
                        spl_theme->pb_bg_blue,
                        spl_theme->pb_bg_alpha,                        
                        spl_theme->pb_dir_boot,
                        spl_theme->pb_dir_shutdown,
                        spl_theme->pb_dir_resume,
                        spl_theme->pb_dir_suspend,
                        spl_theme->pb_on_boot,
                        spl_theme->pb_on_shutdown,
                        spl_theme->pb_on_resume,
                        spl_theme->pb_on_suspend,
                        spl_theme->bg_boot,
                        spl_theme->bg_shutdown,
                        spl_theme->bg_resume,
                        spl_theme->bg_suspend,
                        spl_theme->bg_error,
                        spl_theme->bg_x,
                        spl_theme->bg_y,
                        spl_theme->bg_width,
                        spl_theme->bg_height,
                        spl_theme->textbox_show,
                        spl_theme->textbox_x,
                        spl_theme->textbox_y,
                        spl_theme->textbox_width,
                        spl_theme->textbox_height,
                        spl_theme->textbox_red,
                        spl_theme->textbox_green,
                        spl_theme->textbox_blue,
                        spl_theme->textbox_alpha,
                        spl_theme->textbox_border_show,
                        spl_theme->textbox_border_red,
                        spl_theme->textbox_border_green,
                        spl_theme->textbox_border_blue,
                        spl_theme->textbox_border_alpha,
                        spl_theme->textfont_file,
                        spl_theme->textfont_height,
                        spl_theme->textfont_red,
                        spl_theme->textfont_green,
                        spl_theme->textfont_blue,
                        spl_theme->textfont_alpha,
                        spl_theme->verbose,
                        spl_theme->fadein,
                        spl_theme->fadeout
                );
        FILE *file;
        if ((file = fopen (theme_file, "w+")) == NULL)
        {
                g_print (_("Error: Could not open theme file for writing\n"));
                return -1;
        }
        fprintf (file,"%s",theme_xml);
        fclose (file);
        g_free(theme_xml);
        return 0;
}
