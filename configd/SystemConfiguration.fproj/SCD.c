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
#include <pthread.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */

/* framework variables */
Boolean	_sc_debug	= FALSE;	/* TRUE if debugging enabled */
Boolean	_sc_verbose	= FALSE;	/* TRUE if verbose logging enabled */
Boolean	_sc_log		= TRUE;		/* TRUE if SCLog() output goes to syslog */

static const struct sc_errmsg {
	int	status;
	char	*message;
} sc_errmsgs[] = {
	{ kSCStatusAccessError,		"Permission denied" },
	{ kSCStatusFailed,		"Failed!" },
	{ kSCStatusInvalidArgument,	"Invalid argument" },
	{ kSCStatusKeyExists,		"Key already defined" },
	{ kSCStatusLocked,		"Lock already held" },
	{ kSCStatusNeedLock,		"Lock required for this operation" },
	{ kSCStatusNoStoreServer,	"Configuration daemon not (no longer) available" },
	{ kSCStatusNoStoreSession,	"Configuration daemon session not active" },
	{ kSCStatusNoConfigFile,	"Configuration file not found" },
	{ kSCStatusNoKey,		"No such key" },
	{ kSCStatusNoLink,		"No such link" },
	{ kSCStatusNoPrefsSession,	"Preference session not active" },
	{ kSCStatusNotifierActive,	"Notifier is currently active" },
	{ kSCStatusOK,			"Success!" },
	{ kSCStatusPrefsBusy,		"Configuration daemon busy" },
	{ kSCStatusReachabilityUnknown,	"Network reachability cannot be determined" },
	{ kSCStatusStale,		"Write attempted on stale version of object" },
};
#define nSC_ERRMSGS (sizeof(sc_errmsgs)/sizeof(struct sc_errmsg))


#define	USE_SCCOPYDESCRIPTION
#ifdef	USE_SCCOPYDESCRIPTION

// from <CoreFoundation/CFVeryPrivate.h>
extern CFStringRef _CFStringCreateWithFormatAndArgumentsAux(CFAllocatorRef alloc, CFStringRef (*copyDescFunc)(void *, CFDictionaryRef), CFDictionaryRef formatOptions, CFStringRef format, va_list arguments);

static CFStringRef
_SCCopyDescription(void *info, CFDictionaryRef formatOptions)
{
	CFMutableDictionaryRef	nFormatOptions;
	CFStringRef		prefix1;
	CFStringRef		prefix2;
	CFTypeID		type	= CFGetTypeID(info);

	if (!formatOptions ||
	    !CFDictionaryGetValueIfPresent(formatOptions, CFSTR("PREFIX1"), (void **)&prefix1)) {
		prefix1 = CFSTR("");
	}

	if (type == CFStringGetTypeID()) {
		return CFStringCreateWithFormat(NULL,
						formatOptions,
						CFSTR("%@%@"),
						prefix1,
						info);
	}

	if (type == CFBooleanGetTypeID()) {
		return CFStringCreateWithFormat(NULL,
						formatOptions,
						CFSTR("%@%s"),
						prefix1,
						CFBooleanGetValue(info) ? "TRUE" : "FALSE");
	}

	if (type == CFDataGetTypeID()) {
		const u_int8_t		*data;
		CFIndex			dataLen;
		CFIndex			i;
		CFMutableStringRef	str;

		str = CFStringCreateMutable(NULL, 0);
		CFStringAppendFormat(str, formatOptions, CFSTR("%@<data> 0x"), prefix1);

		data    = CFDataGetBytePtr(info);
		dataLen = CFDataGetLength(info);
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
						info);
	}

	if (type == CFDateGetTypeID()) {
		CFGregorianDate	gDate;
		CFStringRef	str;
		CFTimeZoneRef	tZone;

		tZone = CFTimeZoneCopySystem();
		gDate = CFAbsoluteTimeGetGregorianDate(CFDateGetAbsoluteTime(info), tZone);
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
	    !CFDictionaryGetValueIfPresent(formatOptions, CFSTR("PREFIX2"), (void **)&prefix2)) {
		prefix2 = CFStringCreateCopy(NULL, prefix1);
	}

	if (formatOptions) {
		nFormatOptions = CFDictionaryCreateMutableCopy(NULL, 0, formatOptions);
	} else {
		nFormatOptions = CFDictionaryCreateMutable(NULL,
							   0,
							   &kCFTypeDictionaryKeyCallBacks,
							   &kCFTypeDictionaryValueCallBacks);
	}

	if (type == CFArrayGetTypeID()) {
		void			**elements;
		CFIndex			i;
		CFIndex			nElements;
		CFMutableStringRef	str;

		str = CFStringCreateMutable(NULL, 0);
		CFStringAppendFormat(str, formatOptions, CFSTR("%@<array> {"), prefix1);

		nElements = CFArrayGetCount(info);
		elements  = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
		CFArrayGetValues(info, CFRangeMake(0, nElements), elements);
		for (i=0; i<nElements; i++) {
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

			vStr = _SCCopyDescription(elements[i], nFormatOptions);
			CFStringAppendFormat(str,
					     formatOptions,
					     CFSTR("\n%@"),
					     vStr);
			CFRelease(vStr);
		}
		CFAllocatorDeallocate(NULL, elements);
		CFStringAppendFormat(str, formatOptions, CFSTR("\n%@}"), prefix2);

		CFRelease(nFormatOptions);
		return str;
	}

	if (type == CFDictionaryGetTypeID()) {
		void			**keys;
		CFIndex			i;
		CFIndex			nElements;
		CFMutableStringRef	nPrefix1;
		CFMutableStringRef	nPrefix2;
		CFMutableStringRef	str;
		void			**values;

		str = CFStringCreateMutable(NULL, 0);
		CFStringAppendFormat(str, formatOptions, CFSTR("%@<dictionary> {"), prefix1);

		nElements = CFDictionaryGetCount(info);
		keys      = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
		values    = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
		CFDictionaryGetKeysAndValues(info, keys, values);
		for (i=0; i<nElements; i++) {
			CFStringRef		kStr;
			CFStringRef		vStr;

			kStr = _SCCopyDescription(keys[i], NULL);

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

			vStr = _SCCopyDescription(values[i], nFormatOptions);
			CFStringAppendFormat(str,
					     formatOptions,
					     CFSTR("\n%@"),
					     vStr);
			CFRelease(vStr);
		}
		CFAllocatorDeallocate(NULL, keys);
		CFAllocatorDeallocate(NULL, values);
		CFStringAppendFormat(str, formatOptions, CFSTR("\n%@}"), prefix2);

		CFRelease(nFormatOptions);
		return str;
	}

	{
		CFStringRef	cfStr;
		CFStringRef	str;

		cfStr = CFCopyDescription(info);
		str = CFStringCreateWithFormat(NULL,
					       formatOptions,
					       CFSTR("%@%@"),
					       prefix1,
					       cfStr);
		CFRelease(cfStr);
		return str;
	}
}

#endif	/* USE_SCCOPYDESCRIPTION */


static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


__private_extern__ void
__SCLog(int level, CFStringRef str)
{
	CFArrayRef	lines;

	lines = CFStringCreateArrayBySeparatingStrings(NULL, str, CFSTR("\n"));
	if (lines) {
		int		i;

		pthread_mutex_lock(&lock);
		for (i=0; i<CFArrayGetCount(lines); i++) {
			CFDataRef	line;

			line = CFStringCreateExternalRepresentation(NULL,
								    CFArrayGetValueAtIndex(lines, i),
								    kCFStringEncodingMacRoman,
								    '?');
			if (line) {
				syslog (level, "%.*s", (int)CFDataGetLength(line), CFDataGetBytePtr(line));
				CFRelease(line);
			}
		}
		pthread_mutex_unlock(&lock);
		CFRelease(lines);
	}

	return;
}


__private_extern__ void
__SCPrint(FILE *stream, CFStringRef str)
{
	CFDataRef	line;

	line = CFStringCreateExternalRepresentation(NULL,
						    str,
						    kCFStringEncodingMacRoman,
						    '?');
	if (line) {
		pthread_mutex_lock(&lock);
		fprintf(stream, "%.*s", (int)CFDataGetLength(line), CFDataGetBytePtr(line));
		fflush (stream);
		pthread_mutex_unlock(&lock);
		CFRelease(line);
	}

	return;
}


void
SCLog(Boolean condition, int level, CFStringRef formatString, ...)
{
	va_list		argList;
	CFStringRef	resultString;

	if (!condition) {
		return;
	}

	va_start(argList, formatString);
#ifdef	USE_SCCOPYDESCRIPTION
	resultString = _CFStringCreateWithFormatAndArgumentsAux(NULL,
								_SCCopyDescription,
								NULL,
								formatString,
								argList);
#else	/* USE_SCCOPYDESCRIPTION */
	resultString = CFStringCreateWithFormatAndArguments(NULL, NULL, formatString, argList);
#endif	/* !USE_SCCOPYDESCRIPTION */
	va_end(argList);

	if (_sc_log) {
		__SCLog(level, resultString);
	} else {
		FILE		*f = (LOG_PRI(level) > LOG_NOTICE) ? stderr : stdout;
		CFStringRef	newString;

		/* add a new-line */
		newString = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@\n"), resultString);
	       __SCPrint(f, newString);
		CFRelease(newString);
	}
	CFRelease(resultString);
	return;
}


void
SCPrint(Boolean condition, FILE *stream, CFStringRef formatString, ...)
{
	va_list		argList;
	CFStringRef	resultString;

	if (!condition) {
		return;
	}

	va_start(argList, formatString);
#ifdef	USE_SCCOPYDESCRIPTION
	resultString = _CFStringCreateWithFormatAndArgumentsAux(NULL,
								_SCCopyDescription,
								NULL,
								formatString,
								argList);
#else	/* USE_SCCOPYDESCRIPTION */
	resultString = CFStringCreateWithFormatAndArguments(NULL, NULL, formatString, argList);
#endif	/* !USE_SCCOPYDESCRIPTION */
	va_end(argList);

	__SCPrint(stream, resultString);
	CFRelease(resultString);
	return;
}


typedef struct {
	int	_sc_error;
} __SCThreadSpecificData, *__SCThreadSpecificDataRef;


static pthread_once_t	tsKeyInitialized	= PTHREAD_ONCE_INIT;
static pthread_key_t	tsDataKey		= NULL;


static void
__SCThreadSpecificDataFinalize(void *arg)
{
	__SCThreadSpecificDataRef	tsd = (__SCThreadSpecificDataRef)arg;

	if (!tsd) return;

	CFAllocatorDeallocate(kCFAllocatorSystemDefault, tsd);
	return;
}


static void
__SCThreadSpecificKeyInitialize()
{
	pthread_key_create(&tsDataKey, __SCThreadSpecificDataFinalize);
	return;
}


void
_SCErrorSet(int error)
{
	__SCThreadSpecificDataRef	tsd;

	pthread_once(&tsKeyInitialized, __SCThreadSpecificKeyInitialize);

	tsd = pthread_getspecific(tsDataKey);
	if (!tsd) {
		tsd = CFAllocatorAllocate(kCFAllocatorSystemDefault, sizeof(__SCThreadSpecificData), 0);
		bzero(tsd, sizeof(__SCThreadSpecificData));
		pthread_setspecific(tsDataKey, tsd);
	}

	tsd->_sc_error = error;
	return;
}


int
SCError()
{
	__SCThreadSpecificDataRef	tsd;

	pthread_once(&tsKeyInitialized, __SCThreadSpecificKeyInitialize);

	tsd = pthread_getspecific(tsDataKey);
	return tsd ? tsd->_sc_error : kSCStatusOK;
}


const char *
SCErrorString(int status)
{
	int i;

	for (i = 0; i < nSC_ERRMSGS; i++) {
		if (sc_errmsgs[i].status == status) {
			return sc_errmsgs[i].message;
		}
	}

	if ((status > 0) && (status <= ELAST)) {
		return strerror(status);
	}

	return mach_error_string(status);
}
