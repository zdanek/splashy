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
#ifndef _LIBSPLASHY_H
#define _LIBSPLASHY_H

/*
 * public functions 
 */

/* Always init the library first, If file==NULL we will take the 
 * default config file. Mode is one of boot, shutdown, resume, suspend*/
int splashy_init(const char * file,const char * mode);
/* Then init splash, from here we init the fb */
int splashy_start_splash ();

/* Redraw background image and progress bar. ... with a new image */
void splashy_reset_splash ();
void splashy_change_splash (const char * newimage);
/* Free resources */
void splashy_stop_splash ();

/* Update the progress bar perc */
int splashy_update_progressbar (int perc);
/* Set direction and visibility of progress bar, reset progress.
 * Note that these functions do not draw anaything.
 * Call splashy_reset_splash to redraw with new values
 */
void splashy_set_progressbar_forward(int go_forward);
void splashy_set_progressbar_visible(int visible);
void splashy_reset_progressbar_counters();

/*
 * allows to print a single message to the center of the textbox area
 * reseting the old one
 */
void splashy_printline (const char * string);
/*
 * allows to print a message appended to the end of the textbox area
 * keeping the old one
 */
void splashy_printline_s (const char * string);
/* 
 * write data to our textbox the right way. it uses splashy_printline_s() to do
 * the actual writing. @see verbose_text_loop() in splashy_functions.c
 */
void splashy_writelog (unsigned char *ptr, 
                int len, char *ringbuf, char *outptr, char *endptr);

/*
 * Some functions for user input.
 * You can block a thread by calling _wait_for_event.
 * Be carefull with splashy_wait_for_event from several threads, you should
 * use mutexes to protect them. You can use splashy_wake_up() to unblock.
 */
void splashy_wait_for_event ();
void splashy_wake_up ();
/* Then get_key_event will return -1 if it wasn't a
 * keyevent, else the unicoce symbol. 
 */
int splashy_get_key_event ();
/*
 * This will show an input box with 'prompt', will return nr of chars
 * Caution: internally it uses splashy_wait_for_event()
 */
int splashy_get_string(char * buf ,int len, const char *prompt);
int splashy_get_password(char * buf ,int len, const char *prompt);
/* Get and return char */
int splashy_getchar();

/* Tell splashy that it is OK to switch VTs, (Doesn't do anything yet) */
void splashy_allow_vt_switching ();

#endif
