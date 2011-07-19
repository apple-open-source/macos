/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/******************************************************************

	Title  : winscard_msg.c
	Package: PC/SC Lite
	Author : David Corcoran
	Date   : 04/19/01
	License: Copyright (C) 2001 David Corcoran
			<corcoran@linuxnet.com>
	Purpose: This is responsible for client/server transport.

$Id: winscard_msg.c 123 2010-03-27 10:50:42Z ludovic.rousseau@gmail.com $

********************************************************************/

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"

#ifdef PCSC_TARGET_SOLARIS
#include <sys/filio.h>
#endif

#include "wintypes.h"
#include "pcsclite.h"
#include "winscard.h"
#include "winscard_msg.h"
#include "sys_generic.h"
#include "debuglog.h"

int MSGSendData(int filedes, int blockAmount, const void *data,
	unsigned int dataSize)
{
	/*
	 * default is success 
	 */
	int retval = 0;
	/*
	 * record the time when we started 
	 */
	time_t start = time(0);
	/*
	 * data to be written 
	 */
	unsigned char *buffer = (unsigned char *) data;
	/*
	 * how many bytes remains to be written 
	 */
	size_t remaining = dataSize;

	/*
	 * repeat until all data is written 
	 */
	while (remaining > 0)
	{
		fd_set write_fd;
		struct timeval timeout;
		int selret;

		FD_ZERO(&write_fd);
		FD_SET(filedes, &write_fd);

		timeout.tv_usec = 0;
		if ((timeout.tv_sec = start + blockAmount - time(0)) < 0)
		{
			/*
			 * we already timed out 
			 */
			retval = -1;
			break;
		}

		selret = select(filedes + 1, NULL, &write_fd, NULL, &timeout);

		/*
		 * try to write only when the file descriptor is writable 
		 */
		if (selret > 0)
		{
			int written;

			if (!FD_ISSET(filedes, &write_fd))
			{
				/*
				 * very strange situation. it should be an assert really 
				 */
				retval = -1;
				break;
			}
			written = write(filedes, buffer, remaining);

			if (written > 0)
			{
				/*
				 * we wrote something 
				 */
				buffer += written;
				remaining -= written;
			} else if (written == 0)
			{
				/*
				 * peer closed the socket 
				 */
				retval = -1;
				break;
			} else
			{
				/*
				 * we ignore the signals and socket full situations, all
				 * other errors are fatal 
				 */
				if (errno != EINTR && errno != EAGAIN)
				{
					retval = -1;
					break;
				}
			}
		} else if (selret == 0)
		{
			/*
			 * timeout 
			 */
			retval = -1;
			break;
		} else
		{
			/*
			 * ignore signals 
			 */
			if (errno != EINTR)
			{
				DebugLogB
					("MSGServerProcessEvents: Select returns with failure: %s",
					strerror(errno));
				retval = -1;
				break;
			}
		}
	}

	return retval;
}

int MSGRecieveData(int filedes, int blockAmount, void *data,
	unsigned int dataSize)
{
	/*
	 * default is success 
	 */
	int retval = 0;
	/*
	 * record the time when we started 
	 */
	time_t start = time(0);
	/*
	 * buffer where we place the readed bytes 
	 */
	unsigned char *buffer = (unsigned char *) data;
	/*
	 * how many bytes we must read 
	 */
	size_t remaining = dataSize;

	/*
	 * repeat until we get the whole message 
	 */
	while (remaining > 0)
	{
		fd_set read_fd;
		struct timeval timeout;
		int selret;

		FD_ZERO(&read_fd);
		FD_SET(filedes, &read_fd);

		timeout.tv_usec = 0;
		if ((timeout.tv_sec = start + blockAmount - time(0)) < 0)
		{
			/*
			 * we already timed out 
			 */
			retval = -1;
			break;
		}

		selret = select(filedes + 1, &read_fd, NULL, NULL, &timeout);

		/*
		 * try to read only when socket is readable 
		 */
		if (selret > 0)
		{
			int readed;

			if (!FD_ISSET(filedes, &read_fd))
			{
				/*
				 * very strange situation. it should be an assert really 
				 */
				retval = -1;
				break;
			}
			readed = read(filedes, buffer, remaining);

			if (readed > 0)
			{
				/*
				 * we got something 
				 */
				buffer += readed;
				remaining -= readed;
			} else if (readed == 0)
			{
				/*
				 * peer closed the socket 
				 */
				retval = -1;
				break;
			} else
			{
				/*
				 * we ignore the signals and empty socket situations, all
				 * other errors are fatal 
				 */
				if (errno != EINTR && errno != EAGAIN)
				{
					retval = -1;
					break;
				}
			}
		} else if (selret == 0)
		{
			/*
			 * timeout 
			 */
			retval = -1;
			break;
		} else
		{
			/*
			 * we ignore signals, all other errors are fatal 
			 */
			if (errno != EINTR)
			{
				DebugLogB
					("MSGServerProcessEvents: Select returns with failure: %s",
					strerror(errno));
				retval = -1;
				break;
			}
		}
	}

	return retval;
}
