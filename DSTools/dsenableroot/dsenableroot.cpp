/*
 * Copyright (c) 2000 - 2004 Apple Computer, Inc. All rights reserved.
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
 * @header dsenableroot
 * Tool used to enable the root user account.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>

#include <CoreFoundation/CoreFoundation.h>

#include <DirectoryService/DirectoryService.h>
#include "dstools_version.h"

#warning VERIFY the version string before each distinct build submission that changes the dsenablerootuser tool
const char *version = "1.2.2";
	
signed long EnableRootUser( const char *userName, const char *password, const char *rootPassword );
signed long EnableRootUser( const char *userName, const char *password, const char *rootPassword )
{
	signed long				siResult		= eDSAuthFailed;
	tDirReference			aDSRef			= 0;
	tDataBufferPtr			dataBuff		= nil;
	unsigned long			nodeCount		= 0;
	tContextData			context			= nil;
	tDataListPtr			nodeName		= nil;
	tDirNodeReference		aSearchNodeRef	= 0;
	tDataListPtr			recName			= nil;
	tDataListPtr			recType			= nil;
	tDataListPtr			attrTypes		= nil;
	unsigned long			recCount		= 1;
	tAttributeListRef		attrListRef		= 0;
	tRecordEntry		   *pRecEntry		= nil;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeEntry		   *pAttrEntry		= nil;
	tAttributeValueEntry   *pValueEntry		= nil;
	char*					authName		= nil;
	tDirNodeReference		nodeRef			= 0;
	tDataNodePtr			authMethod		= nil;
	tDataBufferPtr			authBuff		= nil;
	unsigned long			length			= 0;
	char*					ptr				= nil;
	tDataNodePtr			recordName		= nil;
	tDataNodePtr			recordType		= nil;
	tRecordReference		recRef			= 0;
	tDataNodePtr			attrName		= nil;
	tDataNodePtr			attrValue		= nil;
	bool					bFoundGUID		= false;
	
	
	if( (userName == nil) || (password == nil) || (rootPassword == nil) )
	{
		return(siResult);
	}
	//adapted from try/catch code so using do/while and break
	do
	{
	
		siResult = dsOpenDirService( &aDSRef );
		if ( siResult != eDSNoErr ) break;

		dataBuff = dsDataBufferAllocate( aDSRef, 1024 );
		if ( dataBuff == nil ) break;
		
		siResult = dsFindDirNodes( aDSRef, dataBuff, nil, eDSAuthenticationSearchNodeName, &nodeCount, &context );
		if ( siResult != eDSNoErr ) break;
		if ( nodeCount < 1 ) break;

		siResult = dsGetDirNodeName( aDSRef, dataBuff, 1, &nodeName );
		if ( siResult != eDSNoErr ) break;

		siResult = dsOpenDirNode( aDSRef, nodeName, &aSearchNodeRef );
		if ( siResult != eDSNoErr ) break;
		if ( nodeName != NULL )
		{
			dsDataListDeallocate( aDSRef, nodeName );
			free( nodeName );
			nodeName = NULL;
		}
		recName = dsBuildListFromStrings( aDSRef, userName, NULL );
		recType = dsBuildListFromStrings( aDSRef, kDSStdRecordTypeUsers, NULL );
		attrTypes = dsBuildListFromStrings( aDSRef, kDSNAttrMetaNodeLocation, kDSNAttrRecordName, NULL );
		recCount = 1; // only care about first match
		do 
		{
			siResult = dsGetRecordList( aSearchNodeRef, dataBuff, recName, eDSExact, recType,
																	attrTypes, false, &recCount, &context);
			if (siResult == eDSBufferTooSmall)
			{
				unsigned long bufSize = dataBuff->fBufferSize;
				dsDataBufferDeAllocate( aDSRef, dataBuff );
				dataBuff = nil;
				dataBuff = ::dsDataBufferAllocate( aDSRef, bufSize * 2 );
			}
		} while ( (siResult == eDSBufferTooSmall) || ( (siResult == eDSNoErr) && (recCount == 0) && (context != nil) ) );
		//worry about multiple calls (ie. continue data) since we continue until first match or no more searching
		
		if ( (siResult == eDSNoErr) && (recCount > 0) )
		{
			siResult = ::dsGetRecordEntry( aSearchNodeRef, dataBuff, 1, &attrListRef, &pRecEntry );
			if ( (siResult == eDSNoErr) && (pRecEntry != nil) )
			{
				//index starts at one - should have two entries
				for (unsigned int i = 1; i <= pRecEntry->fRecordAttributeCount; i++)
				{
					siResult = ::dsGetAttributeEntry( aSearchNodeRef, dataBuff, attrListRef, i, &valueRef, &pAttrEntry );
					//need to have at least one value - get first only
					if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
					{
						// Get the first attribute value
						siResult = ::dsGetAttributeValue( aSearchNodeRef, dataBuff, 1, valueRef, &pValueEntry );
						// Is it what we expected
						if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
						{
							nodeName = dsBuildFromPath( aDSRef, pValueEntry->fAttributeValueData.fBufferData, "/" );
						}
						else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
						{
							authName = (char*)calloc( 1, pValueEntry->fAttributeValueData.fBufferLength + 1 );
							strncpy( authName, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
						}
						if ( pValueEntry != NULL )
						{
							dsDeallocAttributeValueEntry( aDSRef, pValueEntry );
							pValueEntry = NULL;
						}
					}
					dsCloseAttributeValueList(valueRef);
					if (pAttrEntry != nil)
					{
						dsDeallocAttributeEntry(aDSRef, pAttrEntry);
						pAttrEntry = nil;
					}
				} //loop over attrs requested
			}//found 1st record entry
			dsCloseAttributeList(attrListRef);
			if (pRecEntry != nil)
			{
				dsDeallocRecordEntry(aDSRef, pRecEntry);
				pRecEntry = nil;
			}
		}// got records returned
		else
		{
			siResult = eDSAuthUnknownUser;
		}
		
		if ( (siResult == eDSNoErr) && (nodeName != nil) && (authName != nil) )
		{
			siResult = dsOpenDirNode( aDSRef, nodeName, &nodeRef );
			if ( siResult != eDSNoErr ) break;
			
			authMethod	= dsDataNodeAllocateString( aDSRef, kDSStdAuthNodeNativeClearTextOK );
			authBuff	= dsDataBufferAllocate( aDSRef, strlen( authName ) + strlen( password ) + 10 );
			// 4 byte length + username + null byte + 4 byte length + password + null byte

			length	= strlen( authName ) + 1;
			ptr		= authBuff->fBufferData;
			
			::memcpy( ptr, &length, 4 );
			ptr += 4;
			authBuff->fBufferLength += 4;
			
			::memcpy( ptr, authName, length );
			ptr += length;
			authBuff->fBufferLength += length;
			
			length = strlen( password ) + 1;
			::memcpy( ptr, &length, 4 );
			ptr += 4;
			authBuff->fBufferLength += 4;
			
			::memcpy( ptr, password, length );
			ptr += length;
			authBuff->fBufferLength += length;
			
			//if this process is run as root then the session auth is not even required
			siResult = dsDoDirNodeAuth( nodeRef, authMethod, false, authBuff, dataBuff, NULL );

			dsDataNodeDeAllocate( aDSRef, authMethod );
			authMethod = NULL;
			dsDataBufferDeAllocate( aDSRef, authBuff );
			authBuff = NULL;			
			
			if (siResult == eDSNoErr)
			{
				recordType	= dsDataNodeAllocateString( aDSRef, kDSStdRecordTypeUsers );
				recordName	= dsDataNodeAllocateString( aDSRef, "root" );
				//open the root user record
				siResult = dsOpenRecord( nodeRef, recordType, recordName, &recRef );
				if ( siResult != eDSNoErr ) break;

                //need to determine if the GUID already exists
                attrName = dsDataNodeAllocateString( aDSRef, kDS1AttrGeneratedUID );

                siResult = dsGetRecordAttributeInfo( recRef, attrName, &pAttrEntry );
                if (siResult == eDSNoErr)
				{
                    if (pAttrEntry != nil)
                    {
                        if (pAttrEntry->fAttributeValueCount > 0 )
                        {
                            bFoundGUID = true;
                        }
                        dsDeallocAttributeEntry(aDSRef, pAttrEntry);
                        pAttrEntry = nil;
                    }
                }

                if (!bFoundGUID)
                {
                    //calculate a generated UID here
					CFUUIDRef       myUUID;
					CFStringRef     myUUIDString;
					char            genUIDValue[100];

					myUUID = CFUUIDCreate(kCFAllocatorDefault);
					myUUIDString = CFUUIDCreateString(kCFAllocatorDefault, myUUID);
					CFStringGetCString(myUUIDString, genUIDValue, 100, kCFStringEncodingASCII);
					CFRelease(myUUID);
					CFRelease(myUUIDString);

                    attrValue = dsDataNodeAllocateString( aDSRef, genUIDValue );

                    //add the correct GeneratedUID
                    siResult = dsAddAttribute( recRef, attrName, NULL, attrValue );
                    if ( siResult != eDSNoErr ) break;

                    dsDataNodeDeAllocate( aDSRef, attrValue );
                    attrValue = nil;
                }

                dsDataNodeDeAllocate( aDSRef, attrName );
                attrName = nil;

				attrName	= dsDataNodeAllocateString( aDSRef, kDSNAttrAuthenticationAuthority );
				attrValue	= dsDataNodeAllocateString( aDSRef, kDSValueAuthAuthorityShadowHash );
				
				//remove the authentication authority if it already exists - don't check status
				dsRemoveAttribute( recRef, attrName );

				//add the correct authentication authority
				siResult = dsAddAttribute( recRef, attrName, NULL, attrValue );
				if ( siResult != eDSNoErr ) break;
				
				dsDataNodeDeAllocate( aDSRef, attrName );
				attrName = nil;
				dsDataNodeDeAllocate( aDSRef, attrValue );
				attrValue = nil;

				attrName	= dsDataNodeAllocateString( aDSRef, kDS1AttrPassword );
				attrValue	= dsDataNodeAllocateString( aDSRef, kDSValueNonCryptPasswordMarker );
				
				//remove the password if it already exists - don't check status
				dsRemoveAttribute( recRef, attrName );

				//add the correct password marker
				siResult = dsAddAttribute( recRef, attrName, NULL, attrValue );
				if ( siResult != eDSNoErr ) break;
				
				siResult = dsCloseRecord( recRef );
				
				authMethod	= dsDataNodeAllocateString( aDSRef, kDSStdAuthSetPasswdAsRoot );
				authBuff	= dsDataBufferAllocate( aDSRef, strlen( "root" ) + strlen( rootPassword ) + 10 );
				// 4 byte length + "root" + null byte + 4 byte length + rootPassword + null byte
	
				length	= strlen( "root" ) + 1;
				ptr		= authBuff->fBufferData;
				
				::memcpy( ptr, &length, 4 );
				ptr += 4;
				authBuff->fBufferLength += 4;
				
				::memcpy( ptr, "root", length );
				ptr += length;
				authBuff->fBufferLength += length;
				
				length = strlen( rootPassword ) + 1;
				::memcpy( ptr, &length, 4 );
				ptr += 4;
				authBuff->fBufferLength += 4;
				
				::memcpy( ptr, rootPassword, length );
				ptr += length;
				authBuff->fBufferLength += length;
				
				//set the root user password
				siResult = dsDoDirNodeAuth( nodeRef, authMethod, true, authBuff, dataBuff, NULL );
			}
		}
		//always leave the while
		break;
	} while(true);

	if ( recName != nil )
	{
		dsDataListDeallocate( aDSRef, recName );
		free( recName );
		recName = nil;
	}
	if ( recType != nil )
	{
		dsDataListDeallocate( aDSRef, recType );
		free( recType );
		recType = nil;
	}
	if ( attrTypes != nil )
	{
		dsDataListDeallocate( aDSRef, attrTypes );
		free( attrTypes );
		attrTypes = nil;
	}
	if ( dataBuff != nil )
	{
		dsDataBufferDeAllocate( aDSRef, dataBuff );
		dataBuff = nil;
	}
	if ( nodeName != nil )
	{
		dsDataListDeallocate( aDSRef, nodeName );
		free( nodeName );
		nodeName = nil;
	}
	if ( authName != nil )
	{
		free( authName );
		authName = nil;
	}
	if ( recordName != nil )
	{
		dsDataNodeDeAllocate( aDSRef, recordName );
		recordName = nil;
	}
	if ( recordType != nil )
	{
		dsDataNodeDeAllocate( aDSRef, recordName );
		recordType = nil;
	}
	if ( attrName != nil )
	{
		dsDataNodeDeAllocate( aDSRef, attrName );
		attrName = nil;
	}
	if ( attrValue != nil )
	{
		dsDataNodeDeAllocate( aDSRef, attrValue );
		attrValue = nil;
	}
	if ( authMethod != nil )
	{
		dsDataNodeDeAllocate( aDSRef, authMethod );
		authMethod = nil;
	}
	if ( authBuff != nil )
	{
		dsDataBufferDeAllocate( aDSRef, authBuff );
		authBuff = nil;
	}
	if ( nodeRef != 0 )
	{
		dsCloseDirNode( nodeRef );
		nodeRef = 0;
	}
	if ( aSearchNodeRef != 0 )
	{
		dsCloseDirNode( aSearchNodeRef );
		aSearchNodeRef = 0;
	}
	if ( aDSRef != 0 )
	{
		dsCloseDirService( aDSRef );
		aDSRef = 0;
	}
	
	return(siResult);
}


signed long DisableRootUser( const char *userName, const char *password );
signed long DisableRootUser( const char *userName, const char *password )
{
	signed long				siResult		= eDSAuthFailed;
	tDirReference			aDSRef			= 0;
	tDataBufferPtr			dataBuff		= nil;
	unsigned long			nodeCount		= 0;
	tContextData			context			= nil;
	tDataListPtr			nodeName		= nil;
	tDirNodeReference		aSearchNodeRef	= 0;
	tDataListPtr			recName			= nil;
	tDataListPtr			recType			= nil;
	tDataListPtr			attrTypes		= nil;
	unsigned long			recCount		= 1;
	tAttributeListRef		attrListRef		= 0;
	tRecordEntry		   *pRecEntry		= nil;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeEntry		   *pAttrEntry		= nil;
	tAttributeValueEntry   *pValueEntry		= nil;
	char*					authName		= nil;
	tDirNodeReference		nodeRef			= 0;
	tDataNodePtr			authMethod		= nil;
	tDataBufferPtr			authBuff		= nil;
	unsigned long			length			= 0;
	char*					ptr				= nil;
	tDataNodePtr			recordName		= nil;
	tDataNodePtr			recordType		= nil;
	tRecordReference		recRef			= 0;
	tDataNodePtr			attrName		= nil;
	tDataNodePtr			attrValue		= nil;
	
	
	if( (userName == nil) || (password == nil) )
	{
		return(siResult);
	}
	//adapted from try/catch code so using do/while and break
	do
	{
	
		siResult = dsOpenDirService( &aDSRef );
		if ( siResult != eDSNoErr ) break;

		dataBuff = dsDataBufferAllocate( aDSRef, 1024 );
		if ( dataBuff == nil ) break;
		
		siResult = dsFindDirNodes( aDSRef, dataBuff, nil, eDSAuthenticationSearchNodeName, &nodeCount, &context );
		if ( siResult != eDSNoErr ) break;
		if ( nodeCount < 1 ) break;

		siResult = dsGetDirNodeName( aDSRef, dataBuff, 1, &nodeName );
		if ( siResult != eDSNoErr ) break;

		siResult = dsOpenDirNode( aDSRef, nodeName, &aSearchNodeRef );
		if ( siResult != eDSNoErr ) break;
		if ( nodeName != NULL )
		{
			dsDataListDeallocate( aDSRef, nodeName );
			free( nodeName );
			nodeName = NULL;
		}
		recName = dsBuildListFromStrings( aDSRef, userName, NULL );
		recType = dsBuildListFromStrings( aDSRef, kDSStdRecordTypeUsers, NULL );
		attrTypes = dsBuildListFromStrings( aDSRef, kDSNAttrMetaNodeLocation, kDSNAttrRecordName, NULL );
		recCount = 1; // only care about first match
		do 
		{
			siResult = dsGetRecordList( aSearchNodeRef, dataBuff, recName, eDSExact, recType,
																	attrTypes, false, &recCount, &context);
			if (siResult == eDSBufferTooSmall)
			{
				unsigned long bufSize = dataBuff->fBufferSize;
				dsDataBufferDeAllocate( aDSRef, dataBuff );
				dataBuff = nil;
				dataBuff = ::dsDataBufferAllocate( aDSRef, bufSize * 2 );
			}
		} while ( (siResult == eDSBufferTooSmall) || ( (siResult == eDSNoErr) && (recCount == 0) && (context != nil) ) );
		//worry about multiple calls (ie. continue data) since we continue until first match or no more searching
		
		if ( (siResult == eDSNoErr) && (recCount > 0) )
		{
			siResult = ::dsGetRecordEntry( aSearchNodeRef, dataBuff, 1, &attrListRef, &pRecEntry );
			if ( (siResult == eDSNoErr) && (pRecEntry != nil) )
			{
				//index starts at one - should have two entries
				for (unsigned int i = 1; i <= pRecEntry->fRecordAttributeCount; i++)
				{
					siResult = ::dsGetAttributeEntry( aSearchNodeRef, dataBuff, attrListRef, i, &valueRef, &pAttrEntry );
					//need to have at least one value - get first only
					if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
					{
						// Get the first attribute value
						siResult = ::dsGetAttributeValue( aSearchNodeRef, dataBuff, 1, valueRef, &pValueEntry );
						// Is it what we expected
						if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
						{
							nodeName = dsBuildFromPath( aDSRef, pValueEntry->fAttributeValueData.fBufferData, "/" );
						}
						else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
						{
							authName = (char*)calloc( 1, pValueEntry->fAttributeValueData.fBufferLength + 1 );
							strncpy( authName, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
						}
						if ( pValueEntry != NULL )
						{
							dsDeallocAttributeValueEntry( aDSRef, pValueEntry );
							pValueEntry = NULL;
						}
					}
					dsCloseAttributeValueList(valueRef);
					if (pAttrEntry != nil)
					{
						dsDeallocAttributeEntry(aDSRef, pAttrEntry);
						pAttrEntry = nil;
					}
				} //loop over attrs requested
			}//found 1st record entry
			dsCloseAttributeList(attrListRef);
			if (pRecEntry != nil)
			{
				dsDeallocRecordEntry(aDSRef, pRecEntry);
				pRecEntry = nil;
			}
		}// got records returned
		else
		{
			siResult = eDSAuthUnknownUser;
		}
		
		if ( (siResult == eDSNoErr) && (nodeName != nil) && (authName != nil) )
		{
			siResult = dsOpenDirNode( aDSRef, nodeName, &nodeRef );
			if ( siResult != eDSNoErr ) break;
			
			authMethod	= dsDataNodeAllocateString( aDSRef, kDSStdAuthNodeNativeClearTextOK );
			authBuff	= dsDataBufferAllocate( aDSRef, strlen( authName ) + strlen( password ) + 10 );
			// 4 byte length + username + null byte + 4 byte length + password + null byte

			length	= strlen( authName ) + 1;
			ptr		= authBuff->fBufferData;
			
			::memcpy( ptr, &length, 4 );
			ptr += 4;
			authBuff->fBufferLength += 4;
			
			::memcpy( ptr, authName, length );
			ptr += length;
			authBuff->fBufferLength += length;
			
			length = strlen( password ) + 1;
			::memcpy( ptr, &length, 4 );
			ptr += 4;
			authBuff->fBufferLength += 4;
			
			::memcpy( ptr, password, length );
			ptr += length;
			authBuff->fBufferLength += length;
			
			//if this process is run as root then the session auth is not even required
			siResult = dsDoDirNodeAuth( nodeRef, authMethod, false, authBuff, dataBuff, NULL );

			dsDataNodeDeAllocate( aDSRef, authMethod );
			authMethod = NULL;
			dsDataBufferDeAllocate( aDSRef, authBuff );
			authBuff = NULL;			
			
			if (siResult == eDSNoErr)
			{
				recordType	= dsDataNodeAllocateString( aDSRef, kDSStdRecordTypeUsers );
				recordName	= dsDataNodeAllocateString( aDSRef, "root" );
				//open the root user record
				siResult = dsOpenRecord( nodeRef, recordType, recordName, &recRef );
				if ( siResult != eDSNoErr ) break;

				attrName	= dsDataNodeAllocateString( aDSRef, kDSNAttrAuthenticationAuthority );
				
				//remove the authentication authority if it already exists - don't check status
				dsRemoveAttribute( recRef, attrName );

				dsDataNodeDeAllocate( aDSRef, attrName );
				attrName = nil;

				attrName	= dsDataNodeAllocateString( aDSRef, kDS1AttrPassword );
				attrValue	= dsDataNodeAllocateString( aDSRef, "*" );
				
				//remove the password if it already exists - don't check status
				dsRemoveAttribute( recRef, attrName );

				//add the correct password legacy disabled marker
				siResult = dsAddAttribute( recRef, attrName, NULL, attrValue );
				if ( siResult != eDSNoErr ) break;
				
				siResult = dsCloseRecord( recRef );
				
			}
		}
		//always leave the while
		break;
	} while(true);

	if ( recName != nil )
	{
		dsDataListDeallocate( aDSRef, recName );
		free( recName );
		recName = nil;
	}
	if ( recType != nil )
	{
		dsDataListDeallocate( aDSRef, recType );
		free( recType );
		recType = nil;
	}
	if ( attrTypes != nil )
	{
		dsDataListDeallocate( aDSRef, attrTypes );
		free( attrTypes );
		attrTypes = nil;
	}
	if ( dataBuff != nil )
	{
		dsDataBufferDeAllocate( aDSRef, dataBuff );
		dataBuff = nil;
	}
	if ( nodeName != nil )
	{
		dsDataListDeallocate( aDSRef, nodeName );
		free( nodeName );
		nodeName = nil;
	}
	if ( authName != nil )
	{
		free( authName );
		authName = nil;
	}
	if ( recordName != nil )
	{
		dsDataNodeDeAllocate( aDSRef, recordName );
		recordName = nil;
	}
	if ( recordType != nil )
	{
		dsDataNodeDeAllocate( aDSRef, recordName );
		recordType = nil;
	}
	if ( attrName != nil )
	{
		dsDataNodeDeAllocate( aDSRef, attrName );
		attrName = nil;
	}
	if ( attrValue != nil )
	{
		dsDataNodeDeAllocate( aDSRef, attrValue );
		attrValue = nil;
	}
	if ( authMethod != nil )
	{
		dsDataNodeDeAllocate( aDSRef, authMethod );
		authMethod = nil;
	}
	if ( authBuff != nil )
	{
		dsDataBufferDeAllocate( aDSRef, authBuff );
		authBuff = nil;
	}
	if ( nodeRef != 0 )
	{
		dsCloseDirNode( nodeRef );
		nodeRef = 0;
	}
	if ( aSearchNodeRef != 0 )
	{
		dsCloseDirNode( aSearchNodeRef );
		aSearchNodeRef = 0;
	}
	if ( aDSRef != 0 )
	{
		dsCloseDirService( aDSRef );
		aDSRef = 0;
	}
	
	return(siResult);
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
//	usage
//
//-----------------------------------------------------------------------------

void
usage(void)
{
	printf("\ndsenableroot:: Enable or disable root user with Directory Services.\n");
	printf("Version %s\n", version);
	printf("Usage: dsenableroot [-d] [-u username] [-p password] [-r rootPassword]\n");
	printf("Example 1: dsenableroot\n");
	printf("Attempt to enable root account.\n");
	printf("Your username will be used.\n");
	printf("Both passwords will be prompted for.\n");
	printf("Example 2: dsenableroot -d -u username\n");
	printf("Attempt to disable root account.\n");
	printf("Only user password will be prompted for.\n");
	printf("In all cases passwords cannot be empty strings.\n");
	printf("\n");
}


int main(int argc, char *argv[])
{
    int				ch;
	char		   *name			= nil;
	char		   *pwd				= nil;
	char		   *rootpwd			= nil;
	char		   *verifyrootpwd	= nil;
	signed long		result			= eDSAuthFailed;
	bool			bEchoName		= false;
	bool			bDisableRootUser= false;
    
	if ( argc == 2 && strcmp(argv[1], "-appleversion") == 0 )
        dsToolAppleVersionExit( argv[0] );
	
	while ((ch = getopt(argc, argv, "u:p:r:dv?h")) != -1) {
        switch (ch) {
        case 'u':
            name = strdup(optarg);
            break;
        case 'p':
            pwd = strdup(optarg);
            break;
        case 'r':
            rootpwd = strdup(optarg);
            break;
        case 'd':
			bDisableRootUser = true;
			break;
        case 'v':
			printf("\ndsenableroot:: Version %s\n", version);
			exit(0);
        case '?':
        case 'h':
        default:
			{
				usage();
				exit(0);
			}
        }
    }
	
	if ( (argc > 1) && (name == nil) && (pwd == nil) && (rootpwd == nil) && (!bDisableRootUser) )
	{
		usage();
		exit(0);
	}
	
	if ( (argc > 2) && (name == nil) && (pwd == nil) && bDisableRootUser )
	{
		usage();
		exit(0);
	}
	
	// get the prompts we still need
	if (name == nil)
	{
		bEchoName = true;
		name = getenv("USER");
		if (name != nil)
		{
			printf("username = %s\n", name);
		}
		else
		{
			printf("***Username <-u username> must be explicitly provided in this shell***\n");
			exit(0);
		}
	}
	
	if (pwd == nil)
	{
		pwd = read_passphrase("user password:", 1);
	}
	
	if ( (rootpwd == nil) && (!bDisableRootUser) )
	{
		rootpwd			= read_passphrase("root password:", 1);
		verifyrootpwd	= read_passphrase("verify root password:", 1);
		
		if (strcmp(rootpwd, verifyrootpwd) != 0)
		{
			printf("\ndsenableroot:: ***Root password was not verified with dual entries.\n");
			exit(0);
		}
	}
	
	if ( (name != nil) && (pwd != nil) )
	{
		if (bDisableRootUser)
		{
			result = DisableRootUser( name, pwd );
		}
		else
		{
			result = EnableRootUser( name, pwd, rootpwd );
		}
		
		free(pwd);
		pwd = nil;
		if (rootpwd)
		{
			free(rootpwd);
			rootpwd = nil;
		}
		
		if (bDisableRootUser)
		{
			if (result == eDSNoErr)
			{
				printf("\ndsenableroot:: ***Successfully disabled root user.\n");
			}
			else
			{
				printf("\ndsenableroot:: ***Failed to disable root user.\n");
			}
		}
		else
		{
			if (result == eDSNoErr)
			{
				printf("\ndsenableroot:: ***Successfully enabled root user.\n");
			}
			else
			{
				printf("\ndsenableroot:: ***Failed to enable root user.\n");
			}
		}
	}
	
	if (!bEchoName && name != nil)
	{
		free(name);
		name = nil;
	}
	
	return 0;
}

