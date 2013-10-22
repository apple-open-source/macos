/* $Id: plog.c,v 1.6.10.1 2005/12/07 10:19:51 vanhu Exp $ */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>

#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <ctype.h>
#include <err.h>
#include <pthread.h>
#include <unistd.h>
#include <asl.h>
#include <syslog.h>
#include <asl_private.h>

#include "var.h"
#include "misc.h"
#include "plog.h"
#include "debug.h"
#include "gcmalloc.h"
#include "preferences.h"

#ifndef VA_COPY
# define VA_COPY(dst,src) memcpy(&(dst), (src), sizeof(va_list))
#endif

const char *plog_facility = "com.apple.racoon";
const char *plog_session_id = "com.apple.racoon.sessionid";
const char *plog_session_type = "com.apple.racoon.sessiontype";
const char *plog_session_ver = "com.apple.racoon.sessionversion";

extern int print_pid;

char *pname = NULL;
u_int32_t loglevel = ASL_LEVEL_NOTICE;
//u_int32_t loglevel = ASL_LEVEL_DEBUG;
int f_foreground = 0;

int print_location = 0;

char *logfile = NULL;
int logfile_fd = -1;
char  logFileStr[MAXPATHLEN+1];
char *gSessId = NULL;
char *gSessType = NULL;
char *gSessVer = NULL;
aslclient logRef = NULL;

void
plogdump_asl (aslmsg msg, int pri, const char *fmt, ...)
{
	caddr_t buf;
	size_t buflen = 512;
    va_list	args;
	char   *level;

	switch (pri) {
	case ASL_LEVEL_INFO:
		level = ASL_STRING_INFO;
		break;

	case ASL_LEVEL_NOTICE:
		level = ASL_STRING_NOTICE;
		break;

	case ASL_LEVEL_WARNING:
		level = ASL_STRING_WARNING;
		break;

	case ASL_LEVEL_ERR:
		level = ASL_STRING_ERR;
		break;

	case ASL_LEVEL_DEBUG:
		level = ASL_STRING_DEBUG;
		break;

	default:
		return;
	}

	asl_set(msg, ASL_KEY_LEVEL, level);

	buf = racoon_malloc(buflen);
	if (buf) {
		buf[0] = '\0';
		va_start(args, fmt);
		vsnprintf(buf, buflen, fmt, args);
//		asl_set(msg, ASL_KEY_MESSAGE, buf);
		va_end(args);
		racoon_free(buf);
	}
}

void
plogdump_func(int pri, void *data, size_t len, const char *fmt, ...)
{
	caddr_t buf;
	size_t buflen;
	int i, j;
    va_list	args;
	char fmt_buf[512];

	/*
	 * 2 words a bytes + 1 space 4 bytes + 1 newline 32 bytes
	 * + 2 newline + '\0'
	 */
	buflen = (len * 2) + (len / 4) + (len / 32) + 3;
	buf = racoon_malloc(buflen);

	i = 0;
	j = 0;
	while (j < len) {
		if (j % 32 == 0)
			buf[i++] = '\n';
		else
		if (j % 4 == 0)
			buf[i++] = ' ';
		snprintf(&buf[i], buflen - i, "%02x",
			((unsigned char *)data)[j] & 0xff);
		i += 2;
		j++;
	}
	if (buflen - i >= 2) {
		buf[i++] = '\n';
		buf[i] = '\0';
	}

	fmt_buf[0] = '\n';
	va_start(args, fmt);
	vsnprintf(fmt_buf, sizeof(fmt_buf), fmt, args);
	va_end(args);

	plog(pri, "%s %s", fmt_buf, buf);

	racoon_free(buf);
}

void
clog_func (clog_err_t *cerr, clog_err_op_t cerr_op, int pri, const char *function, const char *line, const char *fmt, ...)
{
	clog_err_t *new, *p;
    va_list	args;

	if (!cerr) {
		return;
	}

	if (!(new = racoon_calloc(1, sizeof(*cerr)))) {
		return;
	}
	// fill in new
	cerr->clog_err_level = pri; /* will be used for filtering */
	/* TODO */
	//cerr->clog_err_code;
	//cerr->client_id;
	//cerr->client_type;
	va_start(args, fmt);
	cerr->description_len = vasprintf(&cerr->description, fmt, args);
	va_end(args);
	cerr->function = function;
	cerr->line = line;
	
	// add new to the tail
	TAILQ_FOREACH(p, &cerr->chain_head, chain) {
		if (TAILQ_NEXT(p, chain) == NULL) {
			TAILQ_NEXT(p, chain) = new;
			new->chain.tqe_prev = &TAILQ_NEXT(p, chain);
			break;
		}
	}

	if (cerr_op == CLOG_ERR_DUMP) {
		char *prev = NULL, *backtrace = NULL;

		TAILQ_FOREACH(p, &cerr->chain_head, chain) {
			// collapse list into backtrace
			if (cerr->description) {
				if (backtrace) {
					prev = backtrace;
					backtrace = NULL;
					asprintf(&backtrace, "%s\n\t\t-> %s", prev, cerr->description);
					free(prev);
				} else {
					asprintf(&backtrace, "%s", cerr->description);
				}
			}
		}

		if (backtrace) {
			// use plog to dump event.
			plog(pri, "%s", backtrace);
		}
	}
}

void
plogsetfile(file)
	char *file;
{
	syslog(LOG_NOTICE, "%s: about to add racoon log file: %s\n", __FUNCTION__, file? file:"bad file path");
	if (logfile != NULL) {
		racoon_free(logfile);
		if (logfile_fd != -1) {
			asl_remove_log_file(logRef, logfile_fd);
			asl_close_auxiliary_file(logfile_fd);
			logfile_fd = -1;
		}
	}
	logfile = racoon_strdup(file);
	STRDUP_FATAL(logfile);
	if ((logfile_fd = open(logfile, O_CREAT | O_WRONLY | O_APPEND | O_NOFOLLOW, 0)) >= 0) {
		asl_add_log_file(logRef, logfile_fd);
	} else {
		syslog(LOG_NOTICE, "%s: failed to add racoon log file: %s. error %d\n", __FUNCTION__, file? file:"bad file path", errno);
	}
}

void
plogresetfile(file)
	char *file;
{
	/* if log paths equal - do nothing */
	if (logfile == NULL && file == NULL) {
		return;
	}
	if (logfile != NULL && file != NULL) {
		if (!strcmp(logfile, file)) {
			return;
		}
		if (logfile_fd != -1) {
			asl_remove_log_file(logRef, logfile_fd);
			close(logfile_fd);
			logfile_fd = -1;
		}
	}

	if (logfile) {
			racoon_free(logfile);
			logfile = NULL;
	}

	if (file)
		plogsetfile(file);
}

int
ploggetlevel(void)
{
    return loglevel;
}

void
plogsetlevel(int level)
{
	int mask;

	if (level && level >= ASL_LEVEL_EMERG && level <= ASL_LEVEL_DEBUG) {
		loglevel = level;
	}
	if (loglevel >= ASL_LEVEL_INFO) {
		mask = ASL_FILTER_MASK_TUNNEL;
	} else {
		mask = 0;
	}
	mask |= ASL_FILTER_MASK_UPTO(loglevel);
	syslog(LOG_DEBUG, "%s: about to set racoon's log level %d, mask %x\n", __FUNCTION__, level, mask);
	asl_set_filter(NULL, mask);
}

void
plogsetlevelstr(char *levelstr)
{
	if (!levelstr) {
		return;
	}

	if (strncmp(levelstr, ASL_STRING_EMERG, sizeof(ASL_STRING_EMERG) - 1) == 0) {
		plogsetlevel(ASL_LEVEL_EMERG);
	} else if (strncmp(levelstr, ASL_STRING_ALERT, sizeof(ASL_STRING_ALERT) - 1) == 0) {
		plogsetlevel(ASL_LEVEL_ALERT);
	} else if (strncmp(levelstr, ASL_STRING_CRIT, sizeof(ASL_STRING_CRIT) - 1) == 0) {
		plogsetlevel(ASL_LEVEL_CRIT);
	} else if (strncmp(levelstr, ASL_STRING_ERR, sizeof(ASL_STRING_ERR) - 1) == 0) {
		plogsetlevel(ASL_LEVEL_ERR);
	} else if (strncmp(levelstr, ASL_STRING_WARNING, sizeof(ASL_STRING_NOTICE) - 1) == 0) {
		plogsetlevel(ASL_LEVEL_WARNING);
	} else if (strncmp(levelstr, ASL_STRING_NOTICE, sizeof(ASL_STRING_NOTICE) - 1) == 0) {
		plogsetlevel(ASL_LEVEL_NOTICE);
	} else if (strncmp(levelstr, ASL_STRING_INFO, sizeof(ASL_STRING_INFO) - 1) == 0) {
		plogsetlevel(ASL_LEVEL_INFO);
	} else if (strncmp(levelstr, ASL_STRING_DEBUG, sizeof(ASL_STRING_DEBUG) - 1) == 0) {
		plogsetlevel(ASL_LEVEL_DEBUG);
	}
}

void
plogsetlevelquotedstr (char *levelquotedstr)
{
	int len;

	if (!levelquotedstr) {
		plog(ASL_LEVEL_ERR, "Null log level (quoted string)");
		return;
	}

	len = strlen(levelquotedstr);
	if (len < 3 ||
		levelquotedstr[0] != '"' ||
		levelquotedstr[len - 1] != '"') {
		plog(ASL_LEVEL_ERR, "Invalid log level (quoted string): %s", levelquotedstr);
		return;
	}
	// skip quotes
	levelquotedstr[len - 1] = '\0';
	plogsetlevelstr(&levelquotedstr[1]);
}

void
plogreadprefs (void)
{
	CFPropertyListRef	globals;
	CFStringRef			logFileRef;
	CFNumberRef			debugLevelRef;
	CFStringRef			debugLevelStringRef;
	char                logLevelStr[16];
	int					level = 0;

	logLevelStr[0] = 0;

    SCPreferencesSynchronize(gPrefs);

    globals = SCPreferencesGetValue(gPrefs, CFSTR("Global"));
    if (!globals || (CFGetTypeID(globals) != CFDictionaryGetTypeID())) {
        return;
    }
    debugLevelRef = CFDictionaryGetValue(globals, CFSTR("DebugLevel"));
    if (debugLevelRef && (CFGetTypeID(debugLevelRef) == CFNumberGetTypeID())) {
        CFNumberGetValue(debugLevelRef, kCFNumberSInt32Type, &level);
        plogsetlevel(level);
    } else {
        debugLevelStringRef = CFDictionaryGetValue(globals, CFSTR("DebugLevelString"));
        if (debugLevelStringRef && (CFGetTypeID(debugLevelStringRef) == CFStringGetTypeID())) {
            CFStringGetCString(debugLevelStringRef, logLevelStr, sizeof(logLevelStr), kCFStringEncodingMacRoman);
            plogsetlevelstr(logLevelStr);
        }
    }
    
    logFileRef = CFDictionaryGetValue(globals, CFSTR("DebugLogfile"));
    if (!logFileRef	|| (CFGetTypeID(logFileRef) != CFStringGetTypeID())) {	
        return;
    }
    CFStringGetCString(logFileRef, logFileStr, MAXPATHLEN, kCFStringEncodingMacRoman);
    plogsetfile(logFileStr);
}

void
ploginit(void)
{
	logFileStr[0] = 0;
	logRef = NULL;//asl_open(NULL, plog_facility, 0);
	plogsetlevel(ASL_LEVEL_NOTICE);
	//plogsetlevel(ASL_LEVEL_DEBUG);
	plogreadprefs();
}

void
plogsetsessioninfo (const char *session_id,
					const char *session_type,
					const char *session_ver)
{
	if (gSessId) {
		free(gSessId);
	}
	if (!session_id) {
		gSessId = NULL;
	} else {
		gSessId = strdup(session_id);
	}
	if (gSessId) {
		free(gSessId);
	}
	if (!session_type) {
		gSessType = NULL;
	} else {
		gSessType = strdup(session_id);
	}
	if (gSessVer) {
		free(gSessVer);
	}
	if (!session_ver) {
		gSessVer = NULL;
	} else {
		gSessVer = strdup(session_ver);
	}
}

char *
createCStringFromCFString(CFAllocatorRef allocator, CFStringRef cfstr)
{
    CFIndex cstr_len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8) + 1;
    char *cstr = (char *)CFAllocatorAllocate(allocator, cstr_len, 0);
    CFStringGetCString(cfstr, cstr, cstr_len, kCFStringEncodingUTF8);
    return cstr;
}

void
plogcf(int priority, CFStringRef fmt, ...)
{
    va_list         args;
    CFStringRef     cfstr;
    char            *cstr;
    
    va_start(args, fmt);
    cfstr = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, NULL, fmt, args);
    va_end(args);
    
    cstr = createCStringFromCFString(kCFAllocatorDefault, cfstr);
    plog(priority, "%s", cstr);
    
    CFAllocatorDeallocate(kCFAllocatorDefault, cstr);
    CFRelease(cfstr);
}


