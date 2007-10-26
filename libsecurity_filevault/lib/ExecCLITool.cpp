/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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

#include <sys/wait.h>
#include "ExecCLITool.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#if !defined(NDEBUG)
    #include <iostream>
#endif
#include <CoreFoundation/CoreFoundation.h>
#include <Security/cssmapple.h>

#pragma mark -------------------- ExecCLITool implementation --------------------

ExecCLITool::ExecCLITool()
{
	stdinpipe [READ]=0, stdinpipe [WRITE]=0;
    stdoutpipe[READ]=0, stdoutpipe[WRITE]=0;
    args.push_back(NULL);						// reserve a spot for path later on
}

ExecCLITool::~ExecCLITool()
{
    reset();
}

int ExecCLITool::run(const char *toolPath, const char *toolEnvVar, ...)
{
    // Note that this does not erase any parameters previously set
    va_list params;
    va_start(params, toolEnvVar);

    ConstCharPtr theArg;
    while ((theArg=va_arg(params,ConstCharPtr)) != NULL)
        addarg(theArg);

    va_end(params);
    
	return runx(toolPath,toolEnvVar);
}

int ExecCLITool::runx(const char *toolPath, const char *toolEnvVar)
{
    int status = 0;
    try
    {
        initializePipes();

        // try to run the tool
        switch (pid_t pid = fork())
        {
        case 0:		// child
            args[0]=toolPath;				// convention says this is arg[0]
            addarg(NULL);					// must end with a NULL
            child(toolPath,toolEnvVar);
            break;
        case -1:	// error (in parent)
            UnixError::throwMe();
            break;
        default:	// parent
            status = parent(pid);
            break;
        }
    }
    catch (...)
    {
        closeAllPipes();
        return errno;
    }
    
	closeAllPipes();
    return status;
}

void ExecCLITool::reset()
{
	closeAllPipes();
    cleardata(inputdata);
    cleardata(outputdata);
    args.erase(args.begin(),args.end());
    args.push_back(NULL);					// reserve a spot for path later on
}

void ExecCLITool::add(const char *data,unsigned int length,iodatavec& vec)
{
    CssmAutoData *tmp = new CssmAutoData(Allocator::standard());
    tmp->copy(data,length);
	vec.push_back(tmp);
}

void ExecCLITool::cleardata(iodatavec& vec)
{
    iodatavec::iterator ix;
    for (ix = vec.begin();ix!=vec.end();++ix)
    	delete *ix;
    vec.erase(vec.begin(),vec.end());
}

CFDataRef ExecCLITool::getResults() const
{
    // Return the flattened results array
    // Caller is reponsible for freeing the storage

    CFMutableDataRef results = CFDataCreateMutable(kCFAllocatorDefault, 0);
    for (iodatavec::const_iterator ix = outputdata.begin();ix!=outputdata.end();++ix)
    {
        const CssmAutoData *px = *ix;
        CFDataAppendBytes(results, (unsigned char *)px, px->length());
    }
    return results;
}

void ExecCLITool::addargs(int count,...)
{
    // all arguments must be char *
    ConstCharPtr pArg;
    va_list params;
    va_start(params, count);
        for (int ix=0; (pArg=va_arg(params,ConstCharPtr)) != NULL && ix < count;ix++)
            addarg(pArg);
    va_end(params);
}

void ExecCLITool::initializePipes()
{
    // Create pipe to catch tool output
    if (pipe(stdoutpipe))						// for reading data from child into parent
        UnixError::throwMe();

    if (fcntl(stdoutpipe[READ], F_SETFL, O_NONBLOCK))
        UnixError::throwMe();

    if (pipe(stdinpipe))						// for writing data from parent to child
        UnixError::throwMe();
}
        
void ExecCLITool::child(const char *toolPath, const char *toolEnvVar)
{
    // construct path to tool
    try
    {
        char toolExecutable[PATH_MAX + 1];
#if defined(NDEBUG)
        const char *path = toolPath;
#else
        const char *path = toolEnvVar ? getenv(toolEnvVar) : NULL;
        if (!path)
            path = toolPath;
#endif
        snprintf(toolExecutable, sizeof(toolExecutable), "%s", path);
    
        close(stdoutpipe[READ]);			// parent read
        close(STDOUT_FILENO);
        if (dup2(stdoutpipe[WRITE], STDOUT_FILENO) < 0)
            UnixError::throwMe();
        close(stdoutpipe[WRITE]);

        close(stdinpipe[WRITE]);			// parent write
        close(STDIN_FILENO);
        if (dup2(stdinpipe[READ], STDIN_FILENO) < 0)
            UnixError::throwMe();
        close(stdinpipe[READ]);

    //  std::cerr << "execl(\"" << toolExecutable << "\")" << std::endl;
        execv(toolPath, args.size()?const_cast<char * const *>(&args[0]):NULL);		// requires that args is a <vector> !!
    //  std::cerr << "execl of " << toolExecutable << " failed, errno=" << errno << std::endl;
    }
    catch (...)
    {
        int err = errno;
        _exit(err);
    }
    
    // Unconditional suicide follows.
    _exit(1);
}

int ExecCLITool::parent(pid_t pid)
{
	static const int timeout = 2 * 24 * 60 * 60 * 10;	// days-hours-min-sec-"1/10th of a second" ==> 2 days [1728000]
	static const bool dontNeedToWait = false;

    close(stdinpipe[READ]);			// child read
    close(stdoutpipe[WRITE]);		// child write

    iodatavec::iterator ix;
    parentReadOutput();
    for (ix = inputdata.begin();ix!=inputdata.end();++ix)
    {
        parentWriteInput((char *)**ix,(**ix).length());
        parentReadOutput();
    }

    // Close pipes; we're done
    close(stdoutpipe[READ]);
	close(stdinpipe[WRITE]);
    
    struct timespec rqtp = {0,};
    rqtp.tv_nsec = 100000000;		// 10^8 nanoseconds = 1/10th of a second
    for (int nn = timeout; nn > 0; nanosleep(&rqtp, NULL), nn--)
    {
        if (dontNeedToWait)
            break;
        int status = 0;
        switch (waitpid(pid, &status, WNOHANG))
        {
        case 0:				// child still running
        //    std::cerr << "child still running" << std::endl;
            break;
        case -1:			// error
        //    std::cerr << "waitpid error:" << errno << std::endl;
            switch (errno)
            {
            case EINTR:
            case EAGAIN:	// transient
                continue;
            case ECHILD:	// no such child (dead; already reaped elsewhere)
                CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);
            default:
                UnixError::throwMe();
            }
        default:
        //    std::cerr << "waitpid succeeded, pid=" << rc << ", status: " << status << std::endl;
            return WEXITSTATUS(status);
        }
    }
    return 0;	// never reached
}

void ExecCLITool::parentReadOutput()
{
    // parent - resulting blob comes in on stdoutpipe[READ]
	unsigned int totalRead = 0, dataLength = 0;
	char buffer[kReadBufSize];
	char *dataRead = (char *)malloc(kReadBufSize);	//new char[kReadBufSize];
//  auto_ptr<char> data(new char[length]);

	for (;;)
    {
		int thisRead = read(stdoutpipe[READ], buffer, kReadBufSize);
		if (thisRead < 0)
        {
			if (errno==EINTR)			// try some more
                continue;
            else
            if (errno==EAGAIN)			// with non-blocking I/O, means that there's nothing there
                dataLength = totalRead;
            else
                debugdisp("abnormal read end:");	//std::cerr << "abnormal read end:" << errno << std::endl;
            break;
		}
		if (thisRead == 0)				// normal termination
        {
			dataLength = totalRead;
            eofSeen[0] = true;
        //  debugdisp("Normal read end");
			break;
		}

		// Resize dataRead if necessary
        if (kReadBufSize < (totalRead + (unsigned int)thisRead))
        {
			uint32 newLen = dataLength + kReadBufSize;
			dataRead = (char *)realloc(dataRead, newLen);
			dataLength = newLen;
		}

		// Append the data to dataRead
        ::memmove(dataRead + totalRead, buffer, thisRead);
		totalRead += thisRead;
	}
    if (dataLength)
        addout(dataRead,dataLength);
    if (dataRead)
        ::free(dataRead);
//  close(stdoutpipe[READ]);
}

void ExecCLITool::parentWriteInput(const char *data,unsigned int length)
{
	if (length>0)
    {
        int bytesWritten = write(stdinpipe[WRITE],data,length);
        if (bytesWritten < 0)
            UnixError::throwMe();
    }
//  close(stdinpipe[WRITE]);
}

void ExecCLITool::closeAllPipes()
{
    for (int ix=0;ix<2;ix++)
        if (stdoutpipe[ix])
        {
            close(stdoutpipe[ix]);
            stdoutpipe[ix]=0;
        }

    for (int ix=0;ix<2;ix++)
        if (stdinpipe[ix])
        {
            close(stdinpipe[ix]);
            stdinpipe[ix]=0;
        }
}

#pragma mark -------------------- hdiutil specific --------------------

void ExecCLITool::addincfs(CFStringRef theString, bool appendNULL)
{
	// Used mainly for preserving UTF-8 passwords
    // hdiutil et al require the NULL to be sent as part of the password string from STDIN
    // @@@ This should move out to the FileVault library, as it is unique to running disk images
    Boolean isExternalRepresentation = false;
    CFStringEncoding encoding = kCFStringEncodingUTF8;
    CFIndex usedBufLen = 0;
    UInt8 lossByte = 0;

    if (!theString)
        MacOSError::throwMe(paramErr);

    CFRange stringRange = CFRangeMake(0,CFStringGetLength(theString));
    // Call once first just to get length
    CFIndex sz = CFStringGetBytes(theString, stringRange, encoding, lossByte,
        isExternalRepresentation, NULL, 0, &usedBufLen);
    
    unsigned int length = appendNULL?sz+2:sz;
    if (sz>0)
    {
        auto_ptr<char> data(new char[length]);
        char *pdata=data.get();
        sz = CFStringGetBytes(theString, stringRange, encoding, lossByte, isExternalRepresentation, 
            reinterpret_cast<UInt8 *>(pdata), length, &usedBufLen);
    
        if (appendNULL)
        {
            pdata[length-2]=0;
            pdata[length-1]='\n';
        }
        addin(pdata,length);
    }
}

#pragma mark -------------------- Debugging functions --------------------

void ExecCLITool::dump(iodatavec& vec)
{
    unsigned char buffer[1024];
    iodatavec::iterator ix;
    for (ix = vec.begin();ix!=vec.end();++ix)
    {
        ::memcpy(buffer,(unsigned char *)**ix,(**ix).length());
        buffer[(**ix).length()]=0;
        debugdisp(reinterpret_cast<char *>(buffer));
    }
}

void ExecCLITool::debugdisp(const char *msg)
{
#if !defined(NDEBUG)
	std::cout << msg << std::endl;
#endif
}
