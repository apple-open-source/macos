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

#ifndef _SCPRIVATE_H
#define _SCPRIVATE_H

#include <sys/cdefs.h>

#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <SystemConfiguration/SCDynamicStoreSetSpecificPrivate.h>

#include <SystemConfiguration/SCPreferencesPrivate.h>

/* framework variables */
extern Boolean	_sc_debug;	/* TRUE if debugging enabled */
extern Boolean	_sc_verbose;	/* TRUE if verbose logging enabled */
extern Boolean	_sc_log;	/* TRUE if SCLog() output goes to syslog */

__BEGIN_DECLS

/*!
	@function _SCErrorSet
	@discussion Returns a last SystemConfiguration.framework API error code.
	@result The last error encountered.
 */
void		_SCErrorSet			(int			error);

/*
	@function SCLog
	@discussion Conditionally issue a log message.
	@param condition A boolean value indicating if the message should be logged
	@param level A syslog(3) logging priority.
	@param formatString The format string
	@result The specified message will be written to the system message
		logger (See syslogd(8)).
 */
void		SCLog				(Boolean		condition,
						 int			level,
						 CFStringRef		formatString,
						 ...);

/*
	@function SCPrint
	@discussion Conditionally issue a debug message.
	@param condition A boolean value indicating if the message should be written
	@param stream The output stream for the log message.
	@param formatString The format string
	@result The specified message will be written to the specified output
		stream.
 */
void		SCPrint				(Boolean		condition,
						 FILE			*stream,
						 CFStringRef		formatString,
						 ...);

__END_DECLS

#endif	/* _SCPRIVATE_H */
