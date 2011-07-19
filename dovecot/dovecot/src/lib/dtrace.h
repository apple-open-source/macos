/* Simple dtrace hooks.  These macros must match the dtrace probes
   defined in dtrace-dovecot.d. */

/*
 * Copyright (c) 2008-2011 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without  
 * modification, are permitted provided that the following conditions  
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright  
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above  
 * copyright notice, this list of conditions and the following  
 * disclaimer in the documentation and/or other materials provided  
 * with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
 * contributors may be used to endorse or promote products derived  
 * from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND  
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF  
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.
 */

#ifdef APPLE_OS_X_SERVER

#include "src/lib/dtrace-dovecot.h"

#define DTRACE_OD_LOOKUP_CACHED(a, b, c)		STMT_START {	\
    if (DOVECOT_OD_LOOKUP_CACHED_ENABLED())				\
	DOVECOT_OD_LOOKUP_CACHED(a, b, c);				\
							} STMT_END

#define DTRACE_OD_LOOKUP_START(a, b)			STMT_START {	\
    if (DOVECOT_OD_LOOKUP_START_ENABLED())				\
	DOVECOT_OD_LOOKUP_START(a, b);					\
							} STMT_END

#define DTRACE_OD_LOOKUP_FINISH(a, b, c)		STMT_START {	\
    if (DOVECOT_OD_LOOKUP_FINISH_ENABLED())				\
	DOVECOT_OD_LOOKUP_FINISH(a, b, c);				\
							} STMT_END

#define DTRACE_OD_SACL_START(a, b)			STMT_START {	\
    if (DOVECOT_OD_SACL_START_ENABLED())				\
	DOVECOT_OD_SACL_START(a, b);					\
							} STMT_END

#define DTRACE_OD_SACL_FINISH(a, b, c)			STMT_START {	\
    if (DOVECOT_OD_SACL_FINISH_ENABLED())				\
	DOVECOT_OD_SACL_FINISH(a, b, c);				\
							} STMT_END

#define DTRACE_IMAP_LOGIN_COMMAND_START(a, b, c)	STMT_START {	\
    if (DOVECOT_IMAP_LOGIN_COMMAND_START_ENABLED())			\
	DOVECOT_IMAP_LOGIN_COMMAND_START(a, b, c);			\
							} STMT_END

#define DTRACE_IMAP_LOGIN_COMMAND_FINISH(a, b, c, d)	STMT_START {	\
    if (DOVECOT_IMAP_LOGIN_COMMAND_FINISH_ENABLED())			\
	DOVECOT_IMAP_LOGIN_COMMAND_FINISH(a, b, c, d);			\
							} STMT_END

#define DTRACE_IMAP_COMMAND_START(a)			STMT_START {	\
    if (DOVECOT_IMAP_COMMAND_START_ENABLED())				\
	DOVECOT_IMAP_COMMAND_START(a);					\
							} STMT_END

#define DTRACE_IMAP_COMMAND_FINISH(a)			STMT_START {	\
    if (DOVECOT_IMAP_COMMAND_FINISH_ENABLED())				\
	DOVECOT_IMAP_COMMAND_FINISH(a);					\
							} STMT_END

#define DTRACE_POP3_LOGIN_COMMAND_START(a, b, c)	STMT_START {	\
    if (DOVECOT_POP3_LOGIN_COMMAND_START_ENABLED())			\
	DOVECOT_POP3_LOGIN_COMMAND_START(a, b, c);			\
							} STMT_END

#define DTRACE_POP3_LOGIN_COMMAND_FINISH(a, b, c, d)	STMT_START {	\
    if (DOVECOT_POP3_LOGIN_COMMAND_FINISH_ENABLED())			\
	DOVECOT_POP3_LOGIN_COMMAND_FINISH(a, b, c, d);			\
							} STMT_END

#define DTRACE_POP3_COMMAND_START(a, b, c)		STMT_START {	\
    if (DOVECOT_POP3_COMMAND_START_ENABLED())				\
	DOVECOT_POP3_COMMAND_START(a, b, c);				\
							} STMT_END

#define DTRACE_POP3_COMMAND_FINISH(a, b, c, d)		STMT_START {	\
    if (DOVECOT_POP3_COMMAND_FINISH_ENABLED())				\
	DOVECOT_POP3_COMMAND_FINISH(a, b, c, d);			\
							} STMT_END

#else /* APPLE_OS_X_SERVER */

#define DTRACE_OD_LOOKUP_CACHED(a, b, c)		/**/
#define DTRACE_OD_LOOKUP_START(a, b)			/**/
#define DTRACE_OD_LOOKUP_FINISH(a, b, c)		/**/
#define DTRACE_OD_SACL_START(a, b)			/**/
#define DTRACE_OD_SACL_FINISH(a, b, c)			/**/
#define DTRACE_IMAP_LOGIN_COMMAND_START(a, b, c)	/**/
#define DTRACE_IMAP_LOGIN_COMMAND_FINISH(a, b, c, d)	/**/
#define DTRACE_IMAP_COMMAND_START(a)			/**/
#define DTRACE_IMAP_COMMAND_FINISH(a)			/**/
#define DTRACE_POP3_LOGIN_COMMAND_START(a, b, c)	/**/
#define DTRACE_POP3_LOGIN_COMMAND_FINISH(a, b, c, d)	/**/
#define DTRACE_POP3_COMMAND_START(a, b, c)		/**/
#define DTRACE_POP3_COMMAND_FINISH(a, b, c, d)		/**/

#endif /* APPLE_OS_X_SERVER */
