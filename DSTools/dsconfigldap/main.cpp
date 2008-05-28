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
 * @header dsconfigldap
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Authorization.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <curses.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <termios.h>
#include <pwd.h>

#include "dscommon.h"
#include "dstools_version.h"

#warning VERIFY the version string before each major OS build submission that changes the dsconfigldap tool
const char *version = "10.5.3";	// Matches OS version
	
#pragma mark Prototypes

void usage( void );

int AddServer( char *inServerName, char *inConfigName, char *inComputerID, char *inUsername, char *inPassword, bool inSecureAuthOnly, bool bManInMiddle, bool bEncryptionOnly, bool bSignPackets, bool inForceBind, bool inSSL, bool inVerbose );
int RemoveServer( char *inServerName, char *inUsername, char *inPassword, bool inForceUnbind, bool inVerbose );

int GetSecurityLevel( CFDictionaryRef inDict );
int sendConfig( CFDictionaryRef configDict, CFMutableDictionaryRef *recvDict, int customCode );

bool preflightAuthorization( void );
bool doAuthorization( char *inUsername, char *inPassword );

#pragma mark Globals
#pragma mark -

//These 21 following defines are found in the DirectoryService source file CLDAPv3Configs.h and must match
#define kXMLServerKey				"Server"
#define kXMLIsSSLFlagKey			"SSL"
#define kXMLUserDefinedNameKey		"UI Name"
#define kXMLServerAccountKey		"Server Account"
#define kXMLServerPasswordKey		"Server Password"

// New Directory Binding functionality --------------
//

// kXMLBoundDirectoryKey => indicates the computer is bound to this directory.
//         This prevents them from changing:  server account, password, 
//         secure use, and port number.  It also means the config cannot  
//         be deleted, without unbinding.
#define kXMLBoundDirectoryKey			"Bound Directory"

// macosxodpolicy config Record flags
//
// These new flags are for determining config-record settings..
#define kXMLDirectoryBindingKey			"Directory Binding"

// Dictionary of keys
#define kXMLConfiguredSecurityKey		"Configured Security Level"
#define kXMLSupportedSecurityKey		"Supported Security Level"
#define kXMLLocalSecurityKey			"Local Security Level"

// Keys for above Dictionaries
#define kXMLSecurityBindingRequired		"Binding Required"
#define kXMLSecurityNoClearTextAuths	"No ClearText Authentications"
#define kXMLSecurityManInTheMiddle		"Man In The Middle"
#define kXMLSecurityPacketSigning		"Packet Signing"
#define kXMLSecurityPacketEncryption	"Packet Encryption"

// Corresponding bit flags for quick checks..
#define kSecNoSecurity				0
#define kSecCompBindingBit			1
#define kSecNoClearTextAuthBit		2
#define kSecManInMiddleBit			4
#define kSecPacketSignBit			8
#define kSecEncryptionBit			16

#define kSecurityMask				(kSecNoClearTextAuthBit | kSecManInMiddleBit | kSecPacketSignBit | kSecEncryptionBit)

//
// m-lo: YBStatusCodes
// these are our new exit codes that preserve the DS error resolution
// bsd/posix exit codes are restricted to the domain 0x00-0x7f
// darwin defines error exit codes in the range 0x00 - ELAST (currently 0x67)
// we'll begin our exit codes after darwin's to avoid collisions
// older versions only returned exit codes within the traditional posix domain (0x00 - 0x3f)
//

// these need a real home
typedef enum YBStatusCode
{
	ybStatusOK						= 0x00,
	ybStatusErrorCodeBegin			= ELAST + 0x07, // (err <= ybStatusErrorCodeBegin) != YBErrorCode
	ybStatusInvalidArgErr			= ELAST + 0x08,
	ybStatusAddRemoveErr			= ELAST + 0x09,
	ybStatusAuthorizationErr		= ELAST + 0x0a,
	ybNoAnonymousQueriesErr			= ELAST + 0x0b,
	ybUnbindNeedsCredentialsErr		= ELAST + 0x0c,
	ybInvalidCredentialsErr			= ELAST + 0x0d,
	ybPermissionErrorErr			= ELAST + 0x0e,
	ybDuplicateComputerNameErr		= ELAST + 0x0f,
	ybInsufficientServerSecurityErr	= ELAST + 0x10,
	ybMissingComputerNameErr		= ELAST + 0x11,
	ybNoComputeMappingAvailableErr	= ELAST + 0x12,
	ybCannotContactServerErr		= ELAST + 0x13,
	ybCannotUseSuppliedMappingsErr	= ELAST + 0x14,
	ybDSErrorHideErr				= ELAST + 0x15,
	ybStatusErrorCodeEnd			= ELAST + 0x16, // (err >= ybStatusErrorCodeBegin) != YBErrorCode
	ybRequestingCredentialsErr		= ELAST + 0x17  // client error -- conveniently add here for switch statements
}YBStatusCode;

#define YBErrorCode(statusCode)	(!(statusCode) || (((statusCode) > ybStatusErrorCodeBegin) && ((statusCode) < ybStatusErrorCodeEnd)))
#define _EXIT_VALUE_ (bYBErrors ? ybStatus : status)

//
// End New Directory Binding functionality ---------------

static AuthorizationRef		gAuthRef				= NULL;
static char					*kDirConfigAuthRight	= "system.services.directory.configure";
static char					*kNetConfigAuthRight	= "system.preferences";

#pragma mark -
#pragma mark Functions

// m-lo: library notes
// don't build main for dylib/lib usage
// the only symbols we need exported from here are AddServer and RemoveServer
// the static gAuthRef may create problems for general library usage

#ifndef _DYLIB_COMPATIBLE_

int main(int argc, char *argv[])
{
    int				ch;
	bool			bAddServer		= false;
	bool			bRemoveServer   = false;
	bool			bForceBinding	= false;
	bool			bInteractivePwd = false;
	bool			bSSL			= false;
	bool			bYBErrors		= false;
	bool			bVerbose		= false;
	bool			bSecureAuthOnly = false;
	bool			bDefaultUser	= false;
	bool			bManInMiddle	= false;
	bool			bEncryptionOnly	= false;
	bool			bSignPackets	= false;
	char		   *serverName		= nil;
	char		   *serverNameDupe	= nil;
	char		   *configName		= nil;
	char		   *computerID		= "";
	char		   *userName		= nil;
	char		   *userPassword	= nil;
	char		   *localName		= nil;
	char		   *localPassword   = nil;
	int				status			= 0;
	int				ybStatus		= ybStatusOK;
	
	if (argc < 2)
	{
		usage();
		
		status = EINVAL; ybStatus = ybStatusInvalidArgErr;
		
		exit(_EXIT_VALUE_);
	}
	
	if ( strcmp(argv[1], "-appleversion") == 0 )
        dsToolAppleVersionExit( argv[0] );
	
    while ((ch = getopt(argc, argv, "fvisxgeyma:r:n:c:u:p:l:q:h")) != -1)
	{
        switch (ch)
		{
        case 'f':
            bForceBinding = true;
            break;
        case 'v':
            bVerbose = true;
            break;
        case 'i':
            bInteractivePwd = true;
            break;
		case 's':
			bSecureAuthOnly = true;
			break;
		case 'm':
			bManInMiddle = true;
			break;
		case 'g':
			bSignPackets = true;
			break;
		case 'e':
			bEncryptionOnly = true;
			break;
		case 'x':
			bSSL = true;
			break;
		case 'y':
			bYBErrors = true;
			break;
		case 'a':
			bAddServer = true;
			if (serverName != nil)
			{
				serverNameDupe = serverName;
			}
            serverName = strdup(optarg);
            break;
        case 'r':
			bRemoveServer = true;
			if (serverName != nil)
			{
				serverNameDupe = serverName;
			}
            serverName = strdup(optarg);
            break;
        case 'n':
            configName = strdup(optarg);
            break;
        case 'c':
            computerID = strdup(optarg);
            break;
        case 'u':
            userName = strdup(optarg);
            break;
        case 'p':
            userPassword = strdup(optarg);
            break;
        case 'l':
            localName = strdup(optarg);
            break;
        case 'q':
            localPassword = strdup(optarg);
            break;
        case 'h':
        default:
			usage();
			status = EINVAL; 
			ybStatus = ybStatusInvalidArgErr;
			exit(_EXIT_VALUE_);
        }
    }
	
	char *envStr = nil;
	envStr = getenv("USER");
	if ( (localName == nil) && (envStr != nil) )
	{
		bDefaultUser = true;
		localName = strdup(envStr);
	}
	envStr = nil;
	envStr = getenv("HOST");
	if ( bAddServer && (computerID == nil) && (envStr != nil) )
	{
		char *dotLocation = nil;
		computerID = strdup(envStr);
		//do not use anything past first dot ie. first token in fully qualified domain name
		if( (dotLocation = strcasestr( computerID, "." )) != NULL )
		{
			*dotLocation = '\0';
		}
	}
	envStr = nil;

	if (bVerbose)
	{
		fprintf( stdout,"dsconfigldap verbose mode\n");
		fprintf( stdout,"Options selected by user:\n");
		if (bForceBinding)
			fprintf( stdout,"Force authenticated (un)binding option selected\n");
		if (bInteractivePwd)
			fprintf( stdout,"Interactive password option selected\n");
		if (bSecureAuthOnly)
			fprintf( stdout,"Enforce Secure Authentication is enabled\n");
		if (bSSL)
			fprintf( stdout,"SSL was chosen\n");
		if (bAddServer)
			fprintf( stdout,"Add server option selected\n");
		if (bRemoveServer)
			fprintf( stdout,"Remove server option selected\n");
		if (serverName)
			fprintf( stdout,"Server name provided as <%s>\n", serverName);
		if (configName)
			fprintf( stdout,"LDAP Configuration name provided as <%s>\n", configName);
		if (computerID)
			fprintf( stdout,"Computer ID provided as <%s>\n", computerID);
		if (userName)
			fprintf( stdout,"Network username provided as <%s>\n", userName);
		if ( userPassword && !bInteractivePwd )
			fprintf( stdout,"Network user password provided as <%s>\n", userPassword);
		if (localName && !bDefaultUser)
			fprintf( stdout,"Local username provided as <%s>\n", localName);
		else if (localName)
			fprintf( stdout,"Local username determined to be <%s>\n", localName);
		else
			fprintf( stdout,"No Local username determined\n");
		if ( localPassword && !bInteractivePwd )
			fprintf( stdout,"Local user password provided as <%s>\n", localPassword);
		if( bManInMiddle )
			fprintf( stdout, "Enforce man-in-the-middle only policy if server supports it.\n" );
		if( bEncryptionOnly )
			fprintf( stdout, "Enforce packet encryption policy if server supports it.\n" );
		if( bSignPackets )
			fprintf( stdout, "Enforce packet signing policy if server supports it.\n" );
		fprintf( stdout,"\n");
	}
	
	if (bAddServer && bRemoveServer)
	{
		fprintf( stdout,"Can't add and remove at the same time.\n");
		if ( (serverName != nil) && (serverNameDupe != nil) )
			fprintf( stdout,"Two server names were given <%s> and <%s>.\n", serverName, serverNameDupe);
		usage();
		ybStatus = ybStatusAddRemoveErr;
		status = EINVAL;
	}
	else
	{
		if ( userName != nil && (userPassword == nil || bInteractivePwd) )
		{
			userPassword = read_passphrase("Please enter network user password: ", 1);
			//do not verbose output this password value
		}
		
		// we were asked to prompt for password or we had a user provided but no password
		if ( bInteractivePwd || (bDefaultUser == false && localName != nil && localPassword == nil) )
		{
			localPassword = read_passphrase("Please enter local user password: ", 1);
			//do not verbose output this password value
		}
		
		do
		{
			if( preflightAuthorization() == false )
			{
				if( doAuthorization(localName, localPassword) == false )
				{
					fprintf( stdout, "Unable to obtain auth rights to update DirectoryService LDAP configuration.\n" );
					bAddServer		= false;
					bRemoveServer   = false;

					ybStatus = ybStatusAuthorizationErr;
					status = EACCES;
					exit( _EXIT_VALUE_ );
				}
			}
			
			if( bAddServer )
			{
				status = AddServer( serverName, configName, computerID, userName, userPassword, bSecureAuthOnly, bManInMiddle, bEncryptionOnly, bSignPackets, bForceBinding, bSSL, bVerbose );
			}

			if( bRemoveServer )
			{
				status = RemoveServer( serverName, userName, userPassword, bForceBinding, bVerbose );
			}
			
			break; // always leave the do-while
		} while(true);
		
		/* using errno.h for exit codes, we don't exceed 64, this is intentional */
		/* to follow other tool usage */
		switch ( status )
		{
			case eDSNoErr:
				break;
				
			case eDSBogusServer:
				fprintf( stderr, "Could not contact a server at that address.\n" );
				status = ENOENT;
				ybStatus = ybCannotContactServerErr;
				break;
				
			case eDSInvalidNativeMapping:
				fprintf( stderr, "Could not use mappings supplied to query directory.\n" );
				status = EINVAL;
				ybStatus = ybCannotUseSuppliedMappingsErr;
				break;
				
			case eDSNoStdMappingAvailable:
				fprintf( stderr, "No Computer Mapping available for binding to the directory.\n" );
				status = EINVAL;
				ybStatus = ybNoComputeMappingAvailableErr;
				break;
				
			case eDSInvalidRecordName:
				fprintf( stderr, "Missing a valid name for the Computer.\n" );
				status = EINVAL;
				ybStatus = ybMissingComputerNameErr;
				break;
				
			case eDSAuthMethodNotSupported:
				fprintf( stderr, "Server does not meet security requirements.\n" );
				status = EINVAL;
				ybStatus = ybInsufficientServerSecurityErr;
				break;
				
			case eDSRecordAlreadyExists:
				fprintf( stderr, "Computer with the name %s already exists\n", computerID );
				status = EEXIST;
				ybStatus = ybDuplicateComputerNameErr;
				break;
				
			case eDSPermissionError:
				fprintf( stderr, "Permission error\n" );
				status = EACCES;
				ybStatus = ybPermissionErrorErr;
				break;
				
			case eDSAuthFailed:
				fprintf( stderr, "Invalid credentials supplied %s server\n", (bAddServer ? "for binding to the" : "to remove the bound") );
				status = EINVAL;
				ybStatus = ybInvalidCredentialsErr;
				break;
				
			case eDSAuthParameterError:
				if ( bRemoveServer )
				{
					fprintf( stderr, "Bound need credentials to unbind\n" );
					ybStatus = ybUnbindNeedsCredentialsErr;
				}else
				{
					fprintf( stderr, "Directory is not allowing anonymous queries\n" );
					ybStatus = ybNoAnonymousQueriesErr;
				}
				status = EINVAL;
				break;
				
			default:
				fprintf( stderr, "Error %d from DirectoryService\n", status );
				status = EINVAL;
				ybStatus = ybDSErrorHideErr;
				break;
		}
		
	}//add or remove server correctly selected

	//cleanup variables
	//not really needed since we exit
	if (serverName)
		free(serverName);
	if (serverNameDupe)
		free(serverNameDupe);
	if (configName)
		free(configName);
	if (computerID)
		//free(computerID);
	if (userName)
		free(userName);
	if (userPassword)
		free(userPassword);
	if (localName)
		free(localName);
	if (localPassword)
		free(localPassword);

	exit(_EXIT_VALUE_);
}

#endif //_DYLIB_COMPATIBLE_

void usage( void )
{
	fprintf( stdout,
			 "dsconfigldap:: Add or remove LDAP server configurations in Directory Services\n"
			 "Version %s\n"
			 "Usage: dsconfigldap -h\n"
			 "Usage: dsconfigldap [-fvixsgem] -a servername [-n configname] [-c computerid]\n"
			 "                    [-u username] [-p userpassword] [-l localusername]\n"
			 "                    [-q localuserpassword]\n"
			 "Usage: dsconfigldap [-fvi] -r servername [-u username] [-p password]\n"
			 "                    [-l localusername] [-q localuserpassword]\n"
			 "  -f                 force authenticated bind/unbind\n"
			 "  -v                 log details\n"
			 "  -i                 interactive password entry\n"
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
			 "  -q password        password of a local administrator\n\n"
			 , version );
}

#pragma mark -
#pragma mark Support Routines

int AddServer( char *inServerName, char *inConfigName, char *inComputerID, char *inUsername, char *inPassword, bool inSecureAuthOnly, bool bManInMiddle, bool bEncryptionOnly, bool bSignPackets, bool inForceBind, bool inSSL, bool inVerbose )
{
	CFStringRef				cfServerName	= CFStringCreateWithCString( kCFAllocatorDefault, inServerName, kCFStringEncodingUTF8 );
	CFMutableDictionaryRef  xmlConfig		= CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	CFMutableDictionaryRef  cfResponse		= NULL;
	int						iError			= eDSNoErr;
	
	if (inVerbose) fprintf( stdout, "Step 1 - Server Information Discovery\n" );
	
	CFDictionarySetValue( xmlConfig, CFSTR(kXMLServerKey), cfServerName );
	
	CFBooleanRef cfBool = kCFBooleanFalse;
	if (inSSL)
	{
		cfBool = kCFBooleanTrue;
		CFDictionarySetValue(xmlConfig, CFSTR( kXMLIsSSLFlagKey ), cfBool);						
	}
	
	iError = sendConfig( xmlConfig, &cfResponse, 201 );
	
	if( iError == eDSRecordAlreadyExists )
	{
		if (inVerbose) fprintf( stdout, "   Status:  Failed - Server already in configuration.\n\n" );
	}
	else if( iError == eDSBogusServer )
	{   
		// if we get bogusServer, it was not an LDAP Server
		if (inVerbose) fprintf( stdout, "   Status:  Failed - Server did not Respond.\n\n" );
	}
	else if( iError == eDSNoErr )
	{
		if (inVerbose) fprintf( stdout, "   Status:  Success - Server Responded.\n\n" );
		
		// Let's rotate our answer back to the sender...
		CFRelease( xmlConfig );
		xmlConfig = cfResponse;
		cfResponse = NULL;
		
		if (inVerbose) fprintf( stdout, "Step 2 - Validating Record/Attribute Mapping\n" );
		iError = sendConfig( xmlConfig, &cfResponse, 202 );
		
		if( iError == eDSInvalidNativeMapping )
		{
			if (inVerbose) fprintf( stdout, "   Status:  Failed - Invalid Record/Attribute Mappings\n\n" );
		}
		// well, we can continue to 203 next
		else if( iError == eDSNoErr )
		{
			if (inVerbose) fprintf( stdout, "   Status:  Success - Valid Record/Attribute Mapping\n\n" );
			
			CFRelease( xmlConfig );
			xmlConfig = cfResponse;
			cfResponse = NULL;
			
			if (inVerbose) fprintf( stdout, "Step 3 - Detecting Required Security Levels and Binding requirements\n" );
			iError = sendConfig( xmlConfig, &cfResponse, 203 );
			
			if( iError == eDSNoErr )
			{
				if (inVerbose) fprintf( stdout, "   Status:  Success\n\n" );
				
				if( cfResponse )
				{
					CFMutableDictionaryRef  cfConfiguredSec = NULL;
					CFMutableDictionaryRef	cfLocalSecurity = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
					
					CFDictionarySetValue( cfResponse, CFSTR(kXMLLocalSecurityKey), cfLocalSecurity );
					
					CFRelease( cfLocalSecurity );
					
					cfConfiguredSec = (CFMutableDictionaryRef) CFDictionaryGetValue(cfResponse, CFSTR("Configured Security Level"));
					
					int iConfigSecurity = GetSecurityLevel( cfConfiguredSec );
					int iSecurityLevel  = GetSecurityLevel( (CFDictionaryRef) CFDictionaryGetValue(cfResponse, CFSTR("Supported Security Level")) );
					
					if( (iSecurityLevel & kSecNoClearTextAuthBit) != 0 )
					{
						if (inVerbose) fprintf( stdout, "   WARNING:  No Security Levels configured by Administrator!\n\n" );
						if (inVerbose) fprintf( stdout, "      Your LDAP server supports Secure authentication.\n\n" );
						
						if ( inSecureAuthOnly )
						{
							if (inVerbose) fprintf( stdout, "   Enforcing no cleartext password security policy.\n\n" );
							CFDictionarySetValue( cfLocalSecurity, CFSTR(kXMLSecurityNoClearTextAuths), kCFBooleanTrue );
						}
					}
					else if( inSecureAuthOnly )
					{
						fprintf( stdout, "   Unable to enforce no cleartext password security policy!\n" );
					}					
					
					if( ((iConfigSecurity & iSecurityLevel) & kSecurityMask) != (iConfigSecurity & kSecurityMask) )
					{
						if (inVerbose) fprintf( stdout, "The LDAP server you selected does not comply with Configured Security Policy.\n\n" );
						if (inVerbose) fprintf( stdout, "Add new Server cancelled\n" );
					}
					else
					{
						CFBooleanRef cfBinding = (CFBooleanRef) CFDictionaryGetValue( cfResponse, CFSTR("Directory Binding") );
						bool bBinding   = false;
						if( cfBinding )
						{
							bBinding = CFBooleanGetValue( cfBinding );
						}
						
						if( bBinding )
						{
							bool	bDoBind = false;
							
							if( (iConfigSecurity & kSecCompBindingBit) == 0 ) 
							{
								if (inVerbose) fprintf( stdout, "   Directory Binding is ENABLED but OPTIONAL.\n\n" );
								bDoBind = inForceBind;
								if (inForceBind && inVerbose)
								{
									fprintf( stdout, "   Directory Binding is being Forced as requested.\n" );
								}
							}
							else
							{
								if (inVerbose) fprintf( stdout, "   Directory Binding is ENABLED and REQUIRED.\n\n" );
                                
								bDoBind = true;
							}
							
							if( bDoBind && inUsername && inPassword )
							{
								CFRelease( xmlConfig );
								xmlConfig = cfResponse;
								cfResponse = NULL;
								
								CFStringRef cfComputerName = CFStringCreateWithCString( kCFAllocatorDefault, inComputerID, kCFStringEncodingUTF8 );
								CFDictionarySetValue( xmlConfig, CFSTR(kXMLUserDefinedNameKey), cfComputerName );
								CFRelease( cfComputerName );								
								
								CFStringRef cfUsername = CFStringCreateWithCString( kCFAllocatorDefault, inUsername, kCFStringEncodingUTF8 );
								CFStringRef cfPassword = CFStringCreateWithCString( kCFAllocatorDefault, inPassword, kCFStringEncodingUTF8 );
								
								CFDictionarySetValue( xmlConfig, CFSTR(kXMLServerAccountKey), cfUsername );
								CFDictionarySetValue( xmlConfig, CFSTR(kXMLServerPasswordKey), cfPassword );
								
								CFRelease( cfUsername );
								CFRelease( cfPassword );
								
								if (inVerbose) fprintf( stdout, "Step 4 - Attempting to bind computer as %s\n", inComputerID );
								
								iError = sendConfig( xmlConfig, &cfResponse, 204 );
								
								if( iError == eDSNoErr )
								{
									if (inVerbose) fprintf( stdout, "   Status:  Success.\n\n" );
								}
								else if( iError == eDSAuthFailed )
								{
									if (inVerbose) fprintf( stdout, "   Status:  Failed - Invalid credentials.\n\n" );
								}
								else if( iError == eDSRecordAlreadyExists )
								{
									if (inVerbose) fprintf( stdout, "   Status:  Failed - Computer Already Exists.\n\n" );
									
									if( inForceBind )
									{
										if (inVerbose) fprintf( stdout, "      WARNING:  If this record is in use, it will disable the other computer!\n\n" );
										iError = sendConfig( xmlConfig, &cfResponse, 205 ); //Join code call
										if( iError == eDSNoErr )
										{
											if (inVerbose) fprintf( stdout, "   Status:  Success.\n\n" );
										}
                                        else if ( iError == eDSEmptyRecordName )
                                        {
                                              if (inVerbose) fprintf( stdout, "      Status: Failed to bind because record name is empty. (computerID=\"%s\"). Make sure that the HOST environment variable is set or that you specified a computer ID with the -c option.\n", inComputerID);
                                        }
										else
										{
											if (inVerbose) fprintf( stdout, "   Status:  Failed to overwrite.\n\n" );
										}
									}
								}
								else
								{
									if (inVerbose) fprintf( stdout, "   Status:  Failed Error Code %d\n\n", iError );
								}
							}
                            else if( bDoBind ) // bind but no username/password, no need to check, was checked above
                            {
                                fprintf( stderr, "   Status:  Failed, no network credentials supplied\n\n" );
                                iError = eDSAuthFailed;
                            }
						}
						else
						{
							if (inVerbose) fprintf( stdout, "Step 4 - Directory Binding\n" );
							if (inVerbose) fprintf( stdout, "   Directory binding is not supported.\n\n" );
						}
						
						if( iError == eDSNoErr )
						{
							char	*pCurrentName = NULL;
							
							CFRelease( xmlConfig );
							xmlConfig = cfResponse;
							cfResponse = NULL;
							
							if (inVerbose) fprintf( stdout, "Step 5 - Adding server to configuration\n" );
							
							cfLocalSecurity = (CFMutableDictionaryRef) CFDictionaryGetValue( xmlConfig, CFSTR(kXMLLocalSecurityKey) );

							if( bManInMiddle && (iSecurityLevel & kSecManInMiddleBit) == kSecManInMiddleBit )
							{
								if (inVerbose) fprintf( stdout, "   Enforcing man-in-the-middle security policy!\n" );
								CFDictionarySetValue( cfLocalSecurity, CFSTR(kXMLSecurityManInTheMiddle), kCFBooleanTrue );
							}
							else if( bManInMiddle )
							{
								fprintf( stdout, "   Unable to enforce Man-in-the-middle security policy!\n" );
								CFDictionarySetValue( cfLocalSecurity, CFSTR(kXMLSecurityManInTheMiddle), kCFBooleanFalse );
							}
							
							if( bEncryptionOnly && (iSecurityLevel & kSecEncryptionBit) == kSecEncryptionBit )
							{
								if (inVerbose) fprintf( stdout, "   Enforcing encryption security policy!\n" );
								CFDictionarySetValue( cfLocalSecurity, CFSTR(kXMLSecurityPacketEncryption), kCFBooleanTrue );
							}
							else if( bEncryptionOnly )
							{
								fprintf( stdout, "   Unable to enforce encryption security policy!\n" );
								CFDictionarySetValue( cfLocalSecurity, CFSTR(kXMLSecurityPacketEncryption), kCFBooleanFalse );
							}
							
							if( bSignPackets && (iSecurityLevel & kSecPacketSignBit) == kSecPacketSignBit )
							{
								if (inVerbose) fprintf( stdout, "   Enforcing packet signing security policy!\n" );
								CFDictionarySetValue( cfLocalSecurity, CFSTR(kXMLSecurityPacketSigning), kCFBooleanTrue );
							}
							else if( bSignPackets )
							{
								fprintf( stdout, "   Unable to enforce packet signing security policy!\n" );
								CFDictionarySetValue( cfLocalSecurity, CFSTR(kXMLSecurityPacketSigning), kCFBooleanFalse );
							}
									
							CFStringRef cfComputerName = (CFStringRef) CFDictionaryGetValue( xmlConfig, CFSTR(kXMLUserDefinedNameKey) );
							
							if( cfComputerName )
							{
								CFIndex iLength = CFStringGetMaximumSizeForEncoding( CFStringGetLength(cfComputerName), kCFStringEncodingUTF8 ) + 1;
								pCurrentName = (char *) calloc( sizeof(char), iLength );
								CFStringGetCString( cfComputerName, pCurrentName, iLength, kCFStringEncodingUTF8 );
							}
							
							if( inConfigName == NULL )
							{
								cfComputerName = CFStringCreateWithCString( kCFAllocatorDefault, pCurrentName, kCFStringEncodingUTF8 );
							}
							else
							{
								cfComputerName = CFStringCreateWithCString( kCFAllocatorDefault, inConfigName, kCFStringEncodingUTF8 );
							}
							
							CFDictionarySetValue( xmlConfig, CFSTR(kXMLUserDefinedNameKey), cfComputerName );
							CFRelease( cfComputerName );
							
							if (pCurrentName != nil)
							{
								free(pCurrentName);
							}
							
							iError = sendConfig( xmlConfig, &cfResponse, 206 );
							if( iError == eDSNoErr )
							{
								if (inVerbose) fprintf( stdout, "   Status:  Success.\n\n" );
							}
							else
							{
								if (inVerbose) fprintf( stdout, "   Status:  Failure.\n\n" );
							}
						}
					}
				}
			}
		}
	}
	
	return iError;
}

int RemoveServer( char *inServerName, char *inUsername, char *inPassword, bool inForceUnbind, bool inVerbose )
{
	CFStringRef				cfServerName	= CFStringCreateWithCString( kCFAllocatorDefault, inServerName, kCFStringEncodingUTF8 );
	CFMutableDictionaryRef  cfResponse		= NULL;
	int						iCode			= 207;
	int						iError			= eDSNoErr;
	CFMutableDictionaryRef  xmlConfig		= CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	
	if (inVerbose) fprintf( stdout, "Attempting to remove server from configuration\n" );
	
	CFDictionarySetValue( xmlConfig, CFSTR(kXMLServerKey), cfServerName );
	
	do
	{
		iError = sendConfig( xmlConfig, &cfResponse, iCode );
		
		// if we get bogusServer, it was not an LDAP Server
		if( iError == eDSBogusServer )
		{
			if (inVerbose) fprintf( stdout, "   Status:  Failed - Server does not exist in configuration.\n\n" );
			iError = eDSNoErr;
		}
		else if( iError == eDSOpenNodeFailed )
		{
			if (inVerbose) fprintf( stdout, "   Status:  Failed - Could not contact the LDAP server to unbind.\n\n" );
			if( inForceUnbind )
			{
				if (inVerbose) fprintf( stdout, "   Status:  Forcing unbind from server.\n\n" );
				iCode = 208;
			}
			else
			{
				iError = eDSNoErr;
			}
		}
		else if( iError == eDSRecordNotFound )
		{
			if (inVerbose) fprintf( stdout, "   Status:  Computer Record missing from Directory.  Force unbinding.\n\n" );
			iCode = 208;
		}
		else if( iError == eDSNoErr )
		{
			if ( (iCode == 207) || (iCode == 208) )
			{
				if (inVerbose) fprintf( stdout, "   Status:  Success - Unbound now if authenticated directory binding was in place.\n\n" );
				iError = eDSContinue;
				iCode = 209;
			}
			else if (iCode == 209)
			{
				if (inVerbose) fprintf( stdout, "   Status:  Success - Server removed from configuration.\n\n" );
			}
		}
		else if( iError == eDSPermissionError )
		{
			// we don't have privilege to unbind from the server, give user ability to force unbind?
			// Let's rotate our answer back to the sender...
			if (inVerbose) fprintf( stdout, "You do not have the privilege to unbind from this server.  Contact your network administrator.\n" );
			iError = eDSNoErr;
		}
		else if( iError == eDSAuthParameterError )
		{
            if( inUsername && inPassword )
            {
                if (inVerbose) fprintf( stdout, "   Status:  Computer is bound to the Directory\n" );

                CFStringRef cfUsername = CFStringCreateWithCString( kCFAllocatorDefault, inUsername, kCFStringEncodingUTF8 );
                CFStringRef cfPassword = CFStringCreateWithCString( kCFAllocatorDefault, inPassword, kCFStringEncodingUTF8 );
                
                CFDictionarySetValue( xmlConfig, CFSTR(kXMLServerAccountKey), cfUsername );
                CFDictionarySetValue( xmlConfig, CFSTR(kXMLServerPasswordKey), cfPassword );
                
                CFRelease( cfUsername );
                CFRelease( cfPassword );
                
                if (inVerbose) fprintf( stdout, "      Attempting to unbind from server with credentials\n" );
                
                iError = sendConfig( xmlConfig, &cfResponse, iCode );
                
                if( iError == eDSNoErr )
                {
                    if (inVerbose) fprintf( stdout, "   Status:  Success.\n\n" );
                    iError = eDSContinue; // set to continue so we remove the config too
                    iCode = 209;
                }
                else if( iError == eDSAuthFailed )
                {
                    if (inVerbose) fprintf( stdout, "   Status:  Failed - Invalid credentials.\n\n" );
                    iError = eDSNoErr;
                }
            }
            else
            {
                fprintf( stderr, "   Status:  Failed - Computer is bound, network credentials not supplied\n\n" );
                break;
            }
		}
	} while (iError != eDSNoErr);
	
	return iError;
}

#pragma mark -
#pragma mark Utility Routines

int GetSecurityLevel( CFDictionaryRef inDict )
{
	CFBooleanRef	cfBoolean;
	int				siSecurityLevel = kSecNoSecurity;
	
	if( inDict != NULL ) {
		if( CFDictionaryGetValueIfPresent(inDict, CFSTR(kXMLSecurityBindingRequired), (const void **) &cfBoolean) && CFBooleanGetValue(cfBoolean) ) {
			siSecurityLevel |= kSecCompBindingBit;
		}

		if( CFDictionaryGetValueIfPresent(inDict, CFSTR(kXMLSecurityNoClearTextAuths), (const void **) &cfBoolean) && CFBooleanGetValue(cfBoolean) ) {
			siSecurityLevel |= kSecNoClearTextAuthBit;
		}
		
		if( CFDictionaryGetValueIfPresent(inDict, CFSTR(kXMLSecurityManInTheMiddle), (const void **) &cfBoolean) && CFBooleanGetValue(cfBoolean) ) {
			siSecurityLevel |= kSecManInMiddleBit;
		}
		
		if( CFDictionaryGetValueIfPresent(inDict, CFSTR(kXMLSecurityPacketSigning), (const void **) &cfBoolean) && CFBooleanGetValue(cfBoolean) ) {
			siSecurityLevel |= kSecPacketSignBit;
		}
		
		if( CFDictionaryGetValueIfPresent(inDict, CFSTR(kXMLSecurityPacketEncryption), (const void **) &cfBoolean) && CFBooleanGetValue(cfBoolean) ) {
			siSecurityLevel |= kSecEncryptionBit;
		}
	}
	
	return siSecurityLevel;
}

int sendConfig( CFDictionaryRef configDict, CFMutableDictionaryRef *recvDict, int customCode )
{
	tDirNodeReference			nodeRef				= nil;
	tDirReference				dsRef				= nil;
	tDataList				   *dataList			= nil;
	tDataBuffer				   *sendBuff			= nil;
	tDataBuffer				   *recvBuff			= nil;
	CFDataRef					xmlData				= nil;
	int							status				= eDSNoErr;
	AuthorizationExternalForm	authRightsExtForm;
	
	try
	{
		if( geteuid() == 0 ) {
			memset( &authRightsExtForm, 0, sizeof(authRightsExtForm) );
		} else if( (status = AuthorizationMakeExternalForm(gAuthRef, &authRightsExtForm)) != noErr ) {
			throw( status );
		}
		
		if( (status = dsOpenDirService(&dsRef)) != eDSNoErr ) {
			throw( status );
		}
		
		recvBuff = dsDataBufferAllocate( dsRef, 128000 );
		dataList = dsDataListAllocate( dsRef );
		
		status = dsBuildListFromStringsAlloc( dsRef, dataList, "LDAPv3", nil );
		if (status != eDSNoErr) {
			throw( status );
		}
		
		status = dsOpenDirNode( dsRef, dataList, &nodeRef );
		if (status != eDSNoErr) {
			throw( status );
		}
		
		xmlData = (CFDataRef) CFPropertyListCreateXMLData( nil, configDict );
		
		CFIndex iLength = CFDataGetLength(xmlData);
		
		sendBuff = dsDataBufferAllocate( dsRef, sizeof(authRightsExtForm) + iLength + 1 );
		
		memcpy( sendBuff->fBufferData, &authRightsExtForm, sizeof(authRightsExtForm) );
		
		CFDataGetBytes( xmlData, CFRangeMake(0, iLength), (UInt8*)(sendBuff->fBufferData+sizeof(authRightsExtForm)) );
		
		sendBuff->fBufferLength = iLength + sizeof(authRightsExtForm);
		
		while( (status = dsDoPlugInCustomCall( nodeRef, customCode, sendBuff, recvBuff )) == eDSBufferTooSmall ) {
			int newLength = recvBuff->fBufferSize + 16384;	// let's add 16k at a time
			dsDataBufferDeAllocate( dsRef, recvBuff );
			recvBuff = dsDataBufferAllocate( dsRef, newLength );
		}
		
		if( recvBuff->fBufferLength ) {
			CFDataRef cfRecvData = CFDataCreate( kCFAllocatorDefault, (UInt8*)recvBuff->fBufferData, recvBuff->fBufferLength );
			CFMutableDictionaryRef cfDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfRecvData, kCFPropertyListMutableContainersAndLeaves, NULL);
			*recvDict = cfDict;
			CFRelease( cfRecvData );
		}
	}
	catch( int error )
	{
		status = error;
	}
	
	if( sendBuff ) {
		dsDataBufferDeAllocate( dsRef, sendBuff );
	}
	
	if( recvBuff ) {
		dsDataBufferDeAllocate( dsRef, recvBuff );
	}
	
	if( dataList ) {
		dsDataListDeallocate( dsRef, dataList );
	}
	
	if( nodeRef ) {
		dsCloseDirNode( nodeRef );
	}
	
	if( dsRef ) {
		dsCloseDirService( dsRef );
	}
	
	return status;
}

#pragma mark -
#pragma mark Security Authorization Support Calls

bool preflightAuthorization( void )
{
    AuthorizationRights		rights;
    AuthorizationFlags		flags;
	AuthorizationRights    *authorizedRights;
    OSStatus				err			= noErr;
	bool					authorized  = false;
	
	// if we are root we don't have to authorize
	if( geteuid() == 0 )
	{
		return true;
	}
	
	AuthorizationItem rightsItems[] = { {kDirConfigAuthRight, 0, 0, 0}, {kNetConfigAuthRight, 0, 0, 0} };
	int rightCount = sizeof(rightsItems) / sizeof(AuthorizationItem);
	
    if( gAuthRef == NULL )
	{
        rights.count	= 0;
        rights.items	= NULL;
		
        flags = kAuthorizationFlagDefaults;
		
        err = AuthorizationCreate( &rights, kAuthorizationEmptyEnvironment, flags, &gAuthRef );
    }
	
    rights.count	= rightCount;
    rights.items	= rightsItems;
	
    flags = kAuthorizationFlagExtendRights | kAuthorizationFlagPreAuthorize;
	
    err = AuthorizationCopyRights( gAuthRef, &rights, kAuthorizationEmptyEnvironment, flags, &authorizedRights );
	
    authorized = (err==errAuthorizationSuccess);
    if( authorized )
	{
        AuthorizationFreeItemSet( authorizedRights );
	} else
	{
		gAuthRef = NULL;
	}
    return authorized;
}

bool doAuthorization( char *inUsername, char *inPassword )
{
    AuthorizationRights		rights;
    AuthorizationRights    *authorizedRights;
    AuthorizationFlags		flags;
    OSStatus				err				= errAuthorizationDenied;
	bool					authorized		= false;
	AuthorizationItem		rightsItems[]   = { {kDirConfigAuthRight, 0, 0, 0}, {kNetConfigAuthRight, 0, 0, 0} };
	int						rightCount		= sizeof(rightsItems) / sizeof(AuthorizationItem);
	
	if ( (inUsername == nil) || (inPassword == nil) )
	{
		return (false);
	}

    if( gAuthRef == NULL )
	{
		rights.count= 0;
		rights.items = NULL;
		flags = kAuthorizationFlagDefaults;
		
		AuthorizationItem			params[]	= { {"username", strlen(inUsername), (void*)inUsername, 0},
													{"password", strlen(inPassword), (void*)inPassword, 0} };
		AuthorizationEnvironment	environment = { sizeof(params)/ sizeof(*params), params };
		
		err = AuthorizationCreate( &rights, kAuthorizationEmptyEnvironment, flags, &gAuthRef );
		
		if( err == noErr )
		{
			rights.count = rightCount;
			rights.items = rightsItems;
			
			flags = kAuthorizationFlagExtendRights | kAuthorizationFlagPreAuthorize;
			
			err = AuthorizationCopyRights( gAuthRef, &rights, &environment, flags, &authorizedRights );
		}
    }
	
    if( err == errAuthorizationSuccess )
	{
		authorized = true;
        AuthorizationFreeItemSet( authorizedRights );
	}
    return authorized;
}
