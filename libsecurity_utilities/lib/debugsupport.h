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
// debugsupport - support interface for making and managing debugger objects.
//
// This header is not needed for logging debug messages.
//
#ifndef _H_DEBUGSUPPORT
#define _H_DEBUGSUPPORT

//
// Generate stub-code support if NDEBUG (but not CLEAN_NDEBUG) is set, to support
// client code that may have been generated with debug enabled. You don't actually
// get *real* debug logging, of course, just cheap dummy stubs to keep the linker happy.
//
#include <security_utilities/debugging.h>
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <cstdarg>
#include <set>

namespace Security {
namespace Debug {


//
// Debug scope names - short strings with value semantics.
// We don't use STL strings because of overhead.
//
class Name {
public:
	static const int maxLength = 12;

	Name(const char *s)
	{ strncpy(mName, s, maxLength-1); mName[maxLength-1] = '\0'; }
	
	Name(const char *start, const char *end)
	{
		int length = end - start; if (length >= maxLength) length = maxLength - 1;
		memcpy(mName, start, length); memset(mName + length, 0, maxLength - length);
	}
	
	operator const char *() const	{ return mName; }
	
	bool operator < (const Name &other) const
	{ return memcmp(mName, other.mName, maxLength) < 0; }
	
	bool operator == (const Name &other) const
	{ return memcmp(mName, other.mName, maxLength) == 0; }

private:
	char mName[maxLength];		// null terminated for easy printing
};


//
// A debugging Target. This is an object that receives debugging requests.
// You can have many, but one default one is always provided.
//
class Target {
public:
	Target();
	virtual ~Target();
	
	// get default (singleton) Target
	static Target &get();
	
	void setFromEnvironment();
	
public:
	class Sink {
	public:
		virtual ~Sink();
		virtual void put(const char *buffer, unsigned int length) = 0;
		virtual void dump(const char *buffer);
		virtual void configure(const char *argument);
		const bool needsDate;
		
	protected:
		Sink(bool nd = true) : needsDate(nd) { }
	};
	
	void to(Sink *sink);
	void to(const char *filename);
	void to(int syslogPriority);
	void to(FILE *openFile);
	
	void configure();						// from DEBUGOPTIONS
	void configure(const char *options);	// from explicit string
	
public:
	void message(const char *scope, const char *format, va_list args);
	bool debugging(const char *scope);
	void dump(const char *format, va_list args);
	bool dump(const char *scope);
	
protected:
	class Selector {
	public:
		Selector();
		void operator = (const char *config);
		
		bool operator () (const char *name) const;

	private:
		bool useSet;				// use contents of enableSet
		bool negate;				// negate meaning of enableSet
		set<Name> enableSet;		// set of names
	};

protected:
	class PerThread {
	public:
		PerThread() { id = ++lastUsed; }
		unsigned int id;			// arbitrary (sequential) ID number

	private:
		static unsigned int lastUsed; // last id used
	};
	ThreadNexus<PerThread> perThread;
	
protected:
	static const size_t messageConstructionSize = 512;	// size of construction buffer

	Selector logSelector;			// selector for logging
	Selector dumpSelector;			// selector for dumping
	
	// output option state (from last configure call)
	bool showScope;					// include scope in output lines
	bool showScopeRight;			// scope after proc/thread, not before
	bool showThread;				// include thread and pid in output lines
	bool showProc;					// include "nice" process/thread id in output lines
	bool showDate;					// include date in output lines
	size_t dumpLimit;				// max. # of bytes dumped by dumpData & friends
	
	// misc. global state
	static const size_t maxProgNameLength = 20;  // max. program name remembered
	static const size_t procLength = 14; // characters for proc/thread column
	static char progName[];			// (short truncated form of) program name

	// current output support
	Sink *sink;
	
	static terminate_handler previousTerminator;	// for chaining
	static void terminator();
	
	// the default Target
	static Target *singleton;
};


//
// Standard Target::Sinks
//
class FileSink : public Target::Sink {
public:
	FileSink(FILE *f) : Sink(true), file(f) { }
	void put(const char *, unsigned int);
	void dump(const char *text);
	void configure(const char *);
	
private:
	FILE *file;
};

class SyslogSink : public Target::Sink {
public:
	SyslogSink(int pri)
		: Sink(false), priority(pri), dumpBase(dumpBuffer), dumpPtr(dumpBuffer) { }
	void put(const char *, unsigned int);
	void dump(const char *text);
	void configure(const char *);
	
private:
	int priority;
	
	// a sliding buffer to hold partial line output
	static const size_t dumpBufferSize = 1024;	// make this about 2 * maximum line length of dumps
	char dumpBuffer[dumpBufferSize];
	char *dumpBase, *dumpPtr;
};


} // end namespace Debug
} // end namespace Security


#endif //_H_DEBUGSUPPORT
