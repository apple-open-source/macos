/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#define __V(x)	x

void closeall();
u_long loadKext(char *kext);
u_long unloadKext(char *kext);
pid_t start_program(char *program, char *command, int in, int out, int err);
u_char exist_file(char *program);



void log_packet (struct ppp *, u_char *, int, char *, int);
/* Format a packet and log it with syslog */
void print_string (struct ppp *, char *, int,  void (*) (struct ppp *, void *, char *, ...), void *);
/* Format a string for output */
int slprintf __P((struct ppp *, char *, int, char *, ...));		/* sprintf++ */
int vslprintf __P((struct ppp *, char *, int, char *, va_list));	/* vsprintf++ */
size_t strlcpy __P((char *, const char *, size_t));	/* safe strcpy */
size_t strlcat __P((char *, const char *, size_t));	/* safe strncpy */
void dbglog __P((struct ppp *, char *, ...));	/* log a debug message */
void notice __P((struct ppp *, char *, ...));	/* log a notice-level message */
void warn __P((struct ppp *, char *, ...));	/* log a warning message */
void fatal __P((struct ppp *, char *, ...));	/* log an error message and die(1) */

void error __V((struct ppp *ppp, char *fmt, ...));
void info __V((struct ppp *ppp, char *fmt, ...));
