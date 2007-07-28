/************************************************************************
 *                      splashy_main.c                                  *
 *                                                                      *
 *  2005-05-28 03:01 EDT                                                *
 *  Copyright  2005   Luis Mondesi    <lemsx1@gmail.com>                *
 *                                                                      *
 ************************************************************************/
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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <ctype.h>              /* isdigit() */
#include <libgen.h>             /* basename() */
#include <stdlib.h>
#include <unistd.h>             /* fork(),sysconf(_SC_OPEN_MAX) and related */
#include <glib.h>
#include <glib/gstdio.h>        /* g_fopen() */

#include "common_macros.h"
#include "splashy_functions.h"
#include "splashycnf.h"

#define USAGE "usage: splashy <boot|shutdown|test> | splashy_chvt <N>"

int
main (int argc, char *argv[])
{
        /* 
         * we are a daemon ... no need for STDOUT or STDIN
         */
        freopen("/dev/null","r+",stdin);
        freopen("/dev/null","r+",stdout);
	DEBUG_PRINT ("main() invoked %d", argc);

	/* grep single /proc/cmdline */
        if (g_getenv ("RUNLEVEL") &&
                        g_ascii_strncasecmp (g_getenv ("RUNLEVEL"), "1", 1) == 0)
        {
                ERROR_PRINT ("%s",
                                "Single user mode detected. Exiting...");
                return 1;
        }

        if (argc < 2)
        {
                g_printerr ("%s\n", USAGE);
                return 1;
        }
        
        if ( g_ascii_strncasecmp (basename(argv[0]),"splashy_chvt",12) == 0 )
        {
                char *c = argv[1];
                for (c = argv[1]; *c != '\0'; c++)
                        if ( ! isdigit(*c) )
                        {
                                g_printerr ("%s\n", USAGE);
                                return 1;
                        }
                /* behave like chvt */
                splashy_chvt (atoi(argv[1]));
                return 0;
        }

        if (!splashy_init_config (SPL_CONFIG_FILE)) 
	{
		ERROR_PRINT ("%s",
			     "Error occured while starting Splashy\n"
			     "Make sure that you can read Splashy's configuration file\n");
		return 1;
        }

        DEBUG_PRINT ("before child, my pid is %d\n", getpid ());
        gint child = fork ();
        if (child < 0)
        {
                ERROR_PRINT ("%s",
                                "Sorry, could not send Splashy to the background. Bailing out...");
                exit (1);
        }
        if (child == 0)
        {
                /*
                 * child 
                 */
                DEBUG_PRINT ("Child: my parent's pid is %d\n", getppid ());
                /*
                 * save your pid if file doesn't exist 
                 */
                /* FIXME const gchar *_pid_file = xml_parser_get_text ("/splashy/pid"); */
                /*
                 * We assume that there is NO splashy running. In fact, this shouldn't 
                 * be our problem if it is already running. the pid file holds the PID 
                 * of the last instance of splashy started. 
                 */
                /* FIXME FILE *file = g_fopen (_pid_file, "w+");
                if (file != NULL)
                {
                        fprintf (file, "%d\n", getpid ());
                        fclose (file);
                } */
                /*
                 * handle arguments 
                 */
                if (g_ascii_strncasecmp (argv[1], "boot", 4) == 0)
                {
                        DEBUG_PRINT ("Calling splashy_child_start()");
                        splashy_child_start ();
                }
                else if (g_ascii_strncasecmp (argv[1], "shutdown", 8) == 0)
                {
                        DEBUG_PRINT ("Calling splashy_child_stop()");
                        splashy_child_stop ();
                }
                else if (g_ascii_strncasecmp (argv[1], "preview", 7) == 0
                                || g_ascii_strncasecmp (argv[1], "test", 4) == 0)
                {
                        DEBUG_PRINT ("Calling splashy_child_test()");
                        splashy_child_test ();
                }
                else
                {
                        DEBUG_PRINT ("No argument given");
                        g_printerr ("%s\n", USAGE);
                }
                splashy_child_exit ();
        }
        /*
         * parent 
         */
        DEBUG_PRINT ("After child, my pid is %d\n", getpid ());
        DEBUG_PRINT ("After child, my child pid is %d\n", child);
        return 0;
}
