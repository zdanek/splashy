/***************************************************************************
 *            splashy_config-main.c
 *
 *  Sun Sep 11 17:37:34 2005
 *  Copyright  2005  Vincent Amouret
 *  vincent.amouret@gmail.com
 *  Luis Mondesi <lemsx1@gmail.com> 
 ****************************************************************************/
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
 * NOTE TO DEVELOPERS:
 * Please keep in mind that the interface (cli) for splashy_config is very
 * volatile at this moment. When writing front-ends for splashy_config, 
 * please use the long version of the options and not the short ones:
 * i.e. --install-theme, --textarea-border-show... etc..
 * 2006-02-26 16:14 EST - Luis Mondesi <lemsx1@gmail.com> 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>             /* getuid */
#include <getopt.h>             /* getopt_long */
#include <glib/gi18n.h>

#include "common_macros.h"
#include "splashycnf.h"
#include "splashy_config-functions.h"

#define PROGNAME "splashy_config"
#define HELP \
_("A Splashy Configuration Tool\n"\
"\n"\
"usage: %s [option] arg \n"\
"\n"\
"-h, --help                          Display this help\n"\
"-g, --get-key XPATH                 Returns the value of a key from Splashy config file, or the current theme\n"\
"-s, --set-theme THEME               Set THEME as the Splashy theme\n"\
"-i, --install-theme THEME.tar.gz    Install the THEME from a tarball\n"\
"-r, --remove-theme THEME            Remove the THEME\n"\
"    --info                          Display some information\n"\
"-c, --create-theme  [args...]       Create a theme in an interactive mode if no arguments were given\n"\
"                                    Inline mode arguments, the following arguments are obligatory\n"\
"                                    see the man page for a complete list\n"\
"            --name  NAME                 Set the NAME of the new Splashy theme\n"\
"            --boot-image PATH            Upload the boot image located at the specified PATH\n"\
"            --shutdown-image PATH        Upload the shutdown image located at the specified PATH\n"\
"            --error-image PATH           Upload the error image located at the specified PATH\n")
#define USAGE \
_("usage: %s [option] arg \n"\
"[-h, --help][-s, --set-theme THEME][-i, --install-theme THEME.tar.gz]\n"\
"[-r, --remove-theme THEME][--info][-c, --create-theme  [args...]]\n"\
"[-g, --get-key XPATH]\n"\
"See --help for more information\n")

static struct option long_options[] = {
        {"set-theme", required_argument, 0, 's'},
        {"install-theme", required_argument, 0, 'i'},
        {"remove-theme", required_argument, 0, 'r'},
        {"info", no_argument, 0, 'a'},
        {"create-theme", optional_argument, 0, 'c'},
        {"help", no_argument, 0, 'h'},
        {"get-key", required_argument, 0, 'k'},
        {0, 0, 0, 0}
};
static struct option create_options[] = {
        {"name", required_argument, 0, 'a'},
        {"version", required_argument, 0, 'b'},
        {"description", required_argument, 0, 'c'},
        {"urls", required_argument, 0, 'd'},
        {"author", required_argument, 0, 'e'},
        {"progressbar-x", required_argument, 0, 'f'},
        {"progressbar-y", required_argument, 0, 'g'},
        {"progressbar-width", required_argument, 0, 'h'},
        {"progressbar-height", required_argument, 0, 'i'},
        {"progressbar-red", required_argument, 0, 'j'},
        {"progressbar-green", required_argument, 0, 'k'},
        {"progressbar-blue", required_argument, 0, 'l'},
        {"progressbar-alpha", required_argument, 0, 'm'},
        {"progressbar-border-show", required_argument, 0, 'n'},
        {"progressbar-border-red", required_argument, 0, 'o'},
        {"progressbar-border-green", required_argument, 0, 'p'},
        {"progressbar-border-blue", required_argument, 0, 'q'},
        {"progressbar-border-alpha", required_argument, 0, 'r'},
        {"boot-image", required_argument, 0, 's'},
        {"shutdown-image", required_argument, 0, 't'},
        {"error-image", required_argument, 0, 'u'},
        {"background-width", required_argument, 0, 'v'},
        {"background-height", required_argument, 0, 'w'},
        {"textbox-show", required_argument, 0, 'x'},
        {"textbox-x", required_argument, 0, 'y'},
        {"textbox-y", required_argument, 0, 'z'},
        {"textbox-width", required_argument, 0, 'A'},
        {"textbox-height", required_argument, 0, 'B'},
        {"textbox-red", required_argument, 0, 'C'},
        {"textbox-green", required_argument, 0, 'D'},
        {"textbox-blue", required_argument, 0, 'E'},
        {"textbox-alpha", required_argument, 0, 'F'},
        {"textbox-border-show", required_argument, 0, 'G'},
        {"textbox-border-red", required_argument, 0, 'H'},
        {"textbox-border-green", required_argument, 0, 'I'},
        {"textbox-border-blue", required_argument, 0, 'J'},
        {"textbox-border-alpha", required_argument, 0, 'K'},
        {"text-font-file", required_argument, 0, 'L'},
        {"text-font-height", required_argument, 0, 'M'},
        {"text-font-red", required_argument, 0, 'N'},
        {"text-font-green", required_argument, 0, 'O'},
        {"text-font-blue", required_argument, 0, 'P'},
        {"text-font-alpha", required_argument, 0, 'Q'},
        {"autoverbose", required_argument, 0, 'R'},
        {"fadein", required_argument, 0, 'U'},
        {"fadeout", required_argument, 0, 'V'},
        {"progressbar-background-red", required_argument, 0, 'W'},
        {"progressbar-background-green", required_argument, 0, 'X'},
        {"progressbar-background-blue", required_argument, 0, 'Y'},
        {"progressbar-background-alpha", required_argument, 0, 'Z'},
        {"resume-image", required_argument, 0, 'S'},
        {"suspend-image", required_argument, 0, 'T'},
        {"progressbar-direction-boot", required_argument, 0, 300},
        {"progressbar-direction-shutdown", required_argument, 0, 301},
        {"progressbar-direction-resume", required_argument, 0, 302},
        {"progressbar-direction-suspend", required_argument, 0, 303},
        {"progressbar-visibility-boot", required_argument, 0, 304},
        {"progressbar-visibility-shutdown", required_argument, 0, 305},
        {"progressbar-visibility-resume", required_argument, 0, 306},
        {"progressbar-visibility-suspend", required_argument, 0, 307},
        {0, 0, 0, 0}
};

gint
main (gint argc, gchar * argv[])
{

#ifdef ENABLE_NLS
        setlocale (LC_ALL, "");
        bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
        textdomain (GETTEXT_PACKAGE);
#endif

        if (!splashy_init_config (SPL_CONFIG_FILE))
        {
                ERROR_PRINT ("%s",
                             _("Error occured while starting Splashy\n"
                               "Make sure that you can read Splashy's configuration file\n"));
                return RETURN_ERROR;
        }
        g_set_prgname (PROGNAME);

        XmlFields *inline_theme = g_new0 (XmlFields, 1);
        gint retopt = 0;
        gint create_retopt = 0;
        gint option_index = 0;
        gint create_option_index = 0;
        retopt = getopt_long (argc, argv, "hs:i:r:ic::k:g:", long_options,
                              &option_index);
        if (retopt == -1)
        {
                printf (_("Missing arguments\n"));
                printf (USAGE, g_get_prgname ());
                return R_OK;
        }

        gint ret = RETURN_ERROR;
        switch (retopt)
        {
        case 's':
                ret = set_new_theme (optarg);
                break;
        case 'i':
                ret = install_theme (optarg);
                break;
        case 'r':
                ret = remove_theme (optarg);
                break;
        case 'a':
                ret = information ();
                break;
        case 'c':
                if (argv[optind] == NULL)       // interactive mode */
                        ret = create_theme (get_fields ());
                else            // inline mode
                {

                        while (argc > optind)
                        {

                                create_retopt =
                                        getopt_long_only (argc, argv, "",
                                                          create_options,
                                                          &create_option_index);
                                switch (create_retopt)
                                {
                                case 'a':
                                        inline_theme->name =
                                                g_strdup (optarg);
                                        break;
                                case 'b':
                                        inline_theme->version =
                                                g_strdup (optarg);
                                        break;
                                case 'c':
                                        inline_theme->description =
                                                g_strdup (optarg);
                                        break;
                                case 'd':
                                        inline_theme->urls =
                                                g_strdup (optarg);
                                        break;
                                case 'e':
                                        inline_theme->author =
                                                g_strdup (optarg);
                                        break;
                                case 'f':
                                        inline_theme->pb_x =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'g':
                                        inline_theme->pb_y =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'h':
                                        inline_theme->pb_width =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'i':
                                        inline_theme->pb_height =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'j':
                                        inline_theme->pb_red =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'k':
                                        inline_theme->pb_green =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'l':
                                        inline_theme->pb_blue =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'm':
                                        inline_theme->pb_alpha =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'n':
                                        inline_theme->pb_border_show =
                                                g_strdup (optarg);
                                        break;
                                case 'o':
                                        inline_theme->pb_border_red =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'p':
                                        inline_theme->pb_border_green =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'q':
                                        inline_theme->pb_border_blue =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'r':
                                        inline_theme->pb_border_alpha =
                                                g_strtod (optarg, 0);
                                        break;
                                case 's':
                                        inline_theme->bg_boot =
                                                g_strdup (optarg);
                                        break;
                                case 't':
                                        inline_theme->bg_shutdown =
                                                g_strdup (optarg);
                                        break;
                                case 'u':
                                        inline_theme->bg_error =
                                                g_strdup (optarg);
                                        break;
                                case 'v':
                                        inline_theme->bg_width =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'w':
                                        inline_theme->bg_height =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'x':
                                        inline_theme->textbox_show =
                                                g_strdup (optarg);
                                        break;
                                case 'y':
                                        inline_theme->textbox_x =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'z':
                                        inline_theme->textbox_y =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'A':
                                        inline_theme->textbox_width =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'B':
                                        inline_theme->textbox_height =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'C':
                                        inline_theme->textbox_red =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'D':
                                        inline_theme->textbox_green =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'E':
                                        inline_theme->textbox_blue =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'F':
                                        inline_theme->textbox_alpha =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'G':
                                        inline_theme->textbox_border_show =
                                                g_strdup (optarg);
                                        break;
                                case 'H':
                                        inline_theme->textbox_border_red =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'I':
                                        inline_theme->textbox_border_green =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'J':
                                        inline_theme->textbox_border_blue =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'K':
                                        inline_theme->textbox_border_alpha =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'L':
                                        inline_theme->textfont_file =
                                                g_strdup (optarg);
                                        break;
                                case 'M':
                                        inline_theme->textfont_height =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'N':
                                        inline_theme->textfont_red =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'O':
                                        inline_theme->textfont_green =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'P':
                                        inline_theme->textfont_blue =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'Q':
                                        inline_theme->textfont_alpha =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'R':
                                        inline_theme->verbose =
                                                g_strdup (optarg);
                                        break;
                                case 'U':
                                        inline_theme->fadein =
                                                g_strdup (optarg);
                                        break;
                                case 'V':
                                        inline_theme->fadeout =
                                                g_strdup (optarg);
                                        break;
                                case 'W':
                                        inline_theme->pb_bg_red =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'X':
                                        inline_theme->pb_bg_green =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'Y':
                                        inline_theme->pb_bg_blue =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'Z':
                                        inline_theme->pb_bg_alpha =
                                                g_strtod (optarg, 0);
                                        break;
                                case 'S':
                                        inline_theme->bg_resume =
                                                g_strdup (optarg);
                                        break;
                                case 'T':
                                        inline_theme->bg_suspend =
                                                g_strdup (optarg);
                                        break;
                                case 300:
                                        inline_theme->pb_dir_boot =
                                                g_strdup (optarg);
                                        break;
                                case 301:
                                        inline_theme->pb_dir_shutdown =
                                                g_strdup (optarg);
                                        break;
                                case 302:
                                        inline_theme->pb_dir_resume =
                                                g_strdup (optarg);
                                        break;
                                case 303:
                                        inline_theme->pb_dir_suspend =
                                                g_strdup (optarg);
                                        break;
                                case 304:
                                        inline_theme->pb_on_boot =
                                                g_strdup (optarg);
                                        break;
                                case 305:
                                        inline_theme->pb_on_shutdown =
                                                g_strdup (optarg);
                                        break;
                                case 306:
                                        inline_theme->pb_on_resume =
                                                g_strdup (optarg);
                                        break;
                                case 307:
                                        inline_theme->pb_on_suspend =
                                                g_strdup (optarg);
                                        break;

                                }
                        }
                        ret = create_theme (inline_theme);

                        /*
                         * memory free up 
                         */

                        if (inline_theme->name)
                                g_free (inline_theme->name);
                        if (inline_theme->version)
                                g_free (inline_theme->version);
                        if (inline_theme->description)
                                g_free (inline_theme->description);
                        if (inline_theme->urls)
                                g_free (inline_theme->urls);
                        if (inline_theme->author)
                                g_free (inline_theme->author);
                        if (inline_theme->pb_border_show)
                                g_free (inline_theme->pb_border_show);
                        if (inline_theme->bg_boot)
                                g_free (inline_theme->bg_boot);
                        if (inline_theme->bg_shutdown)
                                g_free (inline_theme->bg_shutdown);
                        if (inline_theme->bg_error)
                                g_free (inline_theme->bg_error);
                        if (inline_theme->textbox_show)
                                g_free (inline_theme->textbox_show);
                        if (inline_theme->textbox_border_show)
                                g_free (inline_theme->textbox_border_show);
                        if (inline_theme->textfont_file)
                                g_free (inline_theme->textfont_file);
                        if (inline_theme->verbose)
                                g_free (inline_theme->verbose);
                        if (inline_theme->fadein)
                                g_free (inline_theme->fadein);
                        if (inline_theme->fadeout)
                                g_free (inline_theme->fadeout);
                        if (inline_theme->bg_resume)
                                g_free (inline_theme->bg_resume);
                        if (inline_theme->bg_suspend)
                                g_free (inline_theme->bg_suspend);


                }
                break;
        case 'g':
        case 'k':
                g_print ("%s", splashy_get_config_string (optarg));
                break;
        case 'h':
                printf (HELP, g_get_prgname ());
                break;
        case '?':
                printf (USAGE, g_get_prgname ());
                break;
        }

        return ret;
}
