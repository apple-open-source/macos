/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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


//
// debugging - non-trivial debugging support
//
#include <security_utilities/debugsupport.h>
#include <security_utilities/globalizer.h>
#include <cstdarg>
#include <ctype.h>

#define SYSLOG_NAMES		// compile syslog name tables
#include <syslog.h>

#include <cxxabi.h>			// for name demangling
#include <mach-o/dyld.h>	// for _NSGetExecutablePath

// enable kernel tracing
#define ENABLE_SECTRACE 1


namespace Security {
namespace Debug {


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
#if !defined(NDEBUG_CODE)
	va_list args;
	va_start(args, format);
	Target::get().dump(format, args);
	va_end(args);
#endif
}

void dumpData(const void *ptr, size_t size)
{
#if !defined(NDEBUG_CODE)
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
			dump("%2.2x", static_cast<unsigned char>(*p));
	}
#endif //NDEBUG_STUBS
}

void dumpData(const char *title, const void *ptr, size_t size)
{
#if !defined(NDEBUG_CODE)
	dump("%s: ", title);
	dumpData(ptr, size);
	dump("\n");
#endif //NDEBUG_STUBS
}


//
// Turn a C++ typeid into a nice type name.
// This uses the C++ ABI where available.
// We're stripping out a few C++ prefixes; they're pretty redundant (and obvious).
//
string makeTypeName(const type_info &type)
{
	int status;
	char *cname = abi::__cxa_demangle(type.name(), NULL, NULL, &status);
	string name = !strncmp(cname, "Security::", 10) ? (cname + 10) :
		!strncmp(cname, "std::", 5) ? (cname + 5) :
		cname;
	::free(cname);	// yes, really (ABI rules)
	return name;
}


//
// Target initialization.
// This where we should do all "first time" initializations.
//
#if !defined(NDEBUG_CODE)

char Target::progName[maxProgNameLength + 1];
unsigned int Target::PerThread::lastUsed;

Target::Target() 
	: showScope(false), showThread(false), showProc(false), showDate(false),
	  sink(NULL)
{
	// put into singleton slot if first
	if (singleton == NULL)
		singleton = this;
	
	// insert terminate handler
	if (!previousTerminator)	// first time we do this
		previousTerminator = set_terminate(terminator);
	
	// get program name
	char execPath[PATH_MAX];
	uint32_t length = sizeof(execPath);
	if (_NSGetExecutablePath(execPath, &length)) {
		strcpy(progName, "unknown");
	} else {
		const char *p = strrchr(execPath, '/');
		if (p)
			p++;
		else
			p = execPath;
		unsigned plen = strlen(p);
		if (plen > maxProgNameLength)		// too long
			p += plen - maxProgNameLength; // take rear
		strcpy(progName, p);
	}
}

Target::~Target()
{
}


static void addScope(char *&bufp, const char *scope)
{
	if (const char *sep = strchr(scope, ',')) {
		bufp += sprintf(bufp, "%-*s", Name::maxLength, (const char *)Name(scope, sep));
	} else {    // single scope
		bufp += sprintf(bufp, "%-*s", Name::maxLength, scope);
	}
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

		// date option
		if (showDate && sink->needsDate) {
			time_t now = time(NULL);
			char *date = ctime(&now);
			date[19] = '\0';
			bufp += sprintf(bufp, "%s ", date + 4);	// Nov 24 18:22:48
		}

		// leading scope
		if (showScope && scope)
			addScope(bufp, scope);
		
		if (showProc || showThread) {
			char sub[maxProgNameLength + 20];
			unsigned plen = 0;
			if (showProc && showThread)
				plen = sprintf(sub, "%s[%d]", progName, getpid());
			else if (showProc)
				plen = sprintf(sub, "%s", progName);
			else
				plen = sprintf(sub, "[%d]", getpid());
			unsigned int id = perThread().id;
			if (id > 1)
				plen += sprintf(sub + plen, ":%d", id);
			if (plen <= procLength)
				bufp += sprintf(bufp, "%-*s ", int(procLength), sub);
			else
				bufp += sprintf(bufp, "%s ", sub + plen - procLength);
		}

		// scope after proc/thread/pid
		if (showScopeRight && scope)
			addScope(bufp, scope);

		// now stuff the message body in, slightly roasted
		size_t left = buffer + sizeof(buffer) - bufp - 1;	// reserve one
		size_t written = vsnprintf(bufp, left, format, args);
        for (char *p = bufp; *p; p++)
            if (!isprint(*p))
                *p = '?';
		if (written >= left) {	// snprintf overflowed
			bufp += left;
			strcpy(bufp - 3, "...");
		} else
			bufp += written;
		
		// now append a newline and a null
		bufp[0] = '\n';
		bufp[1] = '\0';

		// submit to sink (do not count newline and null in count)
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
	char buffer[messageConstructionSize];	// building the message here
	vsnprintf(buffer, sizeof(buffer), format, args);
	for (char *p = buffer; *p; p++)
		if (!isprint(*p) && !isspace(*p) || *p == '\r')
			*p = '?';
	sink->dump(buffer);
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
	//	>&number -> that (already) open (for write or append) file descriptor
	//	anything else -> try as a filename sight unseen [may change]
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
				secdebug("", "cannot log to fd[%d]: %s", fd, strerror(errno));
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
	showScopeRight = config && strstr(config, "rscope");
	showScope = !showScopeRight && config && strstr(config, "scope");
	showThread = config && (strstr(config, "thread") || strstr(config, "pid")); // (legacy)
	showProc = config && strstr(config, "proc");
	showDate = config && strstr(config, "date");

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
		secdebug("", "cannot debug to \"%s\": %s", filename, strerror(errno));
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

void Target::Sink::dump(const char *)
{ }

void Target::Sink::configure(const char *)
{ }


//
// The terminate handler installed when a Target is created
//
terminate_handler Target::previousTerminator;

void Target::terminator()
{
	secdebug("exception", "uncaught exception terminates program");
	previousTerminator();
	secdebug("exception", "prior termination handler failed to abort; forcing abort");
	abort();
}


//
// File sinks (write to file via stdio)
//
void FileSink::put(const char *inbuf, unsigned int length)
{
	fwrite(inbuf, 1, length + 1, file);	// do pick up the trailing newline
}

void FileSink::dump(const char *text)
{
	fputs(text, file);
}

void FileSink::configure(const char *options)
{
	if (options == NULL || !strstr(options, "noflush")) {
		// we mean "if the file isn't unbuffered", but what's the portable way to say that?
		if (file != stderr)
			setlinebuf(file);
	}
}


//
// Syslog sinks (write to syslog)
//
void SyslogSink::put(const char *buffer, unsigned int length)
{
	syslog(priority, "%1.*s", length, buffer); // don't pick up trailing newline
}

void SyslogSink::dump(const char *text)
{
	// add to dump buffer
	snprintf(dumpPtr, dumpBuffer + dumpBufferSize - dumpPtr, "%s", text);
	
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

#endif //NDEBUG_CODE


} // end namespace Debug
} // end namespace Security
