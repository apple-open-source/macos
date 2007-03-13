/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 * @header CNiPlugIn
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>	// for struct timespec and gettimeofday()
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <ctype.h>
#include <regex.h>
#include <syslog.h>			// for syslog() to log calls
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <mach/mach_time.h>
#include <sasl.h>
#include <membership.h>

#include <Security/Authorization.h>
#include <PasswordServer/AuthFile.h>
#include <PasswordServer/CAuthFileBase.h>
#include <PasswordServer/CPolicyGlobalXML.h>
#include <PasswordServer/CPolicyXML.h>
#include <DirectoryService/DirServicesUtilsPriv.h>

#include "CNiPlugIn.h"
#include "CNetInfoPlugin.h"
#include "SMBAuth.h"
#include "CBuff.h"
#include "CSharedData.h"
#include "CString.h"
#include "CFile.h"
#include "NiLib2.h"
#include "NiLib3.h"
#include "DSUtils.h"
#include "DirServicesConst.h"
#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesPriv.h"
#include "ServerModuleLib.h"
#include "CRecTypeList.h"
#include "CRecNameList.h"
#include "SharedConsts.h"
#include "PrivateTypes.h"
#include "CPlugInRef.h"
#include "CContinue.h"
#include "CRCCalc.h"
#include "CDataBuff.h"
#include "CLog.h"
#include "TimConditional.h"
#include "netinfo_open.h"
#include "buffer_unpackers.h"
#include "CNiMaps.h"
#include "CNiUtilities.h"
#include "CDSPluginUtils.h"
#include "ServerControl.h"

// Auth server
#ifdef TIM_CLIENT_PRESENT
#include <TimClient/TimClient.h>
#endif

// -- Typedef's -------------------------------------

typedef struct NetInfoAuthAuthorityHandler {
	char* fTag;
	NetInfoAuthAuthorityHandlerProc fHandler;
} NetInfoAuthAuthorityHandler;

#define		kNetInfoAuthAuthorityHandlerProcs		6

static NetInfoAuthAuthorityHandler sNetInfoAuthAuthorityHandlerProcs[ kNetInfoAuthAuthorityHandlerProcs ] =
{
	{ kDSTagAuthAuthorityBasic,				(NetInfoAuthAuthorityHandlerProc)CNiPlugIn::DoBasicAuth },
	{ kDSTagAuthAuthorityLocalWindowsHash,	(NetInfoAuthAuthorityHandlerProc)CNiPlugIn::DoShadowHashAuth },
	{ kDSTagAuthAuthorityShadowHash,		(NetInfoAuthAuthorityHandlerProc)CNiPlugIn::DoShadowHashAuth },
	{ kDSTagAuthAuthorityPasswordServer,	(NetInfoAuthAuthorityHandlerProc)CNiPlugIn::DoPasswordServerAuth },
	{ kDSTagAuthAuthorityDisabledUser,		(NetInfoAuthAuthorityHandlerProc)CNiPlugIn::DoDisabledAuth },
	{ kDSTagAuthAuthorityLocalCachedUser,	(NetInfoAuthAuthorityHandlerProc)CNiPlugIn::DoLocalCachedUserAuth }
};

// --------------------------------------------------------------------------------
//	Globals

CPlugInRef			   *gNINodeRef			= nil;
CContinue			   *gNIContinue			= nil;
time_t					gCheckTimAgainTime	= 0;
double					gAllowNetInfoParentOpenTime = 0;
bool					gTimIsRunning		= false;
static const uInt32		kNodeInfoBuffTag	= 'NInf';
static unsigned int		sHashList;
bool					gbBuildNILocalUserCache	= true;
CFMutableArrayRef		gNILocalNameCache		= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
CFMutableArrayRef		gNILocalRealNameCache	= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
uInt32					gBuildCacheSizeLimit	= 100;
char				   *gLocalNIDataStamp		= NULL;

HashAuthFailedMap			gHashAuthFailedMap;

extern  DSMutexSemaphore   *gNetInfoMutex;
extern  DSMutexSemaphore   *gHashAuthFailedMapLock;
extern  uInt32				gDelayFailedLocalAuthReturnsDeltaInSeconds;
extern	bool				gServerOS;
extern	sInt32				gProcessPID;
extern	int					gCountToCheckLocalDataStamp;


// Enums -----------------------------------------------------------------------------

typedef	enum
{
	eHasAuthMethod			= -7000,
	eNoAuthServer			= -7001,
	eNoAuthMethods			= -7002,
	eAuthMethodNotFound		= -7003
} eAuthValues;


//------------------------------------------------------------------------------------
//	* CNiPlugIn
//------------------------------------------------------------------------------------

CNiPlugIn::CNiPlugIn ( void )
{
	struct stat sb;
	
	if ( gNINodeRef == nil )
	{
		if (gServerOS)
		{
			gNINodeRef = new CPlugInRef( CNiPlugIn::ContextDeallocProc, 1024 );
		}
		else
		{
			gNINodeRef = new CPlugInRef( CNiPlugIn::ContextDeallocProc, 256 );
		}
	}

	if ( gNIContinue == nil )
	{
		if (gServerOS)
		{
			gNIContinue = new CContinue( CNiPlugIn::ContinueDeallocProc, 256 );
		}
		else
		{
			gNIContinue = new CContinue( CNiPlugIn::ContinueDeallocProc, 64 );
		}
	}
	
	fRecData	= new CDataBuff();
	fAttrData	= new CDataBuff();
	fTmpData	= new CDataBuff();
	fHashList	= kNiPluginHashDefaultSet;
	if ( gServerOS && gProcessPID < 100 && stat("/var/db/netinfo/network.nidb",&sb) == 0 )
	{
		gAllowNetInfoParentOpenTime = dsTimestamp() + USEC_PER_SEC * 45;
	}
} // CNiPlugIn


//------------------------------------------------------------------------------------
//	* ~CNiPlugIn
//------------------------------------------------------------------------------------

CNiPlugIn::~CNiPlugIn ( void )
{
	if ( fRecData != nil )
	{
		delete( fRecData );
		fRecData = nil;
	}

	if ( fAttrData != nil )
	{
		delete( fAttrData );
		fAttrData = nil;
	}

	if ( fTmpData != nil )
	{
		delete( fTmpData );
		fTmpData = nil;
	}
} // ~CNiPlugIn


//------------------------------------------------------------------------------------
//	* HandleRequest
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::HandleRequest ( void *inData )
{
	sInt32		siResult	= 0;
	sHeader	   *pMsgHdr		= nil;

	if ( inData == nil )
	{
		return( -8088 );
	}

	pMsgHdr = (sHeader *)inData;

	switch ( pMsgHdr->fType )
	{
		case kReleaseContinueData:
			siResult = ReleaseContinueData( (sReleaseContinueData *)inData );
			break;

		case kOpenDirNode:
			siResult = OpenDirNode( (sOpenDirNode *)inData );
			break;

		case kCloseDirNode:
			siResult = CloseDirNode( (sCloseDirNode *)inData );
			break;

		case kGetDirNodeInfo:
			siResult = GetDirNodeInfo( (sGetDirNodeInfo *)inData );
			break;

		case kGetRecordList:
			siResult = GetRecordList( (sGetRecordList *)inData );
			break;

		case kGetRecordEntry:
			siResult = GetRecordEntry( (sGetRecordEntry *)inData );
			break;

		case kGetAttributeEntry:
			siResult = GetAttributeEntry( (sGetAttributeEntry *)inData );
			break;

		case kGetAttributeValue:
			siResult = GetAttributeValue( (sGetAttributeValue *)inData );
			break;

		case kCloseAttributeList:
			siResult = CloseAttributeList( (sCloseAttributeList *)inData );
			break;

		case kCloseAttributeValueList:
			siResult = CloseAttributeValueList( (sCloseAttributeValueList *)inData );
			break;

		case kOpenRecord:
			siResult = OpenRecord( (sOpenRecord *)inData );
			break;

		case kGetRecordReferenceInfo:
			siResult = GetRecRefInfo( (sGetRecRefInfo *)inData );
			break;

		case kGetRecordAttributeInfo:
			siResult = GetRecAttribInfo( (sGetRecAttribInfo *)inData );
			break;

		case kGetRecordAttributeValueByID:
			siResult = GetRecordAttributeValueByID( (sGetRecordAttributeValueByID *)inData );
			break;

		case kGetRecordAttributeValueByIndex:
			siResult = GetRecAttrValueByIndex( (sGetRecordAttributeValueByIndex *)inData );
			break;

		case kGetRecordAttributeValueByValue:
			siResult = GetRecAttrValueByValue( (sGetRecordAttributeValueByValue *)inData );
			break;

		case kCloseRecord:
			siResult = CloseRecord( (sCloseRecord *)inData );
			break;

		case kSetRecordName:
			siResult = SetRecordName( (sSetRecordName *)inData );
			break;

		case kSetRecordType:
			siResult = SetRecordType( (sSetRecordType *)inData );
			break;

		case kDeleteRecord:
			siResult = DeleteRecord( (sDeleteRecord *)inData );
			break;

		case kCreateRecord:
		case kCreateRecordAndOpen:
			siResult = CreateRecord( (sCreateRecord *)inData );
			break;

		case kAddAttribute:
			siResult = AddAttribute( (sAddAttribute *)inData );
			break;

		case kRemoveAttribute:
			siResult = RemoveAttribute( (sRemoveAttribute *)inData );
			break;

		case kAddAttributeValue:
			siResult = AddAttributeValue( (sAddAttributeValue *)inData );
			break;

		case kRemoveAttributeValue:
			siResult = RemoveAttributeValue( (sRemoveAttributeValue *)inData );
			break;

		case kSetAttributeValue:
			siResult = SetAttributeValue( (sSetAttributeValue *)inData );
			break;

		case kSetAttributeValues:
			siResult = SetAttributeValues( (sSetAttributeValues *)inData );
			break;

		case kDoDirNodeAuth:
			siResult = DoAuthentication( (sDoDirNodeAuth *)inData );
			break;

		case kDoDirNodeAuthOnRecordType:
			siResult = DoAuthenticationOnRecordType( (sDoDirNodeAuthOnRecordType *)inData );
			break;

		case kDoAttributeValueSearch:
		case kDoAttributeValueSearchWithData:
			siResult = DoAttributeValueSearch( (sDoAttrValueSearchWithData *)inData );
			break;

		case kDoMultipleAttributeValueSearch:
		case kDoMultipleAttributeValueSearchWithData:
			siResult = DoMultipleAttributeValueSearch( (sDoMultiAttrValueSearchWithData *)inData );
			break;

		case kDoPlugInCustomCall:
			siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
			break;

		case kFlushRecord:
			siResult = eDSNoErr;
			break;

		default:
			siResult = eNotHandledByThisNode;
			break;
	}

	pMsgHdr->fResult = siResult;

	return( siResult );

} // HandleRequest


//------------------------------------------------------------------------------------
//	* ReleaseContinueData
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::ReleaseContinueData ( sReleaseContinueData *inData )
{
	sInt32	siResult	= eDSNoErr;

	gNetInfoMutex->Wait();
	
	// RemoveItem calls our ContinueDeallocProc to clean up
	if ( gNIContinue->RemoveItem( inData->fInContinueData ) != eDSNoErr )
	{
		siResult = eDSInvalidContext;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // ReleaseContinueData


#pragma mark -
#pragma mark DS API Service Routines
#pragma mark -

//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::OpenDirNode ( sOpenDirNode *inData )
{
	sInt32			siResult		= eDSNoErr;
	char		   *nameStr			= nil;
	char		   *pathStr			= nil;
	char		   *domName			= nil;
	sInt32			timeOutSecs		= 3;
	sNIContextData   *pContext		= nil;

	try
	{
		nameStr = ::dsGetPathFromListPriv( inData->fInDirNodeName, kstrDelimiter );
		if ( nameStr == nil ) throw( (sInt32)eDSNullNodeName );

		pathStr = BuildDomainPathFromName( nameStr ); //what we actually want to use in the netinfo API
		if ( pathStr == nil ) throw( (sInt32)eDSUnknownNodeName );
		
		if ( gAllowNetInfoParentOpenTime != 0 && strcmp( pathStr, kstrParentDotDot ) == 0 )
		{
			if ( dsTimestamp() > gAllowNetInfoParentOpenTime )
			{
				// ok to open
				gAllowNetInfoParentOpenTime = 0;
			}
			else
			{
				throw( (sInt32)eDSOpenNodeFailed );
			}
		}
		
		gNetInfoMutex->Wait();
		siResult = CNetInfoPlugin::SafeOpen( pathStr, timeOutSecs, &domName );
		gNetInfoMutex->Signal();
		if ( siResult != eDSNoErr ) throw( siResult );

		if (::strcmp(domName,"") == 0)
		{
			siResult = eDSOpenNodeFailed;
			if ( siResult != eDSNoErr ) throw( siResult );
		}
		
		pContext = MakeContextData();
		if ( pContext == nil ) throw( (sInt32)eMemoryAllocError );

		pContext->fDomain = nil;				//init to nil since not authenticated connection
		pContext->fDomainName = domName;		//we own this char* now
		pContext->fDomainPath = pathStr;		//we own this char* now
		pathStr = nil;
		//DBGLOG1( kLogPlugin, "CNiPlugIn::OpenDirNode: node name is %s", domName );
		pContext->fUID = inData->fInUID;
		pContext->fEffectiveUID = inData->fInEffectiveUID;
		if (strcmp(pContext->fDomainPath,".") == 0 )
		{
			pContext->bIsLocal = true;
		}

		gNINodeRef->AddItem( inData->fOutNodeRef, pContext );
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	DSFreeString( pathStr );
	DSFreeString( nameStr );

	return( siResult );

} // OpenDirNode


//------------------------------------------------------------------------------------
//	* CloseDirNode
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::CloseDirNode ( sCloseDirNode *inData )
{
	sInt32			siResult		= NI_OK;
	sNIContextData   *pContext		= nil;

	//gNetInfoMutex->Wait(); //don't hold mutex here since leads to deadlock in cleanup

	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		// RemoveItem calls our ContextDeallocProc to clean up
		gNINodeRef->RemoveItem( inData->fInNodeRef );
		gNIContinue->RemoveItems( inData->fInNodeRef );
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	//gNetInfoMutex->Signal();

	return( siResult );

} // CloseDirNode


//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetDirNodeInfo ( sGetDirNodeInfo *inData )
{
	sInt32				siResult		= eDSNoErr;
	uInt32				uiOffset		= 0;
#ifdef TIM_CLIENT_PRESENT
	sInt32				siTimStatus		= eDSNoErr;
	TIMHandle		   *timHndl			= nil;
	Buffer			   *pTmp			= nil;
	Buffer			   *pTimInfo		= nil;
	uInt32				timAuthCount	= 0;
	char			   *pBuffStr		= nil;
	CString			   *pAuthStr		= nil;
	uInt32				timAuthAvail	= 0;
#endif
	uInt32				i				= 0;
	uInt32				authCount		= 0;
	uInt32				uiCntr			= 1;
	uInt32				uiAttrCnt		= 0;
	CAttributeList	   *inAttrList		= nil;
	char			   *pAttrName		= nil;
	char			   *pData			= nil;
	sNIContextData	   *pContext		= nil;
	sNIContextData	   *pAttrContext	= nil;
	char			   *niDomNameStr	= nil;
	tDataList		   *pNodePath		= nil;
	tDataNode		   *pDataNode		= nil;
	CBuff				outBuff;
	time_t				delayedNI		= 0;
	ni_id				niDirID;
	ni_entrylist		niEntryList;
	ni_proplist			niPropList;
	ni_index			niIndex			= 0;


	gNetInfoMutex->Wait();

	try
	{
		if ( inData == nil ) throw( (sInt32)eMemoryError );

		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
		if ( inAttrList == nil ) throw( (sInt32)eDSNullNodeInfoTypeList );
		if (inAttrList->GetCount() == 0) throw( (sInt32)eDSEmptyNodeInfoTypeList );

		siResult = outBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff.SetBuffType( 'Gdni' ); //Cannot use 'StdA' since no tRecordEntry returned
		if ( siResult != eDSNoErr ) throw( siResult );

		fRecData->Clear();
		fAttrData->Clear();

		// Set the record name and type
		fRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) );
		fRecData->AppendString( "dsAttrTypeStandard:DirectoryNodeInfo" );
		fRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) );
		fRecData->AppendString( "DirectoryNodeInfo" );

		while ( inAttrList->GetAttribute( uiCntr++, &pAttrName ) == eDSNoErr )
		{
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDSNAttrAuthMethod ) == 0) )
			{
				fTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				fTmpData->AppendShort( ::strlen( kDSNAttrAuthMethod ) );
				fTmpData->AppendString( kDSNAttrAuthMethod );

				if ( inData->fInAttrInfoOnly == false )
				{
#ifdef TIM_CLIENT_PRESENT
					//if tim is running then we can add some additional auth methods to our list
					if ( IsTimRunning() )
					{
						authCount = 7;

						// call tim to ask what auth methods are available
						//and compare to the ones we might actually use since we determine what are available and
						//we can call whatever tim methods we want to support those methods
						void * aNIDomain = RetrieveNIDomain(pContext);
						if (aNIDomain != NULL)
						{
							timHndl = ::timServerForDomain( aNIDomain );
							if ( timHndl != nil )
							{
								siTimStatus = ::timGetOperations( timHndl, TimOpAuthentication, &pTimInfo );
								if ( siTimStatus == eDSNoErr )
								{
									timAuthCount = ::bufferArrayCount( pTimInfo );
									if ( timAuthCount != 0 )
									{
										for ( i = 0; i < timAuthCount; i++ )
										{
											pTmp = ::bufferArrayBufferAtIndex( pTimInfo, i );
											if ( pTmp != nil )
											{
												pBuffStr = ::bufferToString( pTmp );
												pAuthStr = GetAuthTypeStr( pBuffStr );
												if ( pAuthStr != nil )
												{
													if (strcmp(kDSStdAuth2WayRandom,pAuthStr->GetData()) == 0)
													{
														authCount++;
														timAuthAvail |= 1;
													}
													if (strcmp(kDSStdAuthSMB_NT_Key,pAuthStr->GetData()) == 0)
													{
														authCount++;
														timAuthAvail |= 2;
													}
													if (strcmp(kDSStdAuthSMB_LM_Key,pAuthStr->GetData()) == 0)
													{
														authCount++;
														timAuthAvail |= 4;
													}
													delete( pAuthStr );
													pAuthStr = nil;
												}

												::bufferRelease( pTmp );
												pTmp = nil;

												if ( pBuffStr != nil )
												{
													free( pBuffStr );
													pBuffStr = nil;
												}
											}
										}
									}
								}
								if ( pTimInfo != nil )
								{
									::bufferRelease( pTimInfo );
									pTimInfo = nil;
								}
								::timHandleFree( timHndl );
								timHndl = nil;
							}
						}
						
						fTmpData->AppendShort( authCount );
						if (timAuthAvail & 1)
						{
							fTmpData->AppendLong( ::strlen( kDSStdAuth2WayRandom ) );
							fTmpData->AppendString( kDSStdAuth2WayRandom );
						}
						if (timAuthAvail & 2)
						{
							fTmpData->AppendLong( ::strlen( kDSStdAuthSMB_NT_Key ) );
							fTmpData->AppendString( kDSStdAuthSMB_NT_Key );
						}
						if (timAuthAvail & 4)
						{
							fTmpData->AppendLong( ::strlen( kDSStdAuthSMB_LM_Key ) );
							fTmpData->AppendString( kDSStdAuthSMB_LM_Key );
						}
					}
					else //we have our own internal auth methods
#endif
					{
						authCount = 7;
						fTmpData->AppendShort( authCount );
					}
					//for either tim or not these methods are available
					fTmpData->AppendLong( ::strlen( kDSStdAuthClearText ) );
					fTmpData->AppendString( kDSStdAuthClearText );
					fTmpData->AppendLong( ::strlen( kDSStdAuthCrypt ) );
					fTmpData->AppendString( kDSStdAuthCrypt );
					fTmpData->AppendLong( ::strlen( kDSStdAuthSetPasswd ) );
					fTmpData->AppendString( kDSStdAuthSetPasswd );
					fTmpData->AppendLong( ::strlen( kDSStdAuthChangePasswd ) );
					fTmpData->AppendString( kDSStdAuthChangePasswd );
					fTmpData->AppendLong( ::strlen( kDSStdAuthSetPasswdAsRoot ) );
					fTmpData->AppendString( kDSStdAuthSetPasswdAsRoot );
					fTmpData->AppendLong( ::strlen( kDSStdAuthNodeNativeClearTextOK ) );
					fTmpData->AppendString( kDSStdAuthNodeNativeClearTextOK );
					fTmpData->AppendLong( ::strlen( kDSStdAuthNodeNativeNoClearText ) );
					fTmpData->AppendString( kDSStdAuthNodeNativeNoClearText );

				}
				else
				{
					fTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				fAttrData->AppendLong( fTmpData->GetLength() );
				fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

				// Clear the temp block
				fTmpData->Clear();
			}
			
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrReadOnlyNode ) == 0) )
			{
				fTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				fTmpData->AppendShort( ::strlen( kDS1AttrReadOnlyNode ) );
				fTmpData->AppendString( kDS1AttrReadOnlyNode );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count
					fTmpData->AppendShort( 1 );

					//possible for a node to be ReadOnly, ReadWrite, WriteOnly
					//note that ReadWrite does not imply fully readable or writable
					
					// Add the root node as an attribute value
					fTmpData->AppendLong( ::strlen( "ReadWrite" ) );
					fTmpData->AppendString( "ReadWrite" );

				}
				else
				{
					fTmpData->AppendShort( 0 );
				}

				// Add the attribute length and data
				fAttrData->AppendLong( fTmpData->GetLength() );
				fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

				// Clear the temp block
				fTmpData->Clear();
			}
				 
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDSNAttrNodePath ) == 0) )
			{
				DBGLOG( kLogPlugin, "CNiPlugIn::GetDirNodeInfo: Try to retrieve a netinfo domain path from netinfo_domainname" );
				// don't call RetrieveNIDomain since we can't pass the authenticated domain pointer to netinfo_domainname
				// since the netinfo_open pkg code does not maintain authenticated connections
				void *aNIDomain = RetrieveSharedNIDomain(pContext);
				if (aNIDomain != NULL)
				{
					delayedNI = time(nil) + 2; //normally netinfo_domainname will complete in under 2 secs
					niDomNameStr = netinfo_domainname( aNIDomain );
					if ( delayedNI < time(nil) )
					{
						if (pContext->fDomainName != nil)
						{
							syslog(LOG_ALERT,"GetDirNodeInfo::Call to netinfo_domainname was with argument domain name: %s and lasted %d seconds.", pContext->fDomainName, (uInt32)(2 + time(nil) - delayedNI));
							DBGLOG2( kLogPlugin, "CNiPlugIn::GetDirNodeInfo: Call to netinfo_domainname was with argument domain name: %s and lasted %d seconds.", pContext->fDomainName, (uInt32)(2 + time(nil) - delayedNI) );
						}
						else
						{
							syslog(LOG_ALERT,"GetDirNodeInfo::Call to netinfo_domainname lasted %d seconds.", (uInt32)(2 + time(nil) - delayedNI));
							DBGLOG1( kLogPlugin, "CNiPlugIn::GetDirNodeInfo: Call to netinfo_domainname lasted %d seconds.", (uInt32)(2 + time(nil) - delayedNI) );
						}
						if (niDomNameStr != nil)
						{
							syslog(LOG_ALERT,"GetDirNodeInfo::Call to netinfo_domainname returned domain name: %s.", niDomNameStr);
							DBGLOG1( kLogPlugin, "CNiPlugIn::GetDirNodeInfo: Call to netinfo_domainname returned domain name: %s.", niDomNameStr );
						}
					}
					if ( niDomNameStr == nil )
					{
						syslog(LOG_ALERT,"GetDirNodeInfo::Call to netinfo_domainname failed or returned nil name.");
						DBGLOG( kLogPlugin, "CNiPlugIn::GetDirNodeInfo: Call to netinfo_domainname failed or returned nil name." );
						//here we force success from the netinfo_domainname call
						niDomNameStr = (char *) calloc(1,2);
						strcpy(niDomNameStr,"/");
					}
				}
				else
				{
					if (pContext->fDomainName != nil)
					{
						syslog(LOG_ALERT,"GetDirNodeInfo::Call to netinfo_domainname was with argument domain name: %s but failed to connect to domain.", pContext->fDomainName );
						DBGLOG1( kLogPlugin, "CNiPlugIn::GetDirNodeInfo: Call to netinfo_domainname was with argument domain name: %s but failed to connect to domain.", pContext->fDomainName );
					}
					throw( (sInt32)eDSInvalidSession );
				}

				if (niDomNameStr != nil)
				{
					DBGLOG1( kLogPlugin, "CNiPlugIn::GetDirNodeInfo: Used netinfo domain path value of <%s>", niDomNameStr );
					fTmpData->Clear();

					uiAttrCnt++;

					// Append the attribute name
					fTmpData->AppendShort( sizeof( kDSNAttrNodePath ) - 1 );
					fTmpData->AppendString( kDSNAttrNodePath );

					if ( inData->fInAttrInfoOnly == false )
					{
						// If this is the root node
						if ( ::strcmp( niDomNameStr, "/" ) == 0 )
						{
							// Attribute count of 1
							fTmpData->AppendShort( 2 );

							// Add the kstrNetInfoName as an attribute value
							fTmpData->AppendLong( sizeof( kstrNetInfoName ) - 1 );
							fTmpData->AppendString( kstrNetInfoName );

							if ( ::strcmp( pContext->fDomainName, kstrDefaultLocalNodeName ) == 0 )
							{
								// stay consistent with the default local node NodeName
								// Add "DefaultLocalNode" as an attribute value
								fTmpData->AppendLong( sizeof( kstrDefaultLocalNode ) - 1 );
								fTmpData->AppendString( kstrDefaultLocalNode );
							}
							else
							{
								// Add the kstrRootOnly as an attribute value
								fTmpData->AppendLong( sizeof( kstrRootOnly ) - 1 );
								fTmpData->AppendString( kstrRootOnly );
							}
						}
						else
						{
							// Attribute count of (list count) + 2
							pNodePath = ::dsBuildFromPathPriv( niDomNameStr, "/" );
							if ( pNodePath == nil ) throw( (sInt32)eMemoryAllocError );

							fTmpData->AppendShort( ::dsDataListGetNodeCountPriv( pNodePath ) + 2 );

							// Add the kstrNetInfoName as an attribute value
							fTmpData->AppendLong( sizeof( kstrNetInfoName ) - 1 );
							fTmpData->AppendString( kstrNetInfoName );

							// Add the kstrRootOnly as an attribute value
							fTmpData->AppendLong( sizeof( kstrRootOnly ) - 1 );
							fTmpData->AppendString( kstrRootOnly );

							i = 1;
							while ( ::dsDataListGetNodeAllocPriv( pNodePath, i, &pDataNode ) == eDSNoErr )
							{
								fTmpData->AppendLong( ::strlen( pDataNode->fBufferData ) );
								fTmpData->AppendString( pDataNode->fBufferData );

								::dsDataBufferDeallocatePriv( pDataNode );
								pDataNode = nil;
								i++;
							}

							::dsDataListDeallocatePriv( pNodePath );
							free( pNodePath );
							pNodePath = nil;
						}
					}
					else
					{
						fTmpData->AppendShort( 0 );
					}

					// Add the attribute length and data
					fAttrData->AppendLong( fTmpData->GetLength() );
					fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

					// Clear the temp block
					fTmpData->Clear();
					DSFreeString(niDomNameStr);
				}
			}
			
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDSNAttrRecordType ) == 0) )
			{
				fTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				fTmpData->AppendShort( ::strlen( kDSNAttrRecordType ) );
				fTmpData->AppendString( kDSNAttrRecordType );

				if ( inData->fInAttrInfoOnly == false )
				{
					void * aNIDomain = RetrieveNIDomain(pContext);
					if (aNIDomain != NULL)
					{
						int valueCount = 0;
						NI_INIT( &niDirID );
						NI_INIT( &niEntryList );
						siResult = ::ni_pathsearch( aNIDomain, &niDirID, "/" );
						if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
						siResult = ::ni_list( aNIDomain, &niDirID, "name", &niEntryList );
						if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
						
						if ( niEntryList.ni_entrylist_val == NULL )
						{
							niEntryList.ni_entrylist_len = 0;
						}
						for ( i = 0; i < niEntryList.ni_entrylist_len; i++ )
						{
							char* dsType = MapNetInfoRecToDSType(niEntryList.ni_entrylist_val[i].names->ni_namelist_val[0]);
							
							if (dsType != NULL) 
							{
								if (strncmp(dsType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
								{
									valueCount++;
								}
								delete( dsType );
								dsType = NULL;
							}
						}
						
						fTmpData->AppendShort( valueCount );

						for ( i = 0; i < niEntryList.ni_entrylist_len; i++ )
						{
							char* dsType = MapNetInfoRecToDSType(niEntryList.ni_entrylist_val[i].names->ni_namelist_val[0]);
							
							if (dsType != NULL)
							{
								if (strncmp(dsType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
								{
									fTmpData->AppendLong( ::strlen( dsType ) );
									fTmpData->AppendString( dsType );
								}
								delete( dsType );
								dsType = NULL;
							}
						}
						
						if ( niEntryList.ni_entrylist_len > 0 )
						{
							ni_entrylist_free(&niEntryList);
						}
					}
					else
					{
						throw( (sInt32)eDSInvalidSession );
					}
				}
				else
				{
					fTmpData->AppendShort( 0 );
				}

				// Add the attribute length and data
				fAttrData->AppendLong( fTmpData->GetLength() );
				fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

				// Clear the temp block
				fTmpData->Clear();
			}
			
			

			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDSNAttrSubNodes ) == 0) )
			{
				fTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				fTmpData->AppendShort( ::strlen( kDSNAttrSubNodes ) );
				fTmpData->AppendString( kDSNAttrSubNodes );

				if ( inData->fInAttrInfoOnly == false )
				{
					int valueCount = 0;
					set<string> nodeNames;
					set<string>::const_iterator iter;
					
					siResult = this->GetSubNodes( pContext, nodeNames );
				
					valueCount = nodeNames.size();
					fTmpData->AppendShort( valueCount );

					for (iter = nodeNames.begin(); iter != nodeNames.end(); ++iter) 
					{
						const char* subnode = (*iter).c_str();
						fTmpData->AppendLong( ::strlen( subnode ) );
						fTmpData->AppendString( subnode );
					}
				}
				else
				{
					fTmpData->AppendShort( 0 );
				}

				// Add the attribute length and data
				fAttrData->AppendLong( fTmpData->GetLength() );
				fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

				// Clear the temp block
				fTmpData->Clear();
			}
			
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrDataStamp ) == 0) )
			{
				fTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				fTmpData->AppendShort( ::strlen( kDS1AttrDataStamp ) );
				fTmpData->AppendString( kDS1AttrDataStamp );

				if ( inData->fInAttrInfoOnly == false )
				{
					void * aNIDomain = RetrieveNIDomain(pContext);
					if (aNIDomain != NULL)
					{
						// Attribute value count
						fTmpData->AppendShort( 1 );

						NI_INIT( &niPropList );
						ni_statistics(aNIDomain, &niPropList);
						niIndex = ::ni_proplist_match( niPropList, "checksum", NULL );
						if ( niIndex == NI_INDEX_NULL || niPropList.nipl_val[ niIndex ].nip_val.ninl_len == 0 )
						{
							fTmpData->AppendLong( ::strlen( "0" ) );
							fTmpData->AppendString( "0" );
						}
						else
						{
							fTmpData->AppendLong( ::strlen( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] ) );
							fTmpData->AppendString( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] );
						}
						::ni_proplist_free( &niPropList );
					}
					else
					{
						throw( (sInt32)eDSInvalidSession );
					}
				}
				else
				{
					fTmpData->AppendShort( 0 );
				}

				// Add the attribute length and data
				fAttrData->AppendLong( fTmpData->GetLength() );
				fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

				// Clear the temp block
				fTmpData->Clear();
			}
				 
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, "dsAttrTypeStandard:TrustInformation" ) == 0) )
			{
				fTmpData->Clear();
				
				uiAttrCnt++;
				
				// Append the attribute name
				fTmpData->AppendShort( ::strlen( "dsAttrTypeStandard:TrustInformation" ) );
				fTmpData->AppendString( "dsAttrTypeStandard:TrustInformation" );
				
				if ( inData->fInAttrInfoOnly == false )
				{
					if( pContext != NULL && pContext->bIsLocal )
					{
						// Attribute value count
						fTmpData->AppendShort( 1 );
						
						fTmpData->AppendLong( ::strlen( "FullTrust" ) );
						fTmpData->AppendString( "FullTrust" );
					}
					else
					{
						// Attribute value count
						fTmpData->AppendShort( 1 );
						fTmpData->AppendLong( ::strlen( "Anonymous" ) );
						fTmpData->AppendString( "Anonymous" );
					}
				}
				else
				{
					fTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length and data
				fAttrData->AppendLong( fTmpData->GetLength() );
				fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );
				
				// Clear the temp block
				fTmpData->Clear();
			}
		}

		fRecData->AppendShort( uiAttrCnt );
		fRecData->AppendBlock( fAttrData->GetData(), fAttrData->GetLength() );

		outBuff.AddData( fRecData->GetData(), fRecData->GetLength() );
		inData->fOutAttrInfoCount = uiAttrCnt;

		pData = outBuff.GetDataBlock( 1, &uiOffset );
		if ( pData != nil )
		{
			pAttrContext = MakeContextData();
			if ( pAttrContext == nil ) throw( (sInt32)eMemoryAllocError );

		//add to the offset for the attr list the length of the GetDirNodeInfo fixed record labels
//		record length = 4
//		fRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) ); = 2
//		fRecData->AppendString( "dsAttrTypeStandard:DirectoryNodeInfo" ); = 36
//		fRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) ); = 2
//		fRecData->AppendString( "DirectoryNodeInfo" ); = 17
//		total adjustment = 4 + 2 + 36 + 2 + 17 = 61

			pAttrContext->offset = uiOffset + 61;

			gNINodeRef->AddItem( inData->fOutAttrListRef, pAttrContext );
		}
		else
		{
			siResult = eDSBufferTooSmall;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	if ( inAttrList != nil )
	{
		delete( inAttrList );
		inAttrList = nil;
	}

	return( siResult );

} // GetDirNodeInfo


//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetAttributeEntry ( sGetAttributeEntry *inData )
{
	sInt32					siResult			= eDSNoErr;
	uInt16					usAttrTypeLen		= 0;
	uInt16					usAttrCnt			= 0;
	uInt32					usAttrLen			= 0;
	uInt16					usValueCnt			= 0;
	uInt32					usValueLen			= 0;
	uInt32					i					= 0;
	uInt32					uiIndex				= 0;
	uInt32					uiAttrEntrySize		= 0;
	uInt32					uiOffset			= 0;
	uInt32					uiTotalValueSize	= 0;
	uInt32					offset				= 0;
	uInt32					buffSize			= 0;
	uInt32					buffLen				= 0;
	char				   *p			   		= nil;
	char				   *pAttrType	   		= nil;
	tDataBuffer			   *pDataBuff			= nil;
	tAttributeValueListRef	attrValueListRef	= 0;
	tAttributeEntryPtr		pAttribInfo			= nil;
	sNIContextData		   *pAttrContext		= nil;
	sNIContextData		   *pValueContext		= nil;

	try
	{
		if ( inData  == nil ) throw( (sInt32)eMemoryError );

		pAttrContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInAttrListRef );
		if ( pAttrContext  == nil ) throw( (sInt32)eDSBadContextData );

		uiIndex = inData->fInAttrInfoIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );
				
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (sInt32)eDSNullDataBuff );
		
		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pAttrContext->offset;
		offset	= pAttrContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the attribute count
		::memcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt) throw( (sInt32)eDSInvalidIndex );

		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		// Skip to the attribute that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the attribute
			::memcpy( &usAttrLen, p, 4 );

			// Move the offset past the length word and the length of the data
			p		+= 4 + usAttrLen;
			offset	+= 4 + usAttrLen;
		}

		// Get the attribute offset
		uiOffset = offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute block
		::memcpy( &usAttrLen, p, 4 );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		::memcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
		
		for ( i = 0; i < usValueCnt; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
			
			uiTotalValueSize += usValueLen;
		}

		uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kBuffPad;
		pAttribInfo = (tAttributeEntry *)::calloc( 1, uiAttrEntrySize );

		pAttribInfo->fAttributeValueCount				= usValueCnt;
		pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
		pAttribInfo->fAttributeValueMaxSize				= 512;				// KW this is never used anywhere
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		attrValueListRef = inData->fOutAttrValueListRef;

		pValueContext = MakeContextData();
		if ( pValueContext  == nil ) throw( (sInt32)eMemoryAllocError );

		pValueContext->offset = uiOffset;

		gNINodeRef->AddItem( inData->fOutAttrValueListRef, pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetRecordEntry
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetRecordEntry ( sGetRecordEntry *inData )
{
	sInt32					siResult		= eDSNoErr;
	uInt32					uiIndex			= 0;
	uInt32					uiCount			= 0;
	uInt32					uiOffset		= 0;
	uInt32					uberOffset		= 0;
	char 				   *pData			= nil;
	tRecordEntryPtr			pRecEntry		= nil;
	sNIContextData		   *pContext		= nil;
	CBuff					inBuff;
	uInt32					offset			= 0;
	uInt16					usTypeLen		= 0;
	char				   *pRecType		= nil;
	uInt16					usNameLen		= 0;
	char				   *pRecName		= nil;
	uInt16					usAttrCnt		= 0;
	uInt32					buffLen			= 0;

	try
	{
		if ( inData  == nil ) throw( (sInt32)eMemoryError );
		if ( inData->fInOutDataBuff  == nil ) throw( (sInt32)eDSEmptyBuffer );
		if (inData->fInOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

		siResult = inBuff.Initialize( inData->fInOutDataBuff );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = inBuff.GetDataBlockCount( &uiCount );
		if ( siResult != eDSNoErr ) throw( siResult );

		uiIndex = inData->fInRecEntryIndex;
		if ((uiIndex > uiCount) || (uiIndex == 0)) throw( (sInt32)eDSInvalidIndex );

		pData = inBuff.GetDataBlock( uiIndex, &uberOffset );
		if ( pData  == nil ) throw( (sInt32)eDSCorruptBuffer );

		//assume that the length retrieved is valid
		buffLen = inBuff.GetDataBlockLength( uiIndex );
		
		// Skip past this same record length obtained from GetDataBlockLength
		pData	+= 4;
		offset	= 0; //buffLen does not include first four bytes

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record type
		::memcpy( &usTypeLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecType = pData;
		
		pData	+= usTypeLen;
		offset	+= usTypeLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record name
		::memcpy( &usNameLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecName = pData;
		
		pData	+= usNameLen;
		offset	+= usNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute count
		::memcpy( &usAttrCnt, pData, 2 );

		pRecEntry = (tRecordEntry *)::calloc( 1, sizeof( tRecordEntry ) + usNameLen + usTypeLen + 4 + kBuffPad );

		pRecEntry->fRecordNameAndType.fBufferSize	= usNameLen + usTypeLen + 4 + kBuffPad;
		pRecEntry->fRecordNameAndType.fBufferLength	= usNameLen + usTypeLen + 4;

		// Add the record name length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData, &usNameLen, 2 );
		uiOffset += 2;

		// Add the record name
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecName, usNameLen );
		uiOffset += usNameLen;

		// Add the record type length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &usTypeLen, 2 );

		// Add the record type
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecType, usTypeLen );

		pRecEntry->fRecordAttributeCount = usAttrCnt;

		pContext = MakeContextData();
		if ( pContext  == nil ) throw( (sInt32)eMemoryAllocError );

		pContext->offset = uberOffset + offset + 4;	// context used by next calls of GetAttributeEntry
													// include the four bytes of the buffLen
		
		gNINodeRef->AddItem( inData->fOutAttrListRef, pContext );

		inData->fOutRecEntryPtr = pRecEntry;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetRecordEntry


//------------------------------------------------------------------------------------
//	* GetRecordList
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetRecordList ( sGetRecordList *inData )
{
	sInt32					siResult		= eDSNoErr;
	uInt32					uiTotal			= 0;
	uInt32					uiCount			= 0;
	char				   *pRecName		= nil;
	char				   *pRecType		= nil;
	char				   *pNIRecType		= nil;
	bool					bAttribOnly		= false;
	tDirPatternMatch		pattMatch		= eDSNoMatch1;
	CRecNameList		   *cpRecNameList	= nil;
	CRecTypeList		   *cpRecTypeList	= nil;
	CAttributeList		   *cpAttrTypeList 	= nil;
	sNIContextData		   *pContext		= nil;
	sNIContinueData		   *pContinue		= nil;
	CBuff				   *outBuff			= nil;
	bool					bBuffFull			= false;
	bool					separateRecTypes	= false;
	uInt32					countDownRecTypes	= 0;

	try
	{
		// Verify all the parameters
		if ( inData == nil ) throw( (sInt32)eMemoryError );
		if ( inData->fInDataBuff == nil ) throw( (sInt32)eDSEmptyBuffer );
		if (inData->fInDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

		if ( inData->fInRecNameList == nil ) throw( (sInt32)eDSEmptyRecordNameList );
		if ( inData->fInRecTypeList == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
		if ( inData->fInAttribTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );

		// Node context data
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		if ( inData->fIOContinueData == nil )
		{
			pContinue = (sNIContinueData *)::calloc( sizeof( sNIContinueData ), sizeof( char ) );

			gNIContinue->AddItem( pContinue, inData->fInNodeRef );

			pContinue->fRecNameIndex	= 1;
			pContinue->fAllRecIndex		= 0;
			pContinue->fTotalRecCount	= 0;
			pContinue->fMultiMapIndex	= 0;
			pContinue->fRecTypeIndex	= 1;
			pContinue->fAttrIndex		= 1;
			pContinue->fLimitRecSearch	= 0;
			
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pContinue
			if (inData->fOutRecEntryCount >= 0)
			{
				pContinue->fLimitRecSearch = inData->fOutRecEntryCount;
			}
		}
		else
		{
			pContinue = (sNIContinueData *)inData->fIOContinueData;
			if ( gNIContinue->VerifyItem( pContinue ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
		}

		inData->fIOContinueData		= nil;
		//return zero if we don't find anything
		inData->fOutRecEntryCount	= 0;

		outBuff = new CBuff();
		if ( outBuff == nil ) throw( (sInt32)eMemoryError );

		siResult = outBuff->Initialize( inData->fInDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff->GetBuffStatus();
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff->SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw( siResult );

		// Get the record name list
		cpRecNameList = new CRecNameList( inData->fInRecNameList );
		if ( cpRecNameList == nil ) throw( (sInt32)eDSEmptyRecordNameList );
		if (cpRecNameList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordNameList );

		// Get the record pattern match
		pattMatch = inData->fInPatternMatch;
		siResult = VerifyPatternMatch( pattMatch );
		if ( siResult != eDSNoErr ) throw( (sInt32)eDSInvalidPatternMatchType );

		// Get the record type list
		cpRecTypeList = new CRecTypeList( inData->fInRecTypeList );
		if ( cpRecTypeList == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
		if (cpRecTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordTypeList );
		//save the number of rec types here to use in separating the buffer data
		countDownRecTypes = cpRecTypeList->GetCount() - pContinue->fRecTypeIndex + 1;

		// Get the attribute list
		cpAttrTypeList = new CAttributeList( inData->fInAttribTypeList );
		if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
		if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );

		// Get the attribute info only flag
		bAttribOnly = inData->fInAttribInfoOnly;

//KW need to be smarter on exiting from the limit search condition ie. stop calling into more record types and names
// if the limit has already been reached ie. check in this routine as well before continuing the search
		// get records of these types
		while ( ( cpRecTypeList->GetAttribute( pContinue->fRecTypeIndex, &pRecType ) == eDSNoErr ) &&
				(!bBuffFull) && (!separateRecTypes) )
		{
			// Do we support this record type?
			//	- There could be a whole list so don't quit if we encounter a bad one.

			pNIRecType = MapRecToNetInfoType( pRecType );
			//code here assumes that there is a single mapping possible
			if ( pNIRecType != nil )
			{
				// Get the records of this type and these names
				while ( (cpRecNameList->GetAttribute( pContinue->fRecNameIndex, &pRecName ) == eDSNoErr) &&
						(siResult == eDSNoErr) && (!bBuffFull) )
				{
					bBuffFull = false;
					if ( ::strcmp( pRecName, kDSRecordsAll ) == 0 )
					{
						siResult = GetAllRecords(	(const char *)pRecType,
													pNIRecType,
													cpAttrTypeList,
													pContinue,
													pContext,
													bAttribOnly,
													outBuff,
													uiCount );
					}
					else
					{
						siResult = GetTheseRecords ( pRecName,
													(const char *)pRecType,
													pNIRecType,
													pattMatch,
													cpAttrTypeList,
													pContext,
													bAttribOnly,
													outBuff,
													pContinue,
													uiCount );
					}

					//outBuff->GetDataBlockCount( &uiCount );
					//cannot use this as there may be records added from different record names
					//at which point the first name will fill the buffer with n records and
					//uiCount is reported as n but as the second name fills the buffer with m MORE records
					//the statement above will return the total of n+m and add it to the previous n
					//so that the total erroneously becomes 2n+m and not what it should be as n+m
					//therefore uiCount is extracted directly out of the GetxxxRecord(s) calls

					if ( siResult == CBuff::kBuffFull )
					{
						bBuffFull = true;
						inData->fIOContinueData = pContinue;

						// check to see if buffer is empty and no entries added
						// which implies that the buffer is too small
						if ( ( uiCount == 0 ) && ( uiTotal == 0 ) )
						{
							throw( (sInt32)eDSBufferTooSmall );
						}
						uiTotal += uiCount;
		
						inData->fOutRecEntryCount = uiTotal;
						outBuff->SetLengthToSize();

						siResult = eDSNoErr;
					}
					else if ( siResult == eDSNoErr )
					{
						uiTotal += uiCount;
						pContinue->fRecNameIndex++;
					}

				} // while loop over record names
				delete( pNIRecType );
				pNIRecType = nil;

			}
			else
			{
				siResult = eDSInvalidRecordType;
			}

			if ( !bBuffFull )
			{
				pRecType = nil;
				pContinue->fRecTypeIndex++;
				pContinue->fRecNameIndex = 1;
				//KW? here we decide to exit with data full of the current type of records
				// and force a good exit with the data we have so we can come back for the next rec type
				separateRecTypes = true;
				//set continue since there may be more data available
				inData->fIOContinueData = pContinue;
				//make sure that the ni_entry_list is cleared for the previous record type
				if ( pContinue->fNIEntryList.ni_entrylist_len > 0 )
				{
					::ni_entrylist_free(&(pContinue->fNIEntryList));
					pContinue->fNIEntryList.ni_entrylist_len = 0;
				}
				siResult = eDSNoErr;

				//however if this was the last rec type then there will be no more data
				// check the number of rec types left
				countDownRecTypes--;
				if ( countDownRecTypes == 0 )
				{
					inData->fIOContinueData = nil;
				}
			}
			
		} // while loop over record types

		if ( (siResult == eDSNoErr) && (!bBuffFull) )
		{
			if ( uiTotal == 0 )
			{
				//KW to remove "if statement" and "siResult = eDSRecordNotFound" altogether
				// see 2531386  dsGetRecordList should not return an error if record not found
				//if ( ( inData->fIOContinueData == nil ) && ( pContinue->fTotalRecCount == 0) )
				//{
					//only set the record not found if no records were found in any of the record types
					//and this is the last record type looked for
					//siResult = eDSRecordNotFound;
				//}
				outBuff->ClearBuff();
			}
			else
			{
				outBuff->SetLengthToSize();
			}

			inData->fOutRecEntryCount = uiTotal;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( (inData->fIOContinueData == nil) && (pContinue != nil) )
	{
		// we've decided not to return continue data, so we should clean up
		gNetInfoMutex->Wait();
		gNIContinue->RemoveItem( pContinue );
		gNetInfoMutex->Signal();
		pContinue = nil;
	}

	if ( cpRecNameList != nil )
	{
		delete( cpRecNameList );
		cpRecNameList = nil;
	}

	if ( cpRecTypeList != nil )
	{
		delete( cpRecTypeList );
		cpRecTypeList = nil;
	}

	if ( cpAttrTypeList != nil )
	{
		delete( cpAttrTypeList );
		cpAttrTypeList = nil;
	}

	if ( outBuff != nil )
	{
		delete( outBuff );
		outBuff = nil;
	}

	if ( pNIRecType != nil )
	{
		delete( pNIRecType );
		pNIRecType = nil;
	}

	return( siResult ); 

} // GetRecordList


//------------------------------------------------------------------------------------
//	* GetAllRecords
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetAllRecords (	const char		*inRecType,
									char			*inNI_RecType,
									CAttributeList	*inAttrTypeList,
									sNIContinueData	*inContinue,
									sNIContextData  *inContext,
									bool			inAttrOnly,
									CBuff			*inBuff,
									uInt32			&outRecCount )
{
	sInt32			siResult		= eDSNoErr;
	sInt32			siValCnt		= 0;
	char		   *pDS_RecType		= 0;
	u_int			en				= 0;
	ni_index		niIndex			= 0;
	ni_id			niDirID;
	ni_entry		niEntry;
	ni_proplist		niPropList;
	ni_entrylist	niEntryList;
	void		   *aNIDomain		= NULL;

	gNetInfoMutex->Wait();

	try
	{
		NI_INIT( &niDirID );
		NI_INIT( &niEntry );
		NI_INIT( &niPropList );
		NI_INIT( &niEntryList );

		outRecCount = 0; //need to track how many records were found by this call to GetAllRecords

		aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			//assuming that record types have no embedded "/"s in them
			siResult = ::ni_pathsearch( aNIDomain, &niDirID, inNI_RecType );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			if ( inContinue->fNIEntryList.ni_entrylist_len == 0 )
			{
				siResult = ::ni_list( aNIDomain, &niDirID, "name", &(inContinue->fNIEntryList) );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}
			niEntryList = inContinue->fNIEntryList;

			if (niEntryList.ni_entrylist_val == NULL)
			{
				niEntryList.ni_entrylist_len = 0;
			}
			for ( en = inContinue->fAllRecIndex; (en < niEntryList.ni_entrylist_len) &&
						(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
						 (inContinue->fLimitRecSearch == 0)); en++ )
			{
				niEntry = niEntryList.ni_entrylist_val[ en ];
				niDirID.nii_object = niEntry.id;

				siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
				if ( siResult == NI_OK )
				{
				siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, aNIDomain, inContext->fDomainName, inRecType, siValCnt );
				if ( siResult == eDSNoErr )
				{
					fRecData->Clear();

					pDS_RecType = MapNetInfoRecToDSType( inNI_RecType );
					if ( pDS_RecType != nil )
					{
						fRecData->AppendShort( ::strlen( pDS_RecType ) );
						fRecData->AppendString( pDS_RecType );

						delete( pDS_RecType );
						pDS_RecType = nil;
					}

					niIndex = ::ni_proplist_match( niPropList, "name", NULL );
					if ( niIndex == NI_INDEX_NULL || niPropList.nipl_val[ niIndex ].nip_val.ninl_len == 0 )
					{
						fRecData->AppendShort( ::strlen( "*** No Name ***" ) );
						fRecData->AppendString( "*** No Name ***" );
					}
					else
					{
						fRecData->AppendShort( ::strlen( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] ) );
						fRecData->AppendString( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] );
					}

					if ( siValCnt == 0 )
					{
						// Attribute count
						fRecData->AppendShort( 0 );
					}
					else
					{
						// Attribute count
						fRecData->AppendShort( siValCnt );
						fRecData->AppendBlock( fAttrData->GetData(), fAttrData->GetLength() );
					}
					siResult = inBuff->AddData( fRecData->GetData(), fRecData->GetLength() );

					if ( siResult == CBuff::kBuffFull )
					{
						inContinue->fAllRecIndex = en;
						::ni_proplist_free( &niPropList );
						break;
					}
					else if ( siResult == eDSNoErr )
					{
						outRecCount++;
						inContinue->fTotalRecCount++;
					}
						else
						{
						inContinue->fAllRecIndex = 0;
						::ni_proplist_free( &niPropList );
							siResult = eDSInvalidBuffFormat;
						break;
					}
				}
					::ni_proplist_free( &niPropList );
				}
			}
			if ( siResult == eDSNoErr )
			{
				inContinue->fAllRecIndex = 0;
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult ); 

} // GetAllRecords


//------------------------------------------------------------------------------------
//	* GetTheseRecords
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn:: GetTheseRecords (char				*inConstRecName,
									const char			*inConstRecType,
									char				*inNativeRecType,
									tDirPatternMatch	 inPattMatch,
									CAttributeList		*inAttrTypeList,
									sNIContextData		*inContext,
									bool				 inAttrOnly,
									CBuff				*inBuff,
									sNIContinueData		*inContinue,
									uInt32				&outRecCount )
//note that inConstRecName should really be called inPatt2Match just like in FindTheseRecords

{
	sInt32			siResult	= eDSNoErr;
	sInt32			siCount		= 0;
	u_int			en			= 0;
	ni_status		niStatus	= NI_OK;
	ni_index		niIndex		= 0;
	ni_index		niIndexComp	= 0;
	ni_id			niDirID;
	ni_id			users_niDirID;
	ni_id			name_niDirID;
	ni_id			realname_niDirID;
	ni_proplist		niPropList;
	ni_entry		niEntry;
	ni_entrylist	niEntryList;
	bool			bGetThisOne	= true;
	char		   *normalizedRecName		= nil;
	char		   *inConstRegExpRecName	= nil;
	bool			bGotAMatch	= false;
	void		   *aNIDomain	= NULL;
	bool			bReBuildLocalUserNameCache = false;
	bool			bReBuildLocalUserRealNameCache = false;
	bool			bSearchNIForName = true;
	bool			bSearchNIForRealName = true;

	gNetInfoMutex->Wait();

	try
	{
		NI_INIT( &niDirID );
		NI_INIT( &users_niDirID );
		NI_INIT( &niEntry );
		NI_INIT( &niPropList );
		NI_INIT( &niEntryList );

		outRecCount = 0; //need to track how many records were found by this call to GetTheseRecords

		//need to flush buffer so that if we retrieve multiple entries with one call to this routine they
		//will not get added to each other
		fRecData->Clear();

		aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			//determine whether to (re)build the local user cache for the local node
			if ( inContext->bIsLocal && gServerOS && (inPattMatch == eDSiExact) )
			{
				if ( gbBuildNILocalUserCache || ( gCountToCheckLocalDataStamp > 2) ) // after one minute or more we recheck the data stamp
				{
					gCountToCheckLocalDataStamp = 0; //counts number of 30 second periodic tasks
					NI_INIT( &niPropList );
					ni_statistics(aNIDomain, &niPropList);
					niIndex = ::ni_proplist_match( niPropList, "checksum", NULL );
					if ( niIndex != NI_INDEX_NULL || niPropList.nipl_val[ niIndex ].nip_val.ninl_len > 0 )
					{
						DBGLOG1( kLogPlugin, "CNiPlugIn::GetTheseRecords found checksum <%s> for data stamp check", niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] );
						if (gLocalNIDataStamp == NULL)
						{
							gLocalNIDataStamp = strdup(niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ]);
						}
						else if (strcmp(gLocalNIDataStamp, niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ]) != 0)
						{
							gbBuildNILocalUserCache = true;
							DSFreeString(gLocalNIDataStamp);
							gLocalNIDataStamp = strdup(niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ]);
							DBGLOG( kLogPlugin, "CNiPlugIn::GetTheseRecords gbBuildNILocalUserCache is TRUE due to data stamp check" );
						}
					}
					::ni_proplist_free( &niPropList );
					NI_INIT( &niPropList );
				}
				if ( gbBuildNILocalUserCache )
				{
					bReBuildLocalUserNameCache = true;
					bReBuildLocalUserRealNameCache = true;
					CFArrayRemoveAllValues(gNILocalNameCache);
					CFArrayRemoveAllValues(gNILocalRealNameCache);
					gbBuildNILocalUserCache = false;
				}
			}
			//need to handle continue data now with the fMultiMapIndex field so that we know
			//where we left off on the last time in here
			//KW need to get rid of this index and consolidate the code together
			
			if ( (::strcmp( inNativeRecType, "users" ) == 0 ) &&
				((inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 2) || (inContinue->fMultiMapIndex == 3)) )

			{
				niStatus = ::ni_pathsearch( aNIDomain, &niDirID, "/users" );
				users_niDirID.nii_object = niDirID.nii_object;
				users_niDirID.nii_instance = niDirID.nii_instance;
				if ( bReBuildLocalUserNameCache )
				{
					NI_INIT(&name_niDirID);
					name_niDirID.nii_object = niDirID.nii_object;
					name_niDirID.nii_instance = niDirID.nii_instance;
				}
				if ( bReBuildLocalUserRealNameCache )
				{
					NI_INIT(&realname_niDirID);
					realname_niDirID.nii_object = niDirID.nii_object;
					realname_niDirID.nii_instance = niDirID.nii_instance;
				}

				if ( niStatus == NI_OK )
				{
					CFMutableStringRef cfNameToFind = CFStringCreateMutable(NULL, 0);
					CFStringAppendCString( cfNameToFind, inConstRecName, kCFStringEncodingUTF8 );
					CFStringUppercase( cfNameToFind, NULL );
					if ( !bReBuildLocalUserNameCache &&
						inContext->bIsLocal && gServerOS &&
						(inPattMatch == eDSiExact) &&
						( CFArrayGetCount(gNILocalNameCache) > 0 ) &&
						( CFArrayContainsValue(gNILocalNameCache, CFRangeMake(0,CFArrayGetCount(gNILocalNameCache)), cfNameToFind) == false ) )
					{
						DBGLOG( kLogPlugin, "CNiPlugIn::GetTheseRecords bSearchNIForName is FALSE so no need to search NI" );
						bSearchNIForName = false;
					}
					CFRelease(cfNameToFind);
					
					if (bSearchNIForName)
					{

						if ( bReBuildLocalUserNameCache )
						{
							niStatus = ::ni_list( aNIDomain, &name_niDirID, "name", &niEntryList );
							DBGLOG( kLogPlugin, "CNiPlugIn::GetTheseRecords bReBuildLocalUserNameCache is TRUE" );
							DBGLOG1( kLogPlugin, "CNiPlugIn::GetTheseRecords number of record entries is <%d>", niEntryList.ni_entrylist_len);
							if (niEntryList.ni_entrylist_len < gBuildCacheSizeLimit)
							{
								NI_INIT( &niEntry );
								NI_INIT( &niPropList );
								for (uInt32 ni_idx = 0; ni_idx < niEntryList.ni_entrylist_len; ni_idx++)
								{
									niEntry = niEntryList.ni_entrylist_val[ ni_idx ];
									name_niDirID.nii_object = niEntry.id;

									niStatus = ::ni_read( aNIDomain, &name_niDirID, &niPropList );
									if ( niStatus == NI_OK )
									{
										niIndex = ::ni_proplist_match( niPropList, "name", NULL );
										if ( ( niIndex != NI_INDEX_NULL ) && ( niPropList.nipl_val[ niIndex ].nip_val.ninl_len != 0 ) )
										{
                                            uInt32 ninl_len = niPropList.nipl_val[ niIndex ].nip_val.ninl_len;
                                            
                                            for( uInt32 ninl_idx = 0; ninl_idx < ninl_len; ninl_idx++ )
                                            {
                                                CFMutableStringRef cfNameString = CFStringCreateMutable(NULL, 0);
                                                CFStringAppendCString( cfNameString, niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ ninl_idx ], kCFStringEncodingUTF8 );
                                                CFStringUppercase( cfNameString, NULL );
                                                DBGLOG1( kLogPlugin, "CNiPlugIn::GetTheseRecords: add to name buffer the name in uppercase<%s>", niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ ninl_idx ] );
                                                CFArrayAppendValue(gNILocalNameCache, cfNameString);
                                                CFRelease(cfNameString);
                                            }
										}
									}
								}
								NI_INIT( &niEntry );
								NI_INIT( &niPropList );
							}
							bReBuildLocalUserNameCache = false;
							if (niEntryList.ni_entrylist_len > 0)
							{
								::ni_entrylist_free( &niEntryList );
								niEntryList.ni_entrylist_len = 0;
							}
							NI_INIT( &niEntryList );
						}

						//no definitive mapping of record names yet ie. only hardcoded here
						//KW how can we ensure that a search that "hits" both the name and realname will not return two found records when indeed
						//there is only one record in NetInfo
						//KW can this command handle a search on two separate attributes??? - not likely
						//make the reg exp that ni_search needs
						inConstRegExpRecName = BuildRegExp(inConstRecName);
						niStatus = ::ni_search( aNIDomain, &niDirID, (char *)"name", inConstRegExpRecName, REG_ICASE, &niEntryList );
						DSFreeString(inConstRegExpRecName);

						if ( (niStatus == NI_OK) && (niEntryList.ni_entrylist_val != NULL) &&
							((inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 2)) )
						{
							for ( en = inContinue->fAllRecIndex; (en < niEntryList.ni_entrylist_len) &&
									(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
									(inContinue->fLimitRecSearch == 0)); en++ )
		//					for ( en = inContinue->fAllRecIndex; en < niEntryList.ni_entrylist_len; en++ )
							{
								niEntry = niEntryList.ni_entrylist_val[ en ];
								niDirID.nii_object = niEntry.id;

								niStatus = ::ni_read( aNIDomain, &niDirID, &niPropList );
								if ( niStatus == NI_OK )
								{
									niIndex = ::ni_proplist_match( niPropList, "name", NULL );
									if ( niIndex != NI_INDEX_NULL )
									{
										//code is here to determine if the regcomp operation on inConstRecName for NetInfo actually
										//returned what was requested ie. it will always return at least it but probably much more
										//so filter out the much more
										
										bGotAMatch = false;
										// For each value in the namelist for this property
										for ( uInt32 pv = 0; pv < niPropList.nipl_val[ niIndex ].nip_val.ninl_len; pv++ )
										{
											// check if we find a match
											if ( DoesThisMatch(	(const char*)(niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ pv ]),
																(const char*)inConstRecName,
																inPattMatch ) )
											{
												bGotAMatch = true;
												break;
											}
										}

										if (bGotAMatch)
										{
											//need to flush buffer so that if we retrieve multiple entries with one call to this routine they
											//will not get added to each other
											//fRecData->Clear(); // not anymore since two loops now
				
											fRecData->AppendShort( ::strlen( inConstRecType ) );
											fRecData->AppendString( inConstRecType );

											niIndex = ::ni_proplist_match( niPropList, "name", NULL );
											if ( niIndex == NI_INDEX_NULL || niPropList.nipl_val[ niIndex ].nip_val.ninl_len == 0 )
											{
												fRecData->AppendShort( ::strlen( "*** No Name ***" ) );
												fRecData->AppendString( "*** No Name ***" );
											}
											else
											{
												fRecData->AppendShort( ::strlen( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] ) );
												fRecData->AppendString( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] );
											}

											siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, aNIDomain, inContext->fDomainName, inConstRecType, siCount );
											if ( siResult == eDSNoErr )
											{
												if ( siCount == 0 )
												{
													// Attribute count
													fRecData->AppendShort( 0 );
												}
												else
												{
													// Attribute count
													fRecData->AppendShort( siCount );
													fRecData->AppendBlock( fAttrData->GetData(), fAttrData->GetLength() );
												}
												siResult = inBuff->AddData( fRecData->GetData(), fRecData->GetLength() );
												fRecData->Clear();

												if ( siResult == CBuff::kBuffFull )
												{
													inContinue->fMultiMapIndex = 2;
													inContinue->fAllRecIndex = en;
													::ni_proplist_free( &niPropList );
													throw( siResult );
												}
												else if ( siResult == eDSNoErr )
												{
													inContinue->fMultiMapIndex = 0;
													outRecCount++;
													inContinue->fTotalRecCount++;
												}
												else
												{
													inContinue->fMultiMapIndex = 0;
													::ni_proplist_free( &niPropList );
													throw( (sInt32)eDSInvalidBuffFormat );
												}
											} // GetTheseAttributes(...)
										} // bGotAMatch
									}
									::ni_proplist_free( &niPropList );
								} // if ( ::ni_read( aNIDomain, &niDirID, &niPropList == NI_OK ))
									
							} // for loop over niEntryList.ni_entrylist_len
						} // if ( ::ni_search( aNIDomain, &niDirID, (char *)"name", inConstRecName, REG_ICASE, &niEntryList ) == NI_OK )


						if (niEntryList.ni_entrylist_len > 0)
						{
							::ni_entrylist_free( &niEntryList );
							niEntryList.ni_entrylist_len = 0;
						}
						NI_INIT( &niEntryList );
					} // if (bSearchNIForName)

					CFMutableStringRef cfRealNameToFind = CFStringCreateMutable(NULL, 0);
					CFStringAppendCString( cfRealNameToFind, inConstRecName, kCFStringEncodingUTF8 );
					CFStringUppercase( cfRealNameToFind, NULL );
					if ( !bReBuildLocalUserRealNameCache &&
						inContext->bIsLocal && gServerOS &&
						(inPattMatch == eDSiExact) &&
						( CFArrayGetCount(gNILocalRealNameCache) > 0 ) &&
						( CFArrayContainsValue(gNILocalRealNameCache, CFRangeMake(0,CFArrayGetCount(gNILocalRealNameCache)), cfRealNameToFind) == false ) )
					{
						DBGLOG( kLogPlugin, "CNiPlugIn::GetTheseRecords bSearchNIForRealName is FALSE so no need to search NI" );
						bSearchNIForRealName = false;
					}
					CFRelease(cfRealNameToFind);
					
					if (bSearchNIForRealName)
					{
						//need to ensure realname cache is rebuilt (if local bool set) even if we have found the limit of entries requested
						if ( bReBuildLocalUserRealNameCache )
						{
							niStatus = ::ni_list( aNIDomain, &realname_niDirID, "realname", &niEntryList );
							DBGLOG( kLogPlugin, "CNiPlugIn::GetTheseRecords bReBuildLocalUserRealNameCache is TRUE" );
							DBGLOG1( kLogPlugin, "CNiPlugIn::GetTheseRecords number of record entries is <%d>", niEntryList.ni_entrylist_len);
							if (niEntryList.ni_entrylist_len < gBuildCacheSizeLimit)
							{
								NI_INIT( &niEntry );
								NI_INIT( &niPropList );
								for (uInt32 ni_idx = 0; ni_idx < niEntryList.ni_entrylist_len; ni_idx++)
								{
									niEntry = niEntryList.ni_entrylist_val[ ni_idx ];
									realname_niDirID.nii_object = niEntry.id;

									niStatus = ::ni_read( aNIDomain, &realname_niDirID, &niPropList );
									if ( niStatus == NI_OK )
									{
										niIndex = ::ni_proplist_match( niPropList, "realname", NULL );
										if ( ( niIndex != NI_INDEX_NULL ) && ( niPropList.nipl_val[ niIndex ].nip_val.ninl_len != 0 ) )
										{
                                            uInt32 ninl_len = niPropList.nipl_val[ niIndex ].nip_val.ninl_len;
                                            
                                            for( uInt32 ninl_idx = 0; ninl_idx < ninl_len; ninl_idx++ )
                                            {
                                                CFMutableStringRef cfRealNameString = CFStringCreateMutable(NULL, 0);
                                                CFStringAppendCString( cfRealNameString, niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ ninl_idx ], kCFStringEncodingUTF8 );
                                                CFStringUppercase( cfRealNameString, NULL );
                                                DBGLOG1( kLogPlugin, "CNiPlugIn::GetTheseRecords: add to realname buffer the realname in uppercase<%s>", niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ ninl_idx ] );
                                                CFArrayAppendValue(gNILocalRealNameCache, cfRealNameString);
                                                CFRelease(cfRealNameString);
                                            }
										}
									}
								}
								NI_INIT( &niEntry );
								NI_INIT( &niPropList );
							}
							bReBuildLocalUserRealNameCache = false;
							if (niEntryList.ni_entrylist_len > 0)
							{
								::ni_entrylist_free( &niEntryList );
								niEntryList.ni_entrylist_len = 0;
							}
							NI_INIT( &niEntryList );
						}

						//ensure that if a client only asks for n records that we stop searching if
						//we have already found n records ie. critical for performance when asking for one record
						if ( (inContinue->fTotalRecCount < inContinue->fLimitRecSearch) || (inContinue->fLimitRecSearch == 0) )
						{

							//make the reg exp that ni_search needs
							normalizedRecName = NormalizeNIString( inConstRecName, "realname" );
							inConstRegExpRecName = BuildRegExp(normalizedRecName);
							niStatus = ::ni_search( aNIDomain, &users_niDirID, (char *)"realname", inConstRegExpRecName, REG_ICASE, &niEntryList );
							DSFreeString(inConstRegExpRecName);

							if ( (niStatus == NI_OK) && (niEntryList.ni_entrylist_val != NULL) &&
								((inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 3)) )
							{
								for ( en = inContinue->fAllRecIndex; (en < niEntryList.ni_entrylist_len) &&
										(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
										(inContinue->fLimitRecSearch == 0)); en++ )
		//						for ( en = inContinue->fAllRecIndex; en < niEntryList.ni_entrylist_len; en++ )
								{
									niEntry = niEntryList.ni_entrylist_val[ en ];
									users_niDirID.nii_object = niEntry.id;
		
									niStatus = ::ni_read( aNIDomain, &users_niDirID, &niPropList );
									if ( niStatus == NI_OK )
									{
										niIndex = ::ni_proplist_match( niPropList, "realname", NULL );
										if ( niIndex != NI_INDEX_NULL )
										{
											//code is here to determine if the regcomp operation on normalizedRecName for NetInfo actually
											//returned what was requested ie. it will always return at least it but probably much more
											//so filter out the much more
											bGotAMatch = false;
											// For each value in the namelist for this property
											for ( uInt32 pv = 0; pv < niPropList.nipl_val[ niIndex ].nip_val.ninl_len; pv++ )
											{
												// check if we find a match
												if ( DoesThisMatch(	(const char*)(niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ pv ]),
																	(const char*)normalizedRecName,
																	inPattMatch ) )
												{
													bGotAMatch = true;
													break;
												}
											}

											if (bGotAMatch)
											{
												bGetThisOne = true;
												//check here if we have already grabbed this record
												//read the name and see if there is a match then ignore if true
												niIndexComp = ::ni_proplist_match( niPropList, "name", NULL );
												if ( niIndexComp != NI_INDEX_NULL )
												{
													for ( uInt32 pv = 0; pv < niPropList.nipl_val[ niIndexComp ].nip_val.ninl_len; pv++ )
													{
														// check if we find a match
														if ( DoesThisMatch(	(const char*)(niPropList.nipl_val[ niIndexComp ].nip_val.ninl_val[ pv ]),
																			(const char*)normalizedRecName,
																			inPattMatch ) )
														{
															//should have already gotten this one above in search on name
															DBGLOG2( kLogPlugin, "GetTheseRecords: Dupe record found with name <%s> and realname <%s> search", (const char*)(niPropList.nipl_val[ niIndexComp ].nip_val.ninl_val[ pv ]), (const char*)normalizedRecName );
															bGetThisOne = false;
															break;
														}
													}
												}
		
												if (bGetThisOne)
												{
													//need to flush buffer so that if we retrieve multiple entries with one call to this routine they
													//will not get added to each other
													//fRecData->Clear(); // not anymore since two loops now
				
													fRecData->AppendShort( ::strlen( inConstRecType ) );
													fRecData->AppendString( inConstRecType );
		
													niIndex = ::ni_proplist_match( niPropList, "name", NULL );
													if ( niIndex == NI_INDEX_NULL || niPropList.nipl_val[ niIndex ].nip_val.ninl_len == 0 )
													{
														fRecData->AppendShort( ::strlen( "*** No Name ***" ) );
														fRecData->AppendString( "*** No Name ***" );
													}
													else
													{
														fRecData->AppendShort( ::strlen( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] ) );
														fRecData->AppendString( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] );
													}
		
													siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, aNIDomain, inContext->fDomainName, inConstRecType, siCount );
													if ( siResult == eDSNoErr )
													{
														if ( siCount == 0 )
														{
															// Attribute count
															fRecData->AppendShort( 0 );
														}
														else
														{
															// Attribute count
															fRecData->AppendShort( siCount );
															fRecData->AppendBlock( fAttrData->GetData(), fAttrData->GetLength() );
														}
														siResult = inBuff->AddData( fRecData->GetData(), fRecData->GetLength() );
														fRecData->Clear();
		
														if ( siResult == CBuff::kBuffFull )
														{
															inContinue->fMultiMapIndex = 3;
															inContinue->fAllRecIndex = en;
															::ni_proplist_free( &niPropList );
															if ( siResult != eDSNoErr ) throw( siResult );
														}
														else if ( siResult == eDSNoErr )
														{
															inContinue->fMultiMapIndex = 0;
															outRecCount++;
															inContinue->fTotalRecCount++;
														}
														else
														{
															inContinue->fMultiMapIndex = 0;
															::ni_proplist_free( &niPropList );
															throw( (sInt32)eDSInvalidBuffFormat );
														}
													} // GetTheseAttributes(...)
													
												}// bGetThisOne
											} // bGotAMatch
										}
										::ni_proplist_free( &niPropList );
									} // if ( ::ni_read( aNIDomain, &users_niDirID, &niPropList == NI_OK ))
								} // for loop over niEntryList.ni_entrylist_len
							} // if ( ::ni_search( aNIDomain, &users_niDirID, (char *)"realname", normalizedRecName, REG_ICASE, &niEntryList ) == NI_OK )
							if ( niEntryList.ni_entrylist_len > 0 )
							{
								::ni_entrylist_free(&niEntryList);
								niEntryList.ni_entrylist_len = 0;
							}
						} //if the client limit on records to return wasn't reached check realname as well
					} // if (bSearchNIForRealName)
				} // if ( ::ni_pathsearch( aNIDomain, &niDirID, "/users" ) == NI_OK )
			} // if ( ::strcmp( inNativeRecType, "users" ) == 0 )


			else if ( (inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 1) )
			{
				//check to see if this type exists
				niStatus = ::ni_pathsearch( aNIDomain, &niDirID, inNativeRecType );
				if ( niStatus == NI_OK )
				{
					//check to see if this record exists
					//make the reg exp that ni_search needs
					inConstRegExpRecName = BuildRegExp(inConstRecName);
					if ( (strcmp(inConstRecName,"/") == 0) &&
						((inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 1)) )
					{
						//want to get the attributes from the root directory
						niStatus = ::ni_read( aNIDomain, &niDirID, &niPropList );
						if ( niStatus == NI_OK )
						{
							fRecData->AppendShort( ::strlen( inConstRecType ) );
							fRecData->AppendString( inConstRecType );

							fRecData->AppendShort( 1 );
							fRecData->AppendString( "/" );

							siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, aNIDomain, inContext->fDomainName, inConstRecType, siCount );
							if ( siResult == eDSNoErr )
							{
								if ( siCount == 0 )
								{
									// Attribute count
									fRecData->AppendShort( 0 );
								}
								else
								{
									// Attribute count
									fRecData->AppendShort( siCount );
									fRecData->AppendBlock( fAttrData->GetData(), fAttrData->GetLength() );
								}
								siResult = inBuff->AddData( fRecData->GetData(), fRecData->GetLength() );
								fRecData->Clear();

								if ( siResult == CBuff::kBuffFull )
								{
									inContinue->fMultiMapIndex = 1;
									inContinue->fAllRecIndex = en;
									::ni_proplist_free( &niPropList );
									if ( siResult != eDSNoErr ) throw( siResult );
								}
								else if ( siResult == eDSNoErr )
								{
									inContinue->fMultiMapIndex = 0;
									outRecCount++;
									inContinue->fTotalRecCount++;
								}
								else
								{
									inContinue->fMultiMapIndex = 0;
									::ni_proplist_free( &niPropList );
									throw( (sInt32)eDSInvalidBuffFormat );
								}
							} // GetTheseAttributes(...)
							::ni_proplist_free( &niPropList );
						} // if ( ::ni_read( aNIDomain, &niDirID, &niPropList == NI_OK ))
					}
					else
					{
						niStatus = ::ni_search( aNIDomain, &niDirID, (char *)"name", inConstRegExpRecName, REG_ICASE, &niEntryList );
						if ( (niStatus == NI_OK) && (niEntryList.ni_entrylist_val != NULL) &&
							((inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 1)) )
						{
							for ( en = inContinue->fAllRecIndex; (en < niEntryList.ni_entrylist_len) &&
									(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
									(inContinue->fLimitRecSearch == 0)); en++ )
		
								{
									niEntry = niEntryList.ni_entrylist_val[ en ];
									niDirID.nii_object = niEntry.id;
		
									niStatus = ::ni_read( aNIDomain, &niDirID, &niPropList );
									if ( niStatus == NI_OK )
									{
										niIndex = ::ni_proplist_match( niPropList, "name", NULL );
										if ( niIndex != NI_INDEX_NULL )
										{
											//code is here to determine if the regcomp operation on inConstRecName for NetInfo actually
											//returned what was requested ie. it will always return at least it but probably much more
											//so filter out the much more
											bGotAMatch = false;
											// For each value in the namelist for this property
											for ( uInt32 pv = 0; pv < niPropList.nipl_val[ niIndex ].nip_val.ninl_len; pv++ )
											{
												// check if we find a match
												if ( DoesThisMatch(	(const char*)(niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ pv ]),
																	(const char*)inConstRecName,
																	inPattMatch ) )
												{
													bGotAMatch = true;
													break;
												}
											}
		
											if (bGotAMatch)
											{		
												fRecData->AppendShort( ::strlen( inConstRecType ) );
												fRecData->AppendString( inConstRecType );
		
												niIndex = ::ni_proplist_match( niPropList, "name", NULL );
												if ( niIndex == NI_INDEX_NULL || niPropList.nipl_val[ niIndex ].nip_val.ninl_len == 0 )
												{
													fRecData->AppendShort( ::strlen( "*** No Name ***" ) );
													fRecData->AppendString( "*** No Name ***" );
												}
												else
												{
													//no longer assuming perfect match on the record name
													fRecData->AppendShort( ::strlen( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] ) );
													fRecData->AppendString( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] );
												}
		
												siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, aNIDomain, inContext->fDomainName, inConstRecType, siCount );
												if ( siResult == eDSNoErr )
												{
													if ( siCount == 0 )
													{
														// Attribute count
														fRecData->AppendShort( 0 );
													}
													else
													{
														// Attribute count
														fRecData->AppendShort( siCount );
														fRecData->AppendBlock( fAttrData->GetData(), fAttrData->GetLength() );
													}
													siResult = inBuff->AddData( fRecData->GetData(), fRecData->GetLength() );
													fRecData->Clear();
		
													if ( siResult == CBuff::kBuffFull )
													{
														inContinue->fMultiMapIndex = 1;
														inContinue->fAllRecIndex = en;
														::ni_proplist_free( &niPropList );
														if ( siResult != eDSNoErr ) throw( siResult );
													}
													else if ( siResult == eDSNoErr )
													{
														inContinue->fMultiMapIndex = 0;
														outRecCount++;
														inContinue->fTotalRecCount++;
													}
													else
													{
														inContinue->fMultiMapIndex = 0;
														::ni_proplist_free( &niPropList );
														throw( (sInt32)eDSInvalidBuffFormat );
													}
												} // GetTheseAttributes(...)
											} // bGotAMatch
										}
										::ni_proplist_free( &niPropList );
									} // if ( ::ni_read( aNIDomain, &niDirID, &niPropList == NI_OK ))
								} // for loop over niEntryList.ni_entrylist_len
		
						} // if ( (niStatus == NI_OK) && ((inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 1)) )
						if ( niEntryList.ni_entrylist_len > 0 )
						{
							::ni_entrylist_free(&niEntryList);
							niEntryList.ni_entrylist_len = 0;
						}
					}
					DSFreeString(inConstRegExpRecName);
				} // if ( niStatus == NI_OK )
			}// else if ( (inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 1) )

			inContinue->fMultiMapIndex = 0;
			inContinue->fAllRecIndex = 0;
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}

	} // try

	catch( sInt32 err )
	{
		if ( niEntryList.ni_entrylist_len > 0 )
		{
			::ni_entrylist_free(&niEntryList);
			niEntryList.ni_entrylist_len = 0;
		}
		siResult = err;
	}
	
	if ( normalizedRecName != inConstRecName )
	{
		DSFreeString( normalizedRecName );
	}

	gNetInfoMutex->Signal();

	return( siResult ); 

} // GetTheseRecords


//------------------------------------------------------------------------------------
//	* GetTheseAttributes
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetTheseAttributes (	CAttributeList	*inAttrTypeList,
										ni_proplist		*inPropList,
										bool			 inAttrOnly,
										void			*inDomain,
										char			*inDomainName,
										const char		*inRecType,
										sInt32			&outCount )
{
	sInt32			siResult		= eDSNoErr;
	uInt32			pn				= 0;
	uInt32			pv				= 0;
	sInt32			attrTypeIndex	= 1;
	char		   *pNI_AttrType	= nil;
	char		   *pDS_AttrType	= nil;
	char		   *pDS_AttrName	= nil;
	ni_index		niIndex			= 0;
	ni_property		niProp;
	ni_namelist		niNameList;

//KW this is now passing fAttrData via a member function - bad style but mutex protected for now
	gNetInfoMutex->Wait();

	try
	{
		outCount = 0;
		fAttrData->Clear();

		// Get the record attributes with/without the values
		while ( inAttrTypeList->GetAttribute( attrTypeIndex++, &pDS_AttrType ) == eDSNoErr )
		{
			siResult = eDSNoErr;
			pNI_AttrType = MapAttrToNetInfoType( pDS_AttrType );
			if ( pNI_AttrType != nil )
			{
				if ( ::strcmp( kDSNAttrMetaNodeLocation, pNI_AttrType ) == 0 )
				{
					fTmpData->Clear();

					outCount++;

					// Append the attribute name
					fTmpData->AppendShort( ::strlen( pDS_AttrType ) );
					fTmpData->AppendString( pDS_AttrType );

					if ( inAttrOnly == false )
					{
						// Append the attribute value count
						fTmpData->AppendShort( 1 );

						fTmpData->AppendLong( ::strlen( inDomainName ) );
						fTmpData->AppendString( inDomainName );
					}
					else
					{
						fTmpData->AppendShort( 0 );
					}

					// Add the attribute length
					fAttrData->AppendLong( 	fTmpData->GetLength() );
					fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

					// Clear the temp block
					fTmpData->Clear();
				}
				else if ( ::strcmp( kDSNAttrRecordType, pNI_AttrType ) == 0 )
				{
					// we simply use the input argument inRecType
					fTmpData->Clear();
					
					// Append the attribute name
					fTmpData->AppendShort( ::strlen( pNI_AttrType ) );
					fTmpData->AppendString( pNI_AttrType );

					if ( inAttrOnly == false )
					{
						// Append the attribute value count
						fTmpData->AppendShort( 1 );

						// Append attribute value
						fTmpData->AppendLong( ::strlen( inRecType ) );
						fTmpData->AppendString( inRecType );

					}// if ( inAttrOnly == false )
					else
					{
						fTmpData->AppendShort( 0 );
					}

					// Add the attribute length
					outCount++;
					fAttrData->AppendLong( fTmpData->GetLength() );
					fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

					// Clear the temp block
					fTmpData->Clear();

				} // if ( ::strcmp( kDSNAttrRecordType, pAttrType ) == 0 )
				else
				{
					// Get all attributes
					if ( ( ::strcmp( kDSAttributesAll, pNI_AttrType ) == 0 ) ||
					     ( ::strcmp( kDSAttributesStandardAll, pNI_AttrType ) == 0 ) )
					{
						//code here is to add the attribute kDSNAttrMetaNodeLocation to
						// kDSAttributesAll and kDSAttributesStandardAll
						fTmpData->Clear();

						// Append the attribute name
						fTmpData->AppendShort( ::strlen( kDSNAttrMetaNodeLocation ) );
						fTmpData->AppendString( kDSNAttrMetaNodeLocation );

						if ( inAttrOnly == false )
						{
							// Append the attribute value count
							fTmpData->AppendShort( 1 );

							fTmpData->AppendLong( ::strlen( inDomainName ) );
							fTmpData->AppendString( inDomainName );
						}
						else
						{
							fTmpData->AppendShort( 0 );
						}

						// Add the attribute length
						outCount++;
						fAttrData->AppendLong( 	fTmpData->GetLength() );
						fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

						// we simply use the input argument inRecType
						fTmpData->Clear();
						
						// Append the attribute name
						fTmpData->AppendShort( ::strlen( kDSNAttrRecordType ) );
						fTmpData->AppendString( kDSNAttrRecordType );

						if ( inAttrOnly == false )
						{
							// Append the attribute value count
							fTmpData->AppendShort( 1 );

							// Append attribute value
							fTmpData->AppendLong( ::strlen( inRecType ) );
							fTmpData->AppendString( inRecType );

						}// if ( inAttrOnly == false )
						else
						{
							fTmpData->AppendShort( 0 );
						}

						// Add the attribute length
						outCount++;
						fAttrData->AppendLong( fTmpData->GetLength() );
						fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

						// Clear the temp block
						fTmpData->Clear();
						
						for ( pn = 0; pn < inPropList->ni_proplist_len; pn++ )
						{
							niProp = inPropList->ni_proplist_val[ pn ];

							fTmpData->Clear();

							pDS_AttrName = MapNetInfoAttrToDSType( niProp.nip_name );
							if ( pDS_AttrName != nil )
							{
								if ( ( ::strncmp(pDS_AttrName,kDSNativeAttrTypePrefix,strlen(kDSNativeAttrTypePrefix)) == 0 ) &&
									( ::strcmp( kDSAttributesStandardAll, pNI_AttrType ) == 0 ) )
								{
									delete( pDS_AttrName );
									pDS_AttrName = nil;
									//native not mapped to standard type so skip in this case
									continue;
								}
								else
								{
									//get the attribute in this case
									fTmpData->AppendShort( ::strlen( pDS_AttrName ) );
									fTmpData->AppendString( pDS_AttrName );
		
									outCount++;
		
									delete( pDS_AttrName );
									pDS_AttrName = nil;
		
									if ( inAttrOnly == false )
									{
										niNameList = niProp.nip_val;
		
										// Append the attribute value count
										fTmpData->AppendShort( niNameList.ni_namelist_len );
		
										// For each value in the namelist for this property
										for ( pv = 0; pv < niNameList.ni_namelist_len; pv++ )
										{
											// Append attribute value "n"
											fTmpData->AppendLong( ::strlen( niNameList.ni_namelist_val[ pv ] ) );
											fTmpData->AppendString( niNameList.ni_namelist_val[ pv ] );
										}
									}
									else
									{
										fTmpData->AppendShort( 0 );
									}
					
									// Add the attribute length
									fAttrData->AppendLong( 	fTmpData->GetLength() );
									fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );
		
									// Clear the temp block
									fTmpData->Clear();
								} //get the attribute in this case
							} //if ( pDS_AttrName != nil )
						}
					}
					else
					{
						// Get one attribute at a time
						niIndex = ::ni_proplist_match( *inPropList, pNI_AttrType, NULL );
						if ( niIndex != NI_INDEX_NULL )
						{
							fTmpData->Clear();

							outCount++;

							fTmpData->AppendShort( ::strlen( pDS_AttrType ) );
							fTmpData->AppendString( pDS_AttrType );

							if ( inAttrOnly == false )
							{
								niNameList = inPropList->nipl_val[ niIndex ].nip_val;

								// Append the attribute value count
								fTmpData->AppendShort( niNameList.ni_namelist_len );

								// For each value in the namelist for this property
								for ( pv = 0; pv < niNameList.ni_namelist_len; pv++ )
								{
									// Append attribute value "n"
									fTmpData->AppendLong( ::strlen( niNameList.ni_namelist_val[ pv ] ) );
									fTmpData->AppendString( niNameList.ni_namelist_val[ pv ] );
								}
							}
							else
							{
								fTmpData->AppendShort( 0 );
							}
							
							// Add the attribute length
							fAttrData->AppendLong( fTmpData->GetLength() );
							fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

							// Clear the temp block
							fTmpData->Clear();
						}
					}
				}
				delete( pNI_AttrType );
				pNI_AttrType = nil;
			}
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // GetTheseAttributes


//------------------------------------------------------------------------------------
//	* AddAttribute
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::AddAttribute ( sAddAttribute *inData )
{
	sInt32					siResult		= eDSNoErr;
	tDataNode			   *pNewAttr		= nil;
	char				   *pAttrKey		= nil;
	tDataNode			   *pAttrValue		= nil;
	sNIContextData		   *pContext		= nil;
	char				   *pEmptyValue		= (char *)"";
	char				   *pValue			= nil;
	ni_namelist				niValues;
	char				   *authUser		= nil;

	gNetInfoMutex->Wait();

	try
	{
		// Record context data
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );

		pNewAttr = inData->fInNewAttr;
		if ( pNewAttr == nil ) throw( (sInt32)eDSNullAttribute );

		pAttrKey = MapAttrToNetInfoType( pNewAttr->fBufferData );
		if ( pAttrKey == nil ) throw( (sInt32)eDSNullAttribute );

		pAttrValue = inData->fInFirstAttrValue;
		if ( pAttrValue == nil )
		{
			pValue = pEmptyValue;
		}
		else
		{
			pValue = pAttrValue->fBufferData;
		}

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			// check access for non-admin users
			if ( ( pContext->fEffectiveUID != 0 )
				 && ( (pContext->fAuthenticatedUserName == NULL) 
					  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
			{
				authUser = pContext->fAuthenticatedUserName;
				if ( ( authUser == NULL ) && (strcmp(pAttrKey,"picture") == 0) )
				{
					authUser = GetUserNameForUID( pContext->fEffectiveUID, aNIDomain );
				}
				ni_proplist niPropList;
				siResult = ::ni_read( aNIDomain, &pContext->dirID, &niPropList );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

				siResult = NiLib2::ValidateDir( authUser, &niPropList );
				::ni_proplist_free( &niPropList );

				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}

			NI_INIT( &niValues );
			::ni_namelist_insert( &niValues, pValue, NI_INDEX_NULL );
			NormalizeNINameList(&niValues, pAttrKey);
			siResult = DoAddAttribute( aNIDomain, &pContext->dirID, pAttrKey, niValues );
			::ni_namelist_free( &niValues );
			if (siResult == eDSNoErr)
			{
				pContext->bDidWriteOperation = true;
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if (authUser != nil)
	{
		if (pContext != nil)
		{
			if (pContext->fAuthenticatedUserName == nil)
			{
				free(authUser);
				authUser = nil;
			}
		}
	}

	if ( pAttrKey != nil )
	{
		delete( pAttrKey );
		pAttrKey = nil;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // AddAttribute


//------------------------------------------------------------------------------------
//	* DoAddAttribute
//
//		- Taken from ni2_createdirprop but modified to be readable.
//			Createprop given a directory rather than a pathname
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoAddAttribute (	void			*inDomain,
									ni_id			*inDir,
									const ni_name	 inKey,
									ni_namelist		 inValues )
{
	sInt32			siResult	= eDSNoErr;
	ni_index		where		= 0;
	ni_property		niPropList;
	ni_namelist		niNameList;

	gNetInfoMutex->Wait();

	try
	{
		// need to be talking to the master
		::ni_needwrite( inDomain, 1 );

		// fetch list of property keys from directory
		NI_INIT( &niNameList );
		siResult = ::ni_listprops( inDomain, inDir, &niNameList );
		if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

		// check for existing property with this key
		where = ::ni_namelist_match( niNameList, inKey );
		::ni_namelist_free( &niNameList );

		// if property doesn't exist, create it
		if ( where == NI_INDEX_NULL )
		{
			NI_INIT( &niPropList );
			niPropList.nip_name	= ::ni_name_dup( inKey );
			niPropList.nip_val	= ::ni_namelist_dup( inValues );
			siResult = ::ni_createprop( inDomain, inDir, niPropList, NI_INDEX_NULL );
			::ni_prop_free( &niPropList );
		}
		else
		{
			// property exists: replace the existing values
			siResult = ::ni_writeprop( inDomain, inDir, where, inValues );
		}
		
		siResult = MapNetInfoErrors( siResult );
		
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // DoAddAttribute


//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetAttributeValue ( sGetAttributeValue *inData )
{
	sInt32						siResult		= eDSNoErr;
	uInt16						usValueCnt		= 0;
	uInt32						usValueLen		= 0;
	uInt16						usAttrNameLen	= 0;
	uInt32						i				= 0;
	uInt32						uiIndex			= 0;
	uInt32						offset			= 0;
	char					   *p				= nil;
	tDataBuffer				   *pDataBuff		= nil;
	tAttributeValueEntry	   *pAttrValue		= nil;
	sNIContextData			   *pValueContext	= nil;
	uInt32						buffSize		= 0;
	uInt32						buffLen			= 0;
	uInt32						attrLen			= 0;

	try
	{
		pValueContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInAttrValueListRef );
		if ( pValueContext  == nil ) throw( (sInt32)eDSBadContextData );

		uiIndex = inData->fInAttrValueIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );

		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (sInt32)eDSNullDataBuff );

		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pValueContext->offset;
		offset	= pValueContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 + offset > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the buffer length
		::memcpy( &attrLen, p, 4 );

		//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
		//AND add the length of the buffer length var as stored ie. 4 bytes
		buffLen		= attrLen + pValueContext->offset + 4;
		if (buffLen > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute name length
		::memcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the value count
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt)  throw( (sInt32)eDSInvalidIndex );

		// Skip to the value that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 + offset > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( &usValueLen, p, 4 );
		
		p		+= 4;
		offset	+= 4;

		//if (usValueLen == 0)  throw( (sInt32)eDSInvalidBuffFormat ); //if zero is it okay?

		pAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );

		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( usValueLen + offset > buffLen ) throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );

		// Set the attribute value ID
		pAttrValue->fAttributeValueID = CalcCRC( pAttrValue->fAttributeValueData.fBufferData );

		inData->fOutAttrValue = pAttrValue;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeValue


//------------------------------------------------------------------------------------
//	* GetRecAttribInfo
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetRecAttribInfo ( sGetRecAttribInfo *inData )
{
	bool					bFound			= false;
	sInt32					siResult		= eDSNoErr;
	ni_status				niStatus		= NI_OK;
	uInt32					uiPv			= 0;
	uInt32					uiTypeLen		= 0;
	uInt32					uiDataLen		= 0;
	tDataNodePtr			pAttrType		= nil;
	char				   *pNI_AttrType	= nil;
	tAttributeEntryPtr		pOutAttrEntry	= nil;
	sNIContextData		   *pContext		= nil;
	ni_id					niDirID;
	ni_namelist				niNameList;		// values;
	ni_index				niIndex;

	gNetInfoMutex->Wait();

	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

		pNI_AttrType = MapAttrToNetInfoType( pAttrType->fBufferData );
		if ( pNI_AttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			::memcpy( &niDirID, &pContext->dirID, sizeof( ni_id ) );

			NI_INIT( &niNameList );
			niStatus = ::ni_listprops( aNIDomain, &niDirID, &niNameList );
			if ( niStatus == NI_OK )
			{
				niIndex = ::ni_namelist_match( niNameList, pNI_AttrType );
				::ni_namelist_free( &niNameList );

				if ( niIndex != NI_INDEX_NULL )
				{
					NI_INIT( &niNameList );
					niStatus = ::ni_readprop( aNIDomain, &niDirID, niIndex, &niNameList );
					if ( niStatus == NI_OK )
					{
						uiTypeLen = ::strlen( pAttrType->fBufferData );
						pOutAttrEntry = (tAttributeEntry *)::calloc( (sizeof( tAttributeEntry ) + uiTypeLen + kBuffPad), sizeof( char ) );

	//					pOutAttrEntry = (tAttributeEntry *)::malloc( sizeof( tAttributeEntry ) + uiTypeLen + kBuffPad );
	//					::memset( pOutAttrEntry, 0, sizeof( tAttributeEntry ) + uiTypeLen + kBuffPad );

						pOutAttrEntry->fAttributeSignature.fBufferSize		= uiTypeLen;
						pOutAttrEntry->fAttributeSignature.fBufferLength	= uiTypeLen;
						::memcpy( pOutAttrEntry->fAttributeSignature.fBufferData, pAttrType->fBufferData, uiTypeLen ); 

						pOutAttrEntry->fAttributeValueCount = niNameList.ni_namelist_len;
						pOutAttrEntry->fAttributeValueMaxSize = 255;

						for ( uiPv = 0; uiPv < niNameList.ni_namelist_len; uiPv++ )
						{
							uiDataLen += ::strlen( niNameList.ni_namelist_val[ uiPv ] );
						}

						pOutAttrEntry->fAttributeDataSize = uiDataLen;

						inData->fOutAttrInfoPtr = pOutAttrEntry;

						bFound = true;

						::ni_namelist_free( &niNameList );
					}
				}
				else
				{
					siResult = eDSAttributeNotFound;
				}
			}
			else
			{
				siResult = MapNetInfoErrors( niStatus );
			}
			delete( pNI_AttrType );
			pNI_AttrType = nil;
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // GetRecAttribInfo


//------------------------------------------------------------------------------------
//	* GetRecordAttributeValueByID
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetRecordAttributeValueByID ( sGetRecordAttributeValueByID *inData )
{
	bool						done			= false;
	sInt32						siResult		= eDSAttributeNotFound;
	ni_status					niStatus		= NI_OK;
	uInt32						crcVal			= 0;
	uInt32						uiDataLen		= 0;
	uInt32						pv				= 0;
	char					   *pNI_AttrType	= nil;
	tAttributeValueEntryPtr		pOutAttrValue	= nil;
	sNIContextData			   *pContext		= nil;
	ni_namelist					niNameList;		// values;
	ni_index					niIndex;

	gNetInfoMutex->Wait();

	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );
		if ( inData->fInAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

		pNI_AttrType = MapAttrToNetInfoType( inData->fInAttrType->fBufferData );
		if ( pNI_AttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			NI_INIT( &niNameList );
			niStatus = ::ni_listprops( aNIDomain, &pContext->dirID, &niNameList );
			if ( niStatus == NI_OK )
			{
				niIndex = ::ni_namelist_match( niNameList, pNI_AttrType );
				::ni_namelist_free( &niNameList );

				if ( niIndex != NI_INDEX_NULL )
				{
					NI_INIT( &niNameList );
					niStatus = ::ni_readprop( aNIDomain, &pContext->dirID, niIndex, &niNameList );
					if ( niStatus == NI_OK )
					{
						siResult = eDSAttributeValueNotFound;
						// For each value in the namelist for this property
						for ( pv = 0; (pv < niNameList.ni_namelist_len) && (done == false); pv++ )
						{
							crcVal = CalcCRC( niNameList.ni_namelist_val[ pv ] );

							if ( crcVal == inData->fInValueID )
							{
								if ( niNameList.ni_namelist_val[ pv ] != nil )
								{
									uiDataLen = ::strlen( niNameList.ni_namelist_val[ pv ] );
								}
								else
								{
									uiDataLen = 0;
								}

								pOutAttrValue = (tAttributeValueEntry *)::calloc( (sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad), sizeof( char ) );

	//							pOutAttrValue = (tAttributeValueEntry *)::malloc( sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
	//							::memset( pOutAttrValue, 0, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );

								pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen;
								pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
								if ( niNameList.ni_namelist_val[ pv ] != nil )
								{
									pOutAttrValue->fAttributeValueID = crcVal;
									::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, niNameList.ni_namelist_val[ pv ], uiDataLen ); 
								}

								inData->fOutEntryPtr = pOutAttrValue;

								done = true;
								siResult = eDSNoErr;

								break;
							}
						}
						::ni_namelist_free( &niNameList );
					}
				}
			}

			delete( pNI_AttrType );
			pNI_AttrType = nil;
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // GetRecordAttributeValueByID


//------------------------------------------------------------------------------------
//	* GetRecAttrValueByIndex
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetRecAttrValueByIndex ( sGetRecordAttributeValueByIndex *inData )
{
	sInt32						siResult		= eDSNoErr;
	ni_status					niStatus		= NI_OK;
	uInt32						uiDataLen		= 0;
	uInt32						uiIndex			= 0;
	char					   *pNI_AttrType	= nil;
	tAttributeValueEntryPtr		pOutAttrValue	= nil;
	sNIContextData			   *pContext		= nil;
	ni_id						niDirID;
	ni_namelist					niNameList;		// values;
	ni_index					niIndex;

	gNetInfoMutex->Wait();

	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );
		if ( inData->fInAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

		pNI_AttrType = MapAttrToNetInfoType( inData->fInAttrType->fBufferData );
		if ( pNI_AttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );

		// Get the index value
		uiIndex = inData->fInAttrValueIndex;

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			// Get the domain and dir ID
			::memcpy( &niDirID, &pContext->dirID, sizeof( ni_id ) );

			NI_INIT( &niNameList );
			niStatus = ::ni_listprops( aNIDomain, &niDirID, &niNameList );
			if ( niStatus == NI_OK )
			{
				niIndex = ::ni_namelist_match( niNameList, pNI_AttrType );
				::ni_namelist_free( &niNameList );

				if ( niIndex != NI_INDEX_NULL )
				{
					NI_INIT( &niNameList );
					niStatus = ::ni_readprop( aNIDomain, &niDirID, niIndex, &niNameList );
					if ( niStatus == NI_OK )
					{
						if ( ( niNameList.ni_namelist_len >= uiIndex ) && (uiIndex > 0) )
						{
							if ( niNameList.ni_namelist_val[ uiIndex - 1 ] != nil )
							{
								uiDataLen = ::strlen( niNameList.ni_namelist_val[ uiIndex - 1 ] );
							}
							else
							{
								uiDataLen = 0;
							}

							pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
	//						::memset( pOutAttrValue, 0, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );

							pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen;
							pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
							if ( niNameList.ni_namelist_val[ uiIndex - 1 ] != nil )
							{
								pOutAttrValue->fAttributeValueID = CalcCRC( niNameList.ni_namelist_val[ uiIndex - 1 ] );
								::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, niNameList.ni_namelist_val[ uiIndex - 1 ], uiDataLen ); 
							}

							inData->fOutEntryPtr = pOutAttrValue;
						}
						else
						{
							siResult = eDSIndexOutOfRange;
						}

						::ni_namelist_free( &niNameList );
					}
				}
				else
				{
					siResult = eDSAttributeNotFound;
				}
			}

			delete( pNI_AttrType );
			pNI_AttrType = nil;
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // GetRecAttrValueByIndex


//------------------------------------------------------------------------------------
//	* GetRecAttrValueByValue
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetRecAttrValueByValue ( sGetRecordAttributeValueByValue *inData )
{
	sInt32						siResult		= eDSNoErr;
	uInt32						uiDataLen		= 0;
	char					   *pNIRecType		= nil;
	char					   *pNI_AttrType	= nil;
	char					   *pAttrValue		= nil;
	char					   *pRecValuePath   = nil;
	tAttributeValueEntryPtr		pOutAttrValue	= nil;
	sNIContextData			   *pContext		= nil;
	ni_namelist					niNameList;

	gNetInfoMutex->Wait();

	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );
		if ( pContext->fRecType == nil ) throw( (sInt32)eDSEmptyRecordType );
		if ( pContext->fRecName == nil ) throw( (sInt32)eDSEmptyRecordName );
		if ( inData->fInAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		if ( inData->fInAttrValue == nil ) throw( (sInt32)eDSEmptyAttributeValue );

		pNIRecType = MapRecToNetInfoType( pContext->fRecType );
		if ( pNIRecType == nil ) throw( (sInt32)eDSInvalidRecordType );

		pNI_AttrType = MapAttrToNetInfoType( inData->fInAttrType->fBufferData );
		if ( pNI_AttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );

		pAttrValue = NormalizeNIString( inData->fInAttrValue->fBufferData, pNI_AttrType );
		if ( pAttrValue == nil ) throw( (sInt32)eDSNullAttributeValue );

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			ni_id		recordDirID;
			pRecValuePath = (char*)calloc( 1 + strlen( pNIRecType ) + 1 + strlen(pContext->fRecName) + 1, 1 );
			pRecValuePath[0] = '/';
			strcat( pRecValuePath, pNIRecType );
			strcat( pRecValuePath, "/" );
			strcat( pRecValuePath, pContext->fRecName );
			
			siResult = ::ni_pathsearch( aNIDomain, &recordDirID, pRecValuePath);
			
			if (siResult == eDSNoErr)
			{
				siResult = ni_lookupprop( aNIDomain, &recordDirID, pNI_AttrType, &niNameList );

				// if property exists
				if ( siResult == eDSNoErr )
				{
					siResult = eDSAttributeValueNotFound;
					sInt32 valIndex = 0;
					valIndex = niNameList.ni_namelist_len - 1;
					while (valIndex >= 0)
					{
						if (niNameList.ni_namelist_val[valIndex] != nil)
						{
							if (strcmp(niNameList.ni_namelist_val[valIndex], pAttrValue) == 0)
							{
								uiDataLen = strlen(pAttrValue);
								pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );

								pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen;
								pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
								pOutAttrValue->fAttributeValueID = CalcCRC( pAttrValue );
								::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, pAttrValue, uiDataLen ); 

								inData->fOutEntryPtr = pOutAttrValue;
								siResult = eDSNoErr;
								valIndex = 0; //exit while
							}
						}
						valIndex--;
					}
					ni_namelist_free(&niNameList);
				}
				else
				{
					siResult = eDSAttributeNotFound;
				}
			}
			else
			{
				siResult = eDSAttributeNotFound;
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	DSFreeString( pNI_AttrType );
	DSFreeString( pRecValuePath );
	if ( ( pAttrValue != nil) && ( pAttrValue != inData->fInAttrValue->fBufferData ) )
	{
		DSFreeString( pAttrValue );
	}
	
	return( siResult );

} // GetRecAttrValueByValue


//------------------------------------------------------------------------------------
//	* CreateRecord
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::CreateRecord ( sCreateRecord *inData )
{
	volatile sInt32		siResult		= eDSNoErr;
	tDataNodePtr		pRecName		= nil;
	tDataNodePtr		pRecType		= nil;
	char			   *pNIRecType		= nil;
	char			   *pNIRecName		= nil;
	sNIContextData	   *pContext		= nil;
	sNIContextData	   *pRecContext		= nil;
	ni_id				niDirID;
	char			   *pPath			= nil;
	char			   *pRecTypePath	= nil;
	ni_namelist	 		niValues;
	char			   *authUser		= nil;

	gNetInfoMutex->Wait();

	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		pRecType = inData->fInRecType;
		if ( pRecType == nil ) throw( (sInt32)eDSNullRecType );

		pRecName = inData->fInRecName;
		if ( pRecName == nil ) throw( (sInt32)eDSNullRecName );

		pNIRecType = MapRecToNetInfoType( pRecType->fBufferData );
		if ( pNIRecType == nil ) throw( (sInt32)eDSInvalidRecordType );

		pNIRecName =  pRecName->fBufferData;
		if ( pNIRecName == nil ) throw( (sInt32)eDSInvalidRecordName );

		pPath = BuildRecordNamePath( pNIRecName, pNIRecType );
		if ( pPath == nil ) throw( (sInt32)eDSInvalidRecordName );
		
		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			NI_INIT( &niDirID );
			(void)::ni_root( aNIDomain, &niDirID );

			// check access for non-admin users
			if ( ( pContext->fEffectiveUID != 0 )
				 && ( (pContext->fAuthenticatedUserName == NULL) 
					  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
			{
				authUser = pContext->fAuthenticatedUserName;
				ni_proplist niPropList;
				ni_id		typeDirID;
				
				pRecTypePath = (char*)calloc( strlen( pNIRecType ) + 2, 1 );
				pRecTypePath[0] = '/';
				strcpy( pRecTypePath + 1, pNIRecType );
				
				siResult = ::ni_pathsearch( aNIDomain, &typeDirID, pRecTypePath);
				while ((siResult != eDSNoErr) && (pRecTypePath[0] != '\0'))
				{
					//case where record type container is not yet present
					//so let's search upwards to a directory that does exist
					char *lastSlashPtr = nil;
					lastSlashPtr = strrchr(pRecTypePath, '/');
					lastSlashPtr[0] = '\0';
					siResult = ::ni_pathsearch( aNIDomain, &typeDirID, pRecTypePath);
				}
				if ( siResult == eDSNoErr )
				{
					siResult = ::ni_read( aNIDomain, &typeDirID, &niPropList );
					if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
		
					siResult = NiLib2::ValidateDir( authUser, &niPropList );
					::ni_proplist_free( &niPropList );
				}
				
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}
			
			//ValidateCreateRecord
			siResult = DoCreateRecord( aNIDomain, &niDirID, pPath );
			if ( siResult != eDSNoErr ) throw( siResult );

			//make sure that the type is users only here when we add these attributes
			if ( (siResult == eDSNoErr) && (strcmp(pNIRecType,"users") == 0) )
			{
				NI_INIT( &niValues );
				::ni_namelist_insert( &niValues, pRecName->fBufferData, NI_INDEX_NULL );

				siResult = DoAddAttribute( aNIDomain, &niDirID, (const ni_name)"_writers_passwd", niValues );
				
	#ifdef TIM_CLIENT_PRESENT
				siResult = DoAddAttribute( aNIDomain, &niDirID, (const ni_name)"_writers_tim_password", niValues );
	#endif
				
				siResult = DoAddAttribute( aNIDomain, &niDirID, (const ni_name)"_writers_picture", niValues );
				
				::ni_namelist_free( &niValues );
			}

			free(pPath);
			pPath = nil;

			if ( (strcmp(pNIRecType,"users") == 0) )
			{
				gbBuildNILocalUserCache = true;
			}

			if ( inData->fInOpen == true )
			{
				pRecContext = MakeContextData();
				if ( pRecContext == nil ) throw( (sInt32)eMemoryAllocError );
				if (pContext->fDomainName != nil)
				{
					pRecContext->fDomainName = strdup(pContext->fDomainName);
				}
				if ( pContext->fDomainPath != nil)
				{
					pRecContext->fDomainPath = strdup(pContext->fDomainPath);
				}
				pRecContext->bIsLocal = pContext->bIsLocal;
				::memcpy( &pRecContext->dirID, &niDirID, sizeof( ni_id ) );
				pRecContext->fUID = pContext->fUID;
				pRecContext->fEffectiveUID = pContext->fEffectiveUID;
				if (pContext->fAuthenticatedUserName != NULL)
				{
					pRecContext->fAuthenticatedUserName = strdup( pContext->fAuthenticatedUserName );
				}

				pRecContext->fRecType = (char *)::malloc( ::strlen( pRecType->fBufferData ) + 2 );
				pRecContext->fRecType[0] = '\0';
				::strcpy( pRecContext->fRecType, pRecType->fBufferData );

				pRecContext->fRecName = (char *)::malloc( pRecName->fBufferLength + 2 );
				pRecContext->fRecName[0] = '\0';
				::strcpy( pRecContext->fRecName, pRecName->fBufferData );

				gNINodeRef->AddItem( inData->fOutRecRef, pRecContext );
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
		DSFreeString(pPath);
	}
	
	if (authUser != nil)
	{
		if (pContext != nil)
		{
			if (pContext->fAuthenticatedUserName == nil)
			{
				free(authUser);
				authUser = nil;
			}
		}
	}

	DSFreeString( pRecTypePath );
	DSDelete( pNIRecType );
	
	gNetInfoMutex->Signal();

	return( siResult );

} // CreateRecord



//------------------------------------------------------------------------------------
//	* DoCreateRecord
//
//		- Taken from ni2_createpath but modified to be readable
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoCreateRecord ( void *inDomain, ni_id *inDir, char *inPathName )
{
	sInt32		siResult	= eDSNoErr;
	ni_status	niStatus	= NI_OK;
	sInt32		siCntr1		= 0;
	sInt32		siCntr2		= 0;
	sInt32		siLen		= 0;
	bool		bSimple		= false;
	bool		bMadeOne	= false;
	ni_id		niTmpDir;
	CString		csDirName( 128 );

	gNetInfoMutex->Wait();

	try
	{
		// Pull out every pathname component and create the directory
		siCntr1 = 0;

		while ( (inPathName[ siCntr1 ] != '\0') && (niStatus == NI_OK) )
		{
			// Search forward for a path component ( a directory )
			bSimple = true;
			for ( siCntr2 = siCntr1; inPathName[ siCntr2 ] != '\0' && bSimple; siCntr2++ )
			{
				if ( inPathName[ siCntr2 ] == '\\' && inPathName[ siCntr2 + 1 ] == '/' )
				{
					if ( inPathName[ siCntr2 + 2 ] == '/' )
					{
						bSimple = false;
					}
					siCntr2++;
					// only advance by one since the for loop advances by the additional one
					// this skips over both the \ and the /
				}
				else if ( inPathName[ siCntr2 ] == '/' )
				{
					bSimple = false;
				}
			}

			siLen = siCntr2 - siCntr1;
			if ( !bSimple )
			{
				siLen--;
			}
			csDirName.Set( inPathName + siCntr1, siLen );

			// Advance the pointer
			siCntr1 = siCntr2;

			// Does this directory exist?
			//this handles the "/"s correctly since the input to the function is adjusted
			::memcpy( &niTmpDir, inDir, sizeof( ni_id ) );
			niStatus = ::ni_pathsearch( inDomain, inDir, csDirName.GetData() );

			// If it doesn't exist, create it
			if ( niStatus == NI_NODIR )
			{
				::memcpy( inDir, &niTmpDir, sizeof( ni_id ) );
				niStatus = DoCreateChild( inDomain, inDir, csDirName.GetData() );
				bMadeOne = true;
			}
		}

		if ( niStatus == NI_OK )
		{
			if ( bMadeOne == false )
			{
				siResult = eDSRecordAlreadyExists;
			}
		}
		else
		{
			siResult = MapNetInfoErrors( niStatus );
		}
	}


	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // DoCreateRecord



//------------------------------------------------------------------------------------
//	* DoCreateChild
//
//		- Taken from ni2_createchild but modified to be readable
//------------------------------------------------------------------------------------

ni_status CNiPlugIn::DoCreateChild ( void *inDomain, ni_id *inDir, const ni_name inDirName )
{
	ni_status		niStatus	= NI_OK;
	sInt32			siCntr1		= 0;
	sInt32			siCntr2		= 0;
	sInt32			siLen		= 0;
	ni_proplist		niPropList;
	ni_id			niChild;
	char		   *pKey	= nil;
	char		   *pValue	= nil;

	gNetInfoMutex->Wait();

	try
	{
		pKey = (char *)::malloc( 5 );
		strcpy( pKey, "name" );

		// compress out backslashes in value
		siLen = strlen( inDirName );
		pValue = (char *)::malloc( siLen + 1 );
		for ( siCntr1 = 0, siCntr2 = 0; siCntr1 < siLen; siCntr1++, siCntr2++ )
		{
			if ( inDirName[ siCntr1 ] == '\\'
				&& ( (inDirName[ siCntr1 + 1 ] == '/') || (inDirName[ siCntr1 + 1 ] == '\\') ) )
			{
				siCntr1++;
			}
			pValue[ siCntr2 ] = inDirName[ siCntr1 ];
		}
		pValue[ siCntr2 ] = '\0';

		// set up the new directory
		NI_INIT( &niPropList );
		NiLib3::AppendProp( &niPropList, pKey, pValue );

		// create it
		niStatus = ::ni_create( inDomain, inDir, niPropList, &niChild, NI_INDEX_NULL );
		if ( niStatus == NI_OK )
		{
			::memcpy( inDir, &niChild, sizeof( ni_id ) );
		}

		::ni_proplist_free( &niPropList );

		if ( pKey != nil )
		{
			free( pKey );
			pKey = nil;
		}

		if ( pValue != nil )
		{
			free( pValue );
			pValue = nil;
		}
	}

	catch( sInt32 err )
	{
		niStatus = (ni_status)err;
	}

	gNetInfoMutex->Signal();

	return( niStatus );

} // DoCreateChild


//------------------------------------------------------------------------------------
//	* CloseAttributeList
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::CloseAttributeList ( sCloseAttributeList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sNIContextData	   *pContext		= nil;

	pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInAttributeListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gNINodeRef->RemoveItem( inData->fInAttributeListRef );
	}
	else
	{
		siResult = eDSInvalidAttrListRef;
	}

	return( siResult );

} // CloseAttributeList


//------------------------------------------------------------------------------------
//	* CloseAttributeValueList
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sNIContextData	   *pContext		= nil;

	pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInAttributeValueListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gNINodeRef->RemoveItem( inData->fInAttributeValueListRef );
	}
	else
	{
		siResult = eDSInvalidAttrValueRef;
	}

	return( siResult );

} // CloseAttributeValueList


//------------------------------------------------------------------------------------
//	* OpenRecord
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::OpenRecord ( sOpenRecord *inData )
{
	sInt32				siResult		= eDSNoErr;
	tDataNodePtr		pRecName		= nil;
	tDataNodePtr		pRecType		= nil;
	char			   *pNIRecName		= nil;
	char			   *pNIRecType		= nil;
	sNIContextData	   *pContext		= nil;
	sNIContextData	   *pRecContext		= nil;
	ni_id				niDirID;
	bool				bFoundRealname	= false;
	char			   *aRealname		= nil;

	gNetInfoMutex->Wait();

	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		pRecType = inData->fInRecType;
		if ( pRecType == nil ) throw( (sInt32)eDSNullRecType );

		pRecName = inData->fInRecName;
		if ( pRecName == nil ) throw( (sInt32)eDSNullRecName );
		pNIRecName = pRecName->fBufferData;

		pNIRecType = MapRecToNetInfoType( pRecType->fBufferData );
		if ( pNIRecType == nil ) throw( (sInt32)eDSInvalidRecordType );

		if (pRecName->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordNameList );
		if (pRecType->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordTypeList );

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			NI_INIT( &niDirID );
			siResult = IsValidRecordName( pNIRecName, pNIRecType, aNIDomain, niDirID );
			if (siResult != eDSNoErr)
			{
				//here we can check if the record type is /users so that we will search now on "realname" as well
				if (::strcmp(pNIRecType,"users") == 0)
				{
					siResult = IsValidRealname( pNIRecName, aNIDomain, &aRealname );
					//need to get the correct niDirID and set the true record name "aRealname" for the context data
					if (siResult == eDSNoErr)
					{
						bFoundRealname = true;
						siResult = IsValidRecordName( aRealname, pNIRecType, aNIDomain, niDirID );
					}
				}
				if ( siResult != eDSNoErr ) throw( siResult );
			}

			pRecContext = MakeContextData();
			if ( pRecContext == nil ) throw( (sInt32)eMemoryAllocError );

			if (pContext->fDomainName != nil)
			{
				pRecContext->fDomainName = strdup(pContext->fDomainName);
			}
			if ( pContext->fDomainPath != nil)
			{
				pRecContext->fDomainPath = strdup(pContext->fDomainPath);
			}
			pRecContext->bIsLocal = pContext->bIsLocal;
			pRecContext->fUID = pContext->fUID;
			pRecContext->fEffectiveUID = pContext->fEffectiveUID;
			if (pContext->fAuthenticatedUserName != NULL)
			{
				pRecContext->fAuthenticatedUserName = strdup( pContext->fAuthenticatedUserName );
			}
			
			pRecContext->fRecType = strdup( pRecType->fBufferData );

			if (bFoundRealname)
			{
				if (aRealname != nil)
				{
					pRecContext->fRecName = aRealname;
				}
			}
			else
			{
				pRecContext->fRecName = strdup( pNIRecName );
			}

			::memcpy( &pRecContext->dirID, &niDirID, sizeof( ni_id ) );

			gNINodeRef->AddItem( inData->fOutRecRef, pRecContext );
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		if (siResult == eDSInvalidRecordName)
		{
			siResult = eDSRecordNotFound; //retaining consistent error code returned to clients
		}
		else
		{
			siResult = err;
		}
	}

	DSDelete( pNIRecType );

	gNetInfoMutex->Signal();

	return( siResult );

} // OpenRecord



//------------------------------------------------------------------------------------
//	* GetRecRefInfo
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetRecRefInfo ( sGetRecRefInfo *inData )
{
	sInt32					siResult		= eDSNoErr;
	uInt32					uiRecSize		= 0;
	uInt32					uiRecTypeSize	= 0;
	uInt32					uiRecNameSize	= 0;
	tRecordEntry		   *pRecEntry		= nil;
	sNIContextData		   *pContext		= nil;

//KW temp fix for the member buffer vars is to put in the mutex here even though
//there is no netinfo call

	gNetInfoMutex->Wait();
	
	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );

		if ( pContext->fRecType != nil )
		{
			uiRecTypeSize = ::strlen( pContext->fRecType );
		}

		if ( pContext->fRecName != nil )
		{
			uiRecNameSize = ::strlen( pContext->fRecName );
		}

		uiRecSize = sizeof( tRecordEntry ) + uiRecTypeSize + uiRecNameSize + 8;
		pRecEntry = (tRecordEntry *)::calloc( 1, uiRecSize );

		fRecData->Clear();

		// Record type
		fRecData->AppendShort( uiRecNameSize );
		fRecData->AppendString( pContext->fRecName );

		fRecData->AppendShort( uiRecTypeSize );
		fRecData->AppendString( pContext->fRecType );

		::memcpy( pRecEntry->fRecordNameAndType.fBufferData, fRecData->GetData(), fRecData->GetLength() );
		pRecEntry->fRecordNameAndType.fBufferSize = uiRecTypeSize + uiRecNameSize + 8;
		pRecEntry->fRecordNameAndType.fBufferLength = fRecData->GetLength();

		inData->fOutRecInfo = pRecEntry;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // GetRecRefInfo


//------------------------------------------------------------------------------------
//	* CloseRecord
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::CloseRecord ( sCloseRecord *inData )
{
	sInt32				siResult		= eDSNoErr;
	sNIContextData	   *pContext		= nil;
	bool				bResetCache		= false;

	gNetInfoMutex->Wait();

	pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
	if ( pContext != nil )
	{
		if ( (pContext->bIsLocal) && (pContext->bDidWriteOperation) )
		{
			char* pNIRecType = MapRecToNetInfoType( pContext->fRecType );
			if (pNIRecType != nil)
			{
				if ( (strcmp(pNIRecType,"users") == 0) || (strcmp(pNIRecType,"groups") == 0) )
				{
					//flush memberd cache since we edited a user or group record
					bResetCache = true;
				}
				if ( (strcmp(pNIRecType,"users") == 0) )
				{
					gbBuildNILocalUserCache = true;
				}
				DSFreeString(pNIRecType);
			}
		}

		gNINodeRef->RemoveItem( inData->fInRecRef );
	}
	else
	{
		siResult = eDSInvalidRecordRef;
	}

	gNetInfoMutex->Signal();
	
	if ( bResetCache )
	{
		// be sure not to hold the NetInfo mutex while calling this
		gSrvrCntl->HandleMemberDaemonFlushCache();
	}

	return( siResult );

} // CloseRecord


//------------------------------------------------------------------------------------
//	* SetRecordType
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::SetRecordType ( sSetRecordType *inData )
{
	sInt32					siResult		= eDSNoErr;
	tDataNode			   *pData			= nil;
	char				   *pNewType		= nil;
	char				   *pNIRecType		= nil;
	char				   *pRecTypePath	= nil;
	sNIContextData		   *pContext		= nil;
	char				   *pPath			= nil;
	char				   *authUser		= nil;

	gNetInfoMutex->Wait();

	try
	{
		// Get the new name
		pData = inData->fInNewRecType;
		if ( pData == nil ) throw( (sInt32)eDSNullRecType );

		pNewType = pData->fBufferData;

		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );

		pNIRecType = MapRecToNetInfoType( pNewType );
		if ( pNIRecType == nil ) throw( (sInt32)eDSInvalidRecordType );

		pPath = BuildRecordNamePath( pContext->fRecName, pNIRecType );
		if ( pPath == nil ) throw( (sInt32)eDSInvalidRecordName );

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			// check access for non-admin users
			if ( ( pContext->fEffectiveUID != 0 )
				 && ( (pContext->fAuthenticatedUserName == NULL) 
					  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
			{
				authUser = pContext->fAuthenticatedUserName;
				ni_proplist niPropList;
				ni_id		niDirID;
				ni_index	tempIndex = NI_INDEX_NULL;

				::memcpy( &niDirID, &pContext->dirID, sizeof( ni_id ) );
				siResult = ::ni_parent( aNIDomain, &niDirID, &tempIndex );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
				niDirID.nii_object = tempIndex;
				siResult = ::ni_self( aNIDomain, &niDirID );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
				siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

				// must have write to the parent type of existing record...
				siResult = NiLib2::ValidateDir( authUser, &niPropList );
				::ni_proplist_free( &niPropList );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

				pRecTypePath = (char*)calloc( strlen( pNIRecType ) + 2, 1 );
				pRecTypePath[0] = '/';
				strcpy( pRecTypePath + 1, pNIRecType );
				
				siResult = ::ni_pathsearch( aNIDomain, &niDirID, pRecTypePath );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
				siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

				// and the destination type (parent of new record)
				siResult = NiLib2::ValidateDir( authUser, &niPropList );
				::ni_proplist_free( &niPropList );			

				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}
			
			siResult = NiLib2::Copy( aNIDomain, &pContext->dirID, pPath, aNIDomain, true );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			siResult = NiLib2::Destroy( aNIDomain, &pContext->dirID );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			//could have called IsValidRecordName here but since pPath already built above no need to
			siResult = ::ni_pathsearch( aNIDomain, &pContext->dirID, pPath );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			DSFreeString(pPath);
			pContext->bDidWriteOperation = true;
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
		DSFreeString(pPath);
	}
	
	if (authUser != nil)
	{
		if (pContext != nil)
		{
			if (pContext->fAuthenticatedUserName == nil)
			{
				free(authUser);
				authUser = nil;
			}
		}
	}

	if ( pRecTypePath != nil )
	{
		free( pRecTypePath );
		pRecTypePath = nil;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // SetRecordType


//------------------------------------------------------------------------------------
//	* DeleteRecord
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DeleteRecord ( sDeleteRecord *inData )
{
	sInt32					siResult		= eDSNoErr;
	ni_status				niStatus		= NI_OK;
	sNIContextData		   *pContext		= nil;
	char				   *authUser		= nil;
	ni_namelist				niValuesGUID;
	bool					bResetCache		= false;

	gNetInfoMutex->Wait();

	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext != nil )
		{
			void * aNIDomain = RetrieveNIDomain(pContext);
			if (aNIDomain != NULL)
			{
				// check access for non-admin users
				if ( ( pContext->fEffectiveUID != 0 )
					&& ( (pContext->fAuthenticatedUserName == NULL) 
						|| (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
				{
					authUser = pContext->fAuthenticatedUserName;
					ni_proplist niPropList;
					ni_id		parentDir;
					ni_index	tempIndex = NI_INDEX_NULL;

					::memcpy( &parentDir, &pContext->dirID, sizeof( ni_id ) );
					siResult = ::ni_parent( aNIDomain, &parentDir, &tempIndex );
					if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
					parentDir.nii_object = tempIndex;
					siResult = ::ni_self( aNIDomain, &parentDir );
					if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
					siResult = ::ni_read( aNIDomain, &parentDir, &niPropList );
					if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

					siResult = NiLib2::ValidateDir( authUser, &niPropList );
					::ni_proplist_free( &niPropList );

					if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
				}
				
				{
					//need the recordname and GUID
					//lookup GUID for ShadowHash but don't worry if not found
					ni_status niResultGUID = ni_lookupprop( aNIDomain, &pContext->dirID, 
												"generateduid", &niValuesGUID );
					char* GUIDString = nil;
					if (niResultGUID == NI_OK)
					{
						if (niValuesGUID.ni_namelist_len > 0)
						{
							if (niValuesGUID.ni_namelist_val[0] != nil)
							{
								GUIDString = strdup(niValuesGUID.ni_namelist_val[0]);
							}
							ni_namelist_free(&niValuesGUID);
						}
					}
					//check to remove all corresponding hash files
					RemoveShadowHash( pContext->fRecName, GUIDString, true );
					if (GUIDString != nil)
					{
						free(GUIDString);
						GUIDString = nil;
					}
				}
				
				niStatus = NiLib2::Destroy( aNIDomain, &pContext->dirID );
				if ( niStatus == NI_NODIR )
				{
					// Record was not found...
					siResult = eDSRecordNotFound;
				}
				else if ( niStatus != NI_OK )
				{
					siResult = MapNetInfoErrors( niStatus );
				}
				else
				{
					if (pContext->bIsLocal)
					{
						char* pNIRecType = MapRecToNetInfoType( pContext->fRecType );
						if (pNIRecType != nil)
						{
							if ( (strcmp(pNIRecType,"users") == 0) || (strcmp(pNIRecType,"groups") == 0) )
							{
								//flush memberd cache since we deleted a user or group record
								bResetCache = true;
							}
							if ( (strcmp(pNIRecType,"users") == 0) )
							{
								gbBuildNILocalUserCache = true;
							}
							DSFreeString(pNIRecType);
						}
					}
					gNINodeRef->RemoveItem( inData->fInRecRef );
				}
			}
			else
			{
				throw( (sInt32)eDSInvalidSession );
			}
		}
		else
		{
			siResult = eDSInvalidRecordRef;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if (authUser != nil)
	{
		if (pContext != nil)
		{
			if (pContext->fAuthenticatedUserName == nil)
			{
				free(authUser);
				authUser = nil;
			}
		}
	}

	gNetInfoMutex->Signal();
	
	if ( bResetCache )
	{
		// be sure not to hold the NetInfo mutex while calling this
		gSrvrCntl->HandleMemberDaemonFlushCache();
	}
	
	return( siResult );

} // DeleteRecord



//------------------------------------------------------------------------------------
//	* SetRecordName
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::SetRecordName ( sSetRecordName *inData )
{
	sInt32					siResult		= eDSNoErr;
	bool					bFreePropList	= false;
	uInt32					uiNameLen		= 0;
	char				   *pNewName		= nil;
	sNIContextData		   *pContext		= nil;
	ni_proplist				niPropList;
	ni_index				niWhere;
	ni_namelist				niValues;
	ni_namelist				niCurrentName;
	char				   *pOldName		= nil;
	uInt32					pv				= 0;
	ni_namelist				niValue;
	ni_property				niProp;
	ni_namelist				niNameList;
	bool					bDidNotFindValue= true;
	char				   *pNIRecType		= nil;
	char				   *authUser		= nil;

	gNetInfoMutex->Wait();

	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			// need to be talking to the master
			::ni_needwrite( aNIDomain, 1 );

			siResult = ::ni_read( aNIDomain, &pContext->dirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			bFreePropList = true;

			niWhere = ::ni_proplist_match( niPropList, "name", nil );
			if (niWhere == NI_INDEX_NULL) throw( (sInt32)ePlugInError );

			// check access for non-admin users
			if ( ( pContext->fEffectiveUID != 0 )
				 && ( (pContext->fAuthenticatedUserName == NULL) 
					  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
			{
				authUser = pContext->fAuthenticatedUserName;
				siResult = NiLib2::ValidateDir( authUser, &niPropList );
				if ( siResult != NI_OK )
				{
					siResult = NiLib2::ValidateName( authUser, &niPropList, niWhere );
				}

				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}

			//read the current name
			NI_INIT(&niCurrentName);
			siResult = ::ni_readprop(aNIDomain, &pContext->dirID, niWhere, &niCurrentName);
			//name replaced below is first one so only get that one
			pOldName = strdup( niCurrentName.ni_namelist_val[ 0 ] );
			::ni_namelist_free( &niCurrentName );		

			// Get the new name
			pNewName = inData->fInNewRecName->fBufferData;
			if ( pNewName == nil ) throw( (sInt32)eMemoryAllocError );

			uiNameLen = ::strlen( pNewName );
			if (uiNameLen == 0) throw( (sInt32)eDSNullRecName );

			// Remove any unexpected characters in the record name
			this->MakeGood( pNewName, pNewName );

			// Now set it
			siResult = ::ni_writename( aNIDomain, &pContext->dirID, niWhere, 0, pNewName );
			siResult = MapNetInfoErrors( siResult );

			//make sure that the type is users only here for setting additional attributes
			if (pContext->fRecType != nil)
			{
				pNIRecType = MapRecToNetInfoType( pContext->fRecType );
			}

			if ( (siResult == eDSNoErr) && (pNIRecType != nil) && (::strcmp(pNIRecType,"users") == 0) )
			{
				//create and set the attribute _writers_passwd

				//check if it exists if so replace otherwise create
				niWhere = ::ni_proplist_match( niPropList, "_writers_passwd", nil );
				if (niWhere != NI_INDEX_NULL)
				{
					//now replace the old with the new
					niProp = niPropList.ni_proplist_val[ niWhere ];
					niValue = niProp.nip_val;

					// For each value in the namelist for this property
					for ( pv = 0; pv < niValue.ni_namelist_len; pv++ )
					{
						if ( strcmp( niValue.ni_namelist_val[ pv ], pOldName ) == 0 )
						{
							NI_INIT( &niNameList );
							::ni_namelist_insert( &niNameList, niValue.ni_namelist_val[ pv ], NI_INDEX_NULL );

							siResult = NiLib2::DestroyDirVal( aNIDomain, &pContext->dirID, (const ni_name)"_writers_passwd", niNameList );

							siResult = NiLib2::InsertDirVal( aNIDomain, &pContext->dirID, (const ni_name)"_writers_passwd", pNewName, pv );
							siResult = MapNetInfoErrors( siResult );

							::ni_namelist_free( &niNameList );

							bDidNotFindValue = false;

							break;
						}
					}
					if (bDidNotFindValue)
					{
						siResult = NiLib2::InsertDirVal( aNIDomain, &pContext->dirID, (const ni_name)"_writers_passwd", pNewName, niValue.ni_namelist_len );
						siResult = MapNetInfoErrors( siResult );
					}
				}
				else
				{
					NI_INIT( &niValues );
					::ni_namelist_insert( &niValues, pNewName, NI_INDEX_NULL );

					siResult = DoAddAttribute( aNIDomain, &pContext->dirID, (const ni_name)"_writers_passwd", niValues );
				
					::ni_namelist_free( &niValues );
				}
				
				//create and set the attribute _writers_picture

				//check if it exists if so replace otherwise create
				niWhere = ::ni_proplist_match( niPropList, "_writers_picture", nil );
				if (niWhere != NI_INDEX_NULL)
				{
					//now replace the old with the new
					niProp = niPropList.ni_proplist_val[ niWhere ];
					niValue = niProp.nip_val;

					// For each value in the namelist for this property
					for ( pv = 0; pv < niValue.ni_namelist_len; pv++ )
					{
						if ( strcmp( niValue.ni_namelist_val[ pv ], pOldName ) == 0 )
						{
							NI_INIT( &niNameList );
							::ni_namelist_insert( &niNameList, niValue.ni_namelist_val[ pv ], NI_INDEX_NULL );

							siResult = NiLib2::DestroyDirVal( aNIDomain, &pContext->dirID, (const ni_name)"_writers_picture", niNameList );

							siResult = NiLib2::InsertDirVal( aNIDomain, &pContext->dirID, (const ni_name)"_writers_picture", pNewName, pv );
							siResult = MapNetInfoErrors( siResult );

							::ni_namelist_free( &niNameList );

							bDidNotFindValue = false;

							break;
						}
					}
					if (bDidNotFindValue)
					{
						siResult = NiLib2::InsertDirVal( aNIDomain, &pContext->dirID, (const ni_name)"_writers_picture", pNewName, niValue.ni_namelist_len );
						siResult = MapNetInfoErrors( siResult );
					}
				}
				else
				{
					NI_INIT( &niValues );
					::ni_namelist_insert( &niValues, pNewName, NI_INDEX_NULL );

					siResult = DoAddAttribute( aNIDomain, &pContext->dirID, (const ni_name)"_writers_picture", niValues );
				
					::ni_namelist_free( &niValues );
				}
				if (siResult == eDSNoErr)
				{
					pContext->bDidWriteOperation = true;
				}

	#ifdef TIM_CLIENT_PRESENT
				bDidNotFindValue = true;
				
				//check if it exists if so replace otherwise create
				niWhere = ::ni_proplist_match( niPropList, "_writers_tim_password", nil );
				if (niWhere != NI_INDEX_NULL)
				{
					//now replace the old with the new
					niProp = niPropList.ni_proplist_val[ niWhere ];
					niValue = niProp.nip_val;

					// For each value in the namelist for this property
					for ( pv = 0; pv < niValue.ni_namelist_len; pv++ )
					{
						if ( strcmp( niValue.ni_namelist_val[ pv ], pOldName ) == 0 )
						{
							NI_INIT( &niNameList );
							::ni_namelist_insert( &niNameList, niValue.ni_namelist_val[ pv ], NI_INDEX_NULL );

							siResult = NiLib2::DestroyDirVal( aNIDomain, &pContext->dirID, (const ni_name)"_writers_tim_password", niNameList );

							siResult = NiLib2::InsertDirVal( aNIDomain, &pContext->dirID, (const ni_name)"_writers_tim_password", pNewName, pv );
							siResult = MapNetInfoErrors( siResult );

							::ni_namelist_free( &niNameList );

							bDidNotFindValue = false;

							break;
						}
					}
					if (bDidNotFindValue)
					{
						siResult = NiLib2::InsertDirVal( aNIDomain, &pContext->dirID, (const ni_name)"_writers_tim_password", pNewName, niValue.ni_namelist_len );
						siResult = MapNetInfoErrors( siResult );
					}
				}
				else
				{
					NI_INIT( &niValues );
					::ni_namelist_insert( &niValues, pNewName, NI_INDEX_NULL );

					siResult = DoAddAttribute( aNIDomain, &pContext->dirID, (const ni_name)"_writers_tim_password", niValues );
				
					::ni_namelist_free( &niValues );
				}
	#endif
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if (authUser != nil)
	{
		if (pContext != nil)
		{
			if (pContext->fAuthenticatedUserName == nil)
			{
				free(authUser);
				authUser = nil;
			}
		}
	}

	if ( bFreePropList == true )
	{
		::ni_proplist_free( &niPropList );
	}

	gNetInfoMutex->Signal();
	
	DSFreeString( pNIRecType );
	DSFreeString( pOldName );

	return( siResult );

} // SetRecordName


//------------------------------------------------------------------------------------
//	* RemoveAttribute
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::RemoveAttribute ( sRemoveAttribute *inData )
{
	sInt32					siResult		= eDSNoErr;
	char				   *pAttrType		= nil;
	sNIContextData		   *pContext		= nil;
	ni_namelist				niAttribute;
	char				   *authUser		= nil;

	gNetInfoMutex->Wait();

	try
	{
		// Record context data
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );

		pAttrType = MapAttrToNetInfoType( inData->fInAttribute->fBufferData );
		if ( pAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			// check access for non-admin users
			if ( ( pContext->fEffectiveUID != 0 )
				 && ( (pContext->fAuthenticatedUserName == NULL) 
					  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
			{
				authUser = pContext->fAuthenticatedUserName;
				if ( ( authUser == NULL ) && (strcmp(pAttrType,"picture") == 0) )
				{
					authUser = GetUserNameForUID( pContext->fEffectiveUID, aNIDomain );
				}
				ni_proplist niPropList;

				siResult = ::ni_read( aNIDomain, &pContext->dirID, &niPropList );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

				siResult = NiLib2::ValidateDir( authUser, &niPropList );
				::ni_proplist_free( &niPropList );

				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}

			// check if the auth authority of a shadow hash user is being removed
			// if so we should also remove the hash file for that user
			if ( (pAttrType != nil) && (strcmp(pAttrType,"authentication_authority") == 0) )
			{
				ni_namelist niValues;
				ni_namelist niValuesGUID;
				NI_INIT( &niValues );
				NI_INIT( &niValuesGUID );
				//lookup authauthority attribute
				siResult = ni_lookupprop( aNIDomain, &pContext->dirID, "authentication_authority", &niValues );
				if ( (siResult == eDSNoErr) && (niValues.ni_namelist_len > 0) && (niValues.ni_namelist_val[0] != nil))
				{
					if ( strcasestr(niValues.ni_namelist_val[0], kDSValueAuthAuthorityShadowHash) != nil )
					{
						siResult = ni_lookupprop( aNIDomain, &pContext->dirID, "generateduid", &niValuesGUID );
						if ( (siResult == eDSNoErr) && (niValuesGUID.ni_namelist_len > 0) && (niValuesGUID.ni_namelist_val[0] != nil))
						{
							//check to remove all corresponding hash files
							RemoveShadowHash( pContext->fRecName, niValuesGUID.ni_namelist_val[0], true );
						}
						if (niValuesGUID.ni_namelist_len > 0)
						{
							ni_namelist_free(&niValuesGUID);
						}
					}
				}
				if (niValues.ni_namelist_len > 0)
				{
					ni_namelist_free(&niValues);
				}
			}
			
			NI_INIT( &niAttribute );
			::ni_namelist_insert( &niAttribute, pAttrType, NI_INDEX_NULL );

			siResult = NiLib2::DestroyDirProp( aNIDomain, &pContext->dirID, niAttribute );
			siResult = MapNetInfoErrors( siResult );
			
			::ni_namelist_free( &niAttribute );

			if (siResult == eDSNoErr)
			{
				pContext->bDidWriteOperation = true;
			}

			if ( pAttrType != nil )
			{
				delete( pAttrType );
				pAttrType = nil;
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if (authUser != nil)
	{
		if (pContext != nil)
		{
			if (pContext->fAuthenticatedUserName == nil)
			{
				free(authUser);
				authUser = nil;
			}
		}
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // RemoveAttribute


//------------------------------------------------------------------------------------
//	* AddAttributeValue
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::AddAttributeValue ( sAddAttributeValue *inData )
{
	sInt32					siResult		= eDSNoErr;
	char				   *pAttrType		= nil;
	char				   *pAttrValue		= nil;
	sNIContextData		   *pContext		= nil;
	ni_namelist				niNameList;
	char				   *authUser		= nil;

	gNetInfoMutex->Wait();

	try
	{
		// Record context data
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );

		pAttrType = MapAttrToNetInfoType( inData->fInAttrType->fBufferData );
		pAttrValue = NormalizeNIString( inData->fInAttrValue->fBufferData, pAttrType );
		if ( pAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			// check access for non-admin users
			if ( ( pContext->fEffectiveUID != 0 )
				 && ( (pContext->fAuthenticatedUserName == NULL) 
					  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
			{
				authUser = pContext->fAuthenticatedUserName;
				if ( ( authUser == NULL ) && (strcmp(pAttrType,"picture") == 0) )
				{
					authUser = GetUserNameForUID( pContext->fEffectiveUID, aNIDomain );
				}
				ni_proplist niPropList;
				ni_index	niIndex;

				siResult = ::ni_read( aNIDomain, &pContext->dirID, &niPropList );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

				siResult = NiLib2::ValidateDir( authUser, &niPropList );
				if ( siResult != NI_OK )
				{
					niIndex = ::ni_proplist_match( niPropList, pAttrType, NULL );
					if ( niIndex != NI_INDEX_NULL )
						siResult = NiLib2::ValidateName( authUser, &niPropList, niIndex );
				}
				::ni_proplist_free( &niPropList );

				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}

			NI_INIT( &niNameList );
			::ni_namelist_insert( &niNameList, pAttrValue, NI_INDEX_NULL );

			siResult = NiLib2::AppendDirProp( aNIDomain, &pContext->dirID, pAttrType, niNameList );
			siResult = MapNetInfoErrors( siResult );

			::ni_namelist_free( &niNameList );

			if (siResult == eDSNoErr)
			{
				pContext->bDidWriteOperation = true;
			}

			DSDelete( pAttrType );
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if (authUser != nil)
	{
		if (pContext != nil)
		{
			if (pContext->fAuthenticatedUserName == nil)
			{
				free(authUser);
				authUser = nil;
			}
		}
	}

	if ( pAttrValue != inData->fInAttrValue->fBufferData )
	{
		DSFreeString( pAttrValue );
	}
	DSDelete( pAttrType );
	
	gNetInfoMutex->Signal();

	return( siResult );

} // AddAttributeValue


//------------------------------------------------------------------------------------
//	* RemoveAttributeValue
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::RemoveAttributeValue ( sRemoveAttributeValue *inData )
{
	bool					done			= false;
	sInt32					siResult		= eDSNoErr;
	bool					bFreePropList	= false;
	uInt32					pn				= 0;
	uInt32					pv				= 0;
	char				   *pAttrType		= nil;
	sNIContextData		   *pContext		= nil;
	ni_namelist				niNameList;
	ni_namelist				niValue;
	ni_property				niProp;
	ni_proplist				niPropList;
	char				   *authUser		= nil;

	gNetInfoMutex->Wait();

	try
	{
		// Record context data
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			// Get the native net info attribute
			pAttrType = MapAttrToNetInfoType( inData->fInAttrType->fBufferData );

			// read the attribute list
			siResult = ::ni_read( aNIDomain, &pContext->dirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			bFreePropList = true;

			// check access for non-admin users
			if ( ( pContext->fEffectiveUID != 0 )
				 && ( (pContext->fAuthenticatedUserName == NULL) 
					  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
			{
				authUser = pContext->fAuthenticatedUserName;
				if ( ( authUser == NULL ) && (strcmp(pAttrType,"picture") == 0) )
				{
					authUser = GetUserNameForUID( pContext->fEffectiveUID, aNIDomain );
				}
				ni_index	niIndex;

				siResult = NiLib2::ValidateDir( authUser, &niPropList );
				if ( siResult != NI_OK )
				{
					niIndex = ::ni_proplist_match( niPropList, pAttrType, NULL );
					siResult = NiLib2::ValidateName( authUser, &niPropList, niIndex );
				}

				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}		
			
			siResult = eDSAttributeNotFound;

	// xxx use ni_namelist_match for better performance

			for ( pn = 0; (pn < niPropList.ni_proplist_len) && (done == false); pn++ )
			{
				niProp = niPropList.ni_proplist_val[ pn ];

				if ( (pAttrType != nil) && (::strcmp( niProp.nip_name, pAttrType ) == 0) )
				{
					niValue = niProp.nip_val;

					// For each value in the namelist for this property
					for ( pv = 0; (pv < niValue.ni_namelist_len) && (done == false); pv++ )
					{
						uInt32 crcVal = CalcCRC( niValue.ni_namelist_val[ pv ] );

						if ( crcVal == inData->fInAttrValueID )
						{
							// check if the auth authority value of shadow hash for a user is being removed
							// if so we should also remove the hash file for that user
							if ( (pAttrType != nil) && (strcmp(pAttrType,"authentication_authority") == 0) )
							{
								ni_namelist niValues;
								ni_namelist niValuesGUID;
								NI_INIT( &niValues );
								NI_INIT( &niValuesGUID );
								//lookup authauthority attribute
								siResult = ni_lookupprop( aNIDomain, &pContext->dirID, "authentication_authority", &niValues );
								if ( (siResult == eDSNoErr) && (niValues.ni_namelist_len > 0) && (niValues.ni_namelist_val[0] != nil))
								{
									if ( strcasestr(niValues.ni_namelist_val[0], kDSValueAuthAuthorityShadowHash) != nil )
									{
										siResult = ni_lookupprop( aNIDomain, &pContext->dirID, "generateduid", &niValuesGUID );
										if ( (siResult == eDSNoErr) && (niValuesGUID.ni_namelist_len > 0) && (niValuesGUID.ni_namelist_val[0] != nil))
										{
											//check to remove all corresponding hash files
											RemoveShadowHash( pContext->fRecName, niValuesGUID.ni_namelist_val[0], true );
										}
										if (niValuesGUID.ni_namelist_len > 0)
										{
											ni_namelist_free(&niValuesGUID);
										}
									}
								}
								if (niValues.ni_namelist_len > 0)
								{
									ni_namelist_free(&niValues);
								}
							}

							NI_INIT( &niNameList );
							::ni_namelist_insert( &niNameList, niValue.ni_namelist_val[ pv ], NI_INDEX_NULL );

							siResult = NiLib2::DestroyDirVal( aNIDomain, &pContext->dirID, pAttrType, niNameList );
							siResult = MapNetInfoErrors( siResult );
							if ( siResult != eDSNoErr ) throw( siResult );
							
							::ni_namelist_free( &niNameList );

							pContext->bDidWriteOperation = true;

							done = true;

							break;
						}
					}
				}
			}
			
			if ( pAttrType != nil )
			{
				delete( pAttrType );
				pAttrType = nil;
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if (authUser != nil)
	{
		if (pContext != nil)
		{
			if (pContext->fAuthenticatedUserName == nil)
			{
				free(authUser);
				authUser = nil;
			}
		}
	}

	if (bFreePropList)
	{
		::ni_proplist_free( &niPropList );
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // RemoveAttributeValue



//------------------------------------------------------------------------------------
//	* SetAttributeValue
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::SetAttributeValue ( sSetAttributeValue *inData )
{
	bool					done			= false;
	sInt32					siResult		= eDSNoErr;
	bool					bFreePropList	= false;
	uInt32					pn				= 0;
	uInt32					pv				= 0;
	char				   *pAttrType		= nil;
	char				   *pAttrValue		= nil;
	sNIContextData		   *pContext		= nil;
	ni_namelist				niNameList;
	ni_namelist				niValue;
	ni_property				niProp;
	ni_proplist				niPropList;
	char				   *authUser		= nil;

	gNetInfoMutex->Wait();

	try
	{
		// Record context data
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			pAttrType = MapAttrToNetInfoType( inData->fInAttrType->fBufferData );

			siResult = ::ni_read( aNIDomain, &pContext->dirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			bFreePropList = true;

			// check access for non-admin users
			if ( ( pContext->fEffectiveUID != 0 )
				 && ( (pContext->fAuthenticatedUserName == NULL) 
					  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
			{
				authUser = pContext->fAuthenticatedUserName;
				if ( ( authUser == NULL ) && (strcmp(pAttrType,"picture") == 0) )
				{
					authUser = GetUserNameForUID( pContext->fEffectiveUID, aNIDomain );
				}
				ni_index	niIndex;

				siResult = NiLib2::ValidateDir( authUser, &niPropList );
				if ( siResult != NI_OK )
				{
					niIndex = ::ni_proplist_match( niPropList, pAttrType, NULL );
					siResult = NiLib2::ValidateName( authUser, &niPropList, niIndex );
				}

				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}

			siResult = eDSAttributeNotFound;
			pAttrValue = NormalizeNIString( inData->fInAttrValueEntry->fAttributeValueData.fBufferData, pAttrType );
			for ( pn = 0; pn < (niPropList.ni_proplist_len) && (done == false); pn++ )
			{
				niProp = niPropList.ni_proplist_val[ pn ];

				if ( (pAttrType != nil) && (::strcmp( niProp.nip_name, pAttrType ) == 0) )
				{
					niValue = niProp.nip_val;

					// For each value in the namelist for this property
					for ( pv = 0; (pv < niValue.ni_namelist_len) && (done == false); pv++ )
					{
						uInt32 crcVal = CalcCRC( niValue.ni_namelist_val[ pv ] );
						if ( crcVal == inData->fInAttrValueEntry->fAttributeValueID )
						{
							// check if the auth authority value of shadow hash for a user is being replaced
							// if so we should also remove the hash file for that user
							if ( (pAttrType != nil) && (strcmp(pAttrType,"authentication_authority") == 0) )
							{
								ni_namelist niValues;
								ni_namelist niValuesGUID;
								NI_INIT( &niValues );
								NI_INIT( &niValuesGUID );
								//lookup authauthority attribute
								siResult = ni_lookupprop( aNIDomain, &pContext->dirID, "authentication_authority", &niValues );
								if ( (siResult == eDSNoErr) && (niValues.ni_namelist_len > 0) && (niValues.ni_namelist_val[0] != nil))
								{
									if ( strcasestr(niValues.ni_namelist_val[0], kDSValueAuthAuthorityShadowHash) != nil ) 
									{
										if (	( strcasestr(pAttrValue, kDSValueAuthAuthorityShadowHash) == nil ) &&
												( strcasestr(pAttrValue, kDSTagAuthAuthorityDisabledUser) == nil ) )
										{
											siResult = ni_lookupprop( aNIDomain, &pContext->dirID, "generateduid", &niValuesGUID );
											if ( (siResult == eDSNoErr) && (niValuesGUID.ni_namelist_len > 0) && (niValuesGUID.ni_namelist_val[0] != nil))
											{
												//check to remove all corresponding hash files
												RemoveShadowHash( pContext->fRecName, niValuesGUID.ni_namelist_val[0], true );
											}
											if (niValuesGUID.ni_namelist_len > 0)
											{
												ni_namelist_free(&niValuesGUID);
											}
										}
									}
								}
								if (niValues.ni_namelist_len > 0)
								{
									ni_namelist_free(&niValues);
								}
							}

							NI_INIT( &niNameList );
							::ni_namelist_insert( &niNameList, niValue.ni_namelist_val[ pv ], NI_INDEX_NULL );

							siResult = NiLib2::DestroyDirVal( aNIDomain, &pContext->dirID, pAttrType, niNameList );
							siResult = NiLib2::InsertDirVal( aNIDomain, &pContext->dirID, pAttrType, pAttrValue, pv );
							siResult = MapNetInfoErrors( siResult );

							if (siResult == eDSNoErr)
							{
								pContext->bDidWriteOperation = true;
							}

							::ni_namelist_free( &niNameList );

							done = true;

							break;
						}
					}
				}
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if (authUser != nil)
	{
		if (pContext != nil)
		{
			if (pContext->fAuthenticatedUserName == nil)
			{
				free(authUser);
				authUser = nil;
			}
		}
	}

	DSDelete( pAttrType );
	if ( pAttrValue != inData->fInAttrValueEntry->fAttributeValueData.fBufferData )
	{
		DSFreeString( pAttrValue );
	}
	
	if (bFreePropList)
	{
		::ni_proplist_free( &niPropList );
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // SetAttributeValue


//------------------------------------------------------------------------------------
//	* SetAttributeValues
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::SetAttributeValues ( sSetAttributeValues *inData )
{
	bool					bWasShadowHash	= false;
	bool					bIsShadowHash	= false;
	sInt32					siResult		= eDSNoErr;
	bool					bFreePropList	= false;
	uInt32					pn				= 0;
	uInt32					pv				= 0;
	char				   *pAttrType		= nil;
	uInt32					valCount		= 0;
	char				   *attrValue		= nil;
	uInt32					attrLength		= 0;
	tDataNodePtr			valuePtr		= nil;
	sNIContextData		   *pContext		= nil;
	ni_namelist				niNameList;
	ni_namelist				niValue;
	ni_property				niProp;
	ni_proplist				niPropList;
	char				   *authUser		= nil;

	gNetInfoMutex->Wait();

	try
	{
		// Record context data
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidRecordRef );

		if ( inData->fInAttrType == nil ) throw( (sInt32)eDSNullAttributeType );
		//if ( inData->fInAttrValueList == nil ) throw( (sInt32)eDSNullAttributeValue ); //would like a plural constant for this

		void * aNIDomain = RetrieveNIDomain(pContext);
		if (aNIDomain != NULL)
		{
			pAttrType = MapAttrToNetInfoType( inData->fInAttrType->fBufferData );
			if ( pAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );

			siResult = ::ni_read( aNIDomain, &pContext->dirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			bFreePropList = true;

			// check access for non-admin users
			if ( ( pContext->fEffectiveUID != 0 )
				 && ( (pContext->fAuthenticatedUserName == NULL) 
					  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
			{
				authUser = pContext->fAuthenticatedUserName;
				if ( ( authUser == NULL ) && (strcmp(pAttrType,"picture") == 0) )
				{
					authUser = GetUserNameForUID( pContext->fEffectiveUID, aNIDomain );
				}
				ni_index	niIndex;

				siResult = NiLib2::ValidateDir( authUser, &niPropList );
				if ( siResult != NI_OK )
				{
					niIndex = ::ni_proplist_match( niPropList, pAttrType, NULL );
					if ( niIndex != NI_INDEX_NULL )
					{
						siResult = NiLib2::ValidateName( authUser, &niPropList, niIndex );
					}
				}

				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}
			valCount	= inData->fInAttrValueList->fDataNodeCount;
			niNameList.ni_namelist_len = valCount;
			niNameList.ni_namelist_val = (ni_name*) calloc(valCount + 1, sizeof(ni_name*) );
			
			for (uInt32 i = 0; i < valCount; i++ ) //extract the values out of the tDataList
			{
				dsDataListGetNodeAlloc( 0, inData->fInAttrValueList, i+1, &valuePtr );
				if ( (valuePtr != nil) && (valuePtr->fBufferLength > 0) )
				{
					attrLength = valuePtr->fBufferLength;
					attrValue = (char *) calloc(1, 1 + attrLength);
					memcpy(attrValue, valuePtr->fBufferData, attrLength);
					niNameList.ni_namelist_val[i] = attrValue;
					attrValue = nil;
				}
				if (valuePtr != NULL)
				{
					dsDataBufferDeallocatePriv(valuePtr);
					valuePtr = NULL;
				}
			} // for each value provided
			NormalizeNINameList(&niNameList, pAttrType);
			
			if ( strcmp(pAttrType,"authentication_authority") == 0 )
			{
				// check if we are going from ShadowHash to something else
				pn = ni_proplist_match(niPropList, pAttrType, NULL);
				if (pn != NI_INDEX_NULL) {
					niProp = niPropList.ni_proplist_val[ pn ];
					niValue = niProp.nip_val;
					for ( pv = 0; pv < niValue.ni_namelist_len && niValue.ni_namelist_val[pv]; pv++ )
					{
						if (strcasestr(niValue.ni_namelist_val[pv], kDSValueAuthAuthorityShadowHash) != nil)
						{
							bWasShadowHash = true;
							break;
						}
					}
				}
				if (bWasShadowHash)
				{
					niValue = niNameList;
					for ( pv = 0; pv < niValue.ni_namelist_len; pv++ )
					{
						if ( strcasestr(niValue.ni_namelist_val[pv], kDSValueAuthAuthorityShadowHash) != nil )
						{
							bIsShadowHash = true;
							break;
						}
					}
				}
			}
			
			if (bWasShadowHash && !bIsShadowHash)
			{
				// check if the auth authority value of shadow hash for a user is being replaced
				// if so we should also remove the hash file for that user
				ni_namelist niValuesGUID;
				NI_INIT( &niValuesGUID );
				//lookup authauthority attribute
				siResult = ni_lookupprop( aNIDomain, &pContext->dirID, "generateduid", &niValuesGUID );
				if ( (siResult == eDSNoErr) && (niValuesGUID.ni_namelist_len > 0) && (niValuesGUID.ni_namelist_val[0] != nil))
				{
					//check to remove all corresponding hash files
					RemoveShadowHash( pContext->fRecName, niValuesGUID.ni_namelist_val[0], true );
				}
				if (niValuesGUID.ni_namelist_len > 0)
				{
					ni_namelist_free(&niValuesGUID);
				}
			}

			siResult = NiLib2::CreateDirProp( aNIDomain, &pContext->dirID, pAttrType, niNameList );
			siResult = MapNetInfoErrors( siResult );

			if (siResult == eDSNoErr)
			{
				pContext->bDidWriteOperation = true;
			}

			::ni_namelist_free( &niNameList );

		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}
	catch( sInt32 err )
	{
		siResult = err;
	}

	if (authUser != nil)
	{
		if (pContext != nil)
		{
			if (pContext->fAuthenticatedUserName == nil)
			{
				free(authUser);
				authUser = nil;
			}
		}
	}

	DSDelete( pAttrType );

	if (bFreePropList)
	{
		::ni_proplist_free( &niPropList );
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // SetAttributeValues


//------------------------------------------------------------------------------------
//	* DoPlugInCustomCall
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32					siResult		= eDSNoErr;
	sNIContextData		   *pContext		= nil;

	gNetInfoMutex->Wait();

	try
	{
		if ( inData == nil ) throw( (sInt32)eMemoryError );

		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		switch ( inData->fInRequestCode )
		{
			default:
				siResult = eNotHandledByThisNode;
				break;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );
} // DoPlugInCustomCall


//------------------------------------------------------------------------------------
//	* DoAttributeValueSearch
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoAttributeValueSearch ( sDoAttrValueSearchWithData *inData )
{
	sInt32					siResult			= eDSNoErr;
	bool					bAttrInfoOnly		= false;
	uInt32					uiCount				= 0;
	uInt32					uiTotal				= 0;
	CRecTypeList		   *cpRecTypeList		= nil;
	CAttributeList		   *cpAttrTypeList		= nil;
	CAttributeList		   *cpPattMatchList		= nil;
	char				   *pDS_AttrType		= nil;
	char				   *pNI_RecType			= nil;
	char				   *pDS_RecType			= nil;
	sNIContextData		   *pContext			= nil;
	sNIContinueData		   *pContinue			= nil;
	tDirPatternMatch		pattMatch			= eDSExact;
	tDataList			   *pTmpDataList		= nil;
	CBuff				   *outBuff				= nil;
	bool					bBuffFull			= false;
	bool					separateRecTypes	= false;
	uInt32					countDownRecTypes	= 0;
	ni_id					niDirID;
	ni_entrylist			niEntryList;

	gNetInfoMutex->Wait();

	try
	{
		if ( inData == nil ) throw( (sInt32)eMemoryError );

		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		if ( inData->fIOContinueData == nil )
		{
			pContinue = (sNIContinueData *)::calloc( 1, sizeof( sNIContinueData ) );
			gNIContinue->AddItem( pContinue, inData->fInNodeRef );

			pContinue->fRecNameIndex = 1;
			pContinue->fRecTypeIndex = 1;
			pContinue->fAttrIndex = 1;
			pContinue->fAllRecIndex		= 0;
			pContinue->fTotalRecCount	= 0;
			pContinue->fLimitRecSearch	= 0;
			
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pContinue
			if (inData->fOutMatchRecordCount >= 0)
			{
				pContinue->fLimitRecSearch = inData->fOutMatchRecordCount;
			}
		}
		else
		{
			pContinue = (sNIContinueData *)inData->fIOContinueData;
			if ( gNIContinue->VerifyItem( pContinue ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
		}

		inData->fIOContinueData			= nil;
		//return zero if we don't find anything
		inData->fOutMatchRecordCount	= 0;

		outBuff = new CBuff;
		if ( outBuff == nil ) throw( (sInt32)eMemoryAllocError );

		siResult = outBuff->Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff->SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw( siResult );

		// Get the record type list
		cpRecTypeList = new CRecTypeList( inData->fInRecTypeList );
		if ( cpRecTypeList == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
		if (cpRecTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordTypeList );
		//now we check to see if kDSStdRecordTypeAll is present by itself only
		//if it is present we rebuild this list to include all existing record types in the directory
		char *aStringValue = nil;
		if (eDSNoErr == cpRecTypeList->GetAttribute(1, &aStringValue))
		{
			if (strcmp(aStringValue, kDSStdRecordTypeAll) == 0)
			{
				delete( cpRecTypeList );
				cpRecTypeList = nil;
				//now need to build the real list
				//make sure that users, groups and computers are first always
				tDataListPtr newNodeList = dsBuildListFromStrings( 0, kDSStdRecordTypeUsers, kDSStdRecordTypeGroups, kDSStdRecordTypeComputers, nil );
				void * aNIDomain = RetrieveNIDomain(pContext);
				if (aNIDomain != NULL)
				{
					NI_INIT( &niDirID );
					NI_INIT( &niEntryList );
					siResult = ::ni_pathsearch( aNIDomain, &niDirID, "/" );
					if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
					siResult = ::ni_list( aNIDomain, &niDirID, "name", &niEntryList );
					if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
					
					if (niEntryList.ni_entrylist_val == NULL)
					{
						niEntryList.ni_entrylist_len = 0;
					}
					for ( uInt32 i = 0; i < niEntryList.ni_entrylist_len; i++ )
					{
						char* dsType = MapNetInfoRecToDSType(niEntryList.ni_entrylist_val[i].names->ni_namelist_val[0]);
						
						if (dsType != NULL)
						{
							//need to filter out all the duplicates for users, groups, computers as well as non-std types
							if (	(strncmp(dsType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0) &&
									(strcmp(dsType,kDSStdRecordTypeUsers) != 0) &&
									(strcmp(dsType,kDSStdRecordTypeGroups) != 0) &&
									(strcmp(dsType,kDSStdRecordTypeComputers) != 0) )
									
							{
								dsAppendStringToListAlloc(0, newNodeList, dsType);
							}
							delete( dsType );
							dsType = nil;
						}
					}
					if ( niEntryList.ni_entrylist_len > 0 )
					{
						ni_entrylist_free(&niEntryList);
					}
					cpRecTypeList = new CRecTypeList( newNodeList );
					if ( cpRecTypeList == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
					if (cpRecTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordTypeList );
				}
				else
				{
					throw( (sInt32)eDSInvalidSession );
				}
			}//if (strcmp(aStringValue, kDSStdRecordTypeAll) == 0)
		}//if (eDSNoErr == cpRecTypeList->GetAttribute(1, &aStringValue))
		
		//save the number of rec types here to use in separating the buffer data
		countDownRecTypes = cpRecTypeList->GetCount() - pContinue->fRecTypeIndex + 1;

		pDS_AttrType = inData->fInAttrType->fBufferData;
		if ( pDS_AttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

		// Get the attribute value matching list
		cpPattMatchList = new CAttributeList( (char *)(inData->fInPatt2Match->fBufferData) );
		if ( cpPattMatchList == nil ) throw( (sInt32)eDSEmptyDataList );
		if (cpPattMatchList->GetCount() == 0) throw( (sInt32)eDSEmptyDataList );
		
		if ( inData->fType == kDoAttributeValueSearchWithData )
		{
			// Get the attribute list
			cpAttrTypeList = new CAttributeList( inData->fInAttrTypeRequestList );
			if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
			if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );

			bAttrInfoOnly = inData->fInAttrInfoOnly;
		}
		else
		{
			pTmpDataList = ::dsBuildListFromStringsPriv( kDSAttributesAll, nil );
			if ( pTmpDataList != nil )
			{
				cpAttrTypeList = new CAttributeList( pTmpDataList );
				if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
				if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );
			}
		}

		pattMatch = inData->fInPattMatchType;
		siResult = VerifyPatternMatch( pattMatch );
		if ( siResult != eDSNoErr ) throw( (sInt32)eDSInvalidPatternMatchType );

//KW need to be smarter on exiting from the limit search condition ie. stop calling into more record types and names
// if the limit has already been reached ie. check in this routine as well before continuing the search

		while ( (cpRecTypeList->GetAttribute( pContinue->fRecTypeIndex, &pDS_RecType ) == eDSNoErr) &&
				(!bBuffFull) && (!separateRecTypes)  )
		{
			// Do we support this record type?
			//	- There could be a whole list so don't quit if we encounter a bad one.

			pNI_RecType = MapRecToNetInfoType( pDS_RecType );
			if ( pNI_RecType != nil )
			{
				if ( ::strcmp( pDS_AttrType, kDSAttributesAll ) == 0 )
				{
					siResult = FindAllRecords ( pNI_RecType,
												pDS_RecType,
												cpPattMatchList,
												pattMatch,
												cpAttrTypeList,
												bAttrInfoOnly,
												pContinue,
												pContext,
												outBuff,
												uiCount );
				}
				else
				{
					siResult = FindTheseRecords ( pNI_RecType,
												pDS_RecType,
												pDS_AttrType,
												cpPattMatchList,
												pattMatch,
												cpAttrTypeList,
												bAttrInfoOnly,
												pContinue,
												pContext,
												outBuff,
												uiCount );

				}
				
				//outBuff->GetDataBlockCount( &uiCount );
				//cannot use this as there may be records added from different record names
				//at which point the first name will fill the buffer with n records and
				//uiCount is reported as n but as the second name fills the buffer with m MORE records
				//the statement above will return the total of n+m and add it to the previous n
				//so that the total erroneously becomes 2n+m and not what it should be as n+m
				//therefore uiCount is extracted directly out of the GetxxxRecord(s) calls

				if ( siResult == CBuff::kBuffFull )
				{
					bBuffFull = true;
					inData->fIOContinueData = pContinue;

					// check to see if buffer is empty and no entries added
					// which implies that the buffer is too small
					if ( ( uiCount == 0 ) && ( uiTotal == 0 ) )
					{
						siResult = eDSBufferTooSmall;
						break;
					}
					uiTotal += uiCount;
		
					inData->fOutMatchRecordCount = uiTotal;
					outBuff->SetLengthToSize();

					siResult = eDSNoErr;
				}
				else if ( siResult == eDSNoErr )
				{
					uiTotal += uiCount;
					pContinue->fRecNameIndex++;
				}
				delete( pNI_RecType );
				pNI_RecType = nil;
			}

			if ( !bBuffFull )
			{
				pDS_RecType = nil;
				pContinue->fRecTypeIndex++;
				pContinue->fRecNameIndex = 1;
				//KW? here we decide to exit with data full of the current type of records
				// and force a good exit with the data we have so we can come back for the next rec type
				separateRecTypes = true;
				//set continue since there may be more data available
				inData->fIOContinueData = pContinue;
				//make sure that the ni_entry_list is cleared for the previous record type
				if ( pContinue->fNIEntryList.ni_entrylist_len > 0 )
				{
					::ni_entrylist_free(&(pContinue->fNIEntryList));
					pContinue->fNIEntryList.ni_entrylist_len = 0;
				}
				siResult = eDSNoErr;

				//however if this was the last rec type then there will be no more data
				// check the number of rec types left
				countDownRecTypes--;
				if ( countDownRecTypes == 0 )
				{
					inData->fIOContinueData = nil;
				}
			}
		}

		if ( (siResult == eDSNoErr) && (!bBuffFull) )
		{
			if ( uiTotal == 0 )
			{
				//KW to remove "siResult = eDSRecordNotFound" altogether
				// see 2531386  dsGetRecordList should not return an error if record not found
				//siResult = eDSRecordNotFound;
				outBuff->ClearBuff();
			}
			else
			{
				outBuff->SetLengthToSize();
			}

			inData->fOutMatchRecordCount = uiTotal;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( (inData->fIOContinueData == nil) && (pContinue != nil) )
	{
		// we've decided not to return continue data, so we should clean up
		gNIContinue->RemoveItem( pContinue );
		pContinue = nil;
	}

	gNetInfoMutex->Signal();

	if ( cpAttrTypeList != nil )
	{
		delete( cpAttrTypeList );
		cpAttrTypeList = nil;
	}
	if ( cpPattMatchList != nil )
	{
		delete( cpPattMatchList );
		cpPattMatchList = nil;
	}
	if ( cpRecTypeList != nil )
	{
		delete( cpRecTypeList );
		cpRecTypeList = nil;
	}
	if ( outBuff != nil )
	{
		delete( outBuff );
		outBuff = nil;
	}
	if ( pNI_RecType != nil )
	{
		delete( pNI_RecType );
		pNI_RecType = nil;
	}

	if ( pTmpDataList != nil )
	{
		(void)::dsDataListDeallocatePriv( pTmpDataList );
		//need to free the header as well
		free( pTmpDataList );
		pTmpDataList = nil;
	}

	return( siResult );
} // DoAttributeValueSearch


//------------------------------------------------------------------------------------
//	* DoMultipleAttributeValueSearch
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoMultipleAttributeValueSearch ( sDoMultiAttrValueSearchWithData *inData )
{
	sInt32					siResult			= eDSNoErr;
	bool					bAttrInfoOnly		= false;
	uInt32					uiCount				= 0;
	uInt32					uiTotal				= 0;
	CRecTypeList		   *cpRecTypeList		= nil;
	CAttributeList		   *cpAttrTypeList		= nil;
	CAttributeList		   *cpPattMatchList		= nil;
	char				   *pDS_AttrType		= nil;
	char				   *pNI_RecType			= nil;
	char				   *pDS_RecType			= nil;
	sNIContextData		   *pContext			= nil;
	sNIContinueData		   *pContinue			= nil;
	tDirPatternMatch		pattMatch			= eDSExact;
	tDataList			   *pTmpDataList		= nil;
	CBuff				   *outBuff				= nil;
	bool					bBuffFull			= false;
	bool					separateRecTypes	= false;
	uInt32					countDownRecTypes	= 0;
	ni_id					niDirID;
	ni_entrylist			niEntryList;

	gNetInfoMutex->Wait();

	try
	{
		if ( inData == nil ) throw( (sInt32)eMemoryError );

		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		if ( inData->fIOContinueData == nil )
		{
			pContinue = (sNIContinueData *)::calloc( 1, sizeof( sNIContinueData ) );
			gNIContinue->AddItem( pContinue, inData->fInNodeRef );

			pContinue->fRecNameIndex = 1;
			pContinue->fRecTypeIndex = 1;
			pContinue->fAttrIndex = 1;
			pContinue->fAllRecIndex		= 0;
			pContinue->fTotalRecCount	= 0;
			pContinue->fLimitRecSearch	= 0;
			
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pContinue
			if (inData->fOutMatchRecordCount >= 0)
			{
				pContinue->fLimitRecSearch = inData->fOutMatchRecordCount;
			}
		}
		else
		{
			pContinue = (sNIContinueData *)inData->fIOContinueData;
			if ( gNIContinue->VerifyItem( pContinue ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
		}

		inData->fIOContinueData			= nil;
		//return zero if we don't find anything
		inData->fOutMatchRecordCount	= 0;

		outBuff = new CBuff;
		if ( outBuff == nil ) throw( (sInt32)eMemoryAllocError );

		siResult = outBuff->Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff->SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw( siResult );

		// Get the record type list
		cpRecTypeList = new CRecTypeList( inData->fInRecTypeList );
		if ( cpRecTypeList == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
		if (cpRecTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordTypeList );
		//now we check to see if kDSStdRecordTypeAll is present by itself only
		//if it is present we rebuild this list to include all existing record types in the directory
		char *aStringValue = nil;
		if (eDSNoErr == cpRecTypeList->GetAttribute(1, &aStringValue))
		{
			if (strcmp(aStringValue, kDSStdRecordTypeAll) == 0)
			{
				delete( cpRecTypeList );
				cpRecTypeList = nil;
				//now need to build the real list
				//make sure that users, groups and computers are first always
				tDataListPtr newNodeList = dsBuildListFromStrings( 0, kDSStdRecordTypeUsers, kDSStdRecordTypeGroups, kDSStdRecordTypeComputers, nil );
				void * aNIDomain = RetrieveNIDomain(pContext);
				if (aNIDomain != NULL)
				{
					NI_INIT( &niDirID );
					NI_INIT( &niEntryList );
					siResult = ::ni_pathsearch( aNIDomain, &niDirID, "/" );
					if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
					siResult = ::ni_list( aNIDomain, &niDirID, "name", &niEntryList );
					if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
					
					for ( uInt32 i = 0; i < niEntryList.ni_entrylist_len; i++ )
					{
						char* dsType = MapNetInfoRecToDSType(niEntryList.ni_entrylist_val[i].names->ni_namelist_val[0]);
						
						if (dsType != NULL)
						{
							//need to filter out all the duplicates for users, groups, computers as well as non-std types
							if (	(strncmp(dsType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0) &&
									(strcmp(dsType,kDSStdRecordTypeUsers) != 0) &&
									(strcmp(dsType,kDSStdRecordTypeGroups) != 0) &&
									(strcmp(dsType,kDSStdRecordTypeComputers) != 0) )
									
							{
								dsAppendStringToListAlloc(0, newNodeList, dsType);
							}
							delete( dsType );
							dsType = nil;
						}
					}
					if ( niEntryList.ni_entrylist_len > 0 )
					{
						ni_entrylist_free(&niEntryList);
					}
					cpRecTypeList = new CRecTypeList( newNodeList );
					if ( cpRecTypeList == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
					if (cpRecTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordTypeList );
				}
				else
				{
					throw( (sInt32)eDSInvalidSession );
				}
			}//if (strcmp(aStringValue, kDSStdRecordTypeAll) == 0)
		}//if (eDSNoErr == cpRecTypeList->GetAttribute(1, &aStringValue))
		
		//save the number of rec types here to use in separating the buffer data
		countDownRecTypes = cpRecTypeList->GetCount() - pContinue->fRecTypeIndex + 1;

		pDS_AttrType = inData->fInAttrType->fBufferData;
		if ( pDS_AttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

		// Get the attribute value matching list
		cpPattMatchList = new CAttributeList( inData->fInPatterns2MatchList );
		if ( cpPattMatchList == nil ) throw( (sInt32)eDSEmptyDataList );
		if (cpPattMatchList->GetCount() == 0) throw( (sInt32)eDSEmptyDataList );
		
		if ( inData->fType == kDoMultipleAttributeValueSearchWithData )
		{
			// Get the attribute list
			cpAttrTypeList = new CAttributeList( inData->fInAttrTypeRequestList );
			if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
			if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );

			bAttrInfoOnly = inData->fInAttrInfoOnly;
		}
		else
		{
			pTmpDataList = ::dsBuildListFromStringsPriv( kDSAttributesAll, nil );
			if ( pTmpDataList != nil )
			{
				cpAttrTypeList = new CAttributeList( pTmpDataList );
				if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
				if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );
			}
		}

		pattMatch = inData->fInPattMatchType;
		siResult = VerifyPatternMatch( pattMatch );
		if ( siResult != eDSNoErr ) throw( (sInt32)eDSInvalidPatternMatchType );

//KW need to be smarter on exiting from the limit search condition ie. stop calling into more record types and names
// if the limit has already been reached ie. check in this routine as well before continuing the search

		while ( (cpRecTypeList->GetAttribute( pContinue->fRecTypeIndex, &pDS_RecType ) == eDSNoErr) &&
				(!bBuffFull) && (!separateRecTypes)  )
		{
			// Do we support this record type?
			//	- There could be a whole list so don't quit if we encounter a bad one.

			pNI_RecType = MapRecToNetInfoType( pDS_RecType );
			if ( pNI_RecType != nil )
			{
				if ( ::strcmp( pDS_AttrType, kDSAttributesAll ) == 0 )
				{
					siResult = FindAllRecords ( pNI_RecType,
												pDS_RecType,
												cpPattMatchList,
												pattMatch,
												cpAttrTypeList,
												bAttrInfoOnly,
												pContinue,
												pContext,
												outBuff,
												uiCount );
				}
				else
				{
					siResult = FindTheseRecords ( pNI_RecType,
												pDS_RecType,
												pDS_AttrType,
												cpPattMatchList,
												pattMatch,
												cpAttrTypeList,
												bAttrInfoOnly,
												pContinue,
												pContext,
												outBuff,
												uiCount );

				}
				
				//outBuff->GetDataBlockCount( &uiCount );
				//cannot use this as there may be records added from different record names
				//at which point the first name will fill the buffer with n records and
				//uiCount is reported as n but as the second name fills the buffer with m MORE records
				//the statement above will return the total of n+m and add it to the previous n
				//so that the total erroneously becomes 2n+m and not what it should be as n+m
				//therefore uiCount is extracted directly out of the GetxxxRecord(s) calls

				if ( siResult == CBuff::kBuffFull )
				{
					bBuffFull = true;
					inData->fIOContinueData = pContinue;

					// check to see if buffer is empty and no entries added
					// which implies that the buffer is too small
					if ( ( uiCount == 0 ) && ( uiTotal == 0 ) )
					{
						siResult = eDSBufferTooSmall;
						break;
					}
					uiTotal += uiCount;
		
					inData->fOutMatchRecordCount = uiTotal;
					outBuff->SetLengthToSize();

					siResult = eDSNoErr;
				}
				else if ( siResult == eDSNoErr )
				{
					uiTotal += uiCount;
					pContinue->fRecNameIndex++;
				}
				delete( pNI_RecType );
				pNI_RecType = nil;
			}

			if ( !bBuffFull )
			{
				pDS_RecType = nil;
				pContinue->fRecTypeIndex++;
				pContinue->fRecNameIndex = 1;
				//KW? here we decide to exit with data full of the current type of records
				// and force a good exit with the data we have so we can come back for the next rec type
				separateRecTypes = true;
				//set continue since there may be more data available
				inData->fIOContinueData = pContinue;
				//make sure that the ni_entry_list is cleared for the previous record type
				if ( pContinue->fNIEntryList.ni_entrylist_len > 0 )
				{
					::ni_entrylist_free(&(pContinue->fNIEntryList));
					pContinue->fNIEntryList.ni_entrylist_len = 0;
				}
				siResult = eDSNoErr;

				//however if this was the last rec type then there will be no more data
				// check the number of rec types left
				countDownRecTypes--;
				if ( countDownRecTypes == 0 )
				{
					inData->fIOContinueData = nil;
				}
			}
		}

		if ( (siResult == eDSNoErr) && (!bBuffFull) )
		{
			if ( uiTotal == 0 )
			{
				//KW to remove "siResult = eDSRecordNotFound" altogether
				// see 2531386  dsGetRecordList should not return an error if record not found
				//siResult = eDSRecordNotFound;
				outBuff->ClearBuff();
			}
			else
			{
				outBuff->SetLengthToSize();
			}

			inData->fOutMatchRecordCount = uiTotal;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( (inData->fIOContinueData == nil) && (pContinue != nil) )
	{
		// we've decided not to return continue data, so we should clean up
		gNIContinue->RemoveItem( pContinue );
		pContinue = nil;
	}

	gNetInfoMutex->Signal();

	if ( cpAttrTypeList != nil )
	{
		delete( cpAttrTypeList );
		cpAttrTypeList = nil;
	}
	if ( cpRecTypeList != nil )
	{
		delete( cpRecTypeList );
		cpRecTypeList = nil;
	}
	if ( cpPattMatchList != nil )
	{
		delete(cpPattMatchList);
		cpPattMatchList = nil;
	}
	if ( outBuff != nil )
	{
		delete( outBuff );
		outBuff = nil;
	}
	if ( pNI_RecType != nil )
	{
		delete( pNI_RecType );
		pNI_RecType = nil;
	}

	if ( pTmpDataList != nil )
	{
		(void)::dsDataListDeallocatePriv( pTmpDataList );
		//need to free the header as well
		free( pTmpDataList );
		pTmpDataList = nil;
	}

	return( siResult );

} // DoMultipleAttributeValueSearch


//------------------------------------------------------------------------------------
//	* FindAllRecords
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn:: FindAllRecords (	const char			*inNI_RecType,
									const char			*inDS_RecType,
									CAttributeList		*inPattMatchList,
									tDirPatternMatch	 inHow,
									CAttributeList		*inAttrTypeList,
									bool				 inAttrOnly,
									sNIContinueData		*inContinue,
									sNIContextData	    *inContext,
									CBuff				*inBuff,
									uInt32				&outRecCount )
{
	uInt32			pn				= 0;
	uInt32			pv				= 0;
	sInt32			siResult		= eDSNoErr;
	sInt32			siCount			= 0;
	uInt32			uiSearchFlag	= REG_BASIC;
	bool			bMatches		= false;
	u_int			en2				= 0;
	ni_index		niIndex			= 0;
	ni_id			niDirID;
	ni_entry		niEntry;
	ni_proplist		niPropList;
	ni_entrylist	niEntryList;
	ni_namelist		niNameList;
	CString			csNI_RecName( 128 );
	CString			csNI_AttrType( 128 );
	ni_property		niProp;
	void		   *aNIDomain		= NULL;

	gNetInfoMutex->Wait();

	outRecCount = 0; //need to track how many records were found by this call to FindAllRecords

	try
	{
		NI_INIT( &niDirID );
		NI_INIT( &niEntry );
		NI_INIT( &niPropList );
		NI_INIT( &niNameList );
		NI_INIT( &niEntryList );

		// Case sensitive check
		if ( (inHow >= eDSiExact) && (inHow <= eDSiRegularExpression) )
		{
			uiSearchFlag = REG_ICASE;
		}

		aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			// Get the dir ID for the record type
			siResult = ::ni_pathsearch( aNIDomain, &niDirID, inNI_RecType );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			// Do the search
			if ( inContinue->fNIEntryList.ni_entrylist_len == 0 )
			{
				siResult = ::ni_list( aNIDomain, &niDirID, "name", &(inContinue->fNIEntryList) );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}
			niEntryList = inContinue->fNIEntryList;

			for ( en2 = inContinue->fAllRecIndex; (en2 < niEntryList.ni_entrylist_len) &&
									(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
									(inContinue->fLimitRecSearch == 0)); en2++ )
			{
				niEntry = niEntryList.ni_entrylist_val[ en2 ];
				niDirID.nii_object = niEntry.id;

				if ( niEntry.names != NULL )
				{
					siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
					if ( siResult == NI_OK )
					{
						bMatches = false;
						
						for ( pn = 0; ((pn < niPropList.ni_proplist_len) && (bMatches == false)); pn++ )
						{
							niProp = niPropList.ni_proplist_val[ pn ];
							niNameList = niProp.nip_val;
							for ( pv = 0; ((pv < niNameList.ni_namelist_len) && (bMatches == false)); pv++ )
							{
								bMatches = DoAnyMatch( niNameList.ni_namelist_val[ pv ], inPattMatchList, inHow );
							}
						}
						if ( bMatches == true )
						{
							fAttrData->Clear();
							fRecData->Clear();
										
							fRecData->AppendShort( ::strlen( inDS_RecType ) );
							fRecData->AppendString( inDS_RecType );

							niIndex = ::ni_proplist_match( niPropList, "name", NULL );
							if ( niIndex == NI_INDEX_NULL || niPropList.nipl_val[ niIndex ].nip_val.ninl_len == 0 )
							{
								fRecData->AppendShort( ::strlen( "*** No Name ***" ) );
								fRecData->AppendString( "*** No Name ***" );
							}
							else
							{
								fRecData->AppendShort( ::strlen( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] ) );
								fRecData->AppendString( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] );
							}

							siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, aNIDomain, inContext->fDomainName, inDS_RecType, siCount );
							if ( siResult == eDSNoErr )
							{
								if ( siCount == 0 )
								{
									// Attribute count
									fRecData->AppendShort( 0 );
								}
								else
								{
									// Attribute count
									fRecData->AppendShort( siCount );
									fRecData->AppendBlock( fAttrData->GetData(), fAttrData->GetLength() );
								}

								siResult = inBuff->AddData( fRecData->GetData(), fRecData->GetLength() );
											
								//specifically check for buffer full here and segregate true errors
								if ( siResult == CBuff::kBuffFull )
								{
									inContinue->fAllRecIndex = en2;
									::ni_proplist_free( &niPropList );
									break;
								}
								else if ( siResult == eDSNoErr )
								{
									outRecCount++;
									inContinue->fTotalRecCount++;
								}
								else
								{
									inContinue->fAllRecIndex = 0;
									::ni_proplist_free( &niPropList );
									siResult = eDSInvalidBuffFormat;
									break;
								}
							}
						} // if ( bMatches == true )
						//need to free this regardless if there were matches or not
						::ni_proplist_free( &niPropList );
					} // if ( siResult == NI_OK )
				} // if ( niEntry.names != NULL )
			} // main for loop on en2
			if ( siResult == eDSNoErr )
			{
				inContinue->fAllRecIndex	= 0;
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult ); 

} // FindAllRecords


//------------------------------------------------------------------------------------
//	* FindTheseRecords
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::FindTheseRecords (	const char		 *inNI_RecType,
										const char		 *inDS_RecType,
										const char		 *inDS_AttrType,
										CAttributeList   *inPattMatchList,
										tDirPatternMatch  inHow,
										CAttributeList	 *inAttrTypeList,
										bool			  inAttrOnly,
										sNIContinueData	 *inContinue,
										sNIContextData   *inContext,
										CBuff			 *inBuff,
										uInt32			 &outRecCount )

{
	bool			done			= false;
	bool			bMatches		= false;
	uInt32			i				= 0;
	sInt32			siCount			= 0;
	sInt32			siResult		= eDSNoErr;
	uInt32			uiSearchFlag	= REG_BASIC;
	ni_status		niStatus		= NI_OK;
	u_int			en				= 0;
	ni_index		niIndex			= 0;
	ni_id			niDirID;
	ni_entry		niEntry;
	ni_entrylist	niEntryList;
	ni_proplist		niPropList;
	ni_namelist		niNameList;
	CString			csNI_AttrType( 128 );
	void		   *aNIDomain		= NULL;

	gNetInfoMutex->Wait();

	outRecCount = 0; //need to track how many records were found by this call to FindTheseRecords

	try
	{
		NI_INIT( &niDirID );
		NI_INIT( &niEntryList );

		// Case sensitive check
		if ( (inHow >= eDSiExact) && (inHow <= eDSiRegularExpression) )
		{
			uiSearchFlag = REG_ICASE;
		}

		aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			while ( !done )
			{
				NI_INIT( &niDirID );
				NI_INIT( &niEntryList );

				// Get the dir ID for the record
				siResult = ::ni_pathsearch( aNIDomain, &niDirID, inNI_RecType );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

				if ( ::strcmp( inDS_AttrType, kDSNAttrRecordName ) == 0 )
				{
					if ( csNI_AttrType.GetLength() == 0 )
					{
						csNI_AttrType.Set( "name" );
					}
					else
					{
						csNI_AttrType.Set( "realname" );
						done = true;
					}
				}
				else
				{
					char *netInfoType = MapAttrToNetInfoType( inDS_AttrType );
					if ( netInfoType != nil )
					{
						csNI_AttrType.Set( netInfoType );
						free( netInfoType );
						netInfoType = nil;
					}
					done = true;
				}

				//ensure that if a client only asks for n records that we stop searching if
				//we have already found n records ie. critical for performance when asking for one record
				if ( (inContinue->fTotalRecCount < inContinue->fLimitRecSearch) || (inContinue->fLimitRecSearch == 0) )
				{
					// Do the search
					//make the reg exp that ni_search needs
					//inConstRegExpPatt2Match = BuildMultiRegExp(inPattMatchList);
					niStatus = ::ni_list( aNIDomain, &niDirID, csNI_AttrType.GetData(), &niEntryList );
					if ( niStatus == NI_OK )
					{
						for ( en = inContinue->fAllRecIndex; (en < niEntryList.ni_entrylist_len) &&
							(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
							(inContinue->fLimitRecSearch == 0)); en++ )
						{
							niEntry = niEntryList.ni_entrylist_val[ en ];
							niDirID.nii_object = niEntry.id;
		
							if ( niEntry.names != NULL )
							{
								niNameList = *niEntry.names;
								bMatches = false;
								for ( i = 0; ((i < niNameList.ni_namelist_len) && (bMatches == false)); i++ )
								{
									bMatches = DoAnyMatch( niNameList.ni_namelist_val[ i ], inPattMatchList, inHow );
								}
								if ( bMatches == true )
								{
									siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
									if ( siResult == NI_OK )
									{
										
										fAttrData->Clear();
										fRecData->Clear();
		
										fRecData->AppendShort( ::strlen( inDS_RecType ) );
										fRecData->AppendString( inDS_RecType );
		
										niIndex = ::ni_proplist_match( niPropList, "name", NULL );
										if ( niIndex == NI_INDEX_NULL || niPropList.nipl_val[ niIndex ].nip_val.ninl_len == 0 )
										{
											fRecData->AppendShort( ::strlen( "*** No Name ***" ) );
											fRecData->AppendString( "*** No Name ***" );
										}
										else
										{
											fRecData->AppendShort( ::strlen( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] ) );
											fRecData->AppendString( niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ] );
										}
		
										siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, aNIDomain, inContext->fDomainName, inDS_RecType, siCount );
										if ( siResult == eDSNoErr )
										{
											if ( siCount == 0 )
											{
												// Attribute count
												fRecData->AppendShort( 0 );
											}
											else
											{
												// Attribute count
												fRecData->AppendShort( siCount );
												fRecData->AppendBlock( fAttrData->GetData(), fAttrData->GetLength() );
											}
		
											siResult = inBuff->AddData( fRecData->GetData(), fRecData->GetLength() );
											//again need to segregate true errors away from buffer full
											if ( siResult == CBuff::kBuffFull )
											{
												inContinue->fAllRecIndex = en;
												::ni_proplist_free( &niPropList );
												if ( siResult != eDSNoErr ) throw( siResult );
											}
											else if ( siResult == eDSNoErr )
											{
												outRecCount++;
												inContinue->fTotalRecCount++;
											}
											else
											{
												inContinue->fAllRecIndex = 0;
												::ni_proplist_free( &niPropList );
												throw( (sInt32)eDSInvalidBuffFormat );
											}
										}
										::ni_proplist_free( &niPropList );
										siResult = eDSNoErr;
									}
								}
							}
						}
					} //ni_list successful
					if ( niEntryList.ni_entrylist_len > 0 )
					{
						::ni_entrylist_free(&niEntryList);
						niEntryList.ni_entrylist_len = 0;
					}
				} // need to get more records ie. did not hit the limit yet
			} // while
			inContinue->fAllRecIndex = 0;
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		if ( niEntryList.ni_entrylist_len > 0 )
		{
			::ni_entrylist_free(&niEntryList);
			niEntryList.ni_entrylist_len = 0;
		}
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult ); 

} // FindTheseRecords


#pragma mark -
#pragma mark Authentication
#pragma mark -

//------------------------------------------------------------------------------------
//	* DoAuthentication
//
//  Optimized for code size, reuses code from the more general
//  DoAuthenticationOnRecordType().
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoAuthentication ( sDoDirNodeAuth *inData )
{
	sInt32 siResult = eDSAuthFailed;
	sDoDirNodeAuthOnRecordType dataPlus;
	
	// note: because sDoDirNodeAuthOnRecordType is not directly derived from sDoDirNodeAuth, the fields must be
	// manually copied for safety in case one struct changes and the other doesn't.
	dataPlus.fType = inData->fType;
	dataPlus.fResult = inData->fResult;
	dataPlus.fInNodeRef = inData->fInNodeRef;
	dataPlus.fInAuthMethod = inData->fInAuthMethod;
	dataPlus.fInDirNodeAuthOnlyFlag = inData->fInDirNodeAuthOnlyFlag;
	dataPlus.fInAuthStepData = inData->fInAuthStepData;
	dataPlus.fOutAuthStepDataResponse = inData->fOutAuthStepDataResponse;
	dataPlus.fIOContinueData = inData->fIOContinueData;
	
	dataPlus.fInRecordType = dsDataBufferAllocatePriv( sizeof(kDSStdRecordTypeUsers) );
	if ( dataPlus.fInRecordType == nil )
		return eMemoryError;
	
	strcpy( dataPlus.fInRecordType->fBufferData, kDSStdRecordTypeUsers );
	dataPlus.fInRecordType->fBufferLength = sizeof(kDSStdRecordTypeUsers) - 1;
	
	// funnel to the more generic method
	siResult = this->DoAuthenticationOnRecordType( &dataPlus );
	
	dsDataBufferDeallocatePriv( dataPlus.fInRecordType );
	
	// copy it all back
	inData->fType = dataPlus.fType;
	inData->fResult = dataPlus.fResult;
	inData->fInNodeRef = dataPlus.fInNodeRef;
	inData->fInAuthMethod = dataPlus.fInAuthMethod;
	inData->fInDirNodeAuthOnlyFlag = dataPlus.fInDirNodeAuthOnlyFlag;
	inData->fInAuthStepData = dataPlus.fInAuthStepData;
	inData->fOutAuthStepDataResponse = dataPlus.fOutAuthStepDataResponse;
	inData->fIOContinueData = dataPlus.fIOContinueData;
	
	return siResult;
} // DoAuthentication


//------------------------------------------------------------------------------------
//	* DoAuthenticationOnRecordType
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoAuthenticationOnRecordType ( sDoDirNodeAuthOnRecordType *inData )
{
	sInt32				siResult				= eDSAuthFailed;
	sInt32				siResult2				= eDSAuthFailed;
	ni_status			niResult				= NI_OK;
	ni_status			niResultGUID			= NI_OK;
	uInt32				uiAuthMethod			= 0;
	sNIContextData	   *pContext				= NULL;
	sNIContinueData	   *pContinueData			= NULL;
	char				*authenticatorName		= NULL;
	char				*authenticatorPassword	= NULL;
	char				*userName				= NULL;
	ni_id				niDirID;
	ni_namelist			niValues;
	ni_namelist			niValuesGUID;
	NetInfoAuthAuthorityHandlerProc handlerProc = NULL;
	char			   *pNIRecType				= NULL;
	uInt32				settingPolicy			= 0;
	tDataNodePtr		origAuthMethod			= NULL;
	char			   *GUIDString				= NULL;
	char			   *UserGUIDString			= NULL;
	void			   *aNIDomain				= NULL;
	
	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );
		if ( inData->fInAuthStepData == nil ) throw( (sInt32)eDSNullAuthStepData );
		if ( inData->fInRecordType == nil ) throw( (sInt32)eDSNullRecType );
		if ( inData->fInRecordType->fBufferData == nil ) throw( (sInt32)eDSNullRecType );
		pNIRecType = MapRecToNetInfoType( inData->fInRecordType->fBufferData );
		if (pNIRecType == nil) throw( (sInt32)eDSAuthFailed );
		
		if ( !(fHashList & kNiPluginHashHasReadConfig) )
		{
			//need netinfo mutex here since using domain
			gNetInfoMutex->Wait();
			aNIDomain = RetrieveNIDomain(pContext);
			if (aNIDomain != NULL)
			{
				GetHashSecurityLevelConfig( aNIDomain, &fHashList );
				fHashList |= kNiPluginHashHasReadConfig;
			}
			else
			{
				gNetInfoMutex->Signal();
				throw( (sInt32)eDSInvalidSession );
			}
			gNetInfoMutex->Signal();
		}
		sHashList = fHashList;
		
		if ( inData->fIOContinueData != NULL )
		{
			// get info from continue
			pContinueData = (sNIContinueData *)inData->fIOContinueData;
			if ( gNIContinue->VerifyItem( pContinueData ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
			
			if ( pContinueData->fAuthHandlerProc == nil ) throw( (sInt32)eDSInvalidContinueData );
			handlerProc = (NetInfoAuthAuthorityHandlerProc)(pContinueData->fAuthHandlerProc);
			siResult = (handlerProc)(	inData->fInNodeRef, inData->fInAuthMethod, pContext,
										&pContinueData, inData->fInAuthStepData, 
										inData->fOutAuthStepDataResponse, 
										inData->fInDirNodeAuthOnlyFlag, false,
										pContinueData->fAuthAuthorityData, NULL, (const char *)pNIRecType);
		}
		else
		{
			// first call
			// we do not want to fail if the method is unknown at this point.
			// For password server users, the PSPlugin may know the method.
			if (inData->fInAuthMethod != nil)
			{
				DBGLOG1( kLogPlugin, "NetInfo PlugIn: Attempting use of authentication method %s", inData->fInAuthMethod->fBufferData );
			}
			siResult = dsGetAuthMethodEnumValue( inData->fInAuthMethod, &uiAuthMethod );
			
			// unsupported auth methods are allowed if the user is on password server
			// otherwise, unsupported auth methods are rejected in their handlers
			if ( siResult == eDSNoErr || siResult == eDSAuthMethodNotSupported )
			{
					// For user policy, we need the GUID
				if ( uiAuthMethod == kAuthGetPolicy || uiAuthMethod == kAuthSetPolicy )
				{
					siResult = GetUserNameFromAuthBuffer( inData->fInAuthStepData, 3, &userName );
					if ( siResult != eDSNoErr ) throw( siResult );
					
					if ( RecordHasAuthAuthority( userName, (const char *)pNIRecType, pContext, kDSTagAuthAuthorityShadowHash, &niDirID ) == eDSNoErr )
					{
						UserGUIDString = GetGUIDForRecord( pContext, &niDirID );
							
						if ( uiAuthMethod == kAuthGetPolicy )
						{
							// no permissions required, just do it.
							siResult = DoShadowHashAuth(inData->fInNodeRef, inData->fInAuthMethod, pContext, 
														&pContinueData, 
														inData->fInAuthStepData, 
														inData->fOutAuthStepDataResponse,
														inData->fInDirNodeAuthOnlyFlag,
														false, NULL, UserGUIDString, (const char *)pNIRecType);
						
							// we're done
							throw( siResult );
						}
					}
				}
				
				// For SetPolicy operations, check the permissions
				if ( uiAuthMethod == kAuthSetGlobalPolicy || uiAuthMethod == kAuthSetPolicy )
				{
					char *user = NULL;
					char *password = NULL;
					unsigned int itemCount = 0;
					bool authenticatorFieldsHaveData = true;
					
					// to use root permissions, the authenticator and authenticator password
					// fields must be blank.
					if ( pContext->fEffectiveUID == 0 )
					{
						// note: Get2FromBuffer returns eDSInvalidBuffFormat if the user name is < 1 character
						siResult = Get2FromBuffer( inData->fInAuthStepData, NULL, &user, &password, &itemCount );
						if ( itemCount >= 2 && user != NULL && user[0] == '\0' && password != NULL && password[0] == '\0' )
							authenticatorFieldsHaveData = false;
						
						if ( user != NULL )
							free( user );
						if ( password != NULL )
							free( password );
					}
					
					if ( pContext->fEffectiveUID != 0 || authenticatorFieldsHaveData )
					{
						settingPolicy = uiAuthMethod;
						uiAuthMethod = kAuthNativeClearTextOK;
						origAuthMethod = inData->fInAuthMethod;
						inData->fInAuthMethod = dsDataNodeAllocateString( 0, kDSStdAuthNodeNativeClearTextOK );
						if ( inData->fInAuthMethod == NULL )
							throw( (sInt32)eMemoryError );
						inData->fInDirNodeAuthOnlyFlag = false;
					}
					else
					{
						// go directly to shadowhash, do not pass go...
						siResult = DoShadowHashAuth(inData->fInNodeRef, inData->fInAuthMethod, pContext, 
														&pContinueData, 
														inData->fInAuthStepData, 
														inData->fOutAuthStepDataResponse,
														inData->fInDirNodeAuthOnlyFlag,
														false, NULL, UserGUIDString, (const char *)pNIRecType);
						
						// we're done here
						throw( siResult );
					}
				}
				else
				if ( uiAuthMethod == kAuthSetPasswd )
				{
					// verify the administrator
					siResult = GetUserNameFromAuthBuffer( inData->fInAuthStepData, 3, &authenticatorName );
					if ( siResult != eDSNoErr ) throw( siResult );
					
					if ( RecordHasAuthAuthority( authenticatorName, (const char *)pNIRecType, pContext, kDSTagAuthAuthorityShadowHash, &niDirID ) == eDSNoErr )
					{
						// get the GUID
						GUIDString = GetGUIDForRecord( pContext, &niDirID );
						
						origAuthMethod = inData->fInAuthMethod;
						inData->fInAuthMethod = dsDataNodeAllocateString( 0, "dsAuthMethodSetPasswd:dsAuthNodeNativeCanUseClearText" );
						if ( inData->fInAuthMethod == NULL )
							throw( (sInt32)eMemoryError );
						
						siResult = DoShadowHashAuth(inData->fInNodeRef, inData->fInAuthMethod, pContext, 
														&pContinueData, 
														inData->fInAuthStepData, 
														inData->fOutAuthStepDataResponse,
														false,	// inAuthOnly
														false, NULL, GUIDString, (const char *)pNIRecType);
						
						if ( siResult != eDSNoErr && siResult != eDSAuthNewPasswordRequired )
							throw( siResult );
						
						// transfer ownership of the auth method constant
						dsDataBufferDeallocatePriv( inData->fInAuthMethod );
						//switch to set passwd as root
						inData->fInAuthMethod = dsDataNodeAllocateString( 0, kDSStdAuthSetPasswdAsRoot );
					}
					else
					{
						// Need to call out to another node
						siResult = GetUserNameFromAuthBuffer( inData->fInAuthStepData, 4, &authenticatorPassword );
						if ( siResult != eDSNoErr )
							throw( siResult );
						
						bool adminPasswordFail = true;
						tDataListPtr authenticatorNode = NULL;
						authenticatorNode = FindNodeForSearchPolicyAuthUser( authenticatorName );
						if ( authenticatorNode != NULL )
						{
							tDirReference		aDirRef		= 0;
							sInt32				aResult		= eDSNoErr;
							tDirNodeReference	aNodeRef	= 0;
							tContextData		tContinue	= nil;
							size_t				authenticatorNameLen = strlen( authenticatorName );
							size_t				authenticatorPasswordLen = strlen( authenticatorPassword );
							tDataNodePtr		authMethod	= dsDataNodeAllocateString( 0, kDSStdAuthNodeNativeClearTextOK );
							tDataBufferPtr		authBuffer	= dsDataBufferAllocate( 0, sizeof(tDataBuffer) + authenticatorNameLen + authenticatorPasswordLen + 16 );
							
							if ( authMethod == NULL || authBuffer == NULL )
								throw( (sInt32)eMemoryError );
							
							aResult = dsOpenDirService( &aDirRef );
							if ( aResult == eDSNoErr )
							{
								aResult = dsOpenDirNode( aDirRef, authenticatorNode, &aNodeRef );
								dsDataListDeallocatePriv( authenticatorNode );
								free( authenticatorNode );
								authenticatorNode = NULL;
								if ( aResult == eDSNoErr )
								{
									siResult = dsFillAuthBuffer( authBuffer, 2,
										authenticatorNameLen, authenticatorName,
										authenticatorPasswordLen, authenticatorPassword );
									if ( siResult != eDSNoErr )
										throw( siResult );
									
									aResult = dsDoDirNodeAuth( aNodeRef, authMethod, true, authBuffer, inData->fOutAuthStepDataResponse, &tContinue );
									adminPasswordFail = (aResult != eDSNoErr);
									dsCloseDirNode( aNodeRef );
									aNodeRef = 0;
								}
								dsCloseDirService( aDirRef );
							}
							dsDataNodeDeAllocate( 0, authMethod );
							authMethod = NULL;
							dsDataBufferDeallocatePriv( authBuffer );
							authBuffer = NULL;
						}
						if ( adminPasswordFail )
							throw( (sInt32)eDSPermissionError );
					}
				}
				
				switch( uiAuthMethod )
				{
					case kAuthWithAuthorizationRef:
					{
						AuthorizationRef authRef = 0;
						AuthorizationItemSet* resultRightSet = NULL;
						if ( inData->fInAuthStepData->fBufferLength < sizeof( AuthorizationExternalForm ) ) throw( (sInt32)eDSInvalidBuffFormat );
						siResult = AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)inData->fInAuthStepData->fBufferData,
														&authRef);
						if (siResult != errAuthorizationSuccess)
						{
							DBGLOG1( kLogPlugin, "CNiPlugIn: AuthorizationCreateFromExternalForm returned error %d", siResult );
							syslog( LOG_ALERT, "AuthorizationCreateFromExternalForm returned error %d", siResult );
							throw( (sInt32)eDSPermissionError );
						}

						AuthorizationItem rights[] = { {"system.preferences", 0, 0, 0} };
						AuthorizationItemSet rightSet = { sizeof(rights)/ sizeof(*rights), rights };

						siResult = AuthorizationCopyRights(authRef, &rightSet, NULL,
											kAuthorizationFlagExtendRights, &resultRightSet);
						if (resultRightSet != NULL)
						{
							AuthorizationFreeItemSet(resultRightSet);
							resultRightSet = NULL;
						}
						if (siResult != errAuthorizationSuccess)
						{
							DBGLOG1( kLogPlugin, "CNiPlugIn: AuthorizationCopyRights returned error %d", siResult );
							syslog( LOG_ALERT, "AuthorizationCopyRights returned error %d", siResult );
							throw( (sInt32)eDSPermissionError );
						}
						if (inData->fInDirNodeAuthOnlyFlag == false)
						{
							if (pContext->fAuthenticatedUserName != NULL)
							{
								free(pContext->fAuthenticatedUserName);
							}
							pContext->fAuthenticatedUserName = strdup("root");
						}
						AuthorizationFree( authRef, 0 ); // really should hang onto this instead
						siResult = eDSNoErr;
					}
					break;
					
					case kAuthGetGlobalPolicy:
					{
						// kAuthXetGlobalPolicy is not associated with a user record and does not
						// have access to an authentication_authority attribute. If kAuthGetGlobalPolicy
						// is requested of a NetInfo node, then always return the shadowhash global
						// policies.
						
						siResult = DoShadowHashAuth(inData->fInNodeRef, inData->fInAuthMethod, pContext, 
															&pContinueData, 
															inData->fInAuthStepData, 
															inData->fOutAuthStepDataResponse,
															inData->fInDirNodeAuthOnlyFlag,
															false, NULL, NULL, (const char *)pNIRecType);
					}
					break;
					
					default: //everything but AuthRef method, GetGlobalPolicy and kAuthSetGlobalPolicy
					{
						if ( uiAuthMethod != kAuth2WayRandom )
						{
							siResult = GetUserNameFromAuthBuffer( inData->fInAuthStepData, 1, &userName );
							if ( siResult != eDSNoErr ) throw( siResult );
						}
						else
						{
							// for 2way random the first buffer is the username
							if ( inData->fInAuthStepData->fBufferLength > inData->fInAuthStepData->fBufferSize ) throw( (sInt32)eDSInvalidBuffFormat );
							userName = (char*)calloc( inData->fInAuthStepData->fBufferLength + 1, 1 );
							strncpy( userName, inData->fInAuthStepData->fBufferData, inData->fInAuthStepData->fBufferLength );
						}
						
						// for kAuthSetPasswd, need to check the authenticator is either self or an admin
						if ( authenticatorName != NULL && strcasecmp(authenticatorName, userName) != 0 )
						{
							struct passwd space, *pp = NULL;
							char buf[1024];
							uuid_t authenUUID;
							uuid_t adminGroupUUID;
							int ismember = 0;
							
							// root is special
							if ( strcasecmp(userName, "root") == 0 )
								throw( (sInt32)eDSPermissionError );
							
							if ( getpwnam_r(authenticatorName, &space, buf, sizeof buf, &pp) != 0 || pp == NULL )
								throw( (sInt32)eDSPermissionError );
							
							if ( mbr_uid_to_uuid(pp->pw_uid, authenUUID) != 0 )
								throw( (sInt32)eDSPermissionError );
							
							if ( mbr_gid_to_uuid(80, adminGroupUUID) != 0 )
								throw( (sInt32)eDSPermissionError );
							
							if ( mbr_check_membership(authenUUID, adminGroupUUID, &ismember) != 0 || ismember == 0 )
								throw( (sInt32)eDSPermissionError );
						}
						
						gNetInfoMutex->Wait();
						aNIDomain = RetrieveNIDomain(pContext);
						if (aNIDomain != NULL)
						{
							// get the auth authority
							siResult = IsValidRecordName( userName, (const char *)pNIRecType, aNIDomain, niDirID );
						}
						else
						{
							gNetInfoMutex->Signal();
							throw( (sInt32)eDSInvalidSession );
						}
						gNetInfoMutex->Signal();
						if ( siResult != eDSNoErr )
						{
							bool bAdminUserAuthUsed = false;
							//here we check if this is AuthOnly == false AND that siResult is not eDSNoErr AND local node only
							if ( (pContext->bIsLocal) && !(inData->fInDirNodeAuthOnlyFlag) && (siResult != eDSNoErr) )
							{
								//check if user is in the admin group
								if (UserIsAdmin(userName, pContext))
								{
									tDataList *usersNode = nil;
									usersNode = FindNodeForSearchPolicyAuthUser(userName);
									if (usersNode != nil)
									{
										tDirReference		aDirRef		= 0;
										sInt32				aResult		= eDSNoErr;
										tDirNodeReference	aNodeRef	= 0;
										tContextData		tContinue	= nil;

										aResult = dsOpenDirService(&aDirRef);
										if (aResult == eDSNoErr)
										{
											aResult = dsOpenDirNode(aDirRef, usersNode, &aNodeRef);
											dsDataListDeallocatePriv(usersNode);
											free(usersNode);
											usersNode = NULL;
											if ( aResult == eDSNoErr)
											{
												aResult = dsDoDirNodeAuth( aNodeRef, inData->fInAuthMethod, true, inData->fInAuthStepData, inData->fOutAuthStepDataResponse, &tContinue );
												//no checking of continue data
												if (aResult == eDSNoErr)
												{
													//user auth has succeeded on the authentication search policy
													//need to get password out of inData->fInAuthStepData
													tDataList *dataList = nil;
													// parse input first
													dataList = dsAuthBufferGetDataListAllocPriv(inData->fInAuthStepData);
													if ( dataList != nil )
													{
														if ( dsDataListGetNodeCountPriv(dataList) >= 2 )
														{
															char *pwd = nil;
															// this allocates a copy of the password string
															pwd = dsDataListGetNodeStringPriv(dataList, 2);
															if ( pwd != nil )
															{
																siResult = AuthOpen( pContext, userName, pwd, true );
																bAdminUserAuthUsed = true;
																bzero(pwd, strlen(pwd));
																free(pwd);
																pwd = nil;
															}
														}
														dsDataListDeallocatePriv(dataList);
														free(dataList);
														dataList = NULL;
													}
												}
												dsCloseDirNode(aNodeRef);
												aNodeRef = 0;
											}// if ( aResult == eDSNoErr) from aResult = dsOpenDirNode(aDirRef, usersNode, &aNodeRef);
											dsCloseDirService(aDirRef);
										}// if (aResult == eDSNoErr) from aResult = dsOpenDirService(&aDirRef);
									}// if (usersNode != nil)
								}// if (UserIsAdmin)
							}// if ( (pContext->bIsLocal) && !(inData->fInDirNodeAuthOnlyFlag) && (siResult != eDSNoErr) )
							if (bAdminUserAuthUsed)
							{
								// If <UserGUIDString> is non-NULL, then the target user is ShadowHash
								if ( settingPolicy && siResult == eDSNoErr && UserGUIDString != NULL )
								{
									// now that the admin is authorized, set the policy
									uiAuthMethod = settingPolicy;
									
									// transfer ownership of the auth method constant
									dsDataBufferDeallocatePriv( inData->fInAuthMethod );
									inData->fInAuthMethod = origAuthMethod;
									origAuthMethod = NULL;
									siResult = DoShadowHashAuth(inData->fInNodeRef, inData->fInAuthMethod, pContext, 
																	&pContinueData, 
																	inData->fInAuthStepData, 
																	inData->fOutAuthStepDataResponse,
																	inData->fInDirNodeAuthOnlyFlag,
																	false, NULL, UserGUIDString, (const char *)pNIRecType);
								}
								
								throw(siResult);
							}
							else
							{
								throw( (sInt32)eDSAuthFailed ); // unknown user really
							}
						}

						NI_INIT( &niValuesGUID );
						NI_INIT( &niValues );
						gNetInfoMutex->Wait();
						aNIDomain = RetrieveNIDomain(pContext);
						if (aNIDomain != NULL)
						{
							//lookup GUID for ShadowHash verification but don't worry if not found
							niResultGUID = ni_lookupprop( aNIDomain, &niDirID, 
													"generateduid", &niValuesGUID );
							//lookup authauthority attribute
							niResult = ni_lookupprop( aNIDomain, &niDirID, 
													"authentication_authority", &niValues );
						}
						else
						{
							gNetInfoMutex->Signal();
							throw( (sInt32)eDSInvalidSession );
						}
						gNetInfoMutex->Signal();
						if ( (niResult == NI_OK) && (niValues.ni_namelist_len > 0) )
						{
							// loop through all possibilities for set
							// do first auth authority that supports the method for check password
							unsigned int i = 0;
							bool bLoopAll = IsWriteAuthRequest(uiAuthMethod);
							bool bIsSecondary = false;
							char* aaVersion = NULL;
							char* aaTag = NULL;
							char* aaData = NULL;
							
							if ( (niResultGUID == NI_OK) && (niValuesGUID.ni_namelist_len > 0) )
							{
								if (niValuesGUID.ni_namelist_val[0] != nil)
								{
									GUIDString = strdup(niValuesGUID.ni_namelist_val[0]);
								}
								gNetInfoMutex->Wait();
								ni_namelist_free(&niValuesGUID);
								gNetInfoMutex->Signal();
							}
							siResult = eDSAuthMethodNotSupported;
							while ( i < niValues.ni_namelist_len 
									&& (siResult == eDSAuthMethodNotSupported ||
										(bLoopAll && siResult == eDSNoErr)))
							{
								//parse this value of auth authority
								siResult2 = dsParseAuthAuthority( niValues.ni_namelist_val[i], &aaVersion, 
															&aaTag, &aaData );
								// JT need to check version
								if (siResult2 != eDSNoErr)
								{
									siResult = eDSAuthFailed;
									//KW do we want to bail if one of the writes failed?
									//could end up with mismatched passwords/hashes regardless of how we handle this
									//note "continue" here instead of "break" would have same effect because of check for siResult in "while" above
									break;
								}
								
								// if the tag is ;DisabledUser; then we need all of the data section
								if ( aaTag != NULL && strcasecmp(aaTag, kDSTagAuthAuthorityDisabledUser) == 0 )
								{
									if ( aaData != NULL )
										DSFreeString( aaData );
									aaData = strdup( niValues.ni_namelist_val[i] + strlen(aaVersion) + strlen(aaTag) + 2 );
								}
								
								handlerProc = GetNetInfoAuthAuthorityHandler( aaTag );
								if (handlerProc != NULL)
								{
									siResult = (handlerProc)(inData->fInNodeRef, inData->fInAuthMethod, pContext, 
															&pContinueData, 
															inData->fInAuthStepData, 
															inData->fOutAuthStepDataResponse,
															inData->fInDirNodeAuthOnlyFlag,
															bIsSecondary, aaData, GUIDString, (const char *)pNIRecType);
									
									if (siResult == eDSNoErr)
									{
										if (pContinueData != NULL)
										{
											// we are supposed to return continue data
											// remember the proc we used
											pContinueData->fAuthHandlerProc = (void*)handlerProc;
											pContinueData->fAuthAuthorityData = aaData;
											aaData = NULL;
											break;
										}
										else
										{
											bIsSecondary = true;
										}
									}
									
								} // if (handlerProc != NULL)
								else if ( !bIsSecondary )
								{
									siResult = eDSAuthMethodNotSupported;
								}
								if (aaVersion != NULL) {
									free(aaVersion);
									aaVersion = NULL;
								}
								if (aaTag != NULL) {
									free(aaTag);
									aaTag = NULL;
								}
								if (aaData != NULL) {
									free(aaData);
									aaData = NULL;
								}
								++i;
							} //while
							
							if (aaVersion != NULL) {
								free(aaVersion);
								aaVersion = NULL;
							}
							if (aaTag != NULL) {
								free(aaTag);
								aaTag = NULL;
							}
							if (aaData != NULL) {
								free(aaData);
								aaData = NULL;
							}
							
							gNetInfoMutex->Wait();
							ni_namelist_free(&niValues);
							gNetInfoMutex->Signal();
						}
						else
						{
							//revert to basic
							siResult = DoBasicAuth(inData->fInNodeRef,inData->fInAuthMethod, pContext, 
												&pContinueData, inData->fInAuthStepData,
												inData->fOutAuthStepDataResponse,
												inData->fInDirNodeAuthOnlyFlag, false, NULL, NULL, (const char *)pNIRecType);
							if (pContinueData != NULL && siResult == eDSNoErr)
							{
								// we are supposed to return continue data
								// remember the proc we used
								pContinueData->fAuthHandlerProc = (void*)CNiPlugIn::DoBasicAuth;
							}
						}
						
						if ( settingPolicy && siResult == eDSNoErr )
						{
							// now that the admin is authorized, set the global policy
							uiAuthMethod = settingPolicy;
							
							// transfer ownership of the auth method constant
							dsDataBufferDeallocatePriv( inData->fInAuthMethod );
							inData->fInAuthMethod = origAuthMethod;
							origAuthMethod = NULL;
							siResult = DoShadowHashAuth(inData->fInNodeRef, inData->fInAuthMethod, pContext, 
															&pContinueData, 
															inData->fInAuthStepData, 
															inData->fOutAuthStepDataResponse,
															inData->fInDirNodeAuthOnlyFlag,
															false, NULL, UserGUIDString, (const char *)pNIRecType);
						}
					} // //everything but AuthRef method
				}
			}
		} //( inData->fIOContinueData == NULL )
	}

	catch( sInt32 err )
	{
		siResult = err;
	}
	
	DSFreeString( GUIDString );
	DSFreeString( UserGUIDString );
		
	if ( origAuthMethod != NULL )
	{
		dsDataBufferDeallocatePriv( origAuthMethod );
		origAuthMethod = NULL;
	}
	
	if (pNIRecType != NULL)
	{
		free(pNIRecType);
		pNIRecType = nil;
	}
	
	DSFreeString( userName );
	DSFreeString( authenticatorName );
	DSFreePassword( authenticatorPassword );
	
	inData->fResult = siResult;
	inData->fIOContinueData = pContinueData;

	return( siResult );

} // DoAuthenticationOnRecordType


//------------------------------------------------------------------------------------
//	* RecordHasAuthAuthority
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::RecordHasAuthAuthority( const char *inRecordName, const char *inRecType, sNIContextData *inContext, const char *inTag, ni_id *outDirID )
{
	sInt32				siResult			= eDSAuthFailed;
	ni_status			niResult			= NI_OK;
	ni_id				niDirID;
	ni_namelist			niValues;
	
	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSInvalidNodeRef );
		if ( inRecType == nil ) throw( (sInt32)eDSNullRecType );
	
	 
		gNetInfoMutex->Wait();
		void *aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			// get the auth authority
			siResult = IsValidRecordName( inRecordName, inRecType, aNIDomain, niDirID );
			gNetInfoMutex->Signal();
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed ); // unknown user really
		}
		else
		{
			gNetInfoMutex->Signal();
			throw( (sInt32)eDSInvalidSession );
		}
		
		//lookup authauthority attribute
		gNetInfoMutex->Wait();
		aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			niResult = ni_lookupprop( aNIDomain, &niDirID, "authentication_authority", &niValues );
		}
		else
		{
			gNetInfoMutex->Signal();
			throw( (sInt32)eDSInvalidSession );
		}
		gNetInfoMutex->Signal();
			
		if ( (niResult == NI_OK) && (niValues.ni_namelist_len > 0) )
		{
			// loop through all possibilities for set
			// do first auth authority that supports the method for check password
			unsigned int i = 0;
			char* aaVersion = NULL;
			char* aaTag = NULL;
			char* aaData = NULL;
			sInt32 siResult2;
			
			siResult = eDSAuthMethodNotSupported;
			while ( i < niValues.ni_namelist_len && siResult == eDSAuthMethodNotSupported )
			{
				//parse this value of auth authority
				siResult2 = dsParseAuthAuthority( niValues.ni_namelist_val[i], &aaVersion, &aaTag, &aaData );
				// JT need to check version
				if (siResult2 != eDSNoErr)
				{
					siResult = eDSAuthFailed;
					//KW do we want to bail if one of the writes failed?
					//could end up with mismatched passwords/hashes regardless of how we handle this
					//note "continue" here instead of "break" would have same effect because of check for siResult in "while" above
					break;
				}
				
				// If the user is disabled, the entire attribute value must be searched.
				// aaData will be empty because of the double semicolon.
				if ( strcasecmp(inTag, aaTag) == 0 )
				{
					siResult = eDSNoErr;
					break;
				}
				else if ( strcasecmp(kDSTagAuthAuthorityDisabledUser, aaTag) == 0 &&
						  strcasestr(niValues.ni_namelist_val[i], inTag) != NULL )
				{
					siResult = eDSNoErr;
					break;
				}
				
				if (aaVersion != NULL) {
					free(aaVersion);
					aaVersion = NULL;
				}
				if (aaTag != NULL) {
					free(aaTag);
					aaTag = NULL;
				}
				if (aaData != NULL) {
					free(aaData);
					aaData = NULL;
				}
				++i;
			} //while
			
			if (aaVersion != NULL) {
				free(aaVersion);
				aaVersion = NULL;
			}
			if (aaTag != NULL) {
				free(aaTag);
				aaTag = NULL;
			}
			if (aaData != NULL) {
				free(aaData);
				aaData = NULL;
			}
			
			gNetInfoMutex->Wait();
			ni_namelist_free(&niValues);
			gNetInfoMutex->Signal();
		}
	}
	
	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( outDirID != NULL )
		*outDirID = niDirID;
	
	return( siResult );
}


//------------------------------------------------------------------------------------
//	* IsWriteAuthRequest
//------------------------------------------------------------------------------------

bool CNiPlugIn::IsWriteAuthRequest ( uInt32 uiAuthMethod )
{
	switch ( uiAuthMethod )
	{
		case kAuthSetPasswd:
		case kAuthSetPasswdAsRoot:
		case kAuthChangePasswd:
		case kAuthSetGlobalPolicy:
		case kAuthSetPolicy:
			return true;
			
		default:
			return false;
	}
} // IsWriteAuthRequest


#pragma mark -
#pragma mark LocalCachedUser
#pragma mark -

//------------------------------------------------------------------------------------
//	* DoLocalCachedUserAuth
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoLocalCachedUserAuth ( 	tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod, 
											sNIContextData* inContext, 
											sNIContinueData** inOutContinueData, 
											tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
											bool inAuthOnly, bool isSecondary,
											const char* inAuthAuthorityData, const char* inGUIDString,
											const char* inNativeRecType )
{
	sInt32					siResult				= eDSAuthFailed;
	bool					bNetworkNodeReachable	= false;
	
	// are we online or offline?
	siResult = LocalCachedUserReachable( inNodeRef, inAuthMethod, inContext, inOutContinueData, 
										 inAuthData, outAuthData, inAuthAuthorityData, inGUIDString, inNativeRecType, &bNetworkNodeReachable );
	if ( siResult == eDSNoErr )
	{
		siResult = DoLocalCachedUserAuthPhase2( inNodeRef, inAuthMethod, inContext, inOutContinueData,
												inAuthData, outAuthData, inAuthOnly, isSecondary,
												inAuthAuthorityData, inGUIDString, inNativeRecType, bNetworkNodeReachable );
	}
	
	if (inContext->fLocalCacheNetNode != NULL)
	{
		dsDataListDeallocatePriv(inContext->fLocalCacheNetNode);
		free(inContext->fLocalCacheNetNode);
		inContext->fLocalCacheNetNode = NULL;
	}
	if (inContext->fLocalCacheRef != 0)
	{
		dsCloseDirService(inContext->fLocalCacheRef);
		inContext->fLocalCacheRef = 0;
	}
	
	return( siResult );
} // DoLocalCachedUserAuth


//------------------------------------------------------------------------------------
//	* LocalCachedUserReachable
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::LocalCachedUserReachable(	tDirNodeReference inNodeRef,
											tDataNodePtr inAuthMethod, 
											sNIContextData* inContext, 
											sNIContinueData** inOutContinueData, 
											tDataBufferPtr inAuthData, 
											tDataBufferPtr outAuthData,
											const char* inAuthAuthorityData,
											const char* inGUIDString,
											const char* inNativeRecType,
											bool *inOutNodeReachable )
{
	sInt32					siResult				= eDSAuthFailed;
	uInt32					uiAuthMethod			= 0;
	char				   *networkNodename			= nil;
	char				   *userRecordName			= nil;
	char				   *userGUID				= nil;
	sInt32					result					= eDSNoErr;
	tDataBuffer			   *dataBuffer				= nil;
	uInt32					nodeCount				= 0;
	tDirNodeReference		aSearchNodeRef			= 0;
	tDataList			   *pSearchNode				= nil;
	tDataList			   *pSearchNodeList			= nil;
	tAttributeListRef		attrListRef				= 0;
	tAttributeValueListRef	attrValueListRef		= 0;
	tAttributeValueEntry   *pAttrValueEntry			= nil;
	tAttributeEntry		   *pAttrEntry				= nil;
	uInt32					aIndex					= 0;
		
	if ( inAuthData == nil ) return( (sInt32)eDSNullAuthStepData );
	if ( inContext == nil ) return( (sInt32)eDSInvalidNodeRef );
	if ( inOutNodeReachable == nil ) return( (sInt32)eParameterError );
	
	*inOutNodeReachable = false;
	
	DBGLOG( kLogPlugin, "LocalCachedUserReachable::" );
	if (inAuthMethod != nil)
		DBGLOG1( kLogPlugin, "NetInfo PlugIn: Attempting use of authentication method %s", inAuthMethod->fBufferData );
	
	try
	{
		siResult = dsGetAuthMethodEnumValue( inAuthMethod, &uiAuthMethod );
		if ( siResult == eDSNoErr )
		{
			siResult = ParseLocalCacheUserData(inAuthAuthorityData, &networkNodename, &userRecordName, &userGUID);
			
			result = dsOpenDirService(&inContext->fLocalCacheRef);
			if (result == eDSNoErr)
			{
				inContext->fLocalCacheNetNode = dsBuildFromPathPriv( networkNodename, "/" );
				if ( inContext->fLocalCacheNetNode == nil ) throw( (sInt32)eMemoryError );
				dataBuffer = ::dsDataBufferAllocate( inContext->fLocalCacheRef, 1024 );
				if ( dataBuffer == nil ) throw( (sInt32)eMemoryError );

				// if this is the Active Directory plugin we will make an exception because the plugin always allows
				// itself to be opened and does not always register all of it's nodes
				if ( strncmp("/Active Directory/", networkNodename, sizeof("/Active Directory/")-1) == 0 )
				{
					*inOutNodeReachable = true;
					siResult = eDSNoErr;
					throw( siResult );
				}
								
				result = dsFindDirNodes( inContext->fLocalCacheRef, dataBuffer, inContext->fLocalCacheNetNode, eDSiExact, &nodeCount, nil );
				if ( (result == eDSNoErr) && (nodeCount == 1) )
				{
					//now check if the node is actually on the search policy
					//get thesearch node. open it and call dsgetdirnodeinfo for kDS1AttrSearchPath
					//extract the node list
					result = dsFindDirNodes( inContext->fLocalCacheRef, dataBuffer, nil, eDSAuthenticationSearchNodeName, &nodeCount, nil );
					if ( ( result == eDSNoErr ) && ( nodeCount == 1 ) )
					{
						result = dsGetDirNodeName( inContext->fLocalCacheRef, dataBuffer, 1, &pSearchNode );
						if ( result == eDSNoErr )
						{
							result = dsOpenDirNode( inContext->fLocalCacheRef, pSearchNode, &aSearchNodeRef );
							if ( pSearchNode != NULL )
							{
								dsDataListDeallocatePriv( pSearchNode );
								free( pSearchNode );
								pSearchNode = NULL;
							}
							if ( result == eDSNoErr )
							{
								pSearchNodeList = dsBuildFromPathPriv( kDS1AttrSearchPath, "/" );
								if ( pSearchNodeList == nil ) throw( (sInt32)eMemoryError );
								do
								{
									nodeCount = 0;
									result = dsGetDirNodeInfo( aSearchNodeRef, pSearchNodeList, dataBuffer, false, &nodeCount, &attrListRef, nil );
									if (result == eDSBufferTooSmall)
									{
										uInt32 bufSize = dataBuffer->fBufferSize;
										dsDataBufferDeallocatePriv( dataBuffer );
										dataBuffer = nil;
										dataBuffer = ::dsDataBufferAllocate( inContext->fLocalCacheRef, bufSize * 2 );
									}
								} while (result == eDSBufferTooSmall);
								
								if ( ( result == eDSNoErr ) && (nodeCount > 0) )
								{	
									//assume first attribute since only 1 expected
									result = dsGetAttributeEntry( aSearchNodeRef, dataBuffer, attrListRef, 1, &attrValueListRef, &pAttrEntry );
									if ( result != eDSNoErr ) throw( result );
									
									//retrieve the node path strings
									for (aIndex=1; aIndex < (pAttrEntry->fAttributeValueCount+1); aIndex++)
									{
										result = dsGetAttributeValue( aSearchNodeRef, dataBuffer, aIndex, attrValueListRef, &pAttrValueEntry );
										if ( result != eDSNoErr ) throw( result );
										if ( pAttrValueEntry->fAttributeValueData.fBufferData == nil ) throw( (sInt32)eMemoryAllocError );
										
										if (strcmp( networkNodename, pAttrValueEntry->fAttributeValueData.fBufferData ) == 0 )
										{
											char *chPtr = strstr( networkNodename, kstrNetInfoName);
											if ( chPtr != nil )
											{
												//we found the user on some netinfo node
												//so now check if there are any parent nodes or we are local only
												tDataList *pNodeNameDL = nil;
												//call to check if there exists a NetInfo parent node
												pNodeNameDL = ::dsBuildListFromStringsPriv( "NetInfo", "..", nil );
												if (pNodeNameDL != nil)
												{
													//try to open the parent NetInfo node
													sInt32				openResult	= eDSNoErr;
													tDirNodeReference	aNodeRef	= 0;
													openResult = dsOpenDirNode( inContext->fLocalCacheRef, pNodeNameDL, &aNodeRef );
													if ( openResult == eDSNoErr )
													{
														*inOutNodeReachable = true; //at least a netinfo hierarchy is present
														dsCloseDirNode(aNodeRef);
														aNodeRef = 0;
													}
													dsDataListDeAllocate( inContext->fLocalCacheRef, pNodeNameDL, false );
													free(pNodeNameDL);
													pNodeNameDL = nil;
												}
											}
											else
											{
												*inOutNodeReachable = true; //node is registered in DS
											}
											dsDeallocAttributeValueEntry(inContext->fLocalCacheRef, pAttrValueEntry);
											pAttrValueEntry = nil;
											break;
										}
										dsDeallocAttributeValueEntry(inContext->fLocalCacheRef, pAttrValueEntry);
										pAttrValueEntry = nil;
									}
									
									dsCloseAttributeList(attrListRef);
									dsCloseAttributeValueList(attrValueListRef);
									dsDeallocAttributeEntry(inContext->fLocalCacheRef, pAttrEntry);
									pAttrEntry = nil;
									
									//close dir node after releasing attr references
									result = ::dsCloseDirNode(aSearchNodeRef);
									aSearchNodeRef = 0;
									if ( result != eDSNoErr ) throw( result );
								}
							}
						}
					}
					//formerly no check for network node on the search policy
					//*inOutNodeReachable = true; //node is registered in DS
				}
			}
		}
	}
	catch( sInt32 err )
	{
		siResult = err;
	}
	
	//cleanup
	DSFreeString( networkNodename );
	DSFreeString( userRecordName );
	DSFreeString( userGUID );
	
    if ( dataBuffer != nil )
	{
        dsDataBufferDeallocatePriv( dataBuffer );
		dataBuffer = nil;
	}

	return( siResult );	
}


//------------------------------------------------------------------------------------
//	* DoLocalCachedUserAuthPhase2
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoLocalCachedUserAuthPhase2	( tDirNodeReference inNodeRef,
												  tDataNodePtr inAuthMethod, 
												  sNIContextData* inContext, 
												  sNIContinueData** inOutContinueData, 
												  tDataBufferPtr inAuthData, 
												  tDataBufferPtr outAuthData, 
												  bool inAuthOnly,
												  bool isSecondary,
												  const char* inAuthAuthorityData,
												  const char* inGUIDString,
												  const char* inNativeRecType,
												  bool inNetNodeReachable )
{
	sInt32					siResult				= eDSAuthFailed;
	uInt32					uiAuthMethod			= 0;
	sInt32					result					= eDSNoErr;
	tDirNodeReference		aNodeRef				= 0;
	tDataNode			   *authMethodPtr			= nil;
	bool					bAuthLocally			= false;
	sNIContextData		   *tmpContext				= nil;
	tDataListPtr			dataList				= NULL;
	char				   *pUserName				= nil;
	char				   *pNewPassword			= nil;
	unsigned int			itemCount				= 0;
	char				   *policyStr				= NULL;
	unsigned long			policyStrLen			= 0;
    tDataBufferPtr          tmpAuthBuffer           = NULL;
	
	try
	{
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
		if ( inContext == nil ) throw( (sInt32)eDSInvalidNodeRef );
		
		siResult = dsGetAuthMethodEnumValue( inAuthMethod, &uiAuthMethod );
		if ( siResult == eDSNoErr )
		{
			siResult = eDSAuthFailed;
			// look at auth method request and decide what to do
			switch( uiAuthMethod )
			{
				case kAuthSetPasswdAsRoot:
					//do local only with no network sync as root is local
					//note here that password may become unsynced
				case kAuthWriteSecureHash:
				case kAuthReadSecureHash:
					//supported since local cached users are completely copied upon cache updates
				case kAuthSecureHash:
					//local only since cannot sync this hash ie. no password
					siResult = CNiPlugIn::DoShadowHashAuth(	inNodeRef,
															inAuthMethod,
															inContext,
															inOutContinueData,
															inAuthData,
															outAuthData,
															inAuthOnly,
															isSecondary,
															kLocalCachedUserHashList,
															inGUIDString,
															inNativeRecType );
					break;

				case kAuthChangePasswd:
                    //network and if success then local otherwise fails immediately
                    if (inNetNodeReachable && inContext->fLocalCacheNetNode != NULL)
                    {
                        if ( inContext->fLocalCacheRef != 0 )
                            result = eDSNoErr;
                        else
                            result = dsOpenDirService(&inContext->fLocalCacheRef);
                        if (result == eDSNoErr)
                        {
                            result = dsOpenDirNode(inContext->fLocalCacheRef, inContext->fLocalCacheNetNode, &aNodeRef);
                            if ( result == eDSNoErr)
                            {
                                //user records only here
                                siResult = dsDoDirNodeAuth( aNodeRef, inAuthMethod, inAuthOnly, inAuthData, outAuthData, nil );
                                //no checking of continue data
                                if (siResult == eDSNoErr)
                                {
                                    siResult = CNiPlugIn::DoShadowHashAuth(	inNodeRef,
                                                                            inAuthMethod,
                                                                            inContext,
                                                                            inOutContinueData,
                                                                            inAuthData,
                                                                            outAuthData,
                                                                            inAuthOnly,
                                                                            isSecondary,
                                                                            kLocalCachedUserHashList,
                                                                            inGUIDString,
                                                                            inNativeRecType );
                                    
                                    if (siResult != eDSNoErr)
                                    {
                                        DBGLOG( kLogPlugin, "DoLocalCachedUserAuth::Local auth not sync'ed so try to set local password" );
                                        //set the auth method to SetPasswdAsRoot for local password sync
                                        authMethodPtr = dsDataNodeAllocateString( inContext->fLocalCacheRef, kDSStdAuthSetPasswdAsRoot );
                                        if (authMethodPtr == nil ) throw( (sInt32)eMemoryError );
                                        
                                        //repackage the context data to ensure uid == 0 for writing shadow hash
                                        tmpContext = (sNIContextData *) calloc(1, sizeof(sNIContextData));
                                        //directly assign pointers so no need to free internals after call
                                        //not all these are actually needed but assigned for completeness
                                        tmpContext->fDomain					= inContext->fDomain;
                                        tmpContext->fDomainName				= inContext->fDomainName;
                                        tmpContext->fDomainPath			= inContext->fDomainPath;
                                        tmpContext->fRecType				= inContext->fRecType;
                                        tmpContext->fRecName				= inContext->fRecName;
                                        tmpContext->dirID					= inContext->dirID;
                                        tmpContext->offset					= inContext->offset;
                                        tmpContext->fDontUseSafeClose		= inContext->fDontUseSafeClose;
                                        tmpContext->bDidWriteOperation		= inContext->bDidWriteOperation;
                                        tmpContext->fUID					= getuid();
                                        tmpContext->fEffectiveUID			= geteuid();
                                        tmpContext->fAuthenticatedUserName	= inContext->fAuthenticatedUserName;
                                        tmpContext->fPWSRef					= inContext->fPWSRef;
                                        tmpContext->fPWSNodeRef				= inContext->fPWSNodeRef;
                                        tmpContext->bIsLocal				= true;
                                        
                                        // need to repackage the auth buffer because change is different than setPassword
                                        tmpAuthBuffer = dsDataBufferAllocatePriv( inAuthData->fBufferLength );
                                        
                                        char *pSrcPtr = inAuthData->fBufferData;
                                        char *pDstPtr = tmpAuthBuffer->fBufferData;
                                        
                                        long lTempLen = *((long *) pSrcPtr);
                                        
                                        // copy name to other buffer
                                        bcopy( pSrcPtr, pDstPtr, 4 + lTempLen );
                                        tmpAuthBuffer->fBufferLength += 4 + lTempLen;
                                        
                                        pSrcPtr += 4 + lTempLen;
                                        pDstPtr += 4 + lTempLen;
                                        
                                        // skip the old password
                                        lTempLen = *((long *) pSrcPtr);
                                        pSrcPtr += 4 + lTempLen;
                                        
                                        // get new password len
                                        lTempLen = *((long *) pSrcPtr);
                                        
                                        // copy new password to other buffer
                                        bcopy( pSrcPtr, pDstPtr, 4 + lTempLen );
                                        tmpAuthBuffer->fBufferLength += 4 + lTempLen;
                                        
                                        //no need to repackage the auth buffer for the call to SetPasswdAsRoot locally
                                        //ie. still username and password so should work fine
                                        siResult = CNiPlugIn::DoShadowHashAuth(	inNodeRef,
                                                                                authMethodPtr,
                                                                                tmpContext,
                                                                                inOutContinueData,
                                                                                tmpAuthBuffer,
                                                                                outAuthData,
                                                                                inAuthOnly,
                                                                                isSecondary,
                                                                                kLocalCachedUserHashList,
                                                                                inGUIDString,
                                                                                inNativeRecType );
                                        
                                        dsDataBufferDeallocatePriv( tmpAuthBuffer );
                                        tmpAuthBuffer = NULL;
                                        
                                        free(tmpContext);
                                        tmpContext = nil;
                                    }
                                }
                            }
                        }
                    }
                    break;
                    
				case kAuthNativeClearTextOK:
				case kAuthNativeNoClearText:
				case kAuthNativeMethod:
					//network if available and if success then switch to kAuthSetPasswdAsRoot
					//otherwise local auth only
					bAuthLocally = false;
					if (inNetNodeReachable && inContext->fLocalCacheNetNode != NULL)
					{
						if ( inContext->fLocalCacheRef != 0 )
							result = eDSNoErr;
						else
							result = dsOpenDirService(&inContext->fLocalCacheRef);
						if (result == eDSNoErr)
						{
							result = dsOpenDirNode(inContext->fLocalCacheRef, inContext->fLocalCacheNetNode, &aNodeRef);
							if ( result == eDSNoErr)
							{
								//user records only here
								//network auth should not have session changed so inAuthOnly fixed at true
								
								siResult = dsDoDirNodeAuth( aNodeRef, inAuthMethod, true, inAuthData, outAuthData, nil );
								//no checking of continue data
								if (siResult == eDSNoErr)
								{
									//try the local auth as well to see if the password needs to be sync'ed up
									siResult = CNiPlugIn::DoShadowHashAuth(	inNodeRef,
																			inAuthMethod,
																			inContext,
																			inOutContinueData,
																			inAuthData,
																			outAuthData,
																			inAuthOnly,
																			isSecondary,
																			kLocalCachedUserHashList,
																			inGUIDString,
																			inNativeRecType );
									if (siResult != eDSNoErr)
									{
										DBGLOG( kLogPlugin, "DoLocalCachedUserAuth::Local auth not sync'ed so try to set local password" );
										//set the auth method to SetPasswdAsRoot for local password sync
										authMethodPtr = dsDataNodeAllocateString( inContext->fLocalCacheRef, kDSStdAuthSetPasswdAsRoot );
										if (authMethodPtr == nil ) throw( (sInt32)eMemoryError );
										
										//repackage the context data to ensure uid == 0 for writing shadow hash
										tmpContext = (sNIContextData *) calloc(1, sizeof(sNIContextData));
										//directly assign pointers so no need to free internals after call
										//not all these are actually needed but assigned for completeness
										tmpContext->fDomain					= inContext->fDomain;
										tmpContext->fDomainName				= inContext->fDomainName;
										tmpContext->fDomainPath			= inContext->fDomainPath;
										tmpContext->fRecType				= inContext->fRecType;
										tmpContext->fRecName				= inContext->fRecName;
										tmpContext->dirID					= inContext->dirID;
										tmpContext->offset					= inContext->offset;
										tmpContext->fDontUseSafeClose		= inContext->fDontUseSafeClose;
										tmpContext->bDidWriteOperation		= inContext->bDidWriteOperation;
										tmpContext->fUID					= getuid();
										tmpContext->fEffectiveUID			= geteuid();
										tmpContext->fAuthenticatedUserName	= inContext->fAuthenticatedUserName;
										tmpContext->fPWSRef					= inContext->fPWSRef;
										tmpContext->fPWSNodeRef				= inContext->fPWSNodeRef;
										tmpContext->bIsLocal				= true;
										
										//no need to repackage the auth buffer for the call to SetPasswdAsRoot locally
										//ie. still username and password so should work fine
										siResult = CNiPlugIn::DoShadowHashAuth(	inNodeRef,
																				authMethodPtr,
																				tmpContext,
																				inOutContinueData,
																				inAuthData,
																				outAuthData,
																				inAuthOnly,
																				isSecondary,
																				kLocalCachedUserHashList,
																				inGUIDString,
																				inNativeRecType );
										free(tmpContext);
										tmpContext = nil;
									}
								}
								else if (siResult == eDSCannotAccessSession)
								{
									// if got a session error, the node must not be available so we should auth locally
									DBGLOG( kLogPlugin, "DoLocalCachedUserAuth::Original node failed with eDSCannotAccessSession, authenticate locally" );
									bAuthLocally = true;
								}								
							}
							else
							{
								//couldn't open the network node so go ahead and auth locally
								bAuthLocally = true;
							}
						}
					}
					else
					{
						bAuthLocally = true;
					}
					if (bAuthLocally)
					{
						siResult = CNiPlugIn::DoShadowHashAuth(	inNodeRef,
																inAuthMethod,
																inContext,
																inOutContinueData,
																inAuthData,
																outAuthData,
																inAuthOnly,
																isSecondary,
																kLocalCachedUserHashList,
																inGUIDString,
																inNativeRecType );
					}
					break;

				case kAuthSetPolicyAsRoot:
					// parse input first, using <pNewPassword> to hold the policy string
					siResult = Get2FromBuffer(inAuthData, &dataList, &pUserName, &pNewPassword, &itemCount );
					if ( siResult != eDSNoErr )
						throw( siResult );
					if ( itemCount != 2 || pNewPassword == NULL || pNewPassword[0] == '\0' )
						throw( (sInt32)eDSInvalidBuffFormat );
					
					siResult = SetUserPolicies( inContext, inNativeRecType, pUserName, pNewPassword, NULL );
					break;
				
				case kAuthGetPolicy:
					// possible to return data
					if ( outAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
					outAuthData->fBufferLength = 0;
					
					// parse input
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					itemCount = dsDataListGetNodeCountPriv(dataList);
					if ( itemCount != 3 ) throw( (sInt32)eDSInvalidBuffFormat );
					
					// this allocates a copy of the string
					pUserName = dsDataListGetNodeStringPriv(dataList, 3);
					if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(pUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
					
					// get policy attribute
					siResult = GetUserPolicies( inContext, inNativeRecType, pUserName, NULL, &policyStr );
					if ( siResult != 0 ) throw( siResult );
					
					if ( policyStr != NULL )
						policyStrLen = strlen( policyStr );
					if ( outAuthData->fBufferSize < 4 + policyStrLen )
						throw( (sInt32)eDSBufferTooSmall );
							
					outAuthData->fBufferLength = 4 + policyStrLen;
					memcpy( outAuthData->fBufferData, &policyStrLen, 4 );
					if ( policyStrLen > 0 )
						memcpy( outAuthData->fBufferData + 4, policyStr, policyStrLen );
					break;
				
				case kAuthSetPolicy:
					// parse input
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					itemCount = dsDataListGetNodeCountPriv(dataList);
					if ( itemCount != 4 ) throw( (sInt32)eDSInvalidBuffFormat );
					
					// this allocates a copy of the string
					pUserName = dsDataListGetNodeStringPriv(dataList, 3);
					if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( pUserName[0] == '\0' ) throw( (sInt32)eDSInvalidBuffFormat );
					
					pNewPassword = dsDataListGetNodeStringPriv(dataList, 4);
					if ( pNewPassword == nil )
						throw( (sInt32)eDSInvalidBuffFormat );
					if ( pNewPassword[0] == '\0' )
						throw( (sInt32)eDSInvalidBuffFormat );
						
					siResult = SetUserPolicies( inContext, inNativeRecType, pUserName, pNewPassword, NULL );
					break;

				case kAuthSetPasswd:
					//could allow call to network and if success then switch to kAuthSetPasswdAsRoot for local
				case kAuthSMB_NT_Key:
				case kAuthSMB_LM_Key:
					//not supported at this time 05-16-03
					//not discussed in any design conversations
				default:
					siResult = eDSAuthMethodNotSupported;
					break;
			}
		}
	}
	catch( sInt32 err )
	{
		siResult = err;
	}
	
	//cleanup
	if (authMethodPtr != nil)
	{
		dsDataBufferDeallocatePriv( authMethodPtr );
		authMethodPtr = nil;
	}
	if (aNodeRef != 0)
	{
		dsCloseDirNode(aNodeRef);
		aNodeRef = 0;
	}
	if (inContext->fLocalCacheNetNode != NULL)
	{
		dsDataListDeallocatePriv(inContext->fLocalCacheNetNode);
		free(inContext->fLocalCacheNetNode);
		inContext->fLocalCacheNetNode = NULL;
	}
	if (inContext->fLocalCacheRef != 0)
	{
		dsCloseDirService(inContext->fLocalCacheRef);
		inContext->fLocalCacheRef = 0;
	}
	if (dataList != NULL)
	{
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
	}
	
	DSFreeString( pUserName );
	DSFreePassword( pNewPassword );
	DSFreeString( policyStr );
	
	return( siResult );
}


#pragma mark -
#pragma mark DisabledUser
#pragma mark -

//------------------------------------------------------------------------------------
//	* DoDisabledAuth
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoDisabledAuth ( 		tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod, 
										sNIContextData* inContext, 
										sNIContinueData** inOutContinueData, 
										tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
										bool inAuthOnly, bool isSecondary,
										const char* inAuthAuthorityData, const char* inGUIDString,
										const char* inNativeRecType )
{
	sInt32				siResult						= eDSAuthFailed;
	uInt32				uiAuthMethod					= 0;
	bool				bNetworkNodeReachable			= false;
	const char			*startOfOrigAuthAuthorityTag	= NULL;
	char				*pUserName						= NULL;
	tDataListPtr		dataList						= NULL;
	
	siResult = dsGetAuthMethodEnumValue( inAuthMethod, &uiAuthMethod );
	if ( siResult != eDSNoErr )
		return siResult;
	
	// The disabled auth authority keeps the enabled version in the data section.
	// Example: ;DisabledUser;;ShadowHash;
	// Double-check that the original auth-authority is ShadowHash and allow get policy methods.
	// We can check for a non-NULL auth-authority data, but the data is not included because of the extra
	// semicolon. 
	if ( inAuthAuthorityData != NULL )
	{
		startOfOrigAuthAuthorityTag = strcasestr( inAuthAuthorityData, kDSValueAuthAuthorityLocalCachedUser );
		if ( startOfOrigAuthAuthorityTag != NULL )
		{
			siResult = LocalCachedUserReachable( inNodeRef, inAuthMethod, inContext, inOutContinueData, 
												 inAuthData, outAuthData,
												 startOfOrigAuthAuthorityTag + sizeof(kDSValueAuthAuthorityLocalCachedUser) - 1,
												 inGUIDString, inNativeRecType, &bNetworkNodeReachable );

			if ( siResult != eDSNoErr )
				return( siResult );
			if ( bNetworkNodeReachable )
			{
				siResult = DoLocalCachedUserAuthPhase2( inNodeRef, inAuthMethod, inContext, inOutContinueData,
														inAuthData, outAuthData, inAuthOnly, isSecondary,
														startOfOrigAuthAuthorityTag + sizeof(kDSValueAuthAuthorityLocalCachedUser) - 1,
														inNativeRecType, inGUIDString, bNetworkNodeReachable );
				
				if ( siResult == eDSNoErr )
				{
					switch( uiAuthMethod )
					{
						// re-enable the user for these methods
						case kAuthNativeClearTextOK:
						case kAuthNativeNoClearText:
						case kAuthNativeMethod:
							dataList = dsAuthBufferGetDataListAllocPriv( inAuthData );
							if ( dataList != NULL )
							{
								if ( dsDataListGetNodeCountPriv(dataList) >= 1 ) 
								{
									pUserName = dsDataListGetNodeStringPriv(dataList, 1);
									if ( pUserName != NULL )
									{
										if ( strlen(pUserName) > 0 )
											SetUserAuthAuthorityAsRoot( inContext, inNativeRecType, pUserName, startOfOrigAuthAuthorityTag );
										DSFreeString( pUserName );
									}
								}
								dsDataListDeallocatePriv( dataList );
								free( dataList );
								dataList = NULL;
							}
							break;
						
						default:
							break;
					}
				}
				
				return siResult;
			}
			else
			{
				return( eDSAuthAccountDisabled );
			}
		}
		
		switch ( uiAuthMethod )
		{
			case kAuthGetPolicy:
			case kAuthGetEffectivePolicy:
			case kAuthSetPolicyAsRoot:
			case kAuthSetPasswdAsRoot:
				siResult = CNiPlugIn::DoShadowHashAuth( inNodeRef, inAuthMethod, inContext, inOutContinueData, 
										inAuthData, outAuthData, inAuthOnly, isSecondary, inAuthAuthorityData,
										inGUIDString, inNativeRecType );
				break;
			
			default: 
				siResult = eDSAuthFailed;
		}
	}
	
	return siResult;
} // DoDisabledAuth


#pragma mark -
#pragma mark Basic
#pragma mark -

//------------------------------------------------------------------------------------
//	* DoBasicAuth
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoBasicAuth ( tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod, 
								sNIContextData* inContext, 
								sNIContinueData** inOutContinueData, 
								tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
								bool inAuthOnly, bool isSecondary, const char* inAuthAuthorityData,
								const char* inGUIDString, const char* inNativeRecType )
{
	sInt32				siResult		= eDSNoErr;
	uInt32				uiAuthMethod	= 0;

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		DBGLOG( kLogPlugin, "DoBasicAuth::" );
		if (inAuthMethod != nil)
		{
			DBGLOG1( kLogPlugin, "NetInfo PlugIn: Attempting use of authentication method %s", inAuthMethod->fBufferData );
		}
		siResult = dsGetAuthMethodEnumValue( inAuthMethod, &uiAuthMethod );
		if ( siResult == eDSNoErr )
		{
			switch( uiAuthMethod )
			{
				case kAuthGetGlobalPolicy:
				case kAuthSetGlobalPolicy:
				case kAuthGetPolicy:
				case kAuthSetPolicy:
					siResult = eDSAuthMethodNotSupported;
					break;
				
				//native auth is always UNIX crypt possibly followed by 2-way random using tim auth server
				case kAuthNativeMethod:
				case kAuthNativeNoClearText:
				case kAuthNativeClearTextOK:
					if ( outAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
					siResult = DoUnixCryptAuth( inContext, inAuthData, inAuthOnly, inNativeRecType );
					if ( siResult == eDSNoErr )
					{
						if ( outAuthData->fBufferSize > ::strlen( kDSStdAuthCrypt ) )
						{
							::strcpy( outAuthData->fBufferData, kDSStdAuthCrypt );
						}
					}
					else if ( (siResult != eDSAuthFailed) && (siResult != eDSInvalidBuffFormat) )
					{
						siResult = DoNodeNativeAuth( inContext, inAuthData, inAuthOnly, inNativeRecType );
						if ( siResult == eDSNoErr )
						{
							if ( outAuthData->fBufferSize > ::strlen( kDSStdAuth2WayRandom ) )
							{
								::strcpy( outAuthData->fBufferData, kDSStdAuth2WayRandom );
							}
						}
					}
					break;

				case kAuthClearText:
				case kAuthCrypt:
					siResult = DoUnixCryptAuth( inContext, inAuthData, inAuthOnly, inNativeRecType );
					break;

				case kAuthSetPasswd:
					siResult = DoSetPassword( inContext, inAuthData, inNativeRecType );
					break;

				case kAuthSetPasswdAsRoot:
					siResult = DoSetPasswordAsRoot( inContext, inAuthData, inNativeRecType );
					break;

				case kAuthChangePasswd:
					siResult = DoChangePassword( inContext, inAuthData, isSecondary, inNativeRecType );
					break;

#ifdef TIM_CLIENT_PRESENT
				case kAuthSMB_NT_Key:
				case kAuthSMB_LM_Key:
					siResult = DoTimSMBAuth( inContext, inAuthData, uiAuthMethod );
					break;

				case kAuth2WayRandom:
					siResult = DoTimMultiPassAuth( inNodeRef, inAuthMethod, inContext, 
												   inOutContinueData, inAuthData, 
												   outAuthData, inAuthOnly );
					break;

				case kAuthAPOP:
				case kAuthCRAM_MD5:
					siResult = ValidateDigest( inContext, inAuthData, uiAuthMethod );
					break;
#endif

				default:
					siResult = eDSAuthFailed;
			}
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // DoBasicAuth


//------------------------------------------------------------------------------------
//	* DoSetPassword
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoSetPassword ( sNIContextData *inContext, tDataBuffer *inAuthData, const char *inNativeRecType )
{
	sInt32				siResult		= eDSAuthFailed;
	bool				bFreePropList	= false;
	bool				bResetCache		= false;
	char			   *pData			= nil;
	char			   *userName		= nil;
	uInt32				userNameLen		= 0;
	char			   *userPwd			= nil;
	uInt32				userPwdLen		= 0;
	char			   *rootName		= nil;
	uInt32				rootNameLen		= 0;
	char			   *rootPwd			= nil;
	uInt32				rootPwdLen		= 0;
	uInt32				offset			= 0;
	uInt32				buffSize		= 0;
	uInt32				buffLen			= 0;
	ni_proplist			niPropList;
#ifdef TIM_CLIENT_PRESENT
	TIMHandle		   *pTimHndl		= nil;
#endif
	void			   *aNIDomain		= NULL;


	gNetInfoMutex->Wait();

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );

		pData = inAuthData->fBufferData;
		buffSize = inAuthData->fBufferSize;
		buffLen = inAuthData->fBufferLength;

		if ( buffLen > buffSize ) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the length of the first data block and verify that it
		//	is greater than 0 and doesn't go past the end of the buffer
		//	(user name)

		if ( buffLen < 4 * sizeof( unsigned long ) + 2 ) throw( (sInt32)eDSInvalidBuffFormat );
		// need length for username, password, root username, and root password.
		// both usernames must be at least one character
		::memcpy( &userNameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (userNameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (offset + userNameLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		userName = (char *)::calloc( 1, userNameLen + 1 );
		::memcpy( userName, pData, userNameLen );
		pData += userNameLen;
		offset += userNameLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the users new password
		::memcpy( &userPwdLen, pData, sizeof( unsigned long ) );
  		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + userPwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
		if (userPwdLen >= kHashRecoverableLength) throw ( (sInt32)eDSAuthPasswordTooLong );

		userPwd = (char *)::calloc( 1, userPwdLen + 1 );
		::memcpy( userPwd, pData, userPwdLen );
		pData += userPwdLen;
		offset += userPwdLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the root users name
		::memcpy( &rootNameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (rootNameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (rootNameLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		rootName = (char *)::calloc( 1, rootNameLen + 1 );
		::memcpy( rootName, pData, rootNameLen );
		pData += rootNameLen;
		offset += rootNameLen;
		if (sizeof( unsigned long ) > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the root users password
		::memcpy( &rootPwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (rootPwdLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		if (rootPwdLen >= kHashRecoverableLength) throw ( (sInt32)eDSAuthPasswordTooLong );

		rootPwd = (char *)::calloc( 1, rootPwdLen + 1 );
		::memcpy( rootPwd, pData, rootPwdLen );
		pData += rootPwdLen;
		offset += rootPwdLen;

		aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{

#ifdef TIM_CLIENT_PRESENT
			//opt out early if we know tim is not there **** check on local first
			if ( !inContext->bIsLocal && IsTimRunning() )
			{
				pTimHndl = ::timServerForDomain( aNIDomain );
				if ( pTimHndl != nil )
				{
					// Set the password for the user
					siResult = ::timSetPasswordWithTIMHandle( pTimHndl, rootName, rootPwd, userName, userPwd );
					timHandleFree( pTimHndl );

					if ( siResult != eDSNoErr )
					{
					
					}
				}

#ifdef DEBUG
				if ( siResult != TimStatusOK )
				{
					DBGLOG1( kLogPlugin, "-- timSetPasswordForUser -- failed with %l.", siResult );
				}
#endif
				siResult = MapAuthResult( siResult );
			}
			else
#endif
			{
				//need some code here to directly set the NI Crypt password
				//also first check to see if the password already exists

				char			salt[3];
				char			hashPwd[ 32 ];
				ni_id			niDirID;
				ni_index   		niWhere			= 0;
				ni_namelist		niValue;
				char		   *niPwd			= nil;

				//need some code here to directly change the NI Crypt password
				//first check to see that the old password is valid
				DBGLOG( kLogPlugin, "Attempting UNIX Crypt password change" );
				siResult = IsValidRecordName ( rootName, inNativeRecType, aNIDomain, niDirID );

#ifdef DEBUG
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
#else
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
#endif

				siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
#ifdef DEBUG
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
				if ( niPropList.ni_proplist_val == nil ) throw( (sInt32)eDSAuthUnknownUser );
#else
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
				if ( niPropList.ni_proplist_val == nil ) throw( (sInt32)eDSAuthFailed );
#endif
				bFreePropList = true;
				niWhere = ::ni_proplist_match( niPropList, "uid", nil );

				if (niWhere == NI_INDEX_NULL) throw( (sInt32)eDSAuthFailed );
				siResult = eDSAuthFailed;
				niValue = niPropList.ni_proplist_val[ niWhere ].nip_val;

				// make sure the user we are going to verify actually is root OR an admin user
				if ( !( ( (niValue.ni_namelist_val[ 0 ] != NULL) && (strcmp( "0", niValue.ni_namelist_val[ 0 ]) == 0) ) || 
						(UserIsAdmin(rootName, inContext)) ) )
				{
					throw( (sInt32)eDSAuthFailed );
				}

				niWhere = ::ni_proplist_match( niPropList, "passwd", nil );
#ifdef DEBUG
				if (niWhere == NI_INDEX_NULL) throw( (sInt32)eDSAuthBadPassword );
				siResult = eDSAuthBadPassword;
#else
				if (niWhere == NI_INDEX_NULL) throw( (sInt32)eDSAuthFailed );
				siResult = eDSAuthFailed;
#endif
				niValue = niPropList.ni_proplist_val[ niWhere ].nip_val;
				// be careful not to dereference nil here.
				// We assume it is an empty password if there are no values for passwd.
				if ((niValue.ni_namelist_len > 0) && (niValue.ni_namelist_val != nil))
				{
					niPwd = niPropList.ni_proplist_val[ niWhere ].nip_val.ni_namelist_val[ 0 ];
				}
				else
				{
					niPwd = (char*)""; // empty string, we are not freeing it so direct assignment is OK
				}

				//account for the case where niPwd == "" such that we will auth if pwdLen is 0
				if (::strcmp(niPwd,"") != 0)
				{
					salt[ 0 ] = niPwd[0];
					salt[ 1 ] = niPwd[1];
					salt[ 2 ] = '\0';

					bzero(hashPwd, 32);
					::strcpy( hashPwd, ::crypt( rootPwd, salt ) );

					siResult = eDSAuthFailed;
					if ( ::strcmp( hashPwd, niPwd ) == 0 )
					{
						siResult = eDSNoErr;
					}
					bzero(hashPwd, 32);
				}
				else // niPwd is == ""
				{
					if (::strcmp(rootPwd,"") != 0)
					{
						siResult = eDSNoErr;
					}
				}

				if (siResult == eDSNoErr)
				{
					if ( inContext->bIsLocal ) //local node and assume crypt password length is correct
					{
						MigrateToShadowHash(aNIDomain, &niDirID, &niPropList, userName, userPwd, bResetCache);
					}
					else
					{
						// we successfully authenticated with root password, now set new user password.
						//set with the new password
						const char *saltchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
		
						bzero(hashPwd, 32);
		
						if ( ::strlen(userPwd) > 0 )
						{
							// only need crypt if password is not empty
							::srandom(getpid() + time(0));
							salt[0] = saltchars[random() % 64];
							salt[1] = saltchars[random() % 64];
							salt[2] = '\0';
		
							::strcpy( hashPwd, ::crypt( userPwd, salt ) );
						}
		
						siResult = IsValidRecordName ( userName, inNativeRecType, aNIDomain, niDirID );
		
#ifdef DEBUG
						if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
#else
						if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
#endif
						::ni_proplist_free( &niPropList );
						bFreePropList = false;
						siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
#ifdef DEBUG
						if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
#else
						if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
#endif
						bFreePropList = true;
						
						niWhere = ::ni_proplist_match( niPropList, "passwd", nil );
						if ( niWhere != NI_INDEX_NULL )
						{
							// password already exists, delete it
							niValue = niPropList.ni_proplist_val[ niWhere ].nip_val;
		
							siResult = NiLib2::DestroyDirVal( aNIDomain, &niDirID, (char*)"passwd", niValue );
						}
		
						siResult = NiLib2::InsertDirVal( aNIDomain, &niDirID, (char*)"passwd", hashPwd, 0 );
						siResult = MapNetInfoErrors( siResult );
						bzero(hashPwd, 32);
					}
				}
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( bFreePropList )
	{
		::ni_proplist_free( &niPropList );
	}

	gNetInfoMutex->Signal();
	
	if ( bResetCache )
	{
		// be sure not to hold the NetInfo mutex while calling this
		gSrvrCntl->HandleMemberDaemonFlushCache();
	}

	DSFreeString( userName );
	DSFreePassword( userPwd );
	DSFreeString( rootName );
	DSFreePassword( rootPwd );

	return( siResult );

} // DoSetPassword


//------------------------------------------------------------------------------------
//	* DoSetPasswordAsRoot
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoSetPasswordAsRoot ( sNIContextData *inContext, tDataBuffer *inAuthData, const char *inNativeRecType )
{
	sInt32				siResult		= eDSAuthFailed;
	bool				bFreePropList	= false;
	bool				bResetCache		= false;
	char			   *pData			= nil;
	char			   *userName		= nil;
	uInt32				userNameLen		= 0;
	char			   *newPasswd		= nil;
	uInt32				newPwdLen		= 0;
	uInt32				offset			= 0;
	uInt32				buffSize		= 0;
	uInt32				buffLen			= 0;
	ni_proplist			niPropList;
#ifdef TIM_CLIENT_PRESENT
	char			   *pTag			= nil;
	uInt32				uiCount			= 0;
	uInt32				i				= 0;
	sInt32				timResult		= eDSAuthFailed;
	bool				done			= false;
	TIMHandle		   *pTimHndl		= nil;
	Buffer			   *pUserBuff		= nil;
	Buffer			   *pBuffArray		= nil;
	Buffer			   *pTmpBuff		= nil;
#endif
  	void			   *aNIDomain		= nil;

	gNetInfoMutex->Wait();

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );

		pData = inAuthData->fBufferData;
		buffSize = inAuthData->fBufferSize;
		buffLen = inAuthData->fBufferLength;

   		if ( buffLen > buffSize ) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the length of the first data block and verify that it
		//	is greater than 0 and doesn't go past the end of the buffer
		//	(user name)

		if ( buffLen < 2 * sizeof( unsigned long ) + 1 ) throw( (sInt32)eDSInvalidBuffFormat );
		// need length for both username and password, plus username must be at least one character
		::memcpy( &userNameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (userNameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (userNameLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		userName = (char *)::calloc( 1, userNameLen + 1 );
		::memcpy( userName, pData, userNameLen );
		pData += userNameLen;
		offset += userNameLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the users new password
		::memcpy( &newPwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (newPwdLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		if (newPwdLen >= kHashRecoverableLength) throw ( (sInt32)eDSAuthPasswordTooLong );

		newPasswd = (char *)::calloc( 1, newPwdLen + 1 );
		::memcpy( newPasswd, pData, newPwdLen );
		pData += newPwdLen;
		offset += newPwdLen;

		// allow root to change anyone's password and
		// others to change their own password
		if ( inContext->fEffectiveUID != 0 )
		{
			if ((inContext->fAuthenticatedUserName == NULL)
				|| ((::strcmp(inContext->fAuthenticatedUserName, "root") != 0)
					&& (::strcmp(inContext->fAuthenticatedUserName, userName) != 0)))
			{
				throw( (sInt32)eDSPermissionError );
			}
		}

		aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
#ifdef TIM_CLIENT_PRESENT
			//opt out early if we know tim is not there **** check on local first
			if ( !inContext->bIsLocal && IsTimRunning() )
			{
				// Get a tim handle
				pTimHndl = ::timServerForDomain( aNIDomain );
				if ( pTimHndl != nil )
				{
					pUserBuff = ::bufferFromString( userName );
					if ( pUserBuff != nil )
					{
						// Get the user tags from the auth server
						timResult = ::timGetTagsForUser( pTimHndl, pUserBuff, &pBuffArray );
						if ( timResult == TimStatusOK )
						{
							uiCount = bufferArrayCount( pBuffArray );
							while ( (i < uiCount) && !done )
							{
								pTmpBuff = bufferArrayBufferAtIndex( pBuffArray, i++ );
								if ( pTmpBuff != nil )
								{
									pTag = bufferToString( pTmpBuff );
									if ( pTag != nil )
									{
										// Set the password for the user
										timResult = ::timSetPasswordForUserAsRoot( userName, newPasswd, pTag );
#ifdef DEBUG
										if ( timResult != TimStatusOK )
										{
											DBGLOG1( kLogPlugin, "-- timSetPasswordForUserAsRoot -- failed with %l.", timResult );
										}
#endif
										if ( timResult == TimStatusOK )
										{
											siResult = eDSNoErr;
											done = true;
										}

										free( pTag );
										pTag = nil;
									}
									::bufferRelease( pTmpBuff );
									pTmpBuff = nil;
								}
							}
						}
						if ( pBuffArray != nil ) {
							::bufferRelease( pBuffArray );
							pBuffArray = nil;
						}
						::bufferRelease( pUserBuff );
						pUserBuff = nil;
					}
					::timHandleFree( pTimHndl );
					pTimHndl = nil;
				} // pTimHndl != nil
			}
			else
#endif
			{
				//need some code here to directly set the NI Crypt password
				//also first check to see if the password already exists
				char			salt[3];
				char			hashPwd[ 32 ];
				ni_id			niDirID;
				ni_index   		niWhere			= 0;
				ni_namelist		niValue;

				//need some code here to directly change the NI Crypt password
				//first check to see that the old password is valid
				DBGLOG( kLogPlugin, "Attempting UNIX Crypt password change" );
				siResult = IsValidRecordName ( userName, inNativeRecType, aNIDomain, niDirID );

#ifdef DEBUG
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
#else
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
#endif

				siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
#ifdef DEBUG
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
#else
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
#endif
				bFreePropList = true;

				niWhere = ::ni_proplist_match( niPropList, "passwd", nil );

				if (siResult == eDSNoErr)
				{
					if ( inContext->bIsLocal ) //local node and assume crypt password length is correct
					{
						MigrateToShadowHash(aNIDomain, &niDirID, &niPropList, userName, newPasswd, bResetCache);
					}
					else
					{
						// we successfully authenticated with old password, now change to new password.
						//set with the new password
						const char *saltchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
		
						bzero(hashPwd, 32);
		
						if ( ::strlen(newPasswd) > 0 )
						{
							// only need crypt if password is not empty
							::srandom(getpid() + time(0));
							salt[0] = saltchars[random() % 64];
							salt[1] = saltchars[random() % 64];
							salt[2] = '\0';
		
							::strcpy( hashPwd, ::crypt( newPasswd, salt ) );
						}
		
						if ( niWhere != NI_INDEX_NULL )
						{
							niValue = niPropList.ni_proplist_val[ niWhere ].nip_val;
		
							siResult = NiLib2::DestroyDirVal( aNIDomain, &niDirID, (char*)"passwd", niValue );
						}
		
						siResult = NiLib2::InsertDirVal( aNIDomain, &niDirID, (char*)"passwd", hashPwd, 0 );
						siResult = MapNetInfoErrors( siResult );
						bzero(hashPwd, 32);
					}
				}
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( bFreePropList )
	{
		::ni_proplist_free( &niPropList );
	}

	gNetInfoMutex->Signal();

	if ( bResetCache )
	{
		// be sure not to hold the NetInfo mutex while calling this
		gSrvrCntl->HandleMemberDaemonFlushCache();
	}

	if ( userName != nil )
	{
		free( userName );
		userName = nil;
	}

	if ( newPasswd != nil )
	{
		bzero(newPasswd, strlen(newPasswd));
		free( newPasswd );
		newPasswd = nil;
	}

	return( siResult );

} // DoSetPasswordAsRoot


//------------------------------------------------------------------------------------
//	* DoChangePassword
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoChangePassword ( sNIContextData *inContext, tDataBuffer *inAuthData,
									 bool isSecondary, const char *inNativeRecType )
{
	sInt32			siResult		= eDSAuthFailed;
#ifdef TIM_CLIENT_PRESENT
	TIMHandle	   *timHandle		= nil;
#endif
	bool			bFreePropList	= false;
	bool			bResetCache		= false;
	char		   *pData			= nil;
	char		   *name			= nil;
	uInt32			nameLen			= 0;
	char		   *oldPwd			= nil;
	uInt32			OldPwdLen		= 0;
	char		   *newPwd			= nil;
	uInt32			newPwdLen		= 0;
	uInt32			offset			= 0;
	uInt32			buffSize		= 0;
	uInt32			buffLen			= 0;
	void		   *domain			= nil;
	ni_proplist		niPropList;

	gNetInfoMutex->Wait();

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );

		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );

		pData = inAuthData->fBufferData;
		buffSize = inAuthData->fBufferSize;
		buffLen = inAuthData->fBufferLength;

		if ( buffLen > buffSize ) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the length of the first data block and verify that it
		//	is greater than 0 and doesn't go past the end of the buffer

		if ( buffLen < 3 * sizeof( unsigned long ) + 1 ) throw( (sInt32)eDSInvalidBuffFormat );
		// we need at least 3 x 4 bytes for lengths of three strings,
		// and username must be at least 1 long
		::memcpy( &nameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (nameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (offset + nameLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		name = (char *)::calloc( 1, nameLen + 1 );
		::memcpy( name, pData, nameLen );
		pData += nameLen;
		offset += nameLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		::memcpy( &OldPwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + OldPwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
		if (OldPwdLen >= kHashRecoverableLength) throw ( (sInt32)eDSAuthPasswordTooLong );

		oldPwd = (char *)::calloc( 1, OldPwdLen + 1 );
		::memcpy( oldPwd, pData, OldPwdLen );
		pData += OldPwdLen;
		offset += OldPwdLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		::memcpy( &newPwdLen, pData, sizeof( unsigned long ) );
   		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + newPwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
		if (newPwdLen >= kHashRecoverableLength) throw ( (sInt32)eDSAuthPasswordTooLong );

		newPwd = (char *)::calloc( 1, newPwdLen + 1 );
		::memcpy( newPwd, pData, newPwdLen );
		pData += newPwdLen;
		offset += newPwdLen;

#ifdef TIM_CLIENT_PRESENT
		//opt out early if we know tim is not there **** check on local first
		if ( !inContext->bIsLocal && IsTimRunning() )
		{
			void* aNIDomain = RetrieveNIDomain(inContext);
			if (aNIDomain != NULL)
			{
				timHandle = ::timServerForDomain( aNIDomain );
				if ( timHandle == nil ) throw( (sInt32)eDSAuthFailed );

				// Set the password
				siResult = ::timSetPasswordWithTIMHandle( timHandle, name, oldPwd, name, newPwd );
	#ifdef DEBUG
				if ( siResult != TimStatusOK )
				{
					DBGLOG1( kLogPlugin, "-- timSetPassword -- failed with %l.", siResult );
				}
	#endif
				siResult = MapAuthResult( siResult );
				::timHandleFree( timHandle );
				timHandle = nil;
			}
			else
			{
				siResult = eDSInvalidSession;
			}
		}
		else
#endif
		{
			char			salt[3];
			char			hashPwd[ 32 ];
			ni_id			niDirID;
			ni_index		niWhere			= 0;
			ni_namelist		niValue;
			char		   *niPwd			= nil;
			ni_status		niStatus		= NI_OK;
			
			//first check to see that the old password is valid
			DBGLOG( kLogPlugin, "Attempting UNIX Crypt password change" );
			if ( inContext->fDomainPath == nil ) throw( (sInt32)eDSAuthFailed );
			
			niStatus = ::ni_open( nil, inContext->fDomainPath, &domain );
			if ( niStatus != 0) throw( (sInt32)eDSAuthFailed );
			DBGLOG1( kLogPlugin, "CNiPlugIn::DoChangePassword: <ni_open> opened on the netinfo node %s for changing password", inContext->fDomainPath );

			niStatus = ::ni_setuser( domain, name );
			niStatus = ::ni_setpassword( domain, oldPwd );

			siResult = IsValidRecordName ( name, inNativeRecType, domain, niDirID );

#ifdef DEBUG
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
#else
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
#endif

			siResult = ::ni_read( domain, &niDirID, &niPropList );
#ifdef DEBUG
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
			if ( niPropList.ni_proplist_val == nil ) throw( (sInt32)eDSAuthUnknownUser );
#else
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
			if ( niPropList.ni_proplist_val == nil ) throw( (sInt32)eDSAuthFailed );
#endif
			bFreePropList = true;

			niWhere = ::ni_proplist_match( niPropList, "passwd", nil );
#ifdef DEBUG
			if (niWhere == NI_INDEX_NULL) throw( (sInt32)eDSAuthBadPassword );
			siResult = eDSAuthBadPassword;
#else
			if (niWhere == NI_INDEX_NULL) throw( (sInt32)eDSAuthFailed );
			siResult = eDSAuthFailed;
#endif
			niValue = niPropList.ni_proplist_val[ niWhere ].nip_val;
			// be careful not to dereference nil here.
			// We assume it is an empty password if there are no values for passwd.
			if ((niValue.ni_namelist_len > 0) && (niValue.ni_namelist_val != nil))
			{
				niPwd = niPropList.ni_proplist_val[ niWhere ].nip_val.ni_namelist_val[ 0 ];
			}
			else
			{
				niPwd = (char*)""; // empty string, we are not freeing it so direct assignment is OK
			}

			siResult = NiLib2::ValidateDir( name, &niPropList );
			if ( siResult != NI_OK )
			{
				siResult = NiLib2::ValidateName( name, &niPropList, niWhere );
			}

			if ( siResult != eDSNoErr )
			{
				siResult = MapNetInfoErrors( siResult );
			}
			else if ( !isSecondary )
			{
				//if another auth authority has successfully changed the password
				//assume we have out of sync passwords and allow the change
				//account for the case where niPwd == "" such that we will auth if pwdLen is 0
				siResult = eDSAuthFailed;
				if (::strcmp(niPwd,"") != 0)
				{
					salt[ 0 ] = niPwd[0];
					salt[ 1 ] = niPwd[1];
					salt[ 2 ] = '\0';
	
					bzero(hashPwd, 32);
					::strcpy( hashPwd, ::crypt( oldPwd, salt ) );
	
					if ( ::strcmp( hashPwd, niPwd ) == 0 )
					{
						siResult = eDSNoErr;
					}
					bzero(hashPwd, 32);
				}
				else // niPwd is == ""
				{
					if (::strcmp(oldPwd,"") == 0)
					{
						siResult = eDSNoErr;
					}
				}
			}

			if (siResult == eDSNoErr)
			{
				if ( inContext->bIsLocal ) //local node and assume crypt password length is correct
				{
					MigrateToShadowHash(domain, &niDirID, &niPropList, name, newPwd, bResetCache);
				}
				else
				{
					// we successfully authenticated with old password, now change to new password.
					//set with the new password
					const char *saltchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
	
					bzero(hashPwd, 32);
	
					if ( ::strlen(newPwd) > 0 )
					{
						// only need crypt if password is not empty
						::srandom(getpid() + time(0));
						salt[0] = saltchars[random() % 64];
						salt[1] = saltchars[random() % 64];
						salt[2] = '\0';
	
						::strcpy( hashPwd, ::crypt( newPwd, salt ) );
					}
	
					siResult = NiLib2::DestroyDirVal( domain, &niDirID, (char*)"passwd", niValue );
	
					siResult = NiLib2::InsertDirVal( domain, &niDirID, (char*)"passwd", hashPwd, 0 );
					siResult = MapNetInfoErrors( siResult );
					bzero(hashPwd, 32);
				}
			}
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( bFreePropList )
	{
		ni_proplist_free( &niPropList );
	}

	if ( domain != nil )
	{
		ni_free( domain );
		domain = nil;
	}

	gNetInfoMutex->Signal();

	if ( bResetCache )
	{
		// be sure not to hold the NetInfo mutex while calling this
		gSrvrCntl->HandleMemberDaemonFlushCache();
	}

	if ( name != nil )
	{
		free( name );
		name = nil;
	}

	if ( newPwd != nil )
	{
		bzero(newPwd, strlen(newPwd));
		free( newPwd );
		newPwd = nil;
	}

	if ( oldPwd != nil )
	{
		bzero(oldPwd, strlen(oldPwd));
		free( oldPwd );
		oldPwd = nil;
	}

	return( siResult );

} // DoChangePassword


#pragma mark -
#pragma mark Shadow Hash
#pragma mark -

//------------------------------------------------------------------------------------
//	* DoShadowHashAuth
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoShadowHashAuth (	tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod, 
										sNIContextData* inContext, 
										sNIContinueData** inOutContinueData, 
										tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
										bool inAuthOnly, bool isSecondary,
										const char* inAuthAuthorityData, const char* inGUIDString,
										const char* inNativeRecType )
{
	sInt32				siResult								= eDSAuthFailed;
	uInt32				uiAuthMethod							= 0;
	char			   *pUserName								= nil;
	char			   *pNewPassword							= nil;
	char			   *pOldPassword							= nil;
	unsigned char	   *pNTLMDigest								= nil;
	unsigned long		ntlmDigestLen							= 0;
	unsigned char	   *pCramResponse							= NULL;
	unsigned long		cramResponseLen							= 0;
	char			   *pSambaName								= nil;
	char			   *pDomain									= nil;
	char			   *pAdminUser								= nil;
	char			   *pAdminPassword							= nil;
	unsigned char		P21[kHashShadowKeyLength]				= {0};
	unsigned char 		C8[kHashShadowChallengeLength] 			= {0};
	unsigned char 		C16[16]									= {0};
	unsigned char 		*PeerC16								= NULL;
	unsigned char		P24[kHashShadowResponseLength] 			= {0};
	unsigned char		P24Input[kHashShadowResponseLength]		= {0};
	unsigned char		GeneratedNTLM[EVP_MAX_MD_SIZE]			= {0};
	char				MSCHAP2Response[MS_AUTH_RESPONSE_LENGTH+1]	= {0};
	tDataListPtr		dataList								= NULL;
	char			   *path									= NULL;
	unsigned char		hashes[kHashTotalLength]				= {0};
	unsigned char		generatedHashes[kHashTotalLength]		= {0};
	uInt32				hashLength								= kHashTotalLength;
	sInt32				hashesLengthFromFile					= 0;
	tDataNodePtr		secureHashNode							= NULL;
	unsigned char 		secureHash[kHashSaltedSHA1Length] 		= {0};
	unsigned int		itemCount								= 0;
	char				*nativeAttrType							= NULL;
	char				*policyStr								= NULL;
	struct timespec		modDateOfPassword;
	struct timeval		modDateAssist;
	sHashState			state;
	char				*stateFilePath							= NULL;
	PWGlobalAccessFeatures globalAccess;
	unsigned long		policyStrLen							= 0;
	sHashAuthFailed    *pHashAuthFailed							= nil;
	time_t				now;
	digest_context_t	digestContext							= {0};
	bool				bFetchHashFiles							= false;
	int					keySize									= 0;
	unsigned int		userLevelHashList						= sHashList;
	char				*challenge								= NULL;
	char				*apopResponse							= NULL;
	
	try
	{
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
		if ( inContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		bzero( &state, sizeof(state) );
		
		DBGLOG( kLogPlugin, "DoShadowHashAuth::" );
		if (inAuthMethod != nil)
		{
			DBGLOG1( kLogPlugin, "NetInfo PlugIn: Attempting use of authentication method %s", inAuthMethod->fBufferData );
		}
		siResult = dsGetAuthMethodEnumValue( inAuthMethod, &uiAuthMethod );
		if ( siResult == eDSNoErr )
		{
			// If a user hash-list is specified, then it takes precedence over the global list
			if ( inAuthAuthorityData != nil && inAuthAuthorityData[0] != '\0' )
			{
				siResult = GetHashSecurityLevelForUser( inAuthAuthorityData, &userLevelHashList );
				if ( siResult != eDSNoErr )
				{
					DBGLOG1( kLogPlugin, "DoShadowHashAuth - encountered invalid record hash list: %s", inAuthAuthorityData );
					userLevelHashList = sHashList;
					siResult = eDSNoErr;
				}
			}
			
			// Parse input buffer, read hash(es)
			switch( uiAuthMethod )
			{
				case kAuthDIGEST_MD5:
					siResult = UnpackDigestBuffer( inAuthData, &pUserName, &digestContext );
					bFetchHashFiles = (siResult == eDSNoErr);
					break;
					
				case kAuthCRAM_MD5:
					if ( (userLevelHashList & kNiPluginHashCRAM_MD5) == 0 )
						throw( (sInt32)eDSAuthFailed );
					siResult = UnpackCramBuffer( inAuthData, &pUserName, &challenge, &pCramResponse, &cramResponseLen );
					bFetchHashFiles = (siResult == eDSNoErr);
					break;
					
				case kAuthAPOP:
					if ( (userLevelHashList & kNiPluginHashRecoverable) == 0 )
						throw( (sInt32)eDSAuthFailed );
					siResult = UnpackAPOPBuffer( inAuthData, &pUserName, &challenge, &apopResponse );
					bFetchHashFiles = (siResult == eDSNoErr);
					break;
					
				case kAuthSMB_NT_Key:
					if ( (userLevelHashList & kNiPluginHashNT) == 0 )
						throw( (sInt32)eDSAuthFailed );
					siResult = UnpackSambaBuffer( inAuthData, &pUserName, C8, P24Input );
					bFetchHashFiles = (siResult == eDSNoErr);
					break;
					
				case kAuthSMB_LM_Key:
					if ( (userLevelHashList & kNiPluginHashLM) == 0 )
						throw( (sInt32)eDSAuthFailed );
					siResult = UnpackSambaBuffer( inAuthData, &pUserName, C8, P24Input );
					bFetchHashFiles = (siResult == eDSNoErr);
					break;
					
				case kAuthNTLMv2:
					if ( (userLevelHashList & kNiPluginHashNT) == 0 )
						throw( (sInt32)eDSAuthFailed );
					siResult = UnpackNTLMv2Buffer( inAuthData, &pUserName, C8, &pNTLMDigest, &ntlmDigestLen, &pSambaName, &pDomain );
					bFetchHashFiles = (siResult == eDSNoErr);
					break;
				
				case kAuthMSCHAP2:
					if ( (userLevelHashList & kNiPluginHashNT) == 0 )
						throw( (sInt32)eDSAuthFailed );
					if ( outAuthData == NULL )
						throw( (sInt32)eDSNullAuthStepData );
					if ( outAuthData->fBufferSize < 4 + MS_AUTH_RESPONSE_LENGTH )
						throw( (sInt32)eDSBufferTooSmall );
					
					siResult = UnpackMSCHAPv2Buffer( inAuthData, &pUserName, C16, &PeerC16, &pNTLMDigest, &ntlmDigestLen, &pSambaName );
					bFetchHashFiles = (siResult == eDSNoErr);
					break;
				
				case kAuthVPN_PPTPMasterKeys:
					if ( outAuthData == NULL )
						throw( (sInt32)eDSNullAuthStepData );
					siResult = UnpackMPPEKeyBuffer( inAuthData, &pUserName, P24Input, &keySize );
					if ( outAuthData->fBufferSize < (unsigned long)(8 + keySize*2) )
						throw( (sInt32)eDSBufferTooSmall );
					bFetchHashFiles = (siResult == eDSNoErr);
					break;
				
				case kAuthSecureHash:
					if ( (userLevelHashList & kNiPluginHashSaltedSHA1) == 0 )
						throw( (sInt32)eDSAuthFailed );
					
					// parse input first
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( dsDataListGetNodeCountPriv(dataList) != 2 ) throw( (sInt32)eDSInvalidBuffFormat );
					
					// this allocates a copy of the string
					pUserName = dsDataListGetNodeStringPriv(dataList, 1);
					if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(pUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
					// these are not copies
					siResult = dsDataListGetNodePriv(dataList, 2, &secureHashNode);
					if ( secureHashNode == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( secureHashNode->fBufferLength != kHashSaltedSHA1Length ) throw( (sInt32)eDSInvalidBuffFormat);
					if ( siResult != eDSNoErr ) throw( (sInt32)eDSInvalidBuffFormat );
					memmove(secureHash, ((tDataBufferPriv*)secureHashNode)->fBufferData, secureHashNode->fBufferLength);
					
					//read file
					bFetchHashFiles = true;
					break;

				case kAuthWriteSecureHash:
					// parse input first
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( dsDataListGetNodeCountPriv(dataList) != 2 ) throw( (sInt32)eDSInvalidBuffFormat );
						
					// this allocates a copy of the string
					pUserName = dsDataListGetNodeStringPriv(dataList, 1);
					if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(pUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
					// these are not copies
					siResult = dsDataListGetNodePriv(dataList, 2, &secureHashNode);
					if ( secureHashNode == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( secureHashNode->fBufferLength != kHashSaltedSHA1Length ) throw( (sInt32)eDSInvalidBuffFormat);
					if ( siResult != eDSNoErr ) throw( (sInt32)eDSInvalidBuffFormat );
					memmove(secureHash, ((tDataBufferPriv*)secureHashNode)->fBufferData, secureHashNode->fBufferLength);
					break;
					
				case kAuthReadSecureHash:
					if ( outAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
					// parse input first
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( dsDataListGetNodeCountPriv(dataList) != 1 ) throw( (sInt32)eDSInvalidBuffFormat );
						
					// this allocates a copy of the string
					pUserName = dsDataListGetNodeStringPriv(dataList, 1);
					if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(pUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
					break;

				// set password operations
				case kAuthSetPasswd:
					// parse input first
					siResult = Get2FromBuffer( inAuthData, &dataList, &pUserName, &pNewPassword, &itemCount );
					if ( (pNewPassword != nil) && (strlen(pNewPassword) >= kHashRecoverableLength) ) throw ( (sInt32)eDSAuthPasswordTooLong );
					if ( siResult == eDSNoErr )
					{
						if ( itemCount != 4 )
							throw( (sInt32)eDSInvalidBuffFormat );					
						
						//read files
						bFetchHashFiles = true;
						siResult = GetUserPolicies( inContext, inNativeRecType, pUserName, NULL, &policyStr );
					}
					break;
				
				case kAuthSetPasswdAsRoot:
					// parse input first
					siResult = Get2FromBuffer(inAuthData, &dataList, &pUserName, &pNewPassword, &itemCount );
					if ( (pNewPassword != nil) && (strlen(pNewPassword) >= kHashRecoverableLength) ) throw ( (sInt32)eDSAuthPasswordTooLong );
					if ( siResult == eDSNoErr )
					{
						if ( itemCount != 2 && itemCount != 4)
							throw( (sInt32)eDSInvalidBuffFormat );	
					}
					break;
				
				case kAuthSetPolicyAsRoot:
					// parse input first, using <pNewPassword> to hold the policy string
					siResult = Get2FromBuffer(inAuthData, &dataList, &pUserName, &pNewPassword, &itemCount );
					if ( (pNewPassword != nil) && (strlen(pNewPassword) >= kHashRecoverableLength) ) throw ( (sInt32)eDSAuthPasswordTooLong );
					if ( siResult != eDSNoErr )
						throw( siResult );
					if ( itemCount != 2 || pNewPassword == NULL || pNewPassword[0] == '\0' )
						throw( (sInt32)eDSInvalidBuffFormat );
					siResult = ReadStateFile(pUserName, inGUIDString, &modDateOfPassword, &path, &stateFilePath, &state, &hashesLengthFromFile);
					break;
					
				case kAuthChangePasswd:
					// parse input first
					siResult = Get2FromBuffer(inAuthData, &dataList, &pUserName, &pOldPassword, &itemCount );
					if ( (pOldPassword != nil) && (strlen(pOldPassword) >= kHashRecoverableLength) ) throw ( (sInt32)eDSAuthPasswordTooLong );
					if ( siResult != eDSNoErr )
						throw( siResult );
					if ( itemCount != 3 )
						throw( (sInt32)eDSInvalidBuffFormat );
					
					// this allocates a copy of the string
					pNewPassword = dsDataListGetNodeStringPriv(dataList, 3);
					if ( pNewPassword == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( (pNewPassword != nil) && (strlen(pNewPassword) >= kHashRecoverableLength) ) throw ( (sInt32)eDSAuthPasswordTooLong );
					
					//read file
					bFetchHashFiles = true;
					if ( siResult == eDSNoErr )
						siResult = GetUserPolicies( inContext, inNativeRecType, pUserName, NULL, &policyStr );
					break;
				
				case kAuthSetShadowHashWindows:
				case kAuthSetShadowHashSecure:
				case kAuthNativeClearTextOK:
				case kAuthNativeNoClearText:
					siResult = Get2FromBuffer(inAuthData, &dataList, &pUserName, &pOldPassword, &itemCount );
					if ( siResult == eDSNoErr )
					{
						//read file
						bFetchHashFiles = true;
						// read user policies
						siResult = GetUserPolicies( inContext, inNativeRecType, pUserName, NULL, &policyStr );
					}
					break;
				
				case kAuthSetPasswdCheckAdmin:
					{
						char *pUserToChangeName = NULL;
						bool modifyingSelf;
						
						dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
						if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
						itemCount = dsDataListGetNodeCountPriv(dataList);
						if ( itemCount != 4 ) throw( (sInt32)eDSInvalidBuffFormat );
						
						// this allocates a copy of the string
						pUserName = dsDataListGetNodeStringPriv(dataList, 3);
						if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
						if ( strlen(pUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
						
						pOldPassword = dsDataListGetNodeStringPriv(dataList, 4);
						if ( pOldPassword == nil )
							throw( (sInt32)eDSInvalidBuffFormat );
						if ( strlen(pOldPassword) < 1 )
							throw( (sInt32)eDSInvalidBuffFormat );
						
						pUserToChangeName = dsDataListGetNodeStringPriv(dataList, 1);
						if ( pUserToChangeName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
						if ( strlen(pUserToChangeName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
						
						modifyingSelf = (pUserToChangeName != NULL) && (pUserName != NULL) && (strcmp(pUserToChangeName, pUserName) == 0);
						if ( !modifyingSelf && !UserIsAdmin(pUserName,inContext) )
							throw( (sInt32)eDSPermissionError );
						
						//read file
						bFetchHashFiles = true;
						// read user policies
						siResult = GetUserPolicies( inContext, inNativeRecType, pUserName, NULL, &policyStr );
					}
					break;
					
				case kAuthGetPolicy:
					// possible to return data
					if ( outAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
					outAuthData->fBufferLength = 0;
					
					// parse input
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					itemCount = dsDataListGetNodeCountPriv(dataList);
					if ( itemCount != 3 ) throw( (sInt32)eDSInvalidBuffFormat );
					
					// this allocates a copy of the string
					pUserName = dsDataListGetNodeStringPriv(dataList, 3);
					if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(pUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
					siResult = ReadStateFile(pUserName, inGUIDString, &modDateOfPassword, &path, &stateFilePath, &state, &hashesLengthFromFile);
					DSFreeString( stateFilePath );	// macro sets to NULL (required)
					break;
					
				case kAuthSetPolicy:
					// parse input
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					itemCount = dsDataListGetNodeCountPriv(dataList);
					if ( itemCount != 4 ) throw( (sInt32)eDSInvalidBuffFormat );
					
					// this allocates a copy of the string
					pUserName = dsDataListGetNodeStringPriv(dataList, 3);
					if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( pUserName[0] == '\0' ) throw( (sInt32)eDSInvalidBuffFormat );
					
					pNewPassword = dsDataListGetNodeStringPriv(dataList, 4);
					if ( pNewPassword == nil )
						throw( (sInt32)eDSInvalidBuffFormat );
					if ( pNewPassword[0] == '\0' )
						throw( (sInt32)eDSInvalidBuffFormat );
					siResult = ReadStateFile(pUserName, inGUIDString, &modDateOfPassword, &path, &stateFilePath, &state, &hashesLengthFromFile);
					break;
					
				case kAuthGetGlobalPolicy:
					// possible to return data
					if ( outAuthData == nil )
						throw( (sInt32)eDSNullAuthStepData );
					outAuthData->fBufferLength = 0;
					
					siResult = GetShadowHashGlobalPolicies( inContext, &globalAccess );
					if ( siResult != eDSNoErr )
						throw( siResult );
				
					{
						char policies[2048];
						
						PWGlobalAccessFeaturesToString( &globalAccess, policies );
						
						policyStrLen = strlen( policies );
						if ( outAuthData->fBufferSize < 4 + policyStrLen )
							throw( (sInt32)eDSBufferTooSmall );
						
						outAuthData->fBufferLength = 4 + policyStrLen;
						memcpy( outAuthData->fBufferData, &policyStrLen, 4 );
						memcpy( outAuthData->fBufferData + 4, policies, policyStrLen );
					}
					if ( outAuthData->fBufferLength == 0 )
						throw( (sInt32)eDSEmptyAttribute );
					break;
				
				case kAuthSetGlobalPolicy:
					GetShadowHashGlobalPolicies( inContext, &globalAccess );
					
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					
					// NOTE: using <pNewPassword> variable for the policy string
					pNewPassword = dsDataListGetNodeStringPriv(dataList, 3);
					if ( pNewPassword == nil )
						throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(pNewPassword) < 1 )
						throw( (sInt32)eDSInvalidBuffFormat );
					
					StringToPWGlobalAccessFeatures( pNewPassword, &globalAccess );
					siResult = SetShadowHashGlobalPolicies( inContext, &globalAccess );
					break;
					
				default:
					break;
			}
			
			if ( bFetchHashFiles )
			{
				siResult = ReadShadowHashAndStateFiles(pUserName, inGUIDString, hashes, &modDateOfPassword, &path, &stateFilePath, &state, &hashesLengthFromFile);
			}
			
			if ((siResult != eDSNoErr) && !(isSecondary && (uiAuthMethod == kAuthChangePasswd)))
			{
				throw( siResult );
			}
			
			// complete the operation
			switch( uiAuthMethod )
			{
				case kAuthGetGlobalPolicy:
				case kAuthSetGlobalPolicy:
					break;
				
				case kAuthSetPolicy:
				case kAuthSetPolicyAsRoot:
					siResult = SetUserPolicies( inContext, inNativeRecType, pUserName, pNewPassword, &state );
					break;
				
				case kAuthGetPolicy:
					// get policy attribute
					siResult = GetUserPolicies( inContext, inNativeRecType, pUserName, &state, &policyStr );
					if ( siResult != 0 ) throw( siResult );
					
					if ( policyStr != NULL )
					{
						policyStrLen = strlen( policyStr );
						if ( outAuthData->fBufferSize < 4 + policyStrLen )
							throw( (sInt32)eDSBufferTooSmall );
						
						outAuthData->fBufferLength = 4 + policyStrLen;
						memcpy( outAuthData->fBufferData, &policyStrLen, 4 );
						if ( policyStrLen > 0 )
							strcpy( outAuthData->fBufferData + 4, policyStr );
					}
					break;
				
				case kAuthDIGEST_MD5:
					{
						char *mutualDigest;
						unsigned int mutualDigestLen;
						unsigned long passwordLength;

						siResult = UnobfuscateRecoverablePassword( hashes + kHashOffsetToRecoverable, (unsigned char **)&pOldPassword, &passwordLength );
						if ( siResult == eDSNoErr )
						{
							siResult = digest_verify( &digestContext, pOldPassword, passwordLength, &mutualDigest, &mutualDigestLen );
							switch( siResult )
							{
								case SASL_OK:		siResult = eDSNoErr;		break;
								case SASL_NOMEM:	siResult = eMemoryError;	break;
								default:			siResult = eDSAuthFailed;
							}
						}
						if ( siResult == eDSNoErr )
						{
							// pack the return digest
							outAuthData->fBufferLength = 4 + digestContext.out_buf_len;
							memcpy( outAuthData->fBufferData, &digestContext.out_buf_len, 4 );
							if ( digestContext.out_buf_len > 0 )
								memcpy( outAuthData->fBufferData + 4, digestContext.out_buf, digestContext.out_buf_len );
						}
					}
					break;
				
				case kAuthCRAM_MD5:
					siResult = CRAM_MD5( hashes + kHashOffsetToCramMD5, challenge, pCramResponse );
					break;
				
				case kAuthAPOP:
					{
						unsigned long passwordLength;

						siResult = UnobfuscateRecoverablePassword( hashes + kHashOffsetToRecoverable, (unsigned char **)&pOldPassword, &passwordLength );
						if ( siResult == eDSNoErr )
							siResult = Verify_APOP( pUserName, (unsigned char *)pOldPassword, passwordLength, challenge, apopResponse );
					}
					break;
				
				case kAuthSMB_NT_Key:
					memmove(P21, hashes, kHashShadowOneLength);
					CalculateP24(P21, C8, P24);
					if (memcmp(P24,P24Input,kHashShadowResponseLength) == 0)
					{
						siResult = eDSNoErr;
					}
					else
					{
						siResult = eDSAuthFailed;
					}
					break;

				case kAuthSMB_LM_Key:
					memmove(P21, hashes + kHashOffsetToLM, kHashShadowOneLength);
					CalculateP24(P21, C8, P24);
					if (memcmp(P24,P24Input,kHashShadowResponseLength) == 0)
					{
						siResult = eDSNoErr;
					}
					else
					{
						siResult = eDSAuthFailed;
					}
					break;
				
				case kAuthNTLMv2:
					if ( NTLMv2(GeneratedNTLM, hashes, pSambaName, pDomain, C8, pNTLMDigest + kShadowHashNTLMv2Length, ntlmDigestLen - kShadowHashNTLMv2Length) == 0 )
					{
						if ( memcmp(GeneratedNTLM, pNTLMDigest, kShadowHashNTLMv2Length) == 0 )
						{
							siResult = eDSNoErr;
						}
						else
						{
							siResult = eDSAuthFailed;
						}
					}
					break;
				
				case kAuthMSCHAP2:
					siResult = MSCHAPv2( C16, PeerC16, pNTLMDigest, pSambaName, hashes, MSCHAP2Response );
					if ( siResult == eDSNoErr )
					{
						// put the response in the out buffer
						outAuthData->fBufferLength = 4 + MS_AUTH_RESPONSE_LENGTH;
						ntlmDigestLen = MS_AUTH_RESPONSE_LENGTH;
						memcpy( outAuthData->fBufferData, &ntlmDigestLen, 4 );
						memcpy( outAuthData->fBufferData + 4, MSCHAP2Response, MS_AUTH_RESPONSE_LENGTH );
					}
					break;
				
				case kAuthVPN_PPTPMasterKeys:
					if ( inContext->fEffectiveUID == 0 )
					{
						unsigned char sendKey[SHA_DIGEST_LENGTH];
						unsigned char receiveKey[SHA_DIGEST_LENGTH];
						
						CalculatePPTPSessionKeys( hashes, P24Input, keySize, sendKey, receiveKey );
						ntlmDigestLen = keySize;
						memcpy( outAuthData->fBufferData, &ntlmDigestLen, 4 );
						memcpy( outAuthData->fBufferData + 4, sendKey, ntlmDigestLen );
						memcpy( outAuthData->fBufferData + 4 + ntlmDigestLen, &ntlmDigestLen, 4 );
						memcpy( outAuthData->fBufferData + 4 + ntlmDigestLen + 4, receiveKey, ntlmDigestLen );
						outAuthData->fBufferLength = 8 + keySize*2;
					}
					else
					{
						siResult = eDSPermissionError;
					}
					break;
				
				case kAuthSecureHash:
					if ( secureHashNode->fBufferLength == kHashSaltedSHA1Length )
					{
						if (memcmp(secureHash, hashes + kHashOffsetToSaltedSHA1, kHashSaltedSHA1Length) == 0)
						{
							siResult = eDSNoErr;
						}
						else
						{
							siResult = eDSAuthFailed;
						}
					}
					else
					{
						siResult = eDSAuthFailed;
					}
					break;
				
				case kAuthWriteSecureHash:
					// allow root to directly overwrite the secure hash
					if ( inContext->fEffectiveUID != 0 )
					{
						throw( (sInt32)eDSPermissionError );
					}
					//write this secure hash with the other hashes as empty
					//we will reconstruct the other hashes upon first successful auth
					memmove(generatedHashes + kHashOffsetToSaltedSHA1, secureHash, kHashSaltedSHA1Length);
					siResult = WriteShadowHash(pUserName, inGUIDString, generatedHashes);
					break;

				case kAuthReadSecureHash:
					// allow root to directly read the secure hash
					if ( inContext->fEffectiveUID != 0 )
					{
						throw( (sInt32)eDSPermissionError );
					}
					//read file
					siResult = ReadShadowHashAndStateFiles(pUserName, inGUIDString, hashes, &modDateOfPassword, &path, &stateFilePath, &state, &hashesLengthFromFile);
					if (siResult == eDSNoErr)
					{
						if ( outAuthData->fBufferSize >= kHashSaltedSHA1Length )
						{
							memmove( outAuthData->fBufferData, hashes + kHashOffsetToSaltedSHA1, kHashSaltedSHA1Length );
							outAuthData->fBufferLength = kHashSaltedSHA1Length;
						}
						else
						{
							throw( (sInt32)eDSInvalidBuffFormat );
						}
					}
					break;

				case kAuthSetShadowHashWindows:
				case kAuthSetShadowHashSecure:
				case kAuthNativeClearTextOK:
				case kAuthNativeNoClearText:
				case kAuthSetPasswdCheckAdmin:
				{
					uInt32 pwdLen = 0;
					if (pOldPassword != nil)
					{
						pwdLen = strlen(pOldPassword);
					}
					else
					{
						throw((sInt32)eDSAuthFailed);
					}
					if ( pwdLen >= kHashRecoverableLength ) throw ( (sInt32)eDSAuthPasswordTooLong );
					if (hashesLengthFromFile == (kHashShadowBothLength + kHashSecureLength))
					{
						//legacy length so compare legacy hashes
						//will rewrite upgraded hashes below
						GenerateShadowHashes(	pOldPassword,
												pwdLen,
												kNiPluginHashSHA1 | kNiPluginHashNT | kNiPluginHashLM,
												hashes + kHashOffsetToSaltedSHA1,
												generatedHashes,
												&hashLength );
					}
					else
					{
						//generate proper hashes according to policy
						GenerateShadowHashes(	pOldPassword,
												pwdLen,
												userLevelHashList,
												hashes + kHashOffsetToSaltedSHA1,
												generatedHashes,
												&hashLength );
					}
					
					if ( NIHashesEqual( hashes, generatedHashes ) )
					{
						siResult = eDSNoErr;
						// update old hash file formats
						// 1. If the shadowhash file is short, save all the proper hashes.
						// 2. If the hash list is out-of-date, update.
						if ( hashesLengthFromFile < kHashTotalLength)
						{
							//generate proper hashes according to policy
							GenerateShadowHashes(	pOldPassword,
													pwdLen,
													userLevelHashList,
													hashes + kHashOffsetToSaltedSHA1,
													generatedHashes,
													&hashLength );
							// sync up the hashes
							siResult = WriteShadowHash( pUserName, inGUIDString, generatedHashes );
						}
						else if ( memcmp(hashes, generatedHashes, kHashTotalLength) != 0 )
						{
							// sync up the hashes
							siResult = WriteShadowHash( pUserName, inGUIDString, generatedHashes );
						}
						
						//see if we need to set the shadowhash tags
						if (uiAuthMethod == kAuthSetShadowHashWindows)
						{
							//set the auth authority
							CFMutableArrayRef myHashTypeArray = NULL;
							bool needToChange = false;
							long convertResult = pwsf_ShadowHashDataToArray( inAuthAuthorityData, &myHashTypeArray );
							
							if (myHashTypeArray == NULL)
							{
								myHashTypeArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
							}

							if (userLevelHashList & kNiPluginHashCRAM_MD5)
							{
								CFStringRef cram_md5_cfString = CFStringCreateWithCString( NULL, kNIHashNameCRAM_MD5, kCFStringEncodingUTF8 );
								CFIndex locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), cram_md5_cfString);
								if (locTag == kCFNotFound)
								{
									CFArrayAppendValue(myHashTypeArray, cram_md5_cfString);
									needToChange = true;
								}
								CFRelease(cram_md5_cfString);
							}
							if (userLevelHashList & kNiPluginHashRecoverable)
							{
								CFStringRef recoverable_cfString = CFStringCreateWithCString( NULL, kNIHashNameRecoverable, kCFStringEncodingUTF8 );
								CFIndex locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), recoverable_cfString);
								if (locTag == kCFNotFound)
								{
									CFArrayAppendValue(myHashTypeArray, recoverable_cfString);
									needToChange = true;
								}
								CFRelease(recoverable_cfString);
							}
							if (userLevelHashList & kNiPluginHashSecurityTeamFavorite)
							{
								CFStringRef team_fav_cfString = CFStringCreateWithCString( NULL, kNIHashNameSecure, kCFStringEncodingUTF8 );
								CFIndex locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), team_fav_cfString);
								if (locTag == kCFNotFound)
								{
									CFArrayAppendValue(myHashTypeArray, team_fav_cfString);
									needToChange = true;
								}
								CFRelease(team_fav_cfString);
							}
							CFStringRef salted_sha1_cfString = CFStringCreateWithCString( NULL, kNIHashNameSHA1, kCFStringEncodingUTF8 );
							CFIndex locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), salted_sha1_cfString);
							if (locTag == kCFNotFound)
							{
								CFArrayAppendValue(myHashTypeArray, salted_sha1_cfString);
								needToChange = true;
							}
							CFStringRef smb_nt_cfString = CFStringCreateWithCString( NULL, kNIHashNameNT, kCFStringEncodingUTF8 );
							locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), smb_nt_cfString);
							if (locTag == kCFNotFound)
							{
								CFArrayAppendValue(myHashTypeArray, smb_nt_cfString);
								needToChange = true;
							}
							CFStringRef smb_lm_cfString = CFStringCreateWithCString( NULL, kNIHashNameLM, kCFStringEncodingUTF8 );;
							locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), smb_lm_cfString);
							if (locTag == kCFNotFound)
							{
								CFArrayAppendValue(myHashTypeArray, smb_lm_cfString);
								needToChange = true;
							}
							if (needToChange)
							{
								char *newAuthAuthority = nil;
								newAuthAuthority = pwsf_ShadowHashArrayToData( myHashTypeArray, &convertResult );
								if (newAuthAuthority != nil)
								{
									char * fullAuthAuthority = (char *) calloc(1, 1 + strlen(kDSValueAuthAuthorityShadowHash) + strlen(newAuthAuthority));
									strcpy(fullAuthAuthority, kDSValueAuthAuthorityShadowHash);
									strcat(fullAuthAuthority, newAuthAuthority);
									siResult = SetUserAuthAuthorityAsRoot(inContext, "users", pUserName, fullAuthAuthority);
									
									//get the hashlist anew since we have written a new auth authority
									// If a user hash-list is specified, then it takes precedence over the global list
									if ( siResult == eDSNoErr )
									{
										siResult = GetHashSecurityLevelForUser( newAuthAuthority, &userLevelHashList );
										if ( siResult != eDSNoErr )
										{
											DBGLOG1( kLogPlugin, "DoShadowHashAuth - encountered invalid record hash list: %s", fullAuthAuthority );
											userLevelHashList = sHashList;
											siResult = eDSNoErr;
										}
										
										GenerateShadowHashes(	pOldPassword,
																pwdLen,
																userLevelHashList,
																hashes + kHashOffsetToSaltedSHA1,
																generatedHashes,
																&hashLength );
										// sync up the hashes
										siResult = WriteShadowHash( pUserName, inGUIDString, generatedHashes );
									}
			
									DSFreeString(newAuthAuthority);
									DSFreeString(fullAuthAuthority);
								}
							}
							CFRelease(myHashTypeArray);
							CFRelease(smb_nt_cfString);
							CFRelease(smb_lm_cfString);
							CFRelease(salted_sha1_cfString);

						}
						else if (uiAuthMethod == kAuthSetShadowHashSecure)
						{
							//set the auth authority
							CFMutableArrayRef myHashTypeArray = NULL;
							bool needToChange = false;
							long convertResult = pwsf_ShadowHashDataToArray( inAuthAuthorityData, &myHashTypeArray );

							if (myHashTypeArray == NULL)
							{
								myHashTypeArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
							}

							if (userLevelHashList & kNiPluginHashCRAM_MD5)
							{
								CFStringRef cram_md5_cfString = CFStringCreateWithCString( NULL, kNIHashNameCRAM_MD5, kCFStringEncodingUTF8 );
								CFIndex locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), cram_md5_cfString);
								if (locTag == kCFNotFound)
								{
									CFArrayAppendValue(myHashTypeArray, cram_md5_cfString);
									needToChange = true;
								}
								CFRelease(cram_md5_cfString);
							}
							if (userLevelHashList & kNiPluginHashRecoverable)
							{
								CFStringRef recoverable_cfString = CFStringCreateWithCString( NULL, kNIHashNameRecoverable, kCFStringEncodingUTF8 );
								CFIndex locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), recoverable_cfString);
								if (locTag == kCFNotFound)
								{
									CFArrayAppendValue(myHashTypeArray, recoverable_cfString);
									needToChange = true;
								}
								CFRelease(recoverable_cfString);
							}
							if (userLevelHashList & kNiPluginHashSecurityTeamFavorite)
							{
								CFStringRef team_fav_cfString = CFStringCreateWithCString( NULL, kNIHashNameSecure, kCFStringEncodingUTF8 );
								CFIndex locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), team_fav_cfString);
								if (locTag == kCFNotFound)
								{
									CFArrayAppendValue(myHashTypeArray, team_fav_cfString);
									needToChange = true;
								}
								CFRelease(team_fav_cfString);
							}
							CFStringRef salted_sha1_cfString = CFStringCreateWithCString( NULL, kNIHashNameSHA1, kCFStringEncodingUTF8 );
							CFIndex locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), salted_sha1_cfString);
							if (locTag == kCFNotFound)
							{
								CFArrayAppendValue(myHashTypeArray, salted_sha1_cfString);
								needToChange = true;
							}
							CFStringRef smb_nt_cfString = CFStringCreateWithCString( NULL, kNIHashNameNT, kCFStringEncodingUTF8 );
							locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), smb_nt_cfString);
							if (locTag != kCFNotFound)
							{
								CFArrayRemoveValueAtIndex(myHashTypeArray, locTag);
								needToChange = true;
							}
							CFStringRef smb_lm_cfString = CFStringCreateWithCString( NULL, kNIHashNameLM, kCFStringEncodingUTF8 );;
							locTag = CFArrayGetFirstIndexOfValue(myHashTypeArray, CFRangeMake(0,CFArrayGetCount(myHashTypeArray)), smb_lm_cfString);
							if (locTag != kCFNotFound)
							{
								CFArrayRemoveValueAtIndex(myHashTypeArray, locTag);
								needToChange = true;
							}
							if (needToChange)
							{
								char *newAuthAuthority = nil;
								newAuthAuthority = pwsf_ShadowHashArrayToData( myHashTypeArray, &convertResult );
								if (newAuthAuthority != nil)
								{
									char * fullAuthAuthority = (char *) calloc(1, 1 + strlen(kDSValueAuthAuthorityShadowHash) + strlen(newAuthAuthority));
									strcpy(fullAuthAuthority, kDSValueAuthAuthorityShadowHash);
									strcat(fullAuthAuthority, newAuthAuthority);
									siResult = SetUserAuthAuthorityAsRoot(inContext, "users", pUserName, fullAuthAuthority);
									
									//get the hashlist anew since we have written a new auth authority
									// If a user hash-list is specified, then it takes precedence over the global list
									if ( siResult == eDSNoErr )
									{
										siResult = GetHashSecurityLevelForUser( newAuthAuthority, &userLevelHashList );
										if ( siResult != eDSNoErr )
										{
											DBGLOG1( kLogPlugin, "DoShadowHashAuth - encountered invalid record hash list: %s", fullAuthAuthority );
											userLevelHashList = sHashList;
											siResult = eDSNoErr;
										}
										
										GenerateShadowHashes(	pOldPassword,
																pwdLen,
																userLevelHashList,
																hashes + kHashOffsetToSaltedSHA1,
																generatedHashes,
																&hashLength );
										// sync up the hashes
										siResult = WriteShadowHash( pUserName, inGUIDString, generatedHashes );
									}
			
									DSFreeString(newAuthAuthority);
									DSFreeString(fullAuthAuthority);
								}
							}
							CFRelease(myHashTypeArray);
							CFRelease(smb_nt_cfString);
							CFRelease(smb_lm_cfString);
							CFRelease(salted_sha1_cfString);
						}
						else
						{
							if (inAuthOnly == false)
							{
								siResult = AuthOpen( inContext, pUserName, pOldPassword );
							}
						}
					}
					else
					{
						siResult = eDSAuthFailed;
					}
				}
					break;

				// set password operations
				case kAuthSetPasswd:
					GetShadowHashGlobalPolicies( inContext, &globalAccess );
					siResult = GetUserNameFromAuthBuffer( inAuthData, 3, &pAdminUser );
					if ( siResult != eDSNoErr )
						throw( siResult );
					
					if ( !UserIsAdmin(pAdminUser, inContext) )
						{
							siResult = NIPasswordOkForPolicies( policyStr, &globalAccess, pUserName, pNewPassword );
							if ( siResult == eDSAuthPasswordTooShort &&
								 globalAccess.minChars == 0 &&
								 pNewPassword != NULL &&
								 *pNewPassword == '\0' &&
								 ((policyStr == NULL) || strstr(policyStr, "minChars=0") != NULL) )
							{
								// special-case for ShadowHash and blank password.
								siResult = eDSNoErr;
							}
						}
					if ( siResult == eDSNoErr )
					{
						bzero(generatedHashes, kHashTotalLength);
						GenerateShadowHashes(pNewPassword, strlen(pNewPassword), userLevelHashList, NULL, generatedHashes, &hashLength );
						
						siResult = WriteShadowHash(pUserName, inGUIDString, generatedHashes);
						if ( siResult == eDSNoErr )
						{
							state.newPasswordRequired = 0;
							time( &now );
							gmtime_r( &now, &state.modDateOfPassword );
							gettimeofday( &modDateAssist, NULL );
							TIMEVAL_TO_TIMESPEC( &modDateAssist, &modDateOfPassword );
						}
					}
					break;
				
				case kAuthSetPasswdAsRoot:
					{
						bool modifyingSelf = inContext->fAuthenticatedUserName && pUserName && (::strcmp(inContext->fAuthenticatedUserName, pUserName) == 0);
						bool notAdmin = ( (inContext->fAuthenticatedUserName != NULL) && (! UserIsAdmin(inContext->fAuthenticatedUserName, inContext)) );
						
						// allow root to change anyone's password and
						// others to change their own password
						if ( inContext->fEffectiveUID != 0 )
						{
							if ( (inContext->fAuthenticatedUserName == NULL) ||
								(notAdmin && (!modifyingSelf)) )
							{
								throw( (sInt32)eDSPermissionError );
							}
						}
						
						GetShadowHashGlobalPolicies( inContext, &globalAccess );
						siResult = eDSNoErr;
						if ( modifyingSelf && notAdmin )
						{
							siResult = NIPasswordOkForPolicies( policyStr, &globalAccess, pUserName, pNewPassword );
							if ( siResult == eDSAuthPasswordTooShort &&
								 globalAccess.minChars == 0 &&
								 pNewPassword != NULL &&
								 *pNewPassword == '\0' &&
								 ((policyStr == NULL) || strstr(policyStr, "minChars=0") != NULL) )
							{
								// special-case for ShadowHash and blank password.
								siResult = eDSNoErr;
							}
						}
						if ( siResult == eDSNoErr )
						{
							GenerateShadowHashes(pNewPassword, strlen(pNewPassword), userLevelHashList, NULL, generatedHashes, &hashLength );
					
							siResult = WriteShadowHash(pUserName, inGUIDString, generatedHashes);
							if ( siResult == eDSNoErr )
							{
								state.newPasswordRequired = 0;
								time( &now );
								gmtime_r( &now, &state.modDateOfPassword );
								gettimeofday( &modDateAssist, NULL );
								TIMEVAL_TO_TIMESPEC( &modDateAssist, &modDateOfPassword );
							}
						}
					}
					break;

				case kAuthChangePasswd:
					GenerateShadowHashes( pOldPassword,
										  strlen(pOldPassword),
										  userLevelHashList,
										  hashes + kHashOffsetToSaltedSHA1,
										  generatedHashes,
										  &hashLength );
					
					if ( NIHashesEqual( hashes, generatedHashes ) || isSecondary )
					{
						bool notAdmin = ( (pUserName != NULL) && (! UserIsAdmin(pUserName, inContext)) );
						
						GetShadowHashGlobalPolicies( inContext, &globalAccess );
						if ( notAdmin )
						{
							siResult = NIPasswordOkForPolicies( policyStr, &globalAccess, pUserName, pNewPassword );
							if ( siResult == eDSAuthPasswordTooShort &&
								 globalAccess.minChars == 0 &&
								 pNewPassword != NULL &&
								 *pNewPassword == '\0' &&
								 ((policyStr == NULL) || strstr(policyStr, "minChars=0") != NULL) )
							{
								// special-case for ShadowHash and blank password.
								siResult = eDSNoErr;
							}
						}
						if ( siResult == eDSNoErr )
						{
							bzero(generatedHashes, kHashTotalLength);
							GenerateShadowHashes(pNewPassword, strlen(pNewPassword), userLevelHashList, NULL, generatedHashes, &hashLength );
							
							siResult = WriteShadowHash(pUserName, inGUIDString, generatedHashes);
							state.newPasswordRequired = 0;
							
							// password changed
							time( &now );
							gmtime_r( &now, &state.modDateOfPassword );
							gettimeofday( &modDateAssist, NULL );
							TIMEVAL_TO_TIMESPEC( &modDateAssist, &modDateOfPassword );
						}
					}
					else
					{
						siResult = eDSAuthFailed;
					}
					break;

				default:
					siResult = eDSAuthMethodNotSupported;
					break;
			}

			// do not enforce login-time policies for administrators
			if ( (pUserName != NULL) && (! UserIsAdmin(pUserName, inContext)) )
			{
				// test policies and update the current user's state
				if ( siResult == eDSNoErr &&
					uiAuthMethod != kAuthGetPolicy && uiAuthMethod != kAuthGetGlobalPolicy &&
					uiAuthMethod != kAuthSetPolicy && uiAuthMethod != kAuthSetGlobalPolicy )
				{
					GetShadowHashGlobalPolicies( inContext, &globalAccess );
					siResult = NITestPolicies( policyStr, &globalAccess, &state, &modDateOfPassword, path );
					if ( siResult == eDSNoErr )
					{
						state.failedLoginAttempts = 0;
					}
					else
					if ( state.disabled == 1 )
					{
						SetUserAAtoDisabled( inContext, inNativeRecType, pUserName );
					}
				}
				else
				if ( siResult == eDSAuthFailed )
				{
					state.failedLoginAttempts++;
					
					GetShadowHashGlobalPolicies( inContext, &globalAccess );
					NITestPolicies( policyStr, &globalAccess, &state, &modDateOfPassword, path );
					if ( state.disabled == 1 )
					{
						SetUserAAtoDisabled( inContext, inNativeRecType, pUserName );
					}
				}
			}
			if ( siResult == eDSNoErr || siResult == eDSAuthNewPasswordRequired )
			{
				time( &now );
				gmtime_r( &now, &(state.lastLoginDate) );
			}
			
			// check for failure in the operation and delay response based on heuristic of number of attempts
			switch( uiAuthMethod )
			{
				case kAuthSMB_NT_Key:
				case kAuthSMB_LM_Key:
				case kAuthSecureHash:
				case kAuthNativeClearTextOK:
				case kAuthNativeNoClearText:
				case kAuthSetPasswd:
				case kAuthSetPasswdAsRoot:
				case kAuthChangePasswd:
					if ( (siResult == eDSAuthFailed) && (gDelayFailedLocalAuthReturnsDeltaInSeconds != 0) )
					{
						//save time of last failure
						//save the current failure time
						//save the pUserName and total count of failures
						//if > 5th failure AND (current failure time - last failure time) > 10*(# failures -5) then remove entry
						//if (> 5th failure) then delay return
						//delay return time = (# failures - 5) seconds
						gHashAuthFailedMapLock->Wait();
						string aUserName(pUserName);
						HashAuthFailedMapI  aHashAuthFailedMapI	= gHashAuthFailedMap.find(aUserName);
						if (aHashAuthFailedMapI == gHashAuthFailedMap.end())
						{
							//create an entry in the map
							pHashAuthFailed = (sHashAuthFailed *)calloc(1, sizeof(sHashAuthFailed));
							pHashAuthFailed->nowTime = dsTimestamp();
							pHashAuthFailed->lastTime = pHashAuthFailed->nowTime;
							pHashAuthFailed->failCount = 1;
							gHashAuthFailedMap[aUserName] = pHashAuthFailed;
							gHashAuthFailedMapLock->Signal();
						}
						else
						{
							//use the current entry
							pHashAuthFailed = aHashAuthFailedMapI->second;
							pHashAuthFailed->lastTime = pHashAuthFailed->nowTime;
							pHashAuthFailed->nowTime = dsTimestamp();
							pHashAuthFailed->failCount++;
							if ( pHashAuthFailed->failCount > 5 )
							{
								if (pHashAuthFailed->failCount == 6)
								{
									syslog(LOG_ALERT,"Failed Authentication return is being delayed due to over five recent auth failures for username: %s.", pUserName);
								}
								if ( (pHashAuthFailed->nowTime - pHashAuthFailed->lastTime)/USEC_PER_SEC > 10*gDelayFailedLocalAuthReturnsDeltaInSeconds*(pHashAuthFailed->failCount - 5) )
								{
									//it has been a long time since the last failure so let's remove this and not track first new failure
									gHashAuthFailedMap.erase(aUserName);
									free(pHashAuthFailed);
									pHashAuthFailed = nil;
									gHashAuthFailedMapLock->Signal();
								}
								else
								{
									//let's delay the return for the failed auth since too many failures have occurred too rapidly
									struct mach_timebase_info timeBaseInfo;
									mach_timebase_info( &timeBaseInfo );
									uint64_t delay = gDelayFailedLocalAuthReturnsDeltaInSeconds * (pHashAuthFailed->failCount - 5) * (((uint64_t)NSEC_PER_SEC * (uint64_t)timeBaseInfo.denom) / (uint64_t)timeBaseInfo.numer);

									gHashAuthFailedMapLock->Signal(); //don't hold the lock when delaying
									mach_wait_until( mach_absolute_time() + delay );
								}
							}
							else
							{
								gHashAuthFailedMapLock->Signal();
							}
						}
					}
					break;
				default:
					break;
			}
		}
	}
	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( stateFilePath != NULL )
	{
		WriteHashStateFile( stateFilePath, &state );
		DSFreeString( stateFilePath );
	}
	
	DSFreeString( path );
	DSFreeString( policyStr );
	
	if ( nativeAttrType != NULL )
	{
		delete nativeAttrType;
		nativeAttrType = NULL;
	}
	if (dataList != NULL)
	{
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
	}
	
	DSFreeString( pUserName );

	if ( pNewPassword != nil )
	{
		bzero(pNewPassword, strlen(pNewPassword));
		free( pNewPassword );
		pNewPassword = nil;
	}
	
	if ( pOldPassword != nil )
	{
		bzero(pOldPassword, strlen(pOldPassword));
		free( pOldPassword );
		pOldPassword = nil;
	}
	
	DSFree( pNTLMDigest );
	DSFreeString( pSambaName );
	DSFreeString( pDomain );
	DSFreeString( pAdminUser );
		
	if ( pAdminPassword != nil )
	{
		bzero(pAdminPassword, strlen(pAdminPassword));
		free( pAdminPassword );
		pAdminPassword = nil;
	}
	
	digest_dispose( &digestContext );
	
	DSFreeString( challenge );
	DSFreeString( apopResponse );
	DSFree( pCramResponse );
	
	//zero out all the hashes used above
	bzero(P21, kHashShadowKeyLength);
	bzero(C8, kHashShadowChallengeLength);
	bzero(P24, kHashShadowResponseLength);
	bzero(P24Input, kHashShadowResponseLength);
	bzero(hashes, kHashTotalLength);
	bzero(generatedHashes, kHashTotalLength);
	bzero(secureHash, kHashSecureLength);

	return( siResult );

} // DoShadowHashAuth


//--------------------------------------------------------------------------------------------------
// * GetUserPolicies
//
//  Returns: ds err
//
//  Note: can return eDSNoErr and *outPolicyStr == NULL; an empty attribute is not an error.
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::GetUserPolicies( sNIContextData *inContext, const char *inNativeRecType, const char *inUsername, sHashState *inState, char **outPolicyStr )
{
	sInt32				error					= eDSNoErr;
	ni_status			niResult				= NI_OK;
	char				*nativeAttrType			= NULL;
	char				*internalPolicyStr		= NULL;
    ni_id				niDirID;
	ni_namelist			niValues;
	long				length					= 0;
	
	try
	{
		// init vars
		if ( outPolicyStr == NULL )
			return eParameterError;
		*outPolicyStr = NULL;
		
		gNetInfoMutex->Wait();
		void *aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			// read user policies
			error = IsValidRecordName( inUsername, inNativeRecType, aNIDomain, niDirID );
			if ( error != eDSNoErr )
			{
				gNetInfoMutex->Signal();
				throw( (sInt32)eDSAuthUnknownUser );
			}
			
			nativeAttrType = MapAttrToNetInfoType( kDS1AttrPasswordPolicyOptions );
			if ( nativeAttrType == NULL )
			{
				gNetInfoMutex->Signal();
				throw( (sInt32)eDSInvalidAttributeType );
			}
			
			// lookup kDS1AttrPasswordPolicyOptions attribute
			niResult = ni_lookupprop( aNIDomain, &niDirID, nativeAttrType, &niValues );
		}
		else
		{
			gNetInfoMutex->Signal();
			throw( (sInt32)eDSInvalidSession );
		}
		gNetInfoMutex->Signal();
		
		if ( niResult == NI_OK && niValues.ni_namelist_len > 0 )
		{
			ConvertXMLPolicyToSpaceDelimited( niValues.ni_namelist_val[0], &internalPolicyStr );
			
			gNetInfoMutex->Wait();
			ni_namelist_free( &niValues );
			gNetInfoMutex->Signal();
		}
		
		// prefix state information if requested
		if ( inState != NULL )
		{
			if ( internalPolicyStr != NULL )
				length = strlen( internalPolicyStr );
			*outPolicyStr = (char *) malloc( sizeof(kPWPolicyStr_newPasswordRequired) + 3 + length );
			if ( (*outPolicyStr) != NULL )
			{
				strcpy( (*outPolicyStr), kPWPolicyStr_newPasswordRequired );
				strcpy( (*outPolicyStr) + sizeof(kPWPolicyStr_newPasswordRequired) - 1, inState->newPasswordRequired ? "=1" : "=0" );
				if ( internalPolicyStr != NULL && length > 0 )
				{
					*((*outPolicyStr) + sizeof(kPWPolicyStr_newPasswordRequired) + 1) = ' ';
					strcpy( (*outPolicyStr) + sizeof(kPWPolicyStr_newPasswordRequired) + 2, internalPolicyStr );
					free( internalPolicyStr );
				}
			}
		}
		else
		{
			*outPolicyStr = internalPolicyStr;
		}
	}
	catch( sInt32 catchErr )
	{
		error = catchErr;
	}
	
	if ( nativeAttrType != NULL )
		delete nativeAttrType;

	return error;
}


//--------------------------------------------------------------------------------------------------
// * SetUserPolicies
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::SetUserPolicies(
	sNIContextData *inContext,
	const char *inNativeRecType,
	const char *inUsername,
	const char *inPolicyStr,
	sHashState *inOutHashState )
{
	sInt32				siResult			= eDSAuthFailed;
	ni_id				niDirID;
	ni_index			niWhere				= 0;
	ni_namelist			niValue;
	ni_proplist			niPropList;
	bool				bFreePropList		= false;
	char				*nativeAttrType		= NULL;
	char				*currentPolicyStr   = NULL;
	char				*newPassRequiredStr	= NULL;
	PWAccessFeatures	access;
	
	NI_INIT( &niValue );

	gNetInfoMutex->Wait();

	try
	{
		if ( inContext == nil || inPolicyStr == nil || inUsername == nil )
			throw( (sInt32)eDSAuthFailed );
		
		if ( (inContext->fEffectiveUID != 0) && (!UserIsAdmin(inContext->fAuthenticatedUserName, inContext)) )
			throw( (sInt32)eDSPermissionError );
		
		// special case for newPasswordRequired because it is stored in the state file
		if ( inOutHashState != NULL && (newPassRequiredStr = strstr(inPolicyStr, kPWPolicyStr_newPasswordRequired)) != NULL )
		{
			newPassRequiredStr += sizeof(kPWPolicyStr_newPasswordRequired) - 1;
			if ( (*newPassRequiredStr == '=') &&
				 (*(newPassRequiredStr + 1) == '0' || *(newPassRequiredStr + 1) == '1') )
			{
				inOutHashState->newPasswordRequired = *(newPassRequiredStr + 1) - '0';
			}
		}
		
		// policies that go in the user record
		GetUserPolicies( inContext, inNativeRecType, inUsername, NULL, &currentPolicyStr );
		
		void *aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			niDirID = inContext->dirID;
			nativeAttrType = MapAttrToNetInfoType( kDS1AttrPasswordPolicyOptions );
			if ( nativeAttrType == NULL )
				throw( (sInt32)eDSInvalidAttributeType );
		
			siResult = IsValidRecordName( inUsername, inNativeRecType, aNIDomain, niDirID );
			if ( siResult != eDSNoErr )
				throw( (sInt32)eDSAuthFailed );
			
			siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
			if ( niPropList.ni_proplist_val == nil ) throw( (sInt32)eDSAuthFailed );
			bFreePropList = true;
			
			niWhere = ::ni_proplist_match( niPropList, nativeAttrType, nil );
			if (niWhere != NI_INDEX_NULL)
			{
				niValue = niPropList.ni_proplist_val[niWhere].nip_val;
				
				if ( ( inContext->fEffectiveUID != 0 )
				 && ( (inContext->fAuthenticatedUserName == NULL) 
					  || (strcmp(inContext->fAuthenticatedUserName,"root") != 0) ) )
				{
					siResult = NiLib2::ValidateDir( inContext->fAuthenticatedUserName, &niPropList );
					if ( siResult != NI_OK )
						siResult = NiLib2::ValidateName( "root", &niPropList, niWhere );
				}
			}
			
			if ( siResult != eDSNoErr )
				siResult = MapNetInfoErrors( siResult );
			
			if ( siResult == eDSNoErr )
			{
				char *xmlDataStr;
				char policyStr[2048];
				
				GetDefaultUserPolicies( &access );
				if ( currentPolicyStr != NULL )
					StringToPWAccessFeatures( currentPolicyStr, &access );
				StringToPWAccessFeatures( inPolicyStr, &access );
				PWAccessFeaturesToStringWithoutStateInfo( &access, policyStr );
				
				if ( ConvertSpaceDelimitedPolicyToXML( policyStr, &xmlDataStr ) == 0 )
				{		
					siResult = NiLib2::DestroyDirVal( aNIDomain, &niDirID, (char*)nativeAttrType, niValue );
					siResult = NiLib2::InsertDirVal( aNIDomain, &niDirID, (char*)nativeAttrType, xmlDataStr, 0 );
					siResult = MapNetInfoErrors( siResult );
					free( xmlDataStr );
				}
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();
	
	if ( bFreePropList )
		::ni_proplist_free( &niPropList );
	
	if ( nativeAttrType != NULL )
		delete nativeAttrType;
	if ( currentPolicyStr != NULL )
		free( currentPolicyStr );
	
	return( siResult );

} // SetUserPolicies


//--------------------------------------------------------------------------------------------------
// * SetUserAAtoDisabled
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::SetUserAAtoDisabled( sNIContextData *inContext, const char *inNativeRecType, const char *inUsername )
{
	sInt32				siResult			= eDSAuthFailed;
	ni_id				niDirID;
	ni_index			niWhere				= 0;
	ni_namelist			niValue;
	ni_proplist			niPropList;
	bool				bFreePropList		= false;
	char				*nativeAttrType		= NULL;
	char				*currentPolicyStr   = NULL;
	char				*curAAString		= NULL;
	char				*aaString			= NULL;
	
	NI_INIT( &niValue );

	gNetInfoMutex->Wait();
	
	try
	{
		if ( inContext == nil )
			throw( (sInt32)eDSAuthFailed );
		
		void *aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			niDirID = inContext->dirID;
			nativeAttrType = MapAttrToNetInfoType( kDSNAttrAuthenticationAuthority );
			if ( nativeAttrType == NULL )
				throw( (sInt32)eDSInvalidAttributeType );
			
			siResult = IsValidRecordName( inUsername, inNativeRecType, aNIDomain, niDirID );
			if ( siResult != eDSNoErr )
				throw( (sInt32)eDSAuthFailed );
			
			siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
			if ( niPropList.ni_proplist_val == nil ) throw( (sInt32)eDSAuthFailed );
			bFreePropList = true;
			
			niWhere = ::ni_proplist_match( niPropList, nativeAttrType, nil );
			if (niWhere != NI_INDEX_NULL)
			{
				niValue = niPropList.ni_proplist_val[niWhere].nip_val;
				
				if ((niValue.ni_namelist_len > 0) && (niValue.ni_namelist_val != nil))
					curAAString = niValue.ni_namelist_val[0];
				
				siResult = NiLib2::ValidateDir( inContext->fAuthenticatedUserName, &niPropList );
				if ( siResult != NI_OK )
					siResult = NiLib2::ValidateName( "root", &niPropList, niWhere );
			}
			
			if ( siResult != eDSNoErr )
				siResult = MapNetInfoErrors( siResult );
			
			if ( siResult == eDSNoErr )
			{
				siResult = NiLib2::DestroyDirVal( aNIDomain, &niDirID, (char*)nativeAttrType, niValue );
				if ( curAAString != NULL )
				{
					aaString = (char *) malloc( strlen(curAAString) + sizeof(kDSValueAuthAuthorityDisabledUser) );
					strcpy( aaString, kDSValueAuthAuthorityDisabledUser );
					strcat( aaString, curAAString );
				}
				else
				{
					aaString = strdup( kDSValueAuthAuthorityDisabledUser";ShadowHash;" );
				}
				
				siResult = NiLib2::InsertDirVal( aNIDomain, &niDirID, (char*)nativeAttrType, aaString, 0 );
				siResult = MapNetInfoErrors( siResult );
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}
	
	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();
	
	if ( bFreePropList )
		::ni_proplist_free( &niPropList );
	
	DSFreeString( aaString );
	
	if ( nativeAttrType != NULL )
		delete nativeAttrType;
	if ( currentPolicyStr != NULL )
		free( currentPolicyStr );
	
	return( siResult );

} // SetUserAAtoDisabled


//--------------------------------------------------------------------------------------------------
// * SetUserAuthAuthorityAsRoot
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::SetUserAuthAuthorityAsRoot( sNIContextData *inContext, const char *inNativeRecType, const char *inUsername, const char *inAuthAuthority )
{
	sInt32				siResult			= eDSAuthFailed;
	ni_id				niDirID;
	ni_index			niWhere				= 0;
	ni_namelist			niValue;
	ni_proplist			niPropList;
	char				*nativeAttrType		= NULL;
	
	NI_INIT( &niValue );
	
	gNetInfoMutex->Wait();

	try
	{
		if ( ( inContext == nil ) || (inAuthAuthority == nil) )
			throw( (sInt32)eDSAuthFailed );
		
		void *aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			niDirID = inContext->dirID;
			nativeAttrType = MapAttrToNetInfoType( kDSNAttrAuthenticationAuthority );
			if ( nativeAttrType == NULL )
				throw( (sInt32)eDSInvalidAttributeType );
			
			siResult = IsValidRecordName( inUsername, inNativeRecType, aNIDomain, niDirID );
			if ( siResult != eDSNoErr )
				throw( (sInt32)eDSAuthFailed );
			
			siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
			if ( niPropList.ni_proplist_val == nil ) throw( (sInt32)eDSAuthFailed );
			
			if (siResult == eDSNoErr)
			{
				niWhere = ::ni_proplist_match( niPropList, nativeAttrType, nil );
				if (niWhere != NI_INDEX_NULL)
				{
					niValue = niPropList.ni_proplist_val[niWhere].nip_val;
				}
			}
			else
			{
				siResult = MapNetInfoErrors( siResult );
			}
			
			if ( siResult == eDSNoErr )
			{
				siResult = NiLib2::DestroyDirVal( aNIDomain, &niDirID, (char*)nativeAttrType, niValue );
				siResult = NiLib2::InsertDirVal( aNIDomain, &niDirID, (char*)nativeAttrType, (char *)inAuthAuthority, 0 );
				siResult = MapNetInfoErrors( siResult );
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}
	
	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();
	
	if ( nativeAttrType != NULL )
		delete nativeAttrType;
	
	return( siResult );

} // SetUserAuthAuthorityAsRoot


//--------------------------------------------------------------------------------------------------
// * ReadShadowHash ()
//
//  <outUserHashPath> can be NULL, if non-null and a value is returned, caller must free.
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::ReadShadowHash (
	const char *inUserName,
	const char *inGUIDString,
	unsigned char outHashes[kHashTotalLength],
	struct timespec *outModTime,
	char **outUserHashPath,
	sInt32 *outHashDataLen,
	bool readHashes )
{
	sInt32	siResult						= eDSAuthFailed;
	char   *path							= NULL;
	char	hexHashes[kHashTotalHexLength + 1]	= { 0 };
	sInt32	readBytes						= 0;
	uInt32	outBytes						= 0;
	CFile  *hashFile						= nil;
	uInt32  pathSize						= 0;
	
	try
	{
		if ( outModTime != NULL )
		{
			outModTime->tv_sec = 0;
			outModTime->tv_nsec = 0;
		}
		if ( outUserHashPath != NULL )
			*outUserHashPath = NULL;
		if ( outHashDataLen != NULL )
			*outHashDataLen = 0;
		
		if (inGUIDString != nil)
		{
			pathSize = sizeof(kShadowHashDirPath) + strlen(inGUIDString) + 1;
		}
		else
		{
			pathSize = sizeof(kShadowHashDirPath) + strlen(inUserName) + 1;
		}
		
		path = (char*)::calloc(pathSize, 1);
		if ( path != NULL )
		{
			if (inGUIDString != nil)
			{
				strcpy( path, kShadowHashDirPath );
				strcat( path, inGUIDString );
			}
			else
			{
				strcpy( path, kShadowHashDirPath );
				strcat( path, inUserName );
			}
			
			// CFile throws, so let's catch, otherwise our logic won't work, could use stat,
			// but for consistency using try/catch
			try {
				hashFile = new CFile(path, false);
			} catch ( ... ) {
				
			}
			
			if (hashFile != nil && hashFile->is_open())
			{
				if ( outModTime != NULL )
					hashFile->ModDate( outModTime );
				
				if ( readHashes )
				{
					bzero( hexHashes, sizeof(hexHashes) );
					readBytes = hashFile->ReadBlock( hexHashes, kHashTotalHexLength );
					delete(hashFile);
					hashFile = nil;
					
					// should check the right number of bytes is there
					if ( readBytes < kHashShadowBothHexLength ) throw( (sInt32)eDSAuthFailed );
					HexToBinaryConversion( hexHashes, &outBytes, outHashes );
					if ( readBytes == (kHashTotalLength - 16)*2 ) {
						memmove( outHashes + kHashOffsetToSaltedSHA1, outHashes + kHashOffsetToSaltedSHA1 - 16, readBytes - kHashOffsetToSaltedSHA1 );
						bzero( outHashes + kHashOffsetToCramMD5, kHashCramLength );
					}
				}
				siResult = eDSNoErr;
			}
			else //support older hash files
			{
				if (hashFile != nil)
				{
					delete(hashFile);
					hashFile = nil;
				}
				free( path );
				path = NULL;
				path = (char*)::calloc(sizeof(kShadowHashOldDirPath) + strlen(inUserName) + 1, 1);
				if ( path != NULL )
				{
					sprintf(path, "%s%s", kShadowHashOldDirPath, inUserName);
					
					// CFile throws so we must catch...
					try
					{
						hashFile = new CFile(path, false);
						
						if (hashFile->is_open())
						{
							if ( outModTime != NULL )
								hashFile->ModDate( outModTime );
							
							if ( readHashes )
							{
								//old hash file format has only kHashShadowBothHexLength bytes
								readBytes = hashFile->ReadBlock( hexHashes, kHashShadowBothHexLength );
								delete(hashFile);
								hashFile = nil;
								// should check the right number of bytes is there
								if ( readBytes != kHashShadowBothHexLength ) throw( (sInt32)eDSAuthFailed );
								HexToBinaryConversion( hexHashes, &outBytes, outHashes );
							}
							siResult = eDSNoErr;
						}
					}
					catch( ... )
					{
					}
				}
			}
		}
	}
	catch( ... )
    {
        siResult = eDSAuthFailed;
    }
	
	if ( path != NULL )
	{
		if ( outUserHashPath != NULL )
		{
			*outUserHashPath = path;
		}
		else
		{
			free( path );
			path = NULL;
		}
	}
	if (hashFile != nil)
	{
		delete(hashFile);
		hashFile = nil;
	}
	
	bzero(hexHashes, kHashTotalHexLength);
	
	if ( outHashDataLen != NULL )
		*outHashDataLen = outBytes;
	
	return( siResult );
} // ReadShadowHash


//--------------------------------------------------------------------------------------------------
// * WriteShadowHash ()
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::WriteShadowHash ( const char *inUserName, const char *inGUIDString, unsigned char inHashes[kHashTotalLength] )
{
	sInt32		result			= eDSAuthFailed;
	char	   *path			= NULL;
	char		hexHashes[kHashTotalHexLength]	= { 0 };
	sInt32		siResult		= eDSNoErr;
	struct stat	statResult;
	CFile	  *hashFile			= nil;
	
	try
	{
		//check to remove any old hash file
		RemoveShadowHash( inUserName, nil, false );
		if (inGUIDString != nil)
		{
			path = (char*)::calloc(1, strlen(kShadowHashDirPath) + strlen(inGUIDString) + 1);
		}
		else
		{
			path = (char*)::calloc(strlen(kShadowHashDirPath) + strlen(inUserName) + 1, 1);
		}
		if ( path != NULL )
		{
			if (inGUIDString != nil)
			{
				sprintf(path, "%s%s", kShadowHashDirPath, inGUIDString);
			}
			else
			{
				sprintf(path, "%s%s", kShadowHashDirPath, inUserName);
			}
			
			siResult = stat( "/var/db/shadow/hash", &statResult );
			if (siResult != eDSNoErr)
			{
				siResult = ::stat( "/var/db/shadow", &statResult );
				//if first sub directory does not exist
				if (siResult != eDSNoErr)
				{
					::mkdir( "/var/db/shadow", 0700 );
					::chmod( "/var/db/shadow", 0700 );
				}
				siResult = ::stat( "/var/db/shadow/hash", &statResult );
				//if second sub directory does not exist
				if (siResult != eDSNoErr)
				{
					::mkdir( "/var/db/shadow/hash", 0700 );
					::chmod( "/var/db/shadow/hash", 0700 );
				}
			}
			
			// CFile throws, but it is okay here
			hashFile = new CFile(path, true);
	
			if (hashFile->is_open())
			{
				BinaryToHexConversion( inHashes, kHashTotalLength, hexHashes );
				hashFile->seekp( 0 ); // start at beginning
				hashFile->write( hexHashes, kHashTotalHexLength );
				chmod( path, 0600 ); //set root as rw only
				delete(hashFile);
				hashFile = nil;
				result = eDSNoErr;
			}
		}
	}
	catch( ... )
    {
        result = eDSAuthFailed;
    }
	
	if ( path != NULL ) {
		free( path );
		path = NULL;
	}
	if (hashFile != nil)
	{
		delete(hashFile);
		hashFile = nil;
	}

	bzero(hexHashes, kHashTotalHexLength);
	
	return( result );
} // WriteShadowHash


//--------------------------------------------------------------------------------------------------
// * RemoveShadowHash ()
//--------------------------------------------------------------------------------------------------

void CNiPlugIn::RemoveShadowHash ( const char *inUserName, const char *inGUIDString, bool bShadowToo )
{
	char   *path							= NULL;
	char	hexHashes[kHashTotalHexLength]	= { 0 };
	bool	bRemovePath						= false;
	CFile  *hashFile						= nil;

	try
	{
		if (bShadowToo) //if flag set remove shadow file
		{
			//accept possibility that orignal file creation used only username but now we have the GUID
			//ie. don't want to cleanup in this case since we might cleanup a file that doesn't actually
			//belong to this record
			if (inGUIDString != nil)
			{
				path = (char*)::calloc(1, strlen(kShadowHashDirPath) + strlen(inGUIDString) + 1);
			}
			else
			{
				path = (char*)::calloc(strlen(kShadowHashDirPath) + strlen(inUserName) + 1, 1);
			}
			if ( path != NULL )
			{
				if (inGUIDString != nil)
				{
					sprintf(path, "%s%s", kShadowHashDirPath, inGUIDString);
				}
				else
				{
					sprintf(path, "%s%s", kShadowHashDirPath, inUserName);
				}
				
				// CFile throws, so we need to catch here so we can continue
				try
				{
					hashFile = new CFile(path, false); //destructor calls close
		
					if (hashFile->is_open())
					{
						hashFile->seekp( 0 ); // start at beginning
						//overwrite with zeros
						hashFile->write( hexHashes, kHashTotalHexLength );
						delete(hashFile);
						hashFile = nil;
						bRemovePath = true;
					}
					if (bRemovePath)
					{
						//remove the file
						unlink(path);
					}
				}
				catch( ... )
				{
					
				}
				free( path );
				path = NULL;
			}
		}
		
		if (hashFile != nil)
		{
			delete(hashFile);
			hashFile = nil;
		}
		bRemovePath = false;
		//check always to remove the older file if present
		if (inUserName != nil)
		{
			path = (char*)::calloc(sizeof(kShadowHashOldDirPath) + strlen(inUserName) + 1, 1);
		}
		if ( path != NULL )
		{
			sprintf(path, "%s%s", kShadowHashOldDirPath, inUserName);
			
			// this throws, but is okay because we expect the throw
			hashFile = new CFile(path, false); //destructor calls close

			if (hashFile->is_open())
			{
				hashFile->seekp( 0 ); // start at beginning
				//overwrite with zeros
				hashFile->write( hexHashes, kHashShadowBothHexLength );
				delete(hashFile);
				hashFile = nil;
				bRemovePath = true;
			}
			if (bRemovePath)
			{
				//remove the file
				unlink(path);
			}
			free( path );
			path = NULL;
		}
	}
	catch( ... )
    {
    }
	
	if ( path != NULL ) {
		free( path );
		path = NULL;
	}
	if (hashFile != nil)
	{
		delete(hashFile);
		hashFile = nil;
	}

	bzero(hexHashes, kHashTotalHexLength);
	
	return;
} // RemoveShadowHash


//--------------------------------------------------------------------------------------------------
// * ReadShadowHashAndStateFiles
//
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::ReadShadowHashAndStateFiles(
	const char *inUserName,
	const char *inGUIDString,
	unsigned char outHashes[kHashTotalLength],
	struct timespec *outModTime,
	char **outUserHashPath,
	char **outStateFilePath,
	sHashState *inOutHashState,
	sInt32 *outHashDataLen )
{
	if ( outStateFilePath == NULL || outUserHashPath == NULL )
		return eParameterError;
	
	*outStateFilePath = NULL;
	
	sInt32 siResult = ReadShadowHash( inUserName, inGUIDString, outHashes, outModTime, outUserHashPath, outHashDataLen );
	if ( siResult == eDSNoErr )
		siResult = NIGetStateFilePath( *outUserHashPath, outStateFilePath );
	if ( siResult == eDSNoErr && inOutHashState != NULL )
	{
		siResult = ReadHashStateFile( *outStateFilePath, inOutHashState );
		if (siResult != eDSNoErr)
		{
			//We have a state file path but nothing is there right now.
			//At the end of the shadow hash auth it will be correctly written
			//so don't fail this call.
			siResult = eDSNoErr;
		}
	}
	
	return siResult;
} // ReadShadowHashAndStateFiles


//--------------------------------------------------------------------------------------------------
// * ReadStateFile
//
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::ReadStateFile(
	const char *inUserName,
	const char *inGUIDString,
	struct timespec *outModTime,
	char **outUserHashPath,
	char **outStateFilePath,
	sHashState *inOutHashState,
	sInt32 *outHashDataLen )
{
	unsigned char hashes[kHashTotalLength];
	
	if ( outStateFilePath == NULL || outUserHashPath == NULL )
		return eParameterError;
	
	*outStateFilePath = NULL;
	
	sInt32 siResult = ReadShadowHash( inUserName, inGUIDString, hashes, outModTime, outUserHashPath, outHashDataLen, false );
	if ( siResult == eDSNoErr )
		siResult = NIGetStateFilePath( *outUserHashPath, outStateFilePath );
	if ( siResult == eDSNoErr && inOutHashState != NULL )
	{
		siResult = ReadHashStateFile( *outStateFilePath, inOutHashState );
		if (siResult != eDSNoErr)
		{
			//We have a state file path but nothing is there right now.
			//At the end of the shadow hash auth it will be correctly written
			//so don't fail this call.
			siResult = eDSNoErr;
		}
	}
	
	return siResult;
} // ReadStateFile


//------------------------------------------------------------------------------------
//	* MigrateToShadowHash ()
//
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::MigrateToShadowHash( void *inDomain, ni_id *inNIDirID, ni_proplist *inNIPropList, const char *inUserName, const char *inPassword, bool &outResetCache )
{
	sInt32			siResult							= eDSAuthFailed;
	unsigned char   generatedHashes[kHashTotalLength]   = {0};
	ni_namelist		niAttribute;
	ni_namelist		niValues;
	ni_index		niWhere								= NI_INDEX_NULL;
	unsigned long   hashTotalLength						= 0;
	
	//find the GUID
	niWhere = ::ni_proplist_match( *inNIPropList, "generateduid", nil );
	char* GUIDString = nil; //no need to free this below
	if (niWhere != NI_INDEX_NULL)
	{
		if ((inNIPropList->ni_proplist_val[ niWhere ].nip_val.ni_namelist_len > 0)
			&& (inNIPropList->ni_proplist_val[ niWhere ].nip_val.ni_namelist_val != nil))
		{
			GUIDString = inNIPropList->ni_proplist_val[ niWhere ].nip_val.ni_namelist_val[ 0 ];
		}
	}
	if (GUIDString == nil)
	{
		//no pre-existing GUID so we make one here
		CFUUIDRef       myUUID;
		CFStringRef     myUUIDString;
		char            genUIDValue[100];

		memset( genUIDValue, 0, 100 );
		myUUID = CFUUIDCreate(kCFAllocatorDefault);
		myUUIDString = CFUUIDCreateString(kCFAllocatorDefault, myUUID);
		CFStringGetCString(myUUIDString, genUIDValue, 100, kCFStringEncodingASCII);
		CFRelease(myUUID);
		CFRelease(myUUIDString);
		GUIDString = strdup(genUIDValue);
		//write the GUID value to the user record
		NI_INIT( &niValues );
		::ni_namelist_insert( &niValues, GUIDString, NI_INDEX_NULL );
		siResult = DoAddAttribute( inDomain, inNIDirID, "generateduid", niValues );
		outResetCache = true;
		::ni_namelist_free( &niValues );
	}

	GenerateShadowHashes(inPassword, strlen(inPassword), sHashList, NULL, generatedHashes, &hashTotalLength );
	
	siResult = WriteShadowHash(inUserName, GUIDString, generatedHashes);
	if (siResult == eDSNoErr)
	{
		NI_INIT( &niAttribute );
		::ni_namelist_insert( &niAttribute, "authentication_authority", NI_INDEX_NULL );
		siResult = NiLib2::DestroyDirProp( inDomain, inNIDirID, niAttribute );
		siResult = MapNetInfoErrors( siResult );
		::ni_namelist_free( &niAttribute );
		
		NI_INIT( &niAttribute );
		::ni_namelist_insert( &niAttribute, "passwd", NI_INDEX_NULL );
		siResult = NiLib2::DestroyDirProp( inDomain, inNIDirID, niAttribute );
		siResult = MapNetInfoErrors( siResult );
		::ni_namelist_free( &niAttribute );

		NI_INIT( &niValues );
		::ni_namelist_insert( &niValues, kDSValueAuthAuthorityShadowHash, NI_INDEX_NULL );
		siResult = DoAddAttribute( inDomain, inNIDirID, "authentication_authority", niValues );
		::ni_namelist_free( &niValues );

		NI_INIT( &niValues );
		::ni_namelist_insert( &niValues, kDSValueNonCryptPasswordMarker, NI_INDEX_NULL );
		siResult = DoAddAttribute( inDomain, inNIDirID, "passwd", niValues );
		::ni_namelist_free( &niValues );
	}
	return(siResult);
}


#pragma mark -
#pragma mark Password Server
#pragma mark -

//--------------------------------------------------------------------------------------------------
// * PWSetReplicaData ()
//
//	Note:	inAuthorityData is the UserID + RSA_key, but the IP address should be pre-stripped by
//			the calling function.
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::PWSetReplicaData( sNIContextData *inContext, const char *inAuthorityData )
{
	sInt32				error					= eDSNoErr;
	ni_status			niResult				= NI_OK;
	bool				bFoundWithHash			= false;
	long				replicaListLen			= 0;
	char				*rsaKeyPtr				= NULL;
	char				*nativeRecType			= NULL;
	char				*nativeAttrType			= NULL;
	tDataBufferPtr		replicaBuffer			= NULL;
    tDataBufferPtr		replyBuffer				= NULL;
    ni_id				niDirID;
	ni_namelist			niValues;
	char				recordName[64];
	char				hashStr[34];
	
	nativeRecType = MapRecToNetInfoType( kDSStdRecordTypeConfig );
	if ( nativeRecType == NULL )
		return eDSInvalidRecordType;
	
	try
	{
		nativeAttrType = MapAttrToNetInfoType( kDS1AttrPasswordServerList );
		if ( nativeAttrType == NULL )
			throw( (sInt32)eDSInvalidAttributeType );
		
		gNetInfoMutex->Wait();
		void *aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			// get /config/passwordserver_HEXHASH
			rsaKeyPtr = strchr( inAuthorityData, ',' );
			if ( rsaKeyPtr != NULL )
			{
				MD5_CTX ctx;
				unsigned char pubKeyHash[MD5_DIGEST_LENGTH];
				
				MD5_Init( &ctx );
				rsaKeyPtr++;
				MD5_Update( &ctx, rsaKeyPtr, strlen(rsaKeyPtr) );
				MD5_Final( pubKeyHash, &ctx );
				
				BinaryToHexConversion( pubKeyHash, MD5_DIGEST_LENGTH, hashStr );
				sprintf( recordName, "passwordserver_%s", hashStr );
				
				error = IsValidRecordName( recordName, nativeRecType, aNIDomain, niDirID );
				if ( error == eDSNoErr )
					bFoundWithHash = true;
			}
			
			if ( ! bFoundWithHash )
			{
				error = IsValidRecordName( "passwordserver", nativeRecType, aNIDomain, niDirID );
				if ( error != eDSNoErr ) 
					throw( error );
			}
			
			//lookup authauthority attribute
			niResult = ni_lookupprop( aNIDomain, &niDirID, nativeAttrType, &niValues );
		}
		else
		{
			gNetInfoMutex->Signal();
			throw( (sInt32)eDSInvalidSession );
		}
		gNetInfoMutex->Signal();
		if ( niResult != NI_OK ) throw( (sInt32)eDSAuthFailed );
		
		if ( niValues.ni_namelist_len >= 1 )
		{
			replicaListLen = strlen( niValues.ni_namelist_val[0] );
			replicaBuffer = ::dsDataBufferAllocatePriv( replicaListLen + 1 );
			if ( replicaBuffer == nil ) throw( (sInt32)eMemoryError );
			
			replyBuffer = ::dsDataBufferAllocatePriv( 1 );
			if ( replyBuffer == nil ) throw( (sInt32)eMemoryError );
			
			replicaBuffer->fBufferLength = replicaListLen;
			memcpy( replicaBuffer->fBufferData, niValues.ni_namelist_val[0], replicaListLen );
			
			error = dsDoPlugInCustomCall( inContext->fPWSNodeRef, 1, replicaBuffer, replyBuffer );
			
			::dsDataBufferDeallocatePriv( replicaBuffer );
			::dsDataBufferDeallocatePriv( replyBuffer );
		}
		
		if ( nativeAttrType != NULL )
			delete nativeAttrType;
		
		gNetInfoMutex->Wait();
		if (niValues.ni_namelist_len > 0)
		{
			ni_namelist_free( &niValues );
		}
		gNetInfoMutex->Signal();
	}
	catch( sInt32 catchErr )
	{
		error = catchErr;
	}
	
	if ( nativeRecType != NULL )
		delete nativeRecType;
	
	return error;
}


//------------------------------------------------------------------------------------
//	* DoPasswordServerAuth
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoPasswordServerAuth ( tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod, 
										 sNIContextData* inContext, 
										 sNIContinueData** inOutContinueData, 
										 tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
										 bool inAuthOnly, bool isSecondary, const char* inAuthAuthorityData,
										 const char* inGUIDString, const char* inNativeRecType )
{
    sInt32 result = eDSAuthFailed;
    sInt32 error;
    uInt32 authMethod;
    char *serverAddr;
    char *uidStr = NULL;
    long uidStrLen;
    tDataBufferPtr authDataBuff = NULL;
    tDataBufferPtr authDataBuffTemp = NULL;
    char *nodeName = NULL;
    char *userName = NULL;
	char *password = NULL;
    sNIContinueData *pContinue = NULL;
    tContextData continueData = NULL;
    
    if ( !inAuthAuthorityData || *inAuthAuthorityData == '\0' )
        return eDSAuthParameterError;
    
    try
    {
        //DBGLOG1( kLogPlugin, "AuthorityData=%s\n", inAuthAuthorityData);
        
        serverAddr = strchr( inAuthAuthorityData, ':' );
        if ( serverAddr )
        {
            uidStrLen = serverAddr - inAuthAuthorityData;
            uidStr = (char *) calloc(1, uidStrLen+1);
            if ( uidStr == nil ) throw( (sInt32)eMemoryError );
            strncpy( uidStr, inAuthAuthorityData, uidStrLen );
            
            // advance past the colon
            serverAddr++;
            
			// try any method to the password server, even if unknown
			DBGLOG( kLogPlugin, "DoPasswordServerAuth::" );
			if (inAuthMethod != nil)
			{
				DBGLOG1( kLogPlugin, "NetInfo PlugIn: Attempting use of authentication method %s", inAuthMethod->fBufferData );
			}
			error = dsGetAuthMethodEnumValue( inAuthMethod, &authMethod );
			if ( error != eDSNoErr && error != eDSAuthMethodNotSupported )
				throw( error );
			
            switch( authMethod )
            {
                case kAuth2WayRandom:
                    if ( inOutContinueData == nil )
                        throw( (sInt32)eDSNullParameter );
                    
                    if ( *inOutContinueData == nil )
                    {
                        pContinue = (sNIContinueData *)::calloc( 1, sizeof( sNIContinueData ) );
                        if ( pContinue == nil )
                            throw( (sInt32)eMemoryError );
                        
                        gNIContinue->AddItem( pContinue, inNodeRef );
                        *inOutContinueData = pContinue;
                        
                        // make a buffer for the user ID
                        authDataBuff = ::dsDataBufferAllocatePriv( uidStrLen + 1 );
                        if ( authDataBuff == nil ) throw ( (sInt32)eMemoryError );
                        
                        // fill
                        strcpy( authDataBuff->fBufferData, uidStr );
                        authDataBuff->fBufferLength = uidStrLen;
                    }
                    else
                    {
                        pContinue = *inOutContinueData;
                        if ( gNIContinue->VerifyItem( pContinue ) == false )
                            throw( (sInt32)eDSInvalidContinueData );
                            
                        authDataBuff = inAuthData;
                    }
                    break;
                    
                case kAuthSetPasswd:
				case kAuthSetPolicy:
				    {
                        ni_id niDirID;
                        ni_namelist niValues;
                        ni_status niResult = NI_OK;
                        char* aaVersion = NULL;
                        char* aaTag = NULL;
                        char* aaData = NULL;
                        unsigned int idx;
                        sInt32 lookupResult;
                        char* endPtr = NULL;
                        
                        // lookup the user that wasn't passed to us
                       	error = GetUserNameFromAuthBuffer( inAuthData, 3, &userName );
                        if ( error != eDSNoErr ) throw( error );
                        
						gNetInfoMutex->Wait();
						void * aNIDomain = RetrieveNIDomain(inContext);
						if (aNIDomain != NULL)
						{
							// get the auth authority
							error = IsValidRecordName ( userName, inNativeRecType, aNIDomain, niDirID );
							if ( error != eDSNoErr )
							{
								gNetInfoMutex->Signal();
								throw( (sInt32)eDSAuthFailed );
							}
							
							//lookup authauthority attribute
							niResult = ni_lookupprop( aNIDomain, &niDirID, 
													"authentication_authority", &niValues );
						}
						else
						{
							gNetInfoMutex->Signal();
							throw( (sInt32)eDSInvalidSession );
						}
						gNetInfoMutex->Signal();
                        if (niResult != NI_OK) throw( (sInt32)eDSAuthFailed );
                     	
                        // don't break or throw to guarantee cleanup
                        lookupResult = eDSAuthFailed;
                        for ( idx = 0; idx < niValues.ni_namelist_len && lookupResult == eDSAuthFailed; idx++ )
                        {
                            // parse this value of auth authority
                            error = dsParseAuthAuthority( niValues.ni_namelist_val[idx], &aaVersion, &aaTag, &aaData );
                            // need to check version
                            if (error != eDSNoErr)
                                lookupResult = eParameterError;
                            
                            if ( error == eDSNoErr && strcmp(aaTag, kDSTagAuthAuthorityPasswordServer) == 0 )
                            {
                                endPtr = strchr( aaData, ':' );
                                if ( endPtr == NULL )
                                {
                                    lookupResult = eParameterError;
                                }
                                else
                                {
                                    *endPtr = '\0';
                                    lookupResult = eDSNoErr;
                                }
                            }
                            
                            if (aaVersion != NULL) {
                                free(aaVersion);
                                aaVersion = NULL;
                            }
                            if (aaTag != NULL) {
                                free(aaTag);
                                aaTag = NULL;
                            }
                            if (lookupResult != eDSNoErr && aaData != NULL) {
                                free(aaData);
                                aaData = NULL;
                            }
                        }
						gNetInfoMutex->Wait();
						if (niValues.ni_namelist_len > 0)
						{
							ni_namelist_free(&niValues);
						}
						gNetInfoMutex->Signal();
                        
                        if ( lookupResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
                        
                        // do the usual
                        error = RepackBufferForPWServer(inAuthData, uidStr, 1, &authDataBuffTemp );
                        if ( error != eDSNoErr ) throw( error );
                        
                        // put the admin user ID in slot 3
                        error = RepackBufferForPWServer(authDataBuffTemp, aaData, 3, &authDataBuff );
                        if (aaData != NULL) {
                            free(aaData);
                            aaData = NULL;
                        }
                        if ( error != eDSNoErr ) throw( error );
                    }
                    break;
					
				case kAuthClearText:
				case kAuthNativeClearTextOK:
				case kAuthNativeNoClearText:
				case kAuthCrypt:
					{
						tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
						if (dataList != NULL)
						{
							userName = dsDataListGetNodeStringPriv(dataList, 1);
							password = dsDataListGetNodeStringPriv(dataList, 2);
							// this allocates a copy of the string
							
							dsDataListDeallocatePriv(dataList);
							free(dataList);
							dataList = NULL;
						}
					}
					//fall through
                
                default:
                    error = RepackBufferForPWServer(inAuthData, uidStr, 1, &authDataBuff );
                    if ( error != eDSNoErr ) throw( error );
            }
            
            //DBGLOG( kLogPlugin, "ready to call PSPlugin\n");
			
            if ( inContext->fPWSRef == 0 )
            {
                error = ::dsOpenDirService( &inContext->fPWSRef );
                if ( error != eDSNoErr ) throw( error );
            }
            
            if ( inContext->fPWSNodeRef == 0 )
            {
                nodeName = (char *)calloc(1,strlen(serverAddr)+17);
                if ( nodeName == nil ) throw ( (sInt32)eMemoryError );
                
                sprintf( nodeName, "/PasswordServer/%s", serverAddr );
                error = PWOpenDirNode( inContext->fPWSRef, nodeName, &inContext->fPWSNodeRef );
                if ( error != eDSNoErr ) throw( error );
            }
            
            if ( pContinue )
                continueData = pContinue->fPassPlugContinueData;
            
            result = dsDoDirNodeAuth( inContext->fPWSNodeRef, inAuthMethod, inAuthOnly,
                                        authDataBuff, outAuthData, &continueData );
            
			if ( result == eDSAuthNoAuthServerFound || result == eDSAuthServerError )
			{
				result = PWSetReplicaData( inContext, uidStr );
				if ( result == eDSNoErr )
					result = dsDoDirNodeAuth( inContext->fPWSNodeRef, inAuthMethod, inAuthOnly,
												authDataBuff, outAuthData, &continueData );
			}
			
            if ( pContinue )
            {
                pContinue->fPassPlugContinueData = continueData;
                if ( continueData == NULL )
                {
                    gNIContinue->RemoveItem( pContinue );
                    if ( inOutContinueData == nil )
                        throw( (sInt32)eDSNullParameter );
                    *inOutContinueData = nil;
                }
            }
            
			if ( (result == eDSNoErr) && (inAuthOnly == false) && (userName != NULL) && (password != NULL) )
			{
				result = AuthOpen( inContext, userName, password );
			}
        }
    }
    
    catch(sInt32 err )
    {
        result = err;
    }
    
	if ( nodeName )
        free( nodeName );

	if ( uidStr != NULL )
	{
		free( uidStr );
		uidStr = NULL;
	}
    if ( userName != NULL )
	{
		free( userName );
		userName = NULL;
	}
	if ( password != NULL )
	{
		bzero(password, strlen(password));
		free( password );
		password = NULL;
	}
    
    if ( authDataBuff )
        dsDataBufferDeallocatePriv( authDataBuff );
    if ( authDataBuffTemp )
        dsDataBufferDeallocatePriv( authDataBuffTemp );
                        
	return( result );
} // DoPasswordServerAuth


#pragma mark -
#pragma mark TIM
#pragma mark -

//------------------------------------------------------------------------------------
//	* DoTimSMBAuth
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoTimSMBAuth ( sNIContextData *inContext, tDataBuffer *inAuthData, 
								 uInt32 inWhichOne )
{
	sInt32				siResult		= eDSAuthFailed;
#ifdef TIM_CLIENT_PRESENT

	TIMHandle		   *timHandle		= nil;
	char			   *pData			= nil;
	char			   *pUserName		= nil;
	uInt32				userNameLen		= 0;
	char			   *pC8				= nil;
	uInt32				c8Len			= 0;
	char			   *pP24			= nil;
	uInt32				p24Len			= 0;
	uInt32				offset			= 0;
	uInt32				buffSize		= 0;
	uInt32				buffLen			= 0;


	gNetInfoMutex->Wait();

	//opt out early if we know tim is not there **** check on local first
	if ( !IsTimRunning() )
	{
		gNetInfoMutex->Signal();
		return( eDSAuthMethodNotSupported );
	}

	try
	{
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
		if ( inContext == nil ) throw( (sInt32)eDSNullParameter );

		pData = inAuthData->fBufferData;
		buffSize = inAuthData->fBufferSize;
		buffLen = inAuthData->fBufferLength;

		if ( buffLen > buffSize ) throw( (sInt32)eDSInvalidBuffFormat );

		if ( buffLen < 3 * sizeof( unsigned long ) + 1 ) throw( (sInt32)eDSInvalidBuffFormat );
		// we need at least 3 x 4 bytes for lengths of three blocks of data,
		// and username must be at least 1 long
		::memcpy( &userNameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (userNameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (offset + userNameLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		pUserName = (char *)::calloc( 1, userNameLen + 1 );
		::memcpy( pUserName, pData, userNameLen );
		pData += userNameLen;
		offset += userNameLen;
   		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		::memcpy( &c8Len, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + c8Len > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		pC8 = (char *)::calloc( 1, c8Len + 1 );
		::memcpy( pC8, pData, c8Len );
		pData += c8Len;
		offset += c8Len;
   		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		::memcpy( &p24Len, pData, sizeof( unsigned long ) );
   		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + p24Len > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		pP24 = (char *)::calloc(  1, p24Len + 1 );
		::memcpy( pP24, pData, p24Len );
		pData += p24Len;
		offset += p24Len;

		void * aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			timHandle = timServerForDomain( aNIDomain );
			if ( timHandle == nil ) throw( (sInt32)eDSAuthFailed );

			switch ( inWhichOne )
			{
				case kAuthSMB_NT_Key:
					siResult = ::timAuthenticateSMBNTKeyWithTIMHandle( timHandle, pUserName, pC8, pP24 );
					break;

				case kAuthSMB_LM_Key:
					siResult = ::timAuthenticateSMBLMKeyWithTIMHandle( timHandle, pUserName, pC8, pP24 );
					break;
			}

			timHandleFree( timHandle );
			timHandle = nil;
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}

#ifdef DEBUG
		if ( siResult != TimStatusOK )
		{
			DBGLOG2( kLogPlugin, "-- timAuthenticateSMBLMKey -- failed with %l for key %l.", siResult, inWhichOne );
		}
#endif
		siResult = MapAuthResult( siResult );
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	if ( pUserName != nil )
	{
		free( pUserName );
		pUserName = nil;
	}

	if ( pC8 != nil )
	{
		bzero(pC8, c8Len);
		free( pC8 );
		pC8 = nil;
	}

	if ( pP24 != nil )
	{
		bzero(pP24, p24Len);
		free( pP24 );
		pP24 = nil;
	}
#endif
	return( siResult );

} // DoTimSMBAuth


//------------------------------------------------------------------------------------
//	* DoTimMultiPassAuth
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoTimMultiPassAuth ( tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod, 
									   sNIContextData *inContext, sNIContinueData** inOutContinueData, 
									   tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
									   bool inAuthOnly )
{
	sInt32			siResult		= eDSAuthFailed;
#ifdef TIM_CLIENT_PRESENT
	tDataBuffer	   *authData		= nil;
	tDataBuffer	   *outData			= nil;
	TIMHandle	   *authHndl		= nil;
	Buffer		   *pAuthBuff		= nil;
	Buffer		   *pAuthOutBuff	= nil;
	CString		   *pAuthStr		= nil;
	sNIContinueData  *pContinue		= nil;

	gNetInfoMutex->Wait(); //assume tim using netinfo needs mutex protection
	//opt out early if we know tim is not there **** check on local first
	if ( !IsTimRunning() )
	{
		gNetInfoMutex->Signal();
		return( eDSAuthMethodNotSupported );
	}

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSNullDataBuff );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
		if ( outAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
		if ( inOutContinueData == nil ) throw( (sInt32)eDSNullParameter );

		if ( *inOutContinueData == nil )
		{
			pContinue = (sNIContinueData *)::calloc( 1, sizeof( sNIContinueData ) );
			gNIContinue->AddItem( pContinue, inNodeRef );
		}
		else
		{
			pContinue = *inOutContinueData;
			if ( gNIContinue->VerifyItem( pContinue ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
		}

		void * aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			// First pass setup
			if ( pContinue->fAuthPass == 0 )
			{
				pAuthStr = GetAuthString( inAuthMethod );
				if ( pAuthStr == nil ) throw( (sInt32)eDSAuthMethodNotSupported );

				// Is there an auth server running?
				authHndl = ::timServerForDomain( aNIDomain );
				if ( authHndl == nil ) throw( (sInt32)eDSAuthNoAuthServerFound );

				// Start a session with the auth server
				pAuthBuff = ::bufferFromString( pAuthStr->GetData() );
				siResult = ::timSessionStart( authHndl, 0, pAuthBuff);
				::bufferRelease( pAuthBuff );
				pAuthBuff = nil;
				if ( siResult != eDSNoErr ) throw( siResult );

				pContinue->fAuthHndl = authHndl;

				delete( pAuthStr );
				pAuthStr = nil;
			}
			else
			{
				//if ( *inOutContinueData == authHndl ) // JT why are we doing this?
				// authHndl is nil at this point anyway
				if ( *inOutContinueData == NULL )
				{
					throw( (sInt32)eDSInvalidContext );
				}
			}

			pContinue->fAuthPass++;

			authData = inAuthData;
			outData	 = outAuthData;
			authHndl = (TIMHandle *)pContinue->fAuthHndl;

			pAuthBuff = ::bufferFromData( authData->fBufferData, authData->fBufferLength );
			siResult = ::timSessionStep( authHndl, pAuthBuff, &pAuthOutBuff );
			::bufferRelease( pAuthBuff );
			pAuthBuff = nil;

			if ( (siResult != TimStatusOK) && (siResult != TimStatusContinue) )
			{
				// Things didn't go well, clean up and get out.
				::bufferRelease( pAuthOutBuff );
				pAuthOutBuff = nil;

				::timSessionEnd( authHndl );
				authHndl = nil;

				gNIContinue->RemoveItem( pContinue );
				*inOutContinueData = nil;
				
				if ( siResult != eDSNoErr ) throw( siResult );
			}

			if ( ::bufferLength( pAuthOutBuff ) <= outData->fBufferSize )
			{
				::memcpy( outData->fBufferData, pAuthOutBuff->data, pAuthOutBuff->length );
				outData->fBufferLength = pAuthOutBuff->length;
				::bufferRelease( pAuthOutBuff );

				if ( siResult == TimStatusOK )
				{
					// This is a good thing.
					// Release the auth handle
					::timSessionEnd( authHndl );
					authHndl = nil;

					// Clean up the continue data
					gNIContinue->RemoveItem( pContinue );

					// No continue data sent back.
					*inOutContinueData = nil;
				}
				else if ( siResult == TimStatusContinue )
				{
					// Save the session handle
					pContinue->fAuthHndl = authHndl;

					*inOutContinueData = pContinue;
					siResult = eDSNoErr;
				}
			}
		}
		else
		{
			throw( (sInt32)eDSInvalidSession );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;

		if ( siResult > -14000 )
		{
#ifdef DEBUG
			switch ( siResult )
			{
				case TimStatusUserUnknown:
					siResult = eDSAuthUnknownUser;
					break;

				case TimStatusInvalidName:
					siResult = eDSAuthInvalidUserName;
					break;

				case TimStatusUnrecoverablePassword:
					siResult = eDSAuthCannotRecoverPasswd;
					break;

				case TimStatusAuthenticationFailed:
					siResult = eDSAuthFailed;
					break;

				default:
					DBGLOG1( kLogPlugin, "Authentication Server Error = %d.", siResult);
					siResult = eDSAuthServerError;
					break;
			}

			if ( siResult != TimStatusOK )
			{
				DBGLOG1( kLogPlugin, "-- DoMultiPassAuth -- failed with %l.", siResult );
			}
#else
			siResult = eDSAuthFailed;
#endif
		}
	}

	gNetInfoMutex->Signal();
#endif
	return( siResult );

} // DoTimMultiPassAuth


// ---------------------------------------------------------------------------
//	* IsTimRunning
//    - check for a tim server on the local domain
// ---------------------------------------------------------------------------

bool CNiPlugIn::IsTimRunning()
{
	bool bTimRunning = false;
#ifdef TIM_CLIENT_PRESENT
	TIMHandle* timHandle = NULL;
	ni_status niStatus = NI_OK;
	void* domain = NULL;
	Buffer *tagBuffer;
	int timSessionStatus;
	
	//use set value if 60 secs have not gone by
	if ( time(nil) < gCheckTimAgainTime )
	{
		return gTimIsRunning;
	}

	gNetInfoMutex->Wait();

	gTimIsRunning		= false;
	gCheckTimAgainTime	= time( nil) + 60;

	niStatus = netinfo_open( NULL, ".", &domain, 10 );
	if (niStatus == NI_OK)
	{
		timHandle = ::timServerForDomain( domain );
		if (timHandle != NULL)
		{
			//checking here now if tim was properly configured and not just started by mistake
			tagBuffer = bufferFromString("local");
			timHandleSetTag(timHandle, tagBuffer);
			bufferRelease(tagBuffer);
	
			tagBuffer = bufferFromString("Status NetInfo Domain");
			timSessionStatus = timSessionStart(timHandle, TimOpUtility, tagBuffer);
			bufferRelease(tagBuffer);
			if (timSessionStatus == TimStatusOK)
			{
				timSessionStatus = timSessionStep(timHandle, NULL, &tagBuffer);
				bufferRelease(tagBuffer);
				if (timSessionStatus == TimStatusOK)
				{
					bTimRunning		= true;
					gTimIsRunning	= true;
				}	
				timSessionEnd(timHandle);
			}
			::timHandleFree( timHandle );
			timHandle = NULL;
		}
	}

	gNetInfoMutex->Signal();
#endif
	return bTimRunning;
} // IsTimRunning()


#pragma mark -
#pragma mark Other Auth Routines
#pragma mark -


//------------------------------------------------------------------------------------
//	* DoNodeNativeAuth
//------------------------------------------------------------------------------------

//this is REALLY 2-way random with tim authserver ie. name is very mis-leading
sInt32 CNiPlugIn::DoNodeNativeAuth ( sNIContextData *inContext, tDataBuffer *inAuthData, bool inAuthOnly, const char *inNativeRecType )
{
	sInt32			siResult		= eDSAuthFailed;
#ifdef TIM_CLIENT_PRESENT
	TIMHandle	   *timHandle		= nil;
	char		   *pathStr			= nil;
	char		   *pData			= nil;
	char		   *userName		= nil;
	uInt32			nameLen			= 0;
	char		   *pwd				= nil;
	uInt32			pwdLen			= 0;

	gNetInfoMutex->Wait();

	//opt out early if we know tim is not there **** check on local first
	if ( !IsTimRunning() )
	{
		gNetInfoMutex->Signal();
		return( eDSAuthMethodNotSupported );
	}
	try
	{
		if ( inAuthData != nil )
		{
			pData = inAuthData->fBufferData;
			if ( pData != nil )
			{
				// Can we do clear text auth for this domain
//				validMethod = VerifyAuthMethod( inContext->fDomain, AppleStandardTwoWay );
//				if ( validMethod == eDSNoErr )
				{
					// Get the length of the user name
					::memcpy( &nameLen, pData, sizeof( unsigned long ) );
					if ( nameLen != 0 )
					{
						userName = (char *)::calloc(  1, nameLen + 1 );

						// Copy the user name
//						::memset( userName, 0, nameLen + 1 );
						pData += sizeof( unsigned long );
						::memcpy( userName, pData, nameLen );
						pData += nameLen;

						// Get the length of the user password
						::memcpy( &pwdLen, pData, sizeof( unsigned long ) );

						if ( pwdLen != 0 )
						{
							pwd = (char *)::calloc( 1, pwdLen + 1 );

							// Copy the user password
							pData += sizeof( unsigned long );
							::memcpy( pwd, pData, pwdLen );
							pData += pwdLen;

							DBGLOG( kLogPlugin, "Attempting Auth Server Node Native Authentication." );

							void * aNIDomain = RetrieveNIDomain(inContext);
							if (aNIDomain != NULL)
							{
								timHandle = timServerForDomain( aNIDomain );
								if ( timHandle == nil ) throw( (sInt32)eDSAuthFailed );
								
								siResult = ::timAuthenticate2WayRandomWithTIMHandle( timHandle, userName, pwd );

								timHandleFree( timHandle );
								timHandle = nil;
							}
							else
							{
								throw( (sInt32)eDSInvalidSession );
							}
#ifdef DEBUG
							if ( siResult != TimStatusOK )
							{
								DBGLOG1( kLogPlugin, "-- timAuthenticate2WayRandom -- failed with %l.", siResult );
							}
#endif
							siResult = MapAuthResult( siResult );

							if ( (siResult == eDSNoErr) && (inAuthOnly == false) )
							{
								siResult = AuthOpen( inContext, userName, pwd );
							}
							
							bzero(pwd, pwdLen);
							free( pwd );
							pwd = nil;
						}
						else
						{
#ifdef DEBUG
							siResult = eDSAuthInBuffFormatError;
#else
							siResult = eDSAuthFailed;
#endif
						}
						free( userName );
						userName = nil;
					}
					else
					{
#ifdef DEBUG
						siResult = eDSAuthInBuffFormatError;
#else
						siResult = eDSAuthFailed;
#endif
					}
				}
			}
			else
			{
				siResult = eDSNullDataBuff;
			}
		}
		else
		{
			siResult = eDSNullDataBuff;
		}
	}

	catch( sInt32 err )
	{
		DBGLOG1( kLogPlugin, "2 way random authentication error %l.", err );
		siResult = err;
	}

	gNetInfoMutex->Signal();

	if ( pathStr != nil )
	{
		free( pathStr );
		pathStr = nil;
	}
#endif
	return( siResult );

} // DoNodeNativeAuth

//------------------------------------------------------------------------------------
//	* DoUnixCryptAuth
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoUnixCryptAuth ( sNIContextData *inContext, tDataBuffer *inAuthData, bool inAuthOnly, const char *inNativeRecType )
{
	sInt32			siResult		= eDSAuthFailed;
	bool			bFreePropList	= false;
	bool			bResetCache		= false;
	ni_index   		niWhere			= 0;
	char		   *pData			= nil;
	char		   *niPwd			= nil;
	char		   *name			= nil;
	uInt32			nameLen			= 0;
	char		   *pwd				= nil;
	uInt32			pwdLen			= 0;
	uInt32			offset			= 0;
	uInt32			buffSize		= 0;
	uInt32			buffLen			= 0;
	char			salt[ 9 ];
	char			hashPwd[ 32 ];
	ni_id			niDirID;
	ni_proplist		niPropList;

	gNetInfoMutex->Wait();

	try
	{
#ifdef DEBUG
		if ( inAuthData == nil ) throw( (sInt32)eDSAuthParameterError );
#else
		if ( inAuthData == nil ) throw( (sInt32)eDSAuthFailed );
#endif

		pData		= inAuthData->fBufferData;
		buffSize	= inAuthData->fBufferSize;
		buffLen		= inAuthData->fBufferLength;

		if (buffLen > buffSize) throw( (sInt32)eDSInvalidBuffFormat );

		if (offset + (2 * sizeof( unsigned long) + 1) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
		// need username length, password length, and username must be at least 1 character
		::memcpy( &nameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		
#ifdef DEBUG
		if (nameLen == 0) throw( (sInt32)eDSAuthUnknownUser );
		if (nameLen > 256) throw( (sInt32)eDSInvalidBuffFormat );
		if (offset + nameLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
#else
		if (nameLen == 0) throw( (sInt32)eDSAuthFailed );
		if (nameLen > 256) throw( (sInt32)eDSAuthFailed );
		if (offset + nameLen > buffLen) throw( (sInt32)eDSAuthFailed );
#endif

		name = (char *)::calloc( nameLen + 1, sizeof( char ) );
		::memcpy( name, pData, nameLen );
		pData += nameLen;
		offset += nameLen;

		DBGLOG( kLogPlugin, "Attempting UNIX Crypt authentication" );

		void * aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
	//		::strcat( userPath, name );
			siResult = IsValidRecordName ( name, inNativeRecType, aNIDomain, niDirID );
	//		siResult = ::ni_pathsearch( aNIDomain, &niDirID, userPath );

	#ifdef DEBUG
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
	#else
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
	#endif

			siResult = ::ni_read( aNIDomain, &niDirID, &niPropList );
	#ifdef DEBUG
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
			if ( niPropList.ni_proplist_val == nil ) throw( (sInt32)eDSAuthUnknownUser );
	#else
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
			if ( niPropList.ni_proplist_val == nil ) throw( (sInt32)eDSAuthFailed );
	#endif
			bFreePropList = true;

			niWhere = ::ni_proplist_match( niPropList, "passwd", nil );
	#ifdef DEBUG
			if (niWhere == NI_INDEX_NULL) throw( (sInt32)eDSAuthBadPassword );
			siResult = eDSAuthBadPassword;
	#else
			if (niWhere == NI_INDEX_NULL) throw( (sInt32)eDSAuthFailed );
			siResult = eDSAuthFailed;
	#endif

			// be careful not to dereference nil here.
			// We assume it is an empty password if there are no values for passwd.
			if ((niPropList.ni_proplist_val[ niWhere ].nip_val.ni_namelist_len > 0)
				&& (niPropList.ni_proplist_val[ niWhere ].nip_val.ni_namelist_val != nil))
			{
				niPwd = niPropList.ni_proplist_val[ niWhere ].nip_val.ni_namelist_val[ 0 ];
			}
			else
			{
				niPwd = (char*)""; // empty string, we are not freeing it so direct assignment is OK
			}

	#ifdef DEBUG
			if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
	#else
			if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSAuthFailed );
	#endif
			::memcpy( &pwdLen, pData, sizeof( unsigned long ) );
			pData += sizeof( unsigned long );
			offset += sizeof( unsigned long );
			if ( pwdLen >= kHashRecoverableLength ) throw ( (sInt32)eDSAuthPasswordTooLong );
			
	#ifdef DEBUG
			if (offset + pwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
			if (pwdLen > 256) throw( (sInt32)eDSAuthFailed );
	#else
			if (offset + pwdLen > buffLen) throw( (sInt32)eDSAuthFailed );
			if (pwdLen > 256) throw( (sInt32)eDSAuthFailed );
	#endif
			pwd = (char *)::calloc( pwdLen + 1, sizeof( char ) );
			::memcpy( pwd, pData, pwdLen );

			//account for the case where niPwd == "" such that we will auth if pwdLen is 0
			if (::strcmp(niPwd,"") != 0)
			{
				salt[ 0 ] = niPwd[0];
				salt[ 1 ] = niPwd[1];
				salt[ 2 ] = '\0';

				::memset( hashPwd, 0, 32 );
				::strcpy( hashPwd, ::crypt( pwd, salt ) );

				siResult = eDSAuthFailed;
				if ( ::strcmp( hashPwd, niPwd ) == 0 )
				{
					siResult = eDSNoErr;
				}
				bzero(hashPwd, 32);
			}
			else // niPwd is == ""
			{
				if ( ::strcmp(pwd,"") == 0 )
				{
					siResult = eDSNoErr;
				}
			}

			if (siResult == eDSNoErr)
			{
				if ( inContext->bIsLocal && (strlen(pwd) < 8) ) //local node and true crypt password since length is 7 or less
				{
					MigrateToShadowHash(aNIDomain, &niDirID, &niPropList, name, pwd, bResetCache);
				}

				if (inAuthOnly == false)
				{
					siResult = AuthOpen( inContext, name, pwd );
				}
			}
		}
		else
		{
#ifdef DEBUG
			siResult = eDSInvalidSession;
#else
			siResult = eDSAuthFailed;
#endif
		}
	}

	catch( sInt32 err )
	{
		DBGLOG1( kLogPlugin, "Crypt authentication error %l", err );
		siResult = err;
	}

	if ( bFreePropList )
	{
		::ni_proplist_free( &niPropList );
	}

	gNetInfoMutex->Signal();
	
	if ( bResetCache )
	{
		// be sure not to hold the NetInfo mutex while calling this
		gSrvrCntl->HandleMemberDaemonFlushCache();
	}

	if ( name != nil )
	{
		free( name );
		name = nil;
	}

	if ( pwd != nil )
	{
		bzero(pwd, strlen(pwd));
		free( pwd );
		pwd = nil;
	}
	
	return( siResult );

} // DoUnixCryptAuth


//------------------------------------------------------------------------------------
//	* ValidateDigest ()
//
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::ValidateDigest ( sNIContextData *inContext, tDataBuffer *inAuthData, uInt32 inAuthMethod )
{
	sInt32			siResult		= eDSAuthFailed;
#ifdef TIM_CLIENT_PRESENT
	TIMHandle	   *pTimHndl		= nil;
	char		   *pathStr			= nil;
	char		   *pData			= nil;
	char		   *pUserName		= nil;
	uInt32			nameLen			= 0;
	char		   *pChallenge		= nil;
	uInt32			chalLen			= 0;
	char		   *pResponse		= nil;
	uInt32			respLen			= 0;
	sInt32			siBuffErr		= 0;
#ifdef DEBUG
	siBuffErr = eDSAuthInBuffFormatError;
#else
	siBuffErr = eDSAuthFailed;
#endif

	gNetInfoMutex->Wait();

	//opt out early if we know tim is not there **** check on local first
	if ( !IsTimRunning() )
	{
		gNetInfoMutex->Signal();
		return( eDSAuthMethodNotSupported );
	}
	try
	{
		if ( inAuthData != nil )
		{
			pData = inAuthData->fBufferData;
			if ( pData != nil )
			{
				// Get the length of the user name
				::memcpy( &nameLen, pData, sizeof( unsigned long ) );
				if ( nameLen != 0 )
				{
					pUserName = (char *)::calloc(  1, nameLen + 1 );

					// Copy the user name
					pData += sizeof( unsigned long );
					::memcpy( pUserName, pData, nameLen );
					pData += nameLen;

					// Get the length of the challenge
					::memcpy( &chalLen, pData, sizeof( unsigned long ) );
					if ( chalLen != 0 )
					{
						pChallenge = (char *)::calloc( 1, chalLen + 1 );

						pData += sizeof( unsigned long );
						::memcpy( pChallenge, pData, chalLen );
						pData += chalLen;

						// Get the length of the challenge
						::memcpy( &respLen, pData, sizeof( unsigned long ) );
						if ( respLen != 0 )
						{
							pResponse = (char *)::calloc( 1, respLen + 1 );

							// Copy the user response
							pData += sizeof( unsigned long );
							::memcpy( pResponse, pData, respLen );
							pData += respLen;

							DBGLOG( kLogPlugin, "Attempting Auth ValidateDigest." );

							void * aNIDomain = RetrieveNIDomain(inContext);
							if (aNIDomain != NULL)
							{
								pTimHndl = ::timServerForDomain( aNIDomain );
								if ( pTimHndl == nil ) throw( (sInt32)eDSAuthFailed );

								if ( inAuthMethod == kAuthAPOP )
								{
									siResult = ::timAuthenticateAPOPWithTIMHandle( pTimHndl, pUserName, pChallenge, pResponse );
								}
								else if ( inAuthMethod == kAuthCRAM_MD5 )
								{
									siResult = ::timAuthenticateCRAM_MD5WithTIMHandle( pTimHndl, pUserName, pChallenge, pResponse );
								}
								else
								{
									siResult = TimStatusServiceUnavailable;
								}

								timHandleFree( pTimHndl );
								pTimHndl = nil;
							}
							else
							{
								throw( (sInt32)eDSInvalidSession );
							}
#ifdef DEBUG
							if ( siResult != TimStatusOK )
							{
								DBGLOG1( kLogPlugin, "-- timAuthenticateCRAM_MD5WithTIMHandle -- failed with %l.", siResult );
							}
#endif
							siResult = MapAuthResult( siResult );

							bzero(pResponse, strlen(pResponse));
							free( pResponse );
							pResponse = nil;

						}
						else
						{
							siResult = siBuffErr;
						}
						bzero(pChallenge, strlen(pChallenge));
						free( pChallenge );
						pChallenge = nil;
					}
					else
					{
						siResult = siBuffErr;
					}
					free( pUserName );
					pUserName = nil;
				}
				else
				{
					siResult = siBuffErr;
				}
			}
			else
			{
				siResult = eDSNullDataBuff;
			}
		}
		else
		{
			siResult = eDSNullDataBuff;
		}
	}

	catch( sInt32 err )
	{
		DBGLOG1( kLogPlugin, "2 way random authentication error %l.", err );
		siResult = err;
	}

	gNetInfoMutex->Signal();

	if ( pathStr != nil )
	{
		free( pathStr );
		pathStr = nil;
	}
#endif
	return( siResult );

} // ValidateDigest


// ---------------------------------------------------------------------------
//	* AuthOpen
// ---------------------------------------------------------------------------

sInt32 CNiPlugIn::AuthOpen ( sNIContextData *inContext, const char * inUserName, 
							 const char * inPassword, bool bIsEffectiveRoot )
{
	sInt32			siResult		= eDSAuthFailed;
	void		   *domain			= nil;
	
	gNetInfoMutex->Wait();
	
	try 
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );
		if ( inUserName == nil ) throw( (sInt32)eDSAuthFailed );
		if ( inPassword == nil ) throw( (sInt32)eDSAuthFailed );		
		if ( inContext->fDomainPath == nil ) throw( (sInt32)eDSAuthFailed ); //shouldn't happen since we opened the node OK
		if ( (inPassword != nil) && (strlen(inPassword) >= kHashRecoverableLength ) ) throw ( (sInt32)eDSAuthPasswordTooLong );
		
		siResult = ::ni_open( nil, inContext->fDomainPath, &domain );
		if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
		DBGLOG1( kLogPlugin, "CNiPlugIn::AuthOpen: <ni_open> opened for auth the netinfo node %s", inContext->fDomainPath );
		if (!bIsEffectiveRoot)
		{
			// we need to establish authentication against this domain
			siResult = ::ni_setuser( domain, inUserName );
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
			siResult = ::ni_setpassword( domain, inPassword );
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
		}
		
		if ( siResult == NI_OK )
		{
			// free up the old one and save our new domain in the context
			if ( inContext->fDontUseSafeClose )
			{
				::ni_free(inContext->fDomain);
			}
			else if (inContext->fDomainName != nil)
			{
				CNetInfoPlugin::SafeClose( inContext->fDomainName );
			}
			inContext->fDomain = domain;
			inContext->fDontUseSafeClose = true;
			if ( inContext->fAuthenticatedUserName != NULL )
			{
				free( inContext->fAuthenticatedUserName );
			}
			// check for admin group, and treat admin users as root
			if (UserIsAdminInDomain(inUserName, domain))
			{
				inContext->fAuthenticatedUserName = strdup( "root" );
			}
			else
			{
				inContext->fAuthenticatedUserName = strdup( inUserName );
			}
		}
	}
	
	catch( sInt32 err )
	{
		if ( domain != nil )
		{
			::ni_free(domain);
			domain = nil;
		}
				
		siResult = err;
	}
	
	gNetInfoMutex->Signal();

	return siResult;
} // AuthOpen


#pragma mark -
#pragma mark Support Routines
#pragma mark -

// ---------------------------------------------------------------------------
//	* VerifyPatternMatch
// ---------------------------------------------------------------------------

sInt32 CNiPlugIn::VerifyPatternMatch ( const tDirPatternMatch inPatternMatch )
{
	sInt32		siResult = eDSNoErr;

	switch ( inPatternMatch )
	{
		case eDSExact:
		case eDSStartsWith:
		case eDSEndsWith:
		case eDSContains:
		case eDSLessThan:
		case eDSGreaterThan:
		case eDSLessEqual:
		case eDSGreaterEqual:
		//case eDSWildCardPattern:
		//case eDSRegularExpression:
		case eDSiExact:
		case eDSiStartsWith:
		case eDSiEndsWith:
		case eDSiContains:
		case eDSiLessThan:
		case eDSiGreaterThan:
		case eDSiLessEqual:
		case eDSiGreaterEqual:
		//case eDSiWildCardPattern:
		//case eDSiRegularExpression:
		case eDSAnyMatch:
			siResult = eDSNoErr;
			break;

		default:
			siResult = eDSInvalidPatternMatchType;
			break;
	}


	return( siResult );

} // VerifyPatternMatch


// ---------------------------------------------------------------------------
//	* GetAuthTypeStr
// ---------------------------------------------------------------------------

CString* CNiPlugIn::GetAuthTypeStr ( const char *inNativeAuthStr )
{
	CString	   *pOutString	= nil;

	if ( inNativeAuthStr == nil )
	{
		return( nil );
	}

	pOutString = new CString( 128 );
	if ( pOutString == nil )
	{
		return( nil );
	}
#ifdef TIM_CLIENT_PRESENT
	if ( ::strcmp( inNativeAuthStr, AppleStandardCleartext ) == 0 )
	{
		pOutString->Set( kDSStdAuthClearText );
	}
	else if ( ::strcmp( inNativeAuthStr, AppleStandardTwoWay ) == 0 )
	{
		pOutString->Set( kDSStdAuth2WayRandom );
	}
	else if ( ::strcmp( inNativeAuthStr, AppleStandardSMBNTKey ) == 0 )
	{
		pOutString->Set( kDSStdAuthSMB_NT_Key );
	}
	else if ( ::strcmp( inNativeAuthStr, AppleStandardSMBLMKey ) == 0 )
	{
		pOutString->Set( kDSStdAuthSMB_LM_Key );
	}
	else
#endif
	{
		pOutString->Set( kDSNativeAuthMethodPrefix );
		pOutString->Append( inNativeAuthStr );
	}

	return( pOutString );

} // GetAuthTypeStr


// ---------------------------------------------------------------------------
//	* GetAuthString
// ---------------------------------------------------------------------------

CString* CNiPlugIn::GetAuthString ( tDataNode *inData )
{
	CString		   *pOutString		= nil;

#ifdef TIM_CLIENT_PRESENT
	char		   *p				= nil;
	uInt32			uiNativeLen		= 0;
	uInt32			uiStrLen		= 0;

	if ( inData == nil )
	{
		return( nil );
	}

	pOutString = new CString( 128 );
	if ( pOutString == nil )
	{
		return( nil );
	}

	p = (char *)inData->fBufferData;

	if ( ::strcmp( p, kDSStdAuthClearText ) == 0 )
	{
		// Clear text auth method
		pOutString->Set( AppleStandardCleartext );
	}
	else if ( (::strcmp( p, kDSStdAuthNodeNativeClearTextOK ) == 0) || 
			  (::strcmp( p, kDSStdAuthNodeNativeNoClearText) == 0 ) )
	{
		// Node native auth method
		pOutString->Set( AppleStandardTwoWay );
	}
	else if ( ::strcmp( p, kDSStdAuth2WayRandom ) == 0 )
	{
		// Two way random auth method
		pOutString->Set( AppleStandardTwoWay );
	}
	else if ( ::strcmp( p, kDSStdAuthSetPasswd ) == 0 )
	{
		// Admin set password
		pOutString->Set( AppleStandardSetPassword );
	}
	else if ( ::strcmp( p, kDSStdAuthChangePasswd ) == 0 )
	{
		// User change password
		pOutString->Set( AppleSetPasswordV2 );
	}
	else if ( ::strcmp( p, kDSStdAuthSetPasswdAsRoot ) == 0 )
	{
		// User change password
		pOutString->Set( AppleSetPasswordV2 );
	}
	else if ( ::strcmp( p, kDSStdAuthSMB_NT_Key ) == 0 )
	{
		// User change password
		pOutString->Set( AppleStandardSMBNTKey );
	}
	else if ( ::strcmp( p, kDSStdAuthSMB_LM_Key ) == 0 )
	{
		// User change password
		pOutString->Set( AppleStandardSMBLMKey );
	}
	else
	{
		uiStrLen = ::strlen( p );
		uiNativeLen	= sizeof( kDSNativeAuthMethodPrefix ) - 1;

		if ( ::strncmp( p, kDSNativeAuthMethodPrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				pOutString->Set( p + uiNativeLen );
			}
		}
		else
		{
			delete( pOutString );
			pOutString = nil;
		}
	}
#endif
	return( pOutString );

} // GetAuthString


// ---------------------------------------------------------------------------
//	* MapAuthResult
// ---------------------------------------------------------------------------

sInt32 CNiPlugIn::MapAuthResult ( sInt32 inAuthErr )
{
	sInt32		siResult	= eUndefinedError;

	switch ( inAuthErr )
	{
#ifdef TIM_CLIENT_PRESENT
		case TimStatusOK:
			siResult = eDSNoErr;
			break;

#ifdef DEBUG
		case TimStatusContinue:
			siResult = eDSContinue;
			break;

		case TimStatusServerTimeout:
			siResult = eDSServerTimeout;
			break;

		case TimStatusInvalidHandle:
			siResult = eDSInvalidHandle;
			break;

		case TimStatusSendFailed:
			siResult = eDSSendFailed;
			break;

		case TimStatusReceiveFailed:
			siResult = eDSReceiveFailed;
			break;

		case TimStatusBadPacket:
			siResult = eDSBadPacket;
			break;

		case TimStatusInvalidTag:
			siResult = eDSInvalidTag;
			break;

		case TimStatusInvalidSession:
			siResult = eDSInvalidSession;
			break;

		case TimStatusInvalidName:
			siResult = eDSInvalidName;
			break;

		case TimStatusUserUnknown:
			siResult = eDSUserUnknown;
			break;

		case TimStatusUnrecoverablePassword:
			siResult = eDSUnrecoverablePassword;
			break;

		case TimStatusAuthenticationFailed:
			siResult = eDSAuthenticationFailed;
			break;

		case TimStatusBogusServer:
			siResult = eDSBogusServer;
			break;

		case TimStatusOperationFailed:
			siResult = eDSOperationFailed;
			break;

		case TimStatusNotAuthorized:
			siResult = eDSNotAuthorized;
			break;

		case TimStatusNetInfoError:
			siResult = eDSNetInfoError;
			break;

		case TimStatusContactMaster:
			siResult = eDSContactMaster;
			break;

		case TimStatusServiceUnavailable:
			siResult = eDSServiceUnavailable;
			break;
#endif
#endif
		default:
			siResult = eDSAuthFailed;
			break;
	}

	return( siResult );

} // MapAuthResult


// ---------------------------------------------------------------------------
//	* MakeGood
// ---------------------------------------------------------------------------

void CNiPlugIn::MakeGood( char *inStr, char *outStr )
{
	uInt32	uiI		= 0;
	uInt32	uiJ		= 0;

	if ( (inStr != nil) && (outStr != nil) )
	{
		while ( inStr[ uiI ] != '\0' )
		{
			if ( (inStr[ uiI ] == '\\') && (inStr[ uiI + 1 ] == '/') )
			{
				// Skip past the escape before the separator char
				uiI++;
			}

			outStr[ uiJ ] = inStr[ uiI ];			
			uiI++;
			uiJ++;
		}
		outStr[ uiJ ] = '\0';
	}
} // MakeGood


// ---------------------------------------------------------------------------
//	* MakeContextData
// ---------------------------------------------------------------------------

sNIContextData* CNiPlugIn::MakeContextData ( void )
{
	sNIContextData	*pOut	= nil;

	pOut = (sNIContextData *) ::calloc( 1, sizeof( sNIContextData) );
	pOut->fUID = (uid_t)-2;
	pOut->fEffectiveUID = (uid_t)-2;
	pOut->bIsLocal = false;

	return( pOut );

} // MakeContextData

// ---------------------------------------------------------------------------
//	* CleanContextData
// ---------------------------------------------------------------------------

sInt32 CNiPlugIn::CleanContextData ( sNIContextData *inContext )
{
    sInt32	siResult = eDSNoErr;
    
    if ( inContext == nil )
    {
        siResult = eDSBadContextData;
	}
    else
    {
		if (inContext->fDomain != nil)
		{
			if ( inContext->fDontUseSafeClose )
			{
				gNetInfoMutex->Wait();
				::ni_free(inContext->fDomain);
				gNetInfoMutex->Signal();
				inContext->fDomain = nil;
			}
		}
		else if (inContext->fDomainName != nil)
		{
			CNetInfoPlugin::SafeClose( inContext->fDomainName );
		}
		//now we do own this pointer
		DSFreeString(inContext->fDomainName);
		DSFreeString(inContext->fDomainPath);
		DSFreeString(inContext->fRecType);
		DSFreeString(inContext->fRecName);
		DSFreeString(inContext->fAuthenticatedUserName);
		//dirID is a struct of two ulongs
		inContext->offset				= 0;
		inContext->fDontUseSafeClose	= false;
		inContext->bIsLocal				= false;
		inContext->bDidWriteOperation	= false;
        
        if ( inContext->fPWSNodeRef != 0 ) {
            dsCloseDirNode(inContext->fPWSNodeRef);
			inContext->fPWSNodeRef = 0;
		}
        if ( inContext->fPWSRef != 0 ) {
            dsCloseDirService(inContext->fPWSRef);
			inContext->fPWSRef = 0;
		}
		if (inContext->fLocalCacheNetNode != NULL)
		{
			dsDataListDeallocatePriv(inContext->fLocalCacheNetNode);
			free(inContext->fLocalCacheNetNode);
			inContext->fLocalCacheNetNode = NULL;
		}
		if (inContext->fLocalCacheRef != 0)
		{
			dsCloseDirService(inContext->fLocalCacheRef);
			inContext->fLocalCacheRef = 0;
		}
	}
    
	return( siResult );

} // CleanContextData


// ---------------------------------------------------------------------------
//	* DoAnyMatch
// ---------------------------------------------------------------------------

bool CNiPlugIn::DoAnyMatch (	const char		   *inString,
								CAttributeList     *inPattMatchList,
								tDirPatternMatch	inHow )
{
	bool				bOutResult  = false;
	CFMutableStringRef	strRef		= CFStringCreateMutable(NULL, 0);
	CFMutableStringRef	patRef		= NULL;
	CFRange				range;

	if ( (inString == nil) || (inPattMatchList == nil) || (strRef == nil) )
	{
		return( false );
	}
	if (inPattMatchList->GetCount() < 1)
	{
		return( false );
	}

	CFStringAppendCString( strRef, inString, kCFStringEncodingUTF8 );
	
	for (uInt32 idx = 1; idx <= inPattMatchList->GetCount(); idx++)
	{
		char *aStringValue = nil;
		if ( eDSNoErr == inPattMatchList->GetAttribute(idx, &aStringValue) )
		{
			patRef = CFStringCreateMutable(NULL, 0);
			CFStringAppendCString( patRef, aStringValue, kCFStringEncodingUTF8 );	
			if ( (inHow >= eDSiExact) && (inHow <= eDSiRegularExpression) )
			{
				CFStringUppercase( strRef, NULL );
				CFStringUppercase( patRef, NULL );
			}

			switch ( inHow )
			{
				case eDSExact:
				case eDSiExact:
				{
					if ( CFStringCompare( strRef, patRef, 0 ) == kCFCompareEqualTo )
					{
						bOutResult = true;
					}
				}
				break;

				case eDSStartsWith:
				case eDSiStartsWith:
				{
					if ( CFStringHasPrefix( strRef, patRef ) )
					{
						bOutResult = true;
					}
				}
				break;

				case eDSEndsWith:
				case eDSiEndsWith:
				{
					if ( CFStringHasSuffix( strRef, patRef ) )
					{
						bOutResult = true;
					}
				}
				break;

				case eDSContains:
				case eDSiContains:
				{
					range = CFStringFind( strRef, patRef, 0 );
					if ( range.location != kCFNotFound )
					{
						bOutResult = true;
					}
				}
				break;

				case eDSLessThan:
				case eDSiLessThan:
				{
					if ( CFStringCompare( strRef, patRef, 0 ) == kCFCompareLessThan )
					{
						bOutResult = true;
					}
				}
				break;

				case eDSGreaterThan:
				case eDSiGreaterThan:
				{
					if ( CFStringCompare( strRef, patRef, 0 ) == kCFCompareGreaterThan )
					{
						bOutResult = true;
					}
				}
				break;

				case eDSLessEqual:
				case eDSiLessEqual:
				{
					if ( CFStringCompare( strRef, patRef, 0 ) != kCFCompareGreaterThan )
					{
						bOutResult = true;
					}
				}
				break;

				case eDSGreaterEqual:
				case eDSiGreaterEqual:
				{
					if ( CFStringCompare( strRef, patRef, 0 ) != kCFCompareLessThan )
					{
						bOutResult = true;
					}
				}
				break;

				default:
					break;
			}

			CFRelease( patRef );
			patRef = nil;
			
			if (bOutResult)
			{
				break;
			}
		}
	}
	
	CFRelease( strRef );
	strRef = nil;

	return( bOutResult );

} // DoAnyMatch


// ---------------------------------------------------------------------------
//	* BuildRegExp
// ---------------------------------------------------------------------------

char* CNiPlugIn:: BuildRegExp ( const char *inString )

{
	char		   *outRegExp	= nil;
	unsigned long  	i			= 0;
	unsigned long	j			= 0;
	unsigned long	inLength	= 0;

	if (inString == nil)
	{
		return( outRegExp );
	}

	//we need to first make sure the string is composed UTF8
	//allocate at most twice the length plus one ie. assume worst case to escape every character
	outRegExp = (char *) calloc( 1, 1 + 2*strlen(inString) );

	j = 0;
	inLength = strlen(inString);
	for (i=0; i< inLength; i++)
	{
		//workaround since ni_search cannot handle UTF-8 characters
		//regex used by ni_search also has problems with ')' and '('
		if ( (isascii(inString[i])) && ( (inString[i]) != '(' ) && ( (inString[i]) != ')' ) && ( (inString[i]) != '{' ) && ( (inString[i]) != '}' ) )
		{
			if (isalnum(inString[i]) || isspace(inString[i]) || (inString[i]) == '_' || (inString[i]) == '-')
			{
				memcpy(outRegExp+j,&inString[i],1);
				j++;
			}
			else
			{
				memcpy(outRegExp+j,"\\",1);
				j++;
				memcpy(outRegExp+j,& inString[i],1);
				j++;
			}
		}
		else
		{
			memcpy(outRegExp+j,".",1);
			j++;
		}
	}

	return( outRegExp );

} // BuildRegExp


// ---------------------------------------------------------------------------
//	* BuildMultiRegExp
// ---------------------------------------------------------------------------

char* CNiPlugIn:: BuildMultiRegExp ( CAttributeList *inPattMatchList )

{
	char		   *outRegExp	= nil;
	char		  **regExpStrs  = nil;
	unsigned long  	i			= 0;
	unsigned long	j			= 0;
	unsigned long	inLength	= 0;
	uInt32			lenOfStrs   = 0;
	char		   *loopString  = nil;
	uInt32			partCount   = 0;
	uInt32			idx			= 0;

	if ( (inPattMatchList == nil) || (inPattMatchList->GetCount() < 1) )
	{
		return( outRegExp );
	}
	
	if (inPattMatchList->GetCount() == 1)
	{
		char *aStringValue = nil;
		if (inPattMatchList->GetAttribute(1, &aStringValue) == eDSNoErr)
		{
			return(BuildRegExp(aStringValue));
		}
		else
		{
			return(nil);
		}
	}
	
	partCount = inPattMatchList->GetCount();
	regExpStrs = (char **)calloc( partCount + 1, sizeof(char *) );

	for (idx = 0; idx < partCount; idx++)
	{
		if ( eDSNoErr == inPattMatchList->GetAttribute(idx+1, &loopString) )
		{
			inLength = strlen(loopString);
			//we need to first make sure the string is composed UTF8
			//allocate at most twice the length plus one ie. assume worst case to escape every character
			regExpStrs[idx] = (char *) calloc( 1, 1 + 2*inLength );

			j = 0;
			for (i=0; i< inLength; i++)
			{
				//workaround since ni_search cannot handle UTF-8 characters
				//regex used by ni_search also has problems with ')' and '('
				if ( (isascii(loopString[i])) && ( (loopString[i]) != '(' ) && ( (loopString[i]) != ')' ) && ( (loopString[i]) != '{' ) && ( (loopString[i]) != '}' ) )
				{
					if (isalnum(loopString[i]) || isspace(loopString[i]) || (loopString[i]) == '_' || (loopString[i]) == '-')
					{
						memcpy(regExpStrs[idx]+j,&loopString[i],1);
						j++;
					}
					else
					{
						memcpy(regExpStrs[idx]+j,"\\",1);
						j++;
						memcpy(regExpStrs[idx]+j,& loopString[i],1);
						j++;
					}
				}
				else
				{
					memcpy(regExpStrs[idx]+j,".",1);
					j++;
				}
			}
			lenOfStrs += strlen(regExpStrs[idx]);
		}
	}

	//build the output with the parts
	outRegExp = (char *)calloc(1, lenOfStrs + partCount);
	strcpy(outRegExp,regExpStrs[0]);
	free(regExpStrs[0]);
	for (idx = 1; idx < partCount; idx++)
	{
		strcat(outRegExp,"|");
		strcat(outRegExp,regExpStrs[idx]);
		free(regExpStrs[idx]);
	}
	free(regExpStrs);

	return( outRegExp );

} // BuildMultiRegExp


// ---------------------------------------------------------------------------
//	* ContinueDeallocProc
// ---------------------------------------------------------------------------

void CNiPlugIn::ContinueDeallocProc ( void* inContinueData )
{
	sNIContinueData *pContinue = (sNIContinueData *)inContinueData;

	if ( pContinue != nil )
	{
	// fAuthHndl is only used in DoMultiPassAuth
	// and it is freed? there by timSessionEnd
		if ( pContinue->fDataBuff != nil )
		{
			::dsDataBufferDeallocatePriv( pContinue->fDataBuff );
			pContinue->fDataBuff = nil;
		}
		
		if ( pContinue->fNIEntryList.ni_entrylist_len > 0 )
		{
			::ni_entrylist_free(&(pContinue->fNIEntryList));
			pContinue->fNIEntryList.ni_entrylist_len = 0;
		}
		
		free( pContinue );
		pContinue = nil;
	}
} // ContinueDeallocProc


// ---------------------------------------------------------------------------
//	* ContextDeallocProc
// ---------------------------------------------------------------------------

void CNiPlugIn::ContextDeallocProc ( void* inContextData )
{
	sNIContextData *pContext = (sNIContextData *) inContextData;

	if ( pContext != nil )
	{
		CleanContextData( pContext );
		
		free( pContext );
		pContext = nil;
	}
} // ContextDeallocProc


// ---------------------------------------------------------------------------
//	* IsValidRealname
// ---------------------------------------------------------------------------

sInt32 CNiPlugIn::IsValidRealname ( char   *inRealname,
									void   *inDomain,
									char  **outRecName )
{
	sInt32			siResult	= eDSInvalidRecordName;
	ni_id			niDirID;
	ni_status		niStatus	= NI_OK;
	char		   *normalizedRealname		= nil;
	char		   *inConstRegExpRecName	= nil;
	ni_entrylist	niEntryList;
	u_int			en			= 0;
	ni_entry		niEntry;
	ni_proplist		niPropList;
	ni_index		niIndex		= 0;
	ni_index		niIndexComp	= 0;

	gNetInfoMutex->Wait();

	NI_INIT( &niDirID );
	NI_INIT( &niEntry );
	NI_INIT( &niPropList );
	NI_INIT( &niEntryList );
	
	niStatus = ::ni_pathsearch( inDomain, &niDirID, "/users" );
	if ( niStatus == NI_OK )
	{
		//make the reg exp that ni_search needs
		normalizedRealname = NormalizeNIString(inRealname, "realname");
		inConstRegExpRecName = BuildRegExp(normalizedRealname);
		niStatus = ::ni_search( inDomain, &niDirID, (char *)"realname", inConstRegExpRecName, REG_ICASE, &niEntryList );
		DSFreeString(inConstRegExpRecName);
		if ( niStatus == NI_OK )
		{
			for ( en = 0; en < niEntryList.ni_entrylist_len; en++ )
			{
				niEntry = niEntryList.ni_entrylist_val[ en ];
				niDirID.nii_object = niEntry.id;

				niStatus = ::ni_read( inDomain, &niDirID, &niPropList );
				if ( niStatus == NI_OK )
				{
					niIndex = ::ni_proplist_match( niPropList, "realname", NULL );
					if ( niIndex != NI_INDEX_NULL )
					{
						//determine if the regcomp operation on inConstRegExpRecName for NetInfo actually
						//returned what was requested ie. it will always return at least it but probably much more
						//so filter out the much more
						if (DoesThisMatch( (const char*)(niPropList.nipl_val[ niIndex ].nip_val.ninl_val[ 0 ]),
											(const char*)normalizedRealname, eDSExact ))
						{
							//get the record name now that we found a match
							niIndexComp = ::ni_proplist_match( niPropList, "name", NULL );
							if ( niIndexComp != NI_INDEX_NULL )
							{
								//pass out the record name value
								*outRecName = (char *) calloc( 1, 1 + strlen(niPropList.nipl_val[ niIndexComp ].nip_val.ninl_val[ 0 ]) );
								strcpy(*outRecName,niPropList.nipl_val[ niIndexComp ].nip_val.ninl_val[ 0 ]);
								siResult = eDSNoErr;
								::ni_proplist_free( &niPropList );
								break;
							}
						}
					} // if ( niIndex != NI_INDEX_NULL )
					::ni_proplist_free( &niPropList );
				}
			}// loop over possible matches
			
			if (niEntryList.ni_entrylist_len > 0)
			{
				::ni_entrylist_free( &niEntryList );
				niEntryList.ni_entrylist_len = 0;
			}
		
		} // found some realname matches using ni_search
		if ( normalizedRealname != inRealname )
		{
			DSFreeString( normalizedRealname );
		}
	} //if ( niStatus == NI_OK ) for ::ni_pathsearch( inDomain, &niDirID, "/users" );

	gNetInfoMutex->Signal();

	return( siResult );

} // IsValidRealname


// ---------------------------------------------------------------------------
//	* GetUserNameForUID
// ---------------------------------------------------------------------------

char* CNiPlugIn::GetUserNameForUID ( uid_t   inUserID,
									 void   *inDomain )
{
	ni_id			niDirID;
	ni_status		niStatus	= NI_OK;
	ni_proplist		niPropList;
	ni_entry		niEntry;
	ni_entrylist	niEntryList;
	ni_namelist		niValues;
	char			idStr[ 64 ] = { 0 };
	u_int			en			= 0;
	char*			userName	= NULL;

	if ( inDomain == NULL ) {
		return NULL;
	}

	gNetInfoMutex->Wait();

	NI_INIT( &niDirID );
	NI_INIT( &niEntry );
	NI_INIT( &niPropList );
	NI_INIT( &niEntryList );
	NI_INIT( &niValues );

	niStatus = ::ni_pathsearch( inDomain, &niDirID, "/users" );

	if (niStatus == NI_OK) {
		// Do the search
		//make the reg exp that ni_search needs
		snprintf( idStr, 64, "%d", inUserID );
		niStatus = ni_search( inDomain, &niDirID, "uid", idStr, REG_BASIC, &niEntryList );
		if ( niStatus == NI_OK )
		{
			for ( en = 0; en < niEntryList.ni_entrylist_len; en++ )
			{
				niEntry = niEntryList.ni_entrylist_val[ en ];
				niDirID.nii_object = niEntry.id;

				niStatus = ni_lookupprop( inDomain, &niDirID, "name", &niValues );

				if ( niStatus == NI_OK )
				{
					if (niValues.ni_namelist_len > 0)
					{
						if (niValues.ni_namelist_val[0] != NULL)
						{
							userName = strdup( niValues.ni_namelist_val[0] );
							ni_namelist_free( &niValues );
							break;
						}
						ni_namelist_free( &niValues );
					}
				}
			}
		} //ni_search successful
		if ( niEntryList.ni_entrylist_len > 0 )
		{
			::ni_entrylist_free(&niEntryList);
			niEntryList.ni_entrylist_len = 0;
		}
	}

	gNetInfoMutex->Signal();

	return userName;
}


// ---------------------------------------------------------------------------
//	* GetNetInfoAuthAuthorityHandler
// ---------------------------------------------------------------------------

NetInfoAuthAuthorityHandlerProc CNiPlugIn::GetNetInfoAuthAuthorityHandler ( const char* inTag )
{
	if (inTag == NULL)
	{
		return NULL;
	}
	for (unsigned int i = 0; i < kNetInfoAuthAuthorityHandlerProcs; ++i)
	{
		if (strcasecmp(inTag,sNetInfoAuthAuthorityHandlerProcs[i].fTag) == 0)
		{
			// found it
			return sNetInfoAuthAuthorityHandlerProcs[i].fHandler;
		}
	}
	return NULL;
}


//------------------------------------------------------------------------------------
//	* FindNodeForSearchPolicyAuthUser
//------------------------------------------------------------------------------------

tDataList* CNiPlugIn::FindNodeForSearchPolicyAuthUser ( const char *userName )
{
	sInt32					siResult		= eDSNoErr;
	sInt32					returnVal		= -3;
	unsigned long			nodeCount		= 0;
	unsigned long			recCount		= 0;
	tContextData			context			= NULL;
	tDataListPtr			nodeName		= NULL;
	tDataListPtr			recName			= NULL;
	tDataListPtr			recType			= NULL;
	tDataListPtr			attrTypes		= NULL;
	tDataBufferPtr			dataBuff		= NULL;
	tRecordEntry		   *pRecEntry		= nil;
	tAttributeListRef		attrListRef		= 0;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeEntry		   *pAttrEntry		= nil;
	tAttributeValueEntry   *pValueEntry		= nil;
	tDirReference			aDSRef			= 0;
	tDirNodeReference		aSearchNodeRef	= 0;
	
	try
	{
		siResult = dsOpenDirService( &aDSRef );
		if ( siResult != eDSNoErr ) throw( siResult );

		dataBuff = dsDataBufferAllocate( aDSRef, 2048 );
		if ( dataBuff == nil ) throw( eMemoryAllocError );
		
		siResult = dsFindDirNodes( aDSRef, dataBuff, nil, 
									eDSAuthenticationSearchNodeName, &nodeCount, &context );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = dsGetDirNodeName( aDSRef, dataBuff, 1, &nodeName );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = dsOpenDirNode( aDSRef, nodeName, &aSearchNodeRef );
		if ( siResult != eDSNoErr ) throw( siResult );
		if ( nodeName != NULL )
		{
			dsDataListDeallocate( aDSRef, nodeName );
			free( nodeName );
			nodeName = NULL;
		}

		recName		= dsBuildListFromStrings( aDSRef, userName, NULL );
		recType		= dsBuildListFromStrings( aDSRef, kDSStdRecordTypeUsers, NULL );
		attrTypes   = dsBuildListFromStrings( aDSRef, kDSNAttrMetaNodeLocation, NULL );

		recCount = 1; // only care about first match
		do 
		{
			siResult = dsGetRecordList( aSearchNodeRef, dataBuff, recName, eDSExact, recType,
										attrTypes, false, &recCount, &context);
			if (siResult == eDSBufferTooSmall)
			{
				uInt32 bufSize = dataBuff->fBufferSize;
				dsDataBufferDeallocatePriv( dataBuff );
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
				//index starts at one - should have one entry
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
	}
	
	catch( sInt32 err )
	{
		returnVal = err;
	}	
	
	if ( recName != NULL )
	{
		dsDataListDeallocate( aDSRef, recName );
		free( recName );
		recName = NULL;
	}
	if ( recType != NULL )
	{
		dsDataListDeallocate( aDSRef, recType );
		free( recType );
		recType = NULL;
	}
	if ( attrTypes != NULL )
	{
		dsDataListDeallocate( aDSRef, attrTypes );
		free( attrTypes );
		attrTypes = NULL;
	}
	if ( dataBuff != NULL )
	{
		dsDataBufferDeAllocate( aDSRef, dataBuff );
		dataBuff = NULL;
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
	
	return nodeName;
} // FindNodeForSearchPolicyAuthUser


sInt32 CNiPlugIn::GetSubNodes ( sNIContextData *inContext, set<string> & nodeNames )
{
	sInt32			siResult		= 0;
	sInt32			entryListCnt	= 0;
	sInt32			entryListLen	= 0;
	sInt32			nameListCnt		= 0;
	sInt32			nameListlen		= 0;
	ni_status		niStatus		= NI_OK;
	bool			isLocal			= false;
	char		   *name			= nil;
	char		   *p				= nil;
	ni_namelist	   *nameList		= nil;
	ni_id			machines;
	ni_id			niDirID;
	ni_entrylist	niEntryList;
	char		   *domName			= nil;
	char		   *tempName		= nil;
	bool			bNodeServes		= false;
	uInt32			pv				= 0;
	char		   *aNIDomainName	= nil;

	do
	{		
		NI_INIT( &niDirID );

		gNetInfoMutex->Wait();

		void *aNIDomain = RetrieveNIDomain(inContext);
		if (aNIDomain != NULL)
		{
			if (inContext->fDomainName != nil)
			{
				aNIDomainName = strdup(inContext->fDomainName);
			}
			do
			{
				NI_INIT( &machines );
				siResult = ni_pathsearch( aNIDomain, &machines, "/machines" );
				if ( siResult != eDSNoErr )
				{
					break;
				}

				NI_INIT( &niEntryList );
				niStatus = ni_list( aNIDomain, &machines, "serves", &niEntryList );
				if ( niStatus != NI_OK )
				{
					break;
				}
			}
			while (false);
		}
		else
		{
			niStatus = (ni_status)eDSInvalidSession;
		}
		gNetInfoMutex->Signal();

		if ( niStatus != 0 ) break;

		entryListLen = niEntryList.ni_entrylist_len;

		for ( entryListCnt = 0; entryListCnt < entryListLen; entryListCnt++ )
		{
			//check here for machines that serve themselves
			bNodeServes = false;
			nameList = niEntryList.ni_entrylist_val[ entryListCnt ].names;

			//eliminate nodes that ONLY serve themselves
			if (nameList != NULL)
			{
				// For each value in the namelist for this property
				for ( pv = 0; pv < nameList->ni_namelist_len; pv++ )
				{
					if (nil == strstr(nameList->ni_namelist_val[ pv ],"/local"))
					{
						bNodeServes = true;
						break;
					}
				}
			}
			
			if (!bNodeServes)
			{
				continue;
			}

			if ( nameList == nil || nameList->ni_namelist_len == 0 )
			{
				continue;
			}

			nameListlen = nameList->ni_namelist_len;
			for ( nameListCnt = 0; nameListCnt < nameListlen; nameListCnt++ )
			{
				name = nameList->ni_namelist_val[ nameListCnt ];
				if ( ::strncmp( name, "./", 2 ) == 0 
					 || ::strncmp( name, "../", 3 ) == 0 )
				{
					continue;
				}

				isLocal = ::strstr( name, "/local" ) != nil;

				p = strchr( name, '/' );
				if ( p == nil )
				{
					continue;
				}
				*p = '\0';
				
				if (domName != nil)
				{
					free(domName);
					domName = nil;
				}
				if (aNIDomainName != nil)
				{
					domName = (char *)calloc(1, strlen(aNIDomainName) + 1 + strlen(name) + 1);
					strcpy(domName, aNIDomainName);
					if (strlen(domName) > 1)
					{
						strcat(domName,"/");
					}
					strcat(domName,name);

					if (isLocal == false)
					{
						string aString(domName);
						nodeNames.insert(aString);
					}
				}
			}
			if (tempName != nil)
			{
				free(tempName);
				tempName = nil;
			}
			if (domName != nil)
			{
				free(domName);
				domName = nil;
			}
		}
		
		ni_entrylist_free(&niEntryList);
	}
	while (false);
	
	return( siResult );

} // GetSubNodes

