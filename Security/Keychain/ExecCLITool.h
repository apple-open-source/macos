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

#ifndef _H_EXECCLITOOL
#define _H_EXECCLITOOL

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdarg>
#include <map>
#include <stdlib.h>
#include <CoreFoundation/CFString.h>

class VAArgList
{
public:
    VAArgList() {};
    ~VAArgList() { if (argv) free(argv); }

    typedef const char * ArgvArgPtr;
    
    int VAArgList::set(const char *path,va_list params);

    const char* const*get() { return argv; }

	int size()	{ return argn; }
    
private:
	ArgvArgPtr *argv;	// array for list of pointers
    int argn;			// count of elements in argv
    bool mSet;			// params have been passed in
};

class ExecCLITool
{
public:
	ExecCLITool();
	~ExecCLITool();
	
    int run(const char *toolPath, const char *toolEnvVar, ...);
    void input(const char *data,unsigned int length);
    void input(CFStringRef theString, bool appendNULL=false);
    const char * data() const	{ return dataRead; }
    unsigned int length() const	{ return dataLength; }
    
protected:

    void child(const char *toolPath, const char *toolEnvVar, VAArgList& arglist);
	void parent(pid_t pid);
    void parentReadOutput();
    void parentWriteInput();
    void closeAllPipes();
    void initialize();
    void reset();
   
	int stdoutpipe[2];				// for reading data from child into parent (child uses stdout)
	int stdinpipe [2];				// for writing data from parent to child (child uses stdin)
    
    char *dataRead;
    unsigned int dataLength;

    char *dataToWrite;
    unsigned int dataToWriteLength;

    static const unsigned int kReadBufSize = 1024;
    
};


#endif //_H_EXECCLITOOL


