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
 * Tool used to manipulate group records via the DirectoryService API.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <membership.h>
#include <membershipPriv.h>

#include <CoreFoundation/CoreFoundation.h>

#include <DirectoryService/DirectoryService.h>

#include "dscommon.h"
#include "dstools_version.h"
#include "HighLevelDirServicesMini.h"

#warning VERIFY the version string before each distinct build submission that changes the dseditgroup tool
const char *version = "1.1.6";
	
signed long			deleteGroupMember			(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													tRecordReference inRecordRef,
													char* inRecordName,
													char* inRecordType,
													bool inVerbose);
signed long			addGroupMember				(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													tRecordReference inRecordRef,
													char* inGroupName,
													char* inRecordName,
													char* inRecordType,
													bool inVerbose);
signed long			changeGroupFormat			(	tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													tRecordReference inRecordRef,
													char* inRecordName,
													char* inDesiredFormat,
													bool inVerbose);
tRecordReference	openRecord					(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													char* inRecordName,
													char* inRecordType,
													signed long *outResult,
													bool inVerbose);
void				usage						(	void);


signed long deleteGroupMember(tDirReference inDSRef, tDirNodeReference inDSNodeRef, tRecordReference inRecordRef, char* inRecordName, char* inRecordType, bool inVerbose)
{
	signed long					siResult			= eDSNoErr;
	tDirNodeReference			aDSNodeRef			= 0;
	tDataNode				   *pAttrType			= nil;
	tAttributeValueEntry	   *pAttrValueEntry		= nil;
	char					   *guidValue			= nil;
	bool						bExists				= false;
	tDataNode				   *pAttrValue			= nil;
	char					   *theRecordType		= nil;
	bool						bAddToUsers			= false;
	
	if (inRecordName == nil)
	{
		if (inVerbose) printf("Null record name\n");
		return((signed long) eDSInvalidRecordName);
	}
	if (inRecordType == nil)
	{
		bAddToUsers = true;
		theRecordType = kDSStdRecordTypeUsers; //default to users
		if (inVerbose) printf("Assuming user record type\n");
	}
	else
	{
		theRecordType = inRecordType;
		if (strcmp(inRecordType, kDSStdRecordTypeUsers) == 0)
		{
			bAddToUsers = true;
		}
	}
	if (inDSRef == 0)
	{
		if (inVerbose) printf("Null dir reference\n");
		return((signed long) eDSInvalidDirRef);
	}
	if (inDSNodeRef == 0)
	{
		if (inVerbose) printf("Null node reference\n");
		return((signed long) eDSInvalidNodeRef);
	}
	if (inRecordRef == 0)
	{
		if (inVerbose) printf("Null record reference\n");
		return((signed long) eDSInvalidRecordRef);
	}

	do
	{
//TBR rework which status gets propagated up?
		//first check if we can retrieve a GUID for the given inRecordName
		//assume that the search on the search node is unauthenticated
		aDSNodeRef = getNodeRef(inDSRef, "/Search", nil, nil, inVerbose);
		guidValue = getSingleRecordAttribute(inDSRef, aDSNodeRef, inRecordName, theRecordType, kDS1AttrGeneratedUID, &siResult, inVerbose);
		if ( (siResult == eDSNoErr) && (guidValue != nil) )
		{
			//retrieve the existing members if there are any
			if (strcmp(theRecordType, kDSStdRecordTypeGroups) == 0)
			{
				pAttrType = dsDataNodeAllocateString( inDSRef, kDSNAttrNestedGroups );
			}
			else
			{
				pAttrType = dsDataNodeAllocateString( inDSRef, kDSNAttrGroupMembers );
			}
			pAttrValue = dsDataNodeAllocateString( inDSRef, guidValue );
			
			if ( (pAttrType != nil) && (pAttrValue != nil) )
			{
				siResult = dsGetRecordAttributeValueByValue( inRecordRef, pAttrType, pAttrValue, &pAttrValueEntry );
				if ( siResult == eDSNoErr )
				{
					siResult = dsRemoveAttributeValue( inRecordRef, pAttrType, pAttrValueEntry->fAttributeValueID );
					dsDeallocAttributeValueEntry( inDSRef, pAttrValueEntry );
					bExists = true;
				}
				pAttrValueEntry = nil;
			}//if ( (pAttrType != nil) && (pAttrValue != nil) )
		}//if (guidValue != nil)
		
		//always leave the while
		break;
	} while(true);

	if (pAttrType != nil)
	{
		dsDataNodeDeAllocate( inDSRef, pAttrType );
		pAttrType = nil;
	}
	if (pAttrValue != nil)
	{
		dsDataNodeDeAllocate( inDSRef, pAttrValue );
		pAttrValue = nil;
	}

	if (bAddToUsers)
	{
		do
		{
	//TBR rework which status gets propagated up?
			pAttrType = dsDataNodeAllocateString( inDSRef, kDSNAttrGroupMembership );
			pAttrValue = dsDataNodeAllocateString( inDSRef, inRecordName );
			
			if ( (pAttrType != nil) && (pAttrValue != nil) )
			{
				siResult = dsGetRecordAttributeValueByValue( inRecordRef, pAttrType, pAttrValue, &pAttrValueEntry );
				if ( siResult == eDSNoErr )
				{
					siResult = dsRemoveAttributeValue( inRecordRef, pAttrType, pAttrValueEntry->fAttributeValueID );
					dsDeallocAttributeValueEntry( inDSRef, pAttrValueEntry );
					bExists = true;
				}
				pAttrValueEntry = nil;
			}//if ( (pAttrType != nil) && (pAttrValue != nil) )
			
			//always leave the while
			break;
		} while(true);
	}//if (bAddToUsers)

	if ( aDSNodeRef != 0 )
	{
		dsCloseDirNode( aDSNodeRef );
		aDSNodeRef = 0;
	}
	if (guidValue != nil)
	{
		free(guidValue);
		guidValue = nil;
	}
	if (pAttrType != nil)
	{
		dsDataNodeDeAllocate( inDSRef, pAttrType );
		pAttrType = nil;
	}
	if (pAttrValue != nil)
	{
		dsDataNodeDeAllocate( inDSRef, pAttrValue );
		pAttrValue = nil;
	}

	return( siResult );
}//deleteGroupMember


signed long addGroupMember(tDirReference inDSRef, tDirNodeReference inDSNodeRef, tRecordReference inRecordRef, char* inGroupName, char* inRecordName, char* inRecordType, bool inVerbose)
{
	signed long					siResult			= eDSNoErr;
	tDirNodeReference			aDSNodeRef			= 0;
	tDataNode				   *pAttrType			= nil;
	tAttributeValueEntry	   *pAttrValueEntry		= nil;
	tAttributeEntry			   *pAttrEntry			= nil;
	char					   *guidValue			= nil;
	char					   *idValue				= nil;
	bool						bExists				= false;
	tDataNode				   *pAttrValue			= nil;
	char					   *theRecordType		= nil;
	bool						bAddToUsers			= false;
	bool						bAttrFound			= false;
	dsBool						bGroupIsLegacy		= false;
	CFStringRef					cfStrFabricatedGUID	= nil;
	CFStringRef					cfStrRecordType		= nil;
	CFIndex						guidLength			= nil;
	
	if (inRecordName == nil)
	{
		if (inVerbose) printf("Null record name\n");
		return((signed long) eDSInvalidRecordName);
	}
	if (inRecordType == nil)
	{
		bAddToUsers = true;
		theRecordType = kDSStdRecordTypeUsers; //default to users
		if (inVerbose) printf("Assuming user record type\n");
	}
	else
	{
		theRecordType = inRecordType;
		if (strcmp(inRecordType, kDSStdRecordTypeUsers) == 0)
		{
			bAddToUsers = true;
		}
	}
	if (inDSRef == 0)
	{
		if (inVerbose) printf("Null dir reference\n");
		return((signed long) eDSInvalidDirRef);
	}
	if (inDSNodeRef == 0)
	{
		if (inVerbose) printf("Null node reference\n");
		return((signed long) eDSInvalidNodeRef);
	}
	if (inRecordRef == 0)
	{
		if (inVerbose) printf("Null record reference\n");
		return((signed long) eDSInvalidRecordRef);
	}
	if (inGroupName == nil)
	{
		if (inVerbose) printf("Null group name\n");
		return((signed long) eDSInvalidRecordName);
	}

	siResult = HLDSIsLegacyGroup( inDSRef, inDSNodeRef, inGroupName, &bGroupIsLegacy, NULL );

	do
	{
		if( siResult != eDSNoErr )
			break;
		if( bGroupIsLegacy )	//don't add GUIDs to legacy groups
		{
			if( strcmp( theRecordType, kDSStdRecordTypeUsers ) != 0 )
				printf( "Only users may be members of legacy style groups.\n" );
			break;
		}
//TBR rework which status gets propagated up?
		//first check if we can retrieve a GUID for the given inRecordName
		//assume that the search on the search node is unauthenticated
		aDSNodeRef = getNodeRef(inDSRef, "/Search", nil, nil, inVerbose);
		guidValue = getSingleRecordAttribute(inDSRef, aDSNodeRef, inRecordName, theRecordType, kDS1AttrGeneratedUID, &siResult, inVerbose);
		if( guidValue == nil )
		{
			idValue = getSingleRecordAttribute(inDSRef, aDSNodeRef, inRecordName, theRecordType, 
				bAddToUsers ? kDS1AttrUniqueID : kDS1AttrPrimaryGroupID, &siResult, inVerbose);
			if( idValue != nil )
			{
				cfStrRecordType = CFStringCreateWithCString( NULL, theRecordType, kCFStringEncodingUTF8 );
				cfStrFabricatedGUID = HLDSCreateFabricatedGUID( atoi( idValue ), cfStrRecordType );
				CFRelease( cfStrRecordType );
				cfStrRecordType = nil;
				
				guidLength = CFStringGetLength( cfStrFabricatedGUID );
				guidValue = calloc( guidLength + 1, sizeof( unsigned short ) );
				if( !CFStringGetCString( cfStrFabricatedGUID, guidValue, ( guidLength + 1 ) * sizeof( unsigned short ), kCFStringEncodingUTF8 ) )
				{
					free( guidValue );
					guidValue = nil;
				}
				
				if( cfStrFabricatedGUID != nil )
				{
					CFRelease( cfStrFabricatedGUID );
					cfStrFabricatedGUID = nil;
				}
				
				free( idValue );
				idValue = nil;
				
			}
		}
		
		if ( (siResult == eDSNoErr) && (guidValue != nil) )
		{
			//retrieve the existing members if there are any
			if (strcmp(theRecordType, kDSStdRecordTypeGroups) == 0)
			{
				pAttrType = dsDataNodeAllocateString( inDSRef, kDSNAttrNestedGroups );
			}
			else
			{
				pAttrType = dsDataNodeAllocateString( inDSRef, kDSNAttrGroupMembers );
			}
			pAttrValue = dsDataNodeAllocateString( inDSRef, guidValue );
			
			if ( (pAttrType != nil) && (pAttrValue != nil) )
			{
				//need to determine if attr type already exists
				bAttrFound = false;
				siResult = dsGetRecordAttributeInfo( inRecordRef, pAttrType, &pAttrEntry );
				if (siResult == eDSNoErr)
				{
					bAttrFound = true;
					dsDeallocAttributeEntry( inDSRef, pAttrEntry );
					pAttrEntry = nil;
					siResult = dsGetRecordAttributeValueByValue( inRecordRef, pAttrType, pAttrValue, &pAttrValueEntry );
					if ( siResult == eDSNoErr )
					{
						dsDeallocAttributeValueEntry( inDSRef, pAttrValueEntry );
						bExists = true;
					}
					pAttrValueEntry = nil;
				}

				if (!bExists)
				{
					if (bAttrFound)
					{
						siResult = dsAddAttributeValue( inRecordRef, pAttrType, pAttrValue );
					}
					else
					{
						siResult = dsAddAttribute( inRecordRef, pAttrType, nil, pAttrValue );
					}
				}//if (!bExists)
			}//if ( (pAttrType != nil) && (pAttrValue != nil) )
		}//if (guidValue != nil)
		else
		{
			printf( "record \"%s\" of type \"%s\" not found.\n", inRecordName, theRecordType );
			bAddToUsers = false;
		}
		
		//always leave the while
		break;
	} while(true);

	if (pAttrType != nil)
	{
		dsDataNodeDeAllocate( inDSRef, pAttrType );
		pAttrType = nil;
	}
	if (pAttrValue != nil)
	{
		dsDataNodeDeAllocate( inDSRef, pAttrValue );
		pAttrValue = nil;
	}

	if (bAddToUsers)
	{
		do
		{
	//TBR rework which status gets propagated up?
			pAttrType = dsDataNodeAllocateString( inDSRef, kDSNAttrGroupMembership );
			pAttrValue = dsDataNodeAllocateString( inDSRef, inRecordName );
			
			if ( (pAttrType != nil) && (pAttrValue != nil) )
			{
				//need to determine if attr type already exists
				bAttrFound = false;
				siResult = dsGetRecordAttributeInfo( inRecordRef, pAttrType, &pAttrEntry );
				if (siResult == eDSNoErr)
				{
					bAttrFound = true;
					dsDeallocAttributeEntry( inDSRef, pAttrEntry );
					pAttrEntry = nil;
					siResult = dsGetRecordAttributeValueByValue( inRecordRef, pAttrType, pAttrValue, &pAttrValueEntry );
					if ( siResult == eDSNoErr )
					{
						dsDeallocAttributeValueEntry( inDSRef, pAttrValueEntry );
						bExists = true;
					}
					pAttrValueEntry = nil;
				}

				if (!bExists)
				{
					if (bAttrFound)
					{
						siResult = dsAddAttributeValue( inRecordRef, pAttrType, pAttrValue );
					}
					else
					{
						siResult = dsAddAttribute( inRecordRef, pAttrType, nil, pAttrValue );
					}
				}//if (!bExists)
			}//if ( (pAttrType != nil) && (pAttrValue != nil) )
			
			//always leave the while
			break;
		} while(true);
	}//if (bAddToUsers)

	if ( aDSNodeRef != 0 )
	{
		dsCloseDirNode( aDSNodeRef );
		aDSNodeRef = 0;
	}
	if (guidValue != nil)
	{
		free(guidValue);
		guidValue = nil;
	}
	if (pAttrType != nil)
	{
		dsDataNodeDeAllocate( inDSRef, pAttrType );
		pAttrType = nil;
	}
	if (pAttrValue != nil)
	{
		dsDataNodeDeAllocate( inDSRef, pAttrValue );
		pAttrValue = nil;
	}

	return( siResult );
}//addGroupMember

signed long changeGroupFormat(tDirReference inDSRef, tDirNodeReference inDSNodeRef, tRecordReference inRecordRef, char* inRecordName, char* inDesiredFormat, bool inVerbose)
{
	signed long					siResult			= eDSNoErr;
	dsBool						isLegacy			= false;
	dsBool						upgrade				= true;
	dsBool						needToReleaseGUID	= false;
	CFArrayRef					shortNameMembers	= NULL;
	CFArrayRef					usersAttrsValues	= NULL;
	CFArrayRef					values				= NULL;
	CFDictionaryRef				recAttrsValues		= NULL;
	CFIndex						i, numRecs;
	CFStringRef					value				= NULL;
	CFStringRef					guid				= NULL;
	char						cStrBuffer[256];
	CFStringRef					desiredAttrNames[]	= { CFSTR( kDS1AttrGeneratedUID ), CFSTR( kDS1AttrUniqueID ) };
	CFArrayRef					attributesToGet		= CFArrayCreate( NULL, (const void**)desiredAttrNames, 2, &kCFTypeArrayCallBacks );
	
	if( ( strcmp( inDesiredFormat, "l" ) != 0 ) && ( strcmp( inDesiredFormat, "n" ) != 0 ) )
	{
		usage();
		return siResult;
	}

	upgrade = ( strcmp( inDesiredFormat, "n" ) == 0 );
	
	
	siResult = HLDSIsLegacyGroup( inDSRef, inDSNodeRef, inRecordName, &isLegacy, &shortNameMembers );

	if( ( siResult == eDSNoErr ) && ( upgrade != isLegacy ) )
	{
		if( shortNameMembers != NULL )
			CFRelease( shortNameMembers );
		printf( "Group is already in desired format.\n" );
		return siResult;
	}

	if( upgrade )
	{
		siResult = HLDSGetAttributeValuesFromRecordsByName( inDSRef, inDSNodeRef, kDSStdRecordTypeUsers, shortNameMembers,
			attributesToGet, &usersAttrsValues );
		if( siResult == eDSNoErr )
		{
			numRecs = CFArrayGetCount( usersAttrsValues );
			for( i=0; ( i < numRecs ) && ( siResult == eDSNoErr ); i++ )
			{
				needToReleaseGUID = false;
				recAttrsValues = (CFDictionaryRef)CFArrayGetValueAtIndex( usersAttrsValues, i );
				values = CFDictionaryGetValue( recAttrsValues, CFSTR( kDS1AttrGeneratedUID ) );
				if( ( values == NULL ) || ( CFArrayGetCount( values ) == 0 ) )
				{
					values = CFDictionaryGetValue( recAttrsValues, CFSTR( kDS1AttrUniqueID ) );
					if( ( values == NULL ) || ( CFArrayGetCount( values ) == 0 ) )	//if there's no GUID *and* no unique ID, then bail on this record
						continue;
					
					value = CFArrayGetValueAtIndex( values, 0 );
					if( !CFStringGetCString( value, cStrBuffer, sizeof( cStrBuffer ), kCFStringEncodingUTF8 ) )
						continue;
					
					guid = HLDSCreateFabricatedGUID( atoi( cStrBuffer ), CFSTR( kDSStdRecordTypeUsers ) );
					needToReleaseGUID = true;
				}
				else
					guid = CFArrayGetValueAtIndex( values, 0 );

				if( CFStringGetCString( guid, cStrBuffer, sizeof( cStrBuffer ), kCFStringEncodingUTF8 ) )
					siResult = HLDSAddAttributeValue( inDSRef, inRecordRef, kDSNAttrGroupMembers, false, cStrBuffer );
				
				if( needToReleaseGUID  )
					CFRelease( guid );
			}
		}
	}
	else	//downgrade the group here
		siResult = HLDSRemoveAttribute( inDSRef, inRecordRef, kDSNAttrGroupMembers );

	if( usersAttrsValues != NULL )
		CFRelease( usersAttrsValues );
	if( shortNameMembers != NULL )
		CFRelease( shortNameMembers );

	return siResult;
}//changeGroupFormat

//-----------------------------------------------------------------------------
//	usage
//
//-----------------------------------------------------------------------------

void
usage(void)
{
	printf("\ndseditgroup:: Manipulate group records with the DirectoryService API.\n");
	printf("Version %s\n", version);
	printf(	"Usage: dseditgroup [-opqv] [-m username] [-n nodename -u username -P password\n"
		"                   -a recordname -d recordname -t recordtype -i gid -g guid\n"
		"                   -r realname -k keyword -c comment -s timetolive -f n|l] groupname\n");
	printf("Please see man page dseditgroup(8) for details.\n");
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
	int				errorcode		= 0;
	
	signed long			siResult		= eDSAuthFailed;
	tDirReference		aDSRef			= 0;
	tDirNodeReference   aDSNodeRef		= 0;
	bool				bContinueAdd	= false;
	char			   *groupRecordName	= nil;
	tRecordReference	aDSRecordRef	= 0;
    
	if (argc < 2)
	{
		usage();
		exit(0);
	}
	
	if ( strcmp(argv[1], "-appleversion") == 0 )
        dsToolAppleVersionExit( argv[0] );
	
    while ((ch = getopt(argc, argv, "o:pqvn:m:u:P:a:d:t:i:g:r:k:c:s:S:f:?h")) != -1) {
        switch (ch) {
        case 'o':
            operation = strdup(optarg);
			if (operation != nil)
			{
				if ( (strcasecmp(operation, "read") == 0) || (strcasecmp(operation, "r") == 0) )
				{
					bReadOption = true;
				}
				else if ( (strcasecmp(operation, "create") == 0) || (strcasecmp(operation, "c") == 0) )
				{
					bCreateOption = true;
				}
				else if ( (strcasecmp(operation, "delete") == 0) || (strcasecmp(operation, "d") == 0) )
				{
					bDeleteOption = true;
				}
				else if ( (strcasecmp(operation, "edit") == 0) || (strcasecmp(operation, "e") == 0) )
				{
					bEditOption = true;
				}
				else if ( (strcasecmp(operation, "checkmember") == 0) )
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
        case 'i':
            gid = strdup(optarg);
            break;
        case 'g':
            guid = strdup(optarg);
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
				exit(0);
			}
        }
    }
	
	if (argc > 1)
	{
		groupname = strdup(argv[argc-1]);
	}
	
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
		if (gid)
			printf("Keyword provided as <%s>\n", gid);
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
		printf("\n");
	}
	
	if ( ( !bDefaultUser && ( (password == nil) || bInteractivePwd ) ) || (bDefaultUser && bInteractivePwd) )
	{
		password = read_passphrase("Please enter user password:", 1);
		//do not verbose output this password value
	}
	
	do //use break to stop for an error
	{
		siResult = dsOpenDirService( &aDSRef );
		if ( siResult != eDSNoErr )
		{
			if (bVerbose)
			{
				printf("dsOpenDirService failed with error <%ld>\n", siResult);
			}
			break;
		}

		//set up the node to be used
		aDSNodeRef = getNodeRef(aDSRef, nodename, username, password, bVerbose);
		if ( aDSNodeRef == 0 )
		{
			if (bVerbose)
			{
				printf("getNodeRef failed to obtain a node reference\n");
			}
			break;
		}

		if (bReadOption)
		{
			getAndOutputRecord(aDSRef, aDSNodeRef, groupname, kDSStdRecordTypeGroups, bVerbose);
		}
		else if (bDeleteOption)
		{
			if (bVerbose)
			{
				printf("Group Record <%s> to be Deleted\n", groupname);
				getAndOutputRecord(aDSRef, aDSNodeRef, groupname, kDSStdRecordTypeGroups, bVerbose);
			}
			//check if already exists
			siResult = eDSNoErr;
			groupRecordName = getSingleRecordAttribute(aDSRef, aDSNodeRef, groupname, kDSStdRecordTypeGroups, kDSNAttrRecordName, &siResult, bVerbose);

			//if already exists then we verify to delete if required
			if ( (groupRecordName != nil) && (siResult == eDSNoErr) )
			{
				if (!bNoVerify)
				{
					char responseValue[8] = {0};
					printf("Delete called on existing record - do you really want to delete, y or n : ");
					scanf("%s", responseValue);
					printf("\n");
					if ((responseValue[0] == 'y') || (responseValue[0] == 'Y'))
					{
						aDSRecordRef = openRecord(aDSRef, aDSNodeRef, groupname, kDSStdRecordTypeGroups, &siResult, bVerbose);
						if (siResult == eDSNoErr)
						{
							siResult = dsDeleteRecord(aDSRecordRef);
							if (siResult == eDSNoErr)
							{
								if (bVerbose) printf("Record has been deleted\n");
							}
						}
						else
						{
							if (bVerbose) printf("Record not opened so not deleted\n");
						}
					}
				}
			} 
		}
		else if (bCreateOption)
		{
			//check if already exists
			siResult = eDSNoErr;
			groupRecordName = getSingleRecordAttribute(aDSRef, aDSNodeRef, groupname, kDSStdRecordTypeGroups, kDSNAttrRecordName, &siResult, bVerbose);

			//if already exists then verify we continue with add of parameters to existing record ie. set bContinueAdd
			if ( (groupRecordName != nil) && (siResult == eDSNoErr) )
			{
				char responseValue[8] = {0};
				if (!bNoVerify)
				{
					printf("Create called on existing record - do you want to overwrite, y or n : ");
					scanf("%s", responseValue);
					printf("\n");
				}
				if (bNoVerify || (responseValue[0] == 'y') || (responseValue[0] == 'Y'))
				{
					aDSRecordRef = openRecord(aDSRef, aDSNodeRef, groupname, kDSStdRecordTypeGroups, &siResult, bVerbose);
					if (siResult == eDSNoErr)
					{
						bContinueAdd = true;
					}
					else
					{
						if (bVerbose) printf("Record not opened but already exists\n");
					}
				}
			} 
			else if ( (groupRecordName == nil) && (siResult == eDSRecordNotFound) )
			{
				aDSRecordRef = createAndOpenRecord(aDSRef, aDSNodeRef, groupname, kDSStdRecordTypeGroups, &siResult, bVerbose);
				if (siResult == eDSNoErr)
				{
					groupRecordName = strdup(groupname);
					bContinueAdd = true;
				}
				else
				{
					if (bVerbose) printf("Record not created\n");
				}
			}
		}
		else if (bEditOption)
		{
			//check if already exists
			siResult = eDSNoErr;
			groupRecordName = getSingleRecordAttribute(aDSRef, aDSNodeRef, groupname, kDSStdRecordTypeGroups, kDSNAttrRecordName, &siResult, bVerbose);
			
			//if already exists then open and set bContinueAdd
			if ( (groupRecordName != nil) && (siResult == eDSNoErr) )
			{
				aDSRecordRef = openRecord(aDSRef, aDSNodeRef, groupRecordName, kDSStdRecordTypeGroups, &siResult, bVerbose);
				if (siResult == eDSNoErr)
				{
					bContinueAdd = true;
				}
				else
				{
					if (bVerbose) printf("Record not opened\n");
				}
			} 
		}
		else if (bCheckMemberOption)
		{
			uuid_t uu;
			uuid_t gu;
			char *user = (member ? member : username);
			
			errorcode = mbr_user_name_to_uuid(user, uu);
			if (errorcode == 0)
			{
				if (bVerbose)
				{
					char uuidStr[MBR_UU_STRING_SIZE];
					mbr_uuid_to_string(uu, uuidStr);
					printf("User UUID = %s\n", uuidStr );
				}
				
				errorcode = mbr_group_name_to_uuid(groupname, gu);
				if (errorcode == 0)
				{
					int isMember;
					
					if(bVerbose)
					{
						char guidStr[MBR_UU_STRING_SIZE];
						mbr_uuid_to_string(gu, guidStr);
						printf("Group UUID = %s\n", guidStr );
					}
					
					errorcode = mbr_check_membership(uu, gu, &isMember);
					if (errorcode == 0)
					{
						if (isMember)
						{
							// return default errorcode of 0 if they are a member
							printf("yes %s is a member of %s\n", user, groupname);
						}
						else
						{
							printf("no %s is NOT a member of %s\n", user, groupname);
							errorcode = ENOENT;
						}
					}
					else
						printf("Error resolving group membership %d (%s)\n", errorcode, strerror(errorcode));
				}
				else
					printf("Error resolving group UUID %d (%s)\n", errorcode, strerror(errorcode));
				
			}
			else
				printf("Error resolving user UUID %d (%s)\n", errorcode, strerror(errorcode));
			
		}
		
		if ( (bCreateOption || bEditOption) && (siResult == eDSNoErr) )
		{
			if (bContinueAdd)
			{
				char *recType = nil;
				recType = kDSStdRecordTypeUsers;
				if (recordtype != nil)
				{
					//map the users, groups, computers names to Std types
					if (strcasecmp(recordtype,"user") == 0)
					{
						recType = kDSStdRecordTypeUsers;
					}
					if (strcasecmp(recordtype,"group") == 0)
					{
						recType = kDSStdRecordTypeGroups;
					}
					if (strcasecmp(recordtype,"computer") == 0)
					{
						recType = kDSStdRecordTypeComputers;
					}
				}
				//series of calls to add or replace parameters as specified
				if (addrecordname != nil)
				{
					siResult = addGroupMember(aDSRef, aDSNodeRef, aDSRecordRef, groupRecordName, addrecordname, recType, bVerbose);
				}
				if (delrecordname != nil)
				{
					siResult = deleteGroupMember(aDSRef, aDSNodeRef, aDSRecordRef, delrecordname, recType, bVerbose);
				}
				if (format != nil)
				{
					siResult = changeGroupFormat(aDSRef, aDSNodeRef, aDSRecordRef, groupRecordName, format, bVerbose);
				}
				if (gid)
				{
					addRecordParameter(aDSRef, aDSNodeRef, aDSRecordRef, kDS1AttrPrimaryGroupID, gid, bVerbose);
				}
				else if (bCreateOption) //gid default creation only for create group
				{
					//check if gid already exists - otherwise create and set one
					siResult = eDSNoErr;
					gid = getSingleRecordAttribute(aDSRef, aDSNodeRef, groupname, kDSStdRecordTypeGroups, kDS1AttrPrimaryGroupID, &siResult, bVerbose);
					
					if ( (gid != nil) && (siResult == eDSNoErr) )
					{
						if (atoi(gid) >= 500)
						{
							if (bVerbose) printf("gid already exists\n");
						}
						else
						{
							gid = createNewgid(aDSRef, aDSNodeRef, bVerbose);
							if (gid != nil)
							{
								addRecordParameter(aDSRef, aDSNodeRef, aDSRecordRef, kDS1AttrPrimaryGroupID, gid, bVerbose);
								if (bVerbose) printf("Next free gid value determined and added\n");
							}
						}
					}
					else
					{
						gid = createNewgid(aDSRef, aDSNodeRef, bVerbose);
						if (gid != nil)
						{
							addRecordParameter(aDSRef, aDSNodeRef, aDSRecordRef, kDS1AttrPrimaryGroupID, gid, bVerbose);
							if (bVerbose) printf("Next free gid value determined and added\n");
						}
					}
				}
				if (guid)
				{
					char* temp = guid;
					while (*temp != '\0') {
						*temp = toupper(*temp);
						temp++;
					}
					addRecordParameter(aDSRef, aDSNodeRef, aDSRecordRef, kDS1AttrGeneratedUID, guid, bVerbose);
					if (bVerbose) printf("GUID value added\n");
				}
				else if (bCreateOption) //guid default creation only for create group
				{
					//check if GUID already exists - otherwise create and set one
					siResult = eDSNoErr;
					guid = getSingleRecordAttribute(aDSRef, aDSNodeRef, groupname, kDSStdRecordTypeGroups, kDS1AttrGeneratedUID, &siResult, bVerbose);
					
					if ( (guid != nil) && (siResult == eDSNoErr) )
					{
						if (bVerbose) printf("GUID already exists\n");
					}
					else
					{
						guid = createNewGUID(bVerbose);
						addRecordParameter(aDSRef, aDSNodeRef, aDSRecordRef, kDS1AttrGeneratedUID, guid, bVerbose);
						if (bVerbose) printf("GUID value created and added\n");
					}
				}
				if (smbSID)
				{
					addRecordParameter(aDSRef, aDSNodeRef, aDSRecordRef, kDS1AttrSMBSID, smbSID, bVerbose);
					if (bVerbose) printf("SID value added\n");
				}
				if (realname)
				{
					addRecordParameter(aDSRef, aDSNodeRef, aDSRecordRef, kDS1AttrDistinguishedName, realname, bVerbose);
				}
				if (keyword)
				{
					addRecordParameter(aDSRef, aDSNodeRef, aDSRecordRef, kDSNAttrKeywords, keyword, bVerbose);
				}
				if (comment)
				{
					addRecordParameter(aDSRef, aDSNodeRef, aDSRecordRef, kDS1AttrComment, comment, bVerbose);
				}
				if (timeToLive)
				{
					addRecordParameter(aDSRef, aDSNodeRef, aDSRecordRef, kDS1AttrTimeToLive, timeToLive, bVerbose);
				}
			}
			
			if (bVerbose)
			{
				if (bCreateOption)
					printf("Group Record <%s> Created\n", groupname);
				if (bEditOption)
					printf("Group Record <%s> Edited\n", groupname);
				getAndOutputRecord(aDSRef, aDSNodeRef, groupname, kDSStdRecordTypeGroups, bVerbose);
			}
		}
		
		//always leave the while
		break;
	} while(true);

	//cleanup DS API references and variables
	if ( aDSRecordRef != 0 )
	{
		dsCloseRecord( aDSRecordRef );
		aDSRecordRef = 0;
	}

	if ( aDSNodeRef != 0 )
	{
		dsCloseDirNode( aDSNodeRef );
		aDSNodeRef = 0;
	}

	if ( aDSRef != 0 )
	{
		dsCloseDirService( aDSRef );
		aDSRef = 0;
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

	exit(errorcode);
}

