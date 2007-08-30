/***************************************************************************
 *            splashy_config-functions.h
 *
 *  Thu Aug 25 16:11:38 2005
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

#ifndef _SPLASHY_CONFIG_FUNCTIONS_H
#define _SPLASHY_CONFIG_FUNCTIONS_H



#include <glib.h>

#include "xml_functions.h"

#define PRINT_DONE g_print(_("          [ DONE ]\n"))
#define PRINT_FAIL g_print(_("          [ FAIL ]\n"))

#define RETURN_OK 0
#define RETURN_ERROR 1
#define RETURN_NEED_ROOT 2

#ifdef __cplusplus
extern "C"
{
#endif

        gint set_new_theme (gchar * theme_name);

        gint install_theme (gchar * tarball_path);

        gint remove_theme (gchar * theme_name);

        gint information (void);

        gint create_theme (XmlFields * options);

        XmlFields *get_fields (void);


#ifdef __cplusplus
}
#endif

#endif        /* _SPLASHY_CONFIG_FUNCTIONS_H */
