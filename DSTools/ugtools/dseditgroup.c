/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
* @header dseditgroup
 * Tool used to manipulate group records via the Open Directory API.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include <uuid/uuid.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sysexits.h>
#include <membershipPriv.h>
#include <grp.h>

#include <CoreFoundation/CoreFoundation.h>
#include <OpenDirectory/OpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>

#include "dstools_version.h"

static void printArray( const void *value, void *context )
{
	CFStringRef cfPrintString = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("\t\t%@"), value );
	CFShow( cfPrintString );
	CFRelease( cfPrintString );
}

static void printDictionary( const void *key, const void *value, void *context )
{
	CFStringRef cfPrintString = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%@ -"), key );
	CFShow( cfPrintString );
	CFRelease( cfPrintString );
	
	CFArrayApplyFunction( value, CFRangeMake(0, CFArrayGetCount(value)), printArray, NULL );
}

static SInt32 printErrorOrMessage( CFErrorRef *inError, const char *errorString, bool inVerbose )
{
	SInt32 errorCode = EX_USAGE;
	
	if ( inError == NULL || (*inError) == NULL || (inVerbose == false && errorString != NULL) )
	{
		CFStringRef cfString = CFStringCreateWithCString( kCFAllocatorDefault, errorString, kCFStringEncodingUTF8 );
		if ( cfString != NULL ) {
			CFShow( cfString );
			CFRelease( cfString );
		}
	}
	else if ( inError != NULL && (*inError) != NULL )
	{
		if ( inVerbose == true )
		{
			CFStringRef cfString = CFErrorCopyDescription( *inError );
			if ( cfString != NULL ) {
				CFShow( cfString );
				CFRelease( cfString );
			}
		}
		
		CFIndex errorCode = CFErrorGetCode( *inError );
		
		// this is temporary until ODFramework has some kind of ranges for error types
		if ( errorCode >= kODErrorCredentialsInvalid && errorCode < kODErrorCredentialsInvalid+999 ) {
			errorCode = EX_NOPERM;
		}
		
		// we null the pointer
		*inError = NULL;
	}
	
	return errorCode;
}

#pragma mark -
#pragma mark Text Input Routines

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
}//intcatch


//-----------------------------------------------------------------------------
//	read_passphrase
//
//	Returns: malloc'd C-str
//	Provides a secure prompt for inputing passwords
/*
 * Reads a passphrase from /dev/tty with echo turned off.  Returns the
 * passphrase (allocated with xmalloc), being very careful to ensure that
 * no other userland buffer is storing the password.
 */
//-----------------------------------------------------------------------------

static char *
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
}//read_passphrase

static bool isLegacyGroup( ODRecordRef inRecordRef, CFArrayRef* outShortNameMembers )
{
	CFIndex		shortNameMembersCount = 0;
	CFIndex		guidMembersCount = 0;
	
	CFArrayRef shortNameMembers = ODRecordCopyValues( inRecordRef, kODAttributeTypeGroupMembership, NULL );
	if ( shortNameMembers != NULL )
	{
		shortNameMembersCount = CFArrayGetCount( shortNameMembers );
		if ( outShortNameMembers != NULL )
		{
			*outShortNameMembers = shortNameMembers;
			CFRetain( shortNameMembers );
		}
		
		CFRelease( shortNameMembers );
	}
	
	CFArrayRef guidMembers = ODRecordCopyValues( inRecordRef, kODAttributeTypeGroupMembers, NULL );
	if ( guidMembers != NULL ) {
		guidMembersCount = CFArrayGetCount( guidMembers );
		CFRelease( guidMembers );
	}
	
	return ( guidMembersCount == 0 && shortNameMembersCount > 0 );
}

//-----------------------------------------------------------------------------
//	usage
//
//-----------------------------------------------------------------------------

void
usage(void)
{
	printf("\ndseditgroup (%s):: Manipulate group records with the Open Directory API.\n\n", TOOLS_VERSION);
	printf(
           "Usage: dseditgroup [-pqv] -o edit [-n nodename] [-u username] [-P password]\n"
           "                   [-r realname] [-c comment] [-s ttl] [-k keyword] [-i gid]\n"
           "                   [-g uuid] [-S sid] [-a addmember] [-d deletemember] \n"
           "                   [-t membertype] [-T grouptype] [-L] groupname\n"
           "Usage: dseditgroup [-pqv] -o create [-n nodename] [-u username] [-P password]\n"
           "                   [-r realname] [-c comment] [-s ttl] [-k keyword] [-i gid]\n"
           "                   [-g uuid] [-S sid] [-T grouptype] [-L] groupname\n"
           "Usage: dseditgroup [-pqv] -o delete [-u username] [-P password] [-T grouptype]\n"
		   "                   [-L] groupname\n"
           "Usage: dseditgroup [-qv] -o read [-T grouptype] groupname\n"
           "Usage: dseditgroup [-qv] -o checkmember [-m membername] groupname\n" );
	printf("\nSee dseditgroup(8) man page for details.\n");
	printf("\n");
}//usage


int main(int argc, char *argv[])
{
    int				ch;
	char		   *operation		= nil;
	bool			bReadOption		= false;
	bool			bCreateOption   = false;
	bool			bDeleteOption   = false;
	bool			bEditOption		= false;
	bool			bInteractivePwd = false;
	bool			bNoVerify		= false;
	bool			bVerbose		= false;
	bool			bCheckMemberOption	= false;
	char		   *nodename		= nil;
	char		   *username		= nil;
	bool			bDefaultUser	= false;
	bool			bCompList		= false;
	char		   *password		= nil;
	char		   *addrecordname   = nil;
	char		   *delrecordname   = nil;
	char		   *recordtype		= nil;
	char		   *gid				= nil;
	char		   *guid			= nil;
	char		   *smbSID			= nil;
	char		   *realname		= nil;
	char		   *keyword			= nil;
	char		   *comment			= nil;
	char		   *timeToLive		= nil;
	char		   *groupname		= nil;
	char		   *member			= nil;
	char		   *format			= nil;	//be either "l" for legacy or "n" for new group format
	const char	   *grouptype		= NULL;
	int				exitcode		= 0;
	uuid_t			uuid;
    
	ODNodeRef			aDSNodeRef		= NULL;
	ODNodeRef			aDSSearchRef	= NULL;
	bool				bContinueAdd	= false;
	char			   *groupRecordName	= nil;
	__block ODRecordRef	aGroupRecRef	= NULL;
	__block ODRecordRef	aGroupRecRef2	= NULL;
	CFErrorRef			aErrorRef		= NULL;
	char				*errorTok		= NULL;

	if (argc < 2)
	{
		usage();
		exit(0);
	}
	
	if ( strcmp(argv[1], "-appleversion") == 0 )
        dsToolAppleVersionExit( argv[0] );
	
    while ((ch = getopt(argc, argv, "LT:o:pqvn:m:u:P:a:d:t:i:g:r:k:c:s:S:f:?h")) != -1) {
        switch (ch) {
            case 'o':
                operation = strdup(optarg);
                if (operation != nil)
                {
                    if ( strcasecmp(operation, "read") == 0 )
                    {
                        bReadOption = true;
                    }
                    else if ( strcasecmp(operation, "create") == 0 )
                    {
                        bCreateOption = true;
                    }
                    else if ( strcasecmp(operation, "delete") == 0 )
                    {
                        bDeleteOption = true;
                    }
                    else if ( strcasecmp(operation, "edit") == 0 )
                    {
                        bEditOption = true;
                    }
                    else if ( strcasecmp(operation, "checkmember") == 0 )
                    {
                        bCheckMemberOption = true;
                    }
                }
				break;
            case 'p':
                bInteractivePwd = true;
                break;
            case 'q':
                bNoVerify = true;
                break;
            case 'v':
                bVerbose = true;
                break;
            case 'm':
                member = strdup(optarg);
                break;
            case 'n':
                nodename = strdup(optarg);
                break;
            case 'u':
                username = strdup(optarg);
                break;
            case 'P':
                password = strdup(optarg);
                break;
            case 'a':
                addrecordname = strdup(optarg);
                break;
            case 'd':
                delrecordname = strdup(optarg);
                break;
            case 't':
                recordtype = strdup(optarg);
                break;
			case 'T':
				grouptype = optarg;
				break;
			case 'L':
				bCompList = true;
				break;
            case 'i':
				strtol( optarg, &errorTok, 10 );
				if ( errorTok == NULL || errorTok[0] == '\0' ) {
					gid = strdup(optarg);
				}
				else {
					printf( "GID contains non-numeric characters\n" );
					return EX_USAGE;
				}
                break;
            case 'g':
				uuid_clear( uuid );
				
				// don't allow malformed UUIDs nor an empty one
				if ( uuid_parse(optarg, uuid) == 0 && uuid_is_null(uuid) == false ) {
					guid = strdup(optarg);
				}
				else {
					printf( "GUID provided is not a valid UUID\n" );
					return EX_USAGE;
				}
                break;
            case 'r':
                realname = strdup(optarg);
                break;
            case 'k':
                keyword = strdup(optarg);
                break;
            case 'c':
                comment = strdup(optarg);
                break;
            case 's':
                timeToLive = strdup(optarg);
                break;
            case 'S':
                smbSID = strdup(optarg);
                break;
            case 'f':
                format = strdup(optarg);
                break;
            case '?':
            case 'h':
            default:
			{
				usage();
				return EX_USAGE;
			}
        }
    }
	
	argc -= optind;
	argv += optind;
	
	if (argc == 0)
	{
		printErrorOrMessage( NULL, "No group name provided", bVerbose );
		return EX_USAGE;
	}
	
	groupname = strdup( argv[0] );
	
	if (!bCreateOption && !bDeleteOption && !bEditOption && !bCheckMemberOption)
	{
		bReadOption = true; //default option
	}
	
	if (username == nil)
	{
		struct passwd* pw = NULL;
		pw = getpwuid(getuid());
		if (pw != NULL && pw->pw_name != NULL && pw->pw_name[0] != '\0')
		{
			username = strdup(pw->pw_name);
		}
		else
		{
			printf("***Username <-u username> must be explicitly provided in this shell***\n");
			usage();
			exit(0);
		}
		bDefaultUser = true;
	}
    
	if (bVerbose)
	{
		printf("dseditgroup verbose mode\n");
		printf("Options selected by user:\n");
		if (bReadOption)
			printf("Read option selected\n");
		if (bCreateOption)
			printf("Create option selected\n");
		if (bDeleteOption)
			printf("Delete option selected\n");
		if (bEditOption)
			printf("Edit option selected\n");
		if (bCheckMemberOption)
			printf("Checking membership selected\n");
		if (bInteractivePwd)
			printf("Interactive password option selected\n");
		if (bNoVerify)
			printf("User verification is disabled\n");
		if (nodename)
			printf("Nodename provided as <%s>\n", nodename);
		if (username && !bDefaultUser)
			printf("Username provided as <%s>\n", username);
		else
			printf("Username determined to be <%s>\n", username);
		if ( password && !bInteractivePwd )
			printf("Password provided as <%s>\n", password);
		if (addrecordname)
			printf("Recordname to be added provided as <%s>\n", addrecordname);
		if (delrecordname)
			printf("Recordname to be deleted provided as <%s>\n", delrecordname);
		if (recordtype)
			printf("Recordtype provided as <%s>\n", recordtype);
		if (grouptype)
			printf("Grouptype provided as <%s>\n", grouptype);
		if (gid)
			printf("GID provided as <%s>\n", gid);
		if (guid)
			printf("GUID provided as <%s>\n", guid);
		if (smbSID)
			printf("SID provided as <%s>\n", smbSID);
		if (realname)
			printf("Realname provided as <%s>\n", realname);
		if (keyword)
			printf("Keyword provided as <%s>\n", keyword);
		if (comment)
			printf("Comment provided as <%s>\n", comment);
		if (timeToLive)
			printf("TimeToLive provided as <%s>\n", timeToLive);
		if (groupname)
			printf("Groupname provided as <%s>\n", groupname);
		if (bCompList)
			printf("Will maintain computer lists when applicable\n" );
		printf("\n");
	}
	
	ODRecordType (^mapRecTypeWithDefault)(const char *, ODRecordType) = ^(const char *inType, ODRecordType inDefault) {
		if ( inType != NULL )
		{
			if ( strcasecmp(inType, "user") == 0) {
				return kODRecordTypeUsers;
			}
			else if ( strcasecmp(inType, "group") == 0) {
				return kODRecordTypeGroups;
			}
			else if ( strcasecmp(inType, "computer") == 0) {
				return kODRecordTypeComputers;
			}
			else if ( strcasecmp(inType, "computergroup") == 0 ) {
				return kODRecordTypeComputerGroups;
			}
		}
		
		return inDefault;
	};
    
	if (bCheckMemberOption == false &&
		bReadOption == false &&
		(!bNoVerify && ( !bDefaultUser && ( (password == nil) || bInteractivePwd ) ) || (bDefaultUser && bInteractivePwd)) )
	{
		password = read_passphrase("Please enter user password:", 1);
		//do not verbose output this password value
	}
	
	do //use break to stop for an error
	{
		bool bIsLocalNode = false;
		ODNodeRef aLocalNodeRef = ODNodeCreateWithNodeType( kCFAllocatorDefault, kODSessionDefault, kODNodeTypeLocalNodes, &aErrorRef );
		
		//set up the node to be used
        if (nodename == NULL)
		{
			// if no nodename is specified we default to the /Search node since OD Framework can work with that
			if ( bEditOption == true || bReadOption == true || bCheckMemberOption == true || bDeleteOption == true ) {
				aDSNodeRef = ODNodeCreateWithNodeType( kCFAllocatorDefault, kODSessionDefault, kODNodeTypeAuthentication, &aErrorRef );
			}
			else {
				/* Default to local node for other operations (i.e. create). */
				aDSNodeRef = aLocalNodeRef;
				bIsLocalNode = true;
			}
        }
		else if ( nodename[0] == '.' )
		{
            // if the nodename is "." we default to the local node by passing nil as the node name to getNodeRef
			aDSNodeRef = aLocalNodeRef;
			bIsLocalNode = true;
        }
		else
		{
            // otherwise we pass the provided nodename to getNodeRef
			CFStringRef cfNodeName = CFStringCreateWithCString( kCFAllocatorDefault, nodename, kCFStringEncodingUTF8 );
			if ( cfNodeName != NULL ) {
				aDSNodeRef = ODNodeCreateWithName( kCFAllocatorDefault, kODSessionDefault, cfNodeName, &aErrorRef );
				CFRelease( cfNodeName );
				if (aDSNodeRef == NULL) {
					exitcode = printErrorOrMessage(NULL, "Error locating specified node.", bVerbose);
					break;
				}
				
				if ( CFEqual(ODNodeGetName(aDSNodeRef), ODNodeGetName(aLocalNodeRef)) == true ) {
					bIsLocalNode = true;
				}
			}
			else {
				exitcode = printErrorOrMessage( NULL, "Error parsing node name.", bVerbose );
				break;
			}
        }
		
		if ( aDSNodeRef == NULL ) {
			exitcode = printErrorOrMessage( &aErrorRef, "getNodeRef failed to obtain a node reference", bVerbose );
			break;
		}
		
		aDSSearchRef = ODNodeCreateWithNodeType( kCFAllocatorDefault, kODSessionDefault, kODNodeTypeAuthentication, &aErrorRef );
		
		CFStringRef groupNameCF = CFStringCreateWithCString( kCFAllocatorDefault, groupname, kCFStringEncodingUTF8 );
		if ( groupNameCF == NULL ) {
			exitcode = EX_SOFTWARE;
			printErrorOrMessage( NULL, "Unable to parse groupname", bVerbose );
			break;
		}
		
		CFArrayRef attribs = CFArrayCreate( kCFAllocatorDefault, (CFTypeRef *) &kODAttributeTypeStandardOnly, 1, &kCFTypeArrayCallBacks );
		if ( attribs == NULL ) {
			exitcode = EX_SOFTWARE;
			printErrorOrMessage( NULL, "Unable to allocate array", bVerbose );
			break;
		}

		ODRecordType grpType = mapRecTypeWithDefault( grouptype, kODRecordTypeGroups );
		
		bool (^isLocalNode)(ODRecordRef record) = ^(ODRecordRef record) {
			CFArrayRef values = ODRecordCopyValues( record, kODAttributeTypeMetaNodeLocation, NULL );
			if ( values != NULL ) {
				
				if ( CFArrayGetCount(values) > 0 && CFEqual(CFArrayGetValueAtIndex(values, 0), ODNodeGetName(aLocalNodeRef)) == true ) {
					return (bool) true;
				}
				
				CFRelease( values );
			}
			
			return (bool) false;
		};
		
		aGroupRecRef = ODNodeCopyRecord( aDSNodeRef, grpType, groupNameCF, attribs, NULL );
		if ( aGroupRecRef != NULL ) {
			bIsLocalNode = isLocalNode( aGroupRecRef );
		}
		
		/* The group must already exist unless -o create is specified. */
		if (aGroupRecRef == NULL && !bCreateOption) {
			exitcode = printErrorOrMessage(NULL, "Group not found.", bVerbose);
			break;
		}

		if ( bCompList == true && grpType == kODRecordTypeComputerGroups )
		{
			aGroupRecRef2 = ODNodeCopyRecord( aDSNodeRef, kODRecordTypeComputerLists, groupNameCF, NULL, NULL );
			if ( aGroupRecRef2 != NULL && isLocalNode(aGroupRecRef2) == true )
			{
				// if we got a group record, let's see if it is also local node
				if ( bIsLocalNode == false && aGroupRecRef != NULL )
				{
					if ( bVerbose == true ) {
						printf( "Skipping Computer list because it's on a different node\n" );
						CFRelease( aGroupRecRef2 );
						aGroupRecRef2 = NULL;
					}
				}
				else {
					bIsLocalNode = true;
				}
			}
		}
		
		if ( geteuid() == 0 && bIsLocalNode == true && (username == NULL || password == NULL) )
		{
			// we are running as root and no password or name provided
			if ( bVerbose == true ) {
				printf( "Skipping authentication because user has effective ID 0\n" );
			}
		}
		else if ( bDeleteOption == true || bCreateOption == true || bEditOption == true )
		{
			// need to auth for changes
			if (username == NULL || password == NULL) {
				exitcode = printErrorOrMessage(NULL, "Username and password must be provided.", bVerbose);
				break;
			}

			bool bSuccess = false;
			CFStringRef user = CFStringCreateWithCString(NULL, username, kCFStringEncodingUTF8);
			CFStringRef pass = CFStringCreateWithCString(NULL, password, kCFStringEncodingUTF8);
			
			/*
			 * aDSNodeRef may be /Search unless we're creating a new group. Fortunately,
			 * we can authenticate with the specific group(s) most of the time. We still
			 * authenticate with the node directly when the specified group doesn't exist.
			 * As noted above, this is only allowed when creating a new group, in which
			 * case aDSNodeRef cannot be /Search.
			 */
			if (aGroupRecRef != NULL) {
				bSuccess = ODRecordSetNodeCredentials(aGroupRecRef, user, pass, &aErrorRef);
				if (aGroupRecRef2 != NULL) {
					ODRecordSetNodeCredentials(aGroupRecRef2, user, pass, &aErrorRef);
				}
			} else {
				bSuccess = ODNodeSetCredentials(aDSNodeRef, NULL, user, pass, &aErrorRef);
			}

			CFRelease(user);
			CFRelease(pass);

			if (!bSuccess) {
				exitcode = printErrorOrMessage(&aErrorRef, "Failed to set credentials.", bVerbose);
				break;
			}
		}
		
		CFErrorRef (^deleteRecords)(void) = ^(void) {
			CFErrorRef error = NULL;
			if ( aGroupRecRef != NULL )
			{
				if ( ODRecordDelete(aGroupRecRef, &error) == false ) {
					return error;
				}
				
				CFRelease( aGroupRecRef );
				aGroupRecRef = NULL;
			}
			
			if ( aGroupRecRef2 != NULL )
			{
				if ( ODRecordDelete(aGroupRecRef2, &error) == false ) {
					return error;
				}
				
				CFRelease( aGroupRecRef2 );
				aGroupRecRef2 = NULL;
			}
			
			return error;
		};
		
		if ( bReadOption == true || bDeleteOption == true )
		{
			if ( aGroupRecRef != NULL || aGroupRecRef2 != NULL )
			{
				if ( bDeleteOption == true && aGroupRecRef != NULL ) {
					printErrorOrMessage( NULL, "Group record below will be deleted:", bVerbose );
				}
				
				CFDictionaryRef cfDetails = ODRecordCopyDetails( aGroupRecRef, NULL, NULL );
				if ( cfDetails != NULL ) {
					CFDictionaryApplyFunction( cfDetails, printDictionary, NULL );
					CFRelease( cfDetails );
				}
				
				if ( bDeleteOption == true && (aErrorRef = deleteRecords()) != NULL ) {
					exitcode = printErrorOrMessage( &aErrorRef, "Unable to delete record", bVerbose );
					break;
				}
			}
			else
			{
				exitcode = printErrorOrMessage( NULL, "Group was not found.", bVerbose );
				break;
			}
		}
		else if (bCreateOption)
		{
			if ( aGroupRecRef != NULL || aGroupRecRef2 != NULL )
			{
				char responseValue[8] = {0};
				if (!bNoVerify)
				{
					printf("Create called on existing record - do you want to overwrite, y or n : ");
					scanf( "%c", responseValue );
					printf("\n");
				}
				
				if (bNoVerify || (responseValue[0] == 'y') || (responseValue[0] == 'Y'))
				{
					if ( (aErrorRef = deleteRecords()) != NULL ) {
						exitcode = printErrorOrMessage( &aErrorRef, "Unable to replace the record", bVerbose );
						break;
					}
				}
				else
				{
					exitcode = EX_CANTCREAT;
					printErrorOrMessage( NULL, "Operation cancelled because record could not be replaced", bVerbose );
					break;
				}
			}
			
			if ( aGroupRecRef == NULL )
			{
				aGroupRecRef = ODNodeCreateRecord( aDSNodeRef, grpType, groupNameCF, NULL, &aErrorRef );
				if ( aGroupRecRef != NULL )
				{
					groupRecordName = strdup(groupname);
					bContinueAdd = true;
					
					// if creating ComputerGroups allow creation of ComputerLists if -L specified
					if ( bCompList == true && grpType == kODRecordTypeComputerGroups ) {
						aGroupRecRef2 = ODNodeCreateRecord( aDSNodeRef, kODRecordTypeComputerLists, groupNameCF, NULL, NULL );
					}
				}
				else
				{
					exitcode = printErrorOrMessage( &aErrorRef, "Unable to create the record", bVerbose );
					break;
				}
			}
		}
		else if (bEditOption)
		{
			if ( aGroupRecRef != NULL ) {
				bContinueAdd = true;
			}
			else {
				printErrorOrMessage( NULL, "Record not found", bVerbose );
				break;
			}
		}
		else if (bCheckMemberOption)
		{
			if ( aGroupRecRef != NULL )
			{
				const char *user = (member ? : username);
				CFStringRef memberCF = CFStringCreateWithCString( kCFAllocatorDefault, user, kCFStringEncodingUTF8 );
				if ( memberCF == NULL ) {
					exitcode = printErrorOrMessage( &aErrorRef, "Unable to to allocate string", bVerbose );
					break;
				}
				
				ODRecordRef memberRec = ODNodeCopyRecord( aDSSearchRef, kODRecordTypeUsers, memberCF, NULL, &aErrorRef );
				if ( memberRec == NULL ) {
					exitcode = printErrorOrMessage( &aErrorRef, "Unable to find the user record", bVerbose );
					break;
				}
				
				if ( ODRecordContainsMember(aGroupRecRef, memberRec, &aErrorRef) == true ) {
					// return default exitcode of 0 if they are a member
					printf("yes %s is a member of %s\n", user, groupname);
					exitcode = EX_OK;
				}
				else {
					printf("no %s is NOT a member of %s\n", user, groupname);
					exitcode = EX_NOUSER;
				}
				
				CFRelease( memberRec );
				CFRelease( memberCF );
			}
			else
			{
				exitcode = printErrorOrMessage( NULL, "Invalid group name", bVerbose );
				break;
			}
		}
        
        if ( (bCreateOption || bEditOption) && aGroupRecRef != NULL )
        {
            if (bContinueAdd)
            {
                ODRecordType recType = mapRecTypeWithDefault( recordtype, kODRecordTypeUsers );
				
                if (format != nil)
                {
					CFArrayRef shortNameMembers = NULL;
					bool isLegacy = isLegacyGroup( aGroupRecRef, &shortNameMembers );
					
					if ( format[0] == 'n' && isLegacy == true )
					{
						// now we ask membership APIs for GUIDs for these users
						// we search each value individually so we grab the first entry
						CFMutableArrayRef newList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
						CFIndex count = CFArrayGetCount( shortNameMembers );
						CFIndex ii;
						
						for ( ii = 0; ii < count; ii++ )
						{
							char tmpStr[512]; // primary record names should ever be this long
							CFStringRef recName = (CFStringRef) CFArrayGetValueAtIndex( shortNameMembers, ii );
							
							const char *recNameStr = CFStringGetCStringPtr( recName, kCFStringEncodingUTF8 );
							if ( recNameStr == NULL ) {
								if ( CFStringGetCString(recName, tmpStr, sizeof(tmpStr), kCFStringEncodingUTF8) == false ) {
									printErrorOrMessage( NULL, "Record name is more than 512 characers - something is wrong", bVerbose );
									return EX_SOFTWARE;
								}
								
								recNameStr = tmpStr;
							}
							
							uuid_t uu;
							if ( mbr_user_name_to_uuid(recNameStr, uu) == 0 ) {
								uuid_string_t uuidStr;
								
								uuid_unparse_upper( uu, uuidStr );
								CFStringRef uuidCF = CFStringCreateWithCString( kCFAllocatorDefault, uuidStr, kCFStringEncodingUTF8 );
								CFArrayAppendValue( newList, uuidCF );
								CFRelease( uuidCF );
							}
							else {
								int len = strlen( recNameStr );
								char tempBuffer[sizeof("UUID for '%s' could not be found please remove bad entry before conversion") + len + 1];
								
								snprintf( tempBuffer, len, "UUID for '%s' could not be found please remove bad entry before conversion", recNameStr );
								printErrorOrMessage( NULL, tempBuffer, bVerbose );
								
								return EX_TEMPFAIL;
							}
						}
						
						if ( CFArrayGetCount(newList) != 0 )
						{
							if ( ODRecordSetValue(aGroupRecRef, kODAttributeTypeGroupMembers, newList, &aErrorRef) == false ) {
								printErrorOrMessage( &aErrorRef, "Failed to set the UUID based membership", bVerbose );
								break;
							}
						}
					}
					else if ( format[0] == 'l' && isLegacy == false )
					{
						CFArrayRef newValue = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
						if ( ODRecordSetValue(aGroupRecRef, kODAttributeTypeGroupMembers, newValue, &aErrorRef) == false ) {
							printErrorOrMessage( &aErrorRef, "Error removing GUIDs from the group", bVerbose );
							break;
						}
					}
					else
					{
						// just break, nothing to do, don't return an error
						if ( bVerbose == true ) {
							printErrorOrMessage( NULL, "Group is already in that format", bVerbose );
						}
						break;
					}
                }
				
                if ( addrecordname != NULL )
                {
					CFStringRef memberName = CFStringCreateWithCString( kCFAllocatorDefault, addrecordname, kCFStringEncodingUTF8 );
					if ( memberName != NULL ) {
						
						ODRecordRef memberRef = ODNodeCopyRecord( aDSSearchRef, recType, memberName, NULL, &aErrorRef );
						if ( memberRef != NULL ) {

							if ( ODRecordAddMember(aGroupRecRef, memberRef, &aErrorRef) == false ) {
								exitcode = printErrorOrMessage( &aErrorRef, "Could not add member to group.", bVerbose );
								break;
							}
							
							// silently update the second one
							ODRecordAddMember( aGroupRecRef2, memberRef, NULL );
						}
						else {
							printErrorOrMessage( &aErrorRef, "Record was not found.", bVerbose );
							exitcode = eDSRecordNotFound;
							break;
						}
					}
					else {
						exitcode = eDSInvalidRecordName;
						break;
					}
                }
				
				if ( delrecordname != NULL )
				{
					CFStringRef memberName = CFStringCreateWithCString( kCFAllocatorDefault, delrecordname, kCFStringEncodingUTF8 );
					if ( memberName != NULL ) {
						
						ODRecordRef memberRef = ODNodeCopyRecord( aDSSearchRef, recType, memberName, NULL, &aErrorRef );
						if ( memberRef != NULL ) {
							
							if ( ODRecordRemoveMember(aGroupRecRef, memberRef, &aErrorRef) == false ) {
								exitcode = printErrorOrMessage( &aErrorRef, "Could not remove member from group.", bVerbose );
								break;
							}
							
							// silently update the computer list if necessary
							ODRecordRemoveMember( aGroupRecRef2, memberRef, NULL );
						}
						else {
							printErrorOrMessage( &aErrorRef, "Record was not found.", bVerbose );
							exitcode = eDSRecordNotFound;
							break;
						}
					}
					else {
						exitcode = eDSInvalidRecordName;
						break;
					}
                }
				
                if ( gid != NULL )
                {
					CFStringRef gidCF = CFStringCreateWithCString( kCFAllocatorDefault, gid, kCFStringEncodingUTF8 );
					if ( gidCF == NULL ) {
						exitcode = printErrorOrMessage( NULL, "GID is invalid", bVerbose );
						break;
					}
					
					// see if it is in use already
					ODQueryRef query = ODQueryCreateWithNodeType( kCFAllocatorDefault, kODNodeTypeAuthentication, kODRecordTypeGroups, 
																  kODAttributeTypePrimaryGroupID, kODMatchEqualTo, gidCF, NULL, 1, &aErrorRef );
					if ( query == NULL ) {
						exitcode = printErrorOrMessage( &aErrorRef, "Error searching for conflicting GID", bVerbose );
						break;
					}
					
					CFArrayRef matches = ODQueryCopyResults( query, false, &aErrorRef );
					if ( (matches == NULL || CFArrayGetCount(matches) == 0) && aErrorRef == NULL ) {
						if ( ODRecordSetValue(aGroupRecRef, kODAttributeTypePrimaryGroupID, gidCF, &aErrorRef) == false ) {
							exitcode = printErrorOrMessage( &aErrorRef, "Error attempting to set GID attribute", bVerbose );
							break;
						}
						else {
							if ( bVerbose == true ) {
								printErrorOrMessage( &aErrorRef, "GID attribute was set", bVerbose );
							}
							exitcode = EX_OK;
						}
					}
					else if ( aErrorRef != NULL ) {
						exitcode = printErrorOrMessage( &aErrorRef, "Error checking if GID exists", bVerbose );
						break;
					}
					else {
						exitcode = printErrorOrMessage( NULL, "GID already exists", bVerbose );
						break;
					}
					
					CFRelease( gidCF );
                }
                else if ( bCreateOption ) //gid default creation only for create group
                {
					gid_t newGID;
					
					for ( newGID = 501; newGID < GID_MAX; newGID++ ) {
						if ( getgrgid(newGID) == NULL ) {
							gid = (char *) malloc( 32 );
							snprintf( gid, 32, "%d", newGID );
							break;
						}
					}
					
					if ( gid != NULL )
					{
						bool bGIDIsOk = false;
						CFStringRef gidCF = CFStringCreateWithCString( kCFAllocatorDefault, gid, kCFStringEncodingUTF8 );
						if ( gidCF == NULL ) {
							exitcode = printErrorOrMessage( NULL, "Unable to allocate string for PrimaryGroupID", bVerbose );
							break;
						}
						
						ODQueryRef query = ODQueryCreateWithNodeType( kCFAllocatorDefault, kODNodeTypeAuthentication, kODRecordTypeGroups, 
																	  kODAttributeTypePrimaryGroupID, kODMatchEqualTo, gidCF, NULL, 1, &aErrorRef );
						if ( query == NULL ) {
							exitcode = printErrorOrMessage( &aErrorRef, "Unable to create query for PrimaryGroupID", bVerbose );
							break;
						}
						
						CFArrayRef results = ODQueryCopyResults( query, false, &aErrorRef );
						if ( results != NULL ) {
							CFRelease( results );
							bGIDIsOk = true;
						}
						
						CFRelease( query );

						if ( bGIDIsOk == true && ODRecordSetValue(aGroupRecRef, kODAttributeTypePrimaryGroupID, gidCF, &aErrorRef) == false ) {
							exitcode = printErrorOrMessage( &aErrorRef, "Unable to create query for PrimaryGroupID", bVerbose );
						}
						
						CFRelease( gidCF );

						if ( bGIDIsOk == false ) {
							exitcode = printErrorOrMessage( &aErrorRef, "Query for PrimaryGroupID failed", bVerbose );
							break;
						}
					}
                }
				
				if ( guid != NULL )
				{
					// first let's canonicalize the UUID
					uuid_string_t uuidStr;
					
					uuid_unparse_upper( uuid, uuidStr );
					
					CFStringRef cfUUID = CFStringCreateWithCString( kCFAllocatorDefault, uuidStr, kCFStringEncodingUTF8 );
					if ( cfUUID != NULL ) {
						if ( ODRecordSetValue(aGroupRecRef, kODAttributeTypeGUID, cfUUID, &aErrorRef) == false ) {
							printErrorOrMessage( &aErrorRef, "Add GUID value - FAILED", bVerbose );
							break;
						} else if ( bVerbose ) {
							printErrorOrMessage( NULL, "Add GUID value - SUCCESS", bVerbose );
						}
					}
				}
				
                if ( smbSID )
                {
					CFStringRef newValue = CFStringCreateWithCString( kCFAllocatorDefault, smbSID, kCFStringEncodingUTF8 );
					if ( newValue != NULL ) {
						if ( ODRecordSetValue(aGroupRecRef, kODAttributeTypeSMBSID, newValue, &aErrorRef) == false ) {
							printErrorOrMessage( &aErrorRef, "Add SID value - FAILED", bVerbose );
						} else if ( bVerbose ) {
							printErrorOrMessage( NULL, "Add SID value - SUCCESS", bVerbose );
						}
						
						CFRelease( newValue );
					}
                }
				
                if ( realname )
                {
					CFStringRef newValue = CFStringCreateWithCString( kCFAllocatorDefault, realname, kCFStringEncodingUTF8 );
					if ( newValue != NULL ) {
						if ( ODRecordSetValue(aGroupRecRef, kODAttributeTypeFullName, newValue, &aErrorRef) == false ) {
							printErrorOrMessage( &aErrorRef, "Add full name value - FAILED", bVerbose );
						} else if ( bVerbose ) {
							printErrorOrMessage( NULL, "Add full name value - SUCCESS", bVerbose );
						}

						ODRecordSetValue( aGroupRecRef2, kODAttributeTypeFullName, newValue, NULL );
						CFRelease( newValue );
					}
                }
				
                if ( keyword )
                {
					CFStringRef newValue = CFStringCreateWithCString( kCFAllocatorDefault, keyword, kCFStringEncodingUTF8 );
					if ( newValue != NULL ) {
						if ( ODRecordSetValue(aGroupRecRef, kODAttributeTypeKeywords, newValue, &aErrorRef) == false ) {
							printErrorOrMessage( &aErrorRef, "Add Keyword value - FAILED", bVerbose );
						} else if ( bVerbose ) {
							printErrorOrMessage( NULL, "Add Keyword value - SUCCESS", bVerbose );
						}
						
						ODRecordSetValue( aGroupRecRef2, kODAttributeTypeKeywords, newValue, NULL );
						CFRelease( newValue );
					}
                }
				
                if ( comment )
                {
					CFStringRef newValue = CFStringCreateWithCString( kCFAllocatorDefault, comment, kCFStringEncodingUTF8 );
					if ( newValue != NULL ) {
						if ( ODRecordSetValue(aGroupRecRef, kODAttributeTypeComment, newValue, &aErrorRef) == false ) {
							printErrorOrMessage( &aErrorRef, "Add Comment value - FAILED", bVerbose );
						} else if ( bVerbose ) {
							printErrorOrMessage( NULL, "Add Comment value - SUCCESS", bVerbose );
						}
						
						ODRecordSetValue( aGroupRecRef2, kODAttributeTypeComment, newValue, NULL );
						CFRelease( newValue );
					}
                }
				
                if ( timeToLive )
                {
					CFStringRef newValue = CFStringCreateWithCString( kCFAllocatorDefault, timeToLive, kCFStringEncodingUTF8 );
					if ( newValue != NULL ) {
						if ( ODRecordSetValue(aGroupRecRef, kODAttributeTypeTimeToLive, newValue, &aErrorRef) == false ) {
							printErrorOrMessage( &aErrorRef, "Add TTL value - FAILED", bVerbose );
						} else if ( bVerbose ) {
							printErrorOrMessage( NULL, "Add TTL value - SUCCESS", bVerbose );
						}
						
						ODRecordSetValue( aGroupRecRef2, kODAttributeTypeTimeToLive, newValue, NULL );
						CFRelease( newValue );
					}
                }
            }
            
            if ( bVerbose == true && (bCreateOption == true || bEditOption == true) )
            {
                if ( bCreateOption == true )
					printErrorOrMessage( NULL, "\nGroup record created.\n", bVerbose );
                if ( bEditOption == true )
					printErrorOrMessage( NULL, "\nGroup record edited.\n", bVerbose );
				printErrorOrMessage( NULL, "=======================\n", bVerbose );
				
				ODRecordSynchronize( aGroupRecRef, NULL );
				ODRecordSynchronize( aGroupRecRef2, NULL );
				
				CFDictionaryRef cfDetails = ODRecordCopyDetails( aGroupRecRef, NULL, NULL );
				if ( cfDetails != NULL ) {
					CFDictionaryApplyFunction( cfDetails, printDictionary, NULL );
					CFRelease( cfDetails );
				}
            }
        }
        
        //always leave the while
        break;
	} while(true);

    //cleanup DS API references and variables
    if ( aDSNodeRef != NULL )
    {
        CFRelease( aDSNodeRef );
        aDSNodeRef = NULL;
    }
    
    //not really needed since we exit
    if (nodename)
        free(nodename);
    if (username)
        free(username);
    if (password)
        free(password);
    if (addrecordname)
        free(addrecordname);
    if (delrecordname)
        free(delrecordname);
    if (recordtype)
        free(recordtype);
    if (gid)
        free(gid);
    if (guid)
        free(guid);
    if (realname)
        free(realname);
    if (keyword)
        free(keyword);
    if (comment)
        free(comment);
    if (timeToLive)
        free(timeToLive);
    if (groupname)
        free(groupname);
    if (groupRecordName)
        free(groupRecordName);

	return exitcode;
}

