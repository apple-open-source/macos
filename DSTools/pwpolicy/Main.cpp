/*
 * Copyright (c) 2000-2009 Apple Inc. All rights reserved.
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
#include <OpenDirectory/OpenDirectory.h>

#include "PwdPolicyTool.h"
#include "dstools_version.h"

#define debug(A, args...)					\
	if (gVerbose)							\
		fprintf(stderr, (A), ##args);		

#define kNodeNotFoundMsg					"Cannot access directory node.\n"
#define kNotPasswordServerUserMsg			"%s is not a password server account.\n"
#define kUserNotOnNodeMsg					"%s is not a user on this directory node.\n"

static const char *sDatePolicyStr[] = {
	"expirationDateGMT=",
	"hardExpireDateGMT=",
	"warnOfExpirationMinutes=",
	"warnOfDisableMinutes=",
	"projectedPasswordExpireDate=",
	"projectedAccountDisableDate=",
	"validAfter=",
	NULL
};

typedef enum Command {
	// 0-6 are order-dependent
	kCmdNone,
	kCmdGetGlobalPolicy,
	kCmdSetGlobalPolicy,
	kCmdGetPolicy,
	kCmdSetPolicy,
	kCmdSetPolicyGlobal,
	kCmdSetPassword,
	
	// not order-dependent
	kCmdEnableUser,
	kCmdGetGlobalHashTypes,
	kCmdSetGlobalHashTypes,
	kCmdGetHashTypes,
	kCmdSetHashTypes,
	kCmdGetEffectivePolicy,
	kCmdEnableWindowsSharing,
	kCmdDisableWindowsSharing,
	kCmdAppleVersion
} Command;

typedef struct CommandTableEntry {
	const char *name;
	Command cmd;
} CommandTableEntry;

typedef enum AuthAuthType {
	kAuthTypeUnknown,
	kAuthTypePasswordServer,
	kAuthTypeShadowHash,
	kAuthTypeKerberos,
	kAuthTypeDisabled
};

Command get_command_id( const char *commandStr );
void PrintErrorMessage( long error, const char *username );

char *ConvertPolicyLongs(const char *inPolicyStr);
char *ConvertPolicyDates(const char *inPolicyStr);
char *ConvertDictionary(CFDictionaryRef dict);
Boolean PreflightDate(const char *theDateStr);

long GetAuthAuthority(
	const char *inNodeName,
	const char *inUsername,
	const char *inRecordType,
	AuthAuthType *outAuthAuthType,
    char *inOutUserID,
	char *inOutServerAddress,
	char **outMetaNode,
	char **outAAData );
	
long GetAuthAuthorityWithSearchNode(
	const char *inUsername,
	const char *inRecordType,
	AuthAuthType *outAuthAuthType,
    char *inOutUserID,
	char *inOutServerAddress,
	char **outMetaNode,
	char **outAAData );

long GetAuthAuthorityWithNode(
	const char *inNodeName,
	const char *inUsername,
	const char *inRecordType,
	AuthAuthType *outAuthAuthType,
    char *inOutUserID,
	char *inOutServerAddress,
	char **outAAData );

AuthAuthType ConvertTagToConstant( const char *inAuthAuthorityTag );
void GetPWServerAddresses(char *outAddressStr);

// Globals
CommandTableEntry gCommandTable[] = 
{
	{ "-appleversion",				kCmdAppleVersion },
	{ "-getglobalpolicy",			kCmdGetGlobalPolicy },
	{ "-setglobalpolicy",			kCmdSetGlobalPolicy },
	{ "-getpolicy",					kCmdGetPolicy },
	{ "-setpolicy",					kCmdSetPolicy },
	{ "-setpolicyglobal",			kCmdSetPolicyGlobal },
	{ "-setpassword",				kCmdSetPassword },
	{ "-enableuser",				kCmdEnableUser },
	{ "-getglobalhashtypes",		kCmdGetGlobalHashTypes },
	{ "-setglobalhashtypes",		kCmdSetGlobalHashTypes },
	{ "-gethashtypes",				kCmdGetHashTypes },
	{ "-sethashtypes",				kCmdSetHashTypes },
	{ "--get-effective-policy",		kCmdGetEffectivePolicy },
	{ "--enable-windows-sharing",	kCmdEnableWindowsSharing },
	{ "--disable-windows-sharing",	kCmdDisableWindowsSharing },
	
	{ NULL,						kCmdNone }
};


bool	gVerbose			= false;
bool	sTerminateServer	= false;

void	DoHelp		( FILE *inFile, const char *inArgv0 );
void	usage(void);
void    invalid_auth(void);

char *read_passphrase(const char *prompt, int from_stdin);

const UInt32 kMaxTestUsers		= 150;
const UInt32 kStressTestUsers	= 2000;
const UInt32 kAllTestUsers		= 123;

PwdPolicyTool myClass;

//-----------------------------------------------------------------------------
// * main()
//-----------------------------------------------------------------------------

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
    Command				commandNum				= kCmdNone;
	Command				tmpCommandNum			= kCmdNone;
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
	char				**localNodeNameList		= NULL;
	bool				useRootPrivileges		= false;
	bool				authenticatorIsDefault	= false;
	bool				userIsDefault			= false;
	const char			*recordType				= kDSStdRecordTypeUsers;


	// read command line
	for ( int index = 1; index < argc; index++ )
	{
		tmpCommandNum = get_command_id( argv[index] );
		switch ( tmpCommandNum )
		{
			case kCmdAppleVersion:
				dsToolAppleVersionExit( argv[0] );
				break;
			
			case kCmdGetGlobalPolicy:
			case kCmdGetPolicy:
			case kCmdGetGlobalHashTypes:
			case kCmdGetHashTypes:
			case kCmdGetEffectivePolicy:
				break;
				
			case kCmdSetGlobalPolicy:
			case kCmdSetPolicy:
			case kCmdSetPolicyGlobal:
			case kCmdSetPassword:
			case kCmdEnableUser:
			case kCmdSetGlobalHashTypes:
			case kCmdSetHashTypes:
			case kCmdEnableWindowsSharing:
			case kCmdDisableWindowsSharing:
				bReadPassword = true;
				break;
			
			default:
				if ( argv[index][0] == '-' )
				{
					p = argv[index];
					p++;
					
					while (*p)
					{
						if ( *p == 'a' || *p == 'c' || *p == 'u' || *p == 'p' || *p == 'n' )
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
							
							case 'c':
								if ( username != NULL ) {
									fprintf( stderr, "-u and -c are mutually exclusive and may only appear once.\n" );
									exit( EX_USAGE );
								}
								username = argv[index+1];
								recordType = kDSStdRecordTypeComputers;
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
								bzero( argv[index+1], strlen(argv[index+1]) );
								break;
							
							case 'u':
								if ( username != NULL ) {
									fprintf( stderr, "-u and -c are mutually exclusive and may only appear once.\n" );
									exit( EX_USAGE );
								}
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
								tmpCommandNum = (Command)(*p - '0' + 1);
								break;
							
							default:
								usage();
								exit(0);
								break;
						}
						
						p++;
					}
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
		 commandNum == kCmdSetGlobalHashTypes ||
		 commandNum == kCmdSetHashTypes )
	{
		if ( argv[argc-1][0] == '-' )
		{
			usage();
			exit(0);
		}
	}
	
	// if no username or authenticator is specified, assume the identity of the session
	if ( geteuid() == 0 && authenticator == NULL )
	{
		useRootPrivileges = true;
		authType = kAuthTypeShadowHash;
	}
	else if ( username == NULL || authenticator == NULL )
	{
		struct passwd *userRec = getpwuid(geteuid());
		if ( userRec != NULL && userRec->pw_name != NULL )
		{
			/* global static mem is volatile; must strdup */
			/* this is a one-shot tool, so no need to worry about */
			/* calling free at the end */
			if ( username == NULL ) {
				username = strdup( userRec->pw_name );
				userIsDefault = true;
			}
			if ( authenticator == NULL ) {
				authenticator = strdup( userRec->pw_name );
				authenticatorIsDefault = true;
			}
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
			siStatus = GetAuthAuthority( nodename, username, recordType, &userAuthType, userID, serverAddress, &metaNode, &aaData );
			if ( siStatus != eDSNoErr )
			{
				if ( userIsDefault ) {
					free( username );
					username = NULL;
					siStatus = eDSNoErr;
				}
			}
		}
		
		if (useRootPrivileges && (0!=strcmp(nodename ?: (metaNode ?: "n/a"), "/Local/Default"))) { // root is only valid for /Local/Default
			useRootPrivileges = false;
		}
			
		// prompt for password if required and not provided on the command line
		if ( bReadPassword && password[0] == '\0' && !useRootPrivileges )
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
		// get authenticator info (if applicable)
		if ( authenticator != NULL )
		{
			siStatus = GetAuthAuthority( nodename, authenticator, kDSStdRecordTypeUsers, &authType, authenticatorID, serverAddress, NULL, NULL );
			if ( siStatus != eDSNoErr )
			{
				if ( authenticatorIsDefault ) {
					free( authenticator );
					authenticator = NULL;
					siStatus = eDSNoErr;
				}
				else {
					PrintErrorMessage( siStatus, authenticator );
					exit(0);
				}
			}
		}
		
		if ( userAuthType != authType && authenticatorIsDefault )
		{
			free( authenticator );
			authenticator = NULL;
			authType = kAuthTypeUnknown;
		}
		
		// userAuthType must be known for some commands
		if ( userAuthType == kAuthTypeUnknown )
		{
			switch ( commandNum )
			{
				case kCmdGetPolicy:
				case kCmdSetPolicy:
				case kCmdSetPassword:
				case kCmdGetEffectivePolicy:
					usage();
					break;
			}
		}
		
		// if authType is still unknown, take a best guess.
		if ( authType == kAuthTypeUnknown &&
			 (commandNum == kCmdGetGlobalPolicy ||
			  commandNum == kCmdSetGlobalPolicy ||
			  commandNum == kCmdGetPolicy ||
			  commandNum == kCmdGetEffectivePolicy) )
		{
			// default
			authType = kAuthTypePasswordServer;
			
			// check for local node if possible
			if ( nodename != NULL &&
				 myClass.FindDirectoryNodes(NULL, eDSLocalNodeNames, &localNodeNameList, false) == eDSNoErr &&
				 localNodeNameList != NULL )
			{
				// eDSLocalNodeNames should be called "eDSLocalNodeName" because there is only one.
				if ( localNodeNameList[0] != NULL ) {
					if ( strcmp(nodename, localNodeNameList[0]) == 0 ) {
						authType = kAuthTypeShadowHash;
					}
					free( localNodeNameList[0] );
				}
				free( localNodeNameList );
			}
		}
		
		// -getglobalpolicy with no arguments gets the password server's global policy
		if ( commandNum == kCmdGetGlobalPolicy && nodename == NULL && userIsDefault && authenticatorIsDefault )
			authType = kAuthTypePasswordServer;
		
		switch( commandNum )
		{
			case kCmdGetGlobalHashTypes:
			case kCmdSetGlobalHashTypes:
				username = strdup("");
				authType = kAuthTypeShadowHash;
				break;

			case kCmdGetHashTypes:
			case kCmdSetHashTypes:
				authType = kAuthTypeShadowHash;
				break;
			
			case kCmdEnableWindowsSharing:
			case kCmdDisableWindowsSharing:
				if ( userAuthType != kAuthTypeShadowHash )
				{
					fprintf(stderr, "The hash types can be set only for ShadowHash accounts.\n");
					exit(0);
				}
				break;
		
			default:
				break;
		}
		
		switch( authType )
		{
			case kAuthTypeUnknown:
				switch ( commandNum) {
					case kCmdSetPolicy:
					case kCmdGetGlobalPolicy:
					case kCmdGetEffectivePolicy:
					case kCmdGetPolicy:
					case kCmdGetHashTypes:
						// auth not needed
						break;
						
					default:
						PrintErrorMessage( siStatus, username );
						exit(0);
						break;
				}
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
				if ( nodename != NULL ) {
					strlcpy(nodeName, nodename, sizeof(nodeName));
				} else if (metaNode != NULL) {
					strlcpy(nodeName, metaNode, sizeof(nodeName));
				} else {
					PrintErrorMessage( eDSNullNodeName, username );
					exit(0);
				}
				
				break;
				
			case kAuthTypeShadowHash:
			case kAuthTypeDisabled:
				if ( myClass.FindDirectoryNodes( NULL, eDSLocalNodeNames, &localNodeNameList, false ) == eDSNoErr &&
					localNodeNameList != NULL || localNodeNameList[0] != NULL ) {
					strcpy( nodeName, localNodeNameList[0] );
					free( localNodeNameList[0] );
					free( localNodeNameList );
				} else {
					fprintf(stderr, "Error: could not resolve the name of the local node.\n");
					exit(0);
				}
				break;
			
			case kAuthTypeKerberos:
				if ( nodename != NULL ) {
					strlcpy(nodeName, nodename, sizeof(nodeName));
				} else if (metaNode != NULL) {
					strlcpy(nodeName, metaNode, sizeof(nodeName));
				} else {
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
					myClass.DoNodePWAuth(
								nodeRef,
								NULL, "",
								kDSStdAuthGetGlobalPolicy, "", NULL, recordType, authResult );
					tptr = ConvertPolicyLongs( authResult );
					printf("%s\n", tptr ? tptr : authResult);
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
						myClass.DoNodePWAuth( nodeRef,
											useRootPrivileges ? "" : authenticatorID,
											useRootPrivileges ? "" : password,
											kDSStdAuthSetGlobalPolicy,
											tptr, NULL, recordType, NULL );
						
						myClass.CloseDirectoryNode( nodeRef );
						
						free( tptr );
					}
				}
				else
				{
					usage();
				}
				break;
			
			case kCmdGetEffectivePolicy: {
				CFErrorRef error = NULL;
				CFStringRef cfnode = CFStringCreateWithCString(kCFAllocatorDefault, nodeName, kCFStringEncodingUTF8);
				ODNodeRef node = ODNodeCreateWithName(kCFAllocatorDefault, kODSessionDefault, cfnode, &error);
				if (error) {
					CFShow(error);
				} else {
					CFStringRef recordName = CFStringCreateWithCString(kCFAllocatorDefault, username, kCFStringEncodingUTF8);
					ODQueryRef query = ODQueryCreateWithNode(kCFAllocatorDefault, node, kODRecordTypeUsers,
															 kODAttributeTypeRecordName, kODMatchEqualTo, 
															 recordName, kODAttributeTypeAllAttributes, 1, &error);
					if (error) {
						CFShow(error);
					} else {
						CFArrayRef records = ODQueryCopyResults(query, false, &error);
						if (error) {
							CFShow(error);
						} else {
							if (records != NULL) {
								if ( (CFGetTypeID(records) != CFArrayGetTypeID()) ||
									 (CFArrayGetCount(records) != 1)) {
									PrintErrorMessage(eDSRecordNotFound, username);
									CFRelease(records);
									records = NULL;
								}
							}
							ODRecordRef record = records ? (ODRecordRef) CFArrayGetValueAtIndex(records, 0) : NULL;
							if (record) {
								CFDictionaryRef dict = ODRecordCopyPasswordPolicy(kCFAllocatorDefault, record, &error);
								if (error) {
									CFShow(error);
								} else {
									tptr = ConvertDictionary(dict);
									printf("%s\n", tptr);
									free(tptr);
								}
								if (dict != NULL) {
									CFRelease(dict);
								}
							}
						}
						if (records != NULL) {
							CFRelease(records);
						}
					}
					if (query != NULL) {
						CFRelease(query);
					}
					CFRelease(recordName);
				}
				if (error != NULL) {
					CFRelease(error);
				}
				if (node != NULL) {
					CFRelease(node);
				}
				CFRelease(cfnode);
			}
			break;

			case kCmdGetPolicy:
				if ( username != NULL )
				{
					char *nodeToUse = metaNode ? metaNode : nodeName;
					strlcpy( userID, username, sizeof(userID) );
					
					printf( "Getting policy for %s %s\n\n", username, nodeToUse );
					if ( myClass.OpenDirNode( nodeToUse, &nodeRef ) == eDSNoErr )
					{
						myClass.DoNodePWAuth(nodeRef, userID, NULL, kDSStdAuthGetPolicy, userID, NULL, recordType, authResult );
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
				if ( (useRootPrivileges == true || (authenticator != NULL && password != NULL)) && username != NULL && tptr != NULL )
				{
					printf( "Setting policy for %s\n", username );
					if ( myClass.OpenDirNode( nodeName, &nodeRef ) == eDSNoErr )
					{
					  if (useRootPrivileges == false && authenticator != NULL && password != NULL) {
					      myClass.DoNodeNativeAuth( nodeRef, authenticator, password);
					  }
						myClass.DoNodePWAuth( nodeRef,
												NULL,
												NULL,
												kDSStdAuthSetPolicyAsRoot,
												username,
												tptr, NULL, NULL );
						
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
											useRootPrivileges ? "" : authenticatorID,
											useRootPrivileges ? "" : password,
											kDSStdAuthSetPolicy,
											userID,
											"newPasswordRequired=0 usingHistory=0 usingExpirationDate=0 "
											"usingHardExpirationDate=0 requiresAlpha=0 requiresNumeric=0 "
											"maxMinutesUntilChangePassword=0 maxMinutesUntilDisabled=0 "
											"maxMinutesOfNonUse=0 maxFailedLoginAttempts=0 "
											"minChars=0 maxChars=0 resetToGlobalDefaults=1", recordType, NULL );
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
					char *userPassPtr = NULL;
					char *verifyPassPtr = NULL;
					
					printf( "Setting password for %s\n", username );
					if ( strcmp(argv[argc-1], "-setpassword") == 0 )
					{
						char prompt[256];
						snprintf(prompt, sizeof(prompt), "Enter new password for %s:", username);
						userPassPtr = read_passphrase( prompt, 1 );
						verifyPassPtr = read_passphrase( "Verify new password:", 1 );
						if ( strcmp(userPassPtr, verifyPassPtr) != 0 )
						{
							printf( "Password mismatch.\n" );
							bzero( userPassPtr, strlen(userPassPtr) );
							free( userPassPtr );
							bzero( verifyPassPtr, strlen(verifyPassPtr) );
							free( verifyPassPtr );
							return EX_TEMPFAIL;
						}
					}
					else
					{
						userPassPtr = strdup( argv[argc-1] );
					}
					
					if ( myClass.OpenDirNode( nodeName, &nodeRef ) == eDSNoErr )
					{
						if ( useRootPrivileges && password[0] == '\0' && userAuthType == kAuthTypeShadowHash )
						{
							myClass.DoNodePWAuth( nodeRef,
													userID,
													userPassPtr,
													kDSStdAuthSetPasswdAsRoot,
													NULL, NULL, NULL, NULL );
						}
						else
						{
							if (myClass.DoNodeNativeAuth( nodeRef, authenticator, password) == eDSNoErr) {
								myClass.DoNodePWAuth( nodeRef,
													userID,
													userPassPtr,
													kDSStdAuthSetPasswdAsRoot,
													"",
													"", recordType, NULL );
							}
						}
						myClass.CloseDirectoryNode( nodeRef );
					}
					if ( userPassPtr != NULL )
					{
						bzero( userPassPtr, strlen(userPassPtr) );
						free( userPassPtr );
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
					printf( "Enabling account for user %s\n\n", username );
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
									if ( myClass.OpenRecord( nodeRef, recordType, username, &recRef ) == eDSNoErr )
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
					myClass.SetHashTypes( authenticator, useRootPrivileges ? NULL : password, commandArgIndex + 1, argc, argv );
				else
					invalid_auth();
				break;
				
			case kCmdGetHashTypes:
				if ( aaData != NULL && aaData[0] != '\0' && strcasecmp(aaData, kDSTagAuthAuthorityBetterHashOnly) != 0 )
				{
					char *hashTypeStr = NULL;
					char *hashListPtr = NULL;
					char *hashListStr = strdup( aaData );
					char *endPtr = NULL;
					
					printf( "Getting hash types for user %s\n\n", username );
					
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
					
					if ( myClass.GetHashTypes( &hashTypesStr, aaData ? (strcasecmp(aaData, kDSTagAuthAuthorityBetterHashOnly) == 0) : false ) == eDSNoErr )
					{
						printf( "%s\n", hashTypesStr );
						free( hashTypesStr );
					}
				}
				break;
			
			case kCmdSetHashTypes:
                if ( aaData == NULL ) {
                    PrintErrorMessage(0, username);
                    exit(EX_CONFIG);
                }
				if ( (geteuid() == 0) || (authenticator != NULL && password != NULL) )
				{
					printf( "Setting hash types for user %s\n", username );
					
					if ( myClass.OpenDirNode( metaNode ? metaNode : nodeName, &nodeRef ) == eDSNoErr )
					{
						tRecordReference recRef;
						int result = 0;
						
						if ( authenticator != NULL )
							siStatus = myClass.DoNodeNativeAuth( nodeRef, authenticator, password );
						else 
							siStatus = eDSNoErr;
						
						if ( siStatus == eDSNoErr )
						{
							if ( myClass.OpenRecord( nodeRef, recordType, username, &recRef ) == eDSNoErr )
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
					invalid_auth();
				break;
			
			case kCmdEnableWindowsSharing:
			case kCmdDisableWindowsSharing:
				if ( (geteuid() == 0) || (authenticator != NULL && password != NULL) )
				{
					printf( "%s Windows sharing for user %s\n",
							(commandNum == kCmdEnableWindowsSharing) ? "Enabling" : "Disabling",
							username );
					
					if ( myClass.OpenDirNode(metaNode ? metaNode : nodeName, &nodeRef) == eDSNoErr )
					{
						tRecordReference recRef;
						int result = 0;
						
						if ( authenticator != NULL )
							siStatus = myClass.DoNodeNativeAuth( nodeRef, authenticator, password );
						
						siStatus = myClass.DoNodePWAuth( nodeRef, username, argv[argc-1],
										(commandNum == kCmdEnableWindowsSharing) ? "dsAuthMethodStandard:dsAuthSetShadowHashWindows" : "dsAuthMethodStandard:dsAuthSetShadowHashSecure",
										NULL, NULL, recordType, NULL );
						
						myClass.CloseDirectoryNode( nodeRef );
					}
					
					if ( siStatus != eDSNoErr )
						printf( "Could not access account <%s>.\n", username );
				}
				else
					invalid_auth();
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
//	get_command_id
//-----------------------------------------------------------------------------

Command get_command_id( const char *commandStr )
{	
	for ( int index = 0; gCommandTable[index].name != NULL; index++ )
	{
		if ( strcmp(gCommandTable[index].name, commandStr) == 0 )
			return gCommandTable[index].cmd;
	}
	
	return kCmdNone;
}


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
typedef struct UsageLine {
	const char *cmd;
	const char *desc;
} UsageLine;

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
	
	static const char * _szpUsage =
		"Usage: %s [-h]\n"
		"Usage: %s [-v] [-a authenticator] [-p password] [-u username | -c computername]\n"
        	"                [-n nodename] command command-arg\n"
		"Usage: %s [-v] [-a authenticator] [-p password] [-u username | -c computername]\n"
        	"                [-n nodename] command \"policy1=value1 policy2=value2 ...\"\n"
		"\n"
		"  -a       name of the authenticator\n"
		"  -c       name of the computer account to modify\n"
		"  -p       password (omit this option for a secure prompt)\n"
		"  -u       name of the user account to modify\n"
		"  -h       help\n"
		"  -n       directory-node to search, uses search node by default\n"
		"  -v       verbose\n"
		"\n";
	
	fprintf( inFile, _szpUsage, toolName, toolName, toolName );

	static UsageLine usage_line[] = {
		{"-getglobalpolicy",		"Get global policies."},
		{"",						"Specify a user if the password server"},
		{"",						"is not configured locally."},
		{"-setglobalpolicy",		"Set global policies"},
		{"-getpolicy",				"Get policies for a user"},
		{"--get-effective-policy",	"Gets the combination of global and user policies that apply to the user."},
		{"-setpolicy",				"Set policies for a user"},
		{"-setpolicyglobal",		"Set a user account to use global policies"},
		{"-setpassword",			"Set a new password for a user"},
		{"-enableuser",				"Enable a shadowhash user account that was disabled"},
		{"",						"by a password policy event."},
		{"-getglobalhashtypes",		"Returns a list of password hashes stored on disk by default."},
		{"-setglobalhashtypes",		"Edits the list of password hashes stored on disk by default."},
		{"-gethashtypes",			"Returns a list of password hashes stored on disk for"},
		{"",						"a user account."},
		{"-sethashtypes",			"Edits the list of password hashes stored on disk for"},
		{"",						"a user account."},
		{NULL, NULL}
	};
	
	for ( int idx = 0; usage_line[idx].cmd != NULL; idx++ )
		fprintf( inFile, "%25s\t%s\n", usage_line[idx].cmd, usage_line[idx].desc );
} // DoHelp


//-----------------------------------------------------------------------------
//	usage
//-----------------------------------------------------------------------------

void usage(void)
{
    fprintf(stdout, "usage: [-v] [-a authenticator] [-p password] [-u username | -c computername] [-n nodename] command args\n");
    exit(EX_USAGE);
}

void invalid_auth(void)
{
    fprintf(stdout, "Need either root privileges or valid authenticator\n");
    exit(EX_NOPERM);
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
//	ConvertPolicyLongs
//
//	Returns: malloc'd C-str, or NULL if a value is bad.
//	substitutes UInt32s with human-readable dates
//-----------------------------------------------------------------------------

char *ConvertPolicyLongs(const char *inPolicyStr)
{
	char *returnString = NULL;
	char *value = NULL;
	char *tempString = NULL;
	struct tm *timerec;
	time_t timeval;
	char scratchStr[256];
	
	tempString = (char *)malloc( strlen(inPolicyStr) + 100 );
	if ( tempString == NULL )
		return NULL;
	
	returnString = (char *)malloc( strlen(inPolicyStr) + 100 );
	if ( returnString == NULL ) {
		free( tempString );
		return NULL;
	}
	
	strcpy( returnString, inPolicyStr );
	
	for ( int idx = 0; sDatePolicyStr[idx] != NULL; idx++ )
	{
		value = strstr( returnString, sDatePolicyStr[idx] );
		if ( value != NULL )
		{
			value += strlen( sDatePolicyStr[idx] );
			strlcpy( tempString, returnString, value - returnString + 1 );
			timeval = 0;
			sscanf( value, "%lu", &timeval );
			timerec = ::gmtime( &timeval );
			strftime( scratchStr, sizeof(scratchStr), "%m/%d/%y", timerec );
			strcat( tempString, scratchStr );
			value = strchr( value, ' ' );
			if ( value != NULL )
				strcat( tempString, value );
			strcpy( returnString, tempString );
		}
	}
	
	if ( tempString != NULL ) {
		free( tempString );
		tempString = NULL;
	}
	
	return returnString;
}

char *
ConvertDictionary(CFDictionaryRef dict)
{
	CFMutableStringRef cfstr = CFStringCreateMutable(kCFAllocatorDefault, 1000);
	CFIndex dictCount = CFDictionaryGetCount(dict), i;
	
	CFTypeRef keys[dictCount];
	CFTypeRef values[dictCount];
	
	CFDictionaryGetKeysAndValues(dict, keys, values);
	
	for (i = 0; i < dictCount; i++) {
		if (i != 0) {
			CFStringAppend(cfstr, CFSTR(" "));
		}
		CFStringAppend(cfstr, (CFStringRef)keys[i]);
		CFStringAppend(cfstr, CFSTR("="));
		// avoid overflow
		CFNumberRef number = (CFNumberRef) values[i];
		int maxInteger = 0x7ffffff;
		CFNumberRef maxInt = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &maxInteger);
		if (CFNumberCompare(number, maxInt, NULL) == kCFCompareGreaterThan) {
			values[i] = maxInt;
		}
		CFStringRef cfval = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), values[i]);
		CFStringAppend(cfstr, cfval);
	}
	size_t stringSize = (size_t) CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8) + 1;
	char *string = (char *)malloc(stringSize);
	if (false == CFStringGetCString(cfstr, string, stringSize, kCFStringEncodingUTF8)) {
		free(string);
		return NULL;
	}
	char *string2 = ConvertPolicyLongs(string);
	free(string);
	return string2;
}

//-----------------------------------------------------------------------------
//	ConvertPolicyDates
//
//	Returns: malloc'd C-str, or NULL if a date is bad.
//	substitutes human-typeable dates for UInt32s
//-----------------------------------------------------------------------------

char *ConvertPolicyDates(const char *inPolicyStr)
{
	char *returnString = NULL;
	char *value = NULL;
	char *tempString = NULL;
	char *firstNonNeeded = NULL;
	struct tm timerec;
	int index;
	char scratchStr[256];
	
	tempString = (char *)malloc( strlen(inPolicyStr) + 100 );
	if ( tempString == NULL )
		return NULL;

	try
	{
		returnString = (char *)malloc( strlen(inPolicyStr) + 100 );
		if ( returnString == NULL )
			throw(1);
		
		strcpy( returnString, inPolicyStr );
		
		for ( int idx = 0; sDatePolicyStr[idx] != NULL; idx++ )
		{
			value = strstr( returnString, sDatePolicyStr[idx] );
			if ( value != NULL )
			{
				value += strlen( sDatePolicyStr[idx] );
				strlcpy( tempString, returnString, value - returnString + 1 );
				strlcpy( scratchStr, value, 9 );
				for ( index = 0; index < 8; index++ )
				{
					if ( scratchStr[index] == ' ' )
					{
						scratchStr[index] = '\0';
						break;
					}
				}
				
				if ( ! PreflightDate(scratchStr) )
					throw(1);
				
				bzero(&timerec, sizeof(timerec));
				firstNonNeeded = strptime(value, "%m/%d/%y", &timerec);
				if ( firstNonNeeded == NULL )
					throw(1);
				
				sprintf( scratchStr, "%lu", mktime(&timerec) );
				strcat( tempString, scratchStr );
				
				value = strchr( value, ' ' );
				if ( value != NULL )
					strcat( tempString, value );
				strcpy( returnString, tempString );
			}
		}
	}
	catch(...)
	{
		// fatal error, return NULL
		if ( returnString != NULL ) {
			free( returnString );
			returnString = NULL;
		}
	}
	
	if ( tempString != NULL ) {
		free( tempString );
		tempString = NULL;
	}
	
	return returnString;
}


//-----------------------------------------------------------------------------
//	PreflightDate
//
//	Returns: TRUE if the date is in a valid format
//	substitutes human-typeable dates into UInt32s
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
	const char *inRecordType,
    AuthAuthType *outAuthAuthType,
	char *inOutUserID,
	char *inOutServerAddress,
	char **outMetaNode,
	char **outAAData )
{
	long result = -1;
	
	if ( inNodeName == NULL )
		result = GetAuthAuthorityWithSearchNode( inUsername, inRecordType, outAuthAuthType, inOutUserID, inOutServerAddress, outMetaNode, outAAData );
	else
		result = GetAuthAuthorityWithNode( inNodeName, inUsername, inRecordType, outAuthAuthType, inOutUserID, inOutServerAddress, outAAData );
	
	return result;
}


//-----------------------------------------------------------------------------
//	GetAuthAuthorityWithSearchNode
//
//	Returns: DS status
//-----------------------------------------------------------------------------

long GetAuthAuthorityWithSearchNode(
	const char *inUsername,
	const char *inRecordType,
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
		status = myClass.GetUserByName( myClass.GetSearchNodeRef(), inUsername, inRecordType, &authAuthorityStr, &metaNodeStr );
		if ( status != eDSNoErr )
			throw( status );
		
		if ( outMetaNode != NULL )
			*outMetaNode = metaNodeStr;
		else
			free( metaNodeStr );
		
		status = dsParseAuthAuthority( authAuthorityStr, &aaVersion, &aaTag, &aaData );
		if ( status != eDSNoErr )
			throw( status );
		
		*outAuthAuthType = ConvertTagToConstant( aaTag );
		strcpy( inOutUserID, inUsername );
		
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
	const char *inRecordType,
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
    UInt32					index				= 0;
    UInt32					nodeCount			= 0;
	UInt32					attrValIndex		= 0;
    UInt32					attrValCount		= 0;
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
		do {
			status = dsFindDirNodes( dsRef, tDataBuff, nodeName, eDSiExact, &nodeCount, &context );
			if (status == eDSBufferTooSmall) {
				UInt32 newSize = tDataBuff->fBufferSize * 2;
				dsDataBufferDeAllocate(dsRef, tDataBuff);
				tDataBuff = dsDataBufferAllocate(dsRef, newSize);
			}
		} while (status == eDSBufferTooSmall);
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
            
            recordTypeNode = dsDataNodeAllocateString( dsRef, inRecordType );
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
                    
                    status = dsParseAuthAuthority( pExistingAttrValue->fAttributeValueData.fBufferData, &aaVersion, &aaTag, &aaData );
                    if (status != eDSNoErr) continue;
                    
					if ( strstr(inNodeName, "/Local") != NULL &&
						 strcmp(aaTag, "Kerberosv5") == 0 )
						 continue;
					
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
    UInt32					index				= 0;
    UInt32					nodeCount			= 0;
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
		do {
			status = dsFindDirNodes( dsRef, tDataBuff, nil, eDSLocalHostedNodes, &nodeCount, &context );
			if (status == eDSBufferTooSmall) {
				UInt32 newSize = tDataBuff->fBufferSize * 2;
				dsDataBufferDeAllocate(dsRef, tDataBuff);
				tDataBuff = dsDataBufferAllocate(dsRef, newSize);
			}
		} while (status == eDSBufferTooSmall);
		
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
			
			do {
				status = dsFindDirNodes( dsRef, tDataBuff, nodeName, eDSiExact, &nodeCount, &context );
				if (status == eDSBufferTooSmall) {
					UInt32 newSize = tDataBuff->fBufferSize * 2;
					dsDataBufferDeAllocate(dsRef, tDataBuff);
					tDataBuff = dsDataBufferAllocate(dsRef, newSize);
				}
			} while (status == eDSBufferTooSmall);
			
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

