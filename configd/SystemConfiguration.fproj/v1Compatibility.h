/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#ifndef _V1COMPATIBILITY_H
#define	_V1COMPATIBILITY_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <mach/message.h>
#include <CoreFoundation/CoreFoundation.h>


#define	kSCCacheDomainFile		kSCDynamicStoreDomainFile
#define	kSCCacheDomainPlugin		kSCDynamicStoreDomainPlugin
#define	kSCCacheDomainSetup		kSCDynamicStoreDomainSetup
#define	kSCCacheDomainState		kSCDynamicStoreDomainState
#define	kSCCacheDomainPrefs		kSCDynamicStoreDomainPrefs
#define	kSCCachePropSetupCurrentSet	kSCDynamicStorePropSetupCurrentSet
#define	kSCCachePropSetupLastUpdated	kSCDynamicStorePropSetupLastUpdated
#define	kSCCachePropNetInterfaces	kSCDynamicStorePropNetInterfaces
#define	kSCCachePropNetPrimaryInterface	kSCDynamicStorePropNetPrimaryInterface
#define	kSCCachePropNetPrimaryService	kSCDynamicStorePropNetPrimaryService
#define	kSCCachePropNetServiceIDs	kSCDynamicStorePropNetServiceIDs


typedef enum {
	SCD_OK                  = 0,    /* Success */
	SCD_NOSESSION           = 1,    /* Configuration daemon session not active */
	SCD_NOSERVER            = 2,    /* Configuration daemon not (no longer) available */
	SCD_LOCKED              = 3,    /* Lock already held */
	SCD_NEEDLOCK            = 4,    /* Lock required for this operation */
	SCD_EACCESS             = 5,    /* Permission denied (must be root to obtain lock) */
	SCD_NOKEY               = 6,    /* No such key */
	SCD_EXISTS              = 7,    /* Data associated with key already defined */
	SCD_STALE               = 8,    /* Write attempted on stale version of object */
	SCD_INVALIDARGUMENT     = 9,    /* Invalid argument */
	SCD_NOTIFIERACTIVE      = 10,   /* Notifier is currently active */
	SCD_FAILED              = 9999  /* Generic error */
} SCDStatus;

typedef const struct __SCDSession *	SCDSessionRef;

typedef const struct __SCDHandle *	SCDHandleRef;

typedef enum {
	kSCDRegexKey		= 0100000,	/* pattern string is a regular expression */
} SCDKeyOption;

typedef enum {
	kSCDOptionDebug		= 0,	/* Enable debugging */
	kSCDOptionVerbose	= 1,	/* Enable verbose logging */
	kSCDOptionUseSyslog	= 2,	/* Use syslog(3) for log messages */
	kSCDOptionUseCFRunLoop	= 3,	/* Calling application is CFRunLoop() based */
} SCDOption;

typedef boolean_t (*SCDCallbackRoutine_t)	(SCDSessionRef	session,
						 void		*context);

typedef void (*SCDBundleStartRoutine_t) (const char *bundlePath,
					 const char *bundleName);

typedef void (*SCDBundlePrimeRoutine_t) ();

typedef enum {
	SCP_OK                  = 0,    /* Success */
	SCP_NOSESSION           = 1024, /* Preference session not active */
	SCP_BUSY                = 1025, /* Preferences update currently in progress */
	SCP_NEEDLOCK            = 1026, /* Lock required for this operation */
	SCP_EACCESS             = 1027, /* Permission denied */
	SCP_ENOENT              = 1028, /* Configuration file not found */
	SCP_BADCF               = 1029, /* Configuration file corrupt */
	SCP_NOKEY               = 1030, /* No such key */
	SCP_NOLINK              = 1031, /* No such link */
	SCP_EXISTS              = 1032, /* No such key */
	SCP_STALE               = 1033, /* Write attempted on stale version of object */
	SCP_INVALIDARGUMENT     = 1034, /* Invalid argument */
	SCP_FAILED              = 9999  /* Generic error */
} SCPStatus;

typedef enum {
	kSCPOpenCreatePrefs	= 1,	/* create preferences file if not found */
} SCPOption;

typedef enum {
	kSCPKeyLock	= 1,
	kSCPKeyCommit	= 2,
	kSCPKeyApply	= 3,
} SCPKeyType;

typedef void *		SCPSessionRef;

typedef enum {
	SCN_REACHABLE_UNKNOWN			= -1,
	SCN_REACHABLE_NO			=  0,
	SCN_REACHABLE_CONNECTION_REQUIRED	=  1,
	SCN_REACHABLE_YES			=  2,
} SCNStatus;

typedef enum {
	kSCNFlagsTransientConnection    =  1<<0,
	kSCNFlagsConnectionAutomatic    =  1<<1,
	kSCNFlagsInterventionRequired   =  1<<2,
} SCNConnectionFlags;

__BEGIN_DECLS

/*
 * handle APIs
 */

SCDHandleRef	SCDHandleInit			();

void		SCDHandleRelease		(SCDHandleRef		handle);

int		SCDHandleGetInstance		(SCDHandleRef		handle);

void		_SCDHandleSetInstance		(SCDHandleRef		handle,
						 int			instance);

CFPropertyListRef SCDHandleGetData		(SCDHandleRef		handle);

void		SCDHandleSetData		(SCDHandleRef		handle,
						 CFPropertyListRef	data);

/*
 * store access APIs
 */

SCDStatus	SCDOpen				(SCDSessionRef		*session,
						 CFStringRef		name);

SCDStatus	SCDClose			(SCDSessionRef		*session);

SCDStatus	SCDLock				(SCDSessionRef		session);

SCDStatus	SCDUnlock			(SCDSessionRef		session);

SCDStatus	SCDList				(SCDSessionRef		session,
						 CFStringRef		key,
						 int			regexOptions,
						 CFArrayRef		*subKeys);

SCDStatus	SCDAdd				(SCDSessionRef		session,
						 CFStringRef		key,
						 SCDHandleRef		handle);

SCDStatus	SCDAddSession			(SCDSessionRef		session,
						 CFStringRef		key,
						 SCDHandleRef		handle);

SCDStatus	SCDGet				(SCDSessionRef		session,
						 CFStringRef		key,
						 SCDHandleRef		*handle);

SCDStatus	SCDSet				(SCDSessionRef		session,
						 CFStringRef		key,
						 SCDHandleRef		handle);

SCDStatus	SCDRemove			(SCDSessionRef		session,
						 CFStringRef		key);

SCDStatus	SCDTouch			(SCDSessionRef		session,
						CFStringRef		key);

SCDStatus	SCDNotifierList			(SCDSessionRef		session,
						 int			regexOptions,
						 CFArrayRef		*changedKeys);

SCDStatus	SCDNotifierAdd			(SCDSessionRef		session,
						 CFStringRef		key,
						 int			regexOptions);

SCDStatus	SCDNotifierRemove		(SCDSessionRef		session,
						 CFStringRef		key,
						 int			regexOptions);

SCDStatus	SCDNotifierGetChanges		(SCDSessionRef		session,
						 CFArrayRef		*notifierKeys);

SCDStatus	SCDNotifierWait			(SCDSessionRef		session);

SCDStatus	SCDNotifierInformViaCallback	(SCDSessionRef		session,
						 SCDCallbackRoutine_t	func,
						 void			*arg);

SCDStatus	SCDNotifierInformViaMachPort	(SCDSessionRef		session,
						 mach_msg_id_t		msgid,
						 mach_port_t		*port);

SCDStatus	SCDNotifierInformViaFD		(SCDSessionRef		session,
						 int32_t		identifier,
						 int			*fd);

SCDStatus	SCDNotifierInformViaSignal	(SCDSessionRef		session,
						 pid_t			pid,
						 int			sig);

SCDStatus	SCDNotifierCancel		(SCDSessionRef		session);

SCDStatus	SCDSnapshot			(SCDSessionRef		session);

int		SCDOptionGet			(SCDSessionRef		session,
						 int			option);

void		SCDOptionSet			(SCDSessionRef		session,
						 int			option,
						 int			value);

void		SCDSessionLog			(SCDSessionRef		session,
						 int			level,
						 CFStringRef		formatString,
						 ...);

void		SCDLog				(int			level,
						 CFStringRef		formatString,
						 ...);

const char *	SCDError			(SCDStatus		status);

/*
 * store/preference keys
 */

CFStringRef	SCDKeyCreate				(CFStringRef	fmt,
							 ...);

CFStringRef	SCDKeyCreateNetworkGlobalEntity		(CFStringRef	domain,
							 CFStringRef	entity);

CFStringRef	SCDKeyCreateNetworkInterface		(CFStringRef	domain);

CFStringRef	SCDKeyCreateNetworkInterfaceEntity	(CFStringRef	domain,
							 CFStringRef	ifname,
							 CFStringRef	entity);

CFStringRef	SCDKeyCreateNetworkServiceEntity	(CFStringRef	domain,
							 CFStringRef	serviceID,
							 CFStringRef	entity);

/*
 * preference APIs
 */

SCPStatus	SCPOpen				(SCPSessionRef		*session,
						 CFStringRef		name,
						 CFStringRef		prefsID,
						 int			options);

SCPStatus	SCPUserOpen			(SCPSessionRef		*session,
						 CFStringRef		name,
						 CFStringRef		prefsID,
						 CFStringRef		user,
						 int			options);

SCPStatus	SCPClose			(SCPSessionRef		*session);

SCPStatus	SCPLock				(SCPSessionRef		session,
						 boolean_t		wait);

SCPStatus	SCPCommit			(SCPSessionRef		session);

SCPStatus	SCPApply			(SCPSessionRef		session);

SCPStatus	SCPUnlock			(SCPSessionRef		session);

SCPStatus	SCPGetSignature			(SCPSessionRef		session,
						 CFDataRef		*signature);

SCPStatus	SCPList				(SCPSessionRef		session,
						 CFArrayRef		*keys);

SCPStatus	SCPGet				(SCPSessionRef		session,
						 CFStringRef		key,
						 CFPropertyListRef	*data);

SCPStatus	SCPAdd				(SCPSessionRef		session,
						 CFStringRef		key,
						 CFPropertyListRef	data);

SCPStatus	SCPSet				(SCPSessionRef		session,
						 CFStringRef		key,
						 CFPropertyListRef	data);

SCPStatus	SCPRemove			(SCPSessionRef		session,
						 CFStringRef		key);

SCPStatus	SCPPathCreateUniqueChild	(SCPSessionRef		session,
						 CFStringRef		prefix,
						 CFStringRef		*newPath);

SCPStatus	SCPPathGetValue			(SCPSessionRef		session,
						 CFStringRef		path,
						 CFDictionaryRef	*value);

SCPStatus	SCPPathGetLink			(SCPSessionRef		session,
						 CFStringRef		path,
						 CFStringRef		*link);

SCPStatus	SCPPathSetValue			(SCPSessionRef		session,
						 CFStringRef		path,
						 CFDictionaryRef	value);

SCPStatus	SCPPathSetLink			(SCPSessionRef		session,
						 CFStringRef		path,
						 CFStringRef		link);

SCPStatus	SCPPathRemove			(SCPSessionRef		session,
						 CFStringRef		path);

CFStringRef	SCPNotificationKeyCreate	(CFStringRef		prefsID,
						 int			keyType);

CFStringRef	SCPUserNotificationKeyCreate	(CFStringRef		prefsID,
						 CFStringRef		user,
						 int			keyType);

const char *	SCPError			(SCPStatus		status);

/*
 * console user APIs
 */

CFStringRef	SCDKeyCreateConsoleUser		();

SCDStatus	SCDConsoleUserGet		(char			*user,
						 int			userlen,
						 uid_t			*uid,
						 gid_t			*gid);

SCDStatus	SCDConsoleUserSet		(const char		*user,
						 uid_t			uid,
						 gid_t			gid);

/*
 * host name APIs
 */

CFStringRef	SCDKeyCreateHostName		();

SCDStatus	SCDHostNameGet			(CFStringRef		*name,
						 CFStringEncoding	*nameEncoding);

/*
 * network reachability APIs
 */
SCNStatus	SCNIsReachableByAddress		(const struct sockaddr	*address,
						 const int		addrlen,
						 int			*flags,
						 const char		**errorMessage);

SCNStatus	SCNIsReachableByName		(const char		*nodename,
						 int			*flags,
						 const char		**errorMessage);

__END_DECLS

#endif	/* _V1COMPATIBILITY_H */
