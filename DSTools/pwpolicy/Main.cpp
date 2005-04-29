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
 * @header pwpolicy
 * Tool for setting passwords and password policies with the DirectoryService API.
 */


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sysexits.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <pwd.h>
#include <dirent.h>
#include <syslog.h>

#include "PwdPolicyTool.h"
#include "dstools_version.h"

#define debug(A, args...)					\
	if (gVerbose)							\
		fprintf(stderr, (A), ##args);		

#define kNodeNotFoundMsg					"Cannot access directory node.\n"
#define kNotPasswordServerUserMsg			"%s is not a password server account.\n"
#define kUserNotOnNodeMsg					"%s is not a user on this directory node.\n"
#define kPasswordServerNodePrefix			"/PasswordServer/"
#define kDatePolicy1						"expirationDateGMT="
#define kDatePolicy2						"hardExpireDateGMT="
#define kNetInfoNodePrefix					"/NetInfo"

enum {
	kCmdNone,
	kCmdGetGlobalPolicy,
	kCmdSetGlobalPolicy,
	kCmdGetPolicy,
	kCmdSetPolicy,
	kCmdSetPolicyGlobal,
	kCmdSetPassword,
	kCmdEnableUser,
	kCmdGetGlobalHashTypes,
	kCmdSetGlobalHashTypes,
	kCmdGetHashTypes,
	kCmdSetHashTypes
};

typedef enum AuthAuthType {
	kAuthTypeUnknown,
	kAuthTypePasswordServer,
	kAuthTypeShadowHash,
	kAuthTypeKerberos,
	kAuthTypeDisabled
};

void PrintErrorMessage( long error, const char *username );

char *ConvertPolicyLongs(const char *inPolicyStr);
char *ConvertPolicyDates(const char *inPolicyStr);
Boolean PreflightDate(const char *theDateStr);

long GetAuthAuthority(
	const char *inNodeName,
	const char *inUsername,
	AuthAuthType *outAuthAuthType,
    char *inOutUserID,
	char *inOutServerAddress,
	char **outMetaNode,
	char **outAAData );
	
long GetAuthAuthorityWithSearchNode(
	const char *inUsername,
	AuthAuthType *outAuthAuthType,
    char *inOutUserID,
	char *inOutServerAddress,
	char **outMetaNode,
	char **outAAData );

long GetAuthAuthorityWithNode(
	const char *inNodeName,
	const char *inUsername,
	AuthAuthType *outAuthAuthType,
    char *inOutUserID,
	char *inOutServerAddress,
	char **outAAData );

AuthAuthType ConvertTagToConstant( const char *inAuthAuthorityTag );

long ParseAuthAuthority ( const char *inAuthAuthority,
                            char ** outVersion,
                            char ** outAuthTag,
                            char ** outAuthData );

void GetPWServerAddresses(char *outAddressStr);

// Globals
bool	gVerbose			= false;
bool	sTerminateServer	= false;

void	DoHelp		( FILE *inFile, const char *inArgv0 );
void	usage(void);
char *read_passphrase(const char *prompt, int from_stdin);

const unsigned long kMaxTestUsers		= 150;
const unsigned long kStressTestUsers	= 2000;
const unsigned long kAllTestUsers		= 123;

PwdPolicyTool myClass;

//--------------------------------------------------------------------------------------------------
// * main()
//--------------------------------------------------------------------------------------------------

int main ( int argc, char * const *argv )
{
	char			   *p						= NULL;
	long				siStatus				= eDSNoErr;
	char			   *authenticator			= NULL;
	char			   *username				= NULL;
	char			   *nodename				= NULL;
    char				password[512]			= {0};
	Boolean				bReadPassword			= false;
	tDirNodeReference   nodeRef					= 0;
    short				commandNum				= kCmdNone;
	short				tmpCommandNum			= kCmdNone;
	int					commandArgIndex			= 0;
	char				serverAddress[1024]		= {0};
	char				nodeName[1024]			= {0};
	char			   *tptr					= NULL;
	char				authenticatorID[1024]   = {0};
	char				userID[1024]			= {0};
	char			   *authResult				= nodeName;
	char			   *metaNode				= NULL;
	char			   *aaData					= NULL;
	AuthAuthType		authType				= kAuthTypeUnknown;
	AuthAuthType		userAuthType			= kAuthTypeUnknown;
	
	// read command line
	for ( int index = 1; index < argc; index++ )
	{
		if ( strcmp(argv[index], "-appleversion") == 0 )
		{
			// print appleversion and quit
			dsToolAppleVersionExit( argv[0] );
		}
		else
		if ( strcmp(argv[index], "-getglobalpolicy") == 0 )
		{
			tmpCommandNum = kCmdGetGlobalPolicy;
		}
		else
		if ( strcmp(argv[index], "-setglobalpolicy") == 0 )
		{
			tmpCommandNum = kCmdSetGlobalPolicy;
		}
		else
		if ( strcmp(argv[index], "-getpolicy") == 0 )
		{
			tmpCommandNum = kCmdGetPolicy;
		}
		else
		if ( strcmp(argv[index], "-setpolicy") == 0 )
		{
			tmpCommandNum = kCmdSetPolicy;
		}
		else
		if ( strcmp(argv[index], "-setpolicyglobal") == 0 )
		{
			tmpCommandNum = kCmdSetPolicyGlobal;
		}
		else
		if ( strcmp(argv[index], "-setpassword") == 0 )
		{
			tmpCommandNum = kCmdSetPassword;
		}
		else
		if ( strcmp(argv[index], "-enableuser") == 0 )
		{
			tmpCommandNum = kCmdEnableUser;
		}
		else
		if ( strcmp(argv[index], "-getglobalhashtypes") == 0 )
		{
			tmpCommandNum = kCmdGetGlobalHashTypes;
		}
		else
		if ( strcmp(argv[index], "-setglobalhashtypes") == 0 )
		{
			tmpCommandNum = kCmdSetGlobalHashTypes;
		}
		else
		if ( strcmp(argv[index], "-gethashtypes") == 0 )
		{
			tmpCommandNum = kCmdGetHashTypes;
		}
		else
		if ( strcmp(argv[index], "-sethashtypes") == 0 )
		{
			tmpCommandNum = kCmdSetHashTypes;
		}
		else
		if ( argv[index][0] == '-' )
		{
			p = argv[index];
			p++;
			
			while (*p)
			{
				if ( *p == 'a' || *p == 'u' || *p == 'p' || *p == 'n' )
					if ( index+1 >= argc )
					{
						usage();
						exit(0);
					}
				
				switch(*p)
				{
					case 'a':
						authenticator = argv[index+1];
						bReadPassword = true;
						break;
					
					case 'h':
						DoHelp( stderr, argv[0] );
						exit( 1 );
						break;
					
					case 'n':
						nodename = argv[index+1];
						break;
					
					case 'p':
						// copy the password and clear it from the arg list
						// so that it does not show up in a ps listing
						strcpy( password, argv[index+1] );
						memset( argv[index+1], 0, strlen(argv[index+1]) );
						break;
					
					case 'u':
						username = argv[index+1];
						break;
			
					case 'v':
						gVerbose = true;
						break;
					
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
						tmpCommandNum = *p - '0' + 1;
						break;
					
					default:
						usage();
						exit(0);
						break;
				}
				
				p++;
			}
		}
		
		// allow only one command on the line
		if ( tmpCommandNum != kCmdNone && commandNum != kCmdNone )
		{
			usage();
			exit(0);
		}
		else
		if ( tmpCommandNum != kCmdNone )
		{
			commandNum = tmpCommandNum;
			tmpCommandNum = kCmdNone;
			commandArgIndex = index;
		}
	}

	debug( "\npwpolicy tool, version 1.2.1\n\n" );
	
	// if data is required, check the last parameter for '-'
	if ( commandNum == kCmdSetGlobalPolicy ||
		 commandNum == kCmdSetPolicy ||
		 commandNum == kCmdSetPassword ||
		 commandNum == kCmdSetGlobalHashTypes ||
		 commandNum == kCmdSetHashTypes )
	{
		if ( argv[argc-1][0] == '-' )
		{
			usage();
			exit(0);
		}
	}
	
	// prompt for password if required and not provided on the command line
	if ( bReadPassword && password[0] == '\0' )
	{
		char *passPtr;
		
        passPtr = read_passphrase("Password:", 1);
		if ( passPtr != NULL )
		{
			strcpy( password, passPtr );
			memset( passPtr, 0, strlen(passPtr) );
			free( passPtr );
		}
	}
	
	if ( argc > 1 )
	{
		serverAddress[0] = '\0';
		
		siStatus = myClass.Initialize();
		if ( siStatus != eDSNoErr )
		{
			fprintf(stderr, "Could not initialize Open Directory.\n");
			exit(1);
		}
		
		// get target user info (if applicable)
		// get this one first so that the authenticator's server address overrides
		if ( username != NULL )
		{
			siStatus = GetAuthAuthority( nodename, username, &authType, userID, serverAddress, &metaNode, &aaData );
			if ( siStatus != eDSNoErr )
			{
				PrintErrorMessage( siStatus, username );
				exit(0);
			}
			userAuthType = authType;
		}
		
		// get authenticator info (if applicable)
		if ( authenticator != NULL )
		{
			siStatus = GetAuthAuthority( nodename, authenticator, &authType, authenticatorID, serverAddress, NULL, NULL );
			if ( siStatus != eDSNoErr )
			{
				PrintErrorMessage( siStatus, authenticator );
				exit(0);
			}
		}
		
		if ( authType == kAuthTypeUnknown &&
			 (commandNum == kCmdGetGlobalPolicy || commandNum == kCmdSetGlobalPolicy) )
		{
			if ( nodename != NULL && strncmp( nodename, kNetInfoNodePrefix, sizeof(kNetInfoNodePrefix)-1 ) == 0 )
			{
				username = "";
				authType = kAuthTypeShadowHash;
			}
			else
				authType = kAuthTypePasswordServer;
		}
		if ( commandNum == kCmdGetGlobalHashTypes || commandNum == kCmdSetGlobalHashTypes )
		{
			username = "";
			authType = kAuthTypeShadowHash;
		}
		
		if ( commandNum == kCmdGetHashTypes || commandNum == kCmdSetHashTypes )
		{
			if ( userAuthType != kAuthTypeShadowHash )
			{
				fprintf(stderr, "The hash types can be set only for ShadowHash accounts.\n");
				exit(0);
			}
		}
		
		switch( authType )
		{
			case kAuthTypeUnknown:
				PrintErrorMessage( siStatus, username );
				exit(0);
				break;
			
			case kAuthTypePasswordServer:
				// if no user info, get the default server for the local directory
				if ( serverAddress[0] == '\0' )
				{
					// get the default server for the local directory
					GetPWServerAddresses(serverAddress);
					
					tptr = strchr(serverAddress, ',');
					if ( tptr != NULL )
						*tptr = '\0';
				}
				
				if ( strlen(serverAddress) < 7 )
				{
					fprintf(stderr, "password server is not configured.\n");
					exit(0);
				}
				
				strcpy(nodeName, kPasswordServerNodePrefix);
				strcat(nodeName, serverAddress);
				break;
				
			case kAuthTypeShadowHash:
			case kAuthTypeDisabled:
				strcpy(nodeName, kNetInfoNodePrefix"/DefaultLocalNode");
				break;
			
			case kAuthTypeKerberos:
				if ( nodename != NULL )
				{
					strcpy(nodeName, nodename);
				}
				else
				{
					PrintErrorMessage( eDSNullNodeName, username );
					exit(0);
				}
				break;
		}
		
		// do it
		switch ( commandNum )
		{
			case kCmdGetGlobalPolicy:
				if ( myClass.OpenDirNode( nodeName, &nodeRef ) == eDSNoErr )
				{
					myClass.DoNodePWAuth( nodeRef, username ? username : "", "", kDSStdAuthGetGlobalPolicy, "", NULL, authResult );
					tptr = ConvertPolicyLongs( authResult );
					printf("%s\n", tptr);
					free(tptr);
					myClass.CloseDirectoryNode( nodeRef );
				}
				break;
				
			case kCmdSetGlobalPolicy:
				tptr = ConvertPolicyDates( argv[argc-1] );
				if ( tptr != NULL )
				{
					if ( myClass.OpenDirNode( nodeName, &nodeRef ) == eDSNoErr )
					{
						myClass.DoNodePWAuth( nodeRef, authenticatorID, password,
												kDSStdAuthSetGlobalPolicy,
												tptr, NULL, NULL );
						myClass.CloseDirectoryNode( nodeRef );
						
						free( tptr );
					}
				}
				else
				{
					usage();
				}
				break;
			
			case kCmdGetPolicy:
				if ( username != NULL )
				{
					if ( myClass.OpenDirNode( nodeName, &nodeRef ) == eDSNoErr )
					{
						myClass.DoNodePWAuth( nodeRef,
											userID ? userID : "",
											"",
											kDSStdAuthGetPolicy,
											userID, NULL, authResult );
						tptr = ConvertPolicyLongs( authResult );
						printf("%s\n", tptr);
						free(tptr);
						
						myClass.CloseDirectoryNode( nodeRef );
					}
				}
				else
				{
					usage();
				}
				break;
			
			case kCmdSetPolicy:
				tptr = ConvertPolicyDates( argv[argc-1] );
				if ( authenticator != NULL && password != NULL && username != NULL && tptr != NULL )
				{
					if ( myClass.OpenDirNode( nodeName, &nodeRef ) == eDSNoErr )
					{
						myClass.DoNodePWAuth( nodeRef,
												authenticatorID,
												password,
												kDSStdAuthSetPolicy,
												userID,
												tptr, NULL );
						
						free( tptr );
						
						myClass.CloseDirectoryNode( nodeRef );
					}
				}
				else
				{
					usage();
				}
				break;
				
			case kCmdSetPolicyGlobal:
				if ( authenticator != NULL && password != NULL && username != NULL )
				{
					if ( myClass.OpenDirNode( nodeName, &nodeRef ) == eDSNoErr )
					{
						myClass.DoNodePWAuth( nodeRef,
											authenticatorID,
											password,
											kDSStdAuthSetPolicy,
											userID,
											"newPasswordRequired=0 usingHistory=0 usingExpirationDate=0 "
											"usingHardExpirationDate=0 requiresAlpha=0 requiresNumeric=0 "
											"maxMinutesUntilChangePassword=0 maxMinutesUntilDisabled=0 "
											"maxMinutesOfNonUse=0 maxFailedLoginAttempts=0 "
											"minChars=0 maxChars=0 resetToGlobalDefaults=1", NULL );
						myClass.CloseDirectoryNode( nodeRef );
					}
				}
				else
				{
					usage();
				}
				break;
				
			case kCmdSetPassword:
				if ( authenticator != NULL && username != NULL )
				{
					if ( myClass.OpenDirNode( nodeName, &nodeRef ) == eDSNoErr )
					{
						myClass.DoNodePWAuth( nodeRef,
												userID,
												argv[argc-1],
												kDSStdAuthSetPasswd,
												authenticatorID,
												password, NULL );
						myClass.CloseDirectoryNode( nodeRef );
					}
				}
				else
				{
					usage();
				}
				break;
			
			case kCmdEnableUser:
				if ( authenticator != NULL && username != NULL )
				{
					switch( userAuthType )
					{
						case kAuthTypeShadowHash:
							printf( "User <%s> is not marked as disabled.\n", username );
							break;
						
						case kAuthTypeDisabled:
							if ( myClass.OpenDirNode( metaNode ? metaNode : nodeName, &nodeRef ) == eDSNoErr )
							{
								tRecordReference recRef;
								
								if ( myClass.DoNodeNativeAuth( nodeRef, authenticator, password ) == eDSNoErr )
								{
									if ( myClass.OpenRecord( nodeRef, kDSStdRecordTypeUsers, username, &recRef ) == eDSNoErr )
									{
										myClass.ChangeAuthAuthorityToShadowHash( recRef );
										dsCloseRecord( recRef );
									}
									else
									{
										printf( "Could not access account <%s>.\n", username );
									}
								}
								else
								{
									printf( "Could not get write permission on directory node: %s\n", metaNode ? metaNode : nodeName );
								}
								
								myClass.CloseDirectoryNode( nodeRef );
							}
							break;
						
						default:
							printf( "User <%s> does not have a shadowhash account.\n", username );
					}
				}
				else
				{
					usage();
				}
				break;
			
			case kCmdGetGlobalHashTypes:
				{	
					char *hashTypesStr;
					
					if ( myClass.GetHashTypes( &hashTypesStr ) == eDSNoErr )
					{
						printf( "%s\n", hashTypesStr );
						free( hashTypesStr );
					}
				}
				break;
			
			case kCmdSetGlobalHashTypes:
				if ( (geteuid() == 0) || (authenticator != NULL && password != NULL) )
					myClass.SetHashTypes( authenticator, password, commandArgIndex + 1, argc, argv );
				else
					usage();
				break;
				
			case kCmdGetHashTypes:
				if ( aaData != NULL && aaData[0] != '\0' && strcasecmp(aaData, kDSTagAuthAuthorityBetterHashOnly) != 0 )
				{
					char *hashTypeStr = NULL;
					char *hashListPtr = NULL;
					char *hashListStr = strdup( aaData );
					char *endPtr = NULL;
					
					hashListPtr = hashListStr;
					if ( strncasecmp( hashListPtr, kHashNameListPrefix, sizeof(kHashNameListPrefix)-1 ) == 0 )
					{
						hashListPtr += sizeof(kHashNameListPrefix) - 1;
						endPtr = strchr( hashListPtr, '>' );
						
						if ( endPtr == NULL || *hashListPtr++ != '<'  )
						{
							printf( "Invalid hash list. Use the -sethashtypes switch to reset the list.\n" );
						}
						else
						{
							*endPtr = '\0';
							while ( (hashTypeStr = strsep(&hashListPtr, ",")) != NULL )
								printf( "%s\n", hashTypeStr );
						}
					}
					
					free( hashListStr );
				}
				else
				{
					// return the global set
					char *hashTypesStr;
					
					if ( myClass.GetHashTypes( &hashTypesStr, (strcasecmp(aaData, kDSTagAuthAuthorityBetterHashOnly) == 0) ) == eDSNoErr )
					{
						printf( "%s\n", hashTypesStr );
						free( hashTypesStr );
					}
				}
				break;
			
			case kCmdSetHashTypes:
				if ( (geteuid() == 0) || (authenticator != NULL && password != NULL) )
				{
					if ( myClass.OpenDirNode( metaNode ? metaNode : nodeName, &nodeRef ) == eDSNoErr )
					{
						tRecordReference recRef;
						int result = 0;
						
						if ( authenticator != NULL )
							siStatus = myClass.DoNodeNativeAuth( nodeRef, authenticator, password );
						
						if ( siStatus == eDSNoErr )
						{
							if ( myClass.OpenRecord( nodeRef, kDSStdRecordTypeUsers, username, &recRef ) == eDSNoErr )
							{
								result = myClass.SetUserHashList( recRef, commandArgIndex + 1, argc, argv );
								dsCloseRecord( recRef );
							}
							else
							{
								printf( "Could not access account <%s>.\n", username );
							}
						}
						else
						{
							printf( "Could not get write permission on directory node: %s\n", metaNode ? metaNode : nodeName );
						}
						
						myClass.CloseDirectoryNode( nodeRef );
						
						if ( result == -1 )
							usage();
					}
				}
				else
					usage();
				break;
		}
		
		myClass.Deinitialize();
	}
	else
	{
		DoHelp( stderr, argv[0] );
		exit( 1 );
	}
} // main


//-----------------------------------------------------------------------------
//	PrintErrorMessage
//-----------------------------------------------------------------------------

void PrintErrorMessage( long error, const char *username )
{
	if ( username == NULL )
		username = "";
	
	switch( error )
	{
		case eDSRecordNotFound:
			fprintf(stderr, kUserNotOnNodeMsg, username);
			break;
		
		case eDSNodeNotFound:
			fprintf(stderr, kNodeNotFoundMsg);
			break;
		
		case eDSNullNodeName:
			fprintf(stderr, "Kerberos authentication authorities require a specific node name (-n option).\n");
			break;
			
		default:
			fprintf(stderr, kNotPasswordServerUserMsg, username);
	}
}

//-----------------------------------------------------------------------------
//	DoHelp
//
//	invoked when 'h' is on the command line
//-----------------------------------------------------------------------------

void DoHelp ( FILE *inFile, const char *inArgv0 )
{
	const char *tmpToolName;
	const char *toolName = inArgv0;
	
	do
	{
		tmpToolName = strchr( toolName, '/' );
		if ( tmpToolName != NULL )
			toolName = tmpToolName + 1;
	}
	while ( tmpToolName != NULL );
	
	static const char * const	_szpUsage =
		"Usage: %s [-h]\n"
		"Usage: %s [-v] [-a authenticator] [-p password] [-u username]\n"
        	"                [-n nodename] command command-arg\n"
		"Usage: %s [-v] [-a authenticator] [-p password] [-u username]\n"
        	"                [-n nodename] command \"policy1=value1 policy2=value2 ...\"\n"
		"\n"
		"  -a       name of the authenticator\n"
		"  -p       password (omit this option for a secure prompt)\n"
		"  -u       name of the user to modify\n"
		"  -h       help\n"
		"  -n       directory-node to search, uses search node by default\n"
		"  -v       verbose\n"
		"\n"
		"  -getglobalpolicy     Get global policies.\n"
		"                       Specify a user if the password server\n"
		"                       is not configured locally.\n"
		"  -setglobalpolicy     Set global policies\n"
		"  -getpolicy           Get policies for a user\n"
		"  -setpolicy           Set policies for a user\n"
		"  -setpolicyglobal     Set a user account to use global policies\n"
		"  -setpassword         Set a new password for a user\n"
		"  -enableuser          Enable a shadowhash user account that was disabled\n"
		"                       by a password policy event.\n"
		"  -getglobalhashtypes  Returns a list of password hashes stored on disk by\n"
		"                       default.\n"
		"  -setglobalhashtypes  Edits the list of password hashes stored on disk by\n"
		"                       default.\n"
		"  -gethashtypes        Returns a list of password hashes stored on disk for\n"
		"                       a user account.\n"
		"  -sethashtypes        Edits the list of password hashes stored on disk for\n"
		"                       a user account.\n"
		"\n"
		"";

	::fprintf( inFile, _szpUsage, toolName, toolName, toolName );
} // DoHelp


//-----------------------------------------------------------------------------
//	usage
//-----------------------------------------------------------------------------

void usage(void)
{
    fprintf(stdout, "usage: [-v] [-a authenticator] [-p password] [-u username] [-n nodename] command args\n");
    exit(EX_USAGE);
}


 
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


//-----------------------------------------------------------------------------
//	ConvertPolicyDates
//
//	Returns: malloc'd C-str, or NULL if a value is bad.
//	substitutes unsigned longs with human-readable dates
//-----------------------------------------------------------------------------

char *ConvertPolicyLongs(const char *inPolicyStr)
{
	char *returnString = NULL;
	char *value = NULL;
	char *tempString = NULL;
	struct tm *timerec;
	time_t timeval;
	char scratchStr[256];
	
	try
	{
		tempString = (char *)malloc( strlen(inPolicyStr) + 100 );
		value = strstr( inPolicyStr, kDatePolicy1 );
		if ( value != NULL )
		{
			// kDatePolicy1
			value += strlen( kDatePolicy1 );
			strncpy( tempString, inPolicyStr, value - inPolicyStr );
			tempString[value - inPolicyStr] = '\0';
			
			timeval = 0;
			sscanf( value, "%lu", &timeval );
			timerec = ::gmtime( &timeval );
			strftime(scratchStr, sizeof(scratchStr), "%m/%d/%y", timerec);
			strcat( tempString, scratchStr );
			value = strchr( value, ' ' );
			if ( value != NULL )
				strcat( tempString, value );
		}
		else
		{
			strcpy( tempString, inPolicyStr );
		}
	}
	catch(...)
	{
		if ( tempString != NULL )
		{
			free( tempString );
			tempString = NULL;
		}
	}
	
	// kDatePolicy2
	try
	{
		if ( tempString == NULL )
			throw(-1);
		
		returnString = (char *)malloc( strlen(tempString) + 100 );
		value = strstr( tempString, kDatePolicy2 );
		if ( value != NULL )
		{
			value += strlen( kDatePolicy2 );
			strncpy( returnString, tempString, value - tempString );
			returnString[value - tempString] = '\0';
			
			timeval = 0;
			sscanf( value, "%lu", &timeval );
			timerec = ::gmtime( &timeval );
			strftime(scratchStr, sizeof(scratchStr), "%m/%d/%y", timerec);
			strcat( returnString, scratchStr );
			value = strchr( value, ' ' );
			if ( value != NULL )
				strcat( returnString, value );
		}
		else
		{
			strcpy( returnString, tempString );
		}
	}
	catch(...)
	{
		if ( returnString != NULL )
		{
			free( returnString );
			returnString = NULL;
		}
	}
	
	if ( tempString != NULL )
	{
		free( tempString );
		tempString = NULL;
	}
	
	return returnString;
}


//-----------------------------------------------------------------------------
//	ConvertPolicyDates
//
//	Returns: malloc'd C-str, or NULL if a date is bad.
//	substitutes human-typeable dates into unsigned longs
//-----------------------------------------------------------------------------

char *ConvertPolicyDates(const char *inPolicyStr)
{
	char *returnString = NULL;
	char *value = NULL;
	char *firstNonNeeded = NULL;
	char *tempString = NULL;
	struct tm timerec;
	char scratchStr[256];
	
	try
	{
		tempString = (char *)malloc( strlen(inPolicyStr) + 100 );
		value = strstr( inPolicyStr, kDatePolicy1 );
		if ( value != NULL )
		{
			// kDatePolicy1
			value += strlen( kDatePolicy1 );
			strncpy( tempString, inPolicyStr, value - inPolicyStr );
			tempString[value - inPolicyStr] = '\0';
			
			strncpy( scratchStr, value, 8 );
			scratchStr[8] = '\0';
			for (int index = 0; index < 8; index++)
			{
				if (scratchStr[index] == ' ')
				{
					scratchStr[index] = '\0';
					break;
				}
			}
			
			if ( ! PreflightDate(scratchStr) )
				throw(-1);
			
			memset(&timerec, 0, sizeof(timerec));
			firstNonNeeded = strptime(value, "%m/%d/%y", &timerec);
			if ( firstNonNeeded == NULL )
				throw(-1);
			
			sprintf( scratchStr, "%lu", mktime(&timerec) );
			strcat( tempString, scratchStr );
			value = strchr( value, ' ' );
			if ( value != NULL )
				strcat( tempString, value );
		}
		else
		{
			strcpy( tempString, inPolicyStr );
		}
	}
	catch(...)
	{
		if ( tempString != NULL )
		{
			free( tempString );
			tempString = NULL;
		}
	}
	
	// kDatePolicy2
	try
	{
		if ( tempString == NULL )
			throw(-1);
		
		returnString = (char *)malloc( strlen(tempString) + 100 );
		value = strstr( tempString, kDatePolicy2 );
		if ( value != NULL )
		{
			value += strlen( kDatePolicy2 );
			strncpy( returnString, tempString, value - tempString );
			returnString[value - tempString] = '\0';
			
			strncpy( scratchStr, value, 8 );
			scratchStr[8] = '\0';
			for (int index = 0; index < 8; index++)
			{
				if (scratchStr[index] == ' ')
				{
					scratchStr[index] = '\0';
					break;
				}
			}
			
			if ( ! PreflightDate(scratchStr) )
				throw(-1);
			
			memset(&timerec, 0, sizeof(timerec));
			firstNonNeeded = strptime(value, "%m/%d/%y", &timerec);
			if ( firstNonNeeded == NULL )
				throw(-1);
			
			sprintf( scratchStr, "%lu", mktime(&timerec) );
			strcat( returnString, scratchStr );
			value = strchr( value, ' ' );
			if ( value != NULL )
				strcat( returnString, value );
		}
		else
		{
			strcpy( returnString, tempString );
		}
	}
	catch(...)
	{
		if ( returnString != NULL )
		{
			free( returnString );
			returnString = NULL;
		}
	}
	
	if ( tempString != NULL )
	{
		free( tempString );
		tempString = NULL;
	}
	
	return returnString;
}


//-----------------------------------------------------------------------------
//	PreflightDate
//
//	Returns: TRUE if the date is in a valid format
//	substitutes human-typeable dates into unsigned longs
//-----------------------------------------------------------------------------
Boolean PreflightDate(const char *theDateStr)
{
	const char *tptr;
	int index, argLen;
	int slashcount = 0;
	bool success = false;
	
	try
	{
		tptr = theDateStr;
		argLen = strlen( theDateStr );
		for ( index = 0; index < argLen; index++ )
		{
			if ( tptr[index] == '/' )
			{
				if ( index == 0 || index == argLen - 1 )
					throw(-1);
				
				slashcount++;
			}
			else
			if ( tptr[index] < '0' )
				throw(-1);
			else
			if ( tptr[index] > '9' )
				throw(-1);
		}
		
		if ( slashcount != 2 )
			throw(-1);
			
		success = true;
	}
	catch(...)
	{
	}
	
	return success;
}


//-----------------------------------------------------------------------------
//	GetAuthAuthority
//
//	Returns: DS status
//	A convenience method that selects the search-node or named-node method
//-----------------------------------------------------------------------------

long GetAuthAuthority(
	const char *inNodeName,
	const char *inUsername,
    AuthAuthType *outAuthAuthType,
	char *inOutUserID,
	char *inOutServerAddress,
	char **outMetaNode,
	char **outAAData )
{
	long result = -1;
	
	if ( inNodeName == NULL )
		result = GetAuthAuthorityWithSearchNode( inUsername, outAuthAuthType, inOutUserID, inOutServerAddress, outMetaNode, outAAData );
	else
		result = GetAuthAuthorityWithNode( inNodeName, inUsername, outAuthAuthType, inOutUserID, inOutServerAddress, outAAData );
	
	if ( result == eDSNoErr && *outAuthAuthType == kAuthTypePasswordServer && (*inOutUserID == '\0' || *inOutServerAddress == '\0') )
		result = -1;
	
	return result;
}


//-----------------------------------------------------------------------------
//	GetAuthAuthorityWithSearchNode
//
//	Returns: DS status
//-----------------------------------------------------------------------------

long GetAuthAuthorityWithSearchNode(
	const char *inUsername,
    AuthAuthType *outAuthAuthType,
	char *inOutUserID,
	char *inOutServerAddress,
	char **outMetaNode,
	char **outAAData )
{
    long status = eDSNoErr;
	char *aaVersion = nil;
    char *aaTag = nil;
    char *aaData = nil;
	char *authAuthorityStr = nil;
	char *metaNodeStr = nil;
	
    if ( inUsername == nil || inOutUserID == nil || inOutServerAddress == nil )
    {
        debug("GetAuthAuthority(): all parameters must be non-null\n");
        exit(-1);
    }
	
    *inOutUserID = '\0';
    *inOutServerAddress = '\0';
	
	try
	{
		status = myClass.GetUserByName( myClass.GetSearchNodeRef(), inUsername, &authAuthorityStr, &metaNodeStr );
		if ( status != eDSNoErr )
			throw( status );
		
		if ( outMetaNode != NULL )
			*outMetaNode = metaNodeStr;
		else
			free( metaNodeStr );
		
		status = ParseAuthAuthority( authAuthorityStr, &aaVersion, &aaTag, &aaData );
		if ( status != eDSNoErr )
			throw( status );
		
		*outAuthAuthType = ConvertTagToConstant( aaTag );
		switch( *outAuthAuthType )
		{
			case kAuthTypePasswordServer:
				{
					char *endPtr = strchr( aaData, ':' );
					if ( endPtr != NULL )
					{
						*endPtr++ = '\0';
						strcpy( inOutUserID, aaData );
						strcpy( inOutServerAddress, endPtr );
					}
				}
				break;
			
			default:
				strcpy( inOutUserID, inUsername );
				break;
		}
		
		if ( outAAData != NULL )
			*outAAData = aaData;
	}
	
	catch( long errCode )
	{
		status = errCode;
	}
	
	if ( authAuthorityStr != NULL )
		free( authAuthorityStr );
	
	return status;
}


//-----------------------------------------------------------------------------
//	 GetAuthAuthorityWithNode
//
//	Returns: DS error
//-----------------------------------------------------------------------------

long GetAuthAuthorityWithNode(
	const char *inNodeName,
	const char *inUsername,
	AuthAuthType *outAuthAuthType,
    char *inOutUserID,
	char *inOutServerAddress,
	char **outAAData )
{
    tDirReference				dsRef				= 0;
    tDataBuffer				   *tDataBuff			= 0;
    tDirNodeReference			nodeRef				= 0;
    long						status				= eDSNoErr;
    tContextData				context				= nil;
	tAttributeValueEntry	   *pExistingAttrValue	= NULL;
    unsigned long				index				= 0;
    unsigned long				nodeCount			= 0;
	unsigned long				attrValIndex		= 0;
    unsigned long				attrValCount		= 0;
	tDataList				   *nodeName			= nil;
    tRecordReference			recordRef			= 0;
    tDataNode				   *attrTypeNode		= nil;
    tDataNodePtr				recordTypeNode		= nil;
    tDataNodePtr				recordNameNode		= nil;
    tAttributeEntryPtr			pAttrEntry			= nil;
    char						*aaVersion			= nil;
    char						*aaTag				= nil;
    char						*aaData				= nil;
    
    try
    {
		if ( inNodeName == nil || inUsername == nil || outAuthAuthType == nil || inOutUserID == nil || inOutServerAddress == nil )
			throw(-1);
		
        debug("\nGet AuthAuthority with nodename = %s and username = %s\n", inNodeName, inUsername);

		*inOutUserID = '\0';
		*inOutServerAddress = '\0';
		
        dsRef = myClass.GetDirRef();
		if (dsRef == 0)
			throw((long)-1);
		
		tDataBuff = dsDataBufferAllocate( dsRef, 4096 );
		if (tDataBuff == 0)
			throw((long)-1);
        
		nodeName = ::dsBuildFromPath( dsRef, inNodeName, "/" );
		if ( nodeName == nil )
			throw((long)-1);
		
        // find
		status = dsFindDirNodes( dsRef, tDataBuff, nodeName, eDSiExact, &nodeCount, &context );
        debug("dsFindDirNodes = %ld, nodeCount = %ld\n", status, nodeCount);
		if ( nodeCount < 1 ) {
            status = eDSNodeNotFound;
            debug("dsFindDirNodes returned 0 nodes\n");
        }
        dsDataListDeallocate( dsRef, nodeName );
		free( nodeName );
		nodeName = nil;
		
		if (status != eDSNoErr)
			throw(status);
		
        for ( index = 1; index <= nodeCount; index++ )
        {
            // initialize state
            pExistingAttrValue = nil;
            
            status = dsGetDirNodeName( dsRef, tDataBuff, index, &nodeName );
            debug("dsGetDirNodeName = %ld\n", status);
            if (status != eDSNoErr) continue;
            
            status = dsOpenDirNode( dsRef, nodeName, &nodeRef );
            dsDataListDeallocate( dsRef, nodeName );
            free( nodeName );
            nodeName = nil;
            debug("dsOpenDirNode = %ld\n", status);
            if (status != eDSNoErr) continue;
            
            recordTypeNode = dsDataNodeAllocateString( dsRef, kDSStdRecordTypeUsers );
            recordNameNode = dsDataNodeAllocateString( dsRef, inUsername );
            status = dsOpenRecord( nodeRef, recordTypeNode, recordNameNode, &recordRef );
            debug("dsOpenRecord = %ld\n", status);
            if (status != eDSNoErr) continue;
            
            // get info about this attribute
            attrTypeNode = dsDataNodeAllocateString( 0, kDSNAttrAuthenticationAuthority );
            status = dsGetRecordAttributeInfo( recordRef, attrTypeNode, &pAttrEntry );
            debug("dsGetRecordAttributeInfo = %ld\n", status);
            if ( status == eDSNoErr )
            {
                // run through the values and replace the ApplePasswordServer authority if it exists
                attrValCount = pAttrEntry->fAttributeValueCount;
                for ( attrValIndex = 1; attrValIndex <= attrValCount; attrValIndex++ )
                {
                    status = dsGetRecordAttributeValueByIndex( recordRef, attrTypeNode, attrValIndex, &pExistingAttrValue );
                    debug("dsGetRecordAttributeValueByIndex = %ld\n", status);
                    if (status != eDSNoErr) continue;
                    
                    status = ParseAuthAuthority( pExistingAttrValue->fAttributeValueData.fBufferData, &aaVersion, &aaTag, &aaData );
                    if (status != eDSNoErr) continue;
                    
					*outAuthAuthType = ConvertTagToConstant( aaTag );
					switch( *outAuthAuthType )
					{
						case kAuthTypePasswordServer:
							{
								char *endPtr = strchr( aaData, ':' );
								if ( endPtr != NULL )
								{
									*endPtr++ = '\0';
									strcpy( inOutUserID, aaData );
									strcpy( inOutServerAddress, endPtr );
								}
							}
							break;
						
						default:
							strcpy( inOutUserID, inUsername );
							break;
					}
					
					if ( outAAData != NULL )
						*outAAData = aaData;
                }
            }
            
            if (recordRef != 0) {
                dsCloseRecord( recordRef );
                recordRef = 0;
            }
            if (nodeRef != 0) {
                dsCloseDirNode(nodeRef);
                nodeRef = 0;
            }
        }
    }
    catch( long errCode )
	{
		status = errCode;
	}
    
    if (recordRef != 0) {
        dsCloseRecord( recordRef );
        recordRef = 0;
    }
    if (tDataBuff != NULL) {
		dsDataBufferDeAllocate( dsRef, tDataBuff );
		tDataBuff = NULL;
	}
    if (nodeRef != 0) {
		dsCloseDirNode(nodeRef);
		nodeRef = 0;
	}
		
	return status;
}


//-----------------------------------------------------------------------------
//	 GetPWServerAddresses
//
//	Returns: all locally hosted password server IP addresses, comma delimited
//-----------------------------------------------------------------------------

void GetPWServerAddresses(char *outAddressStr)
{
    tDirReference				dsRef				= 0;
    tDataBuffer				   *tDataBuff			= 0;
    tDirNodeReference			nodeRef				= 0;
    long						status				= eDSNoErr;
    tContextData				context				= nil;
	tAttributeValueEntry	   *pAttrValueEntry		= NULL;
    unsigned long				index				= 0;
    unsigned long				nodeCount			= 0;
	tDataList				   *nodeName			= nil;
    tRecordReference			recordRef			= 0;
    tDataNode				   *attrTypeNode		= nil;
    tDataNodePtr				recordTypeNode		= nil;
    tDataNodePtr				recordNameNode		= nil;
    
    *outAddressStr = '\0';
    
    do
    {
		status = dsOpenDirService( &dsRef );
		if (status != eDSNoErr) break;
		
		tDataBuff = dsDataBufferAllocate( dsRef, 4096 );
		if (tDataBuff == 0) break;
        
        // find and don't open
		status = dsFindDirNodes( dsRef, tDataBuff, nil, eDSLocalHostedNodes, &nodeCount, &context );
        if (status != eDSNoErr) break;
        if ( nodeCount < 1 ) {
            status = eDSNodeNotFound;
            break;
        }
        
        for ( index = 1; index <= nodeCount; index++ )
        {
            status = dsGetDirNodeName( dsRef, tDataBuff, index, &nodeName );
            if (status != eDSNoErr) break;
            
            status = dsOpenDirNode( dsRef, nodeName, &nodeRef );
            dsDataListDeallocate( dsRef, nodeName );
            free( nodeName );
            nodeName = nil;
            if (status != eDSNoErr) break;
            
            recordTypeNode = dsDataNodeAllocateString( dsRef, kDSStdRecordTypeConfig );
            recordNameNode = dsDataNodeAllocateString( dsRef, "passwordserver" );
            status = dsOpenRecord( nodeRef, recordTypeNode, recordNameNode, &recordRef );
            if (status != eDSNoErr) continue;
            
            attrTypeNode = dsDataNodeAllocateString( 0, kDS1AttrPasswordServerLocation );
            status = dsGetRecordAttributeValueByIndex( recordRef, attrTypeNode, 1, &pAttrValueEntry );
            if (status != eDSNoErr) break;
            
            if ( *outAddressStr != '\0' )
                strcat( outAddressStr, "," );
            strcat( outAddressStr, (char *)&(pAttrValueEntry->fAttributeValueData.fBufferData) );
            
            if (recordRef != 0) {
                dsCloseRecord( recordRef );
                recordRef = 0;
            }
            if (nodeRef != 0) {
                dsCloseDirNode(nodeRef);
                nodeRef = 0;
            }
        }
	}
	while (false);
	
	// Local LDAP node
	if ( status != eDSNoErr )
	{
		do
		{
			nodeName = ::dsBuildFromPath( dsRef, "/LDAPv3/127.0.0.1", "/" );
			if ( nodeName == nil )
				break;
			
			status = dsFindDirNodes( dsRef, tDataBuff, nodeName, eDSiExact, &nodeCount, &context );
			if (status != eDSNoErr) break;
			if ( nodeCount < 1 ) {
				status = eDSNodeNotFound;
				break;
			}
			
			status = dsGetDirNodeName( dsRef, tDataBuff, 1, &nodeName );
			if (status != eDSNoErr) break;
			
			status = dsOpenDirNode( dsRef, nodeName, &nodeRef );
			dsDataListDeallocate( dsRef, nodeName );
			free( nodeName );
			nodeName = nil;
			if (status != eDSNoErr) break;
			
			recordTypeNode = dsDataNodeAllocateString( dsRef, kDSStdRecordTypeConfig );
			recordNameNode = dsDataNodeAllocateString( dsRef, "passwordserver" );
			status = dsOpenRecord( nodeRef, recordTypeNode, recordNameNode, &recordRef );
			if (status != eDSNoErr) break;
			
			attrTypeNode = dsDataNodeAllocateString( 0, kDS1AttrPasswordServerLocation );
			status = dsGetRecordAttributeValueByIndex( recordRef, attrTypeNode, 1, &pAttrValueEntry );
			if (status != eDSNoErr) break;
			
			if ( *outAddressStr != '\0' )
				strcat( outAddressStr, "," );
			strcat( outAddressStr, (char *)&(pAttrValueEntry->fAttributeValueData.fBufferData) );
		}
		while(false);
		
		if (recordRef != 0) {
			dsCloseRecord( recordRef );
			recordRef = 0;
		}
		if (tDataBuff != NULL) {
			dsDataBufferDeAllocate( dsRef, tDataBuff );
			tDataBuff = NULL;
		}
		if (nodeRef != 0) {
			dsCloseDirNode(nodeRef);
			nodeRef = 0;
		}
		if (dsRef != 0) {
			dsCloseDirService(dsRef);
			dsRef = 0;
		}
	}
}


// ---------------------------------------------------------------------------
//	* ConvertTagToConstant
//
//	Returns: AuthAuthType
//
//	Translates the authentication-authority tag to its enumerated value.
// ---------------------------------------------------------------------------

AuthAuthType ConvertTagToConstant( const char *inAuthAuthorityTag )
{
	AuthAuthType returnType = kAuthTypeUnknown;
	
	if ( strcasecmp( inAuthAuthorityTag, kDSTagAuthAuthorityPasswordServer ) == 0 )
	{
		returnType = kAuthTypePasswordServer;
	}
	else
	if ( strcasecmp( inAuthAuthorityTag, kDSTagAuthAuthorityShadowHash ) == 0 )
	{
		returnType = kAuthTypeShadowHash;
	}
	else
	if ( strcasecmp( inAuthAuthorityTag, kDSTagAuthAuthorityKerberosv5 ) == 0 )
	{
		returnType = kAuthTypeKerberos;
	}
	else
	if ( strcasecmp( inAuthAuthorityTag, kDSTagAuthAuthorityDisabledUser ) == 0 )
	{
		returnType = kAuthTypeDisabled;
	}
	
	return returnType;
}


// ---------------------------------------------------------------------------
//	* ParseAuthAuthority
//    retrieve version, tag, and data from authauthority
//    format is version;tag;data
// ---------------------------------------------------------------------------

long ParseAuthAuthority ( const char *inAuthAuthority,
                            char ** outVersion,
                            char ** outAuthTag,
                            char ** outAuthData )
{
	char* authAuthority = NULL;
	char* current = NULL;
	char* tempPtr = NULL;
	long result = eDSAuthFailed;
	if ( inAuthAuthority == NULL || outVersion == NULL 
		 || outAuthTag == NULL || outAuthData == NULL )
	{
		return eDSAuthFailed;
	}
	authAuthority = strdup(inAuthAuthority);
	if (authAuthority == NULL)
	{
		return eDSAuthFailed;
	}
	current = authAuthority;
	do
	{
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outVersion = strdup(tempPtr);
		
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outAuthTag = strdup(tempPtr);
		
		result = eDSNoErr;
		
		*outAuthData = NULL;
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outAuthData = strdup(tempPtr);
	}
	while (false);
	
	free(authAuthority);
	authAuthority = NULL;
	return result;
}


