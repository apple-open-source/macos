/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <notify.h>
#include <asl.h>
#include <asl_private.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include <crt_externs.h>

#define	LOG_NO_NOTIFY	0x1000
#define	INTERNALLOG	LOG_ERR|LOG_CONS|LOG_PERROR|LOG_PID

#ifdef BUILDING_VARIANT
__private_extern__ int	_sl_LogFile;		/* fd for log */
__private_extern__ int	_sl_connected;		/* have done connect */
__private_extern__ int	_sl_LogStat;		/* status bits, set by openlog() */
__private_extern__ const char *_sl_LogTag;	/* string to tag the entry with */
__private_extern__ int	_sl_LogFacility;	/* default facility code */
__private_extern__ int	_sl_LogMask;		/* mask of priorities to be logged */
__private_extern__ int  _sl_NotifyToken;	/* for remote control of priority filter */
__private_extern__ int  _sl_NotifyMaster;	/* for remote control of priority filter */
#else /* !BUILDING_VARIANT */
__private_extern__ int	_sl_LogFile = -1;		/* fd for log */
__private_extern__ int	_sl_connected = 0;		/* have done connect */
__private_extern__ int	_sl_LogStat = 0;		/* status bits, set by openlog() */
__private_extern__ const char *_sl_LogTag = NULL;	/* string to tag the entry with */
__private_extern__ int	_sl_LogFacility = LOG_USER;	/* default facility code */
__private_extern__ int	_sl_LogMask = 0xff;		/* mask of priorities to be logged */
__private_extern__ int  _sl_NotifyToken = -1;	/* for remote control of max logged priority */
__private_extern__ int  _sl_NotifyMaster = -1;	/* for remote control of max logged priority */
#endif /* BUILDING_VARIANT */

__private_extern__ void _sl_init_notify();

#define NOTIFY_SYSTEM_MASTER "com.apple.system.syslog.master"
#define NOTIFY_PREFIX_SYSTEM "com.apple.system.syslog"
#define NOTIFY_PREFIX_USER "user.syslog"
#define NOTIFY_STATE_OFFSET 1000

/* notify SPI */
uint32_t notify_register_plain(const char *name, int *out_token);
const char *asl_syslog_faciliy_num_to_name(int);

/*
 * syslog, vsyslog --
 *	print message on log file; output is intended for syslogd(8).
 */
void
#ifdef __STDC__
syslog(int pri, const char *fmt, ...)
#else
syslog(pri, fmt, va_alist)
	int pri;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vsyslog(pri, fmt, ap);
	va_end(ap);
}

void
vsyslog(int pri, const char *fmt, va_list ap)
{
	int status, i, saved_errno, filter, rc_filter;
	time_t tick;
	pid_t pid;
	uint32_t elen, count;
	char *p, *str, *expanded, *err_str, hname[MAXHOSTNAMELEN+1];
	uint64_t cval;
	int fd, mask, level, facility;
	aslmsg msg;

	saved_errno = errno;

	/* Check for invalid bits. */
	if (pri & ~(LOG_PRIMASK | LOG_FACMASK))
	{
		syslog(INTERNALLOG, "syslog: unknown facility/priority: %x", pri);
		pri &= (LOG_PRIMASK | LOG_FACMASK);
	}

	level = LOG_PRI(pri);
	facility = pri & LOG_FACMASK;

	if (facility == 0) facility = _sl_LogFacility;

	/* Get remote-control priority filter */
	filter = _sl_LogMask;
	rc_filter = 0;

	_sl_init_notify();

	if (_sl_NotifyToken >= 0) 
	{
		if (notify_get_state(_sl_NotifyToken, &cval) == NOTIFY_STATUS_OK)
		{
			if (cval != 0)
			{
				filter = cval;
				rc_filter = 1;
			}
		}
	}

	if ((rc_filter == 0) && (_sl_NotifyMaster >= 0))
	{
		if (notify_get_state(_sl_NotifyMaster, &cval) == NOTIFY_STATUS_OK)
		{
			if (cval != 0)
			{
				filter = cval;
			}
		}
	}

	mask = LOG_MASK(level);
	if ((mask & filter) == 0) return;

	/* Build the message. */
	msg = asl_new(ASL_TYPE_MSG);

	if (_sl_LogTag == NULL) _sl_LogTag = *(*_NSGetArgv());
	if (_sl_LogTag != NULL) 
	{
		asl_set(msg, ASL_KEY_SENDER, _sl_LogTag);
	}

	str = (char *)asl_syslog_faciliy_num_to_name(facility);
	if (str != NULL) asl_set(msg, ASL_KEY_FACILITY, str);

	str = NULL;
	tick = time(NULL);
	asprintf(&str, "%lu", tick);
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_TIME, str);
		free(str);
	}

	str = NULL;
	pid = getpid();
	asprintf(&str, "%u", pid);
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_PID, str);
		free(str);
	}

	str = NULL;
	asprintf(&str, "%d", getuid());
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_UID, str);
		free(str);
	}

	str = NULL;
	asprintf(&str, "%u", getgid());
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_GID, str);
		free(str);
	}

	str = NULL;
	asprintf(&str, "%u", level);
	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_LEVEL, str);
		free(str);
	}

	status = gethostname(hname, MAXHOSTNAMELEN);
	if (status < 0) asl_set(msg, ASL_KEY_HOST, "localhost");
	else asl_set(msg, ASL_KEY_HOST, hname);

	/* check for %m */
	count = 0;
	for (i = 0; fmt[i] != '\0'; i++)
	{
		if ((fmt[i] == '%') && (fmt[i+1] == 'm')) count++;
	}

	expanded = NULL;
	elen = 0;
	err_str = NULL;

	/* deal with malloc failures gracefully */
	if (count > 0)
	{
		err_str = strdup(strerror(saved_errno));
		if (err_str == NULL) count = 0;
		else
		{
			elen = strlen(err_str);
			expanded = malloc(i + (count * elen));
			if (expanded == NULL) count = 0;
		}
	}

	if (expanded == NULL) expanded = (char *)fmt;
	if (count > 0)
	{
		p = expanded;

		for (i = 0; fmt[i] != '\0'; i++)
		{
			if ((fmt[i] == '%') && (fmt[i+1] == 'm'))
			{
				memcpy(p, err_str, elen);
				p += elen;
				i++;
			}
			else
			{
				*p++ = fmt[i];
			}
		}

		*p = '\0';
	}

	if (err_str != NULL) free(err_str);

	vasprintf(&str, expanded, ap);
	if (count > 0) free(expanded);

	if (str != NULL)
	{
		asl_set(msg, ASL_KEY_MSG, str);

		/* Output to stderr if requested. */
		if (_sl_LogStat & LOG_PERROR)
		{
			p = NULL;
			if (_sl_LogStat & LOG_PID) asprintf(&p, "%s[%u]: %s", (_sl_LogTag == NULL) ? "???" : _sl_LogTag, pid, str);
			else asprintf(&p, "%s: %s", (_sl_LogTag == NULL) ? "???" : _sl_LogTag, str);

			if (p != NULL)
			{
				struct iovec iov[2];

				iov[0].iov_base = p;
				iov[0].iov_len = strlen(p);
				iov[1].iov_base = "\n";
				iov[1].iov_len = 1;
				writev(STDERR_FILENO, iov, 2);
				free(p);
			}
		}

		free(str);
	}

	/* Get connected, output the message to the local logger. */
	str = asl_format_message(msg, ASL_MSG_FMT_RAW, ASL_TIME_FMT_SEC, &count);
	if (str != NULL)
	{
		p = NULL;
		asprintf(&p, "%10u %s", count, str);
		free(str);

		if (p != NULL)
		{
			count += 12;
			if (_sl_connected == 0) openlog(_sl_LogTag, _sl_LogStat | LOG_NDELAY, 0);

			status = send(_sl_LogFile, p, count, 0);
			if (status< 0)
			{
				closelog();
				openlog(_sl_LogTag, _sl_LogStat | LOG_NDELAY, 0);
				status = send(_sl_LogFile, p, count, 0);
			}

			if (status >= 0)
			{
				free(p);
				asl_free(msg);
				return;
			}

			free(p);
		}
	}

	/*
	 * Output the message to the console.
	 */
	if (_sl_LogStat & LOG_CONS && (fd = open(_PATH_CONSOLE, O_WRONLY | O_NOCTTY | O_NONBLOCK)) >= 0)
	{
		count = 0;

		p = asl_format_message(msg, ASL_MSG_FMT_STD, ASL_TIME_FMT_LCL, &count);
		if (p != NULL)
		{
			struct iovec iov;

			/* count includes trailing nul */
			iov.iov_len = count - 1;
			iov.iov_base = p;
			writev(fd, &iov, 1);
	
			free(p);
		}

		close(fd);
	}

	asl_free(msg);
}

#ifndef BUILDING_VARIANT

static struct sockaddr_un SyslogAddr;	/* AF_UNIX address of local logger */

__private_extern__ void
_sl_init_notify()
{
	int status;
	char *notify_name;
	const char *prefix;

	if (_sl_LogStat & LOG_NO_NOTIFY)
	{
		_sl_NotifyMaster = -2;
		_sl_NotifyToken = -2;
		return;
	}

	if (_sl_NotifyMaster == -1)
	{
		status = notify_register_plain(NOTIFY_SYSTEM_MASTER, &_sl_NotifyMaster);
		if (status != NOTIFY_STATUS_OK) _sl_NotifyMaster = -2;
	}

	if (_sl_NotifyToken == -1)
	{
		_sl_NotifyToken = -2;

		notify_name = NULL;
		prefix = NOTIFY_PREFIX_USER;
		if (getuid() == 0) prefix = NOTIFY_PREFIX_SYSTEM;
		asprintf(&notify_name, "%s.%d", prefix, getpid());

		if (notify_name != NULL)
		{
			status = notify_register_plain(notify_name, &_sl_NotifyToken);
			free(notify_name);
			if (status != NOTIFY_STATUS_OK) _sl_NotifyToken = -2;
		}
	}
}

void
openlog(ident, logstat, logfac)
	const char *ident;
	int logstat, logfac;
{
	if (ident != NULL) _sl_LogTag = ident;

	_sl_LogStat = logstat;

	if (logfac != 0 && (logfac &~ LOG_FACMASK) == 0) _sl_LogFacility = logfac;

	if (_sl_LogFile == -1)
	{
		SyslogAddr.sun_family = AF_UNIX;
		(void)strncpy(SyslogAddr.sun_path, _PATH_LOG, sizeof(SyslogAddr.sun_path));
		if (_sl_LogStat & LOG_NDELAY)
		{
			if ((_sl_LogFile = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) return;
			(void)fcntl(_sl_LogFile, F_SETFD, 1);
		}
	}

	if ((_sl_LogFile != -1) && (_sl_connected == 0))
	{
		if (connect(_sl_LogFile, (struct sockaddr *)&SyslogAddr, sizeof(SyslogAddr)) == -1)
		{
			(void)close(_sl_LogFile);
			_sl_LogFile = -1;
		}
		else
		{
			_sl_connected = 1;
		}
	}

	_sl_init_notify();
}

void
closelog()
{
	if (_sl_LogFile >= 0) {
		(void)close(_sl_LogFile);
		_sl_LogFile = -1;
	}
	_sl_connected = 0;
}

/* setlogmask -- set the log mask level */
int
setlogmask(pmask)
	int pmask;
{
	int omask;

	omask = _sl_LogMask;
	if (pmask != 0) _sl_LogMask = pmask;
	return (omask);
}

#endif /* !BUILDING_VARIANT */
