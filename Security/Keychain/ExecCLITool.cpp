
#include <sys/wait.h>
#include "ExecCLITool.h"
#include <Security/utilities.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#pragma mark -------------------- ExecCLITool implementation --------------------

ExecCLITool::ExecCLITool() : dataRead(NULL),dataLength(0),dataToWrite(NULL),dataToWriteLength(0)
{
	stdinpipe[0]=0, stdinpipe[1]=0;
    stdoutpipe [0]=0, stdoutpipe [1]=0;
}

ExecCLITool::~ExecCLITool()
{
	if (dataRead)
        free(dataRead);
    reset();
}

int ExecCLITool::run(const char *toolPath, const char *toolEnvVar, ...)
{
    try
    {
        reset();
        initialize();
        
        // try to run the tool
        switch (pid_t pid = fork())
        {
        case 0:		// child
            {
                VAArgList arglist;
                va_list params;
                va_start(params, toolEnvVar);
                arglist.set(toolPath,params);
                va_end(params);
                child(toolPath,toolEnvVar,arglist);
            }
            break;
        case -1:	// error (in parent)
            UnixError::throwMe();
            break;
        default:	// parent
            parent(pid);
            break;
        }
    }
    catch (...)
    {
        closeAllPipes();
        return errno;
    }
    
	closeAllPipes();
    return 0;
}

void ExecCLITool::reset()
{
	closeAllPipes();
#if 0
    if (dataToWrite)
    {
        free(dataToWrite);
        dataToWrite = NULL;
    }
    dataToWriteLength = 0;
#endif
}

void ExecCLITool::input(const char *data,unsigned int length)
{
    if (dataToWrite)
    {
        ::free(dataToWrite);
        dataToWrite = NULL;
    }
    dataToWriteLength=length;
    if (!data)
        return;

    dataToWrite=reinterpret_cast<char *>(malloc(length));
    ::memmove(dataToWrite, data, dataToWriteLength);
}

void ExecCLITool::input(CFStringRef theString, bool appendNULL)
{
	// Used mainly for preserving UTF-8 passwords
    // hdiutil et al require the NULL to be sent as part of the password string from STDIN
    Boolean isExternalRepresentation = false;
    CFStringEncoding encoding = kCFStringEncodingUTF8;
    CFIndex usedBufLen = 0;
    UInt8 lossByte = 0;

    if (!theString)
        MacOSError::throwMe(paramErr);

    CFRange stringRange = CFRangeMake(0,CFStringGetLength(theString));
    // Call once first just to get length
    CFIndex length = CFStringGetBytes(theString, stringRange, encoding, lossByte,
        isExternalRepresentation, NULL, 0, &usedBufLen);
        
    if (dataToWrite)
        ::free(dataToWrite);
    dataToWriteLength=usedBufLen;
    if (appendNULL)
    {
        dataToWriteLength++;
        dataToWriteLength++;
}

    dataToWrite=reinterpret_cast<char *>(malloc(dataToWriteLength));
    length = CFStringGetBytes(theString, stringRange, encoding, lossByte, isExternalRepresentation, 
        reinterpret_cast<UInt8 *>(dataToWrite), dataToWriteLength, &usedBufLen);

    if (appendNULL)
    {
        dataToWrite[dataToWriteLength-1]=0;
        dataToWrite[dataToWriteLength]='\n';
    }
}

void ExecCLITool::initialize()
{
    dataLength = 0;								// ignore any previous output on new run
   
    if (!dataRead)								// Allocate buffer for child's STDOUT return
    {
        dataRead = (char *)malloc(256);
        if (!dataRead)
            UnixError::throwMe();
    }

    // Create pipe to catch tool output
    if (pipe(stdoutpipe))						// for reading data from child into parent
        UnixError::throwMe();

    if (pipe(stdinpipe))						// for writing data from parent to child
        UnixError::throwMe();
}
        
void ExecCLITool::child(const char *toolPath, const char *toolEnvVar, VAArgList& arglist)
{
    // construct path to tool
    try
    {
        char toolExecutable[PATH_MAX + 1];
        const char *path = toolEnvVar ? getenv(toolEnvVar) : NULL;
        if (!path)
            path = toolPath;
        snprintf(toolExecutable, sizeof(toolExecutable), "%s", toolPath);
    
        close(stdoutpipe[0]);			// parent read
        close(STDOUT_FILENO);
        if (dup2(stdoutpipe[1], STDOUT_FILENO) < 0)
            UnixError::throwMe();
        close(stdoutpipe[1]);

        close(stdinpipe[1]);			// parent write
        close(STDIN_FILENO);
        if (dup2(stdinpipe[0], STDIN_FILENO) < 0)
            UnixError::throwMe();
        close(stdinpipe[0]);

    //  std::cerr << "execl(\"" << toolExecutable << "\")" << std::endl;
        execv(toolPath, const_cast<char * const *>(arglist.get()));
    //  std::cerr << "execl of " << toolExecutable << " failed, errno=" << errno << std::endl;
    }
    catch (...)
    {
        int err = errno;
//		closeAllPipes();
        _exit(err);
    }
    
    // Unconditional suicide follows.
    _exit(1);
}

void ExecCLITool::parent(pid_t pid)
{
	static const int timeout = 300;
	static const bool dontNeedToWait = false;

    close(stdinpipe[0]);			// child read
    close(stdoutpipe[1]);			// child write

	parentWriteInput();

    parentReadOutput();
    
    struct timespec rqtp = {0,};
    rqtp.tv_nsec = 100000000;		// 10^8 nanoseconds = 1/10th of a second
    for (int nn = timeout; nn > 0; nanosleep(&rqtp, NULL), nn--)
    {
        if (dontNeedToWait)
            break;
        int status;
        switch (waitpid(pid, &status, WNOHANG))
        {
        case 0:				// child still running
            break;
        case -1:			// error
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
        //  std::cerr << "waitpid succeeded, pid=" << rc << std::endl;
            return;
        }
    }
}

void ExecCLITool::parentReadOutput()
{
    // parent - resulting blob comes in on stdoutpipe[0]
	unsigned int totalRead = 0;
	char buffer[kReadBufSize];
	
	for (;;)
    {
		int thisRead = read(stdoutpipe[0], buffer, kReadBufSize);
		if (thisRead < 0)
        {
			if (errno==EINTR)			// try some more
                continue;
//			std::cerr << "abnormal read end:" << errno << std::endl;
            break;
		}
		if (thisRead == 0)				// normal termination
        {
			dataLength = totalRead;
//			std::cerr << "Normal read end" << std::endl;
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
        memmove(dataRead + totalRead, buffer, thisRead);
		totalRead += thisRead;
	}
    close(stdoutpipe[0]);

}

void ExecCLITool::parentWriteInput()
{
	if (dataToWriteLength>0)
    {
        int bytesWritten = write(stdinpipe[1],dataToWrite,dataToWriteLength);
        if (bytesWritten < 0)
            UnixError::throwMe();
    }
    close(stdinpipe[1]);
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

#pragma mark -------------------- VAArgList implementation --------------------

int VAArgList::set(const char *path,va_list params)
{
    va_list params2;
    va_copy(params2, params);

	// Count up the number of arguments
    int nn = 1;
	while (va_arg(params,const char *) != NULL)
        nn++;
    argn = nn;
    argv = (ArgvArgPtr *)malloc((nn + 1) * sizeof(*argv));
    if (argv == NULL)
        return 0;

    nn = 1;
	argv[0]=path;
    while ((argv[nn]=va_arg(params2,const char *)) != NULL)
    	nn++;
    mSet = true;
    return 0;
}

