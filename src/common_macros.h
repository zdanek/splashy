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
#ifndef __COMMON_MACROS_H
#define __COMMON_MACROS_H

#include <stdio.h>

#define ERROR_PRINT(format_string, args...) \
        fprintf(stderr, "Splashy ERROR: " format_string "\n", ## args)

#ifdef DEBUG
#define DEBUG_PRINT(str, args...) \
  fprintf(stderr, "Splashy DEBUG %s (%d): " str "\n" , __FILE__, __LINE__, ## args)
#else
#define DEBUG_PRINT(...) while(0){}   /* NULL */
#endif

#define DFBCHECK(x...)                                                          \
{                                                                               \
        DFBResult err = x;                                                      \
                                                                                \
        if (err != DFB_OK)                                                      \
        {                                                                       \
                fprintf( stderr, "FATAL: %s <%d>:\n\t", __FILE__, __LINE__ );   \
                DirectFBErrorFatal( #x, err );                                  \
        }                                                                       \
}

#define SPL_SOCKET                      "\0/splashy"

#endif /* __COMMON_MACROS_H */
