/*
 * Copyright (c) 2000-2009 Apple Inc. All rights reserved.
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

#ifndef _SCPRIVATE_H
#define _SCPRIVATE_H

#include <sys/cdefs.h>
#include <sys/socket.h>
#include <asl.h>
#include <sys/syslog.h>
#include <mach/message.h>

#include <CoreFoundation/CoreFoundation.h>

/* SCDynamicStore SPIs */
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecificPrivate.h>
#include <SystemConfiguration/SCDynamicStoreSetSpecificPrivate.h>

/* SCPreferences SPIs */
#include <SystemConfiguration/SCPreferencesPrivate.h>
#include <SystemConfiguration/SCPreferencesGetSpecificPrivate.h>
#include <SystemConfiguration/SCPreferencesSetSpecificPrivate.h>

/* [private] Schema Definitions (for SCDynamicStore and SCPreferences) */
#include <SystemConfiguration/SCSchemaDefinitionsPrivate.h>

/* SCNetworkConfiguration SPIs */
#include <SystemConfiguration/SCNetworkConfigurationPrivate.h>

/* SCNetworkConnection SPIs */
#include <SystemConfiguration/SCNetworkConnectionPrivate.h>

/* Keychain SPIs */
#include <SystemConfiguration/SCPreferencesKeychainPrivate.h>

/*!
	@header SCPrivate
 */

/* framework variables */
extern int	_sc_debug;	/* non-zero if debugging enabled */
extern int	_sc_verbose;	/* non-zero if verbose logging enabled */
extern int	_sc_log;	/* 0 if SC messages should be written to stdout/stderr,
				   1 if SC messages should be logged w/asl(3),
				   2 if SC messages should be written to stdout/stderr AND logged */

/*!
	@group SCNetworkReachabilityCreateWithOptions #defines
	@discussion The following defines the keys and values that can
		be passed to the SCNetworkReachabilityCreateWithOptions
		API.
 */

/*!
	@constant kSCNetworkReachabilityOptionNodeName
	@discussion A CFString that will be passed to getaddrinfo(3).  An acceptable
		value is either a valid host name or a numeric host address string
		consisting of a dotted decimal IPv4 address or an IPv6 address.
 */
#define kSCNetworkReachabilityOptionNodeName	CFSTR("nodename")

/*!
	@constant kSCNetworkReachabilityOptionServName
	@discussion A CFString that will be passed to getaddrinfo(3).  An acceptable
		value is either a decimal port number or a service name listed in
		services(5).
 */
#define kSCNetworkReachabilityOptionServName	CFSTR("servname")

/*!
	@constant kSCNetworkReachabilityOptionHints
	@discussion A CFData wrapping a "struct addrinfo" that will be passed to
		getaddrinfo(3).  The caller can supply any of the ai_family,
		ai_socktype, ai_protocol, and ai_flags structure elements.  All
		other elements must be 0 or the null pointer.
 */
#define kSCNetworkReachabilityOptionHints	CFSTR("hints")

/*!
	@constant kSCNetworkReachabilityOptionConnectionOnDemandByPass
	@discussion A CFBoolean that indicates if we should bypass the VPNOnDemand
		checks for this target.
 */
#define kSCNetworkReachabilityOptionConnectionOnDemandByPass	CFSTR("ConnectionOnDemandByPass")

/*!
	@group
 */

__BEGIN_DECLS

/*!
	@function _SCErrorSet
	@discussion Sets the last SystemConfiguration.framework API error code.
	@param error The error encountered.
 */
void		_SCErrorSet			(int			error);

/*!
	@function _SCSerialize
	@discussion Serialize a CFPropertyList object for passing
		to/from configd.
	@param obj CFPropertyList object to serialize
	@param xml A pointer to a CFDataRef, NULL if data should be
		vm_allocated.
	@param dataRef A pointer to the newly allocated/serialized data
	@param dataLen A pointer to the length in bytes of the newly
		allocated/serialized data
 */
Boolean		_SCSerialize			(CFPropertyListRef	obj,
						 CFDataRef		*xml,
						 void			**dataRef,
						 CFIndex		*dataLen);

/*!
	@function _SCUnserialize
	@discussion Unserialize a stream of bytes passed from/to configd
		into a CFPropertyList object.
	@param obj A pointer to memory that will be filled with the CFPropertyList
		associated with the stream of bytes.
	@param xml CFDataRef with the serialized data
	@param dataRef A pointer to the serialized data
	@param dataLen A pointer to the length of the serialized data

	Specify either "xml" or "data/dataLen".
 */
Boolean		_SCUnserialize			(CFPropertyListRef	*obj,
						 CFDataRef		xml,
						 void			*dataRef,
						 CFIndex		dataLen);

/*!
	@function _SCSerializeString
	@discussion Serialize a CFString object for passing
		to/from configd.
	@param str CFString key to serialize
	@param data A pointer to a CFDataRef, NULL if storage should be
		vm_allocated.
	@param dataRef A pointer to the newly allocated/serialized data
	@param dataLen A pointer to the length in bytes of the newly
		allocated/serialized data
 */
Boolean		_SCSerializeString		(CFStringRef		str,
						 CFDataRef		*data,
						 void			**dataRef,
						 CFIndex		*dataLen);

/*!
	@function _SCUnserializeString
	@discussion Unserialize a stream of bytes passed from/to configd
		into a CFString object.
	@param str A pointer to memory that will be filled with the CFString
		associated with the stream of bytes.
	@param utf8 CFDataRef with the serialized data
	@param dataRef A pointer to the serialized data
	@param dataLen A pointer to the length of the serialized data

	Specify either "utf8" or "data/dataLen".
 */
Boolean		_SCUnserializeString		(CFStringRef		*str,
						 CFDataRef		utf8,
						 void			*dataRef,
						 CFIndex		dataLen);

/*!
	@function _SCSerializeData
	@discussion Serialize a CFData object for passing
		to/from configd.
	@param data CFData key to serialize
	@param dataRef A pointer to the newly allocated/serialized data
	@param dataLen A pointer to the length in bytes of the newly
		allocated/serialized data
 */
Boolean		_SCSerializeData		(CFDataRef		data,
						 void			**dataRef,
						 CFIndex		*dataLen);

/*!
	@function _SCUnserializeData
	@discussion Unserialize a stream of bytes passed from/to configd
		into a CFData object.
	@param data A pointer to memory that will be filled with the CFData
		associated with the stream of bytes.
	@param dataRef A pointer to the serialized data
	@param dataLen A pointer to the length of the serialized data
 */
Boolean		_SCUnserializeData		(CFDataRef		*data,
						 void			*dataRef,
						 CFIndex		dataLen);

/*!
	@function _SCSerializeMultiple
	@discussion Convert a CFDictionary containing a set of CFPropertlyList
		values into a CFDictionary containing a set of serialized CFData
		values.
	@param dict The CFDictionary with CFPropertyList values.
	@result The serialized CFDictionary with CFData values
 */
CFDictionaryRef	_SCSerializeMultiple		(CFDictionaryRef	dict);

/*!
	@function _SCUnserializeMultiple
	@discussion Convert a CFDictionary containing a set of CFData
		values into a CFDictionary containing a set of serialized
		CFPropertlyList values.
	@param dict The CFDictionary with CFData values.
	@result The serialized CFDictionary with CFPropertyList values
 */
CFDictionaryRef	_SCUnserializeMultiple		(CFDictionaryRef	dict);

/*!
	@function _SC_cfstring_to_cstring
	@discussion Extracts a C-string from a CFString.
	@param cfstr The CFString to extract the data from.
	@param buf A user provided buffer of the specified length.  If NULL,
		a new buffer will be allocated to contain the C-string.  It
		is the responsiblity of the caller to free an allocated
		buffer.
	@param bufLen The size of the user provided buffer.
	@param encoding The string encoding
	@result If the extraction (conversion) is successful then a pointer
		to the user provided (or allocated) buffer is returned, NULL
		if the string could not be extracted.
 */
char *		_SC_cfstring_to_cstring		(CFStringRef		cfstr,
						 char			*buf,
						 CFIndex		bufLen,
						 CFStringEncoding	encoding);

/*!
 *      @function _SC_sockaddr_to_string
 *      @discussion Formats a "struct sockaddr" for reporting
 *      @param address The address to format
 *	@param buf A user provided buffer of the specified length.
 *	@param bufLen The size of the user provided buffer.
 */
void		_SC_sockaddr_to_string		(const struct sockaddr  *address,
						 char			*buf,
						 size_t			bufLen);

/*!
	@function _SC_sendMachMessage
	@discussion Sends a trivial mach message (one with just a
		message ID) to the specified port.
	@param port The mach port.
	@param msg_id The message id.
 */
void		_SC_sendMachMessage		(mach_port_t		port,
						 mach_msg_id_t		msg_id);


/*!
	@function _SCCopyDescription
	@discussion Returns a formatted textual description of a CF object.
	@param cf The CFType object (a generic reference of type CFTypeRef) from
		which to derive a description.
	@param formatOptions A dictionary containing formatting options for the object.
	@result A string that contains a formatted description of cf.
 */
CFStringRef	_SCCopyDescription		(CFTypeRef		cf,
						 CFDictionaryRef	formatOptions);


/*!
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


/*!
	@function SCLOG
	@discussion Issue a log message.
	@param asl An asl client handle to be used for logging. If NULL, a shared
		handle will be used.
	@param msg An asl msg structure to be used for logging. If NULL, a default
		asl msg will be used.
	@param level A asl(3) logging priority. Passing the complement of a logging
		priority (e.g. ~ASL_LEVEL_NOTICE) will result in log message lines
		NOT being split by a "\n".
	@param formatString The format string
	@result The specified message will be written to the system message
		logger (See syslogd(8)).
 */
void		SCLOG				(aslclient		asl,
						 aslmsg			msg,
						 int			level,
						 CFStringRef		formatString,
						 ...);


/*!
	@function SCPrint
	@discussion Conditionally issue a debug message.
	@param condition A boolean value indicating if the message should be written
	@param stream The output stream for the log message.
	@param formatString The format string
	@result The message will be written to the specified stream
		stream.
 */
void		SCPrint				(Boolean		condition,
						 FILE			*stream,
						 CFStringRef		formatString,
						 ...);

/*!
	@function SCTrace
	@discussion Conditionally issue a debug message with a time stamp.
	@param condition A boolean value indicating if the message should be written
	@param stream The output stream for the log message.
	@param formatString The format string
	@result The message will be written to the specified stream
		stream.
 */
void		SCTrace				(Boolean		condition,
						 FILE			*stream,
						 CFStringRef		formatString,
						 ...);

/*!
	@function SCNetworkReachabilityCopyOnDemandService
	@discussion For target hosts that require an OnDemand connection, returns
		the SCNetworkService associated with the connection and user
		options to use with SCNetworkConnectionStart.
	@result The SCNetworkService for the target; NULL if there is
		no associated OnDemand service.
 */
SCNetworkServiceRef
SCNetworkReachabilityCopyOnDemandService	(SCNetworkReachabilityRef	target,
						 CFDictionaryRef		*userOptions);

/*!
	@function SCNetworkReachabilityCopyResolvedAddress
	@discussion Return the resolved addresses associated with the
		target host.
	@result A CFArray[CFData], where each CFData is a (struct sockaddr)
 */
CFArrayRef
SCNetworkReachabilityCopyResolvedAddress	(SCNetworkReachabilityRef	target,
						 int				*error_num);

/*!
	@function SCNetworkReachabilityCreateWithOptions
	@discussion Creates a reference to a specified network host.  The
		options allow the caller to specify the node name and/or
		the service name.  This reference can be used later to
		monitor the reachability of the target host.
	@param allocator The CFAllocator that should be used to allocate
		memory for the SCNetworkReachability object.
		This parameter may be NULL in which case the current
		default CFAllocator is used. If this reference is not
		a valid CFAllocator, the behavior is undefined.
	@param options A CFDictionary containing options specifying the
		network host.  The options reflect the arguments that would
		be passed to getaddrinfo().
  */
SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithOptions		(CFAllocatorRef		allocator,
						 CFDictionaryRef	options);

/*!
	@function _SC_checkResolverReachabilityByAddress
	@discussion Check the reachability of a reverse DNS query
 */
Boolean
_SC_checkResolverReachabilityByAddress		(SCDynamicStoreRef		*storeP,
						 SCNetworkReachabilityFlags	*flags,
						 Boolean			*haveDNS,
						 struct sockaddr		*sa);

#if	!TARGET_OS_IPHONE
/*
 * DOS encoding/codepage
 */
void
_SC_dos_encoding_and_codepage			(CFStringEncoding	macEncoding,
						 UInt32			macRegion,
						 CFStringEncoding	*dosEncoding,
						 UInt32			*dosCodepage);
#endif	// !TARGET_OS_IPHONE

/*
 * object / CFRunLoop  management
 */
void
_SC_signalRunLoop				(CFTypeRef		obj,
						 CFRunLoopSourceRef     rls,
						 CFArrayRef		rlList);

Boolean
_SC_isScheduled					(CFTypeRef		obj,
						 CFRunLoopRef		runLoop,
						 CFStringRef		runLoopMode,
						 CFMutableArrayRef      rlList);

void
_SC_schedule					(CFTypeRef		obj,
						 CFRunLoopRef		runLoop,
						 CFStringRef		runLoopMode,
						 CFMutableArrayRef      rlList);

Boolean
_SC_unschedule					(CFTypeRef		obj,
						 CFRunLoopRef		runLoop,
						 CFStringRef		runLoopMode,
						 CFMutableArrayRef      rlList,
						 Boolean		all);

/*
 * bundle access
 */
CFBundleRef
_SC_CFBundleGet					(void);

CFStringRef
_SC_CFBundleCopyNonLocalizedString		(CFBundleRef		bundle,
						 CFStringRef		key,
						 CFStringRef		value,
						 CFStringRef		tableName);

/*
 * misc
 */
static __inline__ Boolean
_SC_CFEqual(CFTypeRef val1, CFTypeRef val2)
{
	if (val1 == val2) {
	    return TRUE;
	}
	if (val1 != NULL && val2 != NULL) {
		return CFEqual(val1, val2);
	}
	return FALSE;
}

/*
 * debugging
 */

#ifdef	DEBUG_MACH_PORT_ALLOCATIONS
	#define __MACH_PORT_DEBUG(cond, str, port)		\
	do {							\
		if (cond) _SC_logMachPortReferences(str, port);	\
	} while (0)
#else	// DEBUG_MACH_PORT_ALLOCATIONS
	#define __MACH_PORT_DEBUG(cond, str, port)
#endif	// DEBUG_MACH_PORT_ALLOCATIONS

void
_SC_logMachPortStatus				(void);

void
_SC_logMachPortReferences			(const char		*str,
						 mach_port_t		port);

CFStringRef
_SC_copyBacktrace				(void);

__END_DECLS

#endif	/* _SCPRIVATE_H */
