/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * @header CConfigurePlugin
 */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>			//used for mkdir and stat

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <Security/Authorization.h>

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"

#include "SharedConsts.h"
#include "PrivateTypes.h"
#include "DSUtils.h"
#include "COSUtils.h"
#include "CAttributeList.h"
#include "CPlugInRef.h"
#include "CBuff.h"
#include "CDataBuff.h"
#include "CLog.h"

#include "CConfigurePlugin.h"
#include "ServerModuleLib.h"
#include "PluginData.h"
#include "DSCThread.h"
#include "DSEventSemaphore.h"
#include "CContinue.h"
#include "CPlugInList.h"
#include "ServerControl.h"
#include "CPluginConfig.h"

typedef struct {
	uInt32				fRecNameIndex;
	uInt32				fRecTypeIndex;
	uInt32				fAllRecIndex;
	uInt32				fAttrIndex;
} sConfigContinueData;


// Globals ---------------------------------------------------------------------------

static	CPlugInRef	 	*gConfigNodeRef			= nil;
static	CContinue	 	*gConfigContinue		= nil;
static	DSEventSemaphore	*gKickConfigRequests	= nil;

extern	CPlugInList		*gPlugins;
extern	CPluginConfig   *gPluginConfig;

#define	kDSConfigPluginsRecType		"dsConfigType::Plugins"
#define	kDSConfigRecordsType		"dsConfigType::RecordTypes"
#define	kDSConfigAttributesType		"dsConfigType::AttributeTypes"
#define	kDSConfigRecordsAll			"dsConfigType::GetAllRecords"

#define	kDSConfigAttrVersion		"dsConfigAttrType::Version"
#define	kDSConfigAttrState			"dsConfigAttrType::State"
#define	kDSConfigAttrConfigAvail	"dsConfigAttrType::ConfigAvailable"
#define	kDSConfigAttrConfigFile		"dsConfigAttrType::ConfigFile"
#define	kDSConfigAttrPlugInIndex 	"dsConfigAttrType::PlugInIndex"

#define		kAttrTypeConsts		118

//sorted alphabetically for ease of source updates.
//ie. used mainly for reference purposes and display by
//Directory Access app but note this does not mean that
//this list is ready for display since the prefix of
// "dsAttrTypeStandard:" will usually be stripped for display
//and some of the constants do not follow naming conventions.
//Also note that kDS1 and kDSN are grouped separately
static const char *sAttrTypes[ kAttrTypeConsts ] =
{
	kDS1AttrAdminLimits,
	kDS1AttrAliasData,
	kDS1AttrAlternateDatastoreLocation,
	kDS1AttrAuthenticationHint,
	kDS1AttrCapabilities,
	kDS1AttrChange,
	kDS1AttrComment,
	kDS1AttrDataStamp,
	kDS1AttrDistinguishedName,
	kDS1AttrDNSDomain,
	kDS1AttrDNSNameServer,
	kDS1AttrENetAddress,
	kDS1AttrExpire,
	kDS1AttrFirstName,
	kDS1AttrGeneratedUID,
	kDS1AttrHomeDirectoryQuota,
	kDS1AttrHomeDirectorySoftQuota,
	kDS1AttrHomeLocOwner,
	kDS1AttrInternetAlias,
	kDS1AttrKDCConfigData,
	kDS1AttrLastName,
	kDS1AttrLocation,
	kDS1AttrMCXFlags,
	kDS1AttrMailAttribute,
	kDS1AttrMiddleName,
	kDS1AttrNFSHomeDirectory,
	kDS1AttrNote,
	kDS1AttrPassword,
	kDS1AttrPasswordPolicyOptions,
	kDS1AttrPasswordServerList,
	kDS1AttrPasswordServerLocation,
	kDS1AttrPicture,
	kDS1AttrPort,
	kDS1AttrPresetUserIsAdmin,
	kDS1AttrPrimaryGroupID,
	kDS1AttrPrinter1284DeviceID,
	kDS1AttrPrinterLPRHost,
	kDS1AttrPrinterLPRQueue,
	kDS1AttrPrinterMakeAndModel,
	kDS1AttrPrinterType,
	kDS1AttrPrinterURI,
	kDSNAttrPrinterXRISupported,
	kDS1AttrPrintServiceInfoText,
	kDS1AttrPrintServiceInfoXML,
	kDS1AttrPrintServiceUserData,
	kDS1AttrRealUserID,
	kDS1AttrRelativeDNPrefix,
	kDS1AttrSMBAcctFlags,
	kDS1AttrSMBGroupRID,
	kDS1AttrSMBHome,
	kDS1AttrSMBHomeDrive,
	kDS1AttrSMBKickoffTime,
	kDS1AttrSMBLogoffTime,
	kDS1AttrSMBLogonTime,
	kDS1AttrSMBPWDLastSet,
	kDS1AttrSMBProfilePath,
	kDS1AttrSMBRID,
	kDS1AttrSMBScriptPath,
	kDS1AttrSMBUserWorkstations,
	kDS1AttrServiceType,
	kDS1AttrSetupAdvertising,
	kDS1AttrSetupAutoRegister,
	kDS1AttrSetupLocation,
	kDS1AttrSetupOccupation,
	kDS1AttrUniqueID,
	kDS1AttrUserShell,
	kDS1AttrVFSDumpFreq,
	kDS1AttrVFSLinkDir,
	kDS1AttrVFSPassNo,
	kDS1AttrVFSType,
	kDS1AttrXMLPlist,
	kDSNAttrAddressLine1,
	kDSNAttrAddressLine2,
	kDSNAttrAddressLine3,
	kDSNAttrAreaCode,
	kDSNAttrAuthenticationAuthority,
	kDSNAttrBootParams,
	kDSNAttrBuilding,
	kDSNAttrCity,
	kDSNAttrComputers,
	kDSNAttrCountry,
	kDSNAttrDepartment,
	kDSNAttrDNSName,
	kDSNAttrEMailAddress,
	kDSNAttrFaxNumber,
	kDSNAttrGroup,
	kDSNAttrGroupMembership,
	kDSNAttrHTML,
	kDSNAttrHomeDirectory,
	kDSNAttrIMHandle,
	kDSNAttrIPAddress,
	kDSNAttrJobTitle,
	kDSNAttrKDCAuthKey,
	kDSNAttrKeywords,
	kDSNAttrLDAPReadReplicas,
	kDSNAttrLDAPWriteReplicas,
	kDSNAttrMCXSettings,
	kDSNAttrMIME,
	kDSNAttrMember,
	kDSNAttrMobileNumber,
	kDSNAttrNBPEntry,
	kDSNAttrNetGroups,
	kDSNAttrNickName,
	kDSNAttrOrganizationName,
	kDSNAttrPagerNumber,
	kDSNAttrPhoneNumber,
	kDSNAttrPGPPublicKey,
	kDSNAttrPostalAddress,
	kDSNAttrPostalCode,
	kDSNAttrNamePrefix,
	kDSNAttrProtocols,
	kDSNAttrRecordName,
	kDSNAttrState,
	kDSNAttrStreet,
	kDSNAttrNameSuffix,
	kDSNAttrURL,
	kDSNAttrURLForNSL,
	kDSNAttrVFSOpts
};


#define		kRecTypeConsts		41

//sorted alphabetically for ease of source updates.
//ie. used mainly for reference purposes and display by
//Directory Access app but note this does not mean that
//this list is ready for display since the prefix of
// "dsRecTypeStandard:" will usually be stripped for display
//and some of the constants do not follow naming conventions.
static const char *sRecTypes[ kRecTypeConsts ] =
{
	kDSStdRecordTypeAFPServer,
	kDSStdRecordTypeAFPUserAliases,
	kDSStdRecordTypeAliases,
	kDSStdRecordTypeAutoServerSetup,
	kDSStdRecordTypeBootp,
	kDSStdRecordTypeComputerLists,
	kDSStdRecordTypeComputers,
	kDSStdRecordTypeConfig,
	kDSStdRecordTypeEthernets,
	kDSStdRecordTypeFTPServer,
	kDSStdRecordTypeGroupAliases,
	kDSStdRecordTypeGroups,
	kDSStdRecordTypeHostServices,
	kDSStdRecordTypeHosts,
	kDSStdRecordTypeLDAPServer,
	kDSStdRecordTypeLocations,
	kDSStdRecordTypeMachines,
	kDSStdRecordTypeMeta,
	kDSStdRecordTypeMounts,
	kDSStdRecordTypeNFS,
	kDSStdRecordTypeNetDomains,
	kDSStdRecordTypeNetGroups,
	kDSStdRecordTypeNetworks,
	kDSStdRecordTypePasswordServer,
	kDSStdRecordTypePeople,
	kDSStdRecordTypePresetComputerLists,
	kDSStdRecordTypePresetGroups,
	kDSStdRecordTypePresetUsers,
	kDSStdRecordTypePrintService,
	kDSStdRecordTypePrintServiceUser,
	kDSStdRecordTypePrinters,
	kDSStdRecordTypeProtocols,
	kDSStdRecordTypeQTSServer,
	kDSStdRecordTypeRPC,
	kDSStdRecordTypeSMBServer,
	kDSStdRecordTypeServer,
	kDSStdRecordTypeServices,
	kDSStdRecordTypeSharePoints,
	kDSStdRecordTypeUserAliases,
	kDSStdRecordTypeUsers,
	kDSStdRecordTypeWebServer
};

// --------------------------------------------------------------------------------
//	* CConfigurePlugin ()
// --------------------------------------------------------------------------------

CConfigurePlugin::CConfigurePlugin ( FourCharCode inSig, const char *inName ) : CServerPlugin(inSig, inName)
{
	fConfigNodeName	= nil;
	fNodeCount		= 0;
	fState			= kUnknownState;

	if ( gConfigNodeRef == nil )
	{
		gConfigNodeRef = new CPlugInRef( CConfigurePlugin::ContextDeallocProc );
		if ( gConfigNodeRef == nil ) throw((sInt32)eMemoryAllocError);
	}

	if ( gConfigContinue == nil )
	{
		gConfigContinue = new CContinue( CConfigurePlugin::ContinueDeallocProc );
		if ( gConfigContinue == nil ) throw((sInt32)eMemoryAllocError);
	}

	if ( gKickConfigRequests == nil )
	{
		gKickConfigRequests = new DSEventSemaphore();
		if ( gKickConfigRequests == nil ) throw((sInt32)eMemoryAllocError);
	}
} // CConfigurePlugin


// --------------------------------------------------------------------------------
//	* ~CConfigurePlugin ()
// --------------------------------------------------------------------------------

CConfigurePlugin::~CConfigurePlugin ( void )
{
} // ~CConfigurePlugin


// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

sInt32 CConfigurePlugin::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	fPlugInSignature = inSignature;

	return( eDSNoErr );
} // Validate


// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

sInt32 CConfigurePlugin::SetPluginState ( const uInt32 inState )
{
//does nothing yet
	return( eDSNoErr );
} // SetPluginState


// --------------------------------------------------------------------------------
//	* PeriodicTask ()
// --------------------------------------------------------------------------------

sInt32 CConfigurePlugin::PeriodicTask ( void )
{
//does nothing yet
	return( eDSNoErr );
} // PeriodicTask


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 CConfigurePlugin::Initialize ( void )
{
	sInt32			siResult		= eDSNoErr;

	// maybe do some config file reading stuff
	fConfigNodeName = ::dsBuildFromPathPriv( "Configure", "/" );

	if ( fConfigNodeName != nil )
	{
		CServerPlugin::_RegisterNode( fPlugInSignature, fConfigNodeName, kConfigNodeType );
	}

	fState = 	kUnknownState;
	fState += 	kInitialized;
	fState += 	kActive;

	return( siResult );

} // Initialize


//--------------------------------------------------------------------------------------------------
//	* WakeUpRequests() (static)
//
//--------------------------------------------------------------------------------------------------

void CConfigurePlugin::WakeUpRequests ( void )
{
	gKickConfigRequests->Signal();
} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CConfigurePlugin::WaitForInit ( void )
{
	volatile	uInt32		uiAttempts	= 0;

	while ( !(fState & kInitialized) &&
			!(fState & kFailedToInit) )
	{
		// Try for 2 minutes before giving up
		if ( uiAttempts++ >= 240 )
		{
			return;
		}

		// Now wait until we are told that there is work to do or
		//	we wake up on our own and we will look for ourselves

		gKickConfigRequests->Wait( (uInt32)(.5 * kMilliSecsPerSec) );
	}
} // WaitForInit


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

sInt32 CConfigurePlugin::ProcessRequest ( void *inData )
{
	sInt32		siResult	= eDSNoErr;
	char	   *pathStr		= nil;

	try
	{
		if ( inData == nil )
		{
			throw( (sInt32)ePlugInDataError );
		}

		if (((sHeader *)inData)->fType == kOpenDirNode)
		{
			if (((sOpenDirNode *)inData)->fInDirNodeName != nil)
			{
				pathStr = ::dsGetPathFromListPriv( ((sOpenDirNode *)inData)->fInDirNodeName, "/" );
				if ( (pathStr != nil) && (strncmp(pathStr,"/Configure",10) != 0) )
				{
					throw( (sInt32)eDSOpenNodeFailed);
				}
			}
		}
		
		WaitForInit();

		if (fState == kUnknownState)
		{
			throw( (sInt32)ePlugInCallTimedOut );
		}

        if ( (fState & kFailedToInit) || !(fState & kInitialized) )
        {
            throw( (sInt32)ePlugInFailedToInitialize );
        }

        if ( (fState & kInactive) || !(fState & kActive) )
        {
            throw( (sInt32)ePlugInNotActive );
        }
        
		siResult = HandleRequest( inData );
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if (pathStr != nil)
	{
		free(pathStr);
		pathStr = nil;
	}
	return( siResult );

} // ProcessRequest


// ---------------------------------------------------------------------------
//	* HandleRequest
//
// ---------------------------------------------------------------------------

sInt32 CConfigurePlugin::HandleRequest ( void *inData )
{
	sInt32		siResult	= eDSNoErr;
	sHeader	   *pMsgHdr		= nil;

	try
	{
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

            case kDoPlugInCustomCall:
                siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
                break;
                
			case kHandleNetworkTransition:
			case kServerRunLoop:
				siResult = eDSNoErr;
				break;

			default:
				siResult = eNotHandledByThisNode;
				break;
		}

		pMsgHdr->fResult = siResult;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // HandleRequest


//------------------------------------------------------------------------------------
//	* ReleaseContinueData
//------------------------------------------------------------------------------------

sInt32 CConfigurePlugin::ReleaseContinueData ( sReleaseContinueData *inData )
{
	sInt32	siResult	= eDSNoErr;

	// RemoveItem calls our ContinueDeallocProc to clean up
	if ( gConfigContinue->RemoveItem( inData->fInContinueData ) != eDSNoErr )
	{
		siResult = eDSInvalidContext;
	}

	return( siResult );

} // ReleaseContinueData


//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

sInt32 CConfigurePlugin::OpenDirNode ( sOpenDirNode *inData )
{
	sInt32			siResult	= eDSOpenNodeFailed;
	char		       *pathStr		= nil;
	sConfigContextData	       *pContext	= nil;

	try
	{
		
		if ( inData != nil )
		{
			pathStr = ::dsGetPathFromListPriv( inData->fInDirNodeName, "/" );
			if ( pathStr != nil )
			{
				if ( ::strcmp( pathStr, "/Configure" ) == 0 )
				{
					siResult = eDSNoErr;
					pContext = MakeContextData();
					pContext->fUID = inData->fInUID;
					pContext->fEffectiveUID = inData->fInEffectiveUID;

					if (pContext == nil ) throw( (sInt32)eMemoryAllocError);

					gConfigNodeRef->AddItem( inData->fOutNodeRef, pContext );
				}

				delete( pathStr );
				pathStr = nil;
			}
		}

	}

	catch( sInt32 err )
	{
		siResult = err;
	}
	return( siResult );

} // OpenDirNode


//------------------------------------------------------------------------------------
//	* CloseDirNode
//------------------------------------------------------------------------------------

sInt32 CConfigurePlugin::CloseDirNode ( sCloseDirNode *inData )
{
	sInt32			siResult		= eDSNoErr;
	sConfigContextData   *pContext		= nil;

	try
	{
		pContext = (sConfigContextData *) gConfigNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		gConfigNodeRef->RemoveItem( inData->fInNodeRef );
		gConfigContinue->RemoveItems( inData->fInNodeRef );
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // CloseDirNode


//------------------------------------------------------------------------------------
//	* GetRecordList
//------------------------------------------------------------------------------------

sInt32 CConfigurePlugin::GetRecordList ( sGetRecordList *inData )
{
	sInt32						siResult		= eDSNoErr;
	uInt32						i				= 0;
	uInt32						uiTotal			= 0;
	uInt32						uiCount			= 0;
	char					   *pRecName		= nil;
	char					   *pRecType		= nil;
	char					   *pNIRecType		= nil;
	bool						bAttribOnly		= false;
	tDirPatternMatch			pattMatch		= eDSNoMatch1;
	CAttributeList 			   *cpRecNameList	= nil;
	CAttributeList 			   *cpRecTypeList	= nil;
	CAttributeList 			   *cpAttrTypeList 	= nil;
	sConfigContextData			   *pContext		= nil;
	sConfigContinueData		   *pContinue		= nil;
	CBuff					   *outBuff			= nil;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	const char				   *typeName		= nil;
    sInt32						siValCnt		= 0;
	uInt32						fillIndex		= 0;
	CDataBuff				   *aRecData		= nil;
	CDataBuff				   *aAttrData		= nil;
	CDataBuff				   *aTmpData		= nil;

	try
	{
		aRecData	= new CDataBuff();
		if ( aRecData == nil ) throw((sInt32)eMemoryAllocError);

		aAttrData	= new CDataBuff();
		if ( aAttrData == nil ) throw((sInt32)eMemoryAllocError);

		aTmpData	= new CDataBuff();
		if ( aTmpData == nil ) throw((sInt32)eMemoryAllocError);

		// Verify all the parameters
		if ( inData == nil ) throw( (sInt32)eMemoryError );
		if ( inData->fInDataBuff == nil ) throw( (sInt32)eDSEmptyBuffer );
		if (inData->fInDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

		if ( inData->fInRecNameList == nil ) throw( (sInt32)eDSEmptyRecordNameList );
		if ( inData->fInRecTypeList == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
		if ( inData->fInAttribTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );

		// Node context data
		pContext = (sConfigContextData *)gConfigNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );

		if ( inData->fIOContinueData == nil )
		{
			pContinue = (sConfigContinueData *)::calloc( 1, sizeof( sConfigContinueData ) );
			gConfigContinue->AddItem( pContinue, inData->fInNodeRef );

			pContinue->fRecNameIndex = 1;
			pContinue->fRecTypeIndex = 1;
			pContinue->fAttrIndex = 1;
			pContinue->fAllRecIndex = 0;
		}
		else
		{
			pContinue = (sConfigContinueData *)inData->fIOContinueData;
			if ( gConfigContinue->VerifyItem( pContinue ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
		}

		inData->fIOContinueData = nil;

		outBuff = new CBuff();
		if ( outBuff == nil ) throw( (sInt32)eMemoryError );

		siResult = outBuff->Initialize( inData->fInDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff->GetBuffStatus();
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff->SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw( siResult );

		// Get the record name list
		cpRecNameList = new CAttributeList( inData->fInRecNameList );
		if ( cpRecNameList == nil ) throw( (sInt32)eDSEmptyRecordNameList );
		if (cpRecNameList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordNameList );

		// Get the record pattern match
		pattMatch = inData->fInPatternMatch;

		// Get the record type list
		cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
		if ( cpRecTypeList == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
		if (cpRecTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordTypeList );

		// Get the attribute list
		cpAttrTypeList = new CAttributeList( inData->fInAttribTypeList );
		if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
		if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );

		// Get the attribute info only flag
		bAttribOnly = inData->fInAttribInfoOnly;

		// get these type of records
		while ( cpRecTypeList->GetAttribute( pContinue->fRecTypeIndex, &pRecType ) == eDSNoErr )
		{
			// get this record type
			if ( ::strcmp( pRecType, kDSConfigPluginsRecType ) == 0 )
			{
				// get these names
				while ( cpRecNameList->GetAttribute( pContinue->fRecNameIndex, &pRecName ) == eDSNoErr )
				{
					// Get all records of this name
					if ( ::strcmp( pRecName, kDSConfigRecordsAll ) == 0 )
					{
						//setup to work with the continue data
						i = pContinue->fAllRecIndex;

						//search over all the plugins in the table
						while ( i < CPlugInList::kMaxPlugIns )
						{

                            pPIInfo = gPlugins->GetPlugInInfo( i );

                            if ( pPIInfo != nil )
                            {
                            	aRecData->Clear();
                            	siValCnt = 0;

                                if ( (pPIInfo->fName != nil) && ( ::strcmp(pPIInfo->fName,"Configure") != 0) && ( ::strcmp(pPIInfo->fName,"Search") != 0) )
                                {
                                    // Add the record type which in this case is config node
                                    aRecData->AppendShort( ::strlen( kDSConfigPluginsRecType ) );
                                    aRecData->AppendString( kDSConfigPluginsRecType );

                                    // Add the record name which in this case is the config node name
                                    aRecData->AppendShort( ::strlen( pPIInfo->fName ) );
                                    aRecData->AppendString( pPIInfo->fName );

                                    aAttrData->Clear();

                                    //let's get the attributes in this order
                                    //plugin table index, plugin status, plugin software version, plugin config HI avail, config file

                                    siValCnt = 5;
                                    
                                    aTmpData->Clear();

                                    //append the plugin table index attr name
                                    aTmpData->AppendShort( ::strlen( kDSConfigAttrPlugInIndex ) );
                                    aTmpData->AppendString( kDSConfigAttrPlugInIndex );
                                    // Append the attribute value count
                                    aTmpData->AppendShort( 1 );
                                    // Append attribute value
									//special case as we will pass the attribute inside the length of the attribute value
									//instead and the value string here will be simply filler to ensure the buffer logic works
                                    aTmpData->AppendLong( i );
									for (fillIndex = 0; fillIndex < i; fillIndex++)
									{
										aTmpData->AppendString( "x" );
									}

									// Add the attribute length
									aAttrData->AppendLong( aTmpData->GetLength() );
									aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                                    aTmpData->Clear();

                                    //append the plugin status attr name
                                    aTmpData->AppendShort( ::strlen( kDSConfigAttrState ) );
                                    aTmpData->AppendString( kDSConfigAttrState );
                                    // Append the attribute value count
                                    aTmpData->AppendShort( 1 );
                                    // Append attribute value
                                    if (pPIInfo->fState & kActive)
                                    {
                                        aTmpData->AppendLong( ::strlen("Active" ));
                                        aTmpData->AppendString( "Active" );
                                    } else if (pPIInfo->fState & kInitialized)
                                    {
                                       aTmpData->AppendLong( ::strlen("Initted" ));
                                       aTmpData->AppendString( "Initted" );
                                   	} else if (pPIInfo->fState & kFailedToInit)
                                    {
                                       aTmpData->AppendLong( ::strlen("FailedToInit" ));
                                       aTmpData->AppendString( "FailedToInit" );
                                    } else
                                    {
                                       aTmpData->AppendLong( ::strlen("Unknown" ));
                                       aTmpData->AppendString( "Unknown" );
                                    }

									// Add the attribute length
									aAttrData->AppendLong( aTmpData->GetLength() );
									aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                                    aTmpData->Clear();

                                    //append the plugin version attr name
                                    aTmpData->AppendShort( ::strlen( kDSConfigAttrVersion ) );
                                    aTmpData->AppendString( kDSConfigAttrVersion );
                                    // Append the attribute value count
                                    aTmpData->AppendShort( 1 );
                                    // Append attribute value
                                    aTmpData->AppendLong( ::strlen( pPIInfo->fVersion ));
                                    aTmpData->AppendString( pPIInfo->fVersion );
									
									// Add the attribute length
									aAttrData->AppendLong( aTmpData->GetLength() );
									aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                                    aTmpData->Clear();

                                    //append the plugin config avail attr name
                                    aTmpData->AppendShort( ::strlen( kDSConfigAttrConfigAvail ) );
                                    aTmpData->AppendString( kDSConfigAttrConfigAvail );
                                    // Append the attribute value count
                                    aTmpData->AppendShort( 1 );
                                    // Append attribute value
                                    aTmpData->AppendLong( ::strlen( pPIInfo->fConfigAvail ));
                                    aTmpData->AppendString( pPIInfo-> fConfigAvail );
									
									// Add the attribute length
									aAttrData->AppendLong( aTmpData->GetLength() );
									aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                                    aTmpData->Clear();

                                    //append the plugin config file attr name
                                    aTmpData->AppendShort( ::strlen( kDSConfigAttrConfigFile ) );
                                    aTmpData->AppendString( kDSConfigAttrConfigFile );
                                    // Append the attribute value count
                                    aTmpData->AppendShort( 1 );
                                    // Append attribute value
                                    aTmpData->AppendLong( ::strlen( pPIInfo->fConfigFile ));
                                    aTmpData->AppendString( pPIInfo-> fConfigFile );
									
									// Add the attribute length
									aAttrData->AppendLong( aTmpData->GetLength() );
									aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                                    aTmpData->Clear();

                        			// Attribute count
                        			aRecData->AppendShort( siValCnt );
                       				aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );

									//add the record (plugin) data to the buffer
									siResult = outBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
									if ( siResult != eDSNoErr )
									{
										pContinue->fAllRecIndex = i;
										//throw( siResult );
										break;
									}
								} // if there is a name and it is not /Configure or /Search ie. this plugin itself or the search node

							} // this is a registered plugin that is not nil
							i++;
						} // while: search over all the registered plugins

						outBuff->GetDataBlockCount( &uiCount );

						if ( siResult == CBuff::kBuffFull )
						{
							uiTotal += uiCount;

							if ( uiTotal == 0 )
							{
								throw( (sInt32)eDSBufferTooSmall );
							}
							else
							{
								inData->fIOContinueData = pContinue;
								inData->fOutRecEntryCount = uiTotal;
								outBuff->SetLengthToSize();

								throw( (sInt32)eDSNoErr );
							}
						}
						else if ( siResult == eDSNoErr )
						{
							uiTotal += uiCount;
						}
						else
						{
							//error
							break;
						}

					} // single record name
					
				pContinue->fRecNameIndex++;
				
				} // loop over record names

			} // single record type
			else if ( ::strcmp( pRecType, kDSConfigRecordsType ) == 0 )
			{
				// get these names
				while ( cpRecNameList->GetAttribute( pContinue->fRecNameIndex, &pRecName ) == eDSNoErr )
				{
					// Get all records of this name
					if ( ::strcmp( pRecName, kDSConfigRecordsAll ) == 0 )
					{
						//setup to work with the continue data
						i = pContinue->fAllRecIndex;

						//search over all the record types in the table
						while ( i < kRecTypeConsts )
						{

                            typeName = sRecTypes[i];

                            if ( typeName != nil )
                            {
								aRecData->Clear();
								siValCnt = 0;

								// Add the record type which in this case is config node
								aRecData->AppendShort( ::strlen( pRecType ) );
								aRecData->AppendString( pRecType );

								// Add the record name which in this case is the config node name
								aRecData->AppendShort( ::strlen( typeName ) );
								aRecData->AppendString( typeName );

								aAttrData->Clear();

								//let's get the attributes in this order
								//plugin table index, plugin status, plugin software version, plugin config HI avail, config file

								siValCnt = 0;

								//aTmpData->Clear();

								// Attribute count
								aRecData->AppendShort( siValCnt );
								aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );

								//add the record (plugin) data to the buffer
								siResult = outBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
								if ( siResult != eDSNoErr )
								{
									pContinue->fAllRecIndex = i;
									//throw( siResult );
									break;
								}

							} // name is not nil
							i++;
						} // while: search over all the registered plugins

						outBuff->GetDataBlockCount( &uiCount );

						if ( siResult == CBuff::kBuffFull )
						{
							uiTotal += uiCount;

							if ( uiTotal == 0 )
							{
								throw( (sInt32)eDSBufferTooSmall );
							}
							else
							{
								inData->fIOContinueData = pContinue;
								inData->fOutRecEntryCount = uiTotal;
								outBuff->SetLengthToSize();

								throw( (sInt32)eDSNoErr );
							}
						}
						else if ( siResult == eDSNoErr )
						{
							uiTotal += uiCount;
						}
						else
						{
							//error
							break;
						}

					} // single record name

					pContinue->fRecNameIndex++;

				} // loop over record names

			} // single record type
			else if ( ::strcmp( pRecType, kDSConfigAttributesType ) == 0 )
			{
				// get these names
				while ( cpRecNameList->GetAttribute( pContinue->fRecNameIndex, &pRecName ) == eDSNoErr )
				{
					// Get all records of this name
					if ( ::strcmp( pRecName, kDSConfigRecordsAll ) == 0 )
					{
						//setup to work with the continue data
						i = pContinue->fAllRecIndex;

						//search over all the record types in the table
						while ( i < kAttrTypeConsts )
						{

                            typeName = sAttrTypes[i];

                            if ( typeName != nil )
                            {
								aRecData->Clear();
								siValCnt = 0;

								// Add the record type which in this case is config node
								aRecData->AppendShort( ::strlen( pRecType ) );
								aRecData->AppendString( pRecType );

								// Add the record name which in this case is the config node name
								aRecData->AppendShort( ::strlen( typeName ) );
								aRecData->AppendString( typeName );

								aAttrData->Clear();

								//let's get the attributes in this order
								//plugin table index, plugin status, plugin software version, plugin config HI avail, config file

								siValCnt = 0;

								//aTmpData->Clear();

								// Attribute count
								aRecData->AppendShort( siValCnt );
								aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );

								//add the record (plugin) data to the buffer
								siResult = outBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
								if ( siResult != eDSNoErr )
								{
									pContinue->fAllRecIndex = i;
									//throw( siResult );
									break;
								}

							} // name is not nil
							i++;
						} // while: search over all the registered plugins

						outBuff->GetDataBlockCount( &uiCount );

						if ( siResult == CBuff::kBuffFull )
						{
							uiTotal += uiCount;

							if ( uiTotal == 0 )
							{
								throw( (sInt32)eDSBufferTooSmall );
							}
							else
							{
								inData->fIOContinueData = pContinue;
								inData->fOutRecEntryCount = uiTotal;
								outBuff->SetLengthToSize();

								throw( (sInt32)eDSNoErr );
							}
						}
						else if ( siResult == eDSNoErr )
						{
							uiTotal += uiCount;
						}
						else
						{
							//error
							break;
						}

					} // single record name

					pContinue->fRecNameIndex++;

				} // loop over record names

			} // single record type
			else
			{
				siResult = eDSInvalidRecordType;
			}
			if (siResult != eDSNoErr)
			{
				break;
			}
			
			pContinue->fRecTypeIndex++;
			
		} // loop over record types

		if ( siResult == eDSNoErr )
		{
			if ( uiTotal == 0 )
			{
				//siResult = eDSRecordNotFound;
				outBuff->ClearBuff();
			}
			else
			{
				outBuff->SetLengthToSize();
			}

			inData->fOutRecEntryCount = uiTotal;
		} // if no error
	} // try block

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( (inData->fIOContinueData == nil) && (pContinue != nil) )
	{
		// we've decided not to return continue data, so we should clean up
		gConfigContinue->RemoveItem( pContinue );
		pContinue = nil;
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

	if ( aRecData != nil )
	{
		delete( aRecData );
		aRecData = nil;
	}

	if ( aAttrData != nil )
	{
		delete( aAttrData );
		aAttrData = nil;
	}

	if ( aTmpData != nil )
	{
		delete( aTmpData );
		aTmpData = nil;
	}

	return( siResult ); 

} // GetRecordList


//------------------------------------------------------------------------------------
//	* GetRecordEntry
//------------------------------------------------------------------------------------

sInt32 CConfigurePlugin::GetRecordEntry ( sGetRecordEntry *inData )
{
	sInt32					siResult		= eDSNoErr;
	uInt32					uiIndex			= 0;
	uInt32					uiCount			= 0;
	uInt32					uiOffset		= 0;
	uInt32					uberOffset		= 0;
	char 				   *pData			= nil;
	tRecordEntryPtr			pRecEntry		= nil;
	sConfigContextData		   *pContext		= nil;
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
		
		// Skip past the record length
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
		
		gConfigNodeRef->AddItem( inData->fOutAttrListRef, pContext );

		inData->fOutRecEntryPtr = pRecEntry;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetRecordEntry


//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

sInt32 CConfigurePlugin::GetAttributeEntry ( sGetAttributeEntry *inData )
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
	sConfigContextData		   *pAttrContext		= nil;
	sConfigContextData		   *pValueContext		= nil;

	try
	{
		if ( inData  == nil ) throw( (sInt32)eMemoryError );

		pAttrContext = (sConfigContextData *)gConfigNodeRef->GetItemData( inData->fInAttrListRef );
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
			::memcpy( &usValueLen, p, 2 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
			
			uiTotalValueSize += usValueLen;
		}

		uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kBuffPad;
		pAttribInfo = (tAttributeEntry *)::calloc( 1, uiAttrEntrySize );

		pAttribInfo->fAttributeValueCount				= usValueCnt;
		pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
		pAttribInfo->fAttributeValueMaxSize				= 512;				// KW this is not used anywhere
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		attrValueListRef = inData->fOutAttrValueListRef;

		pValueContext = MakeContextData();
		if ( pValueContext  == nil ) throw( (sInt32)eMemoryAllocError );

		pValueContext->offset = uiOffset;

		gConfigNodeRef->AddItem( inData->fOutAttrValueListRef, pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

sInt32 CConfigurePlugin::GetAttributeValue ( sGetAttributeValue *inData )
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
	sConfigContextData			   *pValueContext	= nil;
	uInt32						buffSize		= 0;
	uInt32						buffLen			= 0;
	uInt32						attrLen			= 0;

	try
	{
		pValueContext = (sConfigContextData *)gConfigNodeRef->GetItemData( inData->fInAttrValueListRef );
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
		pAttrValue->fAttributeValueID = 0x00;

		inData->fOutAttrValue = pAttrValue;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeValue
	

// ---------------------------------------------------------------------------
//	* MakeContextData
// ---------------------------------------------------------------------------

sConfigContextData* CConfigurePlugin::MakeContextData ( void )
{
	sConfigContextData	*pOut	= nil;

	pOut = new sConfigContextData;
	if ( pOut != nil )
	{
		::memset( pOut, 0, sizeof( sConfigContextData ) );
	}

	return( pOut );

} // MakeContextData

//------------------------------------------------------------------------------------
//      * DoPlugInCustomCall
//------------------------------------------------------------------------------------

sInt32 CConfigurePlugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32						siResult	= eDSNoErr;
	unsigned long				aRequest	= 0;
	uInt32						pluginIndex	= 0;
	CPlugInList::sTableData    *pPIInfo		= nil;
	uInt32						thePIState	= 0;
	unsigned long				bufLen		= 0;
	AuthorizationExternalForm   authExtForm;
	AuthorizationRef			authRef		= 0;
	AuthorizationItemSet	   *resultRightSet = NULL;
	sConfigContextData		   *pContext	= nil;

	try
	{
		bzero(&authExtForm,sizeof(AuthorizationExternalForm));
		pContext = (sConfigContextData *)gConfigNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );
		if ( inData == nil ) throw( (sInt32)eDSNullParameter );
		if ( inData->fInRequestData == nil ) throw( (sInt32)eDSNullDataBuff );
		if ( inData->fOutRequestResponse == nil ) throw( (sInt32)eDSNullDataBuff );
		if ( inData->fInRequestData->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );

		aRequest = inData->fInRequestCode;
		AuthorizationItem rights[] = { {"system.services.directory.configure", 0, 0, 0} };
		AuthorizationItemSet rightSet = { sizeof(rights)/ sizeof(*rights), rights };
		bufLen = inData->fInRequestData->fBufferLength;
		
		if ( aRequest == 111 )
		{
			// we need to get an authref set up in this case
			// support for Directory Setup over proxy
			uInt32 userNameLength = 0;
			char* userName = NULL;
			uInt32 passwordLength = 0;
			char* password = NULL;
			char* current = inData->fInRequestData->fBufferData;
			uInt32 offset = 0;
			if ( bufLen < 2 * sizeof( uInt32 ) + 1 ) throw( (sInt32)eDSInvalidBuffFormat );
			
			memcpy( &userNameLength, current, sizeof( uInt32 ) );
			current += sizeof( uInt32 );
			offset += sizeof( uInt32 );
			if ( bufLen - offset < userNameLength ) throw( (sInt32)eDSInvalidBuffFormat );
			
			userName = current; //don't free this
			current += userNameLength;
			offset += userNameLength;
			if ( bufLen - offset < sizeof( uInt32 ) ) throw( (sInt32)eDSInvalidBuffFormat );
			
			memcpy( &passwordLength, current, sizeof( uInt32 ) );
			current += sizeof( uInt32 );
			offset += sizeof( uInt32 );
			if ( passwordLength == 0 )
			{
				password = "";
			}
			else
			{
				if ( bufLen - offset < passwordLength ) throw( (sInt32)eDSInvalidBuffFormat );
				password = current;
			}
			
			AuthorizationItem params[] = { {"username", userNameLength, (void*)userName, 0}, {"password", passwordLength, (void*)password, 0} };
			AuthorizationEnvironment environment = { sizeof(params)/ sizeof(*params), params };
			
			siResult = AuthorizationCreate( &rightSet, &environment, kAuthorizationFlagExtendRights, &authRef);
			if (siResult != errAuthorizationSuccess)
			{
				throw( (sInt32)eDSPermissionError );
			}
			if ( inData->fOutRequestResponse->fBufferSize < sizeof( AuthorizationExternalForm ) ) throw( (sInt32)eDSInvalidBuffFormat );
			siResult = AuthorizationMakeExternalForm(authRef, (AuthorizationExternalForm*)inData->fOutRequestResponse->fBufferData);
			if (siResult != errAuthorizationSuccess)
			{
				throw( (sInt32)eDSPermissionError );
			}
			// should we free this authRef? probably not since it will be coming back to us
			inData->fOutRequestResponse->fBufferLength = sizeof( AuthorizationExternalForm );
			siResult = eDSNoErr;
			authRef = 0;
		}
		else if (aRequest == 222)
		{
			// version check, no AuthRef required
			uInt32 versLength = strlen( "1" );
			char* current = inData->fOutRequestResponse->fBufferData;
			inData->fOutRequestResponse->fBufferLength = 0;
			if ( inData->fOutRequestResponse->fBufferSize < sizeof(versLength) + versLength ) throw( (sInt32)eDSInvalidBuffFormat );
			memcpy(current, &versLength, sizeof(versLength));
			current += sizeof(versLength);
			inData->fOutRequestResponse->fBufferLength += sizeof(versLength);
			memcpy(current, "1", versLength);
			current += versLength;
			inData->fOutRequestResponse->fBufferLength += versLength;
		}
		else if (aRequest == 444 || aRequest == 445)
		{
			// read SystemConfiguration key, no authref required
			// for Remote Directory Setup
			uInt32 keyLength = bufLen;
			CFStringRef key = NULL;
			CFPropertyListRef dict = NULL;
			CFDataRef xmlData = NULL;
			char* current = inData->fInRequestData->fBufferData;

			key = CFStringCreateWithBytes(NULL, (UInt8*)current, keyLength, kCFStringEncodingUTF8,
					false);

			if (pContext->session == 0)
			{
				// first call, need to set up the SystemConfiguration session
				pContext->session = SCPreferencesCreate( NULL, CFSTR("DSConfigurePlugIn"), NULL );
			}
			
			dict = SCPreferencesPathGetValue( pContext->session, key );
			xmlData = CFPropertyListCreateXMLData( NULL, dict );
			
			if (xmlData != 0)
			{
				CFRange	aRange;
				aRange.location = 0;
				aRange.length = CFDataGetLength(xmlData);
				if (aRequest == 444)
				{
					if ( inData->fOutRequestResponse->fBufferSize < sizeof(CFIndex) ) throw( (sInt32)eDSBufferTooSmall );
					memcpy(inData->fOutRequestResponse->fBufferData,&aRange.length,sizeof(CFIndex));
				}
				else
				{
					if ( inData->fOutRequestResponse->fBufferSize < (uInt32)aRange.length ) throw( (sInt32)eDSBufferTooSmall );
					CFDataGetBytes( xmlData, aRange, 
								(UInt8*)(inData->fOutRequestResponse->fBufferData) );
					inData->fOutRequestResponse->fBufferLength = aRange.length;
				}
				CFRelease(xmlData);
				xmlData = 0;
			}
			
			if (key != NULL)
			{
				CFRelease(key);
				key = NULL;
			}
		}
		else if (aRequest == 446 || aRequest == 447)
		{
			// read SystemConfiguration key, no authref required
			// for Remote Directory Setup
			uInt32 keyLength = bufLen;
			CFStringRef key = NULL;
			CFStringRef stringValue = NULL;
			char* current = inData->fInRequestData->fBufferData;

			key = CFStringCreateWithBytes(NULL, (UInt8*)current, keyLength, kCFStringEncodingUTF8,
								 false);

			if (pContext->session == 0)
			{
				// first call, need to set up the SystemConfiguration session
				pContext->session = SCPreferencesCreate( NULL, CFSTR("DSConfigurePlugIn"), NULL );
			}

			stringValue = (CFStringRef)SCPreferencesGetValue( pContext->session, key );

			if (stringValue != 0)
			{
				CFRange	aRange;
				aRange.location = 0;
				aRange.length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(stringValue),
													  kCFStringEncodingUTF8);
				if (aRequest == 446)
				{
					if ( inData->fOutRequestResponse->fBufferSize < sizeof(CFIndex) ) throw( (sInt32)eDSBufferTooSmall );
					memcpy(inData->fOutRequestResponse->fBufferData,&aRange.length,sizeof(CFIndex));
				}
				else
				{
					if ( inData->fOutRequestResponse->fBufferSize < (uInt32)aRange.length ) throw( (sInt32)eDSBufferTooSmall );
					CFStringGetCString(stringValue, inData->fOutRequestResponse->fBufferData, inData->fOutRequestResponse->fBufferSize, kCFStringEncodingUTF8);
					inData->fOutRequestResponse->fBufferLength = aRange.length;
				}
			}

			if (key != NULL)
			{
				CFRelease(key);
				key = NULL;
			}
		}
#ifdef BUILD_IN_PERFORMANCE
		else if ( aRequest == 666 )
		{
			// this is a request to turn on performance stat gathering - auth?
			gSrvrCntl->ActivatePeformanceStatGathering();
			
		}
		else if ( aRequest == 667 )
		{
			// this is a request to turn off performance stat gathering - auth?
			gSrvrCntl->DeactivatePeformanceStatGathering();			
		}
#endif
		else
		{
			if ( bufLen < sizeof( AuthorizationExternalForm ) ) throw( (sInt32)eDSInvalidBuffFormat );
			if (!(pContext->fEffectiveUID == 0 && 
				memcmp(inData->fInRequestData->fBufferData,&authExtForm,
						sizeof(AuthorizationExternalForm)) == 0)) {
				siResult = AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)inData->fInRequestData->fBufferData,
					&authRef);
				if (siResult != errAuthorizationSuccess)
				{
					throw( (sInt32)eDSPermissionError );
				}
		
				siResult = AuthorizationCopyRights(authRef, &rightSet, NULL,
					kAuthorizationFlagExtendRights, &resultRightSet);
				if (resultRightSet != NULL)
				{
					AuthorizationFreeItemSet(resultRightSet);
					resultRightSet = NULL;
				}
				if (siResult != errAuthorizationSuccess)
				{
					throw( (sInt32)eDSPermissionError );
				}
			}
		}
        //request to toggle the active versus inactive state of a plugin comes in with the plugin table index plus 1000
        //index could be zero
        if (aRequest > 999)
        {
			//might want to pass in the plugin name within the buffer and check it instead of using an offset from the 1000 value
            pluginIndex = aRequest - 1000;
            if (pluginIndex < CPlugInList::kMaxPlugIns)
            {
                pPIInfo = gPlugins->GetPlugInInfo( pluginIndex );

                if (pPIInfo->fState & kActive)
                {
					thePIState = pPIInfo->fState;
					thePIState += (uInt32)kInactive;
					thePIState -= (uInt32)kActive;
					gPlugins->SetState( pPIInfo->fName, thePIState );

					gPluginConfig->SetPluginState( pPIInfo->fName, kInactive);
					gPluginConfig->SaveConfigData();

					SRVRLOG1( kLogApplication, "Plug-in %s state is now set inactive.", pPIInfo->fName );
                }
                else if (pPIInfo->fState & kInactive)
                {
					thePIState = pPIInfo->fState;
					thePIState -= kInactive;
					thePIState += kActive;
					gPlugins->SetState( pPIInfo->fName, thePIState );

					gPluginConfig->SetPluginState( pPIInfo->fName, kActive);
					gPluginConfig->SaveConfigData();

					SRVRLOG1( kLogApplication, "Plug-in %s state is now set active.", pPIInfo->fName );
                }
            }
        }
		else if (aRequest == 333)
		{
			// destroy the auth ref
			if (authRef != 0)
			{
				AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
				authRef = 0;
			}			
		}
		else if (aRequest == 555)
		{
			// write SystemConfiguration
			// for Remote Directory Setup
			bool			success		= false;
			sInt32			xmlDataLength	= 0;
			CFDataRef   	xmlData			= nil;
			CFPropertyListRef propList		= nil;

			//here we accept an XML blob to replace the "/Sets" configuration
			//need to make xmlData large enough to receive the data
			xmlDataLength = (sInt32) bufLen - sizeof( AuthorizationExternalForm );
			if ( xmlDataLength <= 0 ) throw( (sInt32)eDSInvalidBuffFormat );
			
			xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm )),xmlDataLength);
			if ( xmlData == nil ) throw( (sInt32)eMemoryError );
			propList = CFPropertyListCreateFromXMLData(NULL,xmlData,0,NULL);
			if ( propList == nil ) throw( (sInt32)eMemoryError );
		
			// we're going to assume this is intended to replace the Sets
			if (pContext->session == 0)
			{
				// first call, need to set up the SystemConfiguration session
				pContext->session = SCPreferencesCreate( NULL, CFSTR("DSNetInfoPlugIn"), NULL );
			}
			if (pContext->session != 0)
			{
				CFStringRef key = SCDynamicStoreKeyCreate( NULL, CFSTR("/%@"), kSCPrefSets );
				success = SCPreferencesPathSetValue(pContext->session, key, 
								(CFDictionaryRef)propList);
				if (success)
					success &= SCPreferencesCommitChanges(pContext->session);
				if (success)
					success &= SCPreferencesApplyChanges(pContext->session);
				if (!success)
					siResult = eDSOperationFailed;
				if (key != NULL)
					CFRelease(key);
			}
			CFRelease(propList);
			propList = nil;
			CFRelease(xmlData);
			xmlData = nil;
		}
		else if (aRequest == 777)
		{
			//toggle whether TCP Listener is active or not
			struct stat		statResult;

			siResult = ::stat( "/Library/Preferences/DirectoryService/.DSTCPListening", &statResult );
            if (siResult != eDSNoErr)
			{
				siResult = ::stat( "/Library/Preferences", &statResult );
				//if first sub directory does not exist
				if (siResult != eDSNoErr)
				{
					::mkdir( "/Library/Preferences", 0775 );
					::chmod( "/Library/Preferences", 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
				}
				siResult = ::stat( "/Library/Preferences/DirectoryService", &statResult );
				//if second sub directory does not exist
				if (siResult != eDSNoErr)
				{
					::mkdir( "/Library/Preferences/DirectoryService", 0775 );
					::chmod( "/Library/Preferences/DirectoryService", 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
				}
				dsTouch( "/Library/Preferences/DirectoryService/.DSTCPListening" );
				gSrvrCntl->StartTCPListener(kDSDefaultListenPort);
			}
			else
			{
				dsRemove( "/Library/Preferences/DirectoryService/.DSTCPListening" );
				gSrvrCntl->StopTCPListener();
			}
		}
		
    } // try

    catch( sInt32 err )
    {
        siResult = err;
    }
	
	if (authRef != 0)
	{
		AuthorizationFree(authRef, 0);
		authRef = 0;
	}

	return( siResult );

} // DoPlugInCustomCall

//------------------------------------------------------------------------------------
//	* CloseAttributeList
//------------------------------------------------------------------------------------

sInt32 CConfigurePlugin::CloseAttributeList ( sCloseAttributeList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sConfigContextData	   *pContext		= nil;

	pContext = (sConfigContextData *) gConfigNodeRef->GetItemData( inData->fInAttributeListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gConfigNodeRef->RemoveItem( inData->fInAttributeListRef );
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

sInt32 CConfigurePlugin::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sConfigContextData	   *pContext		= nil;

	pContext = (sConfigContextData *) gConfigNodeRef->GetItemData( inData->fInAttributeValueListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gConfigNodeRef->RemoveItem( inData->fInAttributeValueListRef );
	}
	else
	{
		siResult = eDSInvalidAttrValueRef;
	}

	return( siResult );

} // CloseAttributeValueList


// ---------------------------------------------------------------------------
//	* ContinueDeallocProc
// ---------------------------------------------------------------------------

void CConfigurePlugin::ContinueDeallocProc ( void* inContinueData )
{
	sConfigContinueData* pContinue = (sConfigContinueData *)inContinueData;

	if ( pContinue != nil )
	{
		free( pContinue );
		pContinue = nil;
	}
} // ContinueDeallocProc


// ---------------------------------------------------------------------------
//	* ContextDeallocProc
// ---------------------------------------------------------------------------

void CConfigurePlugin::ContextDeallocProc ( void* inContextData )
{
	sConfigContextData *pContext = (sConfigContextData *) inContextData;

	if ( pContext != nil )
	{
		if ( pContext->session != NULL )
		{
			CFRelease( pContext->session );
			pContext->session = NULL;
		}
		free( pContext );
		pContext = nil;
	}
} // ContextDeallocProc
