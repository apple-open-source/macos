/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <pthread.h>
#include <sys/time.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */


/* framework variables */
int	_sc_debug	= FALSE;	/* non-zero if debugging enabled */
int	_sc_verbose	= FALSE;	/* non-zero if verbose logging enabled */
int	_sc_log		= TRUE;		/* 0 if SC messages should be written to stdout/stderr,
					   1 if SC messages should be logged w/asl(3),
					   2 if SC messages should be written to stdout/stderr AND logged */


#pragma mark -
#pragma mark Thread specific data


typedef struct {
	aslclient	_asl;
	int		_sc_error;
} __SCThreadSpecificData, *__SCThreadSpecificDataRef;


static pthread_once_t	tsKeyInitialized	= PTHREAD_ONCE_INIT;
static pthread_key_t	tsDataKey;


static void
__SCThreadSpecificDataFinalize(void *arg)
{
	__SCThreadSpecificDataRef	tsd = (__SCThreadSpecificDataRef)arg;

	if (tsd != NULL) {
		if (tsd->_asl    != NULL) asl_close(tsd->_asl);
		CFAllocatorDeallocate(kCFAllocatorSystemDefault, tsd);
	}
	return;
}


static void
__SCThreadSpecificKeyInitialize()
{
	pthread_key_create(&tsDataKey, __SCThreadSpecificDataFinalize);
	return;
}


static __SCThreadSpecificDataRef
__SCGetThreadSpecificData()
{
	__SCThreadSpecificDataRef	tsd;

	pthread_once(&tsKeyInitialized, __SCThreadSpecificKeyInitialize);

	tsd = pthread_getspecific(tsDataKey);
	if (tsd == NULL) {
		tsd = CFAllocatorAllocate(kCFAllocatorSystemDefault, sizeof(__SCThreadSpecificData), 0);
		tsd->_asl = asl_open(NULL, NULL, 0);
		asl_set_filter(tsd->_asl, ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG));
		tsd->_sc_error = kSCStatusOK;
		pthread_setspecific(tsDataKey, tsd);
	}

	return tsd;
}


#pragma mark -
#pragma mark Logging


#define	ENABLE_SC_FORMATTING
#ifdef	ENABLE_SC_FORMATTING
// from <CoreFoundation/ForFoundationOnly.h>
extern CFStringRef _CFStringCreateWithFormatAndArgumentsAux(CFAllocatorRef alloc, CFStringRef (*copyDescFunc)(CFTypeRef, CFDictionaryRef), CFDictionaryRef formatOptions, CFStringRef format, va_list arguments);
#endif	/* ENABLE_SC_FORMATTING */


CFStringRef
_SCCopyDescription(CFTypeRef cf, CFDictionaryRef formatOptions)
{
#ifdef	ENABLE_SC_FORMATTING
	CFMutableDictionaryRef	nFormatOptions;
	CFStringRef		prefix1;
	CFStringRef		prefix2;
	CFTypeID		type	= CFGetTypeID(cf);

	if (!formatOptions ||
	    !CFDictionaryGetValueIfPresent(formatOptions, CFSTR("PREFIX1"), (const void **)&prefix1)) {
		prefix1 = CFSTR("");
	}

	if (type == CFStringGetTypeID()) {
		return CFStringCreateWithFormat(NULL,
						formatOptions,
						CFSTR("%@%@"),
						prefix1,
						cf);
	}

	if (type == CFBooleanGetTypeID()) {
		return CFStringCreateWithFormat(NULL,
						formatOptions,
						CFSTR("%@%s"),
						prefix1,
						CFBooleanGetValue(cf) ? "TRUE" : "FALSE");
	}

	if (type == CFDataGetTypeID()) {
		const uint8_t		*data;
		CFIndex			dataLen;
		CFIndex			i;
		CFMutableStringRef	str;

		str = CFStringCreateMutable(NULL, 0);
		CFStringAppendFormat(str, formatOptions, CFSTR("%@<data> 0x"), prefix1);

		data    = CFDataGetBytePtr(cf);
		dataLen = CFDataGetLength(cf);
		for (i = 0; i < dataLen; i++) {
			CFStringAppendFormat(str, NULL, CFSTR("%02x"), data[i]);
		}

		return str;
	}

	if (type == CFNumberGetTypeID()) {
		return CFStringCreateWithFormat(NULL,
						formatOptions,
						CFSTR("%@%@"),
						prefix1,
						cf);
	}

	if (type == CFDateGetTypeID()) {
		CFGregorianDate	gDate;
		CFStringRef	str;
		CFTimeZoneRef	tZone;

		tZone = CFTimeZoneCopySystem();
		gDate = CFAbsoluteTimeGetGregorianDate(CFDateGetAbsoluteTime(cf), tZone);
		str   = CFStringCreateWithFormat(NULL,
						formatOptions,
						CFSTR("%@%02d/%02d/%04d %02d:%02d:%02.0f %@"),
						prefix1,
						gDate.month,
						gDate.day,
						gDate.year,
						gDate.hour,
						gDate.minute,
						gDate.second,
						CFTimeZoneGetName(tZone));
		CFRelease(tZone);
		return str;
	}

	if (!formatOptions ||
	    !CFDictionaryGetValueIfPresent(formatOptions, CFSTR("PREFIX2"), (const void **)&prefix2)) {
		prefix2 = prefix1;
	}

	if (formatOptions) {
		nFormatOptions = CFDictionaryCreateMutableCopy(NULL, 0, formatOptions);
	} else {
		nFormatOptions = CFDictionaryCreateMutable(NULL,
							   0,
							   &kCFTypeDictionaryKeyCallBacks,
							   &kCFTypeDictionaryValueCallBacks);
	}

#define	N_QUICK	32

	if (type == CFArrayGetTypeID()) {
		const void *		elements_q[N_QUICK];
		const void **		elements	= elements_q;
		CFIndex			i;
		CFIndex			nElements;
		CFMutableStringRef	str;

		str = CFStringCreateMutable(NULL, 0);
		CFStringAppendFormat(str, formatOptions, CFSTR("%@<array> {"), prefix1);

		nElements = CFArrayGetCount(cf);
		if (nElements > 0) {
			if (nElements > (CFIndex)(sizeof(elements_q)/sizeof(CFTypeRef)))
				elements  = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			CFArrayGetValues(cf, CFRangeMake(0, nElements), elements);
			for (i = 0; i < nElements; i++) {
				CFMutableStringRef	nPrefix1;
				CFMutableStringRef	nPrefix2;
				CFStringRef		nStr;
				CFStringRef		vStr;

				nStr = CFStringCreateWithFormat(NULL, NULL, CFSTR("%u"), i);

				nPrefix1 = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(nPrefix1,
						     formatOptions,
						     CFSTR("%@  %@ : "),
						     prefix2,
						     nStr);
				nPrefix2 = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(nPrefix2,
						     formatOptions,
						     CFSTR("%@  "),
						     prefix2);

				CFDictionarySetValue(nFormatOptions, CFSTR("PREFIX1"), nPrefix1);
				CFDictionarySetValue(nFormatOptions, CFSTR("PREFIX2"), nPrefix2);
				CFRelease(nPrefix1);
				CFRelease(nPrefix2);
				CFRelease(nStr);

				vStr = _SCCopyDescription((CFTypeRef)elements[i], nFormatOptions);
				CFStringAppendFormat(str,
						     formatOptions,
						     CFSTR("\n%@"),
						     vStr);
				CFRelease(vStr);
			}
			if (elements != elements_q) CFAllocatorDeallocate(NULL, elements);
		}
		CFStringAppendFormat(str, formatOptions, CFSTR("\n%@}"), prefix2);

		CFRelease(nFormatOptions);
		return str;
	}

	if (type == CFDictionaryGetTypeID()) {
		const void *		keys_q[N_QUICK];
		const void **		keys	= keys_q;
		CFIndex			i;
		CFIndex			nElements;
		CFMutableStringRef	nPrefix1;
		CFMutableStringRef	nPrefix2;
		CFMutableStringRef	str;
		const void *		values_q[N_QUICK];
		const void **		values	= values_q;

		str = CFStringCreateMutable(NULL, 0);
		CFStringAppendFormat(str, formatOptions, CFSTR("%@<dictionary> {"), prefix1);

		nElements = CFDictionaryGetCount(cf);
		if (nElements > 0) {
			if (nElements > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
				keys   = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
				values = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			}
			CFDictionaryGetKeysAndValues(cf, keys, values);
			for (i = 0; i < nElements; i++) {
				CFStringRef		kStr;
				CFStringRef		vStr;

				kStr = _SCCopyDescription((CFTypeRef)keys[i], NULL);

				nPrefix1 = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(nPrefix1,
						     formatOptions,
						     CFSTR("%@  %@ : "),
						     prefix2,
						     kStr);
				nPrefix2 = CFStringCreateMutable(NULL, 0);
				CFStringAppendFormat(nPrefix2,
						     formatOptions,
						     CFSTR("%@  "),
						     prefix2);

				CFDictionarySetValue(nFormatOptions, CFSTR("PREFIX1"), nPrefix1);
				CFDictionarySetValue(nFormatOptions, CFSTR("PREFIX2"), nPrefix2);
				CFRelease(nPrefix1);
				CFRelease(nPrefix2);
				CFRelease(kStr);

				vStr = _SCCopyDescription((CFTypeRef)values[i], nFormatOptions);
				CFStringAppendFormat(str,
						     formatOptions,
						     CFSTR("\n%@"),
						     vStr);
				CFRelease(vStr);
			}
			if (keys != keys_q) {
				CFAllocatorDeallocate(NULL, keys);
				CFAllocatorDeallocate(NULL, values);
			}
		}
		CFStringAppendFormat(str, formatOptions, CFSTR("\n%@}"), prefix2);

		CFRelease(nFormatOptions);
		return str;
	}

	CFRelease(nFormatOptions);
#endif	/* ENABLE_SC_FORMATTING */

	return CFStringCreateWithFormat(NULL,
					formatOptions,
					CFSTR("%@%@"),
					prefix1,
					cf);
}


static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


static void
__SCLog(aslclient asl, aslmsg msg, int level, CFStringRef formatString, va_list formatArguments)
{
	CFDataRef	line;
	CFArrayRef	lines;
	CFStringRef	str;

	if (asl == NULL) {
		__SCThreadSpecificDataRef	tsd;

		tsd = __SCGetThreadSpecificData();
		asl = tsd->_asl;
	}

#ifdef	ENABLE_SC_FORMATTING
	str = _CFStringCreateWithFormatAndArgumentsAux(NULL,
						       _SCCopyDescription,
						       NULL,
						       formatString,
						       formatArguments);
#else	/* ENABLE_SC_FORMATTING */
	str =  CFStringCreateWithFormatAndArguments   (NULL,
						       NULL,
						       formatString,
						       formatArguments);
#endif	/* !ENABLE_SC_FORMATTING */

	if (level >= 0) {
		lines = CFStringCreateArrayBySeparatingStrings(NULL, str, CFSTR("\n"));
		if (lines != NULL) {
			int	i;
			int	n	= CFArrayGetCount(lines);

			for (i = 0; i < n; i++) {
				line = CFStringCreateExternalRepresentation(NULL,
									    CFArrayGetValueAtIndex(lines, i),
									    kCFStringEncodingUTF8,
									    (UInt8)'?');
				if (line) {
					asl_log(asl, msg, level, "%.*s", (int)CFDataGetLength(line), CFDataGetBytePtr(line));
					CFRelease(line);
				}
			}
			CFRelease(lines);
		}
	} else {
		line = CFStringCreateExternalRepresentation(NULL,
							    str,
							    kCFStringEncodingUTF8,
							    (UInt8)'?');
		if (line) {
			asl_log(asl, msg, ~level, "%.*s", (int)CFDataGetLength(line), CFDataGetBytePtr(line));
			CFRelease(line);
		}
	}
	CFRelease(str);

	return;
}


static void
__SCPrint(FILE *stream, CFStringRef formatString, va_list formatArguments, Boolean trace, Boolean addNL)
{
	CFDataRef	line;
	CFStringRef	str;

#ifdef	ENABLE_SC_FORMATTING
	str = _CFStringCreateWithFormatAndArgumentsAux(NULL,
						       _SCCopyDescription,
						       NULL,
						       formatString,
						       formatArguments);
#else	/* ENABLE_SC_FORMATTING */
	str =  CFStringCreateWithFormatAndArguments   (NULL,
						       NULL,
						       formatString,
						       formatArguments);
#endif	/* !ENABLE_SC_FORMATTING */

	line = CFStringCreateExternalRepresentation(NULL,
						    str,
						    kCFStringEncodingUTF8,
						    (UInt8)'?');
	CFRelease(str);
	if (!line) {
		return;
	}

	pthread_mutex_lock(&lock);
	if (trace) {
		struct tm	tm_now;
		struct timeval	tv_now;

		(void)gettimeofday(&tv_now, NULL);
		(void)localtime_r(&tv_now.tv_sec, &tm_now);
		(void)fprintf(stream, "%2d:%02d:%02d.%03d ",
			      tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, tv_now.tv_usec / 1000);
	}
	(void)fwrite((const void *)CFDataGetBytePtr(line), (size_t)CFDataGetLength(line), 1, stream);
	if (addNL) {
		(void)fputc('\n', stream);
	}
	fflush (stream);
	pthread_mutex_unlock(&lock);
	CFRelease(line);

	return;
}


void
SCLog(Boolean condition, int level, CFStringRef formatString, ...)
{
	va_list		formatArguments;

	if (!condition) {
		return;
	}

	va_start(formatArguments, formatString);
	if (_sc_log > 0) {
		__SCLog(NULL, NULL, level, formatString, formatArguments);
	}
	if (_sc_log != 1) {
		__SCPrint((LOG_PRI(level) > LOG_NOTICE) ? stderr : stdout,
			  formatString,
			  formatArguments,
			  (_sc_log > 0),	// trace
			  TRUE);		// add newline
	}
	va_end(formatArguments);

	return;
}


void
SCLOG(aslclient asl, aslmsg msg, int level, CFStringRef formatString, ...)
{
	va_list		formatArguments;

	va_start(formatArguments, formatString);
	if (_sc_log > 0) {
		__SCLog(asl, msg, level, formatString, formatArguments);
	}
	if (_sc_log != 1) {
		if (level < 0) {
			level = ~level;
		}
		__SCPrint((level > ASL_LEVEL_NOTICE) ? stderr : stdout,
			  formatString,
			  formatArguments,
			  (_sc_log > 0),	// trace
			  TRUE);		// add newline
	}
	va_end(formatArguments);

	return;
}


void
SCPrint(Boolean condition, FILE *stream, CFStringRef formatString, ...)
{
	va_list		formatArguments;

	if (!condition) {
		return;
	}

	va_start(formatArguments, formatString);
	__SCPrint(stream, formatString, formatArguments, FALSE, FALSE);
	va_end(formatArguments);

	return;
}


void
SCTrace(Boolean condition, FILE *stream, CFStringRef formatString, ...)
{
	va_list		formatArguments;

	if (!condition) {
		return;
	}

	va_start(formatArguments, formatString);
	__SCPrint(stream, formatString, formatArguments, TRUE, FALSE);
	va_end(formatArguments);

	return;
}


#pragma mark -
#pragma mark SC error handling / logging


const CFStringRef kCFErrorDomainSystemConfiguration	= CFSTR("com.apple.SystemConfiguration");


static const struct sc_errmsg {
	int	status;
	char	*message;
} sc_errmsgs[] = {
	{ kSCStatusAccessError,		"Permission denied" },
	{ kSCStatusConnectionNoService,	"Network service for connection not available" },
	{ kSCStatusFailed,		"Failed!" },
	{ kSCStatusInvalidArgument,	"Invalid argument" },
	{ kSCStatusKeyExists,		"Key already defined" },
	{ kSCStatusLocked,		"Lock already held" },
	{ kSCStatusMaxLink,		"Maximum link count exceeded" },
	{ kSCStatusNeedLock,		"Lock required for this operation" },
	{ kSCStatusNoStoreServer,	"Configuration daemon not (no longer) available" },
	{ kSCStatusNoStoreSession,	"Configuration daemon session not active" },
	{ kSCStatusNoConfigFile,	"Configuration file not found" },
	{ kSCStatusNoKey,		"No such key" },
	{ kSCStatusNoLink,		"No such link" },
	{ kSCStatusNoPrefsSession,	"Preference session not active" },
	{ kSCStatusNotifierActive,	"Notifier is currently active" },
	{ kSCStatusOK,			"Success!" },
	{ kSCStatusPrefsBusy,		"Preferences update currently in progress" },
	{ kSCStatusReachabilityUnknown,	"Network reachability cannot be determined" },
	{ kSCStatusStale,		"Write attempted on stale version of object" },
};
#define nSC_ERRMSGS (sizeof(sc_errmsgs)/sizeof(struct sc_errmsg))


void
_SCErrorSet(int error)
{
	__SCThreadSpecificDataRef	tsd;

	tsd = __SCGetThreadSpecificData();
	tsd->_sc_error = error;
	return;
}


CFErrorRef
SCCopyLastError(void)
{
	CFStringRef			domain;
	CFErrorRef			error;
	int				i;
	int				code;
	__SCThreadSpecificDataRef	tsd;
	CFMutableDictionaryRef		userInfo	= NULL;

	tsd = __SCGetThreadSpecificData();
	code =tsd->_sc_error;

	for (i = 0; i < (int)nSC_ERRMSGS; i++) {
		if (sc_errmsgs[i].status == code) {
			CFStringRef	str;

			domain = kCFErrorDomainSystemConfiguration;
			userInfo = CFDictionaryCreateMutable(NULL,
							     0,
							     &kCFCopyStringDictionaryKeyCallBacks,
							     &kCFTypeDictionaryValueCallBacks);
			str = CFStringCreateWithCString(NULL,
							sc_errmsgs[i].message,
							kCFStringEncodingASCII);
			CFDictionarySetValue(userInfo, kCFErrorDescriptionKey, str);
			CFRelease(str);
			goto done;
		}
	}

	if ((code > 0) && (code <= ELAST)) {
		domain = kCFErrorDomainPOSIX;
		goto done;
	}

	domain = kCFErrorDomainMach;

    done :

	error = CFErrorCreate(NULL, domain, code, userInfo);
	if (userInfo != NULL) CFRelease(userInfo);
	return error;
}


int
SCError(void)
{
	__SCThreadSpecificDataRef	tsd;

	tsd = __SCGetThreadSpecificData();
	return tsd->_sc_error;
}


const char *
SCErrorString(int status)
{
	int i;

	for (i = 0; i < (int)nSC_ERRMSGS; i++) {
		if (sc_errmsgs[i].status == status) {
			return sc_errmsgs[i].message;
		}
	}

	if ((status > 0) && (status <= ELAST)) {
		return strerror(status);
	}

	if ((status >= BOOTSTRAP_SUCCESS) && (status <= BOOTSTRAP_NO_MEMORY)) {
		return bootstrap_strerror(status);
	}

	return mach_error_string(status);
}
