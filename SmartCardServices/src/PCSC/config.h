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

/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if you have the daemon function.  */
#define HAVE_DAEMON 1

/* Name of package */
#define PACKAGE "PCSC Framework"

/* Version number of package */
#define VERSION "1.1.2"

/* OSX */
#define PCSC_TARGET_OSX 1
#define MSC_TARGET_OSX 1

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* enable full PCSC debug messaging. */
  #define PCSC_DEBUG 1

/* enable full musclecard debug messaging. */
  #define MSC_DEBUG 1

/* display ATR parsing debug messages. */
/* #define ATR_DEBUG */

/* send messages to syslog instead of stdout */
/* #define USE_SYSLOG */

/* pcsc runs as a daemon in the background. */
#define USE_DAEMON 1

/* enable client side thread safety. */
#define USE_THREAD_SAFETY 1

/* enable run pid */
#define USE_RUN_PID "/var/run/pcscd.pid"
