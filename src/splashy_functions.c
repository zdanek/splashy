/************************************************************************
 *                              functions.c                             *
 *                                                                      *
 *  2005-05-28 03:01 EDT                                                *
 *  Copyright  2005  Otavio Salvador <otavio@debian.org>                *
 *                   Luis Mondesi    <lemsx1@gmail.com>                 *
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
 *
 * Bugs: 
 *  - openpty() is only in glibc
 *  - uses VT_* calls on Linux
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/*
 * Standard headers 
 */
#ifndef _GNU_SOURCE
/*
 * Allow GNU specific funtions: *_unlocked() in stdio.h 
 */
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */
#include <errno.h>              /* needed by all file manipulating functions */
#include <stdio.h>              /* NULL and others */
#include <stdlib.h>             /* strtol() exit() */
#include <unistd.h>             /* fork() getpid() unlink() */
#include <pthread.h>            /* pthread functions and data structures */
/*
 * TODO get rid of string.h. See FIXME's 
 */
#include <string.h>             /* strlen() and str*() fam */
#include <signal.h>             /* kill() handle SIGHUP SIGUSR1 SIGUSR2 */
#include <time.h>               /* nanosleep() */
#include <glib.h>               /* too many... */
#include <glib/gstdio.h>        /* g_fopen() */
#include <fcntl.h>              /* open() */
#include <sys/ioctl.h>          /* ioctl() */
#include <sys/types.h>          /* needed by regex.h regcomp() */
#include <regex.h>              /* regexec() regcomp() */
#include <sys/poll.h>           /* poll() */
#ifdef __linux__
/*
 * #include <sys/mount.h> USE for mounting /proc if not mounted yet 
 */
#include <linux/vt.h>           /* VT_* */
#endif

#include <sys/socket.h>         /* socket(), bind(), ... */
#include <sys/un.h>

#include <directfb_keyboard.h>  /* DIKI_F2 etc */

#include "common_macros.h"
#include "splashycnf.h"
#include "splashy_functions.h"
#include "splashy.h"

static gint last_progress = 0;  /* how many exact pixels to draw in the
                                 * progressbar (0-100). Lock before using
                                 * this value. */

static gint arg_progress = 0;   /* value to be sent to last_progress from
                                 * SplashyClient. @see keyevent_loop() */

static gboolean exiting = FALSE;        /* threads should read this before
                                         * doing anything at all. this is set 
                                         * * * * * to true at cmd_exit() */
static gboolean read_console = FALSE;   /* flag to turn on/off reading the
                                         * /dev/vcs* files */
static gboolean switched_to_verbose = FALSE;    /* flag to know when verbose
                                                 * image is displayed */
static gboolean F2_toggle_pressed = FALSE;      /* keeps track of the depress 
                                                 * F2 key */
static gint timeout = 120;      /* time out when reading from splashy socket
                                 * (in seconds). If splashy doesn't recvmsg
                                 * for this long, splashy will exit. */

pthread_mutex_t key_mut = PTHREAD_MUTEX_INITIALIZER;


/*
 * Some helper functions
 */

void
splashy_chvt (gint vt_no)
{
        gint fd = g_open ("/dev/tty0", O_RDONLY | O_WRONLY);
        if (fd < 0)
                return;
        if (!ioctl (fd, VT_ACTIVATE, vt_no))
                ioctl (fd, VT_WAITACTIVE, vt_no);
        close (fd);
}

/**
 * @desc looks for pattern in string ignoring known failure text messages
 * @param perr_pattern a valid regular expression pattern. @see man regex
 * @param str string to search for pattern in
 * @return true if pattern was found
 */
inline gboolean
search_pattern_str (const gchar * perr_pattern, const gchar * str,
                    gshort use_ignore)
{
        gboolean ret = FALSE;   /* assumes we will fail */
        gshort ignore_error = 0;        /* we assume that we will be ignoring 
                                         * patterns */
        /*
         * by default we ignore our own errors and loading kernel modules 
         */
        const gchar *ignore_pattern =
                "(splashy|Cannot[[:space:]]+test[[:space:]]+file|load[[:space:]]+module|fatal:[[:space:]]+module[[:space:]].*[[:space:]]not[[:space:]]found|fatal:[[:space:]]+error[[:space:]]+inserting[[:space:]].*|warning:[[:space:]]+error[[:space:]]+inserting.*)";
        regex_t ignore_preg;
        regex_t preg;

        if (perr_pattern == NULL || str == NULL)
                return FALSE;

        /*
         * ignore_error should be zero 
         */
        if (use_ignore)
        {
                ignore_error = regcomp (&ignore_preg, ignore_pattern,
                                        REG_EXTENDED | REG_ICASE | REG_NEWLINE
                                        | REG_NOSUB);

                /*
                 * make sure we are not ignoring this type of errors before
                 * we continue 
                 */
                if (!ignore_error)
                {
                        if (regexec (&ignore_preg, str, 0, NULL, 0) == 0)
                        {
                                /*
                                 * we ignore this and return FALSE 
                                 */
                                regfree (&ignore_preg);
                                goto end;
                        }
                        regfree (&ignore_preg);
                }
                else
                {
                        DEBUG_PRINT
                                ("There was an error compiling the ignore_pattern regex: %s",
                                 ignore_pattern);
                }
        }

        if (regcomp (&preg, perr_pattern,
                     REG_EXTENDED | REG_ICASE | REG_NEWLINE | REG_NOSUB) == 0)
        {
                if (regexec (&preg, str, 0, NULL, 0) == 0)
                        ret = TRUE;
                regfree (&preg);
        }
        else
        {
                DEBUG_PRINT
                        ("There was an error compiling the perr_pattern regex: %s",
                         perr_pattern);
        }
      end:
        return ret;
}

/**
 * @desc reads file for pattern for known failure text messages
 * @param perr_pattern a valid regular expression pattern. @see man regex
 * @param filename a valid filename. Can be a special filename like "/dev/vcs1"
 * @return true if pattern was found
 */
inline gboolean
search_pattern (const gchar * perr_pattern, const gchar * filename,
                gshort use_ignore)
{
        FILE *fp;
        gchar *buffer = NULL;
        gint max_buf = 6200;    /* vcs is 6144 chars long. no new-lines are
                                 * added. man vcs */
        gboolean ret = FALSE;   /* assumes we will fail */

        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
        {
                ERROR_PRINT ("Cannot test file %s\n", filename);
                return FALSE;
        }

        buffer = g_try_malloc (max_buf);
        if (buffer && (fp = g_fopen (filename, "r")))
        {
                fread_unlocked (buffer, max_buf, 1, fp);        /* *_unlocked 
                                                                 * are a GNU
                                                                 * extension: 
                                                                 * makes
                                                                 * function
                                                                 * fread()
                                                                 * thread
                                                                 * safe (to use, but with care) */
                fclose (fp);
                buffer[max_buf - 1] = '\0';
                ret = search_pattern (perr_pattern, buffer, use_ignore);
        }
        if (buffer)
                g_free (buffer);
        return ret;
}

/*
 * int open_nb(char *buf) { int fd, n;
 * 
 * if ((fd = open(buf, O_WRONLY|O_NONBLOCK|O_NOCTTY)) < 0) return -1; n =
 * fcntl(fd, F_GETFL); n &= ~(O_NONBLOCK); fcntl(fd, F_SETFL, n);
 * 
 * return fd; }
 */

/*
 * cmd_functions 
 */
gint
cmd_allow_chvt (void **args)
{
        splashy_allow_vt_switching ();
        return 0;
}

gint
cmd_exit (void **args)
{
        /*
         * TODO: getting splashy to switch consoles (tty's) automatically
         * after finishing booting turned out to be a bad idea. I'm living
         * the code here commented out just in case we find a solution for
         * this issue later. For now, we wait until we exit, and then send a
         * chvt from splashy99 when splashy has already exited. Users should
         * customize the tty to which splashy will be switched from
         * /etc/default/splashy (on debian). Nonetheless, we let
         * splashy_allow_vt_switching() here just don't set vt-switching
         * directfb option there as this breaks splashy. - Luis Mondesi
         * <lemsx1@gmail.com> 
         */
        if (exiting == TRUE)
                return -1;      /* do not allow this function to be called
                                 * more than once */
        exiting = TRUE;
        DEBUG_PRINT ("exiting set to TRUE");

        /*
         * TODO
         * splashy_allow_vt_switching ();
         */
        read_console = FALSE;   /* stop updating textbox area */


        DEBUG_PRINT ("removing splashy temp files and pid");

        const gchar *_pid_file = splashy_get_config_string (SPL_PID_FILE);
        if (g_file_test (_pid_file, G_FILE_TEST_EXISTS))
        {
                unlink (_pid_file);
        }

        DEBUG_PRINT ("releasing memory");

        splashy_stop_splash ();

        exit (0);
        return 0;               /* we never reach this */
}

gint
cmd_progress (void **args)
{
        if (*(gint *) args[0] < 0 || *(gint *) args[0] > PROGRESS_MAX)
                return -1;

        arg_progress = *(gint *) args[0];

        if (arg_progress <= 0)
                return 1;
        /*
         * allow time for display to be drawn 
         */

        if (last_progress != arg_progress)
        {
                DEBUG_PRINT ("cmd_progress: Setting progressbar to %d ticks",
                             arg_progress);
                splashy_update_progressbar (arg_progress);
        }

        last_progress = arg_progress;

        sched_yield ();

        return 0;
}

gint
cmd_print (void **args)
{
        splashy_printline (args[0]);
        return 0;
}

gint
cmd_print_scroll (void **args)
{
        splashy_printline_s (args[0]);
        return 0;
}

gint
cmd_print_clear (void **args)
{
        splashy_printline ("");
        return 0;
}

gint
cmd_chvt (void **args)
{
        /*
         * vt[1-7] 
         */
        if (*(gint *) args[0] < 1 || *(gint *) args[0] > 7)
                return -1;
        splashy_allow_vt_switching ();  /* tell libdirectfb that's ok to *
                                         * allow vt switching */
        splashy_chvt (*(gint *) args[0]);
        return 0;
}

gint
cmd_set_timeout (void **args)
{
        if (*(gint *) args[0] < 1)
                return -1;

        timeout = *(gint *) args[0];
        sched_yield ();

        return 0;
}

gint
cmd_repaint (void **args)
{
        /*
         * TODO change background to current image 
         */
        /*
         * splashy_printline (""); 
         */
        return 0;
}

/*
 * This one is a bit different than the other ones
 * args[0] is prompt string comming from the user
 * args[1] amd args[2] are a *buf and buf_len resp.
 */
gint
cmd_getstring (void **args)
{
        /*
         * Get the key event loop give up its lock 
         */
        splashy_wake_up ();

        pthread_mutex_lock (&key_mut);
        splashy_get_string ((char *) args[1], *(int *) args[2],
                            (char *) args[0]);
        pthread_mutex_unlock (&key_mut);

        return 0;
}

gint
cmd_getpass (void **args)
{
        /*
         * Get the key event loop give up its lock 
         */
        splashy_wake_up ();

        pthread_mutex_lock (&key_mut);
        splashy_get_password ((char *) args[1], *(int *) args[2],
                              (char *) args[0]);
        pthread_mutex_unlock (&key_mut);

        return 0;
}

gint
cmd_chroot (void **args)
{
        char *dir = args[0];
        chdir (dir);
        chroot (dir);
        return 0;
}

/**
 * @desc our command-holding object and a list of supported commands
 *
 * TODO implement the rest of the commands
 */
struct
{
        const gchar *cmd;
          gint (*handler) (void **);
        gint args;
        gchar *specs;
} known_cmds[] =
{
        {
        .cmd = "timeout",.handler = cmd_set_timeout,.args = 1,.specs =
                        "d"},
        {
        .cmd = "repaint",.handler = cmd_repaint,.args = 0,.specs =
                        NULL,},
        {
        .cmd = "progress",.handler = cmd_progress,.args = 1,.specs =
                        "d"},
        {
        .cmd = "PROGRESS",.handler = cmd_progress,.args = 1,.specs =
                        "d"},
        {
        .cmd = "print",.handler = cmd_print,.args = 1,.specs = "s"},
        {
        .cmd = "TEXT",.handler = cmd_print,.args = 1,.specs = "s"},
        {
        .cmd = "scroll",.handler = cmd_print_scroll,.args = 1,.specs =
                        "s"},
        {
        .cmd = "SCROLL",.handler = cmd_print_scroll,.args = 1,.specs =
                        "s"},
        {
        .cmd = "CLEAR",.handler = cmd_print_clear,.args =
                        0,.specs = NULL},
        {
        .cmd = "clear",.handler = cmd_print_clear,.args =
                        0,.specs = NULL},
        {
        .cmd = "chvt",.handler = cmd_chvt,.args = 1,.specs = "d"},
        {
        .cmd = "allowchvt",.handler = cmd_allow_chvt,.args =
                        0,.specs = NULL},
        {
        .cmd = "exit",.handler = cmd_exit,.args = 0,.specs = NULL},
        {
        .cmd = "QUIT",.handler = cmd_exit,.args = 0,.specs = NULL},
        {
        .cmd = "getstring",.handler = cmd_getstring,.args = 1,.specs =
                        "s"},
        {
        .cmd = "getpass",.handler = cmd_getpass,.args = 1,.specs = "s"},
        {
        .cmd = "chroot",.handler = cmd_chroot,.args = 1,.specs = "s"}

};

/*
 * helper for signal handling 
 */
void
sig_exit_safe (gint sig)
{
        g_printerr ("Splashy caught signal number %d. Exiting...", sig);
        cmd_exit (NULL);
}

void
sig_hup_handler (gint sig)
{
        int i = 0;
        g_printerr
                ("Splashy caught HUP signal number %d. Closing open FD's...",
                 sig);
        /*
         * close all open filehandles
         */
        for (i = 0; i < sysconf (_SC_OPEN_MAX); i++)
        {
                if (i == 2)
                        continue;       /* never close stderr */
                DEBUG_PRINT ("sig_hup_handler: Filehandle closed %d", i);
                close (i);
        }
}

/**
 * @desc a simple function that waits for text to be written to a socket and dispatches commands accordingly
 * @param sock -- socket to read from
 * @return
 */
inline void
spl_read_socket (int sock)
{
        DEBUG_PRINT ("Reading socket %s...\n", SPL_SOCKET);
        int i, j, k;

        /*
         * FIXME make buf_len=1024 a define 
         */
        int fd, buf_len = 1024;

        struct ucred cred;
        struct sockaddr cli_addr;
        socklen_t cl = sizeof (cred), cli_len = sizeof (cli_addr);
        char buf[buf_len], r_buf[buf_len];

        if ((fd = accept (sock, &cli_addr, &cli_len)) < 0)
        {
                ERROR_PRINT ("Error in accept: %s", strerror (errno));
                return;
        }

        if (getsockopt (fd, SOL_SOCKET, SO_PEERCRED, &cred, &cl) < 0)
        {
                ERROR_PRINT ("Couldn't get credentials");
                goto out;
        }

        DEBUG_PRINT ("From: pid %d, uid %d, gid %d",
                     cred.pid, cred.uid, cred.gid);

        if (cred.uid != 0)
                goto out;

        if (read (fd, &buf, buf_len) <= 0)
        {
                ERROR_PRINT ("Couldn't read from socket");
                goto out;
        }

        buf[buf_len - 1] = '\0';

        DEBUG_PRINT ("Message payload: %s", buf);


        /*
         * Read Socket and call functor to handle exceptions 
         */

        if (strlen (buf) > 0)
        {
                gchar *t;
                int args_i[4];
                void *args[4];

                /*
                 * buf[1023] = 0; 
                 */
                /*
                 * FIXME buf[g_utf8_strlen(buf,1024)-1] = 0; 
                 */
                /*
                 * buf[strlen (buf) - 1] = 0; 
                 */

                for (i = 0; i < sizeof (known_cmds) / sizeof (known_cmds[0]);
                     i++)
                {
                        k = strlen (known_cmds[i].cmd);
                        /*
                         * FIXME k = g_utf8_strlen(known_cmds[i].cmd,1024); 
                         */

                        /*
                         * FIXME if (g_ascii_strncasecmp(buf,
                         * known_cmds[i].cmd, k)) 
                         */
                        if (strncmp (buf, known_cmds[i].cmd, k))
                                continue;

                        for (j = 0; j < known_cmds[i].args; j++)
                        {

                                for (; buf[k] == ' '; buf[k] = 0, k++);
                                if (!buf[k])
                                        goto out;

                                switch (known_cmds[i].specs[j])
                                {

                                case 's':
                                        /*
                                         * [Tim] is it me, or will this only
                                         * work if 's' is the last arg in the 
                                         * specification? 
                                         */
                                        args[j] = &(buf[k]);
                                        for (; buf[k] != ' '; k++);
                                        break;

                                case 'd':
                                        /*
                                         * FIXME args_i[j] =
                                         * g_ascii_strtod(&(buf[k]), &t); 
                                         */
                                        args_i[j] =
                                                strtol (&(buf[k]), &t, 10);
                                        if (t == &(buf[k]))
                                                goto out;

                                        args[j] = &(args_i[j]);
                                        k = t - buf;
                                        break;
                                }
                        }
                        /*
                         * We add 'r_buf' to the args in case cmd_ wants to
                         * return anything. update_splashy drops the
                         * connection in case it doesn't expect an answer
                         * back. 
                         */
                        r_buf[0] = '\0';
                        args[j] = r_buf;
                        args[j + 1] = &buf_len;
                        /**                        
                         * cmds should be used for everything
                         * @see known_cmds
                         * and handlers
                         */
                        known_cmds[i].handler (args);

                        if ((strlen (r_buf) > 0)
                            && (write (fd, &r_buf, strlen (r_buf) + 1) < 0))
                                DEBUG_PRINT ("Writing to socket failed");
                }
        }
      out:
        close (fd);
        return;
}


inline void
_switch_to_verbose_image ()
{
        const gchar *background =
                splashy_image_path ("/splashy/background/errorimg");

        if (!g_file_test (background, G_FILE_TEST_IS_REGULAR))
        {
                ERROR_PRINT ("Can't find %s ", background);
                return;
        }

        splashy_change_splash (background);
}

/**
 * When autoverboseonerror is set we show the scrolling 
 * text from the consoles /dev/vcs*
 * @param data - not used
 * @return
 *
 * TODO
 * - make use of real console
 * - this is a slow function
 */
inline void *
verbose_text_loop (void *data)
{
        FILE *dev_vcs;
        char buf[81];
        GString *buf_str;       /* what to display */
        GString *device;        /* /dev/vcs to read */
        gshort i, j;
        const gchar *error_pattern;

        gboolean found_error;
        gboolean autoverbose;
        struct timespec _sleep;

        dev_vcs = NULL;
        switched_to_verbose = FALSE;    /* verbose image flag */
        splashy_set_textbox_area_visible (FALSE);

        autoverbose = FALSE;    /* assume we don't need verbose mode for now */
        read_console = TRUE;    /* assume we will be able to read from a
                                 * console */
        found_error = FALSE;    /* we have not found any errors yet */
        error_pattern = "(FATAL|fail|error|abort|===|recovering journal)";

        _sleep.tv_sec = 1;
        _sleep.tv_nsec = 0;

        device = g_string_new ("");

        /*
         * get autoverboseonerror from config file and set autoverbose
         * accordingly 
         */
        if (g_ascii_strncasecmp
            (splashy_get_config_string (SPL_AUTO_VERBOSE_ON_ERROR), "yes",
             3) == 0)
                autoverbose = TRUE;

        /*
         * set the cancellation parameters -- - Enable thread cancellation - 
         * Defer the action of the cancellation 
         */
        pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
        pthread_setcanceltype (PTHREAD_CANCEL_DEFERRED, NULL);

        while (1)
        {
                if (exiting || read_console == FALSE)
                {
                        DEBUG_PRINT ("Not reading console at all...");
                        /*
                         * this thread shouldn't be holding a lock on this
                         * but... 
                         */
                        sched_yield ();
                        nanosleep (&_sleep, NULL);
                        continue;
                }
                /*
                 * read the realcons console and look for error patterns 
                 * on error, set our background image accordingly and display
                 * text printing 1 line (80 char) at a time
                 */
                /*
                 * Ubuntu uses /dev/vcs2, every other distro /dev/vcs1 
                 */
                for (i = 1; i <= 7; i++)
                {
                        g_string_printf (device, "/dev/vcs%d", i);
                        if (!g_file_test (device->str, G_FILE_TEST_EXISTS))
                        {
                                sched_yield ();
                                continue;
                        }
                        dev_vcs = g_fopen (device->str, "r");
                        if (!dev_vcs)
                        {
                                DEBUG_PRINT ("Can't open %s for reading",
                                             device->str);
                                sched_yield ();
                                /*
                                 * short wait 
                                 */
                                _sleep.tv_sec = 0;
                                /*
                                 * 1/2 second 
                                 */
                                _sleep.tv_nsec = 459999999;
                                nanosleep (&_sleep, NULL);
                                continue;
                        }
                        while (fgets_unlocked (buf, 81, dev_vcs))
                        {
                                if (strlen (buf) < 8)
                                        continue;
                                /*
                                 * we need to find where the spaces end 
                                 */
                                for (j = 0; j < strlen (buf); j++)
                                {
                                        if (buf[j] != ' ')
                                        {
                                                break;
                                        }
                                }

                                /*
                                 * copy only the characters now (no leading spaces)
                                 */

                                buf_str = g_string_new ("");
                                for (; j < strlen (buf); j++)
                                {
                                        g_string_append_c (buf_str, buf[j]);
                                }

                                /*
                                 * we don't need to print blank lines or new-lines
                                 */
                                /*
                                 * if (strlen(buf) < 8)
                                 */
                                if (search_pattern_str
                                    ("[^a-zA-Z0-9]", buf_str->str,
                                     0) == FALSE)
                                {

                                        g_string_free (buf_str, TRUE);
                                        continue;
                                }

                                /*
                                 * look for error messages only if we weren't asked
                                 * to display text right the way
                                 */
                                if (splashy_get_textbox_area_visible () !=
                                    TRUE)
                                {
                                        /*
                                         * we only look for errors if autoverbose
                                         * is set to TRUE
                                         * once an error is found, we keep printing
                                         * the text to the textbox area without
                                         * matching any more errors
                                         */

                                        if (found_error == FALSE)
                                        {
                                                if (autoverbose == TRUE)
                                                {
                                                        found_error =
                                                                search_pattern_str
                                                                (error_pattern,
                                                                 buf, 1);
                                                }

                                                /*
                                                 * when the user presses F2
                                                 * we want to show the buffer 
                                                 * in the textbox anyway.
                                                 * regardless of whether an
                                                 * error was found or not 
                                                 */
                                                if (F2_toggle_pressed != TRUE)
                                                {
                                                        g_string_free
                                                                (buf_str,
                                                                 TRUE);
                                                        continue;
                                                }
                                        }
                                        else
                                        {
                                                /*
                                                 * error was found, we need to display the error image
                                                 * and allow the textbox area to be shown
                                                 */
                                                found_error = TRUE;
                                                splashy_set_textbox_area_visible
                                                        (TRUE);

                                                if (autoverbose == TRUE
                                                    && switched_to_verbose ==
                                                    FALSE)
                                                {
                                                        _switch_to_verbose_image
                                                                ();
                                                        switched_to_verbose =
                                                                TRUE;
                                                        /*
                                                         * fake F2 pressed so that users can press
                                                         * this key to turn off the textbox area
                                                         */
                                                        F2_toggle_pressed =
                                                                TRUE;
                                                }
                                        }
                                }

                                /*
                                 * pressing F2 will cause text to be printed to the textbox
                                 * as well as having autoverbose set to ON in the config.xml
                                 */
                                if (autoverbose == TRUE
                                    || F2_toggle_pressed == TRUE)
                                {
                                        if (!exiting)
                                                splashy_printline_s ((char *)
                                                                     buf_str->
                                                                     str);

                                        sched_yield ();

                                        for (j = 0; j < 81; j++)
                                        {
                                                buf[j] = '\0';  /* make it
                                                                 * all NULs */
                                        }

                                        /*
                                         * wait for 1/6 seconds
                                         */
                                        _sleep.tv_sec = 0;
                                        _sleep.tv_nsec = 114999999;
                                        nanosleep (&_sleep, NULL);
                                }
                                g_string_free (buf_str, TRUE);
                        }
                        fclose (dev_vcs);
                }
        }
        g_string_free (device, TRUE);   /* never reached */
        pthread_exit (NULL);    /* never reached */
}
inline void *
socket_loop (void *data)
{
        gboolean _preview = *(gboolean *) data;
        int err, i = 0;

        struct timeval tv;
        struct timespec _sleep;
        struct pollfd pfd;

        int sock;
        struct sockaddr sock_addr = { AF_UNIX, SPL_SOCKET };

        gboolean forward_flag = TRUE;   /* this is only used when testing. we 
                                         * always start forward when testing */

        /*
         * FIXME splashy_set_progressbar_forward(forward_flag);
         */

        /*
         * It's important that the progress bar code runs as a child process
         * because this doesn't need to grab events from the keyboard. All
         * communication to this is done through a socket.
         */

        /*
         * Open the socket 
         */
        if ((sock = socket (PF_UNIX, SOCK_STREAM, 0)) < 0)
        {
                ERROR_PRINT ("%s", strerror (errno));
                pthread_exit (NULL);
        }

        if (bind (sock, &sock_addr, sizeof (sock_addr)) < 0)
        {
                ERROR_PRINT ("%s", strerror (errno));
                pthread_exit (NULL);
        }

        if (listen (sock, 100) < 0)
        {
                ERROR_PRINT ("%s", strerror (errno));
                pthread_exit (NULL);
        }

        /*
         * I don't want to die if the client drops the connection 
         */
        signal (SIGPIPE, SIG_IGN);

        /*
         * 0 seconds and 10 nano seconds: 10 x 1x10^-9 
         */
        _sleep.tv_sec = 0;
        _sleep.tv_nsec = 10;

        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        pfd.fd = sock;
        pfd.events = POLLIN;

        /*
         * set the cancellation parameters -- - Enable thread cancellation - 
         * Defer the action of the cancellation 
         */
        pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
        pthread_setcanceltype (PTHREAD_CANCEL_DEFERRED, NULL);

        /*
         * run our progress bar in a tight loop 
         */
        while (1)
        {
                if (exiting)
                {
                        sched_yield ();
                        nanosleep (&_sleep, NULL);
                        continue;
                }
                /*
                 * if we are in test mode, update progressbar continuously 
                 */
                if (_preview == TRUE)
                {
                        _sleep.tv_nsec = 1000;
                        /*
                         * since we are testing, allow progressbar to loop 
                         */
                        if (i >= 100)
                        {
                                i = 0;
                                forward_flag = (forward_flag) ? FALSE : TRUE;
                                /*
                                 * if (forward_flag)
                                 * g_printerr("Forward...\n"); else
                                 * g_printerr("Backward...\n");
                                 */

                                splashy_reset_progressbar_counters ();
                                splashy_set_progressbar_forward
                                        (forward_flag);
                                /*
                                 * re-draw the image so that we can get our
                                 * progressbar filled 
                                 */
                                splashy_reset_splash ();
                                sched_yield ();
                                nanosleep (&_sleep, NULL);
                        }
                        DEBUG_PRINT ("Updating progressbar by %d+10", i);
                        splashy_update_progressbar (i += 10);
                        sched_yield ();
                        nanosleep (&_sleep, NULL);
                }
                else
                {
                        /*
                         * if no special keys were pressed, read from our
                         * socket and continue waiting for events 
                         */
                        if ((err = poll (&pfd, 1, timeout * 1000)) == -1)
                        {
                                ERROR_PRINT
                                        ("Error while selecting poll event: %s",
                                         strerror (errno));
                                splashy_child_exit ();
                        }
                        else if (err == 0)
                        {
                                /*
                                 * Timeout
                                 */
                                ERROR_PRINT
                                        ("Timeout (%d sec) occurred while waiting for a message on the Splashy socket",
                                         timeout);
                                splashy_child_exit ();
                        }

                        spl_read_socket (sock);

                }
                pthread_testcancel ();
                sched_yield ();
                nanosleep (&_sleep, NULL);
        }                       /* ends while(1) */
        close (sock);           /* FIXME: Do we ever get here? */
        pthread_exit (NULL);    /* never reached */
}
inline void *
keyevent_loop (void *data)
{
        /*
         * this function needs to always handle keyevents and not yield cpu too often
         */

        /*
         * counter to know when to yield CPU cycles * to other threads 
         */
        guint i_cpu = 0;

        /*
         * setup an event interface 
         */
        int key;

        /*
         * set the cancellation parameters -- - Enable thread cancellation - 
         * Defer the action of the cancellation 
         */
        pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
        pthread_setcanceltype (PTHREAD_CANCEL_DEFERRED, NULL);

        while (1)
        {
                if (exiting)
                {
                        sched_yield ();
                        continue;
                }
                /*
                 * sub-parent (we are a fork after all). init is our parent 
                 */
                pthread_mutex_lock (&key_mut);
                splashy_wait_for_event ();
                /*
                 * get all events from our event buffer (fifo queue) and
                 * process them. 
                 */
                while ((key = splashy_get_key_event ()) > 0)
                {
                        /*
                         * exit if ESC or F2 is pressed also kill our
                         * sub_child while at it 
                         */
                        if (key == DIKS_ESCAPE)
                        {
                                splashy_child_exit ();
                        }
                        else if (key == DIKS_F2)
                        {
                                /*
                                 * when F2 is pressed we display a
                                 * small theme-defined surface where 
                                 * text scrolls showing the currect
                                 * /dev/vcs[1,2,3,4,5,6,7] text 
                                 * @see verbose_text_loop
                                 */
                                if (F2_toggle_pressed != TRUE)
                                {
                                        F2_toggle_pressed = TRUE;
                                        splashy_set_textbox_area_visible
                                                (TRUE);
                                }
                                else
                                {
                                        F2_toggle_pressed = FALSE;
                                        splashy_set_textbox_area_visible
                                                (FALSE);
                                        /*
                                         * pressing F2 also clears the 
                                         * switched_to_verbose flag
                                         * @see verbose_text_loop
                                         * we need to re-draw the screen
                                         */
                                        splashy_reset_splash ();
                                }
                        }
                }               /* ends splashy_get_key_event */
                pthread_mutex_unlock (&key_mut);

                /*
                 * every 100 tries check to see if the thread has been
                 * cancelled if the thread has not been cancelled then yield 
                 * the thread's LWP to another thread that may be able to run 
                 */
                if (i_cpu % 100 == 0)
                {
                        pthread_testcancel ();
                        sched_yield ();
                }
                i_cpu++;
        }                       /* end while(1) */
        pthread_exit (NULL);    /* never reached */
}

/**
 * @desc _splashy_child handles both start and stop routines for splashy during boot and shutdown respectively
 * Note that the fork() happens before this function is called by its
 * wrappers: splashy_child_start() and splashy_child_stop()
 * @see splashy_child_start() and @see splashy_child_stop()
 * @param seq sequence command to execute: "boot" or "shutdown"
 * @return
 */
gint
_splashy_child (const gchar * seq)
{
        gint i;                 /* general purpose counter */
        gint thr_id_c, thr_id_d;        /* thread ID for the newly * created
                                         * thread: c - keyboard events d -
                                         * SplashyClient handler */
        pthread_t p_thread_c, p_thread_d;       /* we need thread C and D to
                                                 * manage our keyboard events 
                                                 * * * and SplashyClient */

        /*
         * gint thr_id_f; * Handles /dev/vcs1 text to be displayed to *
         * splashy overlay textbox ... when pressing * F2 we launch a thread
         * to handle the * console text 
         */
        /*
         * pthread_t p_thread_f;
         */

        gboolean _preview = FALSE;

        int ret;


        /*
         * handle exit signals
         * man 7 signal
         */
        /*
         * (void) signal (SIGUSR1, sig_progress_update); (void) signal
         * (SIGUSR2, sig_progress_update);
         */
        (void) signal (SIGHUP, sig_hup_handler);
        (void) signal (SIGTERM, SIG_IGN);
        (void) signal (SIGINT, sig_exit_safe);
        (void) signal (SIGQUIT, sig_exit_safe);
        /*
         * TODO handling SIGABRT causes
         * "libgcc_s.so.1 must be installed for pthread_cancel to work" ?
         */
        (void) signal (SIGABRT, sig_exit_safe);

        if (strncmp ("preview", seq, 7) == 0)
        {
                _preview = TRUE;
                if ((ret = splashy_init (SPL_CONFIG_FILE, "boot")) < 0)
                {
                        ERROR_PRINT ("Problems boot. Error %d", ret);
                        return 1;
                }

        }
        else
        {
                if ((ret = splashy_init (SPL_CONFIG_FILE, seq)) < 0)
                {
                        ERROR_PRINT ("Problems %s, Error %d", seq, ret);
                        return 1;
                }
        }



        /*
         * close all open filehandles
         */
        for (i = 0; i < sysconf (_SC_OPEN_MAX); i++)
        {
                if (i == 2)
                        continue;       /* never close stderr */
                // DEBUG_PRINT ("Filehandle closed %d", i);
                close (i);
        }

        if ((ret = splashy_start_splash ()) < 0)
        {
                ERROR_PRINT ("Couldn't splashy_start_splashy(). Error %d\n",
                             ret);
                return 1;
        }

        /*
         * thread C (main)
         * takes care of reading the messages from SplashyClient and updating 
         * splashy internals according to the commands read from there
         */
        thr_id_c =
                pthread_create (&p_thread_c, NULL, socket_loop,
                                (void *) &_preview);

        /*
         * thread D 
         * takes care of "listening" for keyboard events. For now:
         * ESC - exits all splashy threads
         * F2 - puts splashy in verbose mode
         */
        thr_id_d = pthread_create (&p_thread_d, NULL, keyevent_loop, NULL);

        /*
         * thr_id_f = pthread_create (&p_thread_f, NULL, verbose_text_loop,
         * NULL);
         */

        /*
         * Wait till thread is finished reading the socket... ie 'forever' 
         */
        pthread_join (p_thread_c, NULL);

        return 0;               /* never reached */
}                               /* end _splashy_child() */

/**
 * @desc the preferred way to exit splashy:
 */
void
splashy_child_exit ()
{
        cmd_exit (NULL);
}

/**
 * @desc what to do when splashy is starting up (boot process)
 * TODO
 * splashy child start/stop functions should allow argumenst from main.c
 */
void
splashy_child_start ()
{
        DEBUG_PRINT ("Entering %s", __FUNCTION__);
        splashy_set_progressbar_forward (TRUE);

        gint ret = _splashy_child ("boot");
        if (ret < 0)
                ERROR_PRINT ("Could not start Splashy. %d\n", ret);
        DEBUG_PRINT ("Exiting %s", __FUNCTION__);
        splashy_child_exit ();
}

/**
 * @desc what to do when splashy is in the stopping process (shutdown)
 */
void
splashy_child_stop ()
{
        splashy_set_progressbar_forward (FALSE);

        gint ret = _splashy_child ("shutdown");
        if (ret < 0)
                ERROR_PRINT ("Main process exited with %d\n", ret);
        splashy_child_exit ();
}

/**
 * @desc testing splashy
 */
void
splashy_child_test ()
{
        DEBUG_PRINT ("Entering %s", __FUNCTION__);
        /*
         * allows preview of Splashy current theme and progressbar 
         */
        splashy_set_progressbar_forward (TRUE);

        gint ret = _splashy_child ("preview");
        if (ret < 0)
                ERROR_PRINT ("Could not start Splashy on test mode. %d\n",
                             ret);
        DEBUG_PRINT ("Exiting %s", __FUNCTION__);
        splashy_child_exit ();
}

/*
 * TODO we might need to mount /proc 
 */
/*
 * #ifdef __linux__ / * Read /proc/cmdline. / stat("/", &st); if
 * (stat("/proc", &st2) < 0) { perror("Splashy: /proc"); return -1; } if
 * (st.st_dev == st2.st_dev) { if (mount("proc", "/proc", "proc", 0, NULL) <
 * 0) { perror("Splashy: mount /proc"); return -1; } didmount = 1; }
 * 
 * n = 0; r = -1; if ((fd = open("/proc/cmdline", O_RDONLY)) < 0) {
 * perror("Splashy: /proc/cmdline"); } else { buf[0] = 0; if ((n = read(fd,
 * buf, sizeof(buf) - 1)) >= 0) r = 0; else perror("Splashy: /proc/cmdline");
 * close(fd); } if (didmount) umount("/proc");
 * 
 * if (r < 0) return r;
 * 
 * #endif 
 */

/**
 * @desc allows switching virtual terminals
 */
