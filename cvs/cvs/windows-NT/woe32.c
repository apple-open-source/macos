/*
 * Copyright (C) 2003-2005 The Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * woe32.c
 * - utility functions for cvs under win32
 *
 */

#include "config.h"

#include "woe32.h"

#include <stdio.h>
#include <assert.h>
#include <errno.h>

#include <sys/socket.h>  /* This does: #include <windows.h> */

#include <stdlib.h>
#include <unistd.h>
#include <xalloc.h>



/* #define SYSTEM_CLEANUP woe32_cleanup */
void
woe32_cleanup (void)
{
    if (WSACleanup ())
    {
#ifdef SERVER_ACTIVE
	if (server_active || error_use_protocol)
	    /* FIXME: how are we supposed to report errors?  As of now
	       (Sep 98), error() can in turn call us (if it is out of
	       memory) and in general is built on top of lots of
	       stuff.  */
	    ;
	else
#endif
	    fprintf (stderr, "cvs: cannot WSACleanup: %s\n",
		     sock_strerror (WSAGetLastError ()));
    }
}



/*============================================================================*/
/*
How Microsoft does fd_set in Windows 2000 and Visual C++ 6.0:

	* Select uses arrays of SOCKETs.  These macros manipulate such
	* arrays.  FD_SETSIZE may be defined by the user before including
	* this file, but the default here should be >= 64.
	*
	* CAVEAT IMPLEMENTOR and USER: THESE MACROS AND TYPES MUST BE
	* INCLUDED IN WINSOCK2.H EXACTLY AS SHOWN HERE.

	#ifndef FD_SETSIZE
	#define FD_SETSIZE      64
	#endif

	typedef struct fd_set {
		u_int   fd_count;
		SOCKET  fd_array[FD_SETSIZE];
	} fd_set;

Microsoft packs all handles between fd_array[0] and fd_array[fd_count-1]
*/

typedef struct
{
	int     is_ready;
	SOCKET  crt;
	DWORD   type;
	union
	{
		long    osf;
		HANDLE  w32;
	};
} woe32_handle_set;

typedef struct
{
	u_int ready_count, used_count;
	woe32_handle_set handle[ FD_SETSIZE ];
} woe32_select_set;

static int woe32_select_set_fini (woe32_select_set * w32_set, fd_set * crt_set)
{
	FD_ZERO (crt_set);

	if (w32_set->ready_count) 
	{
		u_int index;
		woe32_handle_set * handle;

		index = 0;
		handle = w32_set->handle;
		while (index < w32_set->used_count)
		{
			if (handle->is_ready)
			{
				FD_SET (handle->crt, crt_set);
			}

			++index;
			++handle;
		}
	}

	return w32_set->ready_count;
}

static int woe32_select_set_init (woe32_select_set * w32_set, fd_set * crt_set)
{
	u_int index;
	DWORD dwBytesAvail;
	woe32_handle_set * handle;
		
	w32_set->ready_count = w32_set->used_count = index = 0;
	handle = w32_set->handle;

	if (crt_set) while (index < crt_set->fd_count)
	{
		handle->crt = crt_set->fd_array[index];

		handle->osf = _get_osfhandle (handle->crt);
		if (handle->w32 == INVALID_HANDLE_VALUE)
		{
			errno = EBADF;
			return -1;
		}

		handle->type = GetFileType (handle->w32);
		switch (handle->type)
		{
		case FILE_TYPE_DISK:
			w32_set->ready_count += handle->is_ready = 1;
			break;

		case FILE_TYPE_PIPE:
			if ( PeekNamedPipe (handle->w32, NULL, 0, NULL, &dwBytesAvail, NULL) )
			{
				w32_set->ready_count += handle->is_ready = dwBytesAvail > 0;
			}
			else
			{
				errno = EBADF;
				return -1;
			}
			break;

		case FILE_TYPE_CHAR:
		case FILE_TYPE_REMOTE:
		case FILE_TYPE_UNKNOWN:
		default:
			errno = EBADF;
			return -1;
		}

		++index;
		++handle;
	}
	w32_set->used_count = index;

	while (index < FD_SETSIZE)
	{
		handle->crt = -1;

		handle->w32 = INVALID_HANDLE_VALUE;

		handle->type = FILE_TYPE_UNKNOWN;

		handle->is_ready = 0;

		++index;
		++handle;
	}

	return w32_set->ready_count;
}

static int woe32_select_set_wait (woe32_select_set * w32_set)
{
	char buffer[ 8 ];
	DWORD dwBytesRead;

	/* set contains only non-ready pipes */
	if (w32_set->used_count != 1)
	{
		errno = EINVAL;
		return -1;
	}

	if (! ReadFile (w32_set->handle[0].w32, buffer, 0, &dwBytesRead, NULL))
	{
		errno = EBADF;
		return -1;
	}

	return w32_set->handle[0].is_ready = 1;
}

/* #define fd_select woe32_fd_select */
#undef fd_select
int woe32_fd_select (	int nfds,
						struct fd_set * readfds,
						struct fd_set * writefds,
						struct fd_set * errorfds,
						struct timeval * timeout)
{
	int ready_fds;
	woe32_select_set woe32_rset;

	/* we don't support these for now */
	assert(writefds != NULL);
	assert(errorfds != NULL);
	assert(timeout != NULL);

	/* Windows doesn't care but POSIX says it does */
	if (nfds < 0 || nfds > FD_SETSIZE)
	{
		errno = EINVAL;
		return -1;
	}

	ready_fds = woe32_select_set_init (&woe32_rset, readfds);
	if (! ready_fds)
	{
		ready_fds = woe32_select_set_wait (&woe32_rset);
	}

	if (ready_fds >= 0)
	{
		woe32_select_set_fini (&woe32_rset, readfds);
	}

	return ready_fds;
}
/*============================================================================*/



char *
woe32_getlogin (void)
{
    static char name[256];
    DWORD dw = sizeof (name);
    GetUserName (name, &dw);
    if (name[0] == '\0')
	return NULL;
    else
	return name;
}



/* #define SYSTEM_INITIALIZE(pargc,pargv) woe32_init_winsock() */
void
woe32_init_winsock (void)
{
    WSADATA data;

    if (WSAStartup (MAKEWORD (1, 1), &data))
    {
	fprintf (stderr, "cvs: unable to initialize winsock\n");
	exit (1);
    }
}



char *
woe32_home_dir (void)
{
    static char *home_dir = NULL;
    char *home_drive, *home_path;

    if (home_dir)
	return home_dir;
    
    if ((home_drive = getenv ("HOMEDRIVE")) && (home_path = getenv ("HOMEPATH")))
    {
	const char NUL = '\0';
	size_t home_drive_len, home_path_len;

	home_drive_len = strlen (home_drive);
	home_path_len  = strlen (home_path);

	home_dir = xmalloc (home_drive_len + home_path_len + sizeof NUL);

	memcpy (home_dir,                  home_drive, home_drive_len );
	memcpy (home_dir + home_drive_len, home_path,  home_path_len  );
	home_dir[ home_drive_len + home_path_len ] = NUL;

	return home_dir;
    }

    return NULL;
}



/* #define nanosleep woe32_nanosleep */
int
woe32_nanosleep (const struct timespec *requested_delay,
                       struct timespec *remaining_delay)
{
    const useconds_t one_second = 1000000;
    const useconds_t nano_per_micro = 1000;
    useconds_t micro_delay;

    micro_delay = requested_delay->tv_sec * one_second
		+ ( requested_delay->tv_nsec + nano_per_micro - 1 ) / nano_per_micro
		;

    return usleep (micro_delay);
}



char *
woe32_shell (void)
{
    char *shell;

    shell = getenv ("ComSpec");

    if (shell == NULL)
    {
	/* Windows always sets ComSpec, the user is messing with us */
	const char *os;

	if ((os = getenv ("OS")) && strcmp (os, "Windows_NT"))
	    /* Windows NT, Windows 2000, Windows XP, Windows 2003 */
	    shell = "cmd.exe";
	else
	    /* Windows 95, Windows 98, Windows Me */
	    shell = "command.com";
    }

    return shell;
}
