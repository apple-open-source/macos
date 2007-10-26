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
 * @header dscommon
 * Record access methods via the DirectoryService API.
 */

#include <string.h>
#include <CoreFoundation/CoreFoundation.h>

#include "dscommon.h"

const int maxAutoUID	= 5000;
const int maxAutoGID	= 50000;

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
}//read_passphrase

#pragma mark -
#pragma mark DS API Support Routines

bool				singleAttributeValueMissing	(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													char* inRecordType,
													char* inAttributeType,
													char* inAttributeValue,
													SInt32 *outResult,
													bool inVerbose)
{
	bool					bMissing		= false;
	tDataBufferPtr			dataBuff		= nil;
	tContextData			context			= 0;
	tDataListPtr			recType			= nil;
	UInt32					recCount		= 1;
	tDataNodePtr			pAttrType		= nil;
	tDataNodePtr			pPatMatchPtr	= nil;
	
	if (inRecordType == nil)
	{
		if (inVerbose) printf("Null record type\n");
		*outResult = (SInt32) eDSNullRecType;
		return(nil);
	}
	if (inAttributeType == nil)
	{
		if (inVerbose) printf("Null attribute type\n");
		*outResult = (SInt32) eDSNullAttributeType;
		return(nil);
	}
	if (inAttributeValue == nil)
	{
		if (inVerbose) printf("Null attribute value\n");
		*outResult = (SInt32) eDSNullAttributeValue;
		return(nil);
	}
	if (inDSRef == 0)
	{
		if (inVerbose) printf("Null dir reference\n");
		*outResult = (SInt32) eDSInvalidDirRef;
		return(nil);
	}
	if (inDSNodeRef == 0)
	{
		if (inVerbose) printf("Null node reference\n");
		*outResult = (SInt32) eDSInvalidNodeRef;
		return(nil);
	}

	do
	{
		dataBuff = dsDataBufferAllocate( inDSRef, 1024 );
		if ( dataBuff == nil )
		{
			if (inVerbose) printf("dsDataBufferAllocate returned NULL\n");
			break;
		}
		
		recType			= dsBuildListFromStrings( inDSRef, inRecordType, NULL );
		pPatMatchPtr	= dsDataNodeAllocateString( inDSRef, inAttributeValue );
		pAttrType		= dsDataNodeAllocateString( inDSRef, inAttributeType );
		recCount		= 1; // only care about a single first match
		do 
		{
			*outResult = dsDoAttributeValueSearch( inDSNodeRef, dataBuff, recType, pAttrType, eDSExact, pPatMatchPtr, &recCount, &context );
			if (*outResult == eDSBufferTooSmall)
			{
				UInt32 bufSize = dataBuff->fBufferSize;
				if (inVerbose) printf("dsDoAttributeValueSearch returned buffer too small so doubling size of buffer to <%lu>\n", bufSize);
				dsDataBufferDeAllocate( inDSRef, dataBuff );
				dataBuff = nil;
				dataBuff = dsDataBufferAllocate( inDSRef, bufSize * 2 );
				if ( dataBuff == nil )
				{
					if (inVerbose) printf("dsDataBufferAllocate returned NULL\n");
				}
			}
		} while ( ( (*outResult == eDSBufferTooSmall) || ( (*outResult == eDSNoErr) && (recCount == 0) && (context != 0) ) ) && (dataBuff != nil) );
		
		if (recCount < 1)
		{
			if (inVerbose) printf("dsDoAttributeValueSearch found no record\n");
			bMissing = true;
		}
		if (*outResult != eDSNoErr)
		{
			if (inVerbose) printf("dsDoAttributeValueSearch returned the error <%ld>\n", *outResult);
		}

		//always leave the while
		break;
	} while(true);

	if (pAttrType != nil)
	{
		dsDataNodeDeAllocate( inDSRef, pAttrType );
		pAttrType = nil;
	}
	if (pPatMatchPtr != nil)
	{
		dsDataNodeDeAllocate( inDSRef, pPatMatchPtr );
		pAttrType = nil;
	}
	if ( recType != nil )
	{
		dsDataListDeallocate( inDSRef, recType );
		free( recType );
		recType = nil;
	}
	if ( dataBuff != nil )
	{
		dsDataBufferDeAllocate( inDSRef, dataBuff );
		dataBuff = nil;
	}
	
	return(bMissing);
}//singleAttributeValueMissing

char* createNewuid  ( tDirReference inDSRef, tDirNodeReference inDSNodeRef, bool inVerbose)
{
	SInt32		siResult		= eDSNoErr;
	int			numericUID		= 502;
	char		uidValue[32]	= {};
	bool		bNextNotFound	= true;
	char	   *outUID			= nil;
	
	do
	{
		bzero(uidValue, 32);
		sprintf(uidValue, "%d", numericUID);
		if ( singleAttributeValueMissing(inDSRef, inDSNodeRef, kDSStdRecordTypeUsers, kDS1AttrUniqueID, uidValue, &siResult, inVerbose) )
		{
			bNextNotFound = false;
		}
		numericUID++;
	} while ( (bNextNotFound) && (numericUID < maxAutoUID));

	if (numericUID == maxAutoUID)
	{
		if (inVerbose) fprintf(stdout, "Automated addition of uid value stops when values up to <%d> are taken\n", maxAutoUID);
	}
	else
	{
		outUID = strdup(uidValue);
	}
	
	return(outUID);
}//createNewuid

char* createNewgid  ( tDirReference inDSRef, tDirNodeReference inDSNodeRef, bool inVerbose)
{
	SInt32		siResult		= eDSNoErr;
	int			numericGID		= 500;
	char		gidValue[32]	= {};
	bool		bNextNotFound	= true;
	char	   *outGID			= nil;
	
	do
	{
		bzero(gidValue, 32);
		sprintf(gidValue, "%d", numericGID);
		if ( singleAttributeValueMissing(inDSRef, inDSNodeRef, kDSStdRecordTypeGroups, kDS1AttrPrimaryGroupID, gidValue, &siResult, inVerbose) )
		{
			bNextNotFound = false;
		}
		numericGID++;
	} while ( (bNextNotFound) && (numericGID < maxAutoGID));

	if (numericGID == maxAutoGID)
	{
		if (inVerbose) printf("Automated addition of gid value stops when values up to <%d> are taken - please enter in value with argument\n", maxAutoGID);
	}
	else
	{
		outGID = strdup(gidValue);
	}
	
	return(outGID);
}//createNewgid

char* createNewGUID  ( bool inVerbose)
{
	CFUUIDRef       myUUID				= 0;
	CFStringRef     myUUIDString		= NULL;
	char            genUIDValue[100]	= {};
	char		   *outGUID				= nil;

	myUUID			= CFUUIDCreate(kCFAllocatorDefault);
	myUUIDString	= CFUUIDCreateString(kCFAllocatorDefault, myUUID);
	CFStringGetCString(myUUIDString, genUIDValue, 100, kCFStringEncodingASCII);
	CFRelease(myUUID);
	CFRelease(myUUIDString);
	outGUID = strdup(genUIDValue);

	return( outGUID );
}//addRecordParameter

SInt32 addRecordParameter (   tDirReference inDSRef, tDirNodeReference inDSNodeRef,
									tRecordReference inRecordRef, char* inAttrType, char* inAttrValue, bool inVerbose)
{
	SInt32						siResult			= eDSNoErr;
	tDirNodeReference			aDSNodeRef			= 0;
	tDataNode				   *pAttrType			= nil;
	tAttributeValueEntry	   *pAttrValueEntry		= nil;
	UInt32						k					= 0;
	tAttributeEntry			   *pAttrEntry			= nil;
	char					   *guidValue			= nil;
	bool						bExists				= false;
	tDataNode				   *pAttrValue			= nil;
	bool						bAttrFound			= false;
	
	if (inAttrValue == nil)
	{
		if (inVerbose) printf("Null attribute value\n");
		return((SInt32) eDSNullAttributeValue);
	}
	if (inAttrType == nil)
	{
		if (inVerbose) printf("Null attribute type\n");
		return((SInt32) eDSNullAttributeType);
	}
	if (inDSRef == 0)
	{
		if (inVerbose) printf("Null dir reference\n");
		return((SInt32) eDSInvalidDirRef);
	}
	if (inDSNodeRef == 0)
	{
		if (inVerbose) printf("Null node reference\n");
		return((SInt32) eDSInvalidNodeRef);
	}
	if (inRecordRef == 0)
	{
		if (inVerbose) printf("Null record reference\n");
		return((SInt32) eDSInvalidRecordRef);
	}

	do
	{
//TBR rework which status gets propagated up?
		pAttrType = dsDataNodeAllocateString( inDSRef, inAttrType );

		siResult = dsGetRecordAttributeInfo( inRecordRef, pAttrType, &pAttrEntry );
		if ( (siResult == eDSNoErr) && (pAttrEntry != nil) )
		{
			bAttrFound = true;
			//verify the attr value: pAttrEntry->fAttributeSignature.fBufferData
			
			// look at all the attribute values
			for ( k = 1; k <= pAttrEntry->fAttributeValueCount; k++ )
			{
				siResult = dsGetRecordAttributeValueByIndex( inRecordRef, pAttrType, k, &pAttrValueEntry );
				if ( siResult == eDSNoErr )
				{
					if (	( pAttrValueEntry->fAttributeValueData.fBufferData != nil ) &&
							( strcmp(inAttrValue, pAttrValueEntry->fAttributeValueData.fBufferData) == 0 ) )
					{
						bExists = true;
					}
						//if not found then set the valueID: pAttrValueEntry->fAttributeValueID
						//if found we reset the valueID to zero, clean up memory and break here
					siResult = dsDeallocAttributeValueEntry( inDSRef, pAttrValueEntry );
					if (bExists) break;
				}
				pAttrValueEntry = nil;
			} // loop over k -- all attribute values
			siResult = dsDeallocAttributeEntry( inDSRef, pAttrEntry );
			pAttrEntry = nil;
		}

		if (!bExists)
		{
			pAttrValue = dsDataNodeAllocateString( inDSRef, inAttrValue );
			if (bAttrFound)
			{
				siResult = dsAddAttributeValue( inRecordRef, pAttrType, pAttrValue );
			}
			else
			{
				siResult = dsAddAttribute( inRecordRef, pAttrType, nil, pAttrValue );
			}
			dsDataNodeDeAllocate( inDSRef, pAttrValue );
			pAttrValue = nil;
		}//if (!bExists)
		
		//always leave the while
		break;
	} while(true);

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

	return( siResult );
}//addRecordParameter

tRecordReference createAndOpenRecord(tDirReference inDSRef, tDirNodeReference inDSNodeRef, char* inRecordName, char* inRecordType, SInt32 *outResult, bool inVerbose)
{
	tRecordReference		outRecordRef	= 0;
	tDataNode				*pRecName		= nil;
	tDataNode				*pRecType		= nil;

	if (inRecordName == nil)
	{
		if (inVerbose) printf("Null group record name\n");
		*outResult = (SInt32) eDSInvalidRecordName;
		return(0);
	}
	if (inRecordType == nil)
	{
		if (inVerbose) printf("Null record type\n");
		*outResult = (SInt32) eDSInvalidRecordType;
		return(0);
	}
	if (inDSRef == 0)
	{
		if (inVerbose) printf("Null dir reference\n");
		*outResult = (SInt32) eDSInvalidDirRef;
		return(0);
	}
	if (inDSNodeRef == 0)
	{
		if (inVerbose) printf("Null node reference\n");
		*outResult = (SInt32) eDSInvalidNodeRef;
		return(0);
	}

	do
	{
		pRecName = dsDataNodeAllocateString( inDSRef, inRecordName );
		pRecType = dsDataNodeAllocateString( inDSRef, inRecordType );
		
		*outResult = dsCreateRecordAndOpen( inDSNodeRef, pRecType, pRecName, &outRecordRef );
		if (*outResult != eDSNoErr)
		{
			if (inVerbose) printf("dsCreateRecordAndOpen returned the error <%ld>\n", *outResult);
		}
		if (outRecordRef == 0)
		{
			if (inVerbose) printf("dsCreateRecordAndOpen returned no record reference\n");
		}
		
		dsDataNodeDeAllocate( inDSRef, pRecName );
		dsDataNodeDeAllocate( inDSRef, pRecType );
		
		//always leave the while
		break;
	} while(true);

	return( outRecordRef );

}//createAndOpenRecord

SInt32 getAndOutputRecord(tDirReference inDSRef, tDirNodeReference inDSNodeRef, char* inRecordName, char* inRecordType, bool inVerbose)
{
	SInt32					siResult		= eDSNoErr;
	tDirReference			aDSRef			= 0;
	tDataBufferPtr			dataBuff		= nil;
	tContextData			context			= 0;
	tDataListPtr			recName			= nil;
	tDataListPtr			recType			= nil;
	tDataListPtr			attrTypes		= nil;
	UInt32					recCount		= 1;
	tAttributeListRef		attrListRef		= 0;
	tRecordEntry		   *pRecEntry		= nil;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeEntry		   *pAttrEntry		= nil;
	tAttributeValueEntry   *pValueEntry		= nil;
	
	if (inRecordName == nil)
	{
		if (inVerbose) printf("Null group record name\n");
		return((SInt32) eDSInvalidRecordName);
	}
	if (inRecordType == nil)
	{
		if (inVerbose) printf("Null record type\n");
		return((SInt32) eDSInvalidRecordType);
	}
	if (inDSRef == 0)
	{
		if (inVerbose) printf("Null dir reference\n");
		return ((SInt32)eDSInvalidDirRef);
	}
	if (inDSNodeRef == 0)
	{
		if (inVerbose) printf("Null node reference\n");
		return ((SInt32)eDSInvalidNodeRef);
	}

	do
	{
		dataBuff = dsDataBufferAllocate( inDSRef, 1024 );
		if ( dataBuff == nil )
		{
			if (inVerbose) printf("dsDataBufferAllocate returned NULL\n");
			break;
		}
		
		recName = dsBuildListFromStrings( aDSRef, inRecordName, NULL );
		recType = dsBuildListFromStrings( aDSRef, inRecordType, NULL );
		attrTypes = dsBuildListFromStrings( aDSRef, kDSAttributesAll, NULL );
		recCount = 1; // only care about first match
		do 
		{
			siResult = dsGetRecordList( inDSNodeRef, dataBuff, recName, eDSExact, recType,
																	attrTypes, false, &recCount, &context);
			if (siResult == eDSBufferTooSmall)
			{
				UInt32 bufSize = dataBuff->fBufferSize;
				if (inVerbose) printf("dsGetRecordList returned buffer too small so doubling size of buffer to <%lu>\n", bufSize);
				dsDataBufferDeAllocate( aDSRef, dataBuff );
				dataBuff = nil;
				dataBuff = dsDataBufferAllocate( aDSRef, bufSize * 2 );
				if ( dataBuff == nil )
				{
					if (inVerbose) printf("dsDataBufferAllocate returned NULL\n");
				}
			}
		} while ( ( (siResult == eDSBufferTooSmall) || ( (siResult == eDSNoErr) && (recCount == 0) && (context != 0) ) ) && (dataBuff != nil) );
		
		if (recCount < 1)
		{
			if (inVerbose) printf("dsGetRecordList found no group record\n");
		}
		if (siResult != eDSNoErr)
		{
			if (inVerbose) printf("dsGetRecordList returned the error <%ld>\n", siResult);
		}

		if ( (siResult == eDSNoErr) && (recCount > 0) )
		{
			siResult = dsGetRecordEntry( inDSNodeRef, dataBuff, 1, &attrListRef, &pRecEntry );
			if ( (siResult == eDSNoErr) && (pRecEntry != nil) )
			{
				printf("\nRecordname <%s>\n", inRecordName);
				//index starts at one - should have two entries
				unsigned int i = 0;
				unsigned int j = 0;
				printf("%d attribute(s) found\n", (int)(pRecEntry->fRecordAttributeCount));
				for (i = 1; i <= pRecEntry->fRecordAttributeCount; i++)
				{
					siResult = dsGetAttributeEntry( inDSNodeRef, dataBuff, attrListRef, i, &valueRef, &pAttrEntry );
					//need to have at least one value to view
					if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
					{
						printf("Attribute[%d] is <%s>\n", i, pAttrEntry->fAttributeSignature.fBufferData);
						if( pAttrEntry->fAttributeValueCount == 0 ) {
							printf("\t0 value(s) found\n");
						}
						// Get all the attribute values
						for ( j = 1; j<= pAttrEntry->fAttributeValueCount; j++)
						{
							siResult = dsGetAttributeValue( inDSNodeRef, dataBuff, j, valueRef, &pValueEntry );
							//TBR this does not handle any binary data which is not expected in group records anyways
							if ( ( siResult == eDSNoErr ) && ( pValueEntry != NULL ) )
							{
								printf("\tValue[%d] is <%s>\n", j, pValueEntry->fAttributeValueData.fBufferData);
								dsDeallocAttributeValueEntry( aDSRef, pValueEntry );
								pValueEntry = NULL;
							}
							else
							{
								if (inVerbose) printf("dsGetAttributeValue returned the error <%ld>\n", siResult);
							}
						}
					}
					else
					{
						if (inVerbose) printf("dsGetAttributeEntry returned the error <%ld>\n", siResult);
					}
					dsCloseAttributeValueList(valueRef);
					if (pAttrEntry != nil)
					{
						dsDeallocAttributeEntry(aDSRef, pAttrEntry);
						pAttrEntry = nil;
					}
				} //loop over attrs requested
			}//found 1st record entry
			else
			{
				if (inVerbose) printf("dsGetRecordEntry returned the error <%ld>\n", siResult);
			}
			dsCloseAttributeList(attrListRef);
			if (pRecEntry != nil)
			{
				dsDeallocRecordEntry(aDSRef, pRecEntry);
				pRecEntry = nil;
			}
		}// got records returned
		else
		{
			siResult = eDSRecordNotFound;
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
	
	return(siResult);
}//getAndOutputRecord

tDirNodeReference getNodeRef(tDirReference inDSRef, char* inNodename, char* inUsername, char* inPassword, bool inVerbose)
{
	tDirNodeReference		outNodeRef		= 0;
	SInt32					siResult		= eDSNoErr;
	tDataBufferPtr			dataBuff		= nil;
	UInt32					nodeCount		= 0;
	tContextData			context			= 0;
	tDataListPtr			nodeName		= nil;
	tDataNodePtr			authMethod		= nil;
	tDataBufferPtr			authBuff		= nil;
	UInt32					length			= 0;
	char*					ptr				= nil;
	
	do
	{
		if ( inDSRef == 0) break;
		
		dataBuff = dsDataBufferAllocate( inDSRef, 512 );
		if ( dataBuff == nil )
		{
			if (inVerbose) printf("dsDataBufferAllocate returned NULL\n");
			break;
		}
		
		if (inNodename)
		{
			nodeName = dsBuildFromPath( inDSRef, inNodename, "/" );
		}
		
		do
		{
			if (nodeName != nil)
			{
				if (inVerbose) printf("dsFindDirNodes using input nodename\n");
				if (strcmp("/Search", inNodename) == 0)
				{
					siResult = dsFindDirNodes( inDSRef, dataBuff, NULL, eDSAuthenticationSearchNodeName, &nodeCount, &context );
				}
				else
				{
					siResult = dsFindDirNodes( inDSRef, dataBuff, nodeName, eDSExact, &nodeCount, &context );
				}
			}
			else
			{
				if (inVerbose) printf("dsFindDirNodes using local node\n");
				siResult = dsFindDirNodes( inDSRef, dataBuff, NULL, eDSLocalNodeNames, &nodeCount, &context );
			}
			if (siResult == eDSBufferTooSmall)
			{
				UInt32 bufSize = dataBuff->fBufferSize;
				dsDataBufferDeAllocate( inDSRef, dataBuff );
				dataBuff = nil;
				dataBuff = dsDataBufferAllocate( inDSRef, bufSize * 2 );
				if ( dataBuff == nil )
				{
					if (inVerbose) printf("dsDataBufferAllocate returned NULL\n");
				}
			}
		} while ( (siResult == eDSBufferTooSmall) && (dataBuff != nil) );
		if ( siResult != eDSNoErr )
		{
			if (inVerbose) printf("dsFindDirNodes returned the error <%ld>\n", siResult);
			break;
		}
		if ( nodeCount < 1 )
		{
			if (inVerbose) printf("dsFindDirNodes could not find the node\n");
			break;
		}

		if ( nodeName != NULL )
		{
			dsDataListDeallocate( inDSRef, nodeName );
			free( nodeName );
			nodeName = NULL;
		}
		
		siResult = dsGetDirNodeName( inDSRef, dataBuff, 1, &nodeName );
		if ( siResult != eDSNoErr )
		{
			if (inVerbose) printf("dsGetDirNodeName returned the error <%ld>\n", siResult);
			break;
		}

		siResult = dsOpenDirNode( inDSRef, nodeName, &outNodeRef );
		if ( siResult != eDSNoErr )
		{
			if (inVerbose) printf("dsOpenDirNode returned the error <%ld>\n", siResult);
			break;
		}
		if ( nodeName != NULL )
		{
			dsDataListDeallocate( inDSRef, nodeName );
			free( nodeName );
			nodeName = NULL;
		}
		
		if ( (inUsername != nil) && (inPassword != nil) )
		{
			authMethod	= dsDataNodeAllocateString( inDSRef, kDSStdAuthNodeNativeClearTextOK );
			authBuff	= dsDataBufferAllocate( inDSRef, strlen( inUsername ) + strlen( inPassword ) + 10 );
			// 4 byte length + username + null byte + 4 byte length + password + null byte

			length	= strlen( inUsername ) + 1;
			ptr		= authBuff->fBufferData;
			
			memcpy( ptr, &length, 4 );
			ptr += 4;
			authBuff->fBufferLength += 4;
			
			memcpy( ptr, inUsername, length );
			ptr += length;
			authBuff->fBufferLength += length;
			
			length = strlen( inPassword ) + 1;
			memcpy( ptr, &length, 4 );
			ptr += 4;
			authBuff->fBufferLength += 4;
			
			memcpy( ptr, inPassword, length );
			ptr += length;
			authBuff->fBufferLength += length;
			
			siResult = dsDoDirNodeAuth( outNodeRef, authMethod, false, authBuff, dataBuff, NULL );
			
			if( siResult == eDSAuthFailed )
				printf( "Authentication failed.\n" );
			
			if (siResult != eDSNoErr)
			{
				dsCloseDirNode( outNodeRef );
				outNodeRef = 0;
			}

			dsDataNodeDeAllocate( inDSRef, authMethod );
			authMethod = NULL;
			dsDataBufferDeAllocate( inDSRef, authBuff );
			authBuff = NULL;
		}
			
		//always leave the while
		break;
	} while(true);

	if ( dataBuff != nil )
	{
		dsDataBufferDeAllocate( inDSRef, dataBuff );
		dataBuff = nil;
	}
	if ( nodeName != nil )
	{
		dsDataListDeallocate( inDSRef, nodeName );
		free( nodeName );
		nodeName = nil;
	}
	if ( authMethod != nil )
	{
		dsDataNodeDeAllocate( inDSRef, authMethod );
		authMethod = nil;
	}
	if ( authBuff != nil )
	{
		dsDataBufferDeAllocate( inDSRef, authBuff );
		authBuff = nil;
	}
	
	return(outNodeRef);
}//getNodeRef
	
char* getSingleRecordAttribute(tDirReference inDSRef, tDirNodeReference inDSNodeRef, char* inRecordName, char* inRecordType, char* inAttributeType, SInt32 *outResult, bool inVerbose)
{
	char				   *outRecordName   = nil;
	tDirReference			aDSRef			= 0;
	tDataBufferPtr			dataBuff		= nil;
	tContextData			context			= 0;
	tDataListPtr			recName			= nil;
	tDataListPtr			recType			= nil;
	tDataListPtr			attrTypes		= nil;
	UInt32					recCount		= 1;
	tAttributeListRef		attrListRef		= 0;
	tRecordEntry		   *pRecEntry		= nil;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeEntry		   *pAttrEntry		= nil;
	tAttributeValueEntry   *pValueEntry		= nil;
	
	if (inRecordName == nil)
	{
		if (inVerbose) printf("Null group record name\n");
		*outResult = (SInt32) eDSInvalidRecordName;
		return(nil);
	}
	if (inAttributeType == nil)
	{
		if (inVerbose) printf("Null attribute type\n");
		*outResult = (SInt32) eDSInvalidAttributeType;
		return(nil);
	}
	if (inRecordType == nil)
	{
		if (inVerbose) printf("Null record type\n");
		*outResult = (SInt32) eDSInvalidRecordType;
		return(nil);
	}
	if (inDSRef == 0)
	{
		if (inVerbose) printf("Null dir reference\n");
		*outResult = (SInt32) eDSInvalidDirRef;
		return(nil);
	}
	if (inDSNodeRef == 0)
	{
		if (inVerbose) printf("Null node reference\n");
		*outResult = (SInt32) eDSInvalidNodeRef;
		return(nil);
	}

	do
	{
		dataBuff = dsDataBufferAllocate( inDSRef, 1024 );
		if ( dataBuff == nil )
		{
			if (inVerbose) printf("dsDataBufferAllocate returned NULL\n");
			break;
		}
		
		recName = dsBuildListFromStrings( aDSRef, inRecordName, NULL );
		recType = dsBuildListFromStrings( aDSRef, inRecordType, NULL );
		attrTypes = dsBuildListFromStrings( aDSRef, inAttributeType, NULL );
		recCount = 1; // only care about first match
		do 
		{
			*outResult = dsGetRecordList( inDSNodeRef, dataBuff, recName, eDSExact, recType,
																	attrTypes, false, &recCount, &context);
			if (*outResult == eDSBufferTooSmall)
			{
				UInt32 bufSize = dataBuff->fBufferSize;
				if (inVerbose) printf("dsGetRecordList returned buffer too small so doubling size of buffer to <%lu>\n", bufSize);
				dsDataBufferDeAllocate( aDSRef, dataBuff );
				dataBuff = nil;
				dataBuff = dsDataBufferAllocate( aDSRef, bufSize * 2 );
				if ( dataBuff == nil )
				{
					if (inVerbose) printf("dsDataBufferAllocate returned NULL\n");
				}
			}
		} while ( ( (*outResult == eDSBufferTooSmall) || ( (*outResult == eDSNoErr) && (recCount == 0) && (context != 0) ) ) && (dataBuff != nil) );
		
		if (recCount < 1)
		{
			if (inVerbose) printf("dsGetRecordList found no record\n");
		}
		if (*outResult != eDSNoErr)
		{
			if (inVerbose) printf("dsGetRecordList returned the error <%ld>\n", *outResult);
		}

		if ( (*outResult == eDSNoErr) && (recCount > 0) )
		{
			*outResult = dsGetRecordEntry( inDSNodeRef, dataBuff, 1, &attrListRef, &pRecEntry );
			if ( (*outResult == eDSNoErr) && (pRecEntry != nil) )
			{
				//index starts at one - should have two entries
				unsigned int i = 0;
				for (i = 1; i <= pRecEntry->fRecordAttributeCount; i++)
				{
					*outResult = dsGetAttributeEntry( inDSNodeRef, dataBuff, attrListRef, i, &valueRef, &pAttrEntry );
					//need to have at least one value to view
					if ( ( *outResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
					{
						// Get the first attribute value of record name
						if (strcmp(inAttributeType,pAttrEntry->fAttributeSignature.fBufferData) == 0)
						{
							*outResult = dsGetAttributeValue( inDSNodeRef, dataBuff, 1, valueRef, &pValueEntry );
							//TBR this does not handle any binary data which is not expected in a record recordname anyways
							if ( ( *outResult == eDSNoErr ) && ( pValueEntry != NULL ) )
							{
								outRecordName = strdup(pValueEntry->fAttributeValueData.fBufferData);
								dsDeallocAttributeValueEntry( aDSRef, pValueEntry );
								pValueEntry = NULL;
							}
							else
							{
								if (inVerbose) printf("dsGetAttributeValue returned the error <%ld>\n", *outResult);
							}
						}
					}
					else
					{
						if (inVerbose) printf("dsGetAttributeEntry returned the error <%ld>\n", *outResult);
					}
					dsCloseAttributeValueList(valueRef);
					if (pAttrEntry != nil)
					{
						dsDeallocAttributeEntry(aDSRef, pAttrEntry);
						pAttrEntry = nil;
					}
				} //loop over attrs requested
			}//found 1st record entry
			else
			{
				if (inVerbose) printf("dsGetRecordEntry returned the error <%ld>\n", *outResult);
			}
			dsCloseAttributeList(attrListRef);
			if (pRecEntry != nil)
			{
				dsDeallocRecordEntry(aDSRef, pRecEntry);
				pRecEntry = nil;
			}
		}// got records returned
		else if (recCount == 0)
		{
			*outResult = eDSRecordNotFound;
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
	
	return(outRecordName);
}//getSingleRecordAttribute

tRecordReference openRecord(tDirReference inDSRef, tDirNodeReference inDSNodeRef, char* inRecordName, char* inRecordType, SInt32 *outResult, bool inVerbose)
{
	tRecordReference		outRecordRef	= 0;
	tDataNode				*pRecName		= nil;
	tDataNode				*pRecType		= nil;

	if (inRecordName == nil)
	{
		if (inVerbose) printf("Null group record name\n");
		*outResult = (SInt32) eDSInvalidRecordName;
		return(0);
	}
	if (inRecordType == nil)
	{
		if (inVerbose) printf("Null record type\n");
		*outResult = (SInt32) eDSInvalidRecordType;
		return(0);
	}
	if (inDSRef == 0)
	{
		if (inVerbose) printf("Null dir reference\n");
		*outResult = (SInt32) eDSInvalidDirRef;
		return(0);
	}
	if (inDSNodeRef == 0)
	{
		if (inVerbose) printf("Null node reference\n");
		*outResult = (SInt32) eDSInvalidNodeRef;
		return(0);
	}

	do
	{
		pRecName = dsDataNodeAllocateString( inDSRef, inRecordName );
		pRecType = dsDataNodeAllocateString( inDSRef, inRecordType );
		
		*outResult = dsOpenRecord( inDSNodeRef, pRecType, pRecName, &outRecordRef );
		if (*outResult != eDSNoErr)
		{
			if (inVerbose) printf("dsOpenRecord returned the error <%ld>\n", *outResult);
		}
		if (outRecordRef == 0)
		{
			if (inVerbose) printf("dsOpenRecord returned no record reference\n");
		}
		
		dsDataNodeDeAllocate( inDSRef, pRecName );
		dsDataNodeDeAllocate( inDSRef, pRecType );
		
		//always leave the while
		break;
	} while(true);

	return( outRecordRef );

}//openRecord

bool UserIsMemberOfGroup( tDirReference inDSRef, tDirNodeReference inDSNodeRef, const char* shortName, const char* groupName )
{
	bool						isInGroup 			= false;
	tDirStatus					dsStatus			= eDSNoErr;
	tAttributeEntryPtr			attrPtr				= nil;
	tAttributeValueEntryPtr		pValueEntry			= nil;
	UInt32						i					= 0;
	tDataListPtr				attrTypeList		= NULL;
	tDataListPtr				recNames			= NULL;
	tDataListPtr				recTypes			= NULL;
	tDataBufferPtr				dataBuff			= NULL;
	UInt32						curRecCount			= 1;
	tContextData				context				= 0;
	tAttributeListRef			attrListRef			= 0;
	tAttributeValueListRef		attrValueListRef	= 0;
	tRecordEntryPtr				recEntry			= NULL;

	if ( groupName == NULL || shortName == NULL )
	{
		return false;
	}

	attrTypeList = dsBuildListFromStrings( inDSRef, kDSNAttrGroupMembership, nil );
	recNames = dsBuildListFromStrings( inDSRef, groupName, nil );
	recTypes = dsBuildListFromStrings( inDSRef, kDSStdRecordTypeGroups, nil );
	dataBuff = dsDataBufferAllocate( inDSRef, 4096 );

	do
	{
		dsStatus = dsGetRecordList( inDSNodeRef, dataBuff, recNames, eDSExact,
							  recTypes, attrTypeList, false, &curRecCount, &context );
		if ( dsStatus == eDSBufferTooSmall )
		{
			UInt32 buffSize = dataBuff->fBufferSize;
			dsDataBufferDeAllocate( inDSRef, dataBuff );
			dataBuff = NULL;
			dataBuff = dsDataBufferAllocate( inDSRef, buffSize * 2 );
		}
	} while (((dsStatus == eDSNoErr) && (curRecCount == 0) && (context != 0)) || (dsStatus == eDSBufferTooSmall));

	if ( ( dsStatus == eDSNoErr ) && ( curRecCount > 0 ) )
	{
		// now walk through the list of users in this group and look for a match
		dsStatus = dsGetRecordEntry( inDSNodeRef, dataBuff, 1, &attrListRef, &recEntry );
		if (dsStatus == eDSNoErr )
		{
			dsStatus = dsGetAttributeEntry( inDSNodeRef, dataBuff, attrListRef, 1, &attrValueListRef,
								   &attrPtr );
		}

		if ( dsStatus == eDSNoErr )
		{
			for ( i = 1; i <= attrPtr->fAttributeValueCount && (dsStatus == eDSNoErr) && (isInGroup == false); i++ )
			{
				// note that since we only asked for one attribute type (group membership)
				// we can assume that if we got this far that is what we're looking at
				dsStatus = dsGetAttributeValue( inDSNodeRef, dataBuff, i, attrValueListRef, &pValueEntry );

				// now compare the member of the group against the user name we were given
				if ( dsStatus == eDSNoErr && strcmp( pValueEntry->fAttributeValueData.fBufferData, shortName ) == 0 )
					isInGroup = true;
				if ( pValueEntry != nil )
				{
					dsStatus = dsDeallocAttributeValueEntry( inDSRef, pValueEntry );
					pValueEntry = nil;
				}
			}
		}
		if ( attrPtr != NULL ) {
			dsDeallocAttributeEntry( inDSRef, attrPtr );
			attrPtr = NULL;
		}
		if ( attrValueListRef != 0 ) {
			dsCloseAttributeValueList( attrValueListRef );
			attrValueListRef = 0;
		}
		if ( recEntry != NULL ) {
			dsDeallocRecordEntry( inDSRef, recEntry );
			recEntry = NULL;
		}
		if ( attrListRef != 0 ) {
			dsCloseAttributeList( attrListRef );
			attrListRef = 0;
		}
	}
	if ( dataBuff != NULL )
	{
		dsDataBufferDeAllocate( inDSRef, dataBuff );
		dataBuff = NULL;
	}
	if ( context != 0 )
	{
		dsReleaseContinueData( inDSNodeRef, context );
		context = 0;
	}
	if ( attrTypeList != NULL)
	{
		dsDataListDeallocate( inDSRef, attrTypeList );
		free( attrTypeList );
		attrTypeList = NULL;
	}
	if ( recNames != NULL)
	{
		dsDataListDeallocate( inDSRef, recNames );
		free( recNames );
		recNames = NULL;
	}
	if ( recTypes != NULL)
	{
		dsDataListDeallocate( inDSRef, recTypes );
		free( recTypes );
		recTypes = NULL;
	}

	return isInGroup;
}

