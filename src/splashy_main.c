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

#include <signal.h>             /* kill() handle SIGCHLD SIGUSR2 */ /* ter_added */

#include "common_macros.h"
#include "splashy_functions.h"
#include "splashycnf.h"

#define USAGE "usage: splashy <boot|shutdown|test> | splashy_chvt <N>"

/* 
 * signal handler. The signal SIGUSR2 is sent by the child process (_splashy_child)
 * if directfb initialisation succeeded
 * this means "ok to exit and let init continue"
 */

void
sig_exit_boot(gint sig)
{
        switch( sig )
        {
                case SIGCHLD:  g_printerr ("Splashy boot: splashy setup via child process failed (SIGCHLD received). Exiting...");
                               break;

                case  SIGUSR2:  g_printerr ("Splashy boot: splashy setup succeeded (SIGUSR2 received).");
                                break;

                default: g_printerr ("Splashy boot: Unexpected signal received.");
                         break;
        }

        DEBUG_PRINT ("Splashy boot caught signal number %d. Exiting...", sig);

        exit(0);
        return;               /* we never reach this */
}

int
main (int argc, char *argv[])
{
        /*
         * we are a daemon ... no need for STDOUT or STDIN
         */
        FILE *fp;
        fp = freopen ("/dev/null", "r+", stdin);
        fp = freopen ("/dev/null", "r+", stdout);
        DEBUG_PRINT ("main() invoked %d", argc);

        /*
         * grep single /proc/cmdline 
         */
        if (g_getenv ("RUNLEVEL") &&
            g_ascii_strncasecmp (g_getenv ("RUNLEVEL"), "1", 1) == 0)
        {
                ERROR_PRINT ("%s", "Single user mode detected. Exiting...");
                return 1;
        }

        if (argc < 2)
        {
                g_printerr ("%s\n", USAGE);
                return 1;
        }

        if (g_ascii_strncasecmp (basename (argv[0]), "splashy_chvt", 12) == 0)
        {
                if (g_ascii_strncasecmp (argv[1], "auto", 4) == 0)
                {
                        /*
                         * This is deprecated 
                         */
                        g_printerr ("keyword 'auto' is deprecated\n%s\n",
                                    USAGE);
                        return 1;
                }
                char *c = argv[1];
                for (c = argv[1]; *c != '\0'; c++)
                {
                        if (!isdigit (*c))
                        {
                                g_printerr ("%s\n", USAGE);
                                return 1;
                        }
                }
                /*
                 * behave like chvt 
                 */
                splashy_chvt (atoi (argv[1]));
                return 0;
        }

        if (!splashy_init_config (SPL_CONFIG_FILE))
        {
                ERROR_PRINT ("%s",
                             "Error occured while starting Splashy\n"
                             "Make sure that you can read Splashy's configuration file\n");
                return 1;
        }

        /* register signal handlers if we are doing splashy boot */
          if (g_ascii_strncasecmp (argv[1], "boot", 4) == 0) {
                 (void) signal (SIGCHLD, sig_exit_boot);   /* this signal is sent when a child process exits */
                 (void) signal (SIGUSR2, sig_exit_boot);   /* this is the signal sent by _splashy_child indicating directfb is fully initialised */
                                                                                     /* so the parent process can exit, letting init continue. We lock initramfs from */
                                                                                      /* being unmounted while the child process is still setting up directfb. */
                                                                                     /* some machines had a race condition causing splashy failure */
                                                                                     /* This fixes bug 505270 */
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
                /*
                 * FIXME const gchar *_pid_file = xml_parser_get_text
                 * ("/splashy/pid"); 
                 */
                /*
                 * We assume that there is NO splashy running. In fact, this shouldn't 
                 * be our problem if it is already running. the pid file holds the PID 
                 * of the last instance of splashy started. 
                 */
                /*
                 * FIXME FILE *file = g_fopen (_pid_file, "w+"); if (file !=
                 * NULL) { fprintf (file, "%d\n", getpid ()); fclose (file);
                 * } 
                 */
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
        {
                if (g_ascii_strncasecmp (argv[1], "boot", 4) == 0)
                {
                        /*
                         * Wait 1s for signal. There are two possible signals.
                         * The child may die (splashy failed), or the child process
                         * signals that directfb is setup. If the exit signal
                         * does not come in this time, then exit anyway the
                         * signal should come very quickly, so this approach
                         * saves most of a second.
                         */

                        g_usleep(1000000); /* 1s */
                        ERROR_PRINT ("%s",
                                        "Splashy boot terminated. Timed out waiting for Splashy to initialise\n");
                }
                return 0;
        }
}



