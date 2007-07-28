/********************************************************************************
 *                              xml_functions.h                                 *
 *                                                                              *
 *  Sat Aug 13 19:56:01 2005                                                    *
 *  Copyright  2005  Vincent Amouret                                            *
 *  vincent.amouret@gmail.com                                                   *
 ********************************************************************************/
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
#ifndef _XML_FUNCTIONS_H
#define _XML_FUNCTIONS_H

#include <stdio.h>
#include <glib.h>

typedef struct _xml_fields
{
	gchar *name;
	gchar *version;
	gchar *description;
	gchar *urls;
	gchar *author;
	/*
	 * 0->100 
	 */
	guint pb_x;
	guint pb_y;
	guint pb_width;
	guint pb_height;
	/*
	 * 0->255 
	 */
	guint pb_red;
	guint pb_green;
	guint pb_blue;
	guint pb_alpha;

	gchar *pb_border_show;
	/*
	 * 0->255 
	 */
	guint pb_border_red;
	guint pb_border_green;
	guint pb_border_blue;
	guint pb_border_alpha;
        
        guint pb_bg_red;
	guint pb_bg_green;
	guint pb_bg_blue;
	guint pb_bg_alpha;

        gchar *pb_dir_boot; 
        gchar *pb_dir_shutdown;
        gchar *pb_dir_resume;
        gchar *pb_dir_suspend;
        
        gchar *pb_on_boot; 
        gchar *pb_on_shutdown;
        gchar *pb_on_resume;
        gchar *pb_on_suspend;
 
	gchar *bg_boot;
        gchar *bg_shutdown;
        gchar *bg_resume;
        gchar *bg_suspend;
	gchar *bg_error;

	guint bg_x;
	guint bg_y;
	guint bg_width;
	guint bg_height;

	gchar *textbox_show;
	/*
	 * 0->100 
	 */
	guint textbox_x;
	guint textbox_y;
	guint textbox_width;
	guint textbox_height;
	/*
	 * 0->255 
	 */
	guint textbox_red;
	guint textbox_green;
	guint textbox_blue;
	guint textbox_alpha;

	gchar *textbox_border_show;
	/*
	 * 0->255 
	 */
	guint textbox_border_red;
	guint textbox_border_green;
	guint textbox_border_blue;
	guint textbox_border_alpha;

	gchar *textfont_file;
	guint textfont_height;
	guint textfont_red;
	guint textfont_green;
	guint textfont_blue;
	guint textfont_alpha;

	gchar *verbose;
	gchar *fadein;
	gchar *fadeout;
} XmlFields;

#ifdef __cplusplus
extern "C"
{
#endif


	void gen_config_xml (gchar * spl_theme_path,
			     gchar * spl_current_path,
			     gchar * spl_pid_path);

	int gen_theme_xml (gchar * theme_file, XmlFields * spl_theme);

#ifdef __cplusplus
}
#endif

#endif				/* _XML_FUNCTIONS_H */
