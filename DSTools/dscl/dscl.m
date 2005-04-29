/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * @header dscl
 * directory service command-line interface
 * Main command processor derived from nicl source.
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirectoryService.h>
#import <stdio.h>
#import <unistd.h>
#import <stdlib.h>
#import <sys/types.h>
#import <sys/ioctl.h>
#import <termios.h>
#import "PathManager.h"
#import "DSoStatus.h"
#import "DSoException.h"
#import "DSCLCommandHistory.h"
#import "NSStringEscPath.h"
#import "dstools_version.h"

#define streq(A,B) (strcmp(A,B) == 0)
#define forever for(;;)

#warning VERIFY the version string before each distinct build submission that changes the dscl tool
#define DSCL_VERSION "20.4"

static char			myname[256];
PathManager		   *engine			= nil;
DSCLCommandHistory *gCommandHistory = nil;
BOOL				gHACK			= NO;
BOOL				gURLEncode		= NO;
static int			interactive		= 0;

#define INPUT_LENGTH 4096

#define PROMPT_NONE			0
#define PROMPT_PLAIN		1
#define PROMPT_DS			2

#define ATTR_CREATE			0
#define ATTR_APPEND			1
#define ATTR_MERGE			2
#define ATTR_DELETE			3
#define ATTR_CHANGE			4
#define ATTR_CHANGE_INDEX   5

//not all of these are actually used but are placeholder carryovers from nicl
static int minargs[] =
{
   -1,  // noop 
	1,  // create 
	1,  // delete 
	3,  // rename 
	1,  // read 
	1,  // list 
	3,  // append 
	3,  // merge 
	4,  // insert 
	2,  // move 
	2,  // copy 
	3,  // search 
	1,  // path 
	0,  // pushd 
	0,  // popd 
	2,  // parent 
	1,  // cd 
	0,  // pwd 
	0,  // version history 
	0,  // stats 
	0,  // domain name 
	0,  // rparent 
	0,  // resync 
	1,  // authenticate 
	0,  // refs 
	2,   // setrdn
	4,  // change
	4,  // changei
	1   // authonly 
};

#define OP_NOOP			0
#define OP_CREATE		1
#define OP_DELETE		2
#define OP_READ			4
#define OP_LIST			5
#define OP_APPEND		6
#define OP_MERGE		7
#define OP_SEARCH		11
#define OP_PUSHD		13
#define OP_POPD			14
#define OP_CD			16
#define OP_PASSWD		18
#define OP_AUTH			23
#define OP_CHANGE		26
#define OP_CHANGE_INDEX 27
#define OP_AUTH_ONLY	28

void usage()
{
    fprintf(stderr, "dscl (v%s)\n", DSCL_VERSION);
	fprintf(stderr, "usage: %s [options] [<datasource> [<command>]]\n", myname);
    fprintf(stderr, "datasource:\n");
    fprintf(stderr, "    localhost    (default)                               or\n");
    fprintf(stderr, "    <hostname>   (requires DS proxy support, >= DS-158)  or\n");
    fprintf(stderr, "    <nodename>   (Directory Service style node name)     or\n");
    fprintf(stderr, "    <domainname> (NetInfo style domain name)\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "    -u <user>      authenticate as user (required when using DS Proxy)\n");
	fprintf(stderr, "    -P <password>  authentication password\n");
	fprintf(stderr, "    -p             prompt for password\n");
	fprintf(stderr, "    -raw           don't strip off prefix from DS constants\n");
	fprintf(stderr, "    -url           print record attribute values in URL-style encoding\n");
	fprintf(stderr, "    -q             quiet - no interactive prompt\n");
	fprintf(stderr, "commands:\n");
	fprintf(stderr, "    -read      <path> [<key>...]\n");
	fprintf(stderr, "    -create    <record path> [<key> [<val>...]]\n");
	fprintf(stderr, "    -delete    <path> [<key> [<val>...]]\n");
	fprintf(stderr, "    -list      <path> [<key>]\n");
	fprintf(stderr, "    -append    <record path> <key> <val>...\n");
	fprintf(stderr, "    -merge     <record path> <key> <val>...\n");
	fprintf(stderr, "    -change    <record path> <key> <old value> <new value>\n");
	fprintf(stderr, "    -changei   <record path> <key> <value index> <new value>\n");
	fprintf(stderr, "    -search    <path> <key> <val>\n"); 
	fprintf(stderr, "    -auth      [<user> [<password>]]\n");
	fprintf(stderr, "    -authonly  [<user> [<password>]]\n");
	fprintf(stderr, "    -passwd    <user path> [<new password> | <old password> <new password>]\n");
}

tDirStatus
dscl_attribute(u_int32_t dsid, int flag, int argc, char *argv[])
{
	// <path> [<key> [<val>...]] 

	tDirStatus		status  = eDSNoErr;
	u_int32_t		i;
    NSString	   *path	= nil;
	NSString	   *key		= nil;
    NSMutableArray *values  = nil;

	if (argc == 0) return eDSNoErr;

    path = [[NSString alloc] initWithUTF8String:argv[0]];
    
    argc--;
    argv++;
    
    if (argc > 0 && argv[0] != NULL)
    {
        // We have a key to use:
        key = [[NSString alloc] initWithUTF8String:argv[0]];
        argc--;
        argv++;
        if (argc > 0 && argv[0] != NULL)
        {
            // we have values
            NSString *val;
            values = [[NSMutableArray alloc] initWithCapacity:argc];
            for (i = 0; i < argc; i++)
            {
                val = [[NSString alloc] initWithUTF8String:argv[i]];
                [values addObject:val];
                [val release];
            }
        }
    }

	NS_DURING
		switch (flag)
		{
			case ATTR_CREATE:
				status = [engine createRecord:path key:key values:values];
				break;
			case ATTR_APPEND:
				status = [engine appendToRecord:path key:key values:values];
				break;
			case ATTR_MERGE:
				status = [engine mergeToRecord:path key:key values:values];
				break;
			case ATTR_DELETE:
				status = [engine deleteInRecord:path key:key values:values];
				break;
			case ATTR_CHANGE:
				status = [engine changeInRecord:path key:key values:values];
				break;
			case ATTR_CHANGE_INDEX:
				status = [engine changeInRecordByIndex:path key:key values:values];
				break;
		}
	NS_HANDLER
		[path release];
		[key release];
		[values release];
		[localException raise];
	NS_ENDHANDLER

	if (status != eDSNoErr)
		fprintf(stderr, "<main> attribute status: %s\n", [[DSoStatus sharedInstance] cStringForStatus:status]);
        
    [path release];
    [key release];
    [values release];
	
	return status;
}

tDirStatus
dscl_create(u_int32_t dsid, int argc, char *argv[])
{
	return dscl_attribute(dsid, ATTR_CREATE, argc, argv);
}

tDirStatus
dscl_append(u_int32_t dsid, int argc, char *argv[])
{
	return dscl_attribute(dsid, ATTR_APPEND, argc, argv);
}

tDirStatus
dscl_merge(u_int32_t dsid, int argc, char *argv[])
{
	return dscl_attribute(dsid, ATTR_MERGE, argc, argv);
}

tDirStatus
dscl_change(u_int32_t dsid, int argc, char *argv[])
{
	return dscl_attribute(dsid, ATTR_CHANGE, argc, argv);
}

tDirStatus
dscl_change_index(u_int32_t dsid, int argc, char *argv[])
{
	return dscl_attribute(dsid, ATTR_CHANGE_INDEX, argc, argv);
}

tDirStatus
dscl_delete(u_int32_t dsid, int argc, char *argv[])
{
	// <path> [<key> [<val>...]]
	
    tDirStatus status = eDSNoErr;
	
    if (argc == 0)
    {
        fprintf(stderr, "<main> delete requires a path\n");
    }
    if (argc == 1)
    {
        // We are deleting a whole record:
        status = [engine deleteRecord:[NSString stringWithUTF8String:argv[0]]];
        if (status != eDSNoErr)
            fprintf(stderr, "<main> delete status: %s\n", [[DSoStatus sharedInstance] cStringForStatus:status]);
    }
    else
    {
        // we are just deleting keys and/or values from a record.
        status = dscl_attribute(dsid, ATTR_DELETE, argc, argv);
    }

    return status;
}

tDirStatus
dscl_read(u_int32_t dsid, int argc, char *argv[])
{
	// <path> [<key> ...] 

    NSString		   *path	= nil;
    NSMutableArray     *keys	= nil;

	if (argc == 0)
	{
        [engine read:nil keys:nil];
		return eDSNoErr;
	}

    path = [[NSString alloc] initWithUTF8String:argv[0]];
    argc--;
    argv++;
    
    keys = [[NSMutableArray alloc] initWithCapacity:argc];
    
	while (argc > 0)
	{
        [keys addObject:[NSString stringWithUTF8String:argv[0]]];
		argc--;
		argv++;
	}
	
    NS_DURING
		[engine read:path keys:keys];
	NS_HANDLER
		[keys release];
		[path release];
		[localException raise];
	NS_ENDHANDLER

    [keys release];
    [path release];
	
	return eDSNoErr;
}


tDirStatus
dscl_list(u_int32_t dsid, int argc, char *argv[])
{
	// <path> [<key>] 
	
    NSString *path = nil;
    NSString *key = nil;
    
    if (argv[0] != NULL)
        path = [[NSString alloc] initWithUTF8String:argv[0]];
    if (argc > 1 && argv[1] != NULL)
        key = [[NSString alloc] initWithUTF8String:argv[1]];

    [engine list:path key:key];
    [path release];
    [key release];
	
	return eDSNoErr;
}

tDirStatus
dscl_cd(u_int32_t dsid, int argc, char *argv[])
{
    if (argv[0] != NULL)
        [engine cd:[NSString stringWithUTF8String:argv[0]]];
		
    return eDSNoErr;
}

tDirStatus
dscl_search(u_int32_t dsid, int argc, char *argv[])
{
	// <path> <key> <val> [<match type>]

	tDirStatus		status			= eDSNoErr;
	NSString	   *path			= [[NSString alloc] initWithUTF8String:argv[0]];
	NSString	   *key				= [[NSString alloc] initWithUTF8String:argv[1]];
	NSString	   *valuePattern	= [[NSString alloc] initWithUTF8String:argv[2]];
	NSString	   *matchType		= nil;

	if (argc > 3)
		matchType = [[NSString alloc] initWithUTF8String:argv[3]];

	NS_DURING
		[engine searchInPath:path forKey:key withValue:valuePattern matchType:matchType];
	NS_HANDLER
		[path release];
		[key release];
		[valuePattern release];
		[matchType release];
		[localException raise];
	NS_ENDHANDLER		

	[path release];
	[key release];
	[valuePattern release];
	[matchType release];
	
	return status;
}

tDirStatus 
dscl_pushd(u_int32_t dsid, int argc, char *argv[])
{
	// [<path>]

	tDirStatus	status  = eDSNoErr;
	NSString   *path	= nil;

	if (argc == 0)
		[engine pushd:nil];
	else
	{
		path = [[NSString alloc] initWithUTF8String:argv[0]];
		[engine pushd:path];
		[path release];
	}

	return status;
}

tDirStatus
dscl_popd(u_int32_t dsid, int argc, char *argv[])
{
	// <path> <dsid> 

	tDirStatus status = eDSNoErr;

	[engine popd];

	return status;
}

tDirStatus
dscl_set_password(u_int32_t dsid, int argc, char *argv[])
{
	NSString	   *path;
	id				passwordItems   = nil;
	tDirStatus		status			= eDSNoErr;

	if (argc < 1 || argc > 3)
	{
		return eDSAuthFailed;
	}
	else
	{
		path = [NSString stringWithUTF8String:argv[0]];
		if (path == nil) return eDSAuthFailed;
		argc--;
		argv++;
	}

	if (argc > 0)
	{
		passwordItems = [NSMutableArray arrayWithCapacity:argc];
		for (;argc > 0; argc--, argv++)
			[passwordItems addObject:[NSString stringWithUTF8String:argv[0]]];
	}
	else
	{
		passwordItems = [NSArray arrayWithObject:[NSString stringWithUTF8String:getpass("New Password: ")]];
	}

	status = [engine setPasswordForUser:path withParams:passwordItems];
	
	return status;	
}

tDirStatus
dscl_authenticate(int argc, char *argv[], BOOL inAuthOnly)
{
	NSString	   *u		= nil;
	NSString	   *p		= nil;
	tDirStatus		status  = eDSNoErr;

	if (argc == 0)
	{
		u = @"root";
	}
	else
	{
		u = [NSString stringWithUTF8String:argv[0]];
		if (u == nil) return eDSAuthFailed;
		argc--;
		argv++;
	}

	if (argc > 0)
	{
		p = [NSString stringWithUTF8String:argv[0]];
	}
	else
	{
		p = [NSString stringWithUTF8String:getpass("Password: ")];
	}
	
	if (p == NULL)
	{
		return eDSAuthFailed;
	}

	status = [engine authenticateUser:u password:p authOnly:inAuthOnly];
		
	return status;
}


int
dscl_cmd(int cc, char *cv[])
{
	tDirStatus	status  = eDSNoErr;
	int			op		= OP_NOOP;
	int			dsid	= 0;		//not used but left for future?
	int			do_path = 1;
	char	   *cmd		= nil;

	cmd = cv[0];
	if (cmd[0] == '-') cmd++;

	if (streq(cmd, "help"))
	{
		usage();
		return eDSNoErr;
	}

	else if (streq(cmd, "create"))		op = OP_CREATE;
	else if (streq(cmd, "createprop"))	op = OP_CREATE;
	else if (streq(cmd, "delete"))		op = OP_DELETE;
	else if (streq(cmd, "destroy"))		op = OP_DELETE;
	else if (streq(cmd, "destroyprop"))	op = OP_DELETE;
	else if (streq(cmd, "read"))		op = OP_READ;
	else if (streq(cmd, "list"))		op = OP_LIST;
	else if (streq(cmd, "append"))		op = OP_APPEND;
	else if (streq(cmd, "merge"))		op = OP_MERGE;
	else if (streq(cmd, "search"))		op = OP_SEARCH;
	else if (streq(cmd, "pushd"))		op = OP_PUSHD;
	else if (streq(cmd, "pd"))			op = OP_PUSHD;
	else if (streq(cmd, "popd"))		op = OP_POPD;
	else if (streq(cmd, "cd"))			op = OP_CD;
	else if (streq(cmd, "mk"))			op = OP_CREATE;
	else if (streq(cmd, "rm"))			op = OP_DELETE;
	else if (streq(cmd, "cat"))			op = OP_READ;
	else if (streq(cmd, "."))			op = OP_READ;
	else if (streq(cmd, "ls"))			op = OP_LIST;
	else if (streq(cmd, "passwd"))		op = OP_PASSWD;

	else if (streq(cmd, "auth") || streq(cmd, "su"))
	{
		op = OP_AUTH;
		do_path = 0;
	}
	
	else if (streq(cmd, "authonly"))
	{
		op = OP_AUTH_ONLY;
		do_path = 0;
	}
	
	else if (streq(cmd, "change"))		op = OP_CHANGE;
	else if (streq(cmd, "changei"))		op = OP_CHANGE_INDEX;

	else
	{
		usage();
		return eDSNoErr;
	}

	cc--;
	cv++;

	if ((interactive == 1) && (cc == 0) && (minargs[op] == 1))
	{
		// default path arg to current directory 
	}
	else if (cc < minargs[op])
	{
		fprintf(stderr, "Too few parameters for %s operation\n", cmd);
		usage();
		return eDSNoErr;
	}

	status = eDSNoErr;

    NS_DURING
	switch (op)
	{
		case OP_NOOP:
			status = eDSNoErr;
			break;
		case OP_CREATE:
			status = dscl_create(dsid, cc, cv);
			break;
		case OP_DELETE:
			status = dscl_delete(dsid, cc, cv);
			break;
		case OP_READ:
			status = dscl_read(dsid, cc, cv);
			break;
		case OP_LIST:
			status = dscl_list(dsid, cc, cv);
			break;
		case OP_APPEND:
			status = dscl_append(dsid, cc, cv);
			break;
		case OP_MERGE:
			status = dscl_merge(dsid, cc, cv);
			break;
		case OP_SEARCH:
			status = dscl_search(dsid, cc, cv);
			break;
		case OP_PUSHD:
			status = dscl_pushd(dsid, cc, cv);
			break;
		case OP_POPD:
			status = dscl_popd(dsid, cc, cv);
			break;
		case OP_CD:
			status = dscl_cd(dsid, cc, cv);
			break;
		case OP_PASSWD:
			status = dscl_set_password(dsid, cc, cv);
			break;
		case OP_AUTH_ONLY:
			status = dscl_authenticate(cc, cv, YES);
			break;
		case OP_AUTH:
			status = dscl_authenticate(cc, cv, NO);
			break;
		case OP_CHANGE:
			status = dscl_change(dsid, cc, cv);
			break;
		case OP_CHANGE_INDEX:
			status = dscl_change_index(dsid, cc, cv);
			break;
	}

    NS_HANDLER
        [engine restoreStack]; // In case the error happened before the stack was restored.

        if ([[localException name] isEqualToString:@"DSCL"])
        {
            printf("%s: %s\n", cmd, [[localException reason] UTF8String]);
            status = eDSUnknownNodeName;
        }
        else if ([localException isKindOfClass:[DSoException class]])
        {
            printf("%s: DS error: %s\n", cmd, [(DSoException*)localException statusCString]);
            status = -[(DSoException*)localException status];
        }
        else
            [localException raise];
    NS_ENDHANDLER
	
	return status;
}

char *
getString(char **s)
{
	char	   *p		= nil;
	char	   *x		= nil;
	int			i		= 0;
	int			quote   = 0;

	if (*s == NULL) return NULL;
	if (**s == '\0') return NULL;

	// Skip leading white space 
	while ((**s == ' ') || (**s == '\t')) *s += 1;

	if (**s == '\0') return NULL;

	x = *s;
	
	if (*x == '\"')
	{
		quote = 1;
		*s += 1;
		x = *s;
	}
	
	forever
	{
		if (x[i] == '\0') break;
		if ((quote == 1) && (x[i] == '\"')) break;
		if ((quote == 0) && (x[i] == ' ' )) break;
		if ((quote == 0) && (x[i] == '\t')) break;
		
		if (x[i] == '\\')
		{
			i++;
			if (x[i] == '\0') break;
		}
		i++;
	}
	
	p = malloc(i+1);
	memmove(p, x, i);
	p[i] = 0;

	*s += i;
	if (x[i] != '\0')
		*s += 1;

	return p;
}

int
dscl_tabcomplete(char *line, int *currentPos)
{
	NSAutoreleasePool      *pool				= [[NSAutoreleasePool alloc] init];
	NSArray				   *possibleCompletions = nil;

	// Remove any pre-existing trailing slash:
	if (line[*currentPos-1] == '/')
	{
		line[*currentPos-1] = '\0';
		putchar(8);
	}
    
    NSString            *inputLine      = [NSString stringWithUTF8String:line];
    NSRange             cmdSeparator    = [inputLine rangeOfString:@" "]; // find the comand-line separator
    
    if( cmdSeparator.location != NSNotFound ) 
    {
        NSMutableString     *completePath   = [NSMutableString stringWithString:[inputLine substringFromIndex: (cmdSeparator.location + 1)]];
        NSMutableArray      *lineComponents = [NSMutableArray arrayWithArray: [completePath unescapedPathComponents]];
        NSString            *partialPath    = [lineComponents lastObject];
        
        possibleCompletions = [engine getPossibleCompletionsFor: completePath];
        if (possibleCompletions != nil)
        {
            int cntLimit = [possibleCompletions count];
            if (cntLimit > 0)
            {
                // For now, newLine is just the current completion.
                NSEnumerator    *compEnum       = [possibleCompletions objectEnumerator];
                NSString        *partialMatch   = [compEnum nextObject];
                NSString        *currentItem;
                
                while( currentItem = [compEnum nextObject] )
                {
                    NSString *commonMatch = [currentItem commonPrefixWithString:partialMatch options:NSCaseInsensitiveSearch];
                    if( [commonMatch length] < [partialMatch length] )
                        partialMatch = commonMatch;
                }
                
                // Strip off previous path & prefix, and replace it with the new one:
                [lineComponents removeLastObject];
                [lineComponents addObject: partialMatch];
                
                // newLine now is going to represent the whole string:
                // Put it back together:
                NSString *finalLine = [NSString escapablePathFromArray:lineComponents];
                
                // Set the internal current cursor postion indicator:
                
                strlcpy( &line[cmdSeparator.location + 1], [finalLine UTF8String], INPUT_LENGTH-(cmdSeparator.location + 1) );
                *currentPos = strlen( line );
                
                // If there were multiple completion possibilities, list
                // them out for the user.
                if (cntLimit > 1)
                {
                    putchar('\n');
                    
                    compEnum = [possibleCompletions objectEnumerator];
                    while( currentItem = [compEnum nextObject] ) {
                        printf("%s \t", [[currentItem escapedString] UTF8String]);
                    }
                    
                    putchar('\n');
                    // re-print the prompt
                    printf("\e[1m%s\e[0m > %s", [[[engine cwd] unescapedString] UTF8String], line);
                }
                else
                {
                    // Otherwise add a trailing slash and do in-line
                    // completion since this is the only completion.
                    int pathLen = strlen([[partialPath escapedString] UTF8String]);
                    int i;
                    line[(*currentPos)++] = '/';
                    for (i = 0; i < pathLen; i++)
                        putchar(8);
                    printf("%s/", [[partialMatch escapedString] UTF8String]);
                }
                
            }
            else
                putchar(7);
        }
        else
            putchar(7);
    }
	
	[pool release];
	
	return 1;
}

int
dscl_myscan(char *line)
{
	int				i			= 0;
	int				j			= 0;
	int				c			= 1;
	BOOL			finished	= NO;
	struct termios  tty;
	struct termios  otty;

	// Get original tty settings and save them in otty
	tcgetattr(STDIN_FILENO, &otty);
	tty = otty;
	// Now set the terminal to char-by-char input
	tty.c_lflag     = tty.c_lflag & ~(ECHO | ECHOK | ICANON);
	tty.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO, TCSANOW, &tty);

	while (c && !finished && i < INPUT_LENGTH)
	{
		c = getchar();
		switch(c)
		{
			case '\t':
				dscl_tabcomplete(line, &i);
				break;
			case '\n':
				finished = YES;
				putchar('\n');
				if (i > 0)
				{
					//peel off trailing backslashes unless there is a blank in front of the backslash
					//ie. allow "cd /" but not "cd //" which would get converted into "cd /"
					while ((i > 2) && (line[i-1] == '/') && (line[i-2] != ' '))
						i--;
					line[i] = '\0';
					[gCommandHistory addCommand:[NSString stringWithUTF8String:line]];
				}
				break;
			case 8:   // Backspace
			case 127: // Delete
				if (i > 0)
				{
					line[--i] = '\0';
					putchar(8); putchar(' '); putchar(8);
				}
				break;
			case 27:   // ESC: We ignore it.
			  // Check for and ignore arrow keys:
				c = getchar();
				if (c == 91)
				{   // It's beginning to look like we have an arrow key
					c = getchar();
					if (c > 68 || c < 65)
					{   // we don't, put the characters back in the queue
						// (except for the ESC, ignore that)
						ungetc(c, stdin);
						ungetc(91, stdin);
					}
					else if (c == 65)
					{ // up
						for (j=0 ; j < i; j++)
							{ putchar(8); putchar(' '); putchar(8); }
						if ([gCommandHistory isClean])
						{
							line[i] = '\0';
							[gCommandHistory addTemporaryCommand:[NSString stringWithUTF8String:line]];
						}
						[[gCommandHistory previousCommand] getCString:line];
						printf("%s", line);
						i = strlen(line);
					}
					else if (c == 66)
					{ // down
						for (j=0 ; j < i; j++)
							{ putchar(8); putchar(' '); putchar(8); }
						[[gCommandHistory nextCommand] getCString:line];
						printf("%s", line);
						i = strlen(line);
					}
				}
				else  // Definitely not an arrow key, put the char back
					ungetc(c, stdin);
				break;
			default:
				line[i++] = c;
				putchar(c);
				break;
		}
	}
	
	/* Reset to the original settings */
	tcsetattr(STDIN_FILENO, TCSANOW, &otty);
	return strlen(line);
}

int
dscl_interactive(int prompt)
{
	char			   *s					= nil;
	char			   *p					= nil;
	char			  **iargv				= NULL;
	char				line[INPUT_LENGTH];
    NSString		   *P					= nil;
	tDirStatus			status				= eDSNoErr;
	int					i					= 0;
	int					iargc				= 0;
    NSAutoreleasePool  *foreverPool			= nil;
	
	gCommandHistory = [[DSCLCommandHistory alloc] init];
	interactive = 1;

	forever
	{
        foreverPool = [[NSAutoreleasePool alloc] init];
		memset(line, 0, INPUT_LENGTH);

		switch (prompt)
		{
			case PROMPT_NONE:
				break;
			case PROMPT_PLAIN:
				printf("> ");
				break;
			case PROMPT_DS:
                // Get the string representation of the absolute path
                // of the current_dir directory id.
                P = [[engine cwd] unescapedString];
				printf("\e[1m%s\e[0m > ", [P UTF8String]);
				break;
			default:
				break;
		}

		fflush(stdout);

		status = dscl_myscan(line);
		
		if (status == 0)
		{
			rewind(stdin);
			continue;
		}

		if (status == -1) break;
		if (streq(line, "quit")) break;
		if (streq(line, "q")) break;

		p = line;
		iargc = 0;

		forever
		{
			s = getString(&p);
			if (s == NULL) break;
			if (iargc == 0)
				iargv = (char **)malloc(sizeof(char *));
			else
				iargv = (char **)realloc(iargv, (iargc + 1) * sizeof(char *));
	
			iargv[iargc++] = s;
		}

		dscl_cmd(iargc, iargv);

		for (i = 0; i < iargc; i++) free(iargv[i]);
		free(iargv);
        
        [foreverPool release];
	}
	[gCommandHistory release];

	return eDSNoErr;
}

int
main(int argc, char *argv[])
{
    NSAutoreleasePool      *pool			= [[NSAutoreleasePool alloc] init];
    int						i				= 0;
	int						opt_tag			= 0;
	int						opt_promptpw	= 0;
	int						opt_user		= 0;
	int						opt_password	= 0;
	int						prompt			= PROMPT_DS;
    char				   *slash			= nil;
    char				   *dataSource		= nil;
    tDirStatus				status			= eDSNoErr;
    NSString			   *auth_user		= nil;
	NSString			   *auth_password   = nil;
    extern BOOL				gRawMode;
	
    NS_DURING
        
    slash = rindex(argv[0], '/');
    if (slash == NULL) strcpy(myname, argv[0]);
    else strcpy(myname, slash+1);

    if ( argc == 2 && strcmp(argv[1], "-appleversion") == 0 )
        dsToolAppleVersionExit( myname );
    
    interactive = 0;

    // Parse program options
    for (i = 1; i < argc; i++)
    {
        if (streq(argv[i], "-H"))			gHACK = YES;
        else if (streq(argv[i], "-q"))		prompt = PROMPT_NONE;
        else if (streq(argv[i], "-t"))		opt_tag = 1;
        else if (streq(argv[i], "-raw"))	gRawMode = 1;
        else if (streq(argv[i], "-p"))		opt_promptpw = 1;
		else if (streq(argv[i], "-url"))	gURLEncode = YES;
        else if (streq(argv[i], "-P"))
        {
            i++;
            opt_password = i;
        }
        else if (streq(argv[i], "-u"))
        {
            i++;
            opt_user = i;
        }
        else break;
    }

    if (i == argc)
    {
        usage();
        printf("Entering interactive mode...\n");
        dataSource = "localhost";
    }
    else
    {
        dataSource = argv[i];
    }

    if (opt_user) opt_promptpw = 1;
    if (opt_password) opt_promptpw = 0;

    if ((opt_user == 0) && ((opt_password == 1) || (opt_promptpw == 1)))
    {
        auth_user = @"root";
    }

    if (opt_user != 0)
        auth_user = [NSString stringWithUTF8String:argv[opt_user]];

    if (opt_password != 0)
    {
        auth_password = [NSString stringWithUTF8String:argv[opt_password]];
        
        // after retrieving the password from the command line, blank it out from the ps listing
        bzero( argv[opt_password], strlen(argv[opt_password]) );
    }
    else if (opt_promptpw == 1)
        auth_password = [NSString stringWithUTF8String:getpass("Password: ")];


    if (streq(dataSource, ".."))
        dataSource = "/NetInfo/..";
        
    if (dataSource[0] == '/')
    {
        // The data source begins with a "/", it's a node,
        // open the node via the local DS service.
        // (Authenticate to the node if necessary.)
        if (auth_user && auth_password)
            engine = [[PathManager alloc] initWithNodeName:[NSString stringWithUTF8String:dataSource]
                                                      user:auth_user
                                                  password:auth_password];
        else
            engine = [[PathManager alloc] initWithNodeName:[NSString stringWithUTF8String:dataSource]];
        
        if (engine == nil)
        {
            // For nicl backward compatibility:
            // If they specified an old, nicl-style domain, then re-try the previous
            // call if nothing found, but by prepending
            // "/NetInfo/root" to whatever the user typed
            engine = [[PathManager alloc] initWithNodeName:[NSString stringWithFormat:@"/NetInfo/root%s",dataSource]];
        }
            
    }
    else if (strcasecmp(dataSource, "eDSLocalHostedNodes") == 0)
    {
        // open the node via the local DS service.
		engine = [[PathManager alloc] initWithNodeEnum:eDSLocalHostedNodes];
        
    }
    else if (strcasecmp(dataSource, "eDSLocalNodeNames") == 0)
    {
        // open the node via the local DS service.
		engine = [[PathManager alloc] initWithNodeEnum:eDSLocalNodeNames];
        
    }
    else if (streq(dataSource, "."))
    {
        // The data source is the local node.
        if (auth_user && auth_password)
            engine = [[PathManager alloc] initWithLocalNodeAuthUser:auth_user
                                                           password:auth_password];
        else
            engine = [[PathManager alloc] initWithLocalNode];
    }
    else if (strcasecmp(dataSource, "localhost") == 0)
    {
        // The data source is the localhost, open the local DS service.
        engine = [[PathManager alloc] initWithLocal];
    }
    else
    {
        // assume the data source is a remote host to be contacted via DS proxy.
        NS_DURING
        engine = [[PathManager alloc] initWithHost:[NSString stringWithUTF8String:dataSource]
                                        user:auth_user
                                        password:auth_password];
        NS_HANDLER
            if ([[localException name] isEqualToString:@"DSOpenDirServiceErr"])
            {
                printf("Cannot open remote host, error: DSOpenDirServiceErr\n");
            }
            else
                [localException raise];
        NS_ENDHANDLER
    }
    
    if (engine != nil)
    {
        i++;
        if (i >= argc)
        {
            status = dscl_interactive(prompt);
            if (prompt != PROMPT_NONE) printf("Goodbye\n");
        }
        else status = dscl_cmd(argc - i, argv + i);
    }
    else
    {
        printf("Data source (%s) is not valid.\n", dataSource);
    }

    NS_HANDLER
        if ([localException isKindOfClass:[DSoException class]])
        {
            NSLog(@"*** Uncaught DS Exception: <%@> (%@)",[localException name], [(DSoException*)localException statusString]);
            status = -[(DSoException*)localException status];
        }
        else
        {
            NSLog(@"*** My Uncaught Exception: <%@> (%@)",[localException name], [localException reason]);
            status = 1;
        }
    NS_ENDHANDLER

    [engine release];
    
    [pool release];
	
	exit(status);
}
