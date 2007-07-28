/************************************************************************
 *                              splashy_update.c                        *
 *                                                                      *
 *  2006-02-25 13:17 EST                                                *
 *  Copyright  2006     Andrew Williams <mistik1@geeksinthehood.net>    *
 *	       2006	Tim Dijkstra <newsuser@famdijkstra.org>		*
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

/*
 * Andrew Williams <mistik1@geeksinthehood.net>
 * This is a simple attempt to replace using echo 
 * write to the Splashy named pipe.
 * Tim Dijkstra
 * I changed it entirely to using anonymous unix-sockets.
 */

#include <string.h> /* strerror */
#include <unistd.h>
#include <stdlib.h>
#include <errno.h> /* errno */

#include <sys/socket.h>
#include <sys/un.h>

#include "common_macros.h"

int
main (int argc, char **argv)
{
        int len, sock;
        struct sockaddr sock_addr = {AF_UNIX, SPL_SOCKET };
        int buf_len=1024;
        char buf[buf_len];

        if (argc != 2)
        {
                ERROR_PRINT ("%s","Invalid number of arguments");
                ERROR_PRINT ("%s","This program accepts a single argument");
                exit (3);
        }
        if ( (len = strlen (argv[1]) + 1) > buf_len ) 
        {
                ERROR_PRINT("Command string to big. Max size is %d",buf_len);
                return 3;
        }
        strncpy(buf, argv[1], buf_len);

        if ( (sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) 
        {
                ERROR_PRINT("%s",strerror(errno));
                return 1;
        }

        if (connect(sock, &sock_addr, sizeof(sock_addr)) < 0) 
        {
                ERROR_PRINT("%s",strerror(errno));
                return 1;
        }

        DEBUG_PRINT("Sending %s", buf);

        if (write(sock, buf, strlen(buf)+1) < 0) 
        {
                ERROR_PRINT("%s",strerror(errno));
                return 2;
        }

        /* If we're asking for a string we'll wait for an answer */
        if (strncmp("get",buf,3) == 0) 
        {
                if (read(sock, &buf, buf_len) < 0)
                        ERROR_PRINT("%s",strerror(errno));

                printf(buf);
        }

        close (sock);

        return 0;
}
