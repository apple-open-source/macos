/*
 * Copyright (c) 2004-2010 Apple Inc. All rights reserved.
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
 * @header dsconfigldap
 */

#include <Security/Authorization.h>
#include <SystemConfiguration/SCDynamicStore.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#include <OpenDirectory/OpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>
#include <OpenDirectoryConfig/OpenDirectoryConfig.h>

#include <getopt.h>
#include <sysexits.h>

#include "dstools_version.h"

#pragma mark -
#pragma mark Functions

static void
usage(void)
{
	const char *progname = getprogname();
    printf("dsconfigldap: Add or remove LDAP server configurations\n"
		   "Version %s\n"
		   "Usage: %s -h\n"
		   "Usage: %s [-fvixsgemS] -a servername [-n configname] [-c computerid]\n"
		   "                    [-u username] [-p userpassword] [-l localusername]\n"
		   "                    [-q localuserpassword]\n"
		   "Usage: %s [-fvi] -r servername [-u username] [-p password]\n"
		   "                    [-l localusername] [-q localuserpassword]\n"
		   "  -f                 force authenticated bind/unbind\n"
		   "  -v                 log details\n"
		   "  -i                 prompt for passwords\n"
		   "  -s                 enforce not using cleartext authentication via policy\n"
		   "  -e                 enforce use of encryption capabilities via policy\n"
		   "  -m                 enforce use of man-in-middle capabilities via policy\n"
		   "  -g                 enforce use of packet signing capabilities via policy\n"
		   "  -x                 SSL connection to LDAP server\n"
		   "  -h                 display usage statement\n"
		   "  -a servername      add config of servername\n"
		   "  -r servername      remove config of servername, unbind if necessary\n"
		   "  -n configname      name to give this new server config\n"
		   "  -c computerid      name to use if when binding to directory\n"
		   "  -u username        username of a privileged network user for binding\n"
		   "  -p password        password of a privileged network user for binding\n"
		   "  -l username        username of a local administrator\n"
		   "  -q password        password of a local administrator\n"
		   "  -S                 add to authentication and contact search policy\n\n"
		   , TOOLS_VERSION, progname, progname, progname);
}

int
main(int argc, char *argv[])
{
    NSString	*serverName		= nil;
    NSString	*configName		= nil;
    NSString	*computerID		= nil;
    NSString	*userName		= nil;
    NSString	*userPassword	= nil;
    NSString	*localName		= nil;
    NSString	*localPassword	= nil;
    BOOL		appleVersion	= NO;
    BOOL		addServer		= NO;
    BOOL		removeServer	= NO;
    BOOL		force			= NO;
    BOOL		prompt			= NO;
    BOOL		useSSL			= NO;
    BOOL		ybErrors		= NO;
    BOOL        verbose			= NO;
    BOOL        secureAuthOnly	= NO;
    BOOL        defaultUser		= NO;
    BOOL        manInMiddle		= NO;
    BOOL        encryptPackets	= NO;
    BOOL        signPackets		= NO;
    BOOL        updateSearch	= YES;
    int         ch;
    int         longindex;
	
    const struct option longopts[] = {
        { "appleversion",	no_argument,		NULL,   1 },	// 0
        { "force",			no_argument,		NULL,   'f' },	// 1
        { "verbose",		no_argument,		NULL,   'v' },	// 2
        { "prompt",			no_argument,		NULL,   'i' },	// 3
        { "secureonly",		no_argument,		NULL,	's' },	// 4
        { "usessl",			no_argument,		NULL,   'x' },	// 5
        { "signpackets",	no_argument,		NULL,   'g' },	// 6
        { "encrypt",		no_argument,		NULL,	'e' },	// 7
        { "mim",			no_argument,		NULL,   'm' },	// 9
        { "add",			required_argument,	NULL,   'a' },	// 10
        { "remove",			required_argument,	NULL,   'r' },	// 11
        { "comment",		required_argument,	NULL,   'n' },	// 12
        { "computerid",		required_argument,	NULL,   'c' },	// 13
        { "netuser",		required_argument,	NULL,   'u' },	// 15
        { "netpassword",	required_argument,	NULL,   'p' },	// 14
        { "localuser",		required_argument,	NULL,   'l' },	// 16
        { "localpassword",	required_argument,	NULL,   'q' },	// 17
        { "help",			no_argument,		NULL,   'h' },	// 18
        { "searchpolicy",	no_argument,		NULL,   'S' },	// 19
        { NULL,				0,					NULL,   0 }
    };
	
    if (argc < 2) {
        usage();
        return EX_USAGE;
    }
	
    while ((ch = getopt_long(argc, argv, "fvisxgema:r:n:c:u:p:l:q:hS", longopts, &longindex)) != -1) {
        switch (ch) {
            case 'f':
                force = YES;
                break;

            case 'v':
                verbose = YES;
                break;
                
            case 'i':
                prompt = YES;
                break;
                
            case 's':
                secureAuthOnly = YES;
                break;
                
            case 'x':
                useSSL = YES;
                break;
                
            case 'g':
                signPackets = YES;
                break;

            case 'e':
                encryptPackets = YES;
                break;
                
            case 'm':
                manInMiddle = YES;
                break;
                
            case 'a':
                addServer = YES;
				serverName = [NSString stringWithUTF8String: optarg];
                break;
				
            case 'r':
                removeServer = YES;
				serverName = [NSString stringWithUTF8String: optarg];
                break;
				
            case 'n':
				configName = [NSString stringWithUTF8String: optarg];
                break;
				
            case 'c':
				computerID = [NSString stringWithUTF8String: optarg];
                break;
				
            case 'u':
				userName = [NSString stringWithUTF8String: optarg];
                break;
				
            case 'l':
				localName = [NSString stringWithUTF8String: optarg];
                break;
				
            case 'p':
				userPassword = [NSString stringWithUTF8String: optarg];
                memset(optarg, '*', strlen(optarg));   // blank out with *****
                break;
				
            case 'q':
				localPassword = [NSString stringWithUTF8String: optarg];
                memset(optarg, '*', strlen(optarg)); // blank out with *****
                break;
                
            case 'S':
                updateSearch = NO;
                break;
				
            case 'h':
                usage();
				return 0;
				
            default:
                usage();
				return EX_USAGE;
        }
    }
	
    if (appleVersion) {
        dsToolAppleVersionExit(argv[0]);
		return EX_OK;
    }
	
	if (localName == nil) {
		const char *envStr = getenv("USER");
		if (envStr != NULL) {
			defaultUser = YES;
			localName = [NSString stringWithUTF8String: envStr];
		}
	}
	
    if (verbose == YES) {
        fprintf(stdout, "dsconfigldap verbose mode\n");
    }
	
    if (addServer == YES) {
        if (computerID == NULL) {
            computerID = [ODCAction calculateSuggestedComputerID];
            
            if (verbose) {
                fprintf(stdout, "Using suggested computer ID <%s>\n", [computerID UTF8String]);
            }
        }
        
        if ([computerID rangeOfString: @" "].location != NSNotFound) {
            fprintf(stderr, "Illegal character ' ' in Computer ID, configuration cancelled.\n");
            return EX_USAGE;
        }
    }
	
    if (verbose) {
        fprintf(stdout,"Options selected by user:\n");
        if (force) {
            fprintf(stdout,"Force authenticated (un)binding option selected\n");
        }
		
        if (prompt) {
            fprintf(stdout,"Interactive password option selected\n");
        }
		
        if (secureAuthOnly) {
            fprintf(stdout,"Enforce Secure Authentication is enabled\n");
        }
		
        if (useSSL) {
            fprintf(stdout,"SSL was chosen\n");
        }
		
        if (addServer) {
            fprintf(stdout,"Add server option selected\n");
        }
		
        if (removeServer) {
            fprintf(stdout,"Remove server option selected\n");
        }
		
        if (serverName) {
            fprintf(stdout,"Server name provided as <%s>\n", [serverName UTF8String]);
        }
		
        if (configName) {
            fprintf(stdout,"LDAP Configuration name provided as <%s>\n", [configName UTF8String]);
        }
		
        if (computerID) {
            fprintf(stdout,"Computer ID provided as <%s>\n", [computerID UTF8String]);
        }
		
        if (userName) {
            fprintf(stdout,"Network username provided as <%s>\n", [userName UTF8String]);
        }
		
        if (userPassword && !prompt) {
            fprintf(stdout,"Network user password provided as <%s>\n", [userPassword UTF8String]);
        }
		
        if (localName && !defaultUser) {
            fprintf(stdout,"Local username provided as <%s>\n", [localName UTF8String]);
        } else if (localName) {
            fprintf(stdout,"Local username determined to be <%s>\n", [localName UTF8String]);
        } else {
            fprintf(stdout,"No Local username determined\n");
        }
		
        if (localPassword && !prompt) {
            fprintf(stdout,"Local user password provided as <%s>\n", [localPassword UTF8String]);
        }
		
        if (manInMiddle) {
            fprintf(stdout, "Enforce man-in-the-middle only policy if server supports it.\n");
        }
		
        if (updateSearch) {
            fprintf(stdout, "Adding new node to search policies\n");
        }
		
        if (encryptPackets) {
            fprintf(stdout, "Enforce packet encryption policy if server supports it.\n");
        }
		
        if (signPackets) {
            fprintf(stdout, "Enforce packet signing policy if server supports it.\n");
        }
		
        fprintf(stdout, "\n");
    }
	
    if (addServer && removeServer) {
        fprintf(stdout,"Can't add and remove at the same time.\n");
        usage();
		return EX_USAGE;
    } else {
        NSError *error = nil;
        
		//add or remove server correctly selected
        if (userName != nil && (userPassword == nil || prompt)) {
            char *tmp = getpass("Please enter network user password: ");
			userPassword = [NSString stringWithUTF8String: tmp];
        }
		
        // we were asked to prompt for password or we had a user provided but no password
        if (prompt || (defaultUser == NO && localName != nil && localPassword == nil)) {
            char *tmp = getpass("Please enter local user password: ");
			localPassword = [NSString stringWithUTF8String: tmp];
        }
		
		if (addServer) {
			ODCAddODServerAction *action = [ODCAddODServerAction action];
            if ([action preflightAuthRightsWithUsername:localName password:localPassword]) {
                BOOL promptForAdd = NO;
                
                action.serverName = serverName;
                action.clientComputerID = computerID;
                action.userName = userName;
                action.password = userPassword;
                action.addServerToSearchPaths = updateSearch;
                action.forceBind = (force != 0);
                
                if (useSSL) {
                    action.useSSL = YES;
                    action.autoSSL = NO;
                } else {
                    action.autoSSL = YES;
                }
                
                // see if the certs are there first
                ODCGetODServerInfoAction *getInfoAction = [[[ODCGetODServerInfoAction alloc] init] autorelease];
                getInfoAction.serverName = serverName;
                [getInfoAction runSynchronously];
                if (getInfoAction.error == nil) {
                    NSData *certData = [getInfoAction.results objectForKey:ODCServerSSLCertificates];
                    promptForAdd = ([certData isKindOfClass:[NSData class]] && [certData length] > 0);
                }
                
                if (promptForAdd) {
                    char answer[2];
                    
                    if (action.useSSL) {
                        printf("Certificates will be automatically added to your system keychain in order to talk to this server.\n"
                               "Would you like to continue (y/n)? ");
                    } else {
                        printf("Certificates are available for this server.\n"
                               "Would you like to add them to system keychain automatically (y/n)? ");
                    }
                    
                    while (1) {
                        fgets(answer, sizeof(answer), stdin);
                        if (tolower(answer[0]) == 'y') {
                            action.addCertificates = YES;
                            break;
                        } else if (tolower(answer[0]) == 'n') {
                            if (action.useSSL == YES) {
                                printf("\nOperation cancelled.\n");
                                return EX_PROTOCOL;
                            }
                            break;
                        } else {
                            printf("\nPlease answer (y or n): ");
                        }
                    }
                }
                
                action.manInMiddle = (manInMiddle != 0);
                action.secureAuthOnly = (secureAuthOnly != 0);
                action.encryptionOnly = (encryptPackets != 0);
                action.signPackets = (signPackets != 0);
                
                [action runSynchronously];
                error = action.error;
            } else {
                fprintf(stderr, "Unable to get rights to change configuration\n");
                return EX_NOPERM;
            }
		} else if (removeServer) {
			ODCRemoveODServerAction *action = [ODCRemoveODServerAction action];
            if ([action preflightAuthRightsWithUsername:localName password:localPassword]) {
                action.serverName = serverName;
                action.forceUnbind = force;
                action.userName = userName;
                action.password = userPassword;
                action.removeServerFromSearchPaths = updateSearch;
                
                [action runSynchronously];
                error = action.error;
            } else {
                fprintf(stderr, "Unable to get rights to change configuration\n");
                return EX_NOPERM;
            }
		}
        
        switch ([error code]) {
            case kODErrorRecordAlreadyExists:
                fprintf(stderr, "Computer with the name '%s' already exists\n", [computerID UTF8String]);
                return EX_CONFIG;
                
            case kODErrorNodeUnknownHost:
                fprintf(stderr, "Could not contact a server at that address.\n");
                return EX_NOHOST;
                
            case kODErrorRecordPermissionError:
                if (removeServer && !force) {
                    fprintf(stderr, "Permission denied, to force remove this server use -f\n");
                } else {
                    fprintf(stderr, "Permission error\n");
                }
                return EX_NOPERM;
                
            case kODErrorCredentialsInvalid:
                fprintf(stderr, "Invalid credentials supplied %s server\n", (addServer ? "for binding to the" : "to remove the bound"));
                return EX_NOUSER;
                
            case kODErrorCredentialsMethodNotSupported:
                fprintf(stderr, "Server does not meet security requirements.\n");
                return EX_IOERR;

            case kODErrorRecordParameterError:
                if (removeServer) {
                    fprintf( stderr, "Bound need credentials to unbind\n" );
                } else {
                    fprintf( stderr, "Directory is not allowing anonymous queries\n" );
                }
                return EX_IOERR;
                
            case 0:
                break;
                
            default:
                fprintf(stderr, "Error: %s (%ld)\n", 
                        [[error localizedFailureReason] UTF8String] ?: "Description unavailable",
			(long)[error code]);
                return EX_IOERR;
        }
    }
	
    return EX_OK;
}
