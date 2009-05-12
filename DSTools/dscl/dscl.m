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
#import <histedit.h>
#import "PathManager.h"
#import "DSoStatus.h"
#import "DSoException.h"
#import "DSCLCommandHistory.h"
#import "NSStringEscPath.h"
#import "dstools_version.h"
#import <sysexits.h>
#import <sys/sysctl.h>

#import "PlugInManager.h"		// ATM - support for plugins

#define streq(A,B) (strcmp(A,B) == 0)
#define forever for(;;)

#warning VERIFY the version string before each major OS build submission
#define DSCL_VERSION "10.5.3"

static char			myname[256];
PathManager		   *engine			= nil;
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
	2,  // setrdn
	4,  // change
	4,  // changei
	1,  // authonly 
	1,  // readall
	2,  // diff
	3,  // readpl
	4,  // createpl
	3,  // deletepl
	4,	// readpli
	5,	// createpli
	4	// deletepli
};

#define OP_NOOP				0
#define OP_CREATE			1
#define OP_DELETE			2
#define OP_READ				4
#define OP_LIST				5
#define OP_APPEND			6
#define OP_MERGE			7
#define OP_SEARCH			11
#define OP_PUSHD			13
#define OP_POPD				14
#define OP_CD				16
#define OP_PASSWD			18
#define OP_AUTH				23
#define OP_CHANGE			26
#define OP_CHANGE_INDEX		27
#define OP_AUTH_ONLY		28
#define OP_READALL			29
#define OP_DIFF				30
#define OP_READPL			31
#define	OP_CREATEPL			32
#define	OP_DELETEPL			33
#define OP_READPL_INDEX		34
#define	OP_CREATEPL_INDEX	35
#define	OP_DELETEPL_INDEX	36

void usage()
{
    fprintf(stderr, "dscl (v%s)\n", DSCL_VERSION);
	fprintf(stderr, "usage: %s [options] [<datasource> [<command>]]\n", myname);
    fprintf(stderr, "datasource:\n");
    fprintf(stderr, "    localhost    (default)                                    or\n");
    fprintf(stderr, "    localonly    (activates a DirectoryService daemon process   \n");
	fprintf(stderr, "                  with Local node only - daemon quits after use \n");
    fprintf(stderr, "    <hostname>   (requires DS proxy support, >= DS-158)       or\n");
    fprintf(stderr, "    <nodename>   (Directory Service style node name)          or\n");
    fprintf(stderr, "    <domainname> (NetInfo style domain name)\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "    -u <user>      authenticate as user (required when using DS Proxy)\n");
	fprintf(stderr, "    -P <password>  authentication password\n");
	fprintf(stderr, "    -p             prompt for password\n");
	fprintf(stderr, "    -f <filepath>  targeted file path for DS daemon running in localonly mode\n");
	fprintf(stderr, "                   (example: /Volumes/Build100/var/db/dslocal/nodes/Default) \n");
	fprintf(stderr, "                   (NOTE: Nodename to use is fixed at /Local/Target) \n");
	fprintf(stderr, "    -raw           don't strip off prefix from DS constants\n");
	fprintf(stderr, "    -plist         print out record(s) or attribute(s) in XML plist format\n");
	fprintf(stderr, "    -url           print record attribute values in URL-style encoding\n");
	fprintf(stderr, "    -q             quiet - no interactive prompt\n");
	fprintf(stderr, "commands:\n");
	fprintf(stderr, "    -read      <path> [<key>...]\n");
	fprintf(stderr, "    -readall   <path> [<key>...]\n");
	fprintf(stderr, "    -readpl    <path> <key> <plist path>\n");
	fprintf(stderr, "    -readpli   <path> <key> <value index> <plist path>\n");
	fprintf(stderr, "    -create    <record path> [<key> [<val>...]]\n");
	fprintf(stderr, "    -createpl  <record path> <key> <plist path> <val1> [<val2>...]\n");
	fprintf(stderr, "    -createpli <record path> <key> <value index> <plist path> <val1> [<val2>...]\n");
	fprintf(stderr, "    -delete    <path> [<key> [<val>...]]\n");
	fprintf(stderr, "    -deletepl  <record path> <key> <plist path> [<val>...]\n");
	fprintf(stderr, "    -deletepli <record path> <key> <value index> <plist path> [<val>...]\n");
	fprintf(stderr, "    -list      <path> [<key>]\n");
	fprintf(stderr, "    -append    <record path> <key> <val>...\n");
	fprintf(stderr, "    -merge     <record path> <key> <val>...\n");
	fprintf(stderr, "    -change    <record path> <key> <old value> <new value>\n");
	fprintf(stderr, "    -changei   <record path> <key> <value index> <new value>\n");
	fprintf(stderr, "    -diff      <first path> <second path>\n");
	fprintf(stderr, "    -search    <path> <key> <val>\n"); 
	fprintf(stderr, "    -auth      [<user> [<password>]]\n");
	fprintf(stderr, "    -authonly  [<user> [<password>]]\n");
	fprintf(stderr, "    -passwd    <user path> [<new password> | <old password> <new password>]\n");
	
	// ATM - give the plugins a chance to display their own help
	dscl_PlugInShowUsage(stderr);
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
dscl_diff(u_int32_t dsid, int argc, char *argv[])
{
	// <path1> <path2> [<key> ...] 
	
    NSString		   *path1	= nil;
    NSString		   *path2	= nil;
    NSMutableArray     *keys	= nil;
	
    path1 = [[NSString alloc] initWithUTF8String:argv[0]];
    path2 = [[NSString alloc] initWithUTF8String:argv[1
		]];
    argc -= 2;
    argv += 2;
    
    keys = [[NSMutableArray alloc] initWithCapacity:argc];
    
	while (argc > 0)
	{
        [keys addObject:[NSString stringWithUTF8String:argv[0]]];
		argc--;
		argv++;
	}
	
    NS_DURING
		[engine diff:path1 otherPath:path2 keys:keys];
	NS_HANDLER
		[keys release];
		[path1 release];
		[path2 release];
		[localException raise];
	NS_ENDHANDLER
	
    [keys release];
    [path1 release];
    [path2 release];
	
	return eDSNoErr;
}


tDirStatus
dscl_read(u_int32_t dsid, int argc, char *argv[], BOOL all)
{
	// <path> [<key> ...] 

    NSString		   *path	= nil;
    NSMutableArray     *keys	= nil;

	if (argc == 0)
	{
		if (all)
			return [engine readAll:nil keys:nil];
		else
			return [engine read:nil keys:nil];
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
		if (all)
			NS_VALUERETURN(([engine readAll:path keys:keys]), tDirStatus);
		else
			NS_VALUERETURN(([engine read:path keys:keys]), tDirStatus);
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
dscl_readpl(u_int32_t dsid, int argc, char *argv[])
{
	// <path> <key> <plist path>

	NSString		   *path	= nil;
	NSString		   *key		= nil;
	NSString		   *plistPath = nil;

	path = [[NSString alloc] initWithUTF8String:argv[0]];
	key = [[NSString alloc] initWithUTF8String:argv[1]];
	plistPath = [[NSString alloc] initWithUTF8String:argv[2]];
		
	NS_DURING
		NS_VALUERETURN(([engine read:path key:key plistPath:plistPath]), tDirStatus);
	NS_HANDLER
		[path release];
		[localException raise];
	NS_ENDHANDLER

	[path release];
	
	return eDSNoErr;
}

tDirStatus
dscl_readpli(u_int32_t dsid, int argc, char *argv[])
{
	// <path> <key> <value index> <plist path>
	
	NSString		   *path	= nil;
	NSString		   *key		= nil;
	NSString		   *plistPath = nil;
	NSString		   *sindex	= nil;
	int				    index = 0;
	
	path = [[NSString alloc] initWithUTF8String:argv[0]];
	key = [[NSString alloc] initWithUTF8String:argv[1]];
	sindex = [[NSString alloc] initWithUTF8String:argv[2]];
	plistPath = [[NSString alloc] initWithUTF8String:argv[3]];
	index = [sindex intValue];
	
	NS_DURING
		NS_VALUERETURN(([engine read:path key:key atIndex:index plistPath:plistPath]), tDirStatus);
	NS_HANDLER
		[path release];
		[localException raise];
	NS_ENDHANDLER
	
	[path release];
	
	return eDSNoErr;
}

tDirStatus
dscl_createpl(u_int32_t dsid, int argc, char *argv[])
{
	// <record path> <key> <plist path> <val1> [<val2>...]
	
	NSString		   *path	= nil;
	NSString		   *key		= nil;
	NSString		   *plistPath = nil;
	NSMutableArray	   *values	= nil;
	
	path = [[NSString alloc] initWithUTF8String:argv[0]];
	key = [[NSString alloc] initWithUTF8String:argv[1]];
	plistPath = [[NSString alloc] initWithUTF8String:argv[2]];
	unsigned int c;
	values = [[NSMutableArray alloc] init];
	for(c = 3; c < argc; c++)
	{
		[values addObject:[NSString stringWithUTF8String:argv[c]]];
	}
	
	NS_DURING
		NS_VALUERETURN(([engine create:path key:key plistPath:plistPath values:values]), tDirStatus);
	NS_HANDLER
		[path release];
		[localException raise];
	NS_ENDHANDLER
	
	[path release];
	return eDSNoErr;
}

tDirStatus
dscl_createpli(u_int32_t dsid, int argc, char *argv[])
{
	// <record path> <key> <plist path> <val1> [<val2>...]
	
	NSString		   *path	= nil;
	NSString		   *key		= nil;
	NSString		   *plistPath = nil;
	NSMutableArray	   *values	= nil;
	NSString		   *sindex	= nil;
	int					index	= 0;
	
	path = [[NSString alloc] initWithUTF8String:argv[0]];
	key = [[NSString alloc] initWithUTF8String:argv[1]];
	sindex = [[NSString alloc] initWithUTF8String:argv[2]];
	plistPath = [[NSString alloc] initWithUTF8String:argv[3]];
	index = [sindex intValue];
	
	unsigned int c;
	values = [[NSMutableArray alloc] init];
	for(c = 4; c < argc; c++)
	{
		[values addObject:[NSString stringWithUTF8String:argv[c]]];
	}
	
	NS_DURING
		NS_VALUERETURN(([engine create:path key:key atIndex:index plistPath:plistPath values:values]), tDirStatus);
	NS_HANDLER
		[path release];
		[localException raise];
	NS_ENDHANDLER
	
	[path release];
	return eDSNoErr;
}

tDirStatus
dscl_deletepl(u_int32_t dsid, int argc, char *argv[])
{
	// <path> <key> <plist path> [<val>...]
	NSString		   *path	= nil;
	NSString		   *key		= nil;
	NSString		   *plistPath = nil;
	NSMutableArray	   *values	= nil;
	
	path = [[NSString alloc] initWithUTF8String:argv[0]];
	key = [[NSString alloc] initWithUTF8String:argv[1]];
	plistPath = [[NSString alloc] initWithUTF8String:argv[2]];
	unsigned int c;
	values = [[NSMutableArray alloc] init];
	for(c = 3; c < argc; c++)
	{
		[values addObject:[NSString stringWithUTF8String:argv[c]]];
	}
	
	NS_DURING
		NS_VALUERETURN(([engine delete:path key:key plistPath:plistPath values:values]), tDirStatus);
	NS_HANDLER
		[path release];
		[localException raise];
	NS_ENDHANDLER
	
	[path release];
	return eDSNoErr;
}

tDirStatus
dscl_deletepli(u_int32_t dsid, int argc, char *argv[])
{
	// <path> <key> <value index> <plist path> [<val>...]
	NSString		   *path	= nil;
	NSString		   *key		= nil;
	NSString		   *plistPath = nil;
	NSMutableArray	   *values	= nil;
	NSString		   *sindex	= nil;
	int					index	= 0;
	
	path = [[NSString alloc] initWithUTF8String:argv[0]];
	key = [[NSString alloc] initWithUTF8String:argv[1]];
	sindex = [[NSString alloc] initWithUTF8String:argv[2]];
	plistPath = [[NSString alloc] initWithUTF8String:argv[3]];
	index = [sindex intValue];
	
	unsigned int c;
	values = [[NSMutableArray alloc] init];
	for(c = 4; c < argc; c++)
	{
		[values addObject:[NSString stringWithUTF8String:argv[c]]];
	}
	
	NS_DURING
		NS_VALUERETURN(([engine delete:path key:key atIndex:index plistPath:plistPath values:values]), tDirStatus);
	NS_HANDLER
		[path release];
		[localException raise];
	NS_ENDHANDLER
	
	[path release];
	return eDSNoErr;
}



tDirStatus
dscl_list(u_int32_t dsid, int argc, char *argv[])
{
	// <path> [<key>] 
	
    NSString *path = nil;
    NSString *key = nil;
    
	if (argc > 0)
	{
		if (argv[0] != NULL)
			path = [[NSString alloc] initWithUTF8String:argv[0]];
		if (argc > 1 && argv[1] != NULL)
			key = [[NSString alloc] initWithUTF8String:argv[1]];
	}
    [engine list:path key:key];
    [path release];
    [key release];
	
	return eDSNoErr;
}

tDirStatus
dscl_cd(u_int32_t dsid, int argc, char *argv[])
{
    if (argc > 0 && argv[0] != NULL)
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
		for (;argc > 0; argc--, argv++) {
			[passwordItems addObject:[NSString stringWithUTF8String:argv[0]]];
			bzero( argv[0], strlen(argv[0]) );
		}
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
	else if (streq(cmd, "createpl"))	op = OP_CREATEPL;
	else if (streq(cmd, "createpli"))	op = OP_CREATEPL_INDEX;
	else if (streq(cmd, "delete"))		op = OP_DELETE;
	else if (streq(cmd, "destroy"))		op = OP_DELETE;
	else if (streq(cmd, "destroyprop"))	op = OP_DELETE;
	else if (streq(cmd, "deletepl"))	op = OP_DELETEPL;
	else if (streq(cmd, "deletepli"))	op = OP_DELETEPL_INDEX;
	else if (streq(cmd, "read"))		op = OP_READ;
	else if (streq(cmd, "readall"))		op = OP_READALL;
	else if (streq(cmd, "readpl"))		op = OP_READPL;
	else if (streq(cmd, "readpli"))		op = OP_READPL_INDEX;
	else if (streq(cmd, "diff"))		op = OP_DIFF;
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

	// ATM - the command is not a built-in command; see if any of the plugin(s) will claim it
	else if (dscl_PlugInDispatch(cc, cv, interactive, dsid, engine, &status))
	{
		// Exit codes seem to get truncated to 0...255 range so if we get something outside this range, convert to something standardized
		if (status < 0 || status > EX__MAX /* 78 */)
			status = EX_OSERR;
		return status;
	}
	
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
		return EX_USAGE;
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
			status = dscl_read(dsid, cc, cv, NO);
			break;
		case OP_READALL:
			status = dscl_read(dsid, cc, cv, YES);
			break;
		case OP_READPL:
			status = dscl_readpl(dsid, cc, cv);
			break;
		case OP_READPL_INDEX:
			status = dscl_readpli(dsid, cc, cv);
			break;
		case OP_CREATEPL:
			status = dscl_createpl(dsid, cc, cv);
			break;
		case OP_CREATEPL_INDEX:
			status = dscl_createpli(dsid, cc, cv);
			break;
		case OP_DELETEPL:
			status = dscl_deletepl(dsid, cc, cv);
			break;
		case OP_DELETEPL_INDEX:
			status = dscl_deletepli(dsid, cc, cv);
			break;
		case OP_DIFF:
			status = dscl_diff(dsid, cc, cv);
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
            status = [(DSoException*)localException status];
        }
        else
            [localException raise];
    NS_ENDHANDLER
	if (status != eDSNoErr)
		fprintf(stderr, "<dscl_cmd> DS Error: %d (%s)\n", status, [[DSoStatus sharedInstance] cStringForStatus:status]);
		
	// Reverse the sign in order to exit with a positive value.
	// Certain external tool expect this to be positive.
	return -status;
}

char *
dscl_get_string(char **s)
{
	char	   *p		= nil;
	char	   *x		= nil;
	int			i		= 0;
	int			quote   = 0;
	int			esc		= 0;

	if (*s == NULL) return NULL;
	if (**s == '\0') return NULL;

	// Skip leading white space 
	while ((**s == ' ') || (**s == '\t')) *s += 1;

	if (**s == '\0') return NULL;

	x = *s;
	
	if (*x == '\"' || *x == '\'')
	{
		quote = 1;
		*s += 1;
		x = *s;
	}
	
	forever
	{
		if (x[i] == '\0') break;
		if ((quote == 1) && ((x[i] == '\"') || (x[i] == '\''))) break;
		if ((quote == 0) && (x[i] == ' ' )) break;
		if ((quote == 0) && (x[i] == '\t')) break;
		
		if (x[i] == '\\')
		{
			if (esc == 0) esc = i;
			i++;
			if (x[i] == '\0') break;
		}
		i++;
	}
	
	p = malloc(i+1);
	memmove(p, x, i);
	p[i] = 0;

	if (quote == 1) i++;
	*s += i;
    
	while (esc != 0)
	{
		i = esc + 1;
		if (p[i] == '\\') i++;
		esc = 0;
		for (; p[i] != '\0'; i++)
		{
			p[i-1] = p[i];
			if ((p[i] == '\\') && (esc == 0)) esc = i - 1;
		}
		p[i - 1] = '\0';
	}

	return p;
}

int
dscl_tabcomplete(EditLine *e, int ch)
{
	NSAutoreleasePool      *pool				= [[NSAutoreleasePool alloc] init];
	NSArray				   *possibleCompletions = nil;
    int						cntLimit			= 0;
    const LineInfo		   *li					= el_line(e);

	// Remove any pre-existing trailing slash:
	if (*(li->lastchar) == '/')
	{
        el_deletestr(e, 1);
	}
    
    NSString            *inputLine      = [[NSString alloc] initWithBytes:li->buffer length:(li->lastchar - li->buffer) encoding:NSUTF8StringEncoding];
    NSRange             cmdSeparator    = [inputLine rangeOfString:@" "]; // find the command-line separator
    
    if( cmdSeparator.location != NSNotFound ) 
    {
        NSMutableString     *completePath   = [NSMutableString stringWithString:[inputLine substringFromIndex: (cmdSeparator.location + 1)]];
        NSMutableArray      *lineComponents = [NSMutableArray arrayWithArray: [completePath unescapedPathComponents]];
        NSString            *partialPath    = [lineComponents lastObject];
        
        possibleCompletions = [engine getPossibleCompletionsFor: completePath];
        if (possibleCompletions != nil)
        {
            cntLimit = [possibleCompletions count];
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
                
                // Remove the partial item from the input line to
                // make room for the complete value.
                el_deletestr(e, [[partialPath escapedString] length]);

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
                    // Insert the partial completion into the input line
                    el_insertstr(e, [[partialMatch escapedString] UTF8String]);
                }
                else
                {
                    // Otherwise add a trailing slash and do in-line
                    // completion since this is the only completion.
                    el_insertstr(e, [[[partialMatch escapedString] stringByAppendingString:@"/"] UTF8String]);
                }
                
            }
            else
                putchar(7);
        }
        else
            putchar(7);
    }
	
    [inputLine release];
	[pool release];
	
    switch (cntLimit)
    {
        case 0:
            return CC_ARGHACK;
            break;
        case 1:
            return CC_REFRESH;
            break;
        default:
            return CC_REDISPLAY;
            break;
    }
}

static char *
dscl_input_line(FILE *in, EditLine *el, History *h)
{
	HistEvent			hev;
	const char		   *eline				= NULL;
	char 			   *out					= NULL;
    char				fline[INPUT_LENGTH];
	int					count				= 0;
    int					len					= 0;
	
	if (el == NULL)
	{
		count = fscanf(in, "%[^\n]%*c", fline);
		if (count < 0) return NULL;
		if (count == 0)
		{
			fscanf(in, "%*c");
			out = calloc(1, 1);
			return out;
		}
		len = strlen(fline);
		out = malloc(len + 1);
		memmove(out, fline, len);
		out[len] = '\0';
		return out;
	}
    
	eline = el_gets(el, &count);
	if (eline == NULL) return NULL;
	if (count <= 0) return NULL;

    
    // Strip trailing '/' if it's not by itself. -- Rajpaul
    if (eline[count-2] == '/' && eline[count-3] != ' ')
        len = count - 2;
    else
        len = count - 1;

	out = malloc(len + 1);
	memmove(out, eline, len);
	out[len] = '\0';
	if (len > 0) history(h, &hev, H_ENTER, out);
    
	return out;
}

char *
dscl_prompt(EditLine *el)
{
    NSString *P = [NSString stringWithFormat:@"\e[1m%@\e[0m > ", [[engine cwd] unescapedString]];
    return (char*)[P UTF8String];
}

int
dscl_interactive(FILE *in, int pmt)
{
	char			   *s					= nil;
	char			   *p					= nil;
	char			  **iargv				= NULL;
	char			   *line				= NULL;
	int					i					= 0;
	int					iargc				= 0;
	int					quote				= 0;
	int					esc					= 0;
	EditLine		   *el					= NULL;
	History			   *h					= NULL;
	HistEvent			hev;
    NSAutoreleasePool  *foreverPool			= nil;

    interactive = 1;

	if (pmt != PROMPT_NONE)
	{
		el = el_init("dscl", in, stdout, stderr);
		h = history_init();
        
		el_set(el, EL_HIST, history, h);
		el_set(el, EL_PROMPT, dscl_prompt);
		el_set(el, EL_EDITOR, "emacs");
		el_set(el, EL_SIGNAL, 1);
		el_set(el, EL_EDITMODE, 1);
        
        // Tell the command line editor library to use
        // the tab completion routine when tab is pressed.
        el_set(el, EL_ADDFN, "tabcomplete", "Tab completion", dscl_tabcomplete);
        el_set(el, EL_BIND, "^I", "tabcomplete", NULL);
        
		history(h, &hev, H_SETSIZE, 100000);
	}
    
	forever
	{
        foreverPool = [[NSAutoreleasePool alloc] init];
		line = dscl_input_line(in, el, h);
        
		if (line == NULL) break;
		if (line[0] == '\0')
		{
			free(line);
			continue;
		}

		if (streq(line, "exit")) break;
		if (streq(line, "quit")) break;
		if (streq(line, "q")) break;

		p = line;
		iargc = 0;
		quote = 0;
		esc = 0;

		forever
		{
			s = dscl_get_string(&p);
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
		free(line);

        [foreverPool release];
	}

	return eDSNoErr;
}

uint32_t isSingleUserMode( void )
{
	uint32_t	sb = 0;
	size_t		sbsz = sizeof(sb);
	
	if ( sysctlbyname("kern.singleuser", &sb, &sbsz, NULL, 0) == -1 )
		return NO;
	
	return sb;	
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
	int						opt_filePath	= 0;
	int						prompt			= PROMPT_DS;
    char				   *slash			= nil;
    char				   *dataSource		= nil;
    tDirStatus				status			= eDSNoErr;
    NSString			   *auth_user		= nil;
	NSString			   *auth_password   = nil;
	NSString			   *filePath		= nil;
    extern BOOL				gRawMode;
	extern BOOL				gPlistMode;
	
    @try {
        
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
        else if (streq(argv[i], "-plist"))	gPlistMode = 1;
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
        else if (streq(argv[i], "-f"))
        {
            i++;
            opt_filePath = i;
        }
        else break;
    }

    if (i == argc)
    {
        printf("Entering interactive mode... (type \"help\" for commands)\n");
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

	if (opt_filePath != 0)
		filePath = [NSString stringWithUTF8String:argv[opt_filePath]];

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

	if (filePath != NULL)
	{
        // The data source is the raw local file, open the local only DS service.
        engine = [[PathManager alloc] initWithLocalPath:filePath];
	}
    else if (dataSource[0] == '/')
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
    else if (strcasecmp(dataSource, "localonly") == 0)
    {
        // The data source is the localonly, open the local only DS service.
        engine = [[PathManager alloc] initWithLocalPath:@"Default"];
    }
    else
    {
        // assume the data source is a remote host to be contacted via DS proxy.
        engine = [[PathManager alloc] initWithHost:[NSString stringWithUTF8String:dataSource]
                                        user:auth_user
                                        password:auth_password];
    }
    
    if (engine != nil)
    {
        i++;
        if (i >= argc)
        {
            status = dscl_interactive(stdin, prompt);
            if (prompt != PROMPT_NONE) printf("Goodbye\n");
        }
        else status = dscl_cmd(argc - i, argv + i);
    }
    else
    {
		status = EX_UNAVAILABLE;
        printf("Data source (%s) is not valid.\n", dataSource);
    }

    } @catch( DSoException *exception ) {
		
		if ([[exception name] isEqualToString:@"DSOpenDirServiceErr"]) {
			printf("Cannot open remote host, error: DSOpenDirServiceErr\n");
		} else {
			status = [exception status];
			switch ( status )
			{
				case eServerSendError:
				case eServerNotRunning:
					if ( isSingleUserMode() ) {
						fprintf( stderr, "For Single User Mode you must run the following command to enable use of dscl.\n" );
						fprintf( stderr, "launchctl load /System/Library/LaunchDaemons/com.apple.DirectoryServicesLocal.plist\n" );
						status = EX_CONFIG;
						break;
					}
				default:
					fprintf( stderr, "Operation failed with error: %s\n", [[exception statusString] UTF8String] );
					status = EX_SOFTWARE;
					break;
			}
		}
	} @catch ( id exception ) {
		fprintf( stderr, "*** Uncaught Exception: <%s> (%s)\n", [[exception name] UTF8String], [[exception reason] UTF8String] );
		status = EX_SOFTWARE;
	}
	
    [engine release];
    
    [pool release];

	exit(status);
}
