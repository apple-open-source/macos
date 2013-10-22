/* $Id: plog.h,v 1.5 2004/06/11 16:00:17 ludvigm Exp $ */

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

#ifndef _PLOG_H
#define _PLOG_H

#include "config.h"

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include  <asl.h>
#include <sys/queue.h>
#include <SystemConfiguration/SCPreferences.h>


extern char *pname;
extern u_int32_t loglevel;
extern int f_foreground;
extern int print_location;
extern char *logfile;
extern char	logFileStr[];
extern char *gSessId;
extern char *gSessType;
extern char *gSessVer;
extern aslclient logRef;

struct sockaddr_storage;

typedef enum clog_err_op {
	CLOG_ERR_OFF = 0,
	CLOG_ERR_FILL,
	CLOG_ERR_DUMP,
} clog_err_op_t;

typedef struct clog_err {
	int  clog_err_level; /* will be used for filtering */
	int  clog_err_code; /* internal code */
	char *client_id;
	char *client_type;
	char *description;
	int   description_len;
	const char *function;
	const char *line;

	// add a CFErrorRef for global error code (i.e. number-space)

	TAILQ_HEAD(_chain_head, clog_err) chain_head;
	TAILQ_ENTRY(clog_err) chain;
} clog_err_t;

extern const char *plog_facility;
extern const char *plog_session_id;
extern const char *plog_session_type;
extern const char *plog_session_ver;
extern void clog_func (clog_err_t *, clog_err_op_t, int, const char *, const char *, const char *, ...);
extern void plogdump_asl (aslmsg, int, const char *, ...);
extern void plogdump_func (int, void *, size_t, const char *, ...);
extern void plogcf(int priority, CFStringRef fmt, ...);

#define clog(cerr, cerr_op, pri, fmt, args...)	do {									\
										if (pri <= loglevel) {							\
											clog_func(cerr, cerr_op, pri, __FUNCTION__, __LINE__, fmt, ##args); \
										}												\
} while(0)

#define plog(pri, fmt, args...)	do {													\
										if (pri <= loglevel) {							\
											aslmsg m;									\
											if ((m = asl_new(ASL_TYPE_MSG))) {			\
												asl_set(m, ASL_KEY_FACILITY, plog_facility); \
												if (gSessId)							\
													asl_set(m, plog_session_id, gSessId); \
												if (gSessType)							\
													asl_set(m, plog_session_type, gSessType); \
												if (gSessVer)							\
													asl_set(m, plog_session_ver, gSessVer); \
												asl_log(logRef, m, pri, fmt, ##args); \
                                                asl_free(m);                            \
											}											\
										}												\
									} while(0)

#define plogdump(pri, buf, len, fmt, args...)	do {									\
										if (pri <= loglevel) {							\
											plogdump_func(pri, buf, len, fmt, ##args);	\
										}												\
								} while(0)

void ploginit(void);

void plogreadprefs (void);

void plogsetfile (char *);

void plogresetfile (char *);

int ploggetlevel(void);

void plogsetlevel (int);

void plogresetlevel (void);

void plogsetlevelstr (char *);
void plogsetlevelquotedstr (char *);

// Called at the beginning of any dispatch event to initialize the logger with protocol client info
void plogsetsessioninfo (const char *session_id,
						 const char *session_type,
						 const char *session_ver);

#endif /* _PLOG_H */
