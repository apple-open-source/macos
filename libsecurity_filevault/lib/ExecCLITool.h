/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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

#ifndef _H_EXECCLITOOL
#define _H_EXECCLITOOL

#include <fcntl.h>
#include <vector>
#include <security_cdsa_utilities/cssmdata.h>
#include <CoreFoundation/CFData.h>

class ExecCLITool
{
protected:
    typedef const char *  ConstCharPtr;	//const
    typedef vector<ConstCharPtr> argvec;
    typedef vector<CssmAutoData *> iodatavec;

public:
	ExecCLITool();
	~ExecCLITool();
	
    int runx(const char *toolPath, const char *toolEnvVar = NULL);
    int run(const char *toolPath, const char *toolEnvVar, ...);
    void addin (const char *data,unsigned int length) { add(data,length,inputdata);  };
    void addout(const char *data,unsigned int length) { add(data,length,outputdata); };
    void addincfs(CFStringRef theString, bool appendNULL=false);
    void reset();

    void addarg(ConstCharPtr theArg) { args.push_back(theArg); };
    void addargs(int count,...);
    CFDataRef getResults() const;

    // Debug only
    void dumpin()  { dump(inputdata); };
    void dumpout() { dump(outputdata); };
    void debugdisp(const char *msg);
    
protected:

    void child(const char *toolPath, const char *toolEnvVar);
	int parent(pid_t pid);
    void parentReadOutput();
    void parentWriteInput(const char *data,unsigned int length);
    void closeAllPipes();
    void initializePipes();
    void cleardata(iodatavec& vec);
    void add(const char *data,unsigned int length,iodatavec& vec);
    void dump(iodatavec& vec);

	int stdoutpipe[2];					// for reading data from child into parent (child uses stdout)
	int stdinpipe [2];					// for writing data from parent to child (child uses stdin)
    
    bool eofSeen[2];
    
	iodatavec inputdata;				// array of CssmDatas to data to pass to stdin
	iodatavec outputdata;				// array of CssmDatas to data to pass to stdout
    
    argvec args;
    
    static const unsigned int kReadBufSize = 1024;
    enum
    {
        READ = 0,
        WRITE = 1
    };
};


#endif //_H_EXECCLITOOL


