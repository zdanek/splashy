/*************************************************************************
 *                      splashy_config-functions.c                       *
 *                                                                       *
 *  Thu Aug 25 16:11:09 2005                                             *
 *  Copyright  2005  Vincent Amouret                                     *
 *  vincent.amouret@gmail.com                                            *
 *************************************************************************/
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>                /* exit */
#include <string.h>                /* strlen */
#include <unistd.h>                /* getuid */
#include <sys/types.h>             /* fork */
#include <sys/wait.h>              /* waitpid */
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <libgen.h>                /* basename */
#include <magic.h>                 /* magic_* , mime type detection */

#include "common_macros.h"
#include "splashycnf.h"
#include "splashy_config-functions.h"

/*
 ** Private function
 */
void need_root (void);
gboolean copy_file (gchar * src_path, gchar * dest_path);
gboolean theme_name_exists (const gchar * theme_name);
gint check_image (const gchar * filename);
void check_fields (XmlFields * check_me);
__ssize_t ask_string (const gchar * message, gchar ** value);
__ssize_t ask_uint (gchar * message, guint * value);
gint tar (gchar * argv[]);

/*
 ** set_new_theme
 ** arg: The name of the theme to apply
 ** desc: Change the theme used by Splashy
 */
gint
set_new_theme (gchar * theme_name)
{
        need_root ();
        gint ret = RETURN_ERROR;
        GString *new_theme =
                g_string_new (splashy_get_config_string (SPL_THEMES_DIR));
        g_string_append (new_theme, G_DIR_SEPARATOR_S);
        g_string_append (new_theme, theme_name);

        g_print (_(">Set theme as: "));
        g_print (theme_name);
        /*
         ** Check if the theme exist
         */
        if (g_file_test (new_theme->str, G_FILE_TEST_IS_DIR)
            &&
            g_file_test (g_build_filename
                         (new_theme->str, SPL_THEME_CONFIG_FILE_NAME, NULL),
                         G_FILE_TEST_IS_REGULAR))
        {

                gen_config_xml ((gchar *)
                                splashy_get_config_string (SPL_THEMES_DIR),
                                theme_name,
                                (gchar *)
                                splashy_get_config_string (SPL_PID_FILE));

                ret = RETURN_OK;
        }

        if (ret == RETURN_OK)
                PRINT_DONE;
        else
                PRINT_FAIL;
              
        g_string_free (new_theme, TRUE);
        return ret;
}

/*
 ** install_theme
 ** desc: install a theme from a tarball 
 ** arg: the tarball path 
 */
gint
install_theme (gchar * tarball_path)
{
        need_root ();
        gint ret = RETURN_ERROR;
        g_print (_(">Install theme"));

        if (g_file_test (tarball_path, G_FILE_TEST_EXISTS))
        {
                gboolean archive_is_valid = FALSE;
                const gchar *msg_invalid_format =
                        _("This format is invalid\n");
                const gchar *mime_type;
                gchar *tar_options[10];
                gint i = 0;
                tar_options[i++] = "tar";
                tar_options[i++] = "--extract";

                magic_t cookie = magic_open (MAGIC_NONE);
                if (cookie == NULL)
                {
                        ERROR_PRINT ("Out of memory\n");
                        return RETURN_ERROR;
                }

                if (magic_load (cookie, NULL) == -1)
                {
                        ERROR_PRINT ("%s", magic_error (cookie));
                        return RETURN_ERROR;
                }

                mime_type = magic_file (cookie, tarball_path);

                if (strncmp ("bzip", mime_type, 4) == 0)
                {
                        tar_options[i++] = "--bzip";
                }
                else if (strncmp ("gzip", mime_type, 4) == 0)
                {
                        tar_options[i++] = "--gzip";
                }
                else
                {
                        if (g_strncasecmp (mime_type, "POSIX tar archive", 17)
                            != 0)
                        {
                                PRINT_FAIL;
                                g_print (_
                                         ("Did you really give me a tarball ?\n"));
                                return RETURN_ERROR;
                        }
                }

                magic_close (cookie);

                tar_options[i++] = "--file";

                gchar *tmp_dir = g_build_filename (g_get_tmp_dir (),
                                                   "splashy-theme-XXXXXX",
                                                   NULL);
                tmp_dir = mkdtemp (g_strdup (tmp_dir));        // Create a temporary directory


                tar_options[i++] =  tarball_path;
                tar_options[i++] = "--directory";
                tar_options[i++] = g_strdup (tmp_dir);
                tar_options[i--] = NULL;

                if (tar(tar_options) != 0)
                {
                        PRINT_FAIL;
                        g_print (_("An error occurred checking theme \n"));
                }
                else
                {
                        // Get the theme name
                        gchar *_theme = NULL;
                        GError *ErrorDir = NULL;
                        GDir *_dir = g_dir_open (tmp_dir, 0, &ErrorDir);
                        if (ErrorDir != NULL)
                        {
                                g_assert (_dir == NULL);
                                PRINT_FAIL;
                                g_print ("%s", msg_invalid_format);
                                fprintf (stderr, "%s\n", ErrorDir->message);
                                g_error_free (ErrorDir);
                                return RETURN_ERROR;
                        }
                        _theme = g_strdup (g_dir_read_name (_dir));
                        g_dir_close (_dir);

                        // Remove the temporary directory                        
                        const gchar *file = NULL;
                        gchar *tmp_theme =
                                g_build_filename (tmp_dir, _theme, NULL);

                        // check if the archive contains SPL_THEME_CONFIG_FILE_NAME
                        if (g_file_test
                            (g_build_filename
                             (tmp_theme, SPL_THEME_CONFIG_FILE_NAME, NULL),
                             G_FILE_TEST_IS_REGULAR))
                                archive_is_valid = TRUE;

                        _dir = g_dir_open (tmp_theme, 0, &ErrorDir);
                        if (ErrorDir != NULL)
                        {
                                g_assert (_dir == NULL);
                                PRINT_FAIL;
                                g_print ("%s", msg_invalid_format);
                                fprintf (stderr, "%s\n", ErrorDir->message);
                                g_error_free (ErrorDir);
                                return RETURN_ERROR;
                        }
                        while ((file = g_dir_read_name (_dir)))
                        {
                                remove (g_strconcat
                                        (tmp_theme, G_DIR_SEPARATOR_S, file,
                                         NULL));
                        }
                        rmdir (tmp_theme); // Clean the tmp dir
                        rmdir (tmp_dir);   // Clean
                        g_dir_close (_dir);
                        g_free (tmp_theme);


                        // Check if a theme with the same name exists
                        gchar *target_path =
                                g_build_filename (splashy_get_config_string
                                                  (SPL_THEMES_DIR), _theme,
                                                  NULL);

                        if (g_strcasecmp
                            (target_path,
                             splashy_get_config_string (SPL_THEMES_DIR)) != 0)
                        {
                                if (g_file_test
                                    (target_path, G_FILE_TEST_IS_DIR))
                                {
                                        PRINT_FAIL;
                                        g_print (_
                                                 ("This theme already exists\n"));
                                        rmdir (tmp_dir);
                                        return RETURN_ERROR;
                                }
                        }

                        g_free (target_path);
                        g_free (_theme);
                        g_free (tmp_dir);
                }

                if (archive_is_valid)
                {
                        // Change the destination 
                        tar_options[i] = g_strdup( splashy_get_config_string (SPL_THEMES_DIR));
                      
                        // Really install the theme
                        if (tar(tar_options) != 0)
                        {
                                PRINT_FAIL;
                                g_print (_
                                         ("An error occurred installing theme \n"));
                        }
                        else
                        {
                                PRINT_DONE;
                                ret = RETURN_OK;
                        }
                }
                else
                {
                        PRINT_FAIL;
                        g_print (msg_invalid_format);
                }

        }
        else
        {
                PRINT_FAIL;
                g_print (_("The tarball doesn't exist !\n"));
        }
        return ret;
}

/*
 ** remove_theme
 ** desc: Remove a theme deleting his directory
 ** arg: the theme name
 */
gint
remove_theme (gchar * theme_name)
{
        need_root ();
        gint ret = RETURN_ERROR;
        g_print (_(">Remove theme"));
        GString *path =
                g_string_new (splashy_get_config_string (SPL_THEMES_DIR));
        g_string_append (path, G_DIR_SEPARATOR_S);
        g_string_append (path, theme_name);
        g_print (" %s", theme_name);


        const gchar *file = NULL;
        GString *buf = g_string_new ("");
        if (!g_file_test (path->str, G_FILE_TEST_IS_DIR))
        {
                PRINT_FAIL;
                g_print (_("The theme doesn't exist !\n"));
        }
        else if (g_strcasecmp
                 (theme_name,
                  splashy_get_config_string (SPL_CURRENT_THEME)) == 0)
        {
                PRINT_FAIL;
                g_print (_("This theme is currently used !\n"));

        }
        else
        {
                DEBUG_PRINT ("splashy_config: Reading files from %s \n",
                             path->str);
                GError *ErrorDir = NULL;
                GDir *_dir = g_dir_open (path->str, 0, &ErrorDir);
                if (ErrorDir != NULL)
                {
                        g_assert (_dir == NULL);
                        PRINT_FAIL;
                        fprintf (stderr, "%s\n", ErrorDir->message);
                        g_error_free (ErrorDir);
                        return RETURN_ERROR;
                }
                while ((file = g_dir_read_name (_dir)))
                {

                        g_string_assign (buf, path->str);
                        g_string_append (buf, G_DIR_SEPARATOR_S);
                        g_string_append (buf, file);
                        DEBUG_PRINT ("splashy_config: Removing file %s \n",
                                     buf->str);

                        if (remove (buf->str) != 0)
                        {
                                PRINT_FAIL;
                        }

                }
                g_dir_close (_dir);
                if (rmdir (path->str) != 0)
                {
                        PRINT_FAIL;
                }
                else
                {
                        PRINT_DONE;
                        ret = RETURN_OK;
                }
        }

        g_string_free (buf, TRUE);
        g_string_free (path, TRUE);
        return ret;
}

/*
 ** information
 ** desc: Display some informations
 */
gint
information (void)
{
        g_print (_(">Theme currently used:\n"));
        g_print ("                      %s\n",
                 splashy_get_config_string ("/splashy/current_theme"));
        g_print ("                      version %s\n",
                 splashy_get_config_string ("/splashy/info/version"));
        g_print ("                         %s\n",
                 splashy_get_config_string ("/splashy/info/description"));
        g_print (_("                      URLs %s\n"),
                 splashy_get_config_string ("/splashy/info/urls"));
        g_print (_("                      by %s\n"),
                 splashy_get_config_string ("/splashy/info/author"));

        g_print (_(">Installed themes:\n"));

        const gchar *_theme = NULL;
        const gchar *themes_dir = splashy_get_config_string (SPL_THEMES_DIR);
        DEBUG_PRINT ("splashy_config: Reading themes from %s \n", themes_dir);
        GError *ErrorDir = NULL;
        GDir *_dir = g_dir_open (themes_dir, 0, &ErrorDir);
        if (ErrorDir != NULL)
        {
                g_assert (_dir == NULL);
                fprintf (stderr, "%s\n", ErrorDir->message);
                g_error_free (ErrorDir);
                return RETURN_ERROR;
        }
        while ((_theme = g_dir_read_name (_dir)))
        {
                gchar *_theme_config = g_build_filename (themes_dir, _theme,
                                                         SPL_THEME_CONFIG_FILE_NAME,
                                                         NULL);
                DEBUG_PRINT ("splashy_config: testing theme dir %s\n",
                             _theme_config);
                if (g_file_test (_theme_config, G_FILE_TEST_IS_REGULAR))
                {
                        g_print ("                      %s\n",
                                 g_path_get_basename (_theme));
                }
        }
        g_dir_close (_dir);
        return RETURN_OK;
}

/*
 ** A simple test if root priviledges are required
 */
void
need_root (void)
{
        if (getuid () != 0)
        {
                g_print (_
                         ("<!> Insufficient priviledges <!>\nRoot priviledges required for this function\n"));
                exit (RETURN_NEED_ROOT);
        }
}

/*
 ** create_theme
 ** desc: create a new theme in an interactive mode
 */
gint
create_theme (XmlFields * NewTheme)
{

        g_print (_(">Create theme "));
        if (NewTheme->name != NULL)
        {
                gboolean font_file_exists = FALSE;
                const gchar *msg_file_doesnot_exists =
                        _("This file doesn't exists: %s\n");
                gchar *theme_path =
                        g_build_filename (splashy_get_config_string
                                          (SPL_THEMES_DIR), NewTheme->name,
                                          NULL);

                // Fields not used
                NewTheme->bg_x = NewTheme->bg_y = 0;

                if (theme_name_exists (NewTheme->name) == TRUE)
                {
                        PRINT_FAIL;
                        g_print (_("This theme already exists\n"));
                        return RETURN_ERROR;
                }

                // Check the boot image
                gint chk = check_image (NewTheme->bg_boot);
                if (chk == 1)
                {
                        PRINT_FAIL;
                        g_print (msg_file_doesnot_exists, NewTheme->bg_boot);
                        return RETURN_ERROR;
                }
                else if (chk == 2)
                {
                        PRINT_FAIL;
                        g_print (_
                                 ("The boot image doesn't seem to be an image\n"));
                        return RETURN_ERROR;
                }

                // Check the shutdown image
                chk = check_image (NewTheme->bg_shutdown);
                if (chk == 1)
                {
                        PRINT_FAIL;
                        g_print (msg_file_doesnot_exists,
                                 NewTheme->bg_shutdown);
                        return RETURN_ERROR;
                }
                else if (chk == 2)
                {
                        PRINT_FAIL;
                        g_print (_
                                 ("The shutdown image doesn't seem to be an image\n"));
                        return RETURN_ERROR;
                }

                // Check the error image
                chk = check_image (NewTheme->bg_error);
                if (chk == 1)
                {
                        PRINT_FAIL;
                        g_print (msg_file_doesnot_exists, NewTheme->bg_error);
                        return RETURN_ERROR;
                }
                else if (chk == 2)
                {
                        PRINT_FAIL;
                        g_print (_
                                 ("The error image doesn't seem to be an image\n"));
                        return RETURN_ERROR;
                }

                // Check the resume image
                chk = check_image (NewTheme->bg_resume);
                if (chk == 1)
                {
                        PRINT_FAIL;
                        g_print (msg_file_doesnot_exists,
                                 NewTheme->bg_resume);
                        return RETURN_ERROR;
                }
                else if (chk == 2)
                {
                        PRINT_FAIL;
                        g_print (_
                                 ("The resume image doesn't seem to be an image\n"));
                        return RETURN_ERROR;
                }

                // Check the suspend image
                chk = check_image (NewTheme->bg_suspend);
                if (chk == 1)
                {
                        PRINT_FAIL;
                        g_print (msg_file_doesnot_exists,
                                 NewTheme->bg_suspend);
                        return RETURN_ERROR;
                }
                else if (chk == 2)
                {
                        PRINT_FAIL;
                        g_print (_
                                 ("The suspend image doesn't seem to be an image\n"));
                        return RETURN_ERROR;
                }

                // Check the font file
                if (NewTheme->textfont_file == NULL)
                {
                        NewTheme->textfont_file = "FreeSans.ttf";        // A dummy name
                }
                else
                {
                        DEBUG_PRINT
                                ("splashy_config: Checking if '%s' exists (font file)\n",
                                 NewTheme->textfont_file);
                        if (!g_file_test
                            (NewTheme->textfont_file, G_FILE_TEST_EXISTS))
                        {
                                PRINT_FAIL;
                                g_print (msg_file_doesnot_exists,
                                         NewTheme->textfont_file);
                                return RETURN_ERROR;
                        }
                        else
                        {
                                font_file_exists = TRUE;
                        }
                }

                // Check and apply default values
                check_fields (NewTheme);

                // Create the target directory
                if (g_mkdir (theme_path, 0755) != 0)
                {
                        g_print (_("\nError! Can't create directory %s\n"),
                                 theme_path);
                        g_print (_("Writing theme %s to current directory\n"),
                                 NewTheme->name);
                        if (g_mkdir (NewTheme->name, 0755) != 0)
                        {
                                g_print (_
                                         ("Error! Can't create directory %s\n"),
                                         NewTheme->name);
                                exit (RETURN_ERROR);
                        }
                        else
                        {
                                theme_path = g_strdup (NewTheme->name);
                        }
                }

                /*
                 * upload the boot image 
                 */
                gchar *dest = g_build_filename (theme_path,
                                                g_basename (NewTheme->
                                                            bg_boot),
                                                NULL);
                if (!copy_file (NewTheme->bg_boot, dest))
                {
                        g_print (_
                                 ("Error! Unable to use the picture %s.\nAborting\n"),
                                 dest);
                        remove_theme (NewTheme->name);
                        return RETURN_ERROR;
                }
                else if (g_path_is_absolute (dest))
                {
                        NewTheme->bg_boot = g_strdup (dest);
                }
                else
                {
                        NewTheme->bg_boot =
                                g_strdup (g_basename (NewTheme->bg_boot));
                }

                /*
                 * upload the shutdown image 
                 */
                dest = g_build_filename (theme_path,
                                         g_basename (NewTheme->bg_shutdown),
                                         NULL);
                if (!copy_file (NewTheme->bg_shutdown, dest))
                {
                        g_print (_
                                 ("Error! Unable to use the picture %s.\nAborting\n"),
                                 dest);
                        remove_theme (NewTheme->name);
                        return RETURN_ERROR;
                }
                else if (g_path_is_absolute (dest))
                {
                        NewTheme->bg_shutdown = g_strdup (dest);
                }
                else
                {
                        NewTheme->bg_shutdown =
                                g_strdup (g_basename (NewTheme->bg_shutdown));
                }

                /*
                 * upload the error image 
                 */
                dest = g_build_filename (theme_path,
                                         g_basename (NewTheme->bg_error),
                                         NULL);
                if (!copy_file (NewTheme->bg_error, dest))
                {
                        g_print (_
                                 ("Error! Unable to use the picture %s.\nAborting\n"),
                                 dest);
                        remove_theme (NewTheme->name);
                        return RETURN_ERROR;
                }
                else if (g_path_is_absolute (dest))
                {
                        NewTheme->bg_error = g_strdup (dest);
                }
                else
                {
                        NewTheme->bg_error =
                                g_strdup (g_basename (NewTheme->bg_error));
                }
                
                /*
                 * upload the resume image 
                 */
                dest = g_build_filename (theme_path,
                                         g_basename (NewTheme->bg_resume),
                                         NULL);
                if (!copy_file (NewTheme->bg_resume, dest))
                {
                        g_print (_
                                 ("Error! Unable to use the picture %s.\nAborting\n"),
                                 dest);
                        remove_theme (NewTheme->name);
                        return RETURN_ERROR;
                }
                else if (g_path_is_absolute (dest))
                {
                        NewTheme->bg_resume = g_strdup (dest);
                }
                else
                {
                        NewTheme->bg_resume =
                                g_strdup (g_basename (NewTheme->bg_resume));
                }
                
                /*
                 * upload the suspend image 
                 */
                dest = g_build_filename (theme_path,
                                         g_basename (NewTheme->bg_suspend),
                                         NULL);
                if (!copy_file (NewTheme->bg_suspend, dest))
                {
                        g_print (_
                                 ("Error! Unable to use the picture %s.\nAborting\n"),
                                 dest);
                        remove_theme (NewTheme->name);
                        return RETURN_ERROR;
                }
                else if (g_path_is_absolute (dest))
                {
                        NewTheme->bg_suspend = g_strdup (dest);
                }
                else
                {
                        NewTheme->bg_suspend =
                                g_strdup (g_basename (NewTheme->bg_suspend));
                }

                /*
                 * upload the font file
                 */
                if (font_file_exists)
                {
                        dest = g_build_filename (theme_path,
                                                 g_basename (NewTheme->
                                                             textfont_file),
                                                 NULL);
                        if (!copy_file (NewTheme->textfont_file, dest))
                        {
                                g_print (_
                                         ("Error! Unable to use the picture %s.\nAborting\n"),
                                         dest);
                                remove_theme (NewTheme->name);
                                return RETURN_ERROR;
                        }
                        else
                        {
                                NewTheme->textfont_file =
                                        g_strdup (g_basename
                                                  (NewTheme->textfont_file));
                        }
                }

                gchar *theme_file_path = g_build_filename (theme_path,
                                                           SPL_THEME_CONFIG_FILE_NAME,
                                                           NULL);

                g_print (NewTheme->name);

                gint ret = RETURN_ERROR;
                if (gen_theme_xml (theme_file_path, NewTheme) == 0)
                {
                        PRINT_DONE;
                        ret = RETURN_OK;
                }
                else
                        PRINT_FAIL;

                g_free (theme_file_path);
                g_free (dest);
                g_free (theme_path);
                return ret;
        }
        else
        {
                PRINT_FAIL;
                g_print (_("You try to create a theme without a name\n"));
                return RETURN_ERROR;
        }
}

/**
 * @desc The interactive mode for create_theme
 * @return The data structure with the fields
 */
XmlFields *
get_fields (void)
{
        XmlFields *NewTheme = g_new0 (XmlFields, 1);
        gboolean use_pixel_units = FALSE;

        check_fields (NewTheme);

        // Name
        do
        {
                if (NewTheme->name != NULL)
                {
                        g_free (NewTheme->name);
                        NewTheme->name = NULL;
                }
                ask_string (_("* Name"), &NewTheme->name);
        }
        while (theme_name_exists (NewTheme->name) == TRUE);

        // Boot image
        do
        {
                if (NewTheme->bg_boot != NULL)
                {
                        g_free (NewTheme->bg_boot);
                        NewTheme->bg_boot = NULL;
                }
                ask_string (_("* Boot image"), &NewTheme->bg_boot);
        }
        while (check_image (NewTheme->bg_boot) != 0);

        // Shutdown image
        do
        {
                if (NewTheme->bg_shutdown != NULL)
                {
                        g_free (NewTheme->bg_shutdown);
                        NewTheme->bg_shutdown = NULL;
                }
                ask_string (_("* Shutdown image"), &NewTheme->bg_shutdown);
        }
        while (check_image (NewTheme->bg_shutdown) != 0);

        // Error image
        do
        {
                if (NewTheme->bg_error != NULL)
                {
                        g_free (NewTheme->bg_error);
                        NewTheme->bg_error = NULL;
                }
                ask_string (_("* Error image"), &NewTheme->bg_error);
        }
        while (check_image (NewTheme->bg_error) != 0);

        // Resume image
        do
        {
                if (NewTheme->bg_resume != NULL)
                {
                        g_free (NewTheme->bg_resume);
                        NewTheme->bg_resume = NULL;
                }
                ask_string (_("* Resume image"), &NewTheme->bg_resume);
        }
        while (check_image (NewTheme->bg_resume) != 0);

        // Suspend image
        do
        {
                if (NewTheme->bg_suspend != NULL)
                {
                        g_free (NewTheme->bg_suspend);
                        NewTheme->bg_suspend = NULL;
                }
                ask_string (_("* Suspend image"), &NewTheme->bg_suspend);
        }
        while (check_image (NewTheme->bg_suspend) != 0);

        // Ask if the height & width of the background must be set        
        gchar *resolution = "no";
        ask_string (_
                    ("Do you want to set the resolution (height & width) of these images ? (yes|no)"),
                    &resolution);

        if (strcmp (resolution, "yes") == 0)
        {
                use_pixel_units = TRUE;

                // Width
                while (NewTheme->bg_width == 0)
                        ask_uint (_(" Width"), &NewTheme->bg_width);

                // Height
                while (NewTheme->bg_height == 0)
                        ask_uint (_(" Height"), &NewTheme->bg_height);

        }

        // Version         
        ask_string (_("Version"), &NewTheme->version);

        // Description
        ask_string (_("Description"), &NewTheme->description);

        // URLs
        ask_string (_("URLs"), &NewTheme->urls);

        // Author
        ask_string (_("Author"), &NewTheme->author);

        // Progressbaronboot
        ask_string (_("Do you want to see a progress bar on boot ? (yes|no)"),
                    &NewTheme->pb_on_boot);

        // Direction on boot
        if (g_strcasecmp (NewTheme->pb_on_boot, "yes") == 0)
                ask_string (_(" Direction ? (forward|backward)"),
                            &NewTheme->pb_dir_boot);

        // Progressbaronshutdown
        ask_string (_
                    ("Do you want to see a progress bar on shutdown ? (yes|no)"),
                    &NewTheme->pb_on_shutdown);

        // Direction on shutdown
        if (g_strcasecmp (NewTheme->pb_on_shutdown, "yes") == 0)
                ask_string (_(" Direction ? (forward|backward)"),
                            &NewTheme->pb_dir_shutdown);

        // Progressbar on resume
        ask_string (_
                    ("Do you want to see a progress bar on resume ? (yes|no)"),
                    &NewTheme->pb_on_resume);

        // Direction on resume
        if (g_strcasecmp (NewTheme->pb_on_resume, "yes") == 0)
                ask_string (_(" Direction ? (forward|backward)"),
                            &NewTheme->pb_dir_resume);

        // Progressbar on suspend
        ask_string (_
                    ("Do you want to see a progress bar on suspend ? (yes|no)"),
                    &NewTheme->pb_on_suspend);

        // Direction on suspend
        if (g_strcasecmp (NewTheme->pb_on_suspend, "yes") == 0)
                ask_string (_(" Direction ? (forward|backward)"),
                            &NewTheme->pb_dir_suspend);

        // Progressbar
        if ((g_strcasecmp (NewTheme->pb_on_boot, "yes") == 0)
            || (g_strcasecmp (NewTheme->pb_on_shutdown, "yes") == 0)
            || (g_strcasecmp (NewTheme->pb_on_resume, "yes") == 0)
            || (g_strcasecmp (NewTheme->pb_on_suspend, "yes") == 0))
        {
                g_print (_("Progress bar\n"));

                // X pos
                if (use_pixel_units)
                {
                        ask_uint (_(" X position in pixel units"),
                                  &NewTheme->pb_x);
                }
                else
                {
                        ask_uint (_(" X position in percentage"),
                                  &NewTheme->pb_x);
                }

                // Y pos
                if (use_pixel_units)
                {
                        ask_uint (_(" Y position in pixel units"),
                                  &NewTheme->pb_y);
                }
                else
                {
                        ask_uint (_(" Y position in percentage"),
                                  &NewTheme->pb_y);
                }

                // Width
                while (NewTheme->pb_width == 0)
                {
                        if (use_pixel_units)
                        {
                                ask_uint (_(" Width in pixel units"),
                                          &NewTheme->pb_width);
                        }
                        else
                        {
                                ask_uint (_(" Width in percentage"),
                                          &NewTheme->pb_width);
                        }
                }

                // Height
                while (NewTheme->pb_height == 0)
                {
                        if (use_pixel_units)
                        {
                                ask_uint (_(" Height in pixel units"),
                                          &NewTheme->pb_height);
                        }
                        else
                        {
                                ask_uint (_(" Height in percentage"),
                                          &NewTheme->pb_height);
                        }
                }

                g_print (_(" Color\n"));

                // Red
                ask_uint (_("  Red"), &NewTheme->pb_red);

                // Green
                ask_uint (_("  Green"), &NewTheme->pb_green);

                // Blue
                ask_uint (_("  Blue"), &NewTheme->pb_blue);

                // Alpha
                ask_uint (_("  Alpha"), &NewTheme->pb_alpha);

                g_print (_(" Background color\n"));

                // Red
                ask_uint (_("  Red"), &NewTheme->pb_bg_red);

                // Green
                ask_uint (_("  Green"), &NewTheme->pb_bg_green);

                // Blue
                ask_uint (_("  Blue"), &NewTheme->pb_bg_blue);

                // Alpha
                ask_uint (_("  Alpha"), &NewTheme->pb_bg_alpha);

                // Border or not
                ask_string (_
                            (" Do you want a border around the progress bar ? (yes|no)"),
                            &NewTheme->pb_border_show);

                // Border
                if (g_strcasecmp (NewTheme->pb_border_show, "yes") == 0)
                {
                        g_print (_(" Border color\n"));

                        // Red
                        ask_uint (_("  Red"), &NewTheme->pb_border_red);

                        // Green
                        ask_uint (_("  Green"), &NewTheme->pb_border_green);

                        // Blue
                        ask_uint (_("  Blue"), &NewTheme->pb_border_blue);

                        // Alpha
                        ask_uint (_("  Alpha"), &NewTheme->pb_border_alpha);

                }                // if border
        }                        // if progressbar

        // Auto verbose
        ask_string (_("Pass in verbose mode on error ? (yes|no)"),
                    &NewTheme->verbose);

        // Text box or not
        ask_string (_("Do you want to see a text box ? (yes|no)"),
                    &NewTheme->textbox_show);

        // Text box
        if ((g_strcasecmp (NewTheme->verbose, "yes") == 0)
            || (g_strcasecmp (NewTheme->textbox_show, "yes") == 0))
        {
                g_print (_("Text box\n"));

                // X
                if (use_pixel_units)
                {
                        ask_uint (_(" X position in pixel units"),
                                  &NewTheme->textbox_x);
                }
                else
                {
                        ask_uint (_(" X position in percentage"),
                                  &NewTheme->textbox_x);
                }


                // Y
                if (use_pixel_units)
                {
                        ask_uint (_(" Y position in pixel units"),
                                  &NewTheme->textbox_y);
                }
                else
                {
                        ask_uint (_(" Y position in percentage"),
                                  &NewTheme->textbox_y);
                }

                // Width
                while (NewTheme->textbox_width == 0)
                {
                        if (use_pixel_units)
                        {
                                ask_uint (_(" Width in pixel units"),
                                          &NewTheme->textbox_width);
                        }
                        else
                        {
                                ask_uint (_(" Width in percentage"),
                                          &NewTheme->textbox_width);
                        }
                }

                // Height
                while (NewTheme->textbox_height == 0)
                {
                        if (use_pixel_units)
                        {
                                ask_uint (_(" Height in pixel units"),
                                          &NewTheme->textbox_height);
                        }
                        else
                        {
                                ask_uint (_(" Height in percentage"),
                                          &NewTheme->textbox_height);
                        }
                }

                g_print (_(" Color\n"));

                // Red
                ask_uint (_("  Red"), &NewTheme->textbox_red);

                // Green
                ask_uint (_("  Green"), &NewTheme->textbox_green);

                // Blue
                ask_uint (_("  Blue"), &NewTheme->textbox_blue);

                // Alpha
                ask_uint (_("  Alpha"), &NewTheme->textbox_alpha);

                // Border around the text box ?
                ask_string (_
                            (" Do you want a border around the text box ? (yes|no)"),
                            &NewTheme->textbox_border_show);

                if (g_strcasecmp (NewTheme->textbox_border_show, "yes") == 0)
                {
                        g_print (_("  Border color\n"));

                        // Red
                        ask_uint (_("   Red"), &NewTheme->textbox_border_red);

                        // Green
                        ask_uint (_("   Green"),
                                  &NewTheme->textbox_border_green);

                        // Blue
                        ask_uint (_("   Blue"),
                                  &NewTheme->textbox_border_blue);

                        // Alpha
                        ask_uint (_("   Alpha"),
                                  &NewTheme->textbox_border_alpha);
                }

                // text font file
                do
                {
                        if (NewTheme->textfont_file != NULL)
                        {
                                g_free (NewTheme->textfont_file);
                                NewTheme->textfont_file = NULL;
                        }
                        ask_string (_(" Text font file"),
                                    &NewTheme->textfont_file);
                }
                while (g_file_test
                       (NewTheme->textfont_file,
                        G_FILE_TEST_IS_REGULAR) == FALSE);

                // size
                while (NewTheme->textfont_height == 0)
                        ask_uint (_(" Font size"),
                                  &NewTheme->textfont_height);

                g_print (_(" Text color\n"));

                // Red
                ask_uint (_("  Red"), &NewTheme->textfont_red);

                // Green
                ask_uint (_("  Green"), &NewTheme->textfont_green);

                // Blue
                ask_uint (_("  Blue"), &NewTheme->textfont_blue);

                // Alpha
                ask_uint (_("  Alpha"), &NewTheme->textfont_alpha);

        }

        // Fade in ?
        ask_string (_("Do you want a fade in ? (yes|no)"), &NewTheme->fadein);

        // Fade out ?
        ask_string (_("Do you want a fade out ? (yes|no)"),
                    &NewTheme->fadeout);

        return NewTheme;
}



/*
 ** upload_image
 ** desc: upload a file
 ** args: src_path the source file
 **       dest_path the destination file
 */
gboolean
copy_file (gchar * src_path, gchar * dest_path)
{
        FILE *src_file;
        FILE *dest_file;
        struct stat file;
        size_t filesize = 0;
        gchar *content;
        size_t return_count = 0;

        if ((src_file = fopen (src_path, "r")) == NULL)
                return FALSE;

        if (!g_stat (src_path, &file))
                filesize = file.st_size;

        content = (gchar *) g_malloc (filesize);
        
        return_count = fread (content, 1, filesize, src_file);
        if (return_count < filesize)
        {
                g_critical (_("Failed to read the file: %s"), src_path);
                fclose (src_file);
                return FALSE;
        }
        fclose (src_file);

        if ((dest_file = fopen (dest_path, "w+")) == NULL)
        {
                g_critical (_("Error opening dest file %s\n"), dest_path);
                return FALSE;
        }

        return_count = fwrite (content, 1, filesize, dest_file);
        if (return_count < filesize)
        {
                g_critical (_("Failed to write the file: %s"), dest_path);
                fclose (dest_file);
                return FALSE;
        }
        fclose (dest_file);

        g_free (content);
        return TRUE;
}

/**
 * @desc Check if such a name exists
 * @param theme_name The theme name to check
 * @return TRUE if exists else FALSE
 */
gboolean
theme_name_exists (const gchar * theme_name)
{
        gchar *theme_path =
                g_build_filename (splashy_get_config_string (SPL_THEMES_DIR),
                                  theme_name, NULL);

        // Check if a theme with the same name exists
        DEBUG_PRINT ("splashy_config: Checking if '%s' already exists\n",
                     theme_path);
        if (g_strcasecmp
            (theme_path, splashy_get_config_string (SPL_THEMES_DIR)) != 0)
        {
                if (g_file_test (theme_path, G_FILE_TEST_IS_DIR))
                        return TRUE;
        }

        return FALSE;
}

/**
 * @desc Check if the file exists and is an image
 * @param filename The file to check
 * @return 0 if success, 1 if the file doesn't exist, 2 if it isn't an image or -1 if an internal error occured
 */
gint
check_image (const gchar * filename)
{
        const gchar *mime_type;
        gint mime_ret = -1;

        DEBUG_PRINT ("splashy_config: Checking if the file '%s' exists\n",
                     filename);
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                return 1;
        else
        {
                magic_t cookie = magic_open (MAGIC_MIME);
                if (cookie == NULL)
                {
                        ERROR_PRINT ("Out of memory");
                        return -1;
                }

                if (magic_load (cookie, NULL) == -1)
                {
                        ERROR_PRINT ("%s", magic_error (cookie));
                        return -1;
                }

                mime_type = magic_file (cookie, filename);

                DEBUG_PRINT
                        ("splashy_config: Checking the file mime type: %s",
                         mime_type);
                mime_ret = g_strncasecmp (mime_type, "image/", 6);
                magic_close (cookie);

                if (mime_ret != 0)
                        return 2;
        }

        return 0;
}


/**
 * @desc Check the fields and apply a default value if any
 * @param CheckMe The structure to check
*/
void
check_fields (XmlFields * CheckMe)
{
        // Version number
        if (CheckMe->version == NULL)
                CheckMe->version = g_strdup ("0.1");

        // Description
        if (CheckMe->description == NULL)
                CheckMe->description = g_strdup ("...");

        // Url
        if (CheckMe->urls == NULL)
                CheckMe->urls = g_strdup ("http://");

        // Author
        if (CheckMe->author == NULL)
                CheckMe->author = g_strdup (_("Author <author@mail.com>"));

        // Progressbar: x coordinate
        if (CheckMe->bg_width == 0 && CheckMe->bg_height == 0)
        {
                if (CheckMe->pb_x > 100)
                {
                        CheckMe->pb_x = 100;
                }
        }

        // Progressbar: y coordinate
        if (CheckMe->bg_width == 0 && CheckMe->bg_height == 0)
        {
                if (CheckMe->pb_y > 100)
                {
                        CheckMe->pb_y = 100;
                }
        }

        // Progressbar: width
        if (CheckMe->bg_width == 0 && CheckMe->bg_height == 0)
        {
                if (CheckMe->pb_width > 100)
                {
                        CheckMe->pb_width = 100;
                }
        }

        // Progressbar: height
        if (CheckMe->bg_width == 0 && CheckMe->bg_height == 0)
        {
                if (CheckMe->pb_height > 100)
                {
                        CheckMe->pb_height = 100;
                }
        }

        // Progressbar: red color channel
        if (CheckMe->pb_red > 255)
                CheckMe->pb_red = 255;

        // Progressbar: green color channel
        if (CheckMe->pb_green > 255)
                CheckMe->pb_green = 255;

        // Progressbar: blue color channel
        if (CheckMe->pb_blue > 255)
                CheckMe->pb_blue = 255;

        // Progressbar: alpha color channel
        if (CheckMe->pb_alpha > 255)
                CheckMe->pb_alpha = 255;

        // Progressbar: a border ?
        if (CheckMe->pb_border_show == NULL
            || ((g_strcasecmp (CheckMe->pb_border_show, "yes") != 0
                 && g_strcasecmp (CheckMe->pb_border_show, "no") != 0)))
        {
                CheckMe->pb_border_show = NULL;
                CheckMe->pb_border_show = g_strdup ("no");
        }

        // Progressbar: border red
        if (CheckMe->pb_border_red > 255)
                CheckMe->pb_border_red = 255;

        // Progressbar: border green
        if (CheckMe->pb_border_green > 255)
                CheckMe->pb_border_green = 255;

        // Progressbar: border blue
        if (CheckMe->pb_border_blue > 255)
                CheckMe->pb_border_blue = 255;

        // Progressbar: border alpha
        if (CheckMe->pb_border_alpha > 255)
                CheckMe->pb_border_alpha = 255;

        // Progressbar: background red
        if (CheckMe->pb_bg_red > 255)
                CheckMe->pb_bg_red = 255;

        // Progressbar: background green
        if (CheckMe->pb_bg_green > 255)
                CheckMe->pb_bg_green = 255;

        // Progressbar: background blue
        if (CheckMe->pb_bg_blue > 255)
                CheckMe->pb_bg_blue = 255;

        // Progressbar: background alpha
        if (CheckMe->pb_bg_alpha > 255)
                CheckMe->pb_bg_alpha = 255;

        // Progressbar: direction on boot
        if (CheckMe->pb_dir_boot == NULL
            || (g_strcasecmp (CheckMe->pb_dir_boot, "forward") != 0
                && g_strcasecmp (CheckMe->pb_dir_boot, "backward") != 0))
        {
                CheckMe->pb_dir_boot = g_strdup ("forward");
        }

        // Progressbar: direction on shutdown
        if (CheckMe->pb_dir_shutdown == NULL
            || (g_strcasecmp (CheckMe->pb_dir_shutdown, "forward") != 0
                && g_strcasecmp (CheckMe->pb_dir_shutdown, "backward") != 0))
        {
                CheckMe->pb_dir_shutdown = g_strdup ("backward");
        }

        // Progressbar: direction on resume
        if (CheckMe->pb_dir_resume == NULL
            || (g_strcasecmp (CheckMe->pb_dir_resume, "forward") != 0
                && g_strcasecmp (CheckMe->pb_dir_resume, "backward") != 0))
        {
                CheckMe->pb_dir_resume = g_strdup ("forward");
        }

        // Progressbar: direction on suspend
        if (CheckMe->pb_dir_suspend == NULL
            || (g_strcasecmp (CheckMe->pb_dir_suspend, "forward") != 0
                && g_strcasecmp (CheckMe->pb_dir_suspend, "backward") != 0))
        {
                CheckMe->pb_dir_suspend = g_strdup ("backward");
        }

        // A textbox ?
        if (CheckMe->textbox_show == NULL
            || (g_strcasecmp (CheckMe->textbox_show, "yes") != 0
                && g_strcasecmp (CheckMe->textbox_show, "no") != 0))
        {
                CheckMe->textbox_show = g_strdup ("no");
        }

        // Textbox: x coordinate
        if (CheckMe->bg_width == 0 && CheckMe->bg_height == 0)
        {
                if (CheckMe->textbox_x > 100)
                {
                        CheckMe->textbox_x = 100;
                }
        }

        // Textbox: y coordinate
        if (CheckMe->bg_width == 0 && CheckMe->bg_height == 0)
        {
                if (CheckMe->textbox_y > 100)
                {
                        CheckMe->textbox_y = 100;
                }
        }

        // Textbox: width
        if (CheckMe->bg_width == 0 && CheckMe->bg_height == 0)
        {
                if (CheckMe->textbox_width > 100)
                {
                        CheckMe->textbox_width = 100;
                }
        }

        // Textbox: height
        if (CheckMe->bg_width == 0 && CheckMe->bg_height == 0)
        {
                if (CheckMe->textbox_height > 100)
                {
                        CheckMe->textbox_height = 100;
                }
        }

        // Textbox: red color
        if (CheckMe->textbox_red > 255)
                CheckMe->textbox_red = 255;

        // Textbox: green color
        if (CheckMe->textbox_green > 255)
                CheckMe->textbox_green = 255;

        // Textbox: blue color
        if (CheckMe->textbox_blue > 255)
                CheckMe->textbox_blue = 255;

        // Textbox: alpha channel
        if (CheckMe->textbox_alpha > 255)
                CheckMe->textbox_alpha = 255;

        // Textbox: a border ?
        if (CheckMe->textbox_border_show == NULL
            || (g_strcasecmp (CheckMe->textbox_border_show, "yes") !=
                0 && g_strcasecmp (CheckMe->textbox_border_show, "no") != 0))
        {
                CheckMe->textbox_border_show = g_strdup ("no");
        }

        // Textbox: border red
        if (CheckMe->textbox_border_red > 255)
                CheckMe->textbox_border_red = 255;

        // Textbox: border green
        if (CheckMe->textbox_border_green > 255)
                CheckMe->textbox_border_green = 255;

        // Textbox: border blue
        if (CheckMe->textbox_border_blue > 255)
                CheckMe->textbox_border_blue = 255;

        // Textbox: border alpha
        if (CheckMe->textbox_border_alpha > 255)
                CheckMe->textbox_border_alpha = 255;

        // Font: red
        if (CheckMe->textfont_red > 255)
                CheckMe->textfont_red = 255;

        // Font: green
        if (CheckMe->textfont_green > 255)
                CheckMe->textfont_green = 255;

        // Font: blue
        if (CheckMe->textfont_blue > 255)
                CheckMe->textfont_blue = 255;

        // Font: alpha
        if (CheckMe->textfont_alpha > 255)
                CheckMe->textfont_alpha = 255;

        // Autoverbose on error ?
        if (CheckMe->verbose == NULL
            || (g_strcasecmp (CheckMe->verbose, "yes") != 0
                && g_strcasecmp (CheckMe->verbose, "no")))
        {
                CheckMe->verbose = g_strdup ("no");
        }

        // Progressbar on boot ?
        if (CheckMe->pb_on_boot == NULL
            || (g_strcasecmp (CheckMe->pb_on_boot, "yes") != 0
                && g_strcasecmp (CheckMe->pb_on_boot, "no")))
        {
                CheckMe->pb_on_boot = g_strdup ("yes");
        }

        // Progressbar on shutdown
        if (CheckMe->pb_on_shutdown == NULL
            || (g_strcasecmp (CheckMe->pb_on_shutdown, "yes")
                != 0 && g_strcasecmp (CheckMe->pb_on_shutdown, "no")))
        {
                CheckMe->pb_on_shutdown = g_strdup ("yes");
        }

        // Progressbar on resume ?
        if (CheckMe->pb_on_resume == NULL
            || (g_strcasecmp (CheckMe->pb_on_resume, "yes") != 0
                && g_strcasecmp (CheckMe->pb_on_resume, "no")))
        {
                CheckMe->pb_on_resume = g_strdup ("yes");
        }

        // Progressbar on suspend ?
        if (CheckMe->pb_on_suspend == NULL
            || (g_strcasecmp (CheckMe->pb_on_suspend, "yes") != 0
                && g_strcasecmp (CheckMe->pb_on_suspend, "no")))
        {
                CheckMe->pb_on_suspend = g_strdup ("yes");
        }

        // Fade in ?
        if (CheckMe->fadein == NULL
            || (g_strcasecmp (CheckMe->fadein, "yes")
                != 0 && g_strcasecmp (CheckMe->fadein, "no")))
        {
                CheckMe->fadein = g_strdup ("yes");
        }

        // Fade out ?
        if (CheckMe->fadeout == NULL
            || (g_strcasecmp (CheckMe->fadeout, "yes")
                != 0 && g_strcasecmp (CheckMe->fadeout, "no")))
        {
                CheckMe->fadeout = g_strdup ("yes");
        }

}

/**
 * @desc Prompt the user for a string with or without a default value
 * @param message The message to display, this message is appended with " [%s]: " if value is not NULL else " : "
 * @param value The value to set which can contain a default value
 * @return The return value of getline(), the bytes read
 * @see getline
 */
__ssize_t
ask_string (const gchar * message, gchar ** value)
{
        gchar *tmp = NULL;
        const gchar *display;
        size_t nbytes = 0;
        __ssize_t bytes_read = 0;

        if (*value != NULL)        // show the default value
                display = g_strconcat (message, " [%s]: ", NULL);
        else
                display = g_strconcat (message, " : ", NULL);

        g_print (display, *value);
        bytes_read = getline (&tmp, &nbytes, stdin);

        if (bytes_read > 1)        // we don't modify the value if only \n is in tmp
        {
                tmp[bytes_read - 1] = '\0';        // Remove the \n
                *value = tmp;
        }

        return bytes_read;
}

/**
 * @desc Prompt the user for an unsigned int
 * @param message The message to display, this message is appended with " [%u]: "
 * @param value The value to set which can contain a default value
 * @return The return value of getline(), the bytes read
 * @see getline
 */
__ssize_t
ask_uint (gchar * message, guint * value)
{
        gchar *tmp = NULL;
        gdouble d_tmp = 0;
        gboolean is_unint = FALSE;
        const gchar *display;
        size_t nbytes = 0;
        __ssize_t bytes_read = 0;

        display = g_strconcat (message, " [%u]: ", NULL);

        while (is_unint == FALSE)
        {
                g_print (display, *value);

                bytes_read = getline (&tmp, &nbytes, stdin);

                if (bytes_read == 1)
                        break;

                if (bytes_read > 1)        // we don't modify the value if only \n is in tmp
                {
                        tmp[bytes_read - 1] = '\0';        // remove the \n
                        d_tmp = strtod (tmp, 0);

                        if (d_tmp >= 0)
                        {
                                *value = d_tmp;
                                is_unint = TRUE;
                        }
                }
        }

        return bytes_read;
}

/**
 * @desc Exec tar, this function is blocant
 * @param argv List of the options passed to tar, terminated by NULL
 * @see execv
 */
gint
tar (gchar * argv[])
{
        pid_t pid;
        gint status;
        if (!(pid = fork ()))
        {
                if (execv ("/bin/tar", argv) == -1)
                {
                        fprintf (stderr, "tar failed to start\n");
                        _exit (1);
                }
        }
        if (pid == -1)
                fprintf (stderr, "fork() failed, the back end hasn't been launched.");
        
        /*
         * Waiting the child...
         */
        waitpid (pid, &status, 0);
        
        return WEXITSTATUS (status);
}
