/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Alert handling (allow user to abort local domain binding)
 * Copyright (C) 1989 by NeXT, Inc.
 *
 */
#include <NetInfo/config.h>
 
/*
 * Disabled for MacOS X for the momment,
 * There's no support for console alerts yet.
 */
#ifdef _OS_APPLE_

static int aborted = 0;

static enum { 
	ALERT_FRESH,
	ALERT_OPENED,
	ALERT_PRINTED, 
	ALERT_CLOSED
} alert_state = ALERT_CLOSED;

void
alert_enable(int enable)
{
	if (enable) alert_state = ALERT_FRESH;
	else alert_state = ALERT_CLOSED;
}

int
alert_aborted(void)
{
	return aborted;
}

void
alert_close(void)
{
	if (alert_state != ALERT_PRINTED) return;
	alert_state = ALERT_CLOSED;
}

void alert_open(const char *language)
{
	if (alert_state != ALERT_FRESH) return;
	alert_state = ALERT_PRINTED;
}

#else

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/file.h>
#include <sys/param.h>
#include <errno.h>
#ifdef _OS_NEXT_
#define BUNDLE_PATH	"/usr/lib/NextStep/Resources"
#define _PATH_CONSOLE "/dev/console"
#else
#define BUNDLE_PATH	"/System/Library/CoreServices/Resources"
#include <sys/ioctl_compat.h>
#endif
#ifndef _PATH_CONSOLE
#include <paths.h>
#endif
#include "ni_globals.h"
#include "system.h"
#include <NetInfo/system_log.h>

#define KERNEL_PRIVATE
#include <dev/kmreg_com.h>
#undef KERNEL_PRIVATE

#define MAXLINE 4096
#define STRING_TABLE "NetInfo"

static const char SEARCH_MESSAGE[] = 
	"\\nStill searching for parent network administration (NetInfo) "
	"server.\\nPlease wait, or press 'c' to continue without network "
	"user accounts.\\n\\nSee your system administrator if you need help.\\n";

static const char MESSAGE[] = 
	"\nStill searching for parent network administration (NetInfo) "
	"server.\nPlease wait, or press 'c' to continue without network "
	"user accounts.\n\nSee your system administrator if you need help.\n";

static int aborted = 0;
static int console;
static int console_flags;
static struct sgttyb console_sg;
static enum { 
	ALERT_FRESH,
	ALERT_OPENED,
	ALERT_PRINTED, 
	ALERT_CLOSED
} alert_state = ALERT_CLOSED;

/*
 * Enable (or disable) alerts
 */
void
alert_enable(int enable)
{
	if (enable) alert_state = ALERT_FRESH;
	else alert_state = ALERT_CLOSED;
}

/*
 * Did the user abort the alert?
 */
int
alert_aborted(void)
{
	return (aborted);
}

/*
 * Close an alert
 */
void
alert_close(void)
{
	if (alert_state != ALERT_PRINTED) return;
	alert_state = ALERT_CLOSED;

	/* Restore fd flags */
	fcntl(console, F_SETFL, console_flags);

	/* Restore terminal parameters */
	ioctl(console, TIOCSETP, &console_sg);

	/* Remove the window */
	ioctl(console, KMIOCRESTORE, 0);
	close(console);
}

/*
 * We got a SIGIO. Handle it.
 */
static void
handle_io(int ignored)
{
	char buf[512];
	char *cp;
	int nchars;

	while ((nchars = read(console, (char *)&buf, sizeof(buf))) > 0)
	{
		cp = buf;
		while (nchars--)
		{
			if ((*cp == 'c') || (*cp == 'C'))
			{
				system_log(LOG_DEBUG, "Binding aborted");
				aborted = 1;
				alert_close();
				return;
			}
			cp++;
		}
	}
	if (errno != EWOULDBLOCK) alert_close();
}

/*
 * Open an alert
 */		
void
alert_open(const char *language)
{
	struct sgttyb sg;
	FILE *fp;
	char filename[MAXPATHLEN + 1];
	char buf[MAXLINE];
	char outmsg[MAXLINE];
	int i, j, len;

	if (alert_state != ALERT_FRESH) return;
	alert_state = ALERT_OPENED;

	/* Open up the console */
	if ((console = open(_PATH_CONSOLE, (O_RDWR|O_ALERT), 0)) < 0)
	{
		system_log(LOG_ERR, "console open failed: %s", strerror(errno));
		aborted = 1;
		return;
	}

	/* Flush any existing input */
	ioctl(console, TIOCFLUSH, FREAD);

	/* Set it up to interrupt on input */
	if ((console_flags = fcntl(console, F_GETFL, 0)) == -1)
	{
		system_log(LOG_ERR, "console F_GETFL fcntl failed: %s",
			strerror(errno));
		aborted = 1;
		close(console);
		return;
	}

	if (fcntl(console, F_SETFL, (console_flags|FASYNC|FNDELAY)) == -1)
	{
		system_log(LOG_ERR, "console F_SETFL fcntl failed: %s",
			strerror(errno));
		aborted = 1;
		close(console);
		return;
	}

	signal(SIGIO, handle_io);

	/* Put it in CBREAK mode */
	if (ioctl(console, TIOCGETP, &sg) == -1)
	{
		system_log(LOG_ERR, "console TIOCGETP ioctl failed: %s",
			strerror(errno));
		aborted = 1;
		close(console);
		return;
	}

	console_sg = sg;
	sg.sg_flags |= CBREAK;
	sg.sg_flags &= ~ECHO;
	if (ioctl(console, TIOCSETP, &sg) == -1)
	{
		system_log(LOG_ERR, "console TIOCSETP ioctl failed: %s",
			strerror(errno));
		aborted = 1;
		close(console);
		return;
	}

	alert_state = ALERT_PRINTED;

	sprintf(filename, "%s/%s.lproj/%s.strings",
		BUNDLE_PATH, language, STRING_TABLE);
	len = strlen(MESSAGE);

	fp = fopen(filename, "r");
	if (fp == NULL)
	{
		write(console, MESSAGE, len);
		return;
	}

	while (fgets(buf, MAXLINE, fp) != NULL)
	{
		if (!strncmp(buf+1, SEARCH_MESSAGE, len))
		{
			for (i = strlen(buf); ((buf[i] != '\"') && (i > 0)); i--);
			if (i <= 0) break;
			buf[i] = '\0';

			for (i = strlen(SEARCH_MESSAGE) + 2; ((buf[i] != '\0') && (buf[i] != '\"')); i++);
			if (buf[i] == '\0') break;

			i++;
			j = 0;
			for (; buf[i] != '\0'; i++)
			{
				if ((buf[i] == '\\') && (buf[i+1] == 'n'))
				{
					i++;
					outmsg[j++] = '\n';
				}
				else outmsg[j++] = buf[i];
			}
			if (j == 0) break;

			fclose(fp);
			outmsg[j] = '\0';
			write(console, outmsg, j);
			return;
		}
	}

	fclose(fp);
	write(console, MESSAGE, len);
}
#endif NO_CONSOLE_SUPPORT
