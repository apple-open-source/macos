/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// debugging - non-trivial debugging support
//
#include <Security/debugsupport.h>
#include <Security/globalizer.h>
#include <cstdarg>

#define SYSLOG_NAMES	// compile syslog name tables
#include <syslog.h>

namespace Security {
namespace Debug {


#if !defined(NDEBUG)


//
// Main debug functions (global and in-scope)
//
void debug(const char *scope, const char *format, ...)
{
#if !defined(NDEBUG_STUBS)
	va_list args;
	va_start(args, format);
	Target::get().message(scope, format, args);
	va_end(args);
#endif
}

void vdebug(const char *scope, const char *format, va_list args)
{
#if !defined(NDEBUG_STUBS)
    Target::get().message(scope, format, args);
#endif
}

void Scope::operator () (const char *format, ...)
{
#if !defined(NDEBUG_STUBS)
	va_list args;
	va_start(args, format);
	Target::get().message(mScope, format, args);
	va_end(args);
#endif
}

bool debugging(const char *scope)
{
#if !defined(NDEBUG_STUBS)
	return Target::get().debugging(scope);
#else
    return false;
#endif
}


//
// Dump facility
//
bool dumping(const char *scope)
{
#if defined(NDEBUG_STUBS)
    return false;
#else
	return Target::get().dump(scope);
#endif
}

void dump(const char *format, ...)
{
#if !defined(NDEBUG_STUBS)
	va_list args;
	va_start(args, format);
	Target::get().dump(format, args);
	va_end(args);
#endif
}

void dumpData(const void *ptr, size_t size)
{
#if !defined(NDEBUG_STUBS)
	const char *addr = reinterpret_cast<const char *>(ptr);
	const char *end = addr + size;
	bool isText = true;
	for (const char *p = addr; p < end; p++)
		if (!isprint(*p)) { isText = false; break; }
		
	if (isText) {
		dump("\"");
		for (const char *p = addr; p < end; p++)
			dump("%c", *p);
		dump("\"");
	} else {
		dump("0x");
		for (const char *p = addr; p < end; p++)
			dump("%2.2x", *p);
	}
#endif //NDEBUG_STUBS
}

void dumpData(const char *title, const void *ptr, size_t size)
{
#if !defined(NDEBUG_STUBS)
	dump("%s: ", title);
	dumpData(ptr, size);
	dump("\n");
#endif //NDEBUG_STUBS
}


//
// Target initialization
//
#if !defined(NDEBUG_STUBS)

Target::Target() : showScope(false), showThread(false),	showPid(false), sink(NULL)
{
	// put into singleton slot if first
	if (singleton == NULL)
		singleton = this;
}

Target::~Target()
{
}


//
// The core logging function of a Target
//
void Target::message(const char *scope, const char *format, va_list args)
{
	if (logSelector(scope)) {
		// note: messageConstructionSize is big enough for all prefixes constructed
		char buffer[messageConstructionSize];	// building the message here
		char *bufp = buffer;
		if (showScope && scope) {		// add "scope "
			if (const char *sep = strchr(scope, ',')) {
				bufp += sprintf(bufp, "%-*s", Name::maxLength, (const char *)Name(scope, sep));
			} else {	// single scope
				bufp += sprintf(bufp, "%-*s", Name::maxLength, scope);
			}
		}
		if (showPid) {		// add "[Pid] "
			bufp += sprintf(bufp, "[%d] ", getpid());
		}
		if (showThread) {		// add "#Tthreadid "
			*bufp++ = '#';
			Thread::Identity::current().getIdString(bufp);
			bufp += strlen(bufp);
			*bufp++ = ' ';
		}
		vsnprintf(bufp, buffer + sizeof(buffer) - bufp, format, args);
        for (char *p = bufp; *p; p++)
            if (!isprint(*p))
                *p = '?';
		sink->put(buffer, bufp - buffer);
	}
}

bool Target::debugging(const char *scope)
{
	return logSelector(scope);
}


//
// The core debug-dump function of a target
//
void Target::dump(const char *format, va_list args)
{
	sink->dump(format, args);
}

bool Target::dump(const char *scope)
{
	return dumpSelector(scope);
}

//
// Selector objects.
//
Target::Selector::Selector() : useSet(false), negate(false)
{ }

void Target::Selector::operator = (const char *scope)
{
	if (scope) {
		// initial values
		if (!strcmp(scope, "all")) {
			useSet = false;
			negate = true;
		} else if (!strcmp(scope, "none")) {
			useSet = negate = false;
		} else {
			useSet = true;
			enableSet.erase(enableSet.begin(), enableSet.end());
			if (scope[0] == '-') {
				negate = true;
				scope++;
			} else
				negate = false;
			while (const char *sep = strchr(scope, ',')) {
				enableSet.insert(Name(scope, sep));
				scope = sep + 1;
			}
			enableSet.insert(scope);
		}
	} else {
		useSet = negate = false;
	}
}

bool Target::Selector::operator () (const char *scope) const
{
	// a scope of NULL is a special override; it always qualifies
	if (scope == NULL)
		return true;

	if (useSet) {
		while (const char *sep = strchr(scope, ',')) {
			if (enableSet.find(Name(scope, sep)) != enableSet.end())
				return !negate;
			scope = sep + 1;
		}
		return (enableSet.find(scope) != enableSet.end()) != negate;
	} else {
		return negate;
	}
}


//
// Establish Target state from the environment
//
void Target::setFromEnvironment()
{
	// set scopes
	logSelector = getenv("DEBUGSCOPE");
	dumpSelector = getenv("DEBUGDUMP");
	
	//
	// Set and configure destination. Currently available:
	//	/some/where -> that file
	//	LOG_SOMETHING -> syslog facility
	//	>&number -> that (already) open file descriptor
	//	anything else -> try as a filename sight unseen
	//	DEBUGDEST not set -> stderr
	//	anything in error -> stderr (with an error message on it)
	//
	if (const char *dest = getenv("DEBUGDEST")) {
		if (dest[0] == '/') {	// full pathname, write to file
			to(dest);
		} else if (!strncmp(dest, "LOG_", 4)) {	// syslog
			int facility = LOG_DAEMON;
			for (CODE *cp = facilitynames; cp->c_name; cp++)
				if (!strcmp(dest, cp->c_name))
					facility = cp->c_val;
			to(facility | LOG_DEBUG);
		} else if (!strncmp(dest, ">&", 2)) {	// to file descriptor
			int fd = atoi(dest+2);
			if (FILE *f = fdopen(fd, "a")) {
				to(f);
			} else {
				to(stderr);
				::debug(NULL, "cannot log to fd[%d]: %s", fd, strerror(errno));
			}
		} else {	// if everything else fails, write a file
			to(dest);
		}
	} else {		// default destination is stderr
		to(stderr);
	}
	configure();
}


void Target::configure()
{
	configure(getenv("DEBUGOPTIONS"));
}

void Target::configure(const char *config)
{
	// configure global options
	showScope = config && strstr(config, "scope");
	showThread = config && strstr(config, "thread");
	showPid = config && strstr(config, "pid");

	// configure sink
	if (sink)
		sink->configure(config);
}


//
// Explicit destination assignments
//
void Target::to(Sink *s)
{		
	delete sink;	
	sink = s;
}

void Target::to(FILE *file)
{
	to(new FileSink(file));
}

void Target::to(const char *filename)
{
	if (FILE *f = fopen(filename, "a")) {
		to(new FileSink(f));
	} else {
		to(stderr);
		::debug(NULL, "cannot debug to \"%s\": %s", filename, strerror(errno));
	}
}

void Target::to(int syslogPriority)
{
	to(new SyslogSink(syslogPriority));
}


//
// Making and retrieving the default singleton
//
Target *Target::singleton;

Target &Target::get()
{
	if (singleton == NULL) {
		Target *t = new Target;
		t->setFromEnvironment();
	}
	return *singleton;
}


//
// Standard sink implementations
//
Target::Sink::~Sink()
{ }

void Target::Sink::dump(const char *, va_list)
{ }

void Target::Sink::configure(const char *)
{ }


//
// File sinks (write to file via stdio)
//
void FileSink::put(const char *buffer, unsigned int)
{
	StLock<Mutex> locker(lock, false);
	if (lockIO)
		locker.lock();
	if (addDate) {
		time_t now = time(NULL);
		char *date = ctime(&now);
		date[19] = '\0';
		fprintf(file, "%s ", date + 4);	// Nov 24 18:22:48
	}
	fputs(buffer, file);
	putc('\n', file);
}

void FileSink::dump(const char *format, va_list args)
{
	StLock<Mutex> locker(lock, false);
	if (lockIO)
		locker.lock();
	vfprintf(file, format, args);
}

void FileSink::configure(const char *options)
{
	if (options == NULL || !strstr(options, "noflush"))
		setlinebuf(file);
	if (options) {
		addDate = strstr(options, "date");
		lockIO = !strstr(options, "nolock");
	}
}


//
// Syslog sinks (write to syslog)
//
void SyslogSink::put(const char *buffer, unsigned int)
{
	syslog(priority, "%s", buffer);
}

void SyslogSink::dump(const char *format, va_list args)
{
	// add to dump buffer
	vsnprintf(dumpPtr, dumpBuffer + dumpBufferSize - dumpPtr, format, args);
	
	// take off full lines and submit
	char *p = dumpBase;
	while (char *q = strchr(p, '\n')) {
		*q++ = '\0';	// terminate/break
		syslog(priority, " @@ %s", p);
		p = q;
	}
	
	if (*p) {	// left-over unterminated line segment in buffer
		dumpPtr = p + strlen(p);
		if ((dumpBase = p) > dumpBuffer + dumpBufferSize / 2) {
			// shift buffer down to make room
			memmove(dumpBuffer, dumpBase, dumpPtr - dumpBase);
			dumpPtr -= (dumpBase - dumpBuffer);
			dumpBase = dumpBuffer;
		}
	} else {	// buffer is empty; reset to start
		dumpBase = dumpPtr = dumpBuffer;
	}
}

void SyslogSink::configure(const char *options)
{
}

#endif //NDEBUG_STUBS

#endif // NDEBUG


} // end namespace Debug

} // end namespace Security
