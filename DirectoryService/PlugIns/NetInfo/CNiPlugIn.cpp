/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <ctype.h>
#include <regex.h>
#include <syslog.h>		// for syslog() to log calls

#include "CNiPlugIn.h"
#include "CNetInfoPI.h"
#include "SMBAuth.h"
#include "CBuff.h"
#include "CSharedData.h"
#include "CString.h"
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryServiceCore/CFile.h>
#include "NiLib2.h"
#include "NiLib3.h"
#include "DSUtils.h"
#include "DirServicesConst.h"
#include "DirServices.h"
#include "DirServicesUtils.h"
#include "ServerModuleLib.h"
#include "CRecTypeList.h"
#include "CRecNameList.h"
#include "SharedConsts.h"
#include "PrivateTypes.h"
#include "CPlugInRef.h"
#include "CContinue.h"
#include "CRCCalc.h"
#include "CDataBuff.h"
#include "TimConditional.h"
#include "my_ni_pwdomain.h"

// Auth server
#ifdef TIM_CLIENT_PRESENT
#include <TimClient/TimClient.h>
#endif

// -- Typedef's -------------------------------------

#define		kAttrConsts		76

//re-ordered more closely in terms of frequency of use
static const char *sAttrMap[ kAttrConsts ][ 2 ] =
{
	/*  1 */	{ kDSNAttrRecordName,			"name" },
	/*    */	{ kDS1AttrDistinguishedName,	"realname" },
	/*    */	{ kDS1AttrPassword,				"passwd" },
	/*    */	{ kDS1AttrPasswordPlus,			"passwd" },  //forward mapping only as reverse mapping uses entry above
	/*  5 */	{ kDS1AttrUniqueID,				"uid" },
	/*    */	{ kDS1AttrPrimaryGroupID,		"gid" },
	/*    */	{ kDS1AttrUserShell,			"shell" },
	/*    */	{ kDS1AttrNFSHomeDirectory,		"home" },
	/*    */	{ kDSNAttrAuthenticationAuthority, "authentication_authority" },
	/* 10 */	{ kDSNAttrHomeDirectory,		"home_loc" }, // or HomeLocation
	/*    */	{ kDS1StandardAttrHomeLocOwner,	"home_loc_owner" },
	/*    */	{ kDS1AttrHomeDirectoryQuota,	"homedirectoryquota" },
	/*    */	{ kDS1AttrPicture,				"picture" },
	/*    */	{ kDS1AttrInternetAlias,		"InetAlias" },
	/* 15 */	{ kDS1AttrMailAttribute,		"applemail" },
	/*    */	{ kDS1AttrAuthenticationHint,	"hint" },
	/*    */	{ kDS1AttrRARA,					"RARA" },
	/*    */	{ kDS1AttrGeneratedUID,			"GeneratedUID" },
	/*    */	{ kDSNAttrGroupMembership,		"users" },
	/* 20 */	{ kDSNAttrEMailAddress,			"mail" },
	/*    */	{ kDSNAttrURL,					"URL" },
	/*    */	{ kDSNAttrURLForNSL,			"URL" },
	/*    */	{ kDSNAttrMIME,					"mime" },
	/*    */	{ kDSNAttrHTML,					"htmldata" },
	/* 25 */	{ kDSNAttrNBPEntry,				"NBPEntry" },
	/*    */	{ kDSNAttrDNSName,				"dnsname" },
	/*    */	{ kDSNAttrIPAddress,			"IP_Address" },
	/*    */	{ kDS1AttrENetAddress,			"en_address" },
	/*    */	{ kDSNAttrComputers,			"computers" },
	/* 30 */	{ kDS1AttrMCXFlags,				"mcx_flags" },
	/*    */	{ kDS1AttrMCXSettings,			"mcx_settings" },
	/*    */	{ kDS1AttrDataStamp,			"data_stamp" },
	/*    */	{ kDS1AttrPrintServiceInfoText,	"PrintServiceInfoText" },
	/*    */	{ kDS1AttrPrintServiceInfoXML,	"PrintServiceInfoXML" },
	/* 35 */	{ kDS1AttrPrintServiceUserData,	"appleprintservice" },
	/*    */	{ kDS1AttrVFSType,				"vfstype" },
	/*    */	{ kDS1AttrVFSPassNo,			"passno" },
	/*    */	{ kDS1AttrVFSDumpFreq,			"dump_freq" },
	/*    */	{ kDS1AttrVFSLinkDir,			"dir" },
	/* 40 */	{ kDSNAttrVFSOpts,				"opts" },
	/*    */	{ kDS1AttrAliasData,			"alias_data" },
	/*    */	{ kDSNAttrPhoneNumber,			"phonenumber" },
	/*    */	{ kDS1AttrCapabilities,			"capabilities" },
	/*    */	{ kDSNAttrProtocols,			"protocols" },
	/* 45 */	{ kDSNAttrMember,				"users" },
	/*    */	{ kDSNAttrAllNames,				"dsAttrTypeStandard:AllNames" },
	/*    */	{ kStandardTargetAlias,			"dsAttrTypeStandard:AppleMetaAliasTarget" },
	/*    */	{ kStandardSourceAlias,			"dsAttrTypeStandard:AppleMetaAliasSource" },
	/*    */	{ kDSNAttrMetaNodeLocation,		"dsAttrTypeStandard:AppleMetaNodeLocation" },
	/* 50 */	{ kDS1AttrComment,				"comment" },
	/*    */	{ kDS1AttrAdminStatus,			"AdminStatus" },
	/*    */	{ kDS1AttrAdminLimits,			"admin_limits" },
	/*    */	{ kDS1AttrPwdAgingPolicy,		"PwdAgingPolicy" },
	/*    */	{ kDS1AttrChange,				"change" },
	/* 55 */	{ kDS1AttrExpire,				"expire" },
	/*    */	{ kDSNAttrGroup,				"groups" },
	/*    */	{ kDS1AttrFirstName,			"firstname" },
	/*    */	{ kDS1AttrMiddleName,			"middlename" },
	/*    */	{ kDS1AttrLastName,				"lastname" },
	/* 60 */	{ kDSNAttrAreaCode ,			"areacode" },
	/*    */	{ kDSNAttrAddressLine1,			"address1" },
	/*    */	{ kDSNAttrAddressLine2,			"address2" },
	/*    */	{ kDSNAttrAddressLine3,			"address3" },
	/*    */	{ kDSNAttrCity,					"city" },
	/* 65 */	{ kDSNAttrState,				"state" },
	/*    */	{ kDSNAttrPostalCode,			"zip" },
	/*    */	{ kDSNAttrOrganizationName,		"orgname" },
	/*    */	{ kDS1AttrSetupOccupation,		"occupation" },
	/*    */	{ kDS1AttrSetupLocation,		"location" },
	/* 70 */	{ kDS1AttrSetupAdvertising,		"spam" },
	/*    */	{ kDS1AttrSetupAutoRegister,	"autoregister" },
	/*    */	{ kDS1AttrPresetUserIsAdmin,	"preset_user_is_admin" },
	/*    */	{ kDS1AttrPasswordServerLocation, "passwordserverlocation" },
	/*    */	{ kDSNAttrBootParams,			"bootparams" },
	/* 75 */	{ kDSNAttrNetGroups,			"netgroups" },
	/* 76 */	{ kDSNAttrRecordAlias,			"RecordAlias" }	// <- go away?
};


#define		kRecConsts		37

static const char *sRecMap[ kRecConsts ][ 2 ] =
{
	/*  1 */
				{ kDSStdRecordTypeUsers,			"users" },
				{ kDSStdRecordTypeUserAliases,		"user_aliases" },
				{ kDSStdRecordTypeGroups,			"groups" },
				{ kDSStdRecordTypeGroupAliases,		"group_aliases" },
	/*  5 */
				{ kDSStdRecordTypeMachines,			"machines" },
				{ kDSStdRecordTypeComputers,		"computers" },	
				{ kDSStdRecordTypeComputerLists,	"computer_lists" },
				{ kDSStdRecordTypePrinters,			"printers" },
				{ kDSStdRecordTypeHosts,			"machines" },
	/* 10 */
				{ kDSStdRecordTypeAliases,			"aliases" },
				{ kDSStdRecordTypeNetworks,			"networks" },
				{ kDSStdRecordTypeServer,			"servers" },
				{ kDSStdRecordTypeWebServer,   		"httpservers" },
				{ kDSStdRecordTypeFTPServer,		"ftpservers" },
	/* 15 */
				{ kDSStdRecordTypeAFPServer,		"afpservers" },
				{ kDSStdRecordTypeLDAPServer,		"ldapservers" },
				{ kDSStdRecordTypeNFS,				"mounts" },
				{ kDSStdRecordTypeServices,			"services" },
				{ kDSStdRecordTypePrintService,		"PrintService" },
	/* 20 */
				{ kDSStdRecordTypeConfig,			"config" },
				{ kDSStdUserNamesMeta,				"dsRecTypeStandard:MetaUserNames" },
				{ kDSStdRecordTypeMeta,				"dsRecTypeStandard:AppleMetaRecord" },
				{ "dsRecTypeStandard:UsreNames",	"users" },
				{ kDSStdRecordTypeAFPUserAliases,	"afpuser_aliases" },
	/* 25 */
				{ kDSStdRecordTypeMounts,			"mounts" },
				{ kDSStdRecordTypePrintServiceUser,	"printserviceusers" },
				{ kDSStdRecordTypePresetUsers,		"presets_users" },
				{ kDSStdRecordTypePresetGroups,		"presets_groups" },
				{ kDSStdRecordTypePresetComputerLists,	"presets_computer_lists" },
	/* 30 */
				{ kDSStdRecordTypeHosts,			"hosts" },
				{ kDSStdRecordTypeProtocols,		"protocols" },
				{ kDSStdRecordTypeRPC,				"rpcs" },
				{ kDSStdRecordTypeBootp,			"bootp" },
				{ kDSStdRecordTypeNetDomains,		"netdomains" },
	/* 35 */
				{ kDSStdRecordTypeEthernets,		"ethernets" },
				{ kDSStdRecordTypeNetGroups,		"netgroups" },
				{ kDSStdRecordTypeHostServices,		"hostservices" },
};

typedef struct AuthAuthorityHandler {
	char* fTag;
	AuthAuthorityHandlerProc fHandler;
} AuthAuthorityHandler;

#define		kAuthAuthorityHandlerProcs		3

static AuthAuthorityHandler sAuthAuthorityHandlerProcs[ kAuthAuthorityHandlerProcs ] =
{
	{ "basic",					(AuthAuthorityHandlerProc)CNiPlugIn::DoBasicAuth },
	{ "LocalWindowsHash",		(AuthAuthorityHandlerProc)CNiPlugIn::DoLocalWindowsAuth },
	{ "ApplePasswordServer",	(AuthAuthorityHandlerProc)CNiPlugIn::DoPasswordServerAuth }
};

// --------------------------------------------------------------------------------
//	Globals

CPlugInRef	 		*gNINodeRef		= nil;
CContinue	 		*gNIContinue	= nil;
time_t				 gCheckTimAgainTime	= 0;
bool				 gTimIsRunning		= false;


static const uInt32		kNodeInfoBuffTag	= 'NInf';


// Enums -----------------------------------------------------------------------------

typedef	enum
{
	eHasAuthMethod			= -7000,
	eNoAuthServer			= -7001,
	eNoAuthMethods			= -7002,
	eAuthMethodNotFound		= -7003
} eAuthValues;


// Consts ----------------------------------------------------------------------------

static const	uInt32	kBuffPad	= 16;

//------------------------------------------------------------------------------------
//	* CNiPlugIn
//------------------------------------------------------------------------------------

CNiPlugIn::CNiPlugIn ( void )
{
	if ( gNINodeRef == nil )
	{
		gNINodeRef = new CPlugInRef( CNiPlugIn::ContextDeallocProc );
	}

	if ( gNIContinue == nil )
	{
		gNIContinue = new CContinue( CNiPlugIn::ContinueDeallocProc );
	}

	fRecData	= new CDataBuff();
	fAttrData	= new CDataBuff();
	fTmpData	= new CDataBuff();
	
} // CNiPlugIn


//------------------------------------------------------------------------------------
//	* InitService
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

		case kDoDirNodeAuth:
			siResult = DoAuthentication( (sDoDirNodeAuth *)inData );
			break;

		case kDoAttributeValueSearch:
		case kDoAttributeValueSearchWithData:
			siResult = DoAttributeValueSearch( (sDoAttrValueSearchWithData *)inData );
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


//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::OpenDirNode ( sOpenDirNode *inData )
{
	sInt32			siResult		= eDSNoErr;
	char		   *nameStr			= nil;
	char		   *pathStr			= nil;
	char		   *domName			= nil;
	void		   *domain			= nil;
	sInt32			timeOutSecs		= 3;
	sNIContextData   *pContext		= nil;
	ni_id			niRootDir;

	try
	{
		nameStr = ::dsGetPathFromListPriv( inData->fInDirNodeName, kstrDelimiter );
		if ( nameStr == nil ) throw( (sInt32)eDSNullNodeName );

		pathStr = BuildDomainPathFromName( nameStr );
		if ( pathStr == nil ) throw( (sInt32)eDSUnknownNodeName );

		siResult = CNetInfoPI::SafeOpen( pathStr, timeOutSecs, &niRootDir, &domain, &domName );
		if ( siResult != eDSNoErr ) throw( siResult );

		if (::strcmp(domName,"") == 0)
		{
			siResult = eDSOpenNodeFailed;
			if ( siResult != eDSNoErr ) throw( siResult );
		}
		
		pContext = MakeContextData();
		if ( pContext == nil ) throw( (sInt32)eMemoryAllocError );

		pContext->fDomain = domain;
		pContext->fDomainName = domName; //we own this char* now
		pContext->fUID = inData->fInUID;
		pContext->fEffectiveUID = inData->fInEffectiveUID;

		gNINodeRef->AddItem( inData->fOutNodeRef, pContext );
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( pathStr != nil )
	{
		free( pathStr );
		pathStr = nil;
	}
	if ( nameStr != nil )
	{
		free( nameStr );
		nameStr = nil;
	}

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

						timHndl = ::timServerForDomain( pContext->fDomain );
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
				// Add the attribute length and data
				fAttrData->AppendLong( fTmpData->GetLength() );
				fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

				// Clear the temp block
				fTmpData->Clear();
			}
				 
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDSNAttrNodePath ) == 0) )
			{
				delayedNI = time(nil) + 2; //normally my_ni_pwdomain will complete in under 2 secs
				siResult = ::my_ni_pwdomain( pContext->fDomain, &niDomNameStr );
				if ( delayedNI < time(nil) )
				{
					syslog(LOG_INFO,"GetDirNodeInfo::Call to my_ni_pwdomain was with argument domain name: %s and lasted %d seconds.", pContext->fDomain, (uInt32)(2 + time(nil) - delayedNI));
				}
				//assume that if siResult is an error then niDomNameStr is invalid
				if ( ( siResult != eDSNoErr ) || (niDomNameStr == nil) )
				{
					syslog(LOG_INFO,"GetDirNodeInfo::Call to my_ni_pwdomain failed or returned nil name.");
					//here we force success from the my_ni_pwdomain call
					niDomNameStr = (char *) calloc(1,2);
					strcpy(niDomNameStr,"/");
				}
				if (niDomNameStr != nil)
				{
					fTmpData->Clear();

					uiAttrCnt++;

					// Append the attribute name
					fTmpData->AppendShort( ::strlen( kDSNAttrNodePath ) );
					fTmpData->AppendString( kDSNAttrNodePath );

					if ( inData->fInAttrInfoOnly == false )
					{
						// Is this is the root node
						if ( ::strcmp( niDomNameStr, "/" ) == 0 )
						{
							// Attribute count of 1
							fTmpData->AppendShort( 2 );

							// Add the kstrNetInfoName as an attribute value
							fTmpData->AppendLong( ::strlen( kstrNetInfoName ) );
							fTmpData->AppendString( kstrNetInfoName );

							// Add the kstrRootOnly as an attribute value
							fTmpData->AppendLong( ::strlen( kstrRootOnly ) );
							fTmpData->AppendString( kstrRootOnly );
						}
						else
						{
							// Attribute count of (list count) + 2

							pNodePath = ::dsBuildFromPathPriv( niDomNameStr, "/" );
							if ( pNodePath == nil ) throw( (sInt32)eMemoryAllocError );

							fTmpData->AppendShort( ::dsDataListGetNodeCountPriv( pNodePath ) + 2 );

							// Add the kstrNetInfoName as an attribute value
							fTmpData->AppendLong( ::strlen( kstrNetInfoName ) );
							fTmpData->AppendString( kstrNetInfoName );

							// Add the kstrRootOnly as an attribute value
							fTmpData->AppendLong( ::strlen( kstrRootOnly ) );
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
					// Add the attribute length and data
					fAttrData->AppendLong( fTmpData->GetLength() );
					fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

					// Clear the temp block
					fTmpData->Clear();
					
					free(niDomNameStr);
					niDomNameStr = nil;
				}
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
						siResult = GetAllRecords( pNIRecType,
													cpAttrTypeList,
													pContinue,
													pContext->fDomain,
													pContext->fDomainName,
													bAttribOnly,
													outBuff,
													uiCount );
					}
					else
					{
						siResult = GetTheseRecords ( pRecName,
													pRecType,
													pNIRecType,
													pattMatch,
													cpAttrTypeList,
													pContext->fDomain,
													pContext->fDomainName,
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

sInt32 CNiPlugIn::GetAllRecords (	char			*inNI_RecType,
									CAttributeList	*inAttrTypeList,
									sNIContinueData	*inContinue,
									void			*inDomain,
									char			*inDomainName,
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

	gNetInfoMutex->Wait();

	try
	{
		NI_INIT( &niDirID );
		NI_INIT( &niEntry );
		NI_INIT( &niPropList );
		NI_INIT( &niEntryList );

		outRecCount = 0; //need to track how many records were found by this call to GetAllRecords

		//assuming that record types have no embedded "/"s in them
		siResult = ::ni_pathsearch( inDomain, &niDirID, inNI_RecType );
		if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

		siResult = ::ni_list( inDomain, &niDirID, "name", &niEntryList );
		if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

		for ( en = inContinue->fAllRecIndex; (en < niEntryList.ni_entrylist_len) &&
					(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
					 (inContinue->fLimitRecSearch == 0)); en++ )
		{
			niEntry = niEntryList.ni_entrylist_val[ en ];
			niDirID.nii_object = niEntry.id;

			siResult = ::ni_read( inDomain, &niDirID, &niPropList );
			if ( siResult == NI_OK )
			{
				siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, inDomain, inDomainName, siValCnt );
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
		if ( niEntryList.ni_entrylist_len > 0 )
		{
			::ni_entrylist_free(&niEntryList);
		}
		if ( siResult == eDSNoErr )
		{
			inContinue->fAllRecIndex = 0;
		}
	}

	catch( sInt32 err )
	{
		if ( niEntryList.ni_entrylist_len > 0 )
		{
			::ni_entrylist_free(&niEntryList);
		}
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult ); 

} // GetAllRecords


//------------------------------------------------------------------------------------
//	* GetTheseRecords
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn:: GetTheseRecords (	char				*inConstRecName,
									char				*inConstRecType,
									char				*inNativeRecType,
									tDirPatternMatch	 inPattMatch,
									CAttributeList		*inAttrTypeList,
									void				*inDomain,
									char				*inDomainName,
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
	ni_proplist		niPropList;
	ni_entry		niEntry;
	ni_entrylist	niEntryList;
	bool			bGetThisOne	= true;
	char		   *inConstRegExpRecName	= nil;
	bool			bGotAMatch	= false;

	gNetInfoMutex->Wait();

	try
	{
		NI_INIT( &niDirID );
		NI_INIT( &niEntry );
		NI_INIT( &niPropList );
		NI_INIT( &niEntryList );

		outRecCount = 0; //need to track how many records were found by this call to GetTheseRecords

		//need to flush buffer so that if we retrieve multiple entries with one call to this routine they
		//will not get added to each other
		fRecData->Clear();

		//need to handle continue data now with the fMultiMapIndex field so that we know
		//where we left off on the last time in here
		//KW need to get rid of this index and consolidate the code together
		
		if ( (::strcmp( inNativeRecType, "users" ) == 0 ) &&
			((inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 2) || (inContinue->fMultiMapIndex == 3)) )

		{
			niStatus = ::ni_pathsearch( inDomain, &niDirID, "/users" );
			if ( niStatus == NI_OK )
			{
//no definitive mapping of record names yet ie. only hardcoded here
//KW how can we ensure that a search that "hits" both the name and realname will not return two found records when indeed
//there is only one record in NetInfo
//KW can this command handle a search on two separate attributes??? - not likely
				//make the reg exp that ni_search needs
				inConstRegExpRecName = BuildRegExp(inConstRecName);
				niStatus = ::ni_search( inDomain, &niDirID, (char *)"name", inConstRegExpRecName, REG_ICASE, &niEntryList );
				free(inConstRegExpRecName);
				if ( (niStatus == NI_OK) &&
					((inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 2)) )
				{
					for ( en = inContinue->fAllRecIndex; (en < niEntryList.ni_entrylist_len) &&
							(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
					 		(inContinue->fLimitRecSearch == 0)); en++ )
//					for ( en = inContinue->fAllRecIndex; en < niEntryList.ni_entrylist_len; en++ )
					{
						niEntry = niEntryList.ni_entrylist_val[ en ];
						niDirID.nii_object = niEntry.id;

						niStatus = ::ni_read( inDomain, &niDirID, &niPropList );
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

									siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, inDomain, inDomainName, siCount );
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
						} // if ( ::ni_read( inDomain, &niDirID, &niPropList == NI_OK ))
							
					} // for loop over niEntryList.ni_entrylist_len
				} // if ( ::ni_search( inDomain, &niDirID, (char *)"name", inConstRecName, REG_ICASE, &niEntryList ) == NI_OK )


				if (niEntryList.ni_entrylist_len > 0)
				{
					::ni_entrylist_free( &niEntryList );
				}
				NI_INIT( &niEntryList );

				//ensure that if a client only asks for n records that we stop searching if
				//we have already found n records ie. critical for performance when asking for one record
				if ( (inContinue->fTotalRecCount < inContinue->fLimitRecSearch) || (inContinue->fLimitRecSearch == 0) )
				{
					NI_INIT( &niDirID );
					niStatus = ::ni_pathsearch( inDomain, &niDirID, "/users" );
					if ( niStatus == NI_OK )
					{
						//make the reg exp that ni_search needs
						inConstRegExpRecName = BuildRegExp(inConstRecName);
						niStatus = ::ni_search( inDomain, &niDirID, (char *)"realname", inConstRegExpRecName, REG_ICASE, &niEntryList );
						free(inConstRegExpRecName);
						if ( (niStatus == NI_OK) &&
							((inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 3)) )
						{
							for ( en = inContinue->fAllRecIndex; (en < niEntryList.ni_entrylist_len) &&
									(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
									(inContinue->fLimitRecSearch == 0)); en++ )
	//						for ( en = inContinue->fAllRecIndex; en < niEntryList.ni_entrylist_len; en++ )
							{
								niEntry = niEntryList.ni_entrylist_val[ en ];
								niDirID.nii_object = niEntry.id;
	
								niStatus = ::ni_read( inDomain, &niDirID, &niPropList );
								if ( niStatus == NI_OK )
								{
									niIndex = ::ni_proplist_match( niPropList, "realname", NULL );
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
																		(const char*)inConstRecName,
																		inPattMatch ) )
													{
														//should have already gotten this one above in search on name
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
	
												siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, inDomain, inDomainName, siCount );
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
								} // if ( ::ni_read( inDomain, &niDirID, &niPropList == NI_OK ))
							} // for loop over niEntryList.ni_entrylist_len
						} // if ( ::ni_search( inDomain, &niDirID, (char *)"realname", inConstRecName, REG_ICASE, &niEntryList ) == NI_OK )
						if ( niEntryList.ni_entrylist_len > 0 )
						{
							::ni_entrylist_free(&niEntryList);
						}
					} // if (::ni_pathsearch( inDomain, &niDirID, "/users" ) == NI_OK)
				} //if the client limit on records to return wasn't reached check realname as well
			} // if ( ::ni_pathsearch( inDomain, &niDirID, "/users" ) == NI_OK )
		} // if ( ::strcmp( inNativeRecType, "users" ) == 0 )


		else if ( (inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 1) )
		{
			//check to see if this type exists
			niStatus = ::ni_pathsearch( inDomain, &niDirID, inNativeRecType );
			if ( niStatus == NI_OK )
			{
				//check to see if this record exists
				//make the reg exp that ni_search needs
				inConstRegExpRecName = BuildRegExp(inConstRecName);
				niStatus = ::ni_search( inDomain, &niDirID, (char *)"name", inConstRegExpRecName, REG_ICASE, &niEntryList );
				free(inConstRegExpRecName);
				if ( (niStatus == NI_OK) &&
					((inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 1)) )
				{
					for ( en = inContinue->fAllRecIndex; (en < niEntryList.ni_entrylist_len) &&
							(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
					 		(inContinue->fLimitRecSearch == 0)); en++ )

						{
							niEntry = niEntryList.ni_entrylist_val[ en ];
							niDirID.nii_object = niEntry.id;

							niStatus = ::ni_read( inDomain, &niDirID, &niPropList );
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

										siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, inDomain, inDomainName, siCount );
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
							} // if ( ::ni_read( inDomain, &niDirID, &niPropList == NI_OK ))
						} // for loop over niEntryList.ni_entrylist_len

				} // if ( (niStatus == NI_OK) && ((inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 1)) )
				if ( niEntryList.ni_entrylist_len > 0 )
				{
					::ni_entrylist_free(&niEntryList);
				}
			} // if ( niStatus == NI_OK )
		}// else if ( (inContinue->fMultiMapIndex == 0) || (inContinue->fMultiMapIndex == 1) )

		inContinue->fMultiMapIndex = 0;
		inContinue->fAllRecIndex = 0;

	} // try

	catch( sInt32 err )
	{
		if ( niEntryList.ni_entrylist_len > 0 )
		{
			::ni_entrylist_free(&niEntryList);
		}
		siResult = err;
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

						// Add the attribute length
						fAttrData->AppendLong( 	fTmpData->GetLength() );
						fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

						// Clear the temp block
						fTmpData->Clear();
					}
				}
				else
				{
					// Get all attributes
					if ( ( ::strcmp( kDSAttributesAll, pNI_AttrType ) == 0 ) ||
					     ( ::strcmp( kDSAttributesStandardAll, pNI_AttrType ) == 0 ) )
					{
						//code here is to add the attribute kDSNAttrMetaNodeLocation to
						// kDSAttributesAll and kDSAttributesStandardAll
						fTmpData->Clear();

						outCount++;

						// Append the attribute name
						fTmpData->AppendShort( ::strlen( kDSNAttrMetaNodeLocation ) );
						fTmpData->AppendString( kDSNAttrMetaNodeLocation );

						if ( inAttrOnly == false )
						{
							// Append the attribute value count
							fTmpData->AppendShort( 1 );

							fTmpData->AppendLong( ::strlen( inDomainName ) );
							fTmpData->AppendString( inDomainName );

							// Add the attribute length
							fAttrData->AppendLong( 	fTmpData->GetLength() );
							fAttrData->AppendBlock( fTmpData->GetData(), fTmpData->GetLength() );

							// Clear the temp block
							fTmpData->Clear();
						}
						
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

		// check access for non-admin users
		if ( ( pContext->fEffectiveUID != 0 )
			 && ( (pContext->fAuthenticatedUserName == NULL) 
				  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
		{
			authUser = pContext->fAuthenticatedUserName;
			if ( ( authUser == NULL ) && (strcmp(pAttrKey,"picture") == 0) )
			{
				authUser = GetUserNameForUID( pContext->fEffectiveUID, pContext->fDomain );
			}
			ni_proplist niPropList;
			siResult = ::ni_read( pContext->fDomain, &pContext->dirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			siResult = NiLib2::ValidateDir( authUser, &niPropList );
			::ni_proplist_free( &niPropList );

			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
		}

		NI_INIT( &niValues );
//		::ni_namelist_insert( &niValues, pAttrValue->fBufferData, NI_INDEX_NULL );
		::ni_namelist_insert( &niValues, pValue, NI_INDEX_NULL );
		siResult = DoAddAttribute( pContext->fDomain, &pContext->dirID, pAttrKey, niValues );
		::ni_namelist_free( &niValues );
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

		::memcpy( &niDirID, &pContext->dirID, sizeof( ni_id ) );

		NI_INIT( &niNameList );
		niStatus = ::ni_listprops( pContext->fDomain, &niDirID, &niNameList );
		if ( niStatus == NI_OK )
		{
			niIndex = ::ni_namelist_match( niNameList, pNI_AttrType );
			::ni_namelist_free( &niNameList );

			if ( niIndex != NI_INDEX_NULL )
			{
				NI_INIT( &niNameList );
				niStatus = ::ni_readprop( pContext->fDomain, &niDirID, niIndex, &niNameList );
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
	sInt32						siResult		= eDSNoErr;
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

		NI_INIT( &niNameList );
		niStatus = ::ni_listprops( pContext->fDomain, &pContext->dirID, &niNameList );
		if ( niStatus == NI_OK )
		{
			niIndex = ::ni_namelist_match( niNameList, pNI_AttrType );
			::ni_namelist_free( &niNameList );

			if ( niIndex != NI_INDEX_NULL )
			{
				NI_INIT( &niNameList );
				niStatus = ::ni_readprop( pContext->fDomain, &pContext->dirID, niIndex, &niNameList );
				if ( niStatus == NI_OK )
				{
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

							break;
						}
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

		// Get the domain and dir ID
		::memcpy( &niDirID, &pContext->dirID, sizeof( ni_id ) );

		NI_INIT( &niNameList );
		niStatus = ::ni_listprops( pContext->fDomain, &niDirID, &niNameList );
		if ( niStatus == NI_OK )
		{
			niIndex = ::ni_namelist_match( niNameList, pNI_AttrType );
			::ni_namelist_free( &niNameList );

			if ( niIndex != NI_INDEX_NULL )
			{
				NI_INIT( &niNameList );
				niStatus = ::ni_readprop( pContext->fDomain, &niDirID, niIndex, &niNameList );
				if ( niStatus == NI_OK )
				{
					if ( niNameList.ni_namelist_len >= uiIndex )
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

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // GetRecAttrValueByIndex


//------------------------------------------------------------------------------------
//	* CreateRecord
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::CreateRecord ( sCreateRecord *inData )
{
	volatile sInt32		siResult		= eDSNoErr;
	tDataNodePtr		pRecName		= nil;
	tDataNodePtr		pRecType		= nil;
	char			   *pNIRecType		= nil;
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

		pPath = BuildRecordNamePath( pRecName->fBufferData, pNIRecType );
		if ( pPath == nil ) throw( (sInt32)eDSInvalidRecordName );
		
		NI_INIT( &niDirID );
		(void)::ni_root( pContext->fDomain, &niDirID );

		// check access for non-admin users
		if ( ( pContext->fEffectiveUID != 0 )
			 && ( (pContext->fAuthenticatedUserName == NULL) 
				  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
		{
			authUser = pContext->fAuthenticatedUserName;
			if ( authUser == NULL )
			{
				//authUser = GetUserNameForUID( pContext->fEffectiveUID, pContext->fDomain );
			}
			ni_proplist niPropList;
			ni_id		typeDirID;
			
			pRecTypePath = (char*)calloc( strlen( pNIRecType ) + 2, 1 );
			pRecTypePath[0] = '/';
			strcpy( pRecTypePath + 1, pNIRecType );
			
			siResult = ::ni_pathsearch( pContext->fDomain, &typeDirID, pRecTypePath);
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			siResult = ::ni_read( pContext->fDomain, &typeDirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			siResult = NiLib2::ValidateDir( authUser, &niPropList );
			::ni_proplist_free( &niPropList );
			
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
		}
		
		//ValidateCreateRecord
		siResult = DoCreateRecord( pContext->fDomain, &niDirID, pPath );
		if ( siResult != eDSNoErr ) throw( siResult );

		//make sure that the type is users only here when we add these attributes
		if ( (siResult == eDSNoErr) && (strcmp(pNIRecType,"users") == 0) )
		{
			NI_INIT( &niValues );
			::ni_namelist_insert( &niValues, pRecName->fBufferData, NI_INDEX_NULL );

			siResult = DoAddAttribute( pContext->fDomain, &niDirID, (const ni_name)"_writers_passwd", niValues );
			
#ifdef TIM_CLIENT_PRESENT
			siResult = DoAddAttribute( pContext->fDomain, &niDirID, (const ni_name)"_writers_tim_password", niValues );
#endif
			
			::ni_namelist_free( &niValues );
		}

		free(pPath);
		pPath = nil;

		if ( inData->fInOpen == true )
		{
			pRecContext = MakeContextData();
			if ( pRecContext == nil ) throw( (sInt32)eMemoryAllocError );

			pRecContext->fDomain = pContext->fDomain;
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

	catch( sInt32 err )
	{
		siResult = err;
		if (pPath != nil)
		{
			free(pPath);
			pPath = nil;
		}
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

	if ( pNIRecType != nil )
	{
		delete( pNIRecType );
		pNIRecType = nil;
	}

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

		pNIRecType = MapRecToNetInfoType( pRecType->fBufferData );
		if ( pNIRecType == nil ) throw( (sInt32)eDSInvalidRecordType );

		if (pRecName->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordNameList );
		if (pRecType->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordTypeList );

		NI_INIT( &niDirID );
		siResult = IsValidRecordName( pRecName->fBufferData, pNIRecType, pContext->fDomain, niDirID );
		if (siResult != eDSNoErr)
		{
			//here we can check if the record type is /users so that we will search now on "realname" as well
			if (::strcmp(pNIRecType,"users") == 0)
			{
				siResult = IsValidRealname( pRecName->fBufferData, pContext->fDomain, &aRealname );
				//need to get the correct niDirID and set the true record name "aRealname" for the context data
				if (siResult == eDSNoErr)
				{
					bFoundRealname = true;
					siResult = IsValidRecordName( aRealname, pNIRecType, pContext->fDomain, niDirID );
				}
			}
			if ( siResult != eDSNoErr ) throw( siResult );
		}

		pRecContext = MakeContextData();
		if ( pRecContext == nil ) throw( (sInt32)eMemoryAllocError );

		pRecContext->fDomain = pContext->fDomain;
		pRecContext->fUID = pContext->fUID;
		pRecContext->fEffectiveUID = pContext->fEffectiveUID;
		if (pContext->fAuthenticatedUserName != NULL)
		{
			pRecContext->fAuthenticatedUserName = strdup( pContext->fAuthenticatedUserName );
		}
		
		pRecContext->fRecType = (char *)::calloc( 1, ::strlen( pRecType->fBufferData ) + 1 );
		::strcpy( pRecContext->fRecType, pRecType->fBufferData );

		if (bFoundRealname)
		{
			if (aRealname != nil)
			{
				pRecContext->fRecName = aRealname;
			}
		}
		else
		{
			pRecContext->fRecName = (char *)::calloc( 1, ::strlen( pRecName->fBufferData ) + 1 );
			::strcpy( pRecContext->fRecName, pRecName->fBufferData );
		}

		::memcpy( &pRecContext->dirID, &niDirID, sizeof( ni_id ) );

		gNINodeRef->AddItem( inData->fOutRecRef, pRecContext );
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

	if ( pNIRecType != nil )
	{
		delete( pNIRecType );
		pNIRecType = nil;
	}

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

	gNetInfoMutex->Wait();

	pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
	if ( pContext != nil )
	{
		gNINodeRef->RemoveItem( inData->fInRecRef );
	}
	else
	{
		siResult = eDSInvalidRecordRef;
	}

	gNetInfoMutex->Signal();

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

		// check access for non-admin users
		if ( ( pContext->fEffectiveUID != 0 )
			 && ( (pContext->fAuthenticatedUserName == NULL) 
				  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
		{
			authUser = pContext->fAuthenticatedUserName;
			if ( authUser == NULL )
			{
				//authUser = GetUserNameForUID( pContext->fEffectiveUID, pContext->fDomain );
			}
			ni_proplist niPropList;
			ni_id		niDirID;
			ni_index	tempIndex = NI_INDEX_NULL;

			::memcpy( &niDirID, &pContext->dirID, sizeof( ni_id ) );
			siResult = ::ni_parent( pContext->fDomain, &niDirID, &tempIndex );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			niDirID.nii_object = tempIndex;
			siResult = ::ni_self( pContext->fDomain, &niDirID );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			siResult = ::ni_read( pContext->fDomain, &niDirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			// must have write to the parent type of existing record...
			siResult = NiLib2::ValidateDir( authUser, &niPropList );
			::ni_proplist_free( &niPropList );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			pRecTypePath = (char*)calloc( strlen( pNIRecType ) + 2, 1 );
			pRecTypePath[0] = '/';
			strcpy( pRecTypePath + 1, pNIRecType );
			
			siResult = ::ni_pathsearch( pContext->fDomain, &niDirID, pRecTypePath );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			siResult = ::ni_read( pContext->fDomain, &niDirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			// and the destination type (parent of new record)
			siResult = NiLib2::ValidateDir( authUser, &niPropList );
			::ni_proplist_free( &niPropList );			

			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
		}
		
//		::strcat( pTypePath, pNIRecType );
//		::strcat( pTypePath, "/" );
//		::strcat( pTypePath, pContext->fRecName );

//		siResult = NiLib2::Copy( pContext->fDomain, &pContext->dirID, pTypePath, pContext->fDomain, true );
		siResult = NiLib2::Copy( pContext->fDomain, &pContext->dirID, pPath, pContext->fDomain, true );
		if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

		siResult = NiLib2::Destroy( pContext->fDomain, &pContext->dirID );
		if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

		//could have called IsValidRecordName here but since pPath already built above no need to
		siResult = ::ni_pathsearch( pContext->fDomain, &pContext->dirID, pPath );
		if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
		free(pPath);
		pPath = nil;
	}

	catch( sInt32 err )
	{
		siResult = err;
		if (pPath != nil)
		{
			free(pPath);
			pPath = nil;
		}
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

	gNetInfoMutex->Wait();

	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInRecRef );
		if ( pContext != nil )
		{
			// check access for non-admin users
			if ( ( pContext->fEffectiveUID != 0 )
				&& ( (pContext->fAuthenticatedUserName == NULL) 
					|| (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
			{
				authUser = pContext->fAuthenticatedUserName;
				if ( authUser == NULL )
				{
					//authUser = GetUserNameForUID( pContext->fEffectiveUID, pContext->fDomain );
				}
				ni_proplist niPropList;
				ni_id		parentDir;
				ni_index	tempIndex = NI_INDEX_NULL;

				::memcpy( &parentDir, &pContext->dirID, sizeof( ni_id ) );
				siResult = ::ni_parent( pContext->fDomain, &parentDir, &tempIndex );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
				parentDir.nii_object = tempIndex;
				siResult = ::ni_self( pContext->fDomain, &parentDir );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
				siResult = ::ni_read( pContext->fDomain, &parentDir, &niPropList );
				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

				siResult = NiLib2::ValidateDir( authUser, &niPropList );
				::ni_proplist_free( &niPropList );

				if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
			}
			
			niStatus = NiLib2::Destroy( pContext->fDomain, &pContext->dirID );
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
				gNINodeRef->RemoveItem( inData->fInRecRef );
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

		// need to be talking to the master
		::ni_needwrite( pContext->fDomain, 1 );

		siResult = ::ni_read( pContext->fDomain, &pContext->dirID, &niPropList );
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
			if ( authUser == NULL )
			{
				//authUser = GetUserNameForUID( pContext->fEffectiveUID, pContext->fDomain );
			}
			siResult = NiLib2::ValidateDir( authUser, &niPropList );
			if ( siResult != NI_OK )
			{
				siResult = NiLib2::ValidateName( authUser, &niPropList, niWhere );
			}

			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
		}

		//read the current name
		NI_INIT(&niCurrentName);
		siResult = ::ni_readprop(pContext->fDomain, &pContext->dirID, niWhere, &niCurrentName);
		//name replaced below is first one so only get that one
		pOldName = (char *)::calloc(1, strlen( niCurrentName.ni_namelist_val[ 0 ] ));
		strcpy(pOldName, niCurrentName.ni_namelist_val[ 0 ]);
		::ni_namelist_free( &niCurrentName );		

		// Get the new name
		uiNameLen = ::strlen( inData->fInNewRecName->fBufferData );
		if (uiNameLen == 0) throw( (sInt32)eDSNullRecName );

		pNewName = (char *)::malloc( uiNameLen );
		if ( pNewName == nil ) throw( (sInt32)eMemoryAllocError );

		// Remove any unexpected characters in the record name
		this->MakeGood( inData->fInNewRecName->fBufferData, pNewName );

		// Now set it
		siResult = ::ni_writename( pContext->fDomain, &pContext->dirID, niWhere, 0, pNewName );
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

						siResult = NiLib2::DestroyDirVal( pContext->fDomain, &pContext->dirID, (const ni_name)"_writers_passwd", niNameList );

						siResult = NiLib2::InsertDirVal( pContext->fDomain, &pContext->dirID, (const ni_name)"_writers_passwd", pNewName, pv );
						siResult = MapNetInfoErrors( siResult );

						::ni_namelist_free( &niNameList );

						bDidNotFindValue = false;

						break;
					}
				}
				if (bDidNotFindValue)
				{
					siResult = NiLib2::InsertDirVal( pContext->fDomain, &pContext->dirID, (const ni_name)"_writers_passwd", pNewName, niValue.ni_namelist_len );
					siResult = MapNetInfoErrors( siResult );
				}
			}
			else
			{
				NI_INIT( &niValues );
				::ni_namelist_insert( &niValues, pNewName, NI_INDEX_NULL );

				siResult = DoAddAttribute( pContext->fDomain, &pContext->dirID, (const ni_name)"_writers_passwd", niValues );
			
				::ni_namelist_free( &niValues );
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

						siResult = NiLib2::DestroyDirVal( pContext->fDomain, &pContext->dirID, (const ni_name)"_writers_tim_password", niNameList );

						siResult = NiLib2::InsertDirVal( pContext->fDomain, &pContext->dirID, (const ni_name)"_writers_tim_password", pNewName, pv );
						siResult = MapNetInfoErrors( siResult );

						::ni_namelist_free( &niNameList );

						bDidNotFindValue = false;

						break;
					}
				}
				if (bDidNotFindValue)
				{
					siResult = NiLib2::InsertDirVal( pContext->fDomain, &pContext->dirID, (const ni_name)"_writers_tim_password", pNewName, niValue.ni_namelist_len );
					siResult = MapNetInfoErrors( siResult );
				}
			}
			else
			{
				NI_INIT( &niValues );
				::ni_namelist_insert( &niValues, pNewName, NI_INDEX_NULL );

				siResult = DoAddAttribute( pContext->fDomain, &pContext->dirID, (const ni_name)"_writers_tim_password", niValues );
			
				::ni_namelist_free( &niValues );
			}
#endif
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
	
	if ( pNIRecType != nil )
	{
		free( pNIRecType );
		pNIRecType = nil;
	}

	if ( pNewName != nil )
	{
		free( pNewName );
		pNewName = nil;
	}

	if ( pOldName != nil )
	{
		free( pOldName );
		pOldName = nil;
	}

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

		// check access for non-admin users
		if ( ( pContext->fEffectiveUID != 0 )
			 && ( (pContext->fAuthenticatedUserName == NULL) 
				  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
		{
			authUser = pContext->fAuthenticatedUserName;
			if ( ( authUser == NULL ) && (strcmp(pAttrType,"picture") == 0) )
			{
				authUser = GetUserNameForUID( pContext->fEffectiveUID, pContext->fDomain );
			}
			ni_proplist niPropList;

			siResult = ::ni_read( pContext->fDomain, &pContext->dirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			siResult = NiLib2::ValidateDir( authUser, &niPropList );
			::ni_proplist_free( &niPropList );

			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
		}

		NI_INIT( &niAttribute );
		::ni_namelist_insert( &niAttribute, pAttrType, NI_INDEX_NULL );

		siResult = NiLib2::DestroyDirProp( pContext->fDomain, &pContext->dirID, niAttribute );
		siResult = MapNetInfoErrors( siResult );
		
		::ni_namelist_free( &niAttribute );

		if ( pAttrType != nil )
		{
			delete( pAttrType );
			pAttrType = nil;
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

		pAttrValue = inData->fInAttrValue->fBufferData;
		pAttrType = MapAttrToNetInfoType( inData->fInAttrType->fBufferData );
		if ( pAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );

		// check access for non-admin users
		if ( ( pContext->fEffectiveUID != 0 )
			 && ( (pContext->fAuthenticatedUserName == NULL) 
				  || (strcmp(pContext->fAuthenticatedUserName,"root") != 0) ) )
		{
			authUser = pContext->fAuthenticatedUserName;
			if ( ( authUser == NULL ) && (strcmp(pAttrType,"picture") == 0) )
			{
				authUser = GetUserNameForUID( pContext->fEffectiveUID, pContext->fDomain );
			}
			ni_proplist niPropList;
			ni_index	niIndex;

			siResult = ::ni_read( pContext->fDomain, &pContext->dirID, &niPropList );
			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

			siResult = NiLib2::ValidateDir( authUser, &niPropList );
			if ( siResult != NI_OK )
			{
				niIndex = ::ni_proplist_match( niPropList, pAttrType, NULL );
				siResult = NiLib2::ValidateName( authUser, &niPropList, niIndex );
			}
			::ni_proplist_free( &niPropList );

			if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );
		}

		NI_INIT( &niNameList );
		::ni_namelist_insert( &niNameList, pAttrValue, NI_INDEX_NULL );

		siResult = NiLib2::AppendDirProp( pContext->fDomain, &pContext->dirID, pAttrType, niNameList );
		siResult = MapNetInfoErrors( siResult );

		::ni_namelist_free( &niNameList );

		if ( pAttrType != nil )
		{
			delete( pAttrType );
			pAttrType = nil;
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

		// Get the native net info attribute
		pAttrType = MapAttrToNetInfoType( inData->fInAttrType->fBufferData );

		// read the attribute list
		siResult = ::ni_read( pContext->fDomain, &pContext->dirID, &niPropList );
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
				authUser = GetUserNameForUID( pContext->fEffectiveUID, pContext->fDomain );
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
						NI_INIT( &niNameList );
						::ni_namelist_insert( &niNameList, niValue.ni_namelist_val[ pv ], NI_INDEX_NULL );

						siResult = NiLib2::DestroyDirVal( pContext->fDomain, &pContext->dirID, pAttrType, niNameList );
						if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

						::ni_namelist_free( &niNameList );

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

		pAttrType = MapAttrToNetInfoType( inData->fInAttrType->fBufferData );

		siResult = ::ni_read( pContext->fDomain, &pContext->dirID, &niPropList );
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
				authUser = GetUserNameForUID( pContext->fEffectiveUID, pContext->fDomain );
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
						NI_INIT( &niNameList );
						::ni_namelist_insert( &niNameList, niValue.ni_namelist_val[ pv ], NI_INDEX_NULL );

						siResult = NiLib2::DestroyDirVal( pContext->fDomain, &pContext->dirID, pAttrType, niNameList );

						pAttrValue = inData->fInAttrValueEntry->fAttributeValueData.fBufferData;

						siResult = NiLib2::InsertDirVal( pContext->fDomain, &pContext->dirID, pAttrType, pAttrValue, pv );
						siResult = MapNetInfoErrors( siResult );

						::ni_namelist_free( &niNameList );

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

} // SetAttributeValue


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
	char				   *pSearchStr			= nil;
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
		//save the number of rec types here to use in separating the buffer data
		countDownRecTypes = cpRecTypeList->GetCount() - pContinue->fRecTypeIndex + 1;

		pDS_AttrType = inData->fInAttrType->fBufferData;
		if ( pDS_AttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

		pSearchStr = inData->fInPatt2Match->fBufferData;
		if ( pSearchStr == nil ) throw( (sInt32)eDSEmptyPattern2Match );

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
												pSearchStr,
												pattMatch,
												cpAttrTypeList,
												bAttrInfoOnly,
												pContinue,
												pContext->fDomain,
												pContext->fDomainName,
												outBuff,
												uiCount );
				}
				else
				{
					siResult = FindTheseRecords ( pNI_RecType,
												pDS_RecType,
												pDS_AttrType,
												pSearchStr,
												pattMatch,
												cpAttrTypeList,
												bAttrInfoOnly,
												pContinue,
												pContext->fDomain,
												pContext->fDomainName,
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
//	* FindAllRecords
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn:: FindAllRecords (	const char			*inNI_RecType,
									const char			*inDS_RecType,
									const char			*inPatt2Match,
									tDirPatternMatch	 inHow,
									CAttributeList		*inAttrTypeList,
									bool				 inAttrOnly,
									sNIContinueData		*inContinue,
									void				*inDomain,
									char				*inDomainName,
									CBuff				*inBuff,
									uInt32				&outRecCount )
{
	uInt32			pn				= 0;
	uInt32			pv				= 0;
	sInt32			siResult		= eDSNoErr;
//	ni_status		niStatus		= NI_OK;
	sInt32			siCount			= 0;
	uInt32			uiSearchFlag	= REG_BASIC;
	bool			bMatches		= false;
//	u_int			en				= 0;
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

		// Get the dir ID for the record type
		siResult = ::ni_pathsearch( inDomain, &niDirID, inNI_RecType );
		if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

		// Do the search
		siResult = ::ni_list( inDomain, &niDirID, "name", &niEntryList );
		if ( siResult != eDSNoErr ) throw( MapNetInfoErrors( siResult ) );

		for ( en2 = inContinue->fAllRecIndex; (en2 < niEntryList.ni_entrylist_len) &&
								(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
					 			(inContinue->fLimitRecSearch == 0)); en2++ )
		{
			niEntry = niEntryList.ni_entrylist_val[ en2 ];
			niDirID.nii_object = niEntry.id;

			if ( niEntry.names != NULL )
			{
				siResult = ::ni_read( inDomain, &niDirID, &niPropList );
				if ( siResult == NI_OK )
				{
					bMatches = false;
					
					for ( pn = 0; ((pn < niPropList.ni_proplist_len) && (bMatches == false)); pn++ )
					{
						niProp = niPropList.ni_proplist_val[ pn ];
						niNameList = niProp.nip_val;
						for ( pv = 0; ((pv < niNameList.ni_namelist_len) && (bMatches == false)); pv++ )
						{
							bMatches = DoesThisMatch( niNameList.ni_namelist_val[ pv ], inPatt2Match, inHow );
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

						siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, inDomain, inDomainName, siCount );
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
		if ( niEntryList.ni_entrylist_len > 0 )
		{
			::ni_entrylist_free(&niEntryList);
		}
		if ( siResult == eDSNoErr )
		{
			inContinue->fAllRecIndex	= 0;
		}
	}

	catch( sInt32 err )
	{
		if ( niEntryList.ni_entrylist_len > 0 )
		{
			::ni_entrylist_free(&niEntryList);
		}
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
										const char		 *inPatt2Match,
										tDirPatternMatch inHow,
										CAttributeList	 *inAttrTypeList,
										bool			 inAttrOnly,
										sNIContinueData	 *inContinue,
										void			 *inDomain,
										char			 *inDomainName,
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
	char		   * inConstRegExpPatt2Match	= nil;


	gNetInfoMutex->Wait();

	outRecCount = 0; //need to track how many records were found by this call to FindTheseRecords

	try
	{
		// Case sensitive check
		if ( (inHow >= eDSiExact) && (inHow <= eDSiRegularExpression) )
		{
			uiSearchFlag = REG_ICASE;
		}

		while ( !done )
		{
			NI_INIT( &niDirID );
			NI_INIT( &niEntryList );

			// Get the dir ID for the record
			siResult = ::ni_pathsearch( inDomain, &niDirID, inNI_RecType );
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
				inConstRegExpPatt2Match = BuildRegExp(inPatt2Match);
				niStatus = ::ni_search( inDomain, &niDirID, csNI_AttrType.GetData(), inConstRegExpPatt2Match, uiSearchFlag, &niEntryList );
				free(inConstRegExpPatt2Match);
				if ( niStatus == NI_OK )
				{
					for ( en = inContinue->fAllRecIndex; (en < niEntryList.ni_entrylist_len) &&
						(((inContinue->fTotalRecCount) < inContinue->fLimitRecSearch) ||
						(inContinue->fLimitRecSearch == 0)); en++ )
	//				for ( en = inContinue->fAllRecIndex; en < niEntryList.ni_entrylist_len; en++ )
					{
						niEntry = niEntryList.ni_entrylist_val[ en ];
						niDirID.nii_object = niEntry.id;
	
						if ( niEntry.names != NULL )
						{
							niNameList = *niEntry.names;
							bMatches = false;
							for ( i = 0; ((i < niNameList.ni_namelist_len) && (bMatches == false)); i++ )
							{
								bMatches = DoesThisMatch( niNameList.ni_namelist_val[ i ], inPatt2Match, inHow );
							}
							if ( bMatches == true )
							{
								siResult = ::ni_read( inDomain, &niDirID, &niPropList );
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
	
									siResult = GetTheseAttributes( inAttrTypeList, &niPropList, inAttrOnly, inDomain, inDomainName, siCount );
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
				} //ni_search successful
				if ( niEntryList.ni_entrylist_len > 0 )
				{
					::ni_entrylist_free(&niEntryList);
				}
			} // need to get more records ie. did not hit the limit yet
		} // while
		inContinue->fAllRecIndex = 0;
	}

	catch( sInt32 err )
	{
		if ( niEntryList.ni_entrylist_len > 0 )
		{
			::ni_entrylist_free(&niEntryList);
		}
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult ); 

} // FindTheseRecords


//------------------------------------------------------------------------------------
//	* DoAuthentication
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoAuthentication ( sDoDirNodeAuth *inData )
{
	sInt32				siResult		= eDSAuthFailed;
	ni_status			niResult		= NI_OK;
	uInt32				uiAuthMethod	= 0;
	sNIContextData	   *pContext		= nil;
	sNIContinueData	   *pContinueData	= NULL;
	char*				userName		= NULL;
	ni_id				niDirID;
	ni_namelist			niValues;
	AuthAuthorityHandlerProc handlerProc = NULL;
	
	try
	{
		pContext = (sNIContextData *)gNINodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );
		if ( inData->fInAuthStepData == nil ) throw( (sInt32)eDSNullAuthStepData );
		
		if ( inData->fIOContinueData != NULL )
		{
			// get info from continue
			pContinueData = (sNIContinueData *)inData->fIOContinueData;
			if ( gNIContinue->VerifyItem( pContinueData ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
			
			if ( eDSInvalidContinueData == nil ) throw( (sInt32)(pContinueData->fAuthHandlerProc) );
			handlerProc = (AuthAuthorityHandlerProc)(pContinueData->fAuthHandlerProc);
			if (handlerProc != NULL)
			{
				siResult = (handlerProc)(inData->fInNodeRef, inData->fInAuthMethod, pContext,
										 &pContinueData, inData->fInAuthStepData, 
										 inData->fOutAuthStepDataResponse, 
										 inData->fInDirNodeAuthOnlyFlag, 
										 pContinueData->fAuthAuthorityData);
			}
		}
		else
		{
			// first call
			siResult = GetAuthMethod( inData->fInAuthMethod, &uiAuthMethod );
			if ( siResult == eDSNoErr )
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
				//printf("username: %s\n", userName);
				// get the auth authority
				siResult = IsValidRecordName ( userName, "/users", pContext->fDomain, niDirID );
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed ); // unknown user really
				gNetInfoMutex->Wait();
				//lookup authauthority attribute
				niResult = ni_lookupprop( pContext->fDomain, &niDirID, 
										"authentication_authority", &niValues );
				gNetInfoMutex->Signal();
				if (niResult == NI_OK) 
				{
					// loop through all possibilities for set
					// do first auth authority that supports the method for check password
					unsigned int i = 0;
					bool bLoopAll = IsWriteAuthRequest(uiAuthMethod);
					char* aaVersion = NULL;
					char* aaTag = NULL;
					char* aaData = NULL;
					siResult = eDSAuthMethodNotSupported;
					while ( i < niValues.ni_namelist_len 
							&& (siResult == eDSAuthMethodNotSupported ||
								(bLoopAll && siResult == eDSNoErr)))
					{
						//parse this value of auth authority
						siResult = ParseAuthAuthority( niValues.ni_namelist_val[i], &aaVersion, 
													&aaTag, &aaData );
						// JT need to check version
						if (siResult != eDSNoErr) {
							siResult = eDSAuthFailed;
							break;
						}
						handlerProc = GetAuthAuthorityHandler( aaTag );
						if (handlerProc != NULL) {
							siResult = (handlerProc)(inData->fInNodeRef, inData->fInAuthMethod, pContext, 
													&pContinueData, 
													inData->fInAuthStepData, 
													inData->fOutAuthStepDataResponse,
													inData->fInDirNodeAuthOnlyFlag,aaData);
							if (pContinueData != NULL && siResult == eDSNoErr)
							{
								// we are supposed to return continue data
								// remember the proc we used
								pContinueData->fAuthHandlerProc = (void*)handlerProc;
								pContinueData->fAuthAuthorityData = aaData;
								aaData = NULL;
								break;
							}
						} else {
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
										   inData->fInDirNodeAuthOnlyFlag,NULL);
					if (pContinueData != NULL && siResult == eDSNoErr)
					{
						// we are supposed to return continue data
						// remember the proc we used
						pContinueData->fAuthHandlerProc = (void*)CNiPlugIn::DoBasicAuth;
					}
				}
			}
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}
	
	if (userName != NULL)
	{
		free(userName);
		userName = NULL;
	}

	inData->fResult = siResult;
	inData->fIOContinueData = pContinueData;

	return( siResult );

} // DoAuthentication

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
			return true;
			
		default:
			return false;
	}
} // IsWriteAuthRequest


//------------------------------------------------------------------------------------
//	* DoBasicAuth
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoBasicAuth ( tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod, 
								sNIContextData* inContext, 
								sNIContinueData** inOutContinueData, 
								tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
								bool inAuthOnly, char* inAuthAuthorityData )
{
	sInt32				siResult		= eDSNoErr;
	uInt32				uiAuthMethod	= 0;

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		siResult = GetAuthMethod( inAuthMethod, &uiAuthMethod );
		if ( siResult == eDSNoErr )
		{
			switch( uiAuthMethod )
			{
				//native auth is always UNIX crypt possibly followed by 2-way random using tim auth server
				case kAuthNativeMethod:
				case kAuthNativeNoClearText:
				case kAuthNativeClearTextOK:
					if ( outAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
					siResult = DoUnixCryptAuth( inContext, inAuthData, inAuthOnly );
					if ( siResult == eDSNoErr )
					{
						if ( outAuthData->fBufferSize > ::strlen( kDSStdAuthCrypt ) )
						{
							::strcpy( outAuthData->fBufferData, kDSStdAuthCrypt );
						}
					}
					else if ( (siResult != eDSAuthFailed) && (siResult != eDSInvalidBuffFormat) )
					{
						siResult = DoNodeNativeAuth( inContext, inAuthData, inAuthOnly );
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
					siResult = DoUnixCryptAuth( inContext, inAuthData, inAuthOnly );
					break;

				case kAuthSetPasswd:
					siResult = DoSetPassword( inContext, inAuthData );
					break;

				case kAuthSetPasswdAsRoot:
					siResult = DoSetPasswordAsRoot( inContext, inAuthData );
					break;

				case kAuthChangePasswd:
					siResult = DoChangePassword( inContext, inAuthData );
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

	//inData->fResult = siResult;

	return( siResult );

} // DoBasicAuth


//------------------------------------------------------------------------------------
//	* DoLocalWindowsAuth
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoLocalWindowsAuth ( tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod, 
									   sNIContextData* inContext, 
									   sNIContinueData** inOutContinueData, 
									   tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
									   bool inAuthOnly, char* inAuthAuthorityData )
{
	sInt32				siResult		= eDSAuthFailed;
#ifdef TIM_CLIENT_PRESENT
	uInt32				uiAuthMethod	= 0;
	char			   *pUserName		= NULL;
	char			   *pNewPassword	= NULL;
	char			   *pOldPassword	= NULL;
	char			   *pAdminUser		= NULL;
	char			   *pAdminPassword	= NULL;
	unsigned char		P21[21]			= {0};
	tDataNodePtr		pC8Node			= NULL;
	unsigned char 		C8[8] 			= {0};
	unsigned char		P24[24] 		= {0};
	unsigned char		P24Input[24]	= {0};
	tDataNodePtr		pP24InputNode	= NULL;
	tDataListPtr		dataList		= NULL;
	char			   *path			= NULL;
	unsigned char		hashes[32]		= {0};
	unsigned char		generatedHashes[32]	= {0};

	// 32 bytes for NT hash and 32 for LAN Manager hash, both hex encoded

	try
	{
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
		if ( inContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		siResult = GetAuthMethod( inAuthMethod, &uiAuthMethod );
		if ( siResult == eDSNoErr )
		{
			switch( uiAuthMethod )
			{
				case kAuthSMB_NT_Key:
				case kAuthSMB_LM_Key:
					// parse input first
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( dsDataListGetNodeCountPriv(dataList) != 3 ) throw( (sInt32)eDSInvalidBuffFormat );
						
					// this allocates a copy of the string
					pUserName = dsDataListGetNodeStringPriv(dataList, 1);
					if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(pUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
					// these are not copies
					siResult = dsDataListGetNodePriv(dataList, 2, &pC8Node);
					if ( pC8Node == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( pC8Node->fBufferLength != 8 ) throw( (sInt32)eDSInvalidBuffFormat);
					if ( siResult != eDSNoErr ) throw( (sInt32)eDSInvalidBuffFormat );
					memmove(C8, ((tDataBufferPriv*)pC8Node)->fBufferData, 8);
					
					siResult = dsDataListGetNodePriv(dataList, 3, &pP24InputNode);
					if ( pP24InputNode == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( pP24InputNode->fBufferLength != 24 ) throw( (sInt32)eDSInvalidBuffFormat);
					if ( siResult != eDSNoErr ) throw( (sInt32)eDSInvalidBuffFormat );
					memmove(P24Input, ((tDataBufferPriv*)pP24InputNode)->fBufferData, 24);
					
					//read file
					siResult = ReadWindowsHash(pUserName,hashes);
					break;

				// set password operations
				case kAuthSetPasswd:
					// parse input first
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( dsDataListGetNodeCountPriv(dataList) != 4 ) throw( (sInt32)eDSInvalidBuffFormat );
						
					// this allocates a copy of the string
					pUserName = dsDataListGetNodeStringPriv(dataList, 1);
					if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(pUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
					
					// this allocates a copy of the string
					pNewPassword = dsDataListGetNodeStringPriv(dataList, 2);
					if ( pNewPassword == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					
					pAdminUser = dsDataListGetNodeStringPriv(dataList, 3);
					if ( pAdminUser == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					
					pAdminPassword = dsDataListGetNodeStringPriv(dataList, 4);
					if ( pAdminPassword == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					//read file
					siResult = ReadWindowsHash(pAdminUser,hashes);
					break;

				case kAuthSetPasswdAsRoot:
					// parse input first
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( dsDataListGetNodeCountPriv(dataList) != 2 ) throw( (sInt32)eDSInvalidBuffFormat );
						
					// this allocates a copy of the string
					pUserName = dsDataListGetNodeStringPriv(dataList, 1);
					if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(pUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );

					// this allocates a copy of the string
					pNewPassword = dsDataListGetNodeStringPriv(dataList, 2);
					if ( pNewPassword == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					break;

				case kAuthChangePasswd:
					// parse input first
					dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( dsDataListGetNodeCountPriv(dataList) != 3 ) throw( (sInt32)eDSInvalidBuffFormat );
						
					// this allocates a copy of the string
					pUserName = dsDataListGetNodeStringPriv(dataList, 1);
					if ( pUserName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(pUserName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );

					// this allocates a copy of the string
					pOldPassword = dsDataListGetNodeStringPriv(dataList, 2);
					if ( pOldPassword == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					
					pNewPassword = dsDataListGetNodeStringPriv(dataList, 3);
					if ( pNewPassword == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					//read file
					siResult = ReadWindowsHash(pUserName,hashes);
					break;

				default:
					break;
			}
			switch( uiAuthMethod )
			{
				case kAuthSMB_NT_Key:
					memmove(P21, hashes, 16);
					//BinaryFromHexString(smbHashFile, 32, P21); //32 hex chars -> 16 bytes
					CalculateP24(P21, C8, P24);
					if (memcmp(P24,P24Input,24) == 0)
					{
						siResult = eDSNoErr;
					}
					else
					{
						siResult = eDSAuthFailed;
					}
					break;

				case kAuthSMB_LM_Key:
					memmove(P21, hashes+16, 16);
					//BinaryFromHexString(smbHashFile+32, 32, P21); //32 hex chars -> 16 bytes
					CalculateP24(P21, C8, P24);
					if (memcmp(P24,P24Input,24) == 0)
					{
						siResult = eDSNoErr;
					}
					else
					{
						siResult = eDSAuthFailed;
					}
					break;

				// set password operations
				/*case kAuthSetPasswd:
					// check admin user password then change password
					break;*/

				case kAuthSetPasswdAsRoot:
					// allow root to change anyone's password and
					// others to change their own password
					if ( inContext->fEffectiveUID != 0 )
					{
						if ((inContext->fAuthenticatedUserName == NULL)
							|| ((::strcmp(inContext->fAuthenticatedUserName, "root") != 0)
								&& (::strcmp(inContext->fAuthenticatedUserName, pUserName) != 0)))
						{
							throw( (sInt32)eDSPermissionError );
						}
					}
					// write new password
					CalculateSMBNTHash(pNewPassword,generatedHashes);
					CalculateSMBLANManagerHash(pNewPassword,generatedHashes+16);
					siResult = WriteWindowsHash(pUserName,generatedHashes);
					break;

				case kAuthChangePasswd:
					// check old password then write new one
					CalculateSMBNTHash(pOldPassword,generatedHashes);
					CalculateSMBLANManagerHash(pOldPassword,generatedHashes+16);
					if (memcmp(hashes,generatedHashes,32) == 0)
					{
						// set new password
						bzero(generatedHashes,32);
						CalculateSMBNTHash(pNewPassword,generatedHashes);
						CalculateSMBLANManagerHash(pNewPassword,generatedHashes+16);
						siResult = WriteWindowsHash(pUserName,generatedHashes);
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
		}
	}
	catch( sInt32 err )
	{
		siResult = err;
	}

	if (dataList != NULL)
	{
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
	}
	
	if ( pUserName != nil )
	{
		free( pUserName );
		pUserName = nil;
	}
	
	if (path != NULL)
	{
		free(path);
		path = NULL;
	}
#endif
	return( siResult );

} // DoLocalWindowsAuth


//--------------------------------------------------------------------------------------------------
// * ReadWindowsHash ()
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::ReadWindowsHash ( const char *inUserName, unsigned char outHashes[32] )
{
	sInt32 siResult = eDSAuthFailed;
#ifdef TIM_CLIENT_PRESENT
	char* path = NULL;
	char hexHashes[64] = { 0 };
	sInt32 readBytes = 0;

	try
	{
		path = (char*)::calloc(strlen("/var/db/samba/hash/") + strlen(inUserName) + 1, 1);
		if ( path != NULL )
		{
			sprintf(path, "%s%s", "/var/db/samba/hash/", inUserName);
			{
				CFile hashFile(path, false);

				if (hashFile.is_open())
				{
					readBytes = hashFile.ReadBlock( hexHashes, 64 );
					// should check the right number of bytes is there
					if ( readBytes != 64 ) throw( (sInt32)eDSAuthFailed );
					BinaryFromHexString(hexHashes, 64, outHashes);
					siResult = eDSNoErr;
				}
			}
		}
	}
	catch( sInt32 err )
    {
        siResult = eDSAuthFailed;
    }
	if ( path != NULL ) {
		free( path );
		path = NULL;
	}
#endif
	return( siResult );
} // ReadWindowsHash


//--------------------------------------------------------------------------------------------------
// * WriteWindowsHash ()
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::WriteWindowsHash ( const char *inUserName, unsigned char inHashes[32] )
{
	sInt32 result = eDSAuthFailed;
#ifdef TIM_CLIENT_PRESENT
	char* path = NULL;
	char* hexHashes = NULL;
	sInt32 siResult = eDSNoErr;
	struct stat statResult;
	
	try
	{
		path = (char*)::calloc(strlen("/var/db/samba/hash/") + strlen(inUserName) + 1, 1);
		if ( path != NULL )
		{
			sprintf(path, "%s%s", "/var/db/samba/hash/", inUserName);
		}
		siResult = stat( "/var/db/samba/hash", &statResult );
		if (siResult != eDSNoErr)
		{
			siResult = ::stat( "/var/db/samba", &statResult );
			//if first sub directory does not exist
			if (siResult != eDSNoErr)
			{
				::mkdir( "/var/db/samba", 0700 );
				::chmod( "/var/db/samba", 0700 );
			}
			siResult = ::stat( "/var/db/samba/hash", &statResult );
			//if second sub directory does not exist
			if (siResult != eDSNoErr)
			{
				::mkdir( "/var/db/samba/hash", 0700 );
				::chmod( "/var/db/samba/hash", 0700 );
			}
		}
		
		CFile hashFile(path, true);

		if (hashFile.is_open())
		{
			hexHashes = HexStringFromBinary( inHashes, 32 );
			hashFile.seekp( 0 ); // start at beginning
			hashFile.write( hexHashes, 64 );
			result = eDSNoErr;
		}
	}
	catch( sInt32 err )
    {
        result = eDSAuthFailed;
    }
	if ( path != NULL ) {
		free( path );
		path = NULL;
	}
	if ( hexHashes != NULL ) {
		free( hexHashes );
		hexHashes = NULL;
	}
#endif
	return( result );
} // WriteWindowsHash


//
//--------------------------------------------------------------------------------------------------
// * PWOpenDirNode ()
//
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::PWOpenDirNode ( tDirNodeReference fDSRef, char *inNodeName, tDirNodeReference *outNodeRef )
{
	sInt32			error		= eDSNoErr;
	sInt32			error2		= eDSNoErr;
	tDataList	   *pDataList	= nil;

	pDataList = ::dsBuildFromPathPriv( inNodeName, "/" );
    if ( pDataList != nil )
    {
        error = ::dsOpenDirNode( fDSRef, pDataList, outNodeRef );
        error2 = ::dsDataListDeallocatePriv( pDataList );
        free( pDataList );
    }

    return( error );

} // PWOpenDirNode


//--------------------------------------------------------------------------------------------------
//	RepackBufferForPWServer
//
//	Replace the user name with the uesr id.
//--------------------------------------------------------------------------------------------------

sInt32 CNiPlugIn::RepackBufferForPWServer ( tDataBufferPtr inBuff, const char *inUserID, unsigned long inUserIDNodeNum, tDataBufferPtr *outBuff )
{
	sInt32 result = eDSNoErr;
    tDataListPtr dataList = NULL;
    tDataNodePtr dataNode = NULL;
	unsigned long index, nodeCount;
	unsigned long uidLen;
                
    if ( !inBuff || !inUserID || !outBuff )
        return eDSAuthParameterError;
    
    try
    {	
        uidLen = strlen(inUserID);
        *outBuff = ::dsDataBufferAllocatePriv( inBuff->fBufferLength + uidLen + 1 );
        if ( *outBuff == nil ) throw( eMemoryError );
        
        (*outBuff)->fBufferLength = 0;
        
        dataList = dsAuthBufferGetDataListAllocPriv(inBuff);
        if ( dataList == nil ) throw( eDSInvalidBuffFormat );
        
        nodeCount = dsDataListGetNodeCountPriv(dataList);
        if ( nodeCount < 2 ) throw( eDSInvalidBuffFormat );
        
        for ( index = 1; index <= nodeCount; index++ )
        {
            if ( index == inUserIDNodeNum )
            {
                // write 4 byte length
                memcpy( (*outBuff)->fBufferData + (*outBuff)->fBufferLength, &uidLen, sizeof(unsigned long) );
                (*outBuff)->fBufferLength += sizeof(unsigned long);
                
                // write uid
                memcpy( (*outBuff)->fBufferData + (*outBuff)->fBufferLength, inUserID, uidLen );
                (*outBuff)->fBufferLength += uidLen;
            }
            else
            {
                // get a node
                result = dsDataListGetNodeAllocPriv(dataList, index, &dataNode);
                if ( result != eDSNoErr ) throw( eDSInvalidBuffFormat );
            
                // copy it
                memcpy((*outBuff)->fBufferData + (*outBuff)->fBufferLength, &dataNode->fBufferLength, sizeof(unsigned long));
                (*outBuff)->fBufferLength += sizeof(unsigned long);
                
                memcpy( (*outBuff)->fBufferData + (*outBuff)->fBufferLength, dataNode->fBufferData, dataNode->fBufferLength );
                (*outBuff)->fBufferLength += dataNode->fBufferLength;
                
                // clean up
                dsDataBufferDeallocatePriv(dataNode);
            }
            
        }
        
        (void)dsDataListDeallocatePriv(dataList);
        free(dataList);
    }
    
    catch( sInt32 error )
    {
        result = error;
    }
    
    return result;
}


//------------------------------------------------------------------------------------
//	* DoPasswordServerAuth
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoPasswordServerAuth ( tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod, 
										 sNIContextData* inContext, 
										 sNIContinueData** inOutContinueData, 
										 tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
										 bool inAuthOnly, char* inAuthAuthorityData )
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
        //CShared::LogIt( 0x0F, "AuthorityData=%s\n", inAuthAuthorityData);
        
        serverAddr = strchr( inAuthAuthorityData, ':' );
        if ( serverAddr )
        {
            uidStrLen = serverAddr - inAuthAuthorityData;
            uidStr = (char *) calloc(1, uidStrLen+1);
            if ( uidStr == nil ) throw( eMemoryError );
            strncpy( uidStr, inAuthAuthorityData, uidStrLen );
            
            // advance past the colon
            serverAddr++;
            
            error = GetAuthMethod( inAuthMethod, &authMethod );
            if ( error ) throw ( error );
            
            switch( authMethod )
            {
                case kAuth2WayRandom:
                    if ( inOutContinueData == nil )
                        throw( (sInt32)eDSNullParameter );
                    
                    if ( *inOutContinueData == nil )
                    {
                        pContinue = (sNIContinueData *)::calloc( 1, sizeof( sNIContinueData ) );
                        gNIContinue->AddItem( pContinue, inNodeRef );
                        
                        // make a buffer for the user ID
                        authDataBuff = ::dsDataBufferAllocatePriv( uidStrLen + 1 );
                        if ( authDataBuff == nil ) throw ( eMemoryError );
                        
                        // fill
                        strcpy( authDataBuff->fBufferData, uidStr );
                        authDataBuff->fBufferLength = uidStrLen;
                    }
                    else
                    {
                        pContinue = *inOutContinueData;
                        if ( gNIContinue->VerifyItem( pContinue ) == false )
                            throw( (sInt32)eDSInvalidContinueData );
                    }
                    break;
                    
                case kAuthSetPasswd:
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
                        
                        // get the auth authority
                        error = IsValidRecordName ( userName, "/users", inContext->fDomain, niDirID );
                        if ( error != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
                        
						gNetInfoMutex->Wait();
                        //lookup authauthority attribute
                        niResult = ni_lookupprop( inContext->fDomain, &niDirID, 
                                                "authentication_authority", &niValues );
						gNetInfoMutex->Signal();
                        if (niResult != NI_OK) throw( (sInt32)eDSAuthFailed );
                     	
                        // don't break or throw to guarantee cleanup
                        lookupResult = eDSAuthFailed;
                        for ( idx = 0; idx < niValues.ni_namelist_len && lookupResult == eDSAuthFailed; idx++ )
                        {
                            // parse this value of auth authority
                            error = ParseAuthAuthority( niValues.ni_namelist_val[idx], &aaVersion, &aaTag, &aaData );
                            // need to check version
                            if (error != eDSNoErr)
                                lookupResult = eParameterError;
                            
                            if ( error == eDSNoErr && strcmp(aaTag, "ApplePasswordServer") == 0 )
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
                        ni_namelist_free(&niValues);
						gNetInfoMutex->Signal();
                        
                        if ( lookupResult != eDSNoErr ) throw( eDSAuthFailed );
                        
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
            
            //CShared::LogIt( 0x0F, "ready to call PSPlugin\n");
			
            if ( inContext->fPWSRef == 0 )
            {
                error = ::dsOpenDirService( &inContext->fPWSRef );
                if ( error != eDSNoErr ) throw( error );
            }
            
            if ( inContext->fPWSNodeRef == 0 )
            {
                nodeName = (char *)calloc(1,strlen(serverAddr)+17);
                if ( nodeName == nil ) throw ( eMemoryError );
                
                sprintf( nodeName, "/PasswordServer/%s", serverAddr );
                error = PWOpenDirNode( inContext->fPWSRef, nodeName, &inContext->fPWSNodeRef );
                if ( error != eDSNoErr ) throw( error );
            }
            
            if ( pContinue )
                continueData = pContinue->fPassPlugContinueData;
            
            result = dsDoDirNodeAuth( inContext->fPWSNodeRef, inAuthMethod, inAuthOnly,
                                        authDataBuff, outAuthData, &continueData );
            
            if ( pContinue )
                pContinue->fPassPlugContinueData = continueData;
				
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
		free( password );
		password = NULL;
	}
    
    if ( authDataBuff )
        dsDataBufferDeallocatePriv( authDataBuff );
    if ( authDataBuffTemp )
        dsDataBufferDeallocatePriv( authDataBuffTemp );
                        
	return( result );
} // DoPasswordServerAuth


//------------------------------------------------------------------------------------
//	* DoSetPassword
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoSetPassword ( sNIContextData *inContext, tDataBuffer *inAuthData )
{
	sInt32				siResult		= eDSAuthFailed;
	bool				bFreePropList	= false;
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
	void			   *domain			= nil;

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

		rootPwd = (char *)::calloc( 1, rootPwdLen + 1 );
		::memcpy( rootPwd, pData, rootPwdLen );
		pData += rootPwdLen;
		offset += rootPwdLen;

#ifdef TIM_CLIENT_PRESENT
		//opt out early if we know tim is not there **** check on local first
		if ( IsTimRunning() )
		{
			pTimHndl = ::timServerForDomain( inContext->fDomain );
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
				CShared::LogIt( 0x0F, "-- timSetPasswordForUser -- failed with %l.", siResult );
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

			domain = inContext->fDomain;
			//need some code here to directly change the NI Crypt password
			//first check to see that the old password is valid
			CShared::LogIt( 0x0F, "Attempting UNIX Crypt password change" );
			siResult = IsValidRecordName ( rootName, (char *)"/users", domain, niDirID );

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
			niWhere = ::ni_proplist_match( niPropList, "uid", nil );

			if (niWhere == NI_INDEX_NULL) throw( (sInt32)eDSAuthFailed );
			siResult = eDSAuthFailed;
			niValue = niPropList.ni_proplist_val[ niWhere ].nip_val;

			// make sure the user we are going to verify actually is root
			if ( (niValue.ni_namelist_val[ 0 ] != NULL) && (::strcmp( "0", niValue.ni_namelist_val[ 0 ])) ) throw( (sInt32)eDSAuthFailed );

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

				::memset( hashPwd, 0, 32 );
				::strcpy( hashPwd, ::crypt( rootPwd, salt ) );

				siResult = eDSAuthFailed;
				if ( ::strcmp( hashPwd, niPwd ) == 0 )
				{
					siResult = eDSNoErr;
				}
			
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
				// we successfully authenticated with root password, now set new user password.
				//set with the new password
				const char *saltchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

				::memset( hashPwd, 0, 32 );

				if ( ::strlen(userPwd) > 0 )
				{
					// only need crypt if password is not empty
					::srandom(getpid() + time(0));
					salt[0] = saltchars[random() % 64];
					salt[1] = saltchars[random() % 64];
					salt[2] = '\0';

					::strcpy( hashPwd, ::crypt( userPwd, salt ) );
				}

				siResult = IsValidRecordName ( userName, (char *)"/users", domain, niDirID );

#ifdef DEBUG
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
#else
				if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
#endif
				::ni_proplist_free( &niPropList );
				bFreePropList = false;
				siResult = ::ni_read( domain, &niDirID, &niPropList );
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

					siResult = NiLib2::DestroyDirVal( domain, &niDirID, (char*)"passwd", niValue );
				}

				siResult = NiLib2::InsertDirVal( domain, &niDirID, (char*)"passwd", hashPwd, 0 );
				siResult = MapNetInfoErrors( siResult );
			}
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

	if ( userName != nil )
	{
		free( userName );
		userName = nil;
	}

	if ( userPwd != nil )
	{
		free( userPwd );
		userPwd = nil;
	}

	if ( rootName != nil )
	{
		free( rootName );
		rootName = nil;
	}

	if ( rootPwd != nil )
	{
		free( rootPwd );
		rootPwd = nil;
	}

	return( siResult );

} // DoSetPassword


//------------------------------------------------------------------------------------
//	* DoSetPasswordAsRoot
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoSetPasswordAsRoot ( sNIContextData *inContext, tDataBuffer *inAuthData )
{
	sInt32				siResult		= eDSAuthFailed;
	bool				bFreePropList	= false;
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
  	void			   *domain			= nil;

	gNetInfoMutex->Wait();

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );

		pData = inAuthData->fBufferData;
		buffSize = inAuthData->fBufferSize;
		buffLen = inAuthData->fBufferLength;

		domain = inContext->fDomain;

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
#ifdef TIM_CLIENT_PRESENT
		//opt out early if we know tim is not there **** check on local first
		if ( IsTimRunning() )
		{
			// Get a tim handle
			pTimHndl = ::timServerForDomain( inContext->fDomain );
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
										CShared::LogIt( 0x0F, "-- timSetPasswordForUserAsRoot -- failed with %l.", timResult );
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
//			char		   *niPwd			= nil;

			//need some code here to directly change the NI Crypt password
			//first check to see that the old password is valid
			CShared::LogIt( 0x0F, "Attempting UNIX Crypt password change" );
			siResult = IsValidRecordName ( userName, (char *)"/users", domain, niDirID );

#ifdef DEBUG
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
#else
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
#endif

			siResult = ::ni_read( domain, &niDirID, &niPropList );
#ifdef DEBUG
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthUnknownUser );
#else
			if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
#endif
			bFreePropList = true;

			niWhere = ::ni_proplist_match( niPropList, "passwd", nil );

			if (siResult == eDSNoErr)
			{
				// we successfully authenticated with old password, now change to new password.
				//set with the new password
				const char *saltchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

				::memset( hashPwd, 0, 32 );

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

					siResult = NiLib2::DestroyDirVal( domain, &niDirID, (char*)"passwd", niValue );
				}

				siResult = NiLib2::InsertDirVal( domain, &niDirID, (char*)"passwd", hashPwd, 0 );
				siResult = MapNetInfoErrors( siResult );
			}
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

	if ( userName != nil )
	{
		free( userName );
		userName = nil;
	}

	if ( newPasswd != nil )
	{
		free( newPasswd );
		newPasswd = nil;
	}

	return( siResult );

} // DoSetPasswordAsRoot


//------------------------------------------------------------------------------------
//	* DoChangePassword
//------------------------------------------------------------------------------------

sInt32 CNiPlugIn::DoChangePassword ( sNIContextData *inContext, tDataBuffer *inAuthData )
{
	sInt32			siResult		= eDSAuthFailed;
#ifdef TIM_CLIENT_PRESENT
	TIMHandle	   *timHandle		= nil;
#endif
	bool			bFreePropList	= false;
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
	char		   *pathStr			= nil;
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

		oldPwd = (char *)::calloc( 1, OldPwdLen + 1 );
		::memcpy( oldPwd, pData, OldPwdLen );
		pData += OldPwdLen;
		offset += OldPwdLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		::memcpy( &newPwdLen, pData, sizeof( unsigned long ) );
   		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + newPwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		newPwd = (char *)::calloc( 1, newPwdLen + 1 );
		::memcpy( newPwd, pData, newPwdLen );
		pData += newPwdLen;
		offset += newPwdLen;

#ifdef TIM_CLIENT_PRESENT
		//opt out early if we know tim is not there **** check on local first
		if ( IsTimRunning() )
		{
			timHandle = ::timServerForDomain( inContext->fDomain );
			if ( timHandle == nil ) throw( (sInt32)eDSAuthFailed );

			// Set the password
			siResult = ::timSetPasswordWithTIMHandle( timHandle, name, oldPwd, name, newPwd );
#ifdef DEBUG
			if ( siResult != TimStatusOK )
			{
				CShared::LogIt( 0x0F, "-- timSetPassword -- failed with %l.", siResult );
			}
#endif
			siResult = MapAuthResult( siResult );
			::timHandleFree( timHandle );
			timHandle = nil;
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
			CShared::LogIt( 0x0F, "Attempting UNIX Crypt password change" );
			pathStr = BuildDomainPathFromName( inContext->fDomainName );
			if ( pathStr == nil ) throw( (sInt32)eDSAuthFailed );
			
			niStatus = ::ni_open( nil, pathStr, &domain );
			if ( niStatus != 0) throw( (sInt32)eDSAuthFailed );

			niStatus = ::ni_setuser( domain, name );
			niStatus = ::ni_setpassword( domain, oldPwd );

			siResult = IsValidRecordName ( name, (char *)"/users", domain, niDirID );

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

			//account for the case where niPwd == "" such that we will auth if pwdLen is 0
			if (::strcmp(niPwd,"") != 0)
			{
				salt[ 0 ] = niPwd[0];
				salt[ 1 ] = niPwd[1];
				salt[ 2 ] = '\0';

				::memset( hashPwd, 0, 32 );
				::strcpy( hashPwd, ::crypt( oldPwd, salt ) );

				siResult = eDSAuthFailed;
				if ( ::strcmp( hashPwd, niPwd ) == 0 )
				{
					siResult = eDSNoErr;
				}
			
			}
			else // niPwd is == ""
			{
				if (::strcmp(oldPwd,"") != 0)
				{
					siResult = eDSNoErr;
				}
			}

			if (siResult == eDSNoErr)
			{
				// we successfully authenticated with old password, now change to new password.
				//set with the new password
				const char *saltchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

				::memset( hashPwd, 0, 32 );

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

	if ( pathStr != nil )
	{
		free( pathStr );
		pathStr = nil;
	}

	if ( name != nil )
	{
		free( name );
		name = nil;
	}

	if ( newPwd != nil )
	{
		free( newPwd );
		newPwd = nil;
	}

	if ( oldPwd != nil )
	{
		free( oldPwd );
		oldPwd = nil;
	}

	return( siResult );

} // DoChangePassword


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

		timHandle = timServerForDomain( inContext->fDomain );
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

#ifdef DEBUG
		if ( siResult != TimStatusOK )
		{
			CShared::LogIt( 0x0F, "-- timAuthenticateSMBLMKey -- failed with %l for key %l.", siResult, inWhichOne );
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
		free( pC8 );
		pC8 = nil;
	}

	if ( pP24 != nil )
	{
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

		// First pass setup
		if ( pContinue->fAuthPass == 0 )
		{
			pAuthStr = GetAuthString( inAuthMethod );
			if ( pAuthStr == nil ) throw( (sInt32)eDSAuthMethodNotSupported );

			// Is there an auth server running?
			authHndl = ::timServerForDomain( inContext->fDomain );
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
					CShared::LogIt( 0x0F, "Authentication Server Error = %d.", siResult, 0 );
					siResult = eDSAuthServerError;
					break;
			}

			if ( siResult != TimStatusOK )
			{
				CShared::LogIt( 0x0F, "-- DoMultiPassAuth -- failed with %l.", siResult );
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


//------------------------------------------------------------------------------------
//	* DoNodeNativeAuth
//------------------------------------------------------------------------------------

//this is REALLY 2-way random with tim authserver ie. name is very mis-leading
sInt32 CNiPlugIn::DoNodeNativeAuth ( sNIContextData *inContext, tDataBuffer *inAuthData, bool inAuthOnly )
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
//							::memset( pwd, 0, pwdLen + 1 );
							pData += sizeof( unsigned long );
							::memcpy( pwd, pData, pwdLen );
							pData += pwdLen;

							CShared::LogIt( 0x0F, "Attempting Auth Server Node Native Authentication." );

							timHandle = timServerForDomain( inContext->fDomain );
							if ( timHandle == nil ) throw( (sInt32)eDSAuthFailed );
							
							siResult = ::timAuthenticate2WayRandomWithTIMHandle( timHandle, userName, pwd );

							free( pwd );
							pwd = nil;
							timHandleFree( timHandle );
							timHandle = nil;
#ifdef DEBUG
							if ( siResult != TimStatusOK )
							{
								CShared::LogIt( 0x0F, "-- timAuthenticate2WayRandom -- failed with %l.", siResult );
							}
#endif
							siResult = MapAuthResult( siResult );

							if ( (siResult == eDSNoErr) && (inAuthOnly == false) )
							{
								siResult = AuthOpen( inContext, userName, pwd );
							}
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
//				else
//				{
//					siResult = eDSAuthMethodNotSupported;
//				}
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
		CShared::LogIt( 0x0F, "2 way random authentication error %l.", err );
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

sInt32 CNiPlugIn::DoUnixCryptAuth ( sNIContextData *inContext, tDataBuffer *inAuthData, bool inAuthOnly )
{
	sInt32			siResult		= eDSAuthFailed;
	bool			bFreePropList	= false;
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
	void		   *domain			= nil;
//	char			userPath[ 256 + 8 ] = "/users/";
	char			salt[ 9 ];
	char			hashPwd[ 32 ];
	ni_id			niDirID;
	ni_proplist		niPropList;

	gNetInfoMutex->Wait();

	try
	{
		domain = inContext->fDomain;

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

		CShared::LogIt( 0x0F, "Attempting UNIX Crypt authentication" );

//		::strcat( userPath, name );
		siResult = IsValidRecordName ( name, (char *)"/users", domain, niDirID );
//		siResult = ::ni_pathsearch( domain, &niDirID, userPath );

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
			
		}
		else // niPwd is == ""
		{
			if ( ::strcmp(pwd,"") == 0 )
			{
				siResult = eDSNoErr;
			}
		}

		if ( (siResult == eDSNoErr) && (inAuthOnly == false) )
		{
			siResult = AuthOpen( inContext, name, pwd );
		}
	}

	catch( sInt32 err )
	{
		CShared::LogIt( 0x0F, "Crypt authentication error %l", err );
		siResult = err;
	}

	if ( bFreePropList )
	{
		::ni_proplist_free( &niPropList );
	}

	gNetInfoMutex->Signal();
	
	if ( name != nil )
	{
		free( name );
		name = nil;
	}

	if ( pwd != nil )
	{
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

							CShared::LogIt( 0x0F, "Attempting Auth ValidateDigest." );

							pTimHndl = ::timServerForDomain( inContext->fDomain );
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

							free( pResponse );
							pResponse = nil;
							timHandleFree( pTimHndl );
							pTimHndl = nil;
#ifdef DEBUG
							if ( siResult != TimStatusOK )
							{
								CShared::LogIt( 0x0F, "-- timAuthenticateCRAM_MD5WithTIMHandle -- failed with %l.", siResult );
							}
#endif
							siResult = MapAuthResult( siResult );

							free( pResponse );
							pResponse = nil;

						}
						else
						{
							siResult = siBuffErr;
						}
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
		CShared::LogIt( 0x0F, "2 way random authentication error %l.", err );
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
							 const char * inPassword )
{
	sInt32			siResult		= eDSAuthFailed;
	void		   *domain			= nil;
	char		   *pathStr			= nil;
	
	gNetInfoMutex->Wait();
	
	try 
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );
		if ( inUserName == nil ) throw( (sInt32)eDSAuthFailed );
		if ( inPassword == nil ) throw( (sInt32)eDSAuthFailed );		
		pathStr = BuildDomainPathFromName( inContext->fDomainName );
		if ( pathStr == nil ) throw( (sInt32)eDSAuthFailed ); //shouldn't happen since we opened the node OK
		
		siResult = ::ni_open( nil, pathStr, &domain );
		if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
		// we need to establish authentication against this domain
		siResult = ::ni_setuser( domain, inUserName );
		if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
		siResult = ::ni_setpassword( domain, inPassword );
		if ( siResult != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
		if ( siResult == NI_OK )
		{
			// free up the old one and save our new domain in the context
			if ( inContext->fDontUseSafeClose )
			{
				::ni_free(inContext->fDomain);
			}
			else if (inContext->fDomainName != nil)
			{
				CNetInfoPI::SafeClose( inContext->fDomainName );
			}
			inContext->fDomain = domain;
			inContext->fDontUseSafeClose = true;
			if ( inContext->fAuthenticatedUserName != NULL )
			{
				free( inContext->fAuthenticatedUserName );
			}
			// check for admin group, and treat admin users as root
			if (UserIsAdmin(inUserName, domain)) {
				inContext->fAuthenticatedUserName = strdup( "root" );
			} else {
				inContext->fAuthenticatedUserName = strdup( inUserName );
			}
			// save it in the context
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
//	* MapAttrToNetInfoType
// ---------------------------------------------------------------------------

char* CNiPlugIn::MapAttrToNetInfoType ( const char *inAttrType )
{
	char	   *outResult	= nil;
	uInt32		uiStrLen	= 0;
	uInt32		uiNativeLen	= ::strlen( kDSNativeAttrTypePrefix );
	uInt32		uiStdLen	= ::strlen( kDSStdAttrTypePrefix );

	if ( inAttrType != nil )
	{
		uiStrLen = ::strlen( inAttrType );

		// First look for native attribute type
		if ( ::strncmp( inAttrType, kDSNativeAttrTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				outResult = new char[ uiStrLen + 1 ];
				::strcpy( outResult, inAttrType + uiNativeLen );
			}
		}
		else if ( ::strncmp( inAttrType, kDSStdAttrTypePrefix, uiStdLen ) == 0 )
		{
			for ( int i = 0; i < kAttrConsts; i++ )
			{
				if ( ::strcmp( inAttrType, sAttrMap[ i ][ 0 ] ) == 0 )
				{
					outResult = new char[ ::strlen( sAttrMap[ i ][ 1 ] ) + 1 ];
					::strcpy( outResult, sAttrMap[ i ][ 1 ] );
					break;
				}
			}
		}
		else if ( ::strcmp( inAttrType, kDSAttributesAll ) == 0 )
		{
			outResult = new char[ ::strlen( kDSAttributesAll ) + 1 ];
			::strcpy( outResult, kDSAttributesAll );
		}
		else if ( ::strcmp( inAttrType, kDSAttributesStandardAll ) == 0 )
		{
			outResult = new char[ ::strlen( kDSAttributesStandardAll ) + 1 ];
			::strcpy( outResult, kDSAttributesStandardAll );
		}
	}

	return( outResult );

} // MapAttrToNetInfoType


// ---------------------------------------------------------------------------
//	* MapNetInfoAttrToDSType
// ---------------------------------------------------------------------------

char* CNiPlugIn::MapNetInfoAttrToDSType ( const char *inAttrType )
{
	char	   *outResult	= nil;

	if ( inAttrType != nil )
	{
		// Look for a standard type
		for ( int i = 0; i < kAttrConsts; i++ )
		{
			if ( ::strcmp( inAttrType, sAttrMap[ i ][ 1 ] ) == 0 )
			{
				outResult = new char[ ::strlen( sAttrMap[ i ][ 0 ] ) + 1 ];
				::strcpy( outResult, sAttrMap[ i ][ 0 ] );
				break;
			}
		}

		if ( outResult == nil )
		{
			outResult = new char[ ::strlen( kDSNativeAttrTypePrefix ) + ::strlen( inAttrType ) + 1 ];
			::strcpy( outResult, kDSNativeAttrTypePrefix );
			::strcat( outResult, inAttrType );
		}
	}

	return( outResult );

} // MapNetInfoAttrToDSType


// ---------------------------------------------------------------------------
//	* MapRecToNetInfoType
// ---------------------------------------------------------------------------

char* CNiPlugIn::MapRecToNetInfoType ( const char *inRecType )
{
	char	   *outResult	= nil;
	uInt32		uiStrLen	= 0;
	uInt32		uiNativeLen	= ::strlen( kDSNativeRecordTypePrefix );
	uInt32		uiStdLen	= ::strlen( kDSStdRecordTypePrefix );


	// xxxx check for imbedded '/'

	if ( inRecType != nil )
	{
		uiStrLen = ::strlen( inRecType );

		// First look for native record type
		if ( ::strncmp( inRecType, kDSNativeRecordTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				outResult = new char[ uiStrLen + 2 ];
				::strcpy( outResult, inRecType + uiNativeLen );
			}
		}
		else if ( ::strncmp( inRecType, kDSStdRecordTypePrefix, uiStdLen ) == 0 )
		{
			for ( int i = 0; i < kRecConsts; i++ )
			{
				if ( ::strcmp( inRecType, sRecMap[ i ][ 0 ] ) == 0 )
				{
					outResult = new char[ ::strlen( sRecMap[ i ][ 1 ] ) + 1 ];
					::strcpy( outResult, sRecMap[ i ][ 1 ] );
					break;
				}
			}
		}
		//this is not possible ie. ni_pathsearch needs a NI record type
		//else if ( ::strcmp( inRecType, kDSRecordsAll ) == 0 )
		//{
			//outResult = new char[ ::strlen( kDSRecordsAll ) + 1 ];
			//::strcpy( outResult, kDSRecordsAll );
		//}
	}

	return( outResult );

} // MapRecToNetInfoType


// ---------------------------------------------------------------------------
//	* MapNetInfoRecToDSType
// ---------------------------------------------------------------------------

char* CNiPlugIn::MapNetInfoRecToDSType ( const char *inRecType )
{
	char	   *outResult	= nil;

	if ( inRecType != nil )
	{
		// Look for a standard type
		for ( int i = 0; i < kRecConsts; i++ )
		{
			if ( ::strcmp( inRecType, sRecMap[ i ][ 1 ] ) == 0 )
			{
				outResult = new char[ ::strlen( sRecMap[ i ][ 0 ] ) + 1 ];
				::strcpy( outResult, sRecMap[ i ][ 0 ] );
				break;
			}
		}

		if ( outResult == nil )
		{
			outResult = new char[ ::strlen( kDSNativeRecordTypePrefix ) + ::strlen( inRecType ) + 1 ];
			::strcpy( outResult, kDSNativeRecordTypePrefix );
			::strcat( outResult, inRecType );
		}
	}

	return( outResult );

} // MapNetInfoRecToDSType


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
		uiNativeLen	= ::strlen( kDSNativeAuthMethodPrefix );

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
//	* GetAuthMethod
// ---------------------------------------------------------------------------

sInt32 CNiPlugIn::GetAuthMethod ( tDataNode *inData, uInt32 *outAuthMethod )
{
	sInt32			siResult		= eDSNoErr;
	uInt32			uiNativeLen		= 0;
	char		   *p				= nil;

	if ( inData == nil )
	{
		*outAuthMethod = kAuthUnknowMethod;
#ifdef DEBUG
		return( eDSAuthParameterError );
#else
		return( eDSAuthFailed );
#endif
	}

	p = (char *)inData->fBufferData;

	CShared::LogIt( 0x0F, "Using authentication method %s.", p );

	if ( ::strcmp( p, kDSStdAuthClearText ) == 0 )
	{
		// Clear text auth method
		*outAuthMethod = kAuthClearText;
	}
	else if ( ::strcmp( p, kDSStdAuthNodeNativeClearTextOK ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeClearTextOK;
	}
	else if ( ::strcmp( p, kDSStdAuthNodeNativeNoClearText ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeNoClearText;
	}
	else if ( ::strcmp( p, kDSStdAuthCrypt ) == 0 )
	{
		// Unix Crypt auth method
		*outAuthMethod = kAuthCrypt;
	}
	else if ( ::strcmp( p, kDSStdAuth2WayRandom ) == 0 )
	{
		// Two way random auth method
		*outAuthMethod = kAuth2WayRandom;
	}
	else if ( ::strcmp( p, kDSStdAuthSMB_NT_Key ) == 0 )
	{
		// Two way random auth method
		*outAuthMethod = kAuthSMB_NT_Key;
	}
	else if ( ::strcmp( p, kDSStdAuthSMB_LM_Key ) == 0 )
	{
		// Two way random auth method
		*outAuthMethod = kAuthSMB_LM_Key;
	}
	else if ( ::strcmp( p, kDSStdAuthSetPasswd ) == 0 )
	{
		// Admin set password
		*outAuthMethod = kAuthSetPasswd;
	}
	else if ( ::strcmp( p, kDSStdAuthSetPasswdAsRoot ) == 0 )
	{
		// Admin set password
		*outAuthMethod = kAuthSetPasswdAsRoot;
	}
	else if ( ::strcmp( p, kDSStdAuthChangePasswd ) == 0 )
	{
		// User change password
		*outAuthMethod = kAuthChangePasswd;
	}
	else if ( ::strcmp( p, kDSStdAuthAPOP ) == 0 )
	{
		// APOP auth method
		*outAuthMethod = kAuthAPOP;
	}
	else if ( ::strcmp( p, kDSStdAuthCRAM_MD5 ) == 0 )
	{
		// CRAM-MD5 auth method
		*outAuthMethod = kAuthCRAM_MD5;
	}
	else
	{
		uiNativeLen	= ::strlen( kDSNativeAuthMethodPrefix );

		if ( ::strncmp( p, kDSNativeAuthMethodPrefix, uiNativeLen ) == 0 )
		{
			// User change password
			*outAuthMethod = kAuthNativeMethod;
		}
		else
		{
			*outAuthMethod = kAuthUnknowMethod;
#ifdef DEBUG
			siResult = eDSAuthMethodNotSupported;
#else
			siResult = eDSAuthFailed;
#endif
		}
	}

	return( siResult );

} // GetAuthMethod


// ---------------------------------------------------------------------------
//	* IsValidRecordName
// ---------------------------------------------------------------------------

sInt32 CNiPlugIn::IsValidRecordName ( const char	*inRecName,
									  const char	*inRecType,
									  void			*inDomain,
									  ni_id			&outDirID )
{
	sInt32			siResult	= eDSInvalidRecordName;
	char		   *pData		= nil;
	ni_status		niStatus	= NI_OK;

	gNetInfoMutex->Wait();

	try
	{
		if ( inDomain == nil ) throw( (sInt32)eDSInvalidDomain );
		if ( inRecName == nil ) throw( (sInt32)eDSInvalidRecordName );
		if ( inRecType == nil ) throw( (sInt32)eDSInvalidRecordType );

		pData = BuildRecordNamePath( inRecName, inRecType );
		if ( pData != nil )
		{
			niStatus = ::ni_pathsearch( inDomain, &outDirID, pData );
			if ( niStatus == NI_OK )
			{
				siResult = eDSNoErr;
			}
			free( pData );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // IsValidRecordName


// ---------------------------------------------------------------------------
//	* BuildRecordNamePath
// ---------------------------------------------------------------------------

char *CNiPlugIn:: BuildRecordNamePath (	const char	*inRecName,
										const char	*inRecType )
{
	const char	   *pData		= nil;
	CString			csPath( 128 );
	char		   *outPath		= nil;
	uInt32			recNameLen	= 0;
	uInt32			nativeLen	= 0;
	uInt32			stdLen		= 0;

	try
	{
		if ( inRecName == nil ) throw( (sInt32)eDSInvalidRecordName );
		if ( inRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		recNameLen	= ::strlen( inRecName );
		nativeLen	= ::strlen( kDSNativeAttrTypePrefix );
		stdLen		= ::strlen( kDSStdAttrTypePrefix );

		if ( ::strncmp( inRecName, kDSNativeAttrTypePrefix, nativeLen ) == 0  )
		{
			if ( recNameLen > nativeLen )
			{
				pData = inRecName + nativeLen;
			}
		}
		else if ( ::strncmp( inRecName, kDSStdAttrTypePrefix, stdLen ) == 0  )
		{
			if ( recNameLen > stdLen )
			{
				pData = inRecName + stdLen;
			}
		}
		else
		{
			pData = inRecName;
		}

		if ( pData != nil )
		{
			//KW check if the recordname has "/" or "\\" in it
			//if so then replace the "/" with "\\/" so that NetInfo can handle the forward slashes
			//separate of the subdirectory delimiters
			//also replace the "\\" with "\\\\" so that NetInfo can handle the backslashes if
			//the intent is for them to be inside the name itself
			if ( (::strstr( pData, "/" ) != nil) || (::strstr( pData, "\\" ) != nil) )
			{
				csPath.Set( "/" );
				csPath.Append( inRecType );
				csPath.Append( "/" );
				while(pData[0] != '\0')
				{
					if (pData[0] == '/')
					{
						csPath.Append( "\\/" );
					}
					else if (pData[0] == '\\')
					{
						csPath.Append( "\\\\" );
					}
					else
					{
						csPath.Append( pData[0] );
					}
					pData++;
				}
			}
			else
			{
				csPath.Set( "/" );
				csPath.Append( inRecType );
				csPath.Append( "/" );
				csPath.Append( pData );

			}
			//check for the case of trying to access the root of netinfo above all the records
			//ie. record type was "/" and record name was also "/" which led to csPath of "///\\/"
			if (strcmp(csPath.GetData(),"///\\/") == 0)
			{
				outPath = (char *)::calloc( 2, sizeof(char));
				if ( outPath == nil ) throw( (sInt32)eMemoryError );
				strcpy(outPath,"/");
			}
			else
			{
				outPath = (char *)::calloc( csPath.GetLength() + 1, sizeof(char));
				if ( outPath == nil ) throw( (sInt32)eMemoryError );
				strcpy(outPath,csPath.GetData());
			}
		}
	}

	catch( sInt32 err )
	{
		if (outPath != nil)
		{
			free(outPath);
			outPath = nil;
		}
	}

	return( outPath );

} // BuildRecordNamePath

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

	niStatus = ::ni_open( NULL, ".", &domain );
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
		ni_free(domain);
		domain = NULL;
	}

	gNetInfoMutex->Signal();
#endif
	return bTimRunning;
} // IsTimRunning()

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
//	* MapNetInfoErrors
//
//		- xxxx make sure i do the right thing with the errors
// ---------------------------------------------------------------------------

sInt32 CNiPlugIn::MapNetInfoErrors ( sInt32 inNiError )
{
	sInt32		siOutError	= 0;

	switch ( inNiError )
	{
		case NI_OK:
			siOutError = eDSNoErr;
			break;

		case NI_BADID:
			siOutError = eUnknownPlugIn;
			break;

		case NI_STALE:
			siOutError = eUnknownPlugIn;
			break;

		case NI_NOSPACE:
			siOutError = eUnknownPlugIn;
			break;

		case NI_PERM:
			siOutError = eDSPermissionError;
			break;

		case NI_NODIR:
			siOutError = eDSInvalidRecordName;
			break;

		case NI_NOPROP:
			siOutError = eUnknownPlugIn;
			break;

		case NI_NONAME:
			siOutError = eUnknownPlugIn;
			break;

		case NI_NOTEMPTY:
			siOutError = eUnknownPlugIn;
			break;

		case NI_UNRELATED:
			siOutError = eUnknownPlugIn;
			break;

		case NI_SERIAL:
			siOutError = eUnknownPlugIn;
			break;

		case NI_NETROOT:
			siOutError = eUnknownPlugIn;
			break;

		case NI_NORESPONSE:
			siOutError = eUnknownPlugIn;
			break;

		case NI_RDONLY:
			siOutError = eDSReadOnly;
			break;

		case NI_SYSTEMERR:
			siOutError = eUnknownPlugIn;
			break;

		case NI_ALIVE:
			siOutError = eUnknownPlugIn;
			break;

		case NI_NOTMASTER:
			siOutError = eUnknownPlugIn;
			break;

		case NI_CANTFINDADDRESS:
			siOutError = eUnknownPlugIn;
			break;

		case NI_DUPTAG:
			siOutError = eUnknownPlugIn;
			break;

		case NI_NOTAG:
			siOutError = eUnknownPlugIn;
			break;

		case NI_AUTHERROR:
			siOutError = eUnknownPlugIn;
			break;

		case NI_NOUSER:
			siOutError = eUnknownPlugIn;
			break;

		case NI_MASTERBUSY:
			siOutError = eUnknownPlugIn;
			break;

		case NI_INVALIDDOMAIN:
			siOutError = eDSInvalidDomain;
			break;

		case NI_BADOP:
			siOutError = eUnknownPlugIn;
			break;

		case NI_FAILED:
			siOutError = eUnknownPlugIn;
			break;

		default:
			siOutError = eUnknownPlugIn;
			break;
	}

	return( siOutError );

} // MapNetInfoErrors


// ---------------------------------------------------------------------------
//	* CalcCRC
// ---------------------------------------------------------------------------

uInt32 CNiPlugIn::CalcCRC ( const char *inStr )
{
	const char 	   *p			= inStr;
	sInt32			siI			= 0;
	sInt32			siStrLen	= 0;
	uInt32			uiCRC		= 0xFFFFFFFF;
	CRCCalc			aCRCCalc;

	if ( inStr != nil )
	{
		siStrLen = ::strlen( inStr );

		for ( siI = 0; siI < siStrLen; ++siI )
		{
			uiCRC = aCRCCalc.UPDC32( *p, uiCRC );
			p++;
		}
	}

	return( uiCRC );

} // CalcCRC


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
			else if (inContext->fDomainName != nil)
			{
				CNetInfoPI::SafeClose( inContext->fDomainName );
				inContext->fDomain = nil;
			}
		}
		//now we do own this pointer
		if (inContext->fDomainName != nil)
		{
			free(inContext->fDomainName);
			inContext->fDomainName = nil;
		}
		if (inContext->fRecType != nil)
		{
			free(inContext->fRecType);
			inContext->fRecType = nil;
		}
		if (inContext->fRecName != nil)
		{
			free(inContext->fRecName);
			inContext->fRecName = nil;
		}
		if (inContext->fAuthenticatedUserName != NULL)
		{
			free(inContext->fAuthenticatedUserName);
			inContext->fAuthenticatedUserName = NULL;
		}
		//dirID is a struct of two ulongs
		inContext->offset				= 0;
		inContext->index				= 0;
		inContext->fDontUseSafeClose	= false;
        
        if ( inContext->fPWSNodeRef != 0 )
            dsCloseDirNode(inContext->fPWSNodeRef);
        if ( inContext->fPWSRef != 0 )
            dsCloseDirService(inContext->fPWSRef);
	}
    
	return( siResult );

} // CleanContextData


// ---------------------------------------------------------------------------
//	* DoesThisMatch
// ---------------------------------------------------------------------------

bool CNiPlugIn::DoesThisMatch ( const char		   *inString,
								const char		   *inPatt,
								tDirPatternMatch	inHow )
{
	bool		bOutResult	= false;
	CFMutableStringRef	strRef	= CFStringCreateMutable(NULL, 0);
	CFMutableStringRef	patRef	= CFStringCreateMutable(NULL, 0);
	CFRange		range;

	if ( (inString == nil) || (inPatt == nil) || (strRef == nil) || (patRef == nil) )
	{
		return( false );
	}

	CFStringAppendCString( strRef, inString, kCFStringEncodingUTF8 );
	CFStringAppendCString( patRef, inPatt, kCFStringEncodingUTF8 );	
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

	CFRelease( strRef );
	strRef = nil;
	CFRelease( patRef );
	patRef = nil;

	return( bOutResult );

} // DoesThisMatch


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

	// JT we need to first make sure the string is composed UTF8
	//allocate at most twice the length plus one ie. assume worst case to escape every character
	outRegExp = (char *) calloc( 1, 1 + 2*strlen(inString) );

	j = 0;
	inLength = strlen(inString);
	for (i=0; i< inLength; i++)
	{
		//workaround since ni_search cannot handle UTF-8 characters
		//regex used by ni_search also has problems with ')' and '('
		if ( (isascii(inString[i])) && ( (inString[i]) != '(' ) && ( (inString[i]) != ')' ) )
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
//	* ContinueDeallocProc
// ---------------------------------------------------------------------------

void CNiPlugIn::ContinueDeallocProc ( void* inContinueData )
{
	sNIContinueData *pContinue = (sNIContinueData *)inContinueData;

	if ( pContinue != nil )
	{
	// fAuthHndl is only used in DoMultiPassAuth
	// and it is freed? there by timSessionEnd
		if ( pContinue->fAliasList != nil )
		{
			::dsDataListDeallocatePriv( pContinue->fAliasList );
			//need to free the header as well
			free( pContinue->fAliasList );
			pContinue->fAliasList = nil;
		}

		if ( pContinue->fAliasAttribute != nil )
		{
			::dsDataListDeallocatePriv( pContinue->fAliasAttribute );
			//need to free the header as well
			free( pContinue->fAliasAttribute );
			pContinue->fAliasAttribute = nil;
		}

		if ( pContinue->fDataBuff != nil )
		{
			::dsDataBufferDeallocatePriv( pContinue->fDataBuff );
			pContinue->fDataBuff = nil;
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
//	* BuildDomainPathFromName
// ---------------------------------------------------------------------------

char* CNiPlugIn::BuildDomainPathFromName( char* inDomainName )
{
	CString		csPathStr( 128 );
	char	   *pathStr = nil;	

	if (::strncmp( inDomainName, kstrRootName, ::strlen( kstrRootName ) ) != 0)
	{
		return nil;
	}

	// Check for local domain
	if ( (::strcmp( inDomainName, kstrLocalDomain ) == 0) ||
		 (::strcmp( inDomainName + ::strlen( kstrRootNodeName ), CNetInfoPI::fLocalDomainName ) == 0) ||
		 (::strcmp( inDomainName, kstrDefaultLocalNodeName ) == 0 ) )
	{
		csPathStr.Set( "." );
	}
	else if ( (::strcmp( inDomainName, kstrRootName ) == 0) ||
			  (::strcmp( inDomainName, kstrPrefixName ) == 0) ||
			  (::strcmp( inDomainName, kstrRootNodeName ) == 0) )
	{
		// Fully qualified root
		csPathStr.Set( kstrDelimiter );
	}
	else if ( ::strcmp( inDomainName, kstrParentDomain ) == 0 )
	{
		csPathStr.Set( ".." );
	}
	else
	{
		csPathStr.Set( inDomainName + 13 ); //this strips off the first "/NetInfo/root" prefix
	}

	pathStr = (char *)::calloc( csPathStr.GetLength() + 1, sizeof( char ) );
	strcpy( pathStr, csPathStr.GetData());
	
	return pathStr;
} // BuildDomainPathFromName


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
		inConstRegExpRecName = BuildRegExp(inRealname);
		niStatus = ::ni_search( inDomain, &niDirID, (char *)"realname", inConstRegExpRecName, REG_ICASE, &niEntryList );
		free(inConstRegExpRecName);
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
											(const char*)inRealname, eDSExact ))
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
			}
		
		} // found some realname matches using ni_search
								
	} //if ( niStatus == NI_OK ) for ::ni_pathsearch( inDomain, &niDirID, "/users" );

	gNetInfoMutex->Signal();

	return( siResult );

} // IsValidRealname


// ---------------------------------------------------------------------------
//	* UserIsAdmin
// ---------------------------------------------------------------------------

bool CNiPlugIn::UserIsAdmin ( const char   *inUserName,
							  void   *inDomain )
{
	ni_id			niDirID;
	ni_status		niStatus	= NI_OK;
	ni_proplist		niPropList;
	ni_index		niIndex		= 0;
	bool			isAdmin = false;

	if ( inUserName == NULL || inDomain == NULL ) {
		return false;
	}
	
	gNetInfoMutex->Wait();

	niStatus = ::ni_pathsearch( inDomain, &niDirID, "/groups/admin" );

	if (niStatus == NI_OK) {
		niStatus = ::ni_read( inDomain, &niDirID, &niPropList );

		if (niStatus == NI_OK) {
			niIndex = ::ni_proplist_match( niPropList, "users", nil );
			if (niIndex != NI_INDEX_NULL) {
				niIndex = ni_namelist_match(niPropList.nipl_val[niIndex].nip_val,inUserName);
				isAdmin = (niIndex != NI_INDEX_NULL);
			}
			::ni_proplist_free( &niPropList );
		}
	}
	
	gNetInfoMutex->Signal();

	return isAdmin;
}


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
					if ((niValues.ni_namelist_len > 0)
						&& (niValues.ni_namelist_val[0] != NULL))
					{
						userName = strdup( niValues.ni_namelist_val[0] );
						ni_namelist_free( &niValues );
						break;
					}
					ni_namelist_free( &niValues );
				}
			}
		} //ni_search successful
		if ( niEntryList.ni_entrylist_len > 0 )
		{
			::ni_entrylist_free(&niEntryList);
		}
	}

	gNetInfoMutex->Signal();

	return userName;
}


// ---------------------------------------------------------------------------
//	* GetUserNameFromAuthBuffer
//    retrieve the username from a standard auth buffer
//    buffer format should be 4 byte length followed by username, then optional
//    additional data after. Buffer length must be at least 5 (length + 1 character name)
// ---------------------------------------------------------------------------

sInt32 CNiPlugIn::GetUserNameFromAuthBuffer ( tDataBufferPtr inAuthData, unsigned long inUserNameIndex, 
											  char  **outUserName )
{
	tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
	if (dataList != NULL)
	{
		*outUserName = dsDataListGetNodeStringPriv(dataList, inUserNameIndex);
		// this allocates a copy of the string
		
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
		return eDSNoErr;
	}
	return eDSInvalidBuffFormat;
}


// ---------------------------------------------------------------------------
//	* GetAuthAuthorityHandler
// ---------------------------------------------------------------------------

AuthAuthorityHandlerProc CNiPlugIn::GetAuthAuthorityHandler ( const char* inTag )
{
	if (inTag == NULL)
	{
		return NULL;
	}
	for (unsigned int i = 0; i < kAuthAuthorityHandlerProcs; ++i)
	{
		if (strcasecmp(inTag,sAuthAuthorityHandlerProcs[i].fTag) == 0)
		{
			// found it
			return sAuthAuthorityHandlerProcs[i].fHandler;
		}
	}
	return NULL;
}


// ---------------------------------------------------------------------------
//	* ParseAuthAuthority
//    retrieve version, tag, and data from authauthority
//    format is version;tag;data
// ---------------------------------------------------------------------------

sInt32 CNiPlugIn::ParseAuthAuthority ( const char	 * inAuthAuthority,
									   char			** outVersion,
									   char			** outAuthTag,
									   char			** outAuthData )
{
	char* authAuthority = NULL;
	char* current = NULL;
	char* tempPtr = NULL;
	sInt32 result = eDSAuthFailed;
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
	do {
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outVersion = strdup(tempPtr);
		
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outAuthTag = strdup(tempPtr);
		
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outAuthData = strdup(tempPtr);
		
		result = eDSNoErr;
	} while (false);
	
	free(authAuthority);
	authAuthority = NULL;
	return result;
}

