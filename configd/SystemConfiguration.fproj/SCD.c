/*
 * Copyright (c) 2000-2008, 2010-2016 Apple Inc. All rights reserved.
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
#include <asl.h>
#include <pthread.h>
#include <sys/time.h>
#include <os/log.h>
#include <os/log_private.h>

#include "SCDynamicStoreInternal.h"
#include "SCD.h"
#include "config.h"		/* MiG generated file */

// asl logging
#define INSTALL_FACILITY	"install"
#define INSTALL_ENVIRONMENT	"__OSINSTALL_ENVIRONMENT"

// LIBASL SPI
extern asl_object_t	_asl_server_control_query(void);

/* framework variables */
int	_sc_debug	= FALSE;	/* non-zero if debugging enabled */
int	_sc_verbose	= FALSE;	/* non-zero if verbose logging enabled */
int	_sc_log		= TRUE;		/* 0 if SC messages should be written to stdout/stderr,
					 1 if SC messages should be logged w/asl(3),
					 2 if SC messages should be written to stdout/stderr AND logged */


#pragma mark -
#pragma mark Thread specific data


static pthread_once_t	tsKeyInitialized	= PTHREAD_ONCE_INIT;
static pthread_key_t	tsDataKey;


static void
__SCThreadSpecificDataFinalize(void *arg)
{
	__SCThreadSpecificDataRef	tsd = (__SCThreadSpecificDataRef)arg;

	if (tsd != NULL) {
		if (tsd->_asl != NULL) asl_release(tsd->_asl);
		if (tsd->_sc_store != NULL) CFRelease(tsd->_sc_store);
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


__private_extern__
__SCThreadSpecificDataRef
__SCGetThreadSpecificData()
{
	__SCThreadSpecificDataRef	tsd;
	pthread_once(&tsKeyInitialized, __SCThreadSpecificKeyInitialize);

	tsd = pthread_getspecific(tsDataKey);
	if (tsd == NULL) {
		tsd = CFAllocatorAllocate(kCFAllocatorSystemDefault, sizeof(__SCThreadSpecificData), 0);
		tsd->_asl = NULL;
		tsd->_sc_error = kSCStatusOK;
		tsd->_sc_store = NULL;
		pthread_setspecific(tsDataKey, tsd);
	}

	return tsd;
}


#pragma mark -
#pragma mark Logging


#define kASLModule		"ASLModule"
#define kASLOption		"ASLOption"
#define kLoggerID		"LoggerID"

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
		CFCalendarRef	calendar;
		CFStringRef	str;
		CFTimeZoneRef	tz;
		int		MM, DD, YYYY, hh, mm, ss;

		calendar = CFCalendarCreateWithIdentifier(NULL, kCFGregorianCalendar);
		tz = CFTimeZoneCopySystem();
		CFCalendarSetTimeZone(calendar, tz);
		CFRelease(tz);
		CFCalendarDecomposeAbsoluteTime(calendar,
						CFDateGetAbsoluteTime(cf),
						"MdyHms",
						&MM, &DD, &YYYY, &hh, &mm, &ss);
		CFRelease(calendar);

		str = CFStringCreateWithFormat(NULL,
					       formatOptions,
					       CFSTR("%@%02d/%02d/%04d %02d:%02d:%02d"),
					       prefix1,
					       MM, DD, YYYY, hh, mm, ss);
		return str;
	}

	if ((formatOptions == NULL) ||
	    !CFDictionaryGetValueIfPresent(formatOptions, CFSTR("PREFIX2"), (const void **)&prefix2)) {
		prefix2 = prefix1;
	}

	if (formatOptions != NULL) {
		nFormatOptions = CFDictionaryCreateMutableCopy(NULL, 0, formatOptions);
	} else {
		nFormatOptions = CFDictionaryCreateMutable(NULL,
							   0,
							   &kCFTypeDictionaryKeyCallBacks,
							   &kCFTypeDictionaryValueCallBacks);
	}
	assert(nFormatOptions != NULL);

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

				nStr = CFStringCreateWithFormat(NULL, NULL, CFSTR("%ld"), i);

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

		str = CFStringCreateMutable(NULL, 0);
		CFStringAppendFormat(str, formatOptions, CFSTR("%@<dictionary> {"), prefix1);

		nElements = CFDictionaryGetCount(cf);
		if (nElements > 0) {
			CFComparatorFunction	compFunc	= NULL;
			CFMutableArrayRef	sortedKeys;

			if (nElements > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
				keys = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			}
			CFDictionaryGetKeysAndValues(cf, keys, NULL);

			sortedKeys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			for (i = 0; i < nElements; i++) {
				CFArrayAppendValue(sortedKeys, (CFStringRef)keys[i]);
			}

			if (isA_CFString(keys[0])) {
				compFunc = (CFComparatorFunction)CFStringCompare;
			}
			else if (isA_CFNumber(keys[0])) {
				compFunc = (CFComparatorFunction)CFNumberCompare;
			}
			else if (isA_CFDate(keys[0])) {
				compFunc = (CFComparatorFunction)CFDateCompare;
			}

			if (compFunc != NULL) {
				CFArraySortValues(sortedKeys,
						  CFRangeMake(0, nElements),
						  compFunc,
						  NULL);
			}

			for (i = 0; i < nElements; i++) {
				CFStringRef		key;
				CFStringRef		kStr;
				CFTypeRef		val;
				CFStringRef		vStr;

				key  = CFArrayGetValueAtIndex(sortedKeys, i);
				kStr = _SCCopyDescription((CFTypeRef)key, NULL);

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

				val  = CFDictionaryGetValue(cf, key);
				vStr = _SCCopyDescription((CFTypeRef)val, nFormatOptions);
				CFStringAppendFormat(str,
						     formatOptions,
						     CFSTR("\n%@"),
						     vStr);
				CFRelease(vStr);
			}

			CFRelease(sortedKeys);

			if (keys != keys_q) {
				CFAllocatorDeallocate(NULL, keys);
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

Boolean
_SC_isInstallEnvironment() {
	static dispatch_once_t	once;
	static Boolean		is_install;

	dispatch_once(&once, ^{
		is_install = (getenv(INSTALL_ENVIRONMENT) != NULL);
	});

	return is_install;
}


os_log_t
_SC_LOG_DEFAULT()
{
	static os_log_t	log	= NULL;

	if (log == NULL) {
		log = os_log_create("com.apple.SystemConfiguration", "");
	}

	return log;
}


os_log_type_t
_SC_syslog_os_log_mapping(int level)
{
	if (level < 0) {
		level = ~level;
	}

	switch (level) {
		case LOG_EMERG :
		case LOG_ALERT :
		case LOG_CRIT :
			return OS_LOG_TYPE_ERROR;

		case LOG_ERR :
		case LOG_WARNING :
		case LOG_NOTICE :
			return OS_LOG_TYPE_DEFAULT;

		case LOG_INFO :
			return OS_LOG_TYPE_INFO;

		case LOG_DEBUG :
			return OS_LOG_TYPE_DEBUG;
	}

	return OS_LOG_TYPE_DEFAULT;
};

static void
__SCLog(asl_object_t asl, asl_object_t msg, int level, void *ret_addr, CFStringRef formatString, va_list formatArguments)
{
	char		*line;
	CFArrayRef	lines;
	CFStringRef	str;

	if ((asl == NULL) && (level >= 0)) {
		const char	*__format;

		__format = CFStringGetCStringPtr(formatString, kCFStringEncodingUTF8);
		if (__format != NULL) {
			os_log_type_t	__type;

			__type = _SC_syslog_os_log_mapping(level);
			os_log_with_args(_SC_LOG_DEFAULT(),
					 __type,
					 __format,
					 formatArguments,
					 ret_addr);
			return;
		}
	}

	if (asl == NULL) {
		__SCThreadSpecificDataRef	tsd;

		tsd = __SCGetThreadSpecificData();
		if (tsd->_asl == NULL) {
			tsd->_asl = asl_open(NULL, (_SC_isInstallEnvironment() ? INSTALL_FACILITY : NULL), 0);
			asl_set_filter(tsd->_asl, ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG));
		}
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
			CFIndex	i;
			CFIndex	n	= CFArrayGetCount(lines);

			for (i = 0; i < n; i++) {
				line =_SC_cfstring_to_cstring_ext(CFArrayGetValueAtIndex(lines, i),
								  NULL,
								  0,
								  kCFStringEncodingUTF8,
								  (UInt8)'?',
								  NULL);
				asl_log(asl, msg, level, "%s", line);
				CFAllocatorDeallocate(NULL, line);
			}
			CFRelease(lines);
		}
	} else {
		line =_SC_cfstring_to_cstring_ext(str,
						  NULL,
						  0,
						  kCFStringEncodingUTF8,
						  (UInt8)'?',
						  NULL);
		asl_log(asl, msg, ~level, "%s", line);
		CFAllocatorDeallocate(NULL, line);
	}
	CFRelease(str);
	return;
}


static void
__SCPrint(FILE *stream, CFStringRef formatString, va_list formatArguments, Boolean trace, Boolean addNL)
{
	char		*line;
	CFStringRef	str;
	CFIndex		usedBufLen;

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

	line =_SC_cfstring_to_cstring_ext(str,
					  NULL,
					  0,
					  kCFStringEncodingUTF8,
					  (UInt8)'?',
					  &usedBufLen);
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
	(void)fwrite((const void *)line, usedBufLen, 1, stream);
	if (addNL) {
		(void)fputc('\n', stream);
	}
	fflush (stream);
	pthread_mutex_unlock(&lock);
	CFAllocatorDeallocate(NULL, line);

	return;
}


void
SCLog(Boolean condition, int level, CFStringRef formatString, ...)
{
	va_list		formatArguments;
	va_list		formatArguments_print;
	Boolean		log	= FALSE;
	Boolean		print	= FALSE;

	if (!condition) {
		return;
	}

	/*
	 * Note: The following are the expected values for _sc_log
	 *
	 * 0 if SC messages should be written to stdout/stderr
	 * 1 if SC messages should be logged w/asl(3)
	 * 2 if SC messages should be written to stdout/stderr AND logged
	 */

	if (_sc_log > 0) {
		log = TRUE;		// log requested
		va_start(formatArguments, formatString);

		if (_sc_log > 1) {
			print = TRUE;	// log AND print requested
			va_copy(formatArguments_print, formatArguments);
		}
	} else {
		print = TRUE;		// print requested
		va_start(formatArguments_print, formatString);
	}

	if (log) {
		__SCLog(NULL, NULL, level, __builtin_return_address(0), formatString, formatArguments);
		va_end(formatArguments);
	}

	if (print) {
		__SCPrint((LOG_PRI(level) > LOG_NOTICE) ? stderr : stdout,
			  formatString,
			  formatArguments_print,
			  (_sc_log > 0),	// trace
			  TRUE);		// add newline
		va_end(formatArguments_print);
	}

	return;
}


void
__SC_Log(int level, CFStringRef format_CF, os_log_t log, os_log_type_t type, const char *format, ...)
{
	Boolean		do_log		= FALSE;
	Boolean		do_print	= FALSE;
	va_list		args_log;
	va_list		args_print;

	/*
	 * Note: The following are the expected values for _sc_log
	 *
	 * 0 if SC messages should be written to stdout/stderr
	 * 1 if SC messages should be logged w/asl(3)
	 * 2 if SC messages should be written to stdout/stderr AND logged
	 */

	if (_sc_log > 0) {
		log = TRUE;		// log requested
		va_start(args_log, format);

		if (_sc_log > 1) {
			do_print = TRUE;	// log AND print requested
			va_copy(args_print, args_log);
		}
	} else {
		do_print = TRUE;		// print requested
		va_start(args_print, format);
	}

	if (do_log) {
		if (level >= 0) {
			os_log_with_args(log,
					 type,
					 format,
					 args_log,
					 __builtin_return_address(0));
		} else {
			// if we need to break apart a multi-line message
			__SCLog(NULL,
				NULL,
				level,
				__builtin_return_address(0),
				format_CF,
				args_log);
		}
		va_end(args_log);
	}

	if (do_print) {
		__SCPrint(stdout,
			  format_CF,
			  args_print,
			  (_sc_log > 0),	// trace
			  TRUE);		// add newline
		va_end(args_print);
	}

	return;

}


void
SCLOG(asl_object_t asl, asl_object_t msg, int level, CFStringRef formatString, ...)
{
	va_list		formatArguments;
	va_list		formatArguments_print;
	Boolean		log	= FALSE;
	Boolean		print	= FALSE;

	/*
	 * Note: The following are the expected values for _sc_log
	 *
	 * 0 if SC messages should be written to stdout/stderr
	 * 1 if SC messages should be logged w/asl(3)
	 * 2 if SC messages should be written to stdout/stderr AND logged
	 */

	if (_sc_log > 0) {
		log = TRUE;		// log requested
		va_start(formatArguments, formatString);

		if (_sc_log > 1) {
			print = TRUE;	// log AND print requested
			va_copy(formatArguments_print, formatArguments);
		}
	} else {
		print = TRUE;		// print requested
		va_start(formatArguments_print, formatString);
	}

	if (log) {
		__SCLog(asl, msg, level, __builtin_return_address(0), formatString, formatArguments);
		va_end(formatArguments);
	}

	if (print) {
		if (level < 0) {
			level = ~level;
		}
		__SCPrint((level > ASL_LEVEL_NOTICE) ? stderr : stdout,
			  formatString,
			  formatArguments_print,
			  (_sc_log > 0),	// trace
			  TRUE);		// add newline
		va_end(formatArguments_print);
	}

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


#pragma mark -
#pragma mark ASL Functions


static CFTypeID __kSCLoggerTypeID = _kCFRuntimeNotATypeID;

typedef  enum {
	kModuleStatusEnabled,
	kModuleStatusDisabled,
	kModuleStatusDoesNotExist
} ModuleStatus;

struct SCLogger
{
	CFRuntimeBase		cf_base;

	char *			loggerID;	// LoggerID
	SCLoggerFlags		flags;
	asl_object_t		aslc;
	asl_object_t		aslm;
	ModuleStatus		module_status;
	pthread_mutex_t		lock;
};


static void __SCLoggerDeallocate(CFTypeRef cf);
static const CFRuntimeClass __SCLoggerClass = {
	0,				/* version */
	"SCLogger",			/* className */
	NULL,				/* init */
	NULL,				/* copy */
	__SCLoggerDeallocate,		/* deallocate */
	NULL,				/* equal */
	NULL,				/* hash */
	NULL,				/* copyFormattingDesc */
	NULL				/* copyDebugDesc */
};


#define		DATETIMEBUFFERSIZE	32


static pthread_once_t	registerLoggerOnce = PTHREAD_ONCE_INIT;
static pthread_once_t	defaultLoggerOnce = PTHREAD_ONCE_INIT;

typedef enum {
	kLoggerASLControlEnableModule,
	kLoggerASLControlDisableModule,
	kLoggerASLControlLogFileCheckpoint
} LoggerASLControl;

static SCLoggerRef	defaultLogger = NULL;
static SCLoggerRef	__SCLoggerCreate(void);
static void		__SCLoggerDefaultLoggerInit();
static SCLoggerRef	SCLoggerGetDefaultLogger();
static void		SCLoggerSetLoggerID(SCLoggerRef logger, CFStringRef loggerID);
static void		SCLoggerSendMessageToModuleOnly(SCLoggerRef logger, Boolean isPrivate);
static void		SCLoggerSendASLControl(SCLoggerRef logger, LoggerASLControl control);
static ModuleStatus	GetModuleStatus(const char * loggerID);

static void
__SCLoggerRegisterClass(void)
{
	if (__kSCLoggerTypeID == _kCFRuntimeNotATypeID) {
		__kSCLoggerTypeID = _CFRuntimeRegisterClass(&__SCLoggerClass);
	}
	return;
}

static SCLoggerRef
__SCLoggerAllocate(CFAllocatorRef allocator)
{
	SCLoggerRef state;
	int size;

	pthread_once(&registerLoggerOnce, __SCLoggerRegisterClass);

	size = sizeof(*state) - sizeof(CFRuntimeBase);
	state = (SCLoggerRef) _CFRuntimeCreateInstance(allocator,
						       __kSCLoggerTypeID,
						       size,
						       NULL);
	return (state);
}

static void
__SCLoggerDeallocate(CFTypeRef cf)
{
	SCLoggerRef logger = (SCLoggerRef)cf;

	if (logger != NULL) {
		// Rotate on close behavior
		if (logger->module_status != kModuleStatusDoesNotExist) {
			SCLoggerSendASLControl(logger,
					       kLoggerASLControlLogFileCheckpoint);
		}
		if (logger->loggerID != NULL) {
			CFAllocatorDeallocate(NULL, logger->loggerID);
			logger->loggerID = NULL;
		}
		if (logger->aslm != NULL) {
			asl_release(logger->aslm);
			logger->aslm = NULL;
		}
		if (logger->aslc != NULL) {
			asl_release(logger->aslc);
			logger->aslc = NULL;
		}
	}
}

static SCLoggerRef
__SCLoggerCreate(void)
{
	SCLoggerRef tempLogger = NULL;

	tempLogger = __SCLoggerAllocate(kCFAllocatorDefault);
	tempLogger->loggerID = NULL;
	tempLogger->flags = kSCLoggerFlagsDefault;
	tempLogger->aslc = asl_open(NULL, (_SC_isInstallEnvironment() ? INSTALL_FACILITY : NULL), ASL_OPT_NO_DELAY);
	tempLogger->aslm = asl_new(ASL_TYPE_MSG);
	pthread_mutex_init(&(tempLogger->lock), NULL);
	tempLogger->module_status = kModuleStatusDoesNotExist;

	return tempLogger;
}

SCLoggerFlags
SCLoggerGetFlags(SCLoggerRef logger)
{
	return logger->flags;
}

void
SCLoggerSetFlags(SCLoggerRef logger, SCLoggerFlags flags)
{
	if (logger == defaultLogger) {
		return;
	}
	pthread_mutex_lock(&(logger->lock));
	if (flags != kSCLoggerFlagsNone) {
		logger->module_status = GetModuleStatus(logger->loggerID);
		if (logger->module_status == kModuleStatusDoesNotExist) {
			goto done;
		}
		if ((flags & kSCLoggerFlagsFile) != 0) {
			if ((logger->flags & kSCLoggerFlagsFile) == 0) {
				// Enable the module if disabled
				if (logger->module_status == kModuleStatusDisabled) {
					SCLoggerSendASLControl(logger, kLoggerASLControlEnableModule);
				}
				// Setting ASL Filter level to debug
				asl_set_filter(logger->aslc, ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG));
				if (logger->loggerID != NULL) {
					asl_set(logger->aslm, kLoggerID,
						logger->loggerID);
				}
			}
		}
		else if ((logger->flags & kSCLoggerFlagsFile) != 0) {
			asl_unset(logger->aslm, kLoggerID);
			asl_set_filter(logger->aslc, ASL_FILTER_MASK_UPTO(ASL_LEVEL_NOTICE));
			SCLoggerSendMessageToModuleOnly(logger, false);
		}
		if ((flags & kSCLoggerFlagsDefault) != 0) {
			if ((logger->flags & kSCLoggerFlagsDefault) == 0) {
				SCLoggerSendMessageToModuleOnly(logger, false);
			}
		}
		else if ((logger->flags & kSCLoggerFlagsDefault) != 0) {
			SCLoggerSendMessageToModuleOnly(logger, true);
		}
	}
	logger->flags = flags;
   done:
	pthread_mutex_unlock(&(logger->lock));
}


static void
SCLoggerSetLoggerID(SCLoggerRef logger, CFStringRef loggerID)
{
	logger->loggerID
		= _SC_cfstring_to_cstring(loggerID, NULL, 0,
					  kCFStringEncodingUTF8);
	// Enable the module if disabled
	logger->module_status = GetModuleStatus(logger->loggerID);
	if (logger->module_status == kModuleStatusDisabled) {
		SCLoggerSendASLControl(logger, kLoggerASLControlEnableModule);
	}
}

static ModuleStatus
GetModuleStatus(const char * loggerID)
{
	ModuleStatus	moduleStatus	= kModuleStatusDoesNotExist;
	asl_object_t	response	= NULL;
	const char*	value		= NULL;

	if (loggerID != NULL) {
		response = _asl_server_control_query();
		if (response == NULL) {
			goto done;
		}
		value = asl_get(response, loggerID);
		if (value == NULL) {
			moduleStatus = kModuleStatusDoesNotExist;
			goto done;
		}

		if (strcmp(value, "enabled") == 0) {
			moduleStatus = kModuleStatusEnabled;
		}
		else {
			moduleStatus = kModuleStatusDisabled;
		}
	}
done:
	asl_release(response);

	return moduleStatus;
}

static void
SCLoggerSendMessageToModuleOnly(SCLoggerRef logger, Boolean isPrivate)
{
	if (isPrivate) {
		asl_set(logger->aslm, kASLModule, logger->loggerID);
	}
	else {
		if (asl_get(logger->aslm, kASLModule) != NULL) {
			asl_unset(logger->aslm, kASLModule);
		}
	}
}

static void
SCLoggerSendASLControl(SCLoggerRef logger, LoggerASLControl control)
{
	SCLoggerRef defLogger = SCLoggerGetDefaultLogger();
	pthread_mutex_lock(&(defLogger->lock));

	// this next line turns the asl_log()'s that follow into control messages
	asl_set(defLogger->aslm, kASLOption, "control");

	switch (control) {
		case kLoggerASLControlEnableModule:
			asl_log(defLogger->aslc, defLogger->aslm,
				ASL_LEVEL_NOTICE, "@ %s enable 1",
				logger->loggerID);
			break;
		case kLoggerASLControlDisableModule:
			asl_log(defLogger->aslc, defLogger->aslm,
				ASL_LEVEL_NOTICE, "@ %s enable 0",
				logger->loggerID);
			break;
		case kLoggerASLControlLogFileCheckpoint:
			asl_log(defLogger->aslc, defLogger->aslm,
				ASL_LEVEL_NOTICE, "@ %s checkpoint",
				logger->loggerID);
			break;
		default:
			break;
	}

	// turn off control mode
	asl_unset(defLogger->aslm, kASLOption);
	pthread_mutex_unlock(&defLogger->lock);
	return;
}

SCLoggerRef
SCLoggerCreate(CFStringRef loggerID)
{
	SCLoggerRef logger = NULL;

	logger = __SCLoggerCreate();
	if (loggerID != NULL) {
		SCLoggerSetLoggerID(logger, loggerID);
	}
	SCLoggerSetFlags(logger, kSCLoggerFlagsDefault);
	return logger;
}

static void
__SCLoggerDefaultLoggerInit()
{
	if (defaultLogger == NULL) {
		defaultLogger = __SCLoggerCreate();
		defaultLogger->flags = kSCLoggerFlagsDefault;
	}
}

static SCLoggerRef
SCLoggerGetDefaultLogger()
{
	pthread_once(&defaultLoggerOnce, __SCLoggerDefaultLoggerInit);
	return defaultLogger;
}

static void
SCLoggerVLogInternal(SCLoggerRef logger, int loglevel, void *ret_addr,
		     CFStringRef formatString, va_list args)
{
	asl_object_t	aslc;
	asl_object_t	aslm;

	if (logger == NULL
	    || logger->module_status == kModuleStatusDoesNotExist) {
		logger = SCLoggerGetDefaultLogger();
	}
	pthread_mutex_lock(&(logger->lock));
	if (logger->flags == kSCLoggerFlagsNone) {
		pthread_mutex_unlock(&(logger->lock));
		return;
	}
	aslc = logger->aslc;
	aslm = logger->aslm;
	__SCLog(aslc, aslm, loglevel, ret_addr, formatString, args);
	pthread_mutex_unlock(&(logger->lock));
	return;
}

void
SCLoggerLog(SCLoggerRef logger, int loglevel, CFStringRef formatString, ...)
{
	va_list	args;

	va_start(args, formatString);
	SCLoggerVLogInternal(logger, loglevel, __builtin_return_address(0), formatString, args);
	va_end(args);

	return;
}

void
SCLoggerVLog(SCLoggerRef logger, int loglevel, CFStringRef formatString, va_list args)
{
	SCLoggerVLogInternal(logger, loglevel, __builtin_return_address(0), formatString, args);
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
	{ kSCStatusConnectionIgnore,	"Network connection information not available at this time" },
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
