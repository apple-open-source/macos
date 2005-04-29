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
 * @header dsidentity
 */

#include <Security/Authorization.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>

#include "dscommon.h"
#include "dstools_version.h"

#warning VERIFY the version string before each distinct build submission that changes the dsidentity tool
const char *version = "1.2";
	
#pragma mark Prototypes

void usage( void );

bool preflightAuthorization( tDirReference inDSRef, tDirNodeReference inDSNodeRef );
bool doAuthorization( char *inUsername, char *inPassword );

void				AddIdentity				(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													char *inUserName,
													char *inPassword,
													char *inDisplayName,
													char *inPicture,
													char *inComment,
													bool inVerbose );
void				RemoveIdentity			(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													char *inUserName,
													bool inVerbose );

#pragma mark Globals
#pragma mark -

AuthorizationRef		gAuthRef					= NULL;
char					*kEditIdentityUserAuthRight	= "com.apple.allowidentityedit";

#pragma mark -
#pragma mark Functions

int main(int argc, char *argv[])
{
    int				ch;
	bool			bAdd			= false; //-a
	bool			bRemove			= false; //-r
	bool			bVerbose		= false; //-v
	bool			bInteractivePwd = false; //-i
	bool			bAuthRightOnly  = false; //-x
	bool			bAuthSuccess	= false;
	bool			bDefaultUser	= false;
	char		   *identityUser	= nil;
	char		   *identityUserDupe= nil;
	char		   *displayName		= nil; //-d
	char		   *picture			= nil; //-j
	char		   *comment			= nil; //-c
	char		   *userName		= nil; //-u
	char		   *userPassword	= nil; //-p
	char		   *identityPassword	= nil; //-s
	
	signed long			siResult		= eDSNoErr;
	tDirReference		aDSRef			= 0;
	tDirNodeReference   aDSNodeRef		= 0;
	tDataBufferPtr		dataBuff		= nil;
	unsigned long		nodeCount		= 0;
	tContextData		context			= nil;
	tDataListPtr		nodeName		= nil;

	if (argc < 2)
	{
		usage();
		exit(0);
	}
	
	if ( strcmp(argv[1], "-appleversion") == 0 )
        dsToolAppleVersionExit( argv[0] );
	
    while ((ch = getopt(argc, argv, "vixa:r:d:j:c:u:p:s:h")) != -1)
	{
        switch (ch)
		{
        case 'v':
            bVerbose = true;
            break;
        case 'i':
            bInteractivePwd = true;
            break;
		case 'x':
			bAuthRightOnly = true;
			break;
        case 'a':
			bAdd = true;
			if (identityUser != nil)
			{
				identityUserDupe = identityUser;
			}
            identityUser = strdup(optarg);
            break;
        case 'r':
			bRemove = true;
			if (identityUser != nil)
			{
				identityUserDupe = identityUser;
			}
            identityUser = strdup(optarg);
            break;
        case 'd':
            displayName = strdup(optarg);
            break;
        case 'j':
            picture = strdup(optarg);
            break;
        case 'c':
            comment = strdup(optarg);
            break;
        case 'u':
            userName = strdup(optarg);
            break;
        case 'p':
            userPassword = strdup(optarg);
            break;
        case 's':
            identityPassword = strdup(optarg);
            break;
        case 'h':
        default:
			usage();
			exit(0);
        }
    }
	
	if (userName == nil)
	{
		bDefaultUser = true;
		userName = strdup(getenv("USER"));
	}
	if (displayName == nil)
	{
		if (identityUser != nil)
		{
			displayName = strdup(identityUser);
		}
	}
	if (picture == nil)
	{
		//would like to use a specific default identity user picture
		//picture = strdup("/Library/User Pictures/Identity/Default.tif");
		picture = strdup("/Library/User Pictures/Animals/Dragonfly.tif");
	}
	if (comment == nil)
	{
		comment = strdup("Identity User");
	}

	if (bVerbose)
	{
		fprintf( stdout,"dsidentity verbose mode\n");
		fprintf( stdout,"Options selected by user:\n");
		if (bInteractivePwd)
			fprintf( stdout,"Interactive password option selected\n");
		if (bAuthRightOnly)
			fprintf( stdout,"Enforce Authentication with Auth Rights only\n");
		if (bAdd)
			fprintf( stdout,"Add identity user option selected\n");
		if (bRemove)
			fprintf( stdout,"Remove identity user option selected\n");
		if (identityUser)
			fprintf( stdout,"Identity user name provided as <%s>\n", identityUser);
		if (bAdd)
		{
			if (displayName)
				fprintf( stdout,"Display (Full) name provided as <%s>\n", displayName);
			if (picture)
				fprintf( stdout,"Picture location provided as <%s>\n", picture);
			if (comment)
				fprintf( stdout,"Comment provided as <%s>\n", comment);
		}
		if (userName && !bDefaultUser)
			fprintf( stdout,"Local username provided as <%s>\n", userName);
		else if (userName)
			fprintf( stdout,"Local username determined to be <%s>\n", userName);
		else
			fprintf( stdout,"No Local username determined\n");
		if ( userPassword && !bInteractivePwd )
			fprintf( stdout,"User password provided as <%s>\n", userPassword);
		if ( identityPassword && !bInteractivePwd )
			fprintf( stdout,"Identity password provided as <%s>\n", identityPassword);
		fprintf( stdout,"\n");
	}
	
	if (bAdd && bRemove)
	{
		fprintf( stdout,"Can't add and remove at the same time.\n");
		if ( (identityUser != nil) && (identityUserDupe != nil) )
			fprintf( stdout,"Two identity usernames were given <%s> and <%s>.\n", identityUser, identityUserDupe);
		usage();
	}
	else
	{
		if ( ( !bDefaultUser && ( (userPassword == nil) || bInteractivePwd ) ) || (bDefaultUser && bInteractivePwd) )
		{
			userPassword = read_passphrase("Please enter admin user password:", 1);
			//do not verbose output this password value
		}
		
		//will use a default password of empty string if none provided
		if ( bInteractivePwd )
		{
			identityPassword = read_passphrase("Please enter password to be assigned to identity user:", 1);
			//do not verbose output this password value
		}
		
		do
		{
			siResult = dsOpenDirService( &aDSRef );
			if ( siResult != eDSNoErr )
			{
				if (bVerbose)
				{
					fprintf( stdout, "dsOpenDirService failed with error <%ld>\n", siResult);
				}
				break;
			}

			dataBuff = dsDataBufferAllocate( aDSRef, 256 );
			if ( dataBuff == nil )
			{
				if (bVerbose) fprintf( stdout, "dsDataBufferAllocate returned NULL\n");
				break;
			}
			
			do
			{
				if (bVerbose) fprintf( stdout, "dsFindDirNodes using local node\n");
				siResult = dsFindDirNodes( aDSRef, dataBuff, NULL, eDSLocalNodeNames, &nodeCount, &context );
				if (siResult == eDSBufferTooSmall)
				{
					unsigned long bufSize = dataBuff->fBufferSize;
					dsDataBufferDeAllocate( aDSRef, dataBuff );
					dataBuff = nil;
					dataBuff = dsDataBufferAllocate( aDSRef, bufSize * 2 );
					if ( dataBuff == nil )
					{
						if (bVerbose) fprintf( stdout, "dsDataBufferAllocate returned NULL\n");
					}
				}
			} while ( (siResult == eDSBufferTooSmall) && (dataBuff != nil) );
			if ( siResult != eDSNoErr )
			{
				if (bVerbose) fprintf( stdout, "dsFindDirNodes returned the error <%ld>\n", siResult);
				break;
			}
			if ( nodeCount < 1 )
			{
				if (bVerbose) fprintf( stdout, "dsFindDirNodes could not find the node\n");
				break;
			}

			siResult = dsGetDirNodeName( aDSRef, dataBuff, 1, &nodeName );
			if ( siResult != eDSNoErr )
			{
				if (bVerbose) fprintf( stdout, "dsGetDirNodeName returned the error <%ld>\n", siResult);
				break;
			}

			siResult = dsOpenDirNode( aDSRef, nodeName, &aDSNodeRef );
			if ( siResult != eDSNoErr )
			{
				if (bVerbose) fprintf( stdout, "dsOpenDirNode returned the error <%ld>\n", siResult);
				break;
			}
			if ( nodeName != NULL )
			{
				dsDataListDeallocate( aDSRef, nodeName );
				free( nodeName );
				nodeName = NULL;
			}

			bAuthSuccess = true;
			if( preflightAuthorization(aDSRef, aDSNodeRef) == false ) //this handles user's uid check and admin group membership as well
			{
				if( doAuthorization(userName, userPassword) == false ) //this checks the correct auth right
				{
					fprintf( stdout, "Unable to obtain auth rights to edit a identity user record.\n" );
					bAuthSuccess = false;
				}
			}
			if ( !bAuthRightOnly && !bAuthSuccess && (userName != nil) && (userPassword != nil) )
			{
				//not likely to be used
				//DoDSLocalAuth(userName, userPassword); //may need to decide what possible auth success will also allow this setuid tool to work
			}
			
			if (bAuthSuccess)
			{
				if( bAdd )
				{
					AddIdentity( aDSRef, aDSNodeRef, identityUser, identityPassword, displayName, picture, comment, bVerbose );
				}
				else if( bRemove )
				{
					RemoveIdentity( aDSRef, aDSNodeRef, identityUser, bVerbose );
				}
			}
			break; // always leave the do-while
		} while(true);
		
	}//add or remove server correctly selected

	//cleanup variables
	//not really needed since we exit
	if (identityUser)
		free(identityUser);
	if (identityPassword)
		free(identityPassword);
	if (identityUserDupe)
		free(identityUserDupe);
	if (displayName)
		free(displayName);
	if (picture)
		free(picture);
	if (comment)
		free(comment);
	if (userName)
		free(userName);
	if (userPassword)
		free(userPassword);
		
	if ( dataBuff != nil )
	{
		dsDataBufferDeAllocate( aDSRef, dataBuff );
		dataBuff = nil;
	}
	if (aDSNodeRef != NULL)
		dsCloseDirNode(aDSNodeRef);
	if (aDSRef != NULL)
		dsCloseDirService(aDSRef);

	exit(0);
}

void usage( void )
{
	fprintf( stdout, 
			"Version %s\n"
			"Usage: dsidentity -h\n"
			"Usage: dsidentity [-vix] -a identity [-s password] [-d displayname] [-j picture]\n"
			"                  [-c comment] [-u username] [-p password]\n"
			"Usage: dsidentity [-vix] -r identity [-u username] [-p password]\n"
			"  -v                 log details\n"
			"  -i                 interactive password entry\n"
			"  -x                 use auth rights only\n"
			"  -h                 display usage statement\n"
			"  -a identity        add identity user\n"
			"  -r identity        remove identity user\n"
			"  -s password        password of the new identity user\n"
			"  -d displayname     display (full) name to give identity user\n"
			"  -j picture         file location of picture for identity user\n"
			"  -c comment         comment on identity user\n"
			"  -u username        username of a local administrator\n"
			"  -p password        password of a local administrator\n\n"
			, version);
}

#pragma mark -
#pragma mark Security Authorization Support Calls

bool preflightAuthorization( tDirReference inDSRef, tDirNodeReference inDSNodeRef )
{
    AuthorizationRights		rights;
    AuthorizationFlags		flags;
	AuthorizationRights    *authorizedRights;
    OSStatus				err			= noErr;
	bool					authorized  = false;
	int						userUID		= -2;
	
	// if we are root or admin user we don't have to authorize
	userUID = getuid();
	if (userUID == 0) //root user
	{
		return true;
	}
	
	char *envStr = nil;
	envStr = getenv("USER");
	//check for member of admin group
	if ( (envStr != nil) && UserIsMemberOfGroup( inDSRef, inDSNodeRef, envStr, "admin" ) )
	{
		return true;
	}
	
	AuthorizationItem rightsItems[] = { {kEditIdentityUserAuthRight, 0, 0, 0} };
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
	AuthorizationItem		rightsItems[]   = { {kEditIdentityUserAuthRight, 0, 0, 0} };
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

#pragma mark -
#pragma mark Directory Service API Use

void AddIdentity( tDirReference inDSRef, tDirNodeReference inDSNodeRef, char *inUserName, char *inPassword, char *inDisplayName, char *inPicture, char *inComment, bool inVerbose )
{
	signed long				siResult		= eDSNoErr;
	char			       *userRecordName  = nil;
	tRecordReference		aDSRecordRef	= 0;
	tDataNodePtr			authMethod		= nil;
	tDataBufferPtr			authBuff		= nil;
	tDataBufferPtr			dataBuff		= nil;
	unsigned long			length			= 0;
	char*					ptr				= nil;

	//check if already exists
	userRecordName = getSingleRecordAttribute(inDSRef, inDSNodeRef, inUserName, kDSStdRecordTypeUsers, kDSNAttrRecordName, &siResult, inVerbose);

	//if already exists then note this if verbose
	if ( (userRecordName != nil) && (siResult == eDSNoErr) )
	{
		if (inVerbose)
		{
			fprintf( stdout,"dsidentity: user <%s> already exists so add failed\n", userRecordName);
		}
		free(userRecordName);
		userRecordName = nil;
	} 
	else if ( (userRecordName == nil) && (siResult == eDSRecordNotFound) )
	{
		aDSRecordRef = createAndOpenRecord(inDSRef, inDSNodeRef, inUserName, kDSStdRecordTypeUsers, &siResult, inVerbose);
		if (siResult == eDSNoErr)
		{
			userRecordName = strdup(inUserName);
			//now modify the record properly
			
			// DisplayName
			addRecordParameter(inDSRef, inDSNodeRef, aDSRecordRef, kDS1AttrDistinguishedName, inDisplayName, inVerbose);
			if (inVerbose) fprintf(stdout, "Display name value of <%s> added to new identity user record\n", inDisplayName);

			// Picture
			addRecordParameter(inDSRef, inDSNodeRef, aDSRecordRef, kDS1AttrPicture, inPicture, inVerbose);
			if (inVerbose) fprintf(stdout, "Picture file location value of <%s> added to new identity user record\n", inPicture);

			// Comment
			addRecordParameter(inDSRef, inDSNodeRef, aDSRecordRef, kDS1AttrComment, inComment, inVerbose);
			if (inVerbose) fprintf(stdout, "Comment value of <%s> added to new identity user record\n", inComment);

			//uid - next available value after 501
			char *uidValue = createNewuid(inDSRef, inDSNodeRef, inVerbose);
			if (uidValue != nil)
			{
				addRecordParameter(inDSRef, inDSNodeRef, aDSRecordRef, kDS1AttrUniqueID, uidValue, inVerbose);
				if (inVerbose) fprintf(stdout, "Next free uid value <%s> was determined and used in new identity user record\n", uidValue);
				free(uidValue);
				uidValue = nil;
			}
			
			//GUID value
			char *guid = createNewGUID(inVerbose);
			if (guid != nil)
			{
				addRecordParameter(inDSRef, inDSNodeRef, aDSRecordRef, kDS1AttrGeneratedUID, guid, inVerbose);
				if (inVerbose) fprintf(stdout, "GUID value <%s> created and added to new identity user record\n", guid);
				free(guid);
				guid = nil;
			}
			
			// NFS home directory
			addRecordParameter(inDSRef, inDSNodeRef, aDSRecordRef, kDS1AttrNFSHomeDirectory, "/dev/null", inVerbose);
			if (inVerbose) fprintf(stdout, "NFS home directory value of </dev/null> added to new identity user record\n");

			// gid of -2 (nobody)
			addRecordParameter(inDSRef, inDSNodeRef, aDSRecordRef, kDS1AttrPrimaryGroupID, "-2", inVerbose);
			if (inVerbose) fprintf(stdout, "gid value of <-2> added to new identity user record\n");

			// UserShell of /dev/null
			addRecordParameter(inDSRef, inDSNodeRef, aDSRecordRef, kDS1AttrUserShell, "/dev/null", inVerbose);
			if (inVerbose) fprintf(stdout, "UserShell value of </dev/null> added to new identity user record\n");

			//setup the identity user's authentication with a shadowhash password
			//if inPassword is NULL then we use empty string
			//this is to be Kerberos in the future
			if (inUserName != nil) //obvious
			{
				dataBuff = dsDataBufferAllocate( inDSRef, 64 );
				authMethod	= dsDataNodeAllocateString( inDSRef, kDSStdAuthSetPasswdAsRoot );
				if (inPassword != nil)
				{
					authBuff	= dsDataBufferAllocate( inDSRef, strlen( inUserName ) + strlen( inPassword ) + 10 );
				}
				else
				{
					authBuff	= dsDataBufferAllocate( inDSRef, strlen( inUserName ) + 10 );
				}
				// 4 byte length + inUserName + null byte + 4 byte length + inPassword + null byte
	
				length	= strlen( inUserName ) + 1;
				ptr		= authBuff->fBufferData;
				
				::memcpy( ptr, &length, 4 );
				ptr += 4;
				authBuff->fBufferLength += 4;
				
				::memcpy( ptr, inUserName, length );
				ptr += length;
				authBuff->fBufferLength += length;
				
				if (inPassword != nil)
				{
					length = strlen( inPassword ) + 1;
				}
				else //use empty string
				{
					length = 0;
				}
				::memcpy( ptr, &length, 4 );
				ptr += 4;
				authBuff->fBufferLength += 4;
				
				if (inPassword != nil)
				{
					::memcpy( ptr, inPassword, length );
				}
				ptr += length;
				authBuff->fBufferLength += length;
				
				//set the root user password
				siResult = dsDoDirNodeAuth( inDSNodeRef, authMethod, true, authBuff, dataBuff, NULL );
				
				dsDataBufferDeAllocate( inDSRef, dataBuff );
				dataBuff = NULL;			
				dsDataNodeDeAllocate( inDSRef, authMethod );
				authMethod = NULL;
				dsDataBufferDeAllocate( inDSRef, authBuff );
				authBuff = NULL;			

				if (siResult == eDSNoErr)
				{
					if (inVerbose) fprintf(stdout, "Authentication setup for the identity user <%s>\n", inUserName);
				}
				else
				{
					if (inVerbose) fprintf(stdout, "Failed to setup Authentication for the identity user <%s>\n", inUserName);
				}
			}

		}
		else
		{
			if (inVerbose) fprintf(stdout, "Record of username <%s> not created\n", inUserName);
		}
	}
	
	if (userRecordName != nil)
	{
		free(userRecordName);
		userRecordName = nil;
	}
}

void RemoveIdentity( tDirReference inDSRef, tDirNodeReference inDSNodeRef, char *inUserName, bool inVerbose )
{
	signed long			siResult		= eDSNoErr;
	char			   *userRecordName  = nil;
	tRecordReference	aDSRecordRef	= 0;
	
	userRecordName = getSingleRecordAttribute(inDSRef, inDSNodeRef, inUserName, kDSStdRecordTypeUsers, kDSNAttrRecordName, &siResult, inVerbose);

	//if already exists then we delete
	if ( (userRecordName != nil) && (siResult == eDSNoErr) )
	{
		aDSRecordRef = openRecord(inDSRef, inDSNodeRef, inUserName, kDSStdRecordTypeUsers, &siResult, inVerbose);
		if (siResult == eDSNoErr)
		{
			siResult = dsDeleteRecord(aDSRecordRef);
			if (siResult == eDSNoErr)
			{
				if (inVerbose) fprintf(stdout, "Record has been deleted\n");
			}
		}
		else
		{
			if (inVerbose) fprintf(stdout, "Record not opened so not deleted\n");
		}
		free(userRecordName);
		userRecordName = nil;
	}
}

