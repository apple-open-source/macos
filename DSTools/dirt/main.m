/*
 * Copyright (c) 2000 - 2003 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header DSAuthenticate
 * Tool for testing authentication against DirectoryService API.
 * DIRT: the DIRectory Tool.
 */


#warning VERIFY the version string before each major OS build submission
#define DIRTVERSION "10.5.3"

#import <Foundation/Foundation.h>
#import "DSAuthenticate.h"
#import "DSAuthenticateLM.h"
#import "DSAuthenticateNT.h"
#import "DSException.h"
#import "DSStatus.h"
#import "dstools_version.h"
#import <unistd.h>
#import <termios.h>
#import <sysexits.h>

BOOL		doVerbose		= NO;
static BOOL sigIntRaised	= NO;

void catch_int(int sig_num);
void usage(void);

//-----------------------------------------------------------------------------
//	intcatch
//
//	Helper function for read_passphrase
//-----------------------------------------------------------------------------

volatile int intr;

void
intcatch(int dontcare)
{
	intr = 1;
}


//-----------------------------------------------------------------------------
//	read_passphrase
//
//	Returns: malloc'd C-str
//	Provides a secure prompt for inputting passwords
/*
 * Reads a passphrase from /dev/tty with echo turned off.  Returns the
 * passphrase (allocated with xmalloc), being very careful to ensure that
 * no other userland buffer is storing the password.
 */
//-----------------------------------------------------------------------------

char *
read_passphrase(const char *prompt, int from_stdin)
{
	char buf[1024], *p, ch;
	struct termios tio, saved_tio;
	sigset_t oset, nset;
	struct sigaction sa, osa;
	int input, output, echo = 0;
    
	if (from_stdin) {
		input = STDIN_FILENO;
		output = STDERR_FILENO;
	} else
		input = output = open("/dev/tty", O_RDWR);
    
	if (input == -1)
		fprintf(stderr, "You have no controlling tty.  Cannot read passphrase.\n");
    
	/* block signals, get terminal modes and turn off echo */
	sigemptyset(&nset);
	sigaddset(&nset, SIGTSTP);
	(void) sigprocmask(SIG_BLOCK, &nset, &oset);
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = intcatch;
	(void) sigaction(SIGINT, &sa, &osa);
    
	intr = 0;
    
	if (tcgetattr(input, &saved_tio) == 0 && (saved_tio.c_lflag & ECHO)) {
		echo = 1;
		tio = saved_tio;
		tio.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
		(void) tcsetattr(input, TCSANOW, &tio);
	}
    
	fflush(stdout);
    
	(void)write(output, prompt, strlen(prompt));
	for (p = buf; read(input, &ch, 1) == 1 && ch != '\n';) {
		if (intr)
			break;
		if (p < buf + sizeof(buf) - 1)
			*p++ = ch;
	}
	*p = '\0';
	if (!intr)
		(void)write(output, "\n", 1);
    
	/* restore terminal modes and allow signals */
	if (echo)
		tcsetattr(input, TCSANOW, &saved_tio);
	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
	(void) sigaction(SIGINT, &osa, NULL);
    
	if (intr) {
		kill(getpid(), SIGINT);
		sigemptyset(&nset);
		/* XXX tty has not neccessarily drained by now? */
		sigsuspend(&nset);
	}
    
	if (!from_stdin)
		(void)close(input);
	p = (char *)malloc(strlen(buf)+1);
    strcpy(p, buf);
	memset(buf, 0, sizeof(buf));
	return (p);
}

int main(int argc, char *argv[])
{
    tDirStatus				status				= eDSNoErr;
    NSAutoreleasePool      *pool				= [[NSAutoreleasePool alloc] init];
    DSAuthenticate		   *dsauth				= nil;
    NSString			   *username			= nil;
    NSString			   *password			= nil;
    NSString			   *sNodeToSearch		= nil;
	NSString			   *authMethod			= nil;
    BOOL					listNodesOnly		= NO;
    BOOL					searchLocalOnly		= NO;
    BOOL					useContactPath		= NO;
    BOOL					groupSearch			= NO; // Thus by default let's perform a user search.
    BOOL					reportStatusCodes   = NO;
    int						queryIterations		= 1;
    long					delayInSeconds		= 0;
    int						ch					= -1;
    char				   *localtime			= nil;
    time_t					nowtime;
    unsigned int			i					= 0;
    NSAutoreleasePool      *loopPool			= nil;
    DSException			   *dirtException		= nil;
    DSStatus			   *dsStat				= [[DSStatus sharedInstance] retain];
	
	if ( argc == 2 && strcmp(argv[1], "-appleversion") == 0 )
		dsToolAppleVersionExit( argv[0] );
	
    while ((ch = getopt(argc, argv, "u:p:nclvgxm:q:d:a:h?")) != -1)
	{
        switch (ch)
		{
			case 'u':
				username = [[NSString alloc] initWithCString:optarg];
				break;
			case 'p':
				password = [[NSString alloc] initWithUTF8String:optarg];
				break;
			case 'n':
				// Only list the nodes where the username is found
				listNodesOnly = YES;
				break;
			case 'l':
				// Only search the localnode
				searchLocalOnly = YES;
				break;
			case 'g':
				// Search for a group name, instead of a user.
				groupSearch = YES;
				break;
			case 'm':
				// Only search the specified node name
				sNodeToSearch = [[NSString alloc] initWithCString:optarg];
				break;
			case 'q':
				queryIterations = atoi(optarg);
				// if iterations parameter < 0, then loop forever by never incrementing the loop counter
				if ((int)queryIterations < 0) {
					fprintf(stderr, "The number of iterations must be a non-negative integer.\n");
					exit(-1);
				}
				break;
			case 'd':
				delayInSeconds = atol(optarg);
				if ((int)queryIterations < 0) {
					fprintf(stderr, "The delay (seconds) must be a non-negative integer.\n");
					exit(-2);
				}
				break;
			case 'c':
				useContactPath = YES;
				break;
			case 'v':
				doVerbose = YES;
				break;
			case 'x':
				reportStatusCodes = YES;
				break;
			case 'a':
				authMethod = [[NSString alloc] initWithCString:optarg];
				break;
			case '?':
			case 'h':
			default:
				usage();
        }
    }
    
    if (queryIterations != 1)
	{
        signal(SIGINT, catch_int);
        signal(SIGTERM, catch_int);
    }
    // If a username is specified without a password and a password is needed we prompt for it.
    if (username != nil && password == nil && !listNodesOnly) {
        password = [[NSString alloc] initWithUTF8String:read_passphrase("User password:",1)];
        NSLog([NSString stringWithFormat:@"password is : %@",password]);
    }
    
    if (username != nil && (password != nil || listNodesOnly))
	{
        NS_DURING
		if (!authMethod)
			dsauth = [[DSAuthenticate alloc] init];
		else if ([authMethod isEqualToString:NT_AUTH])
			dsauth = [[DSAuthenticateNT alloc] init];
		else if ([authMethod isEqualToString:LM_AUTH])
			dsauth = [[DSAuthenticateLM alloc] init];
		else {
			fprintf(stderr, "Error: the authentication method '%s' is not supported by the dirt tool.\n", [authMethod UTF8String]);
			exit(EX_USAGE);
		}
		
        for (i=0; (queryIterations == 0 ? TRUE : i < queryIterations) && (sigIntRaised == NO); i++ )
		{
            NS_DURING
            loopPool = [[NSAutoreleasePool alloc] init];
            if (queryIterations != 1)
			{
                nowtime = time(NULL);
                localtime = ctime(&nowtime);
                localtime[strlen(localtime)-1] = 0;
                printf("%d:\n%s ---------------- \n", i, localtime);
            }
			
            // Open DirectoryService by instantiating the dsauth object.
            if (useContactPath == YES)
                [dsauth useContactSearchPath];
                
            if (groupSearch == YES)
                [dsauth searchForGroups];
            
            if (searchLocalOnly == YES) 
                status = [dsauth authUserOnLocalNode:username password:password];
            else if (sNodeToSearch != nil)
                status = [dsauth authOnNodePath:sNodeToSearch username:username password:password];
            else if (listNodesOnly == YES)
			{
                int iList;
                NSArray *nodeList = [dsauth getListOfNodesWithUser:username];
                [nodeList retain];
                printf("%s %s was found in:\n", groupSearch ? "Group" : "User", 
                                                [username cString]);
				int iListLimit = [nodeList count];
                for (iList = 0; iList < iListLimit; iList++)
				{
                    printf("%s\n", [[nodeList objectAtIndex:iList] cString]);
                }
                [nodeList release];
            }
            else
                status = [dsauth authUserOnSearchPath:username password:password];
            
            [loopPool release];
            loopPool = nil;
            if (queryIterations != 1)
			{
                nowtime = time(NULL);
                localtime = ctime(&nowtime);
                localtime[strlen(localtime)-1] = 0;
				char *statusString = [dsStat cStringForStatus:status];
                printf("%s ---------------- status: %s (%d)\n", localtime, statusString, status);
				free(statusString);
				statusString = nil;
                if (reportStatusCodes) fprintf(stderr, "%d\n",status);
            }
            fflush(stdout);
            if (delayInSeconds > 0 && (i+1 < queryIterations || queryIterations == 0))
                sleep(delayInSeconds);
            if ((queryIterations > 1 && i+1 < queryIterations) || queryIterations == 0)
                [dsauth reset];

            NS_HANDLER
                if (queryIterations == 1)
                {
                    [localException retain];
                    [loopPool release];
                    [[localException autorelease] raise];
                }
                else
                {
                    dirtException = (DSException*)localException;
                    printf("%s -- DS status: %s (%d)\n", [[dirtException reason] cString],
                                        [dirtException statusCString], [dirtException status]);
                    
                    // If this is a eDSServerTimeout or eDSCannotAccessSession error, then probably DS shut down.
                    // We attempt to open a new connection to the DS server:
                    if ([dirtException status] == eDSServerTimeout || [dirtException status] == eDSCannotAccessSession)
                    {
                        printf("RESTARTING Directory Services.\n");
                        fflush(stdout);
                        [dsauth release];
                        dsauth = [[DSAuthenticate alloc] init];
                        if (reportStatusCodes) fprintf(stderr, "%d\n", [dirtException status]);
                    }
                                        
                    dirtException = nil;
                }
            NS_ENDHANDLER
        } // End for loop
        
        NS_HANDLER
            if ([localException isKindOfClass:[DSException class]])
            {
                dirtException = (DSException*)localException;
                fprintf(stderr, "%s -- DS status: %s (%d)\n", [[dirtException reason] cString],
                                            [dirtException statusCString], [dirtException status]);
                status = [dirtException status];
            }
            else
            {
                fprintf(stderr, "Catching unknown exception: <%s> %s\n", [[localException name] cString], [[localException reason] cString]);
                fprintf(stderr, "Attempting to clean up.\n");
            }
        NS_ENDHANDLER
        // Clean up, close Dir Server
        if (dsauth != nil) [dsauth release];
    }
    else 
        usage();

    if (username != nil)
		[username release];
    if (password != nil)
		[password release];
    if (sNodeToSearch != nil)
		[sNodeToSearch release];
		
    [pool release];
	
    if (doVerbose)
	{
		char *statusString = [dsStat cStringForStatus: status];
		printf("exiting cleanly, status: %s (%d)\n", statusString, status);
		free(statusString);
		statusString = nil;
	}
		
    [dsStat release];
    return status;
}

void catch_int(int sig_num)
{
    static BOOL calledOnce = NO;

    if (calledOnce)
        exit(3);
    sigIntRaised = YES;
    fprintf(stderr, "\n\n-------------------- Cleaning up, please wait. --------------------\n");
    fprintf(stderr, "  If you wish to exit immediately, issue CTRL-C or signal again.\n\n");
    fflush(stderr);
    calledOnce = YES;
}

void usage(void) {
    printf("DIRT: The DIRectory Tool for testing authentication against the\n" );
    printf("      DirectoryServices API.\n");
    printf("Version %s\n\n", DIRTVERSION);
    printf("Usage: dirt [-c] [-g] [-l | -m path | -n] [-q query_iterations [-d seconds]]\n"
	   "            [-a auth_method] -u username [-p password]\n\n");
    printf(" -l\t\tQuery Local Node only\n");
    printf(" -c\t\tQuery the Contacts Search Policy\n\t\t(default is to use the Authentication Search Policy)\n");
    printf(" -m path\tQuery on Node named by given path\n");
    printf(" -q n\t\tPerform the specified query operation n times.\n\t\t(specify 0 to loop forever)\n");
    printf(" -d n\t\tSleep n seconds between each query iteration.  Default is 0.\n");
    printf(" -n\t\tOnly list Nodes in Search Policy where user (or group) is found\n\t\t(password not required)\n");
    printf(" -g\t\tQuery for groups instead of users.\n");
	printf(" -a auth_method\tUse specified authentication method. Available methods:\n\t\t\tnt --> SMB-NT\n");
    printf("\n");
    exit(0);
}
