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
#ifndef _splashy_functions_H
#define _splashy_functions_H

#include <glib.h>
#include <directfb.h>

#define PROGRESS_MAX 	                100
#define TTY_SILENT 	                8
#define TTY_VERBOSE 	                1

void splashy_child_exit ();
void splashy_child_start ();    /* thread handler */
void splashy_child_stop ();     /* thread handler */
void splashy_child_test ();     /* same as *_child_start() but for testing
                                 * purposes */
void splashy_chvt (gint vt_no);

#endif /* _splashy_H */
