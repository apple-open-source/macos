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
 * @header CConfigurePlugin
 */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>			//used for mkdir and stat
#include <syslog.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <Security/Authorization.h>
#include <Kerberos/Kerberos.h>

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"

#include "SharedConsts.h"
#include "CSharedData.h"
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
#include "CRefTable.h"
#include "CContinue.h"
#include "CPlugInList.h"
#include "ServerControl.h"
#include "CPluginConfig.h"
#include "DSNetworkUtilities.h"
#include "GetMACAddress.h"
#include "CDSPluginUtils.h"

// Globals ---------------------------------------------------------------------------

static	CPlugInRef	 	*gConfigNodeRef			= nil;
static	CContinue	 	*gConfigContinue		= nil;
DSEventSemaphore gKickConfigRequests;

extern	CPlugInList		*gPlugins;
extern  CPluginConfig   *gPluginConfig;
extern  const char		*gStrDaemonBuildVersion;
extern  DSMutexSemaphore    *gKerberosMutex;
extern	CRefTable			*gRefTable;

struct sConfigContinueData
{
	UInt32				fRecNameIndex;
	UInt32				fRecTypeIndex;
	UInt32				fAllRecIndex;
	UInt32				fAttrIndex;
    gss_ctx_id_t		gssContext;
    gss_cred_id_t		gssCredentials;
    gss_name_t			gssServicePrincipal; // e.g., "ldap/host@REALM"
    gss_name_t			gssClientPrincipal; // e.g., "user@REALM"
    bool				gssFinished;
    u_int32_t			exportContext;
	
    sConfigContinueData( void )
    {
		fRecNameIndex = 0;
		fRecTypeIndex = 0;
		fAllRecIndex = 0;
		fAttrIndex = 0;
        gssCredentials = GSS_C_NO_CREDENTIAL;
        gssContext = GSS_C_NO_CONTEXT;
        gssServicePrincipal = NULL;
        gssClientPrincipal = NULL;
        gssFinished = false;
        exportContext = 0;
    }
    
    ~sConfigContinueData( void )
    {
        OM_uint32           minorStatus     = 0;
		
        gKerberosMutex->WaitLock();
        
        if( gssServicePrincipal != NULL )
            gss_release_name( &minorStatus, &gssServicePrincipal );
        
        if( gssClientPrincipal != NULL )
            gss_release_name( &minorStatus, &gssClientPrincipal );
        
        if( gssCredentials != GSS_C_NO_CREDENTIAL )
            gss_release_cred( &minorStatus, &gssCredentials );
        
        if( gssContext != GSS_C_NO_CONTEXT )
            gss_delete_sec_context( &minorStatus, &gssContext, GSS_C_NO_BUFFER );
        
        gKerberosMutex->SignalLock();
    }
};

//deprecate these constants and use actual DS standard types
#define	kDSConfigPluginsRecType		"dsConfigType::Plugins"				//kDSStdRecordTypePlugins
#define	kDSConfigRecordsType		"dsConfigType::RecordTypes"			//kDSStdRecordTypeRecordTypes
#define	kDSConfigAttributesType		"dsConfigType::AttributeTypes"		//kDSStdRecordTypeAttributeTypes
#define	kDSConfigRecordsAll			"dsConfigType::GetAllRecords"		//kDSRecordsAll

#define	kDSConfigAttrVersion		"dsConfigAttrType::Version"			//kDS1AttrVersion
#define	kDSConfigAttrState			"dsConfigAttrType::State"			//kDS1AttrFunctionalState
#define	kDSConfigAttrConfigAvail	"dsConfigAttrType::ConfigAvailable"	//kDS1AttrConfigAvail
#define	kDSConfigAttrConfigFile		"dsConfigAttrType::ConfigFile"		//kDS1AttrConfigFile
#define	kDSConfigAttrPlugInIndex 	"dsConfigAttrType::PlugInIndex"		//kDS1AttrPluginIndex

//sorted alphabetically for ease of source updates.
//ie. used mainly for reference purposes and display by
//Directory Access app but note this does not mean that
//this list is ready for display since the prefix of
// "dsAttrTypeStandard:" will usually be stripped for display
//and some of the constants do not follow naming conventions.
//Also note that kDS1 and kDSN are grouped separately
static const char *sAttrTypes[  ] =
{
	kDS1AttrAdminLimits,
	kDS1AttrAliasData,
	kDS1AttrAlternateDatastoreLocation,
	kDS1AttrAuthenticationHint,
	kDS1AttrAuthorityRevocationList,
	kDS1AttrBirthday,
	kDS1AttrBootFile,
	kDS1AttrCACertificate,
	kDS1AttrCapabilities,
	kDS1AttrCapacity,
	kDS1AttrCategory,
	kDS1AttrCertificateRevocationList,
	kDS1AttrChange,
	kDS1AttrComment,
	kDS1AttrContactGUID,
	kDS1AttrContactPerson,
	kDS1AttrCreationTimestamp,
	kDS1AttrCrossCertificatePair,
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
	kDS1AttrMapGUID,
	kDS1AttrMCXFlags,
	kDS1AttrMailAttribute,
	kDS1AttrMetaAutomountMap,
	kDS1AttrMiddleName,
	kDS1AttrModificationTimestamp,
	kDSNAttrNeighborhoodAlias,
	kDS1AttrNeighborhoodType,
	kDS1AttrNetworkView,
	kDS1AttrNFSHomeDirectory,
	kDS1AttrNote,
	kDS1AttrOwner,
	kDS1AttrOwnerGUID,
	kDS1AttrPassword,
	kDS1AttrPasswordPolicyOptions,
	kDS1AttrPasswordServerList,
	kDS1AttrPasswordServerLocation,
	kDS1AttrPicture,
	kDS1AttrPort,
	kDS1AttrPresetUserIsAdmin,
	kDS1AttrPrimaryComputerGUID,
	kDS1AttrPrimaryComputerList,
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
	kDS1AttrSMBPrimaryGroupSID,
	kDS1AttrSMBPWDLastSet,
	kDS1AttrSMBProfilePath,
	kDS1AttrSMBRID,
	kDS1AttrSMBScriptPath,
	kDS1AttrSMBSID,
	kDS1AttrSMBUserWorkstations,
	kDS1AttrServiceType,
	kDS1AttrSetupAdvertising,
	kDS1AttrSetupAutoRegister,
	kDS1AttrSetupLocation,
	kDS1AttrSetupOccupation,
	kDS1AttrTimeToLive,
	kDS1AttrUniqueID,
	kDS1AttrUserCertificate,
	kDS1AttrUserPKCS12Data,
	kDS1AttrUserShell,
	kDS1AttrUserSMIMECertificate,
	kDS1AttrVersion,
	kDS1AttrVFSDumpFreq,
	kDS1AttrVFSLinkDir,
	kDS1AttrVFSPassNo,
	kDS1AttrVFSType,
	kDS1AttrWeblogURI,
	kDS1AttrXMLPlist,
	kDSNAttrAccessControlEntry,
	kDSNAttrAddressLine1,
	kDSNAttrAddressLine2,
	kDSNAttrAddressLine3,
	kDSNAttrAreaCode,
	kDSNAttrAuthenticationAuthority,
	kDSNAttrAutomountInformation,
	kDSNAttrBootParams,
	kDSNAttrBuilding,
	kDSNAttrServicesLocator,
	kDSNAttrCity,
	kDSNAttrCompany,
	kDSNAttrComputerAlias,
	kDSNAttrComputers,
	kDSNAttrCountry,
	kDSNAttrDepartment,
	kDSNAttrDNSName,
	kDSNAttrEMailAddress,
	kDSNAttrEMailContacts,
	kDSNAttrFaxNumber,
	kDSNAttrGroup,
	kDSNAttrGroupMembers,
	kDSNAttrGroupMembership,
	kDSNAttrGroupServices,
	kDSNAttrHomePhoneNumber,
	kDSNAttrHTML,
	kDSNAttrHomeDirectory,
	kDSNAttrIMHandle,
	kDSNAttrIPAddress,
	kDSNAttrIPAddressAndENetAddress,
	kDSNAttrIPv6Address,
	kDSNAttrJPEGPhoto,
	kDSNAttrJobTitle,
	kDSNAttrKDCAuthKey,
	kDSNAttrKeywords,
	kDSNAttrLDAPReadReplicas,
	kDSNAttrLDAPWriteReplicas,
	kDSNAttrMapCoordinates,
	kDSNAttrMapURI,
	kDSNAttrMachineServes,
	kDSNAttrMCXSettings,
	kDSNAttrMember,
	kDSNAttrMIME,
	kDSNAttrMobileNumber,
	kDSNAttrNBPEntry,
	kDSNAttrNestedGroups,
	kDSNAttrNetGroups,
	kDSNAttrNickName,
	kDSNAttrNodePathXMLPlist,
	// The following will not published in DirServicesConst.h, but are here so that nodes know they exist
	"dsAttrTypeStandard:OLCDatabaseIndex",
	"dsAttrTypeStandard:OLCDatabase",
	// end special types
	kDSNAttrOrganizationInfo,
	kDSNAttrOrganizationName,
	kDSNAttrPagerNumber,
	kDSNAttrPhoneContacts,
	kDSNAttrPhoneNumber,
	kDSNAttrPGPPublicKey,
	kDSNAttrPostalAddress,
	kDSNAttrPostalAddressContacts,
	kDSNAttrPostalCode,
	kDSNAttrNamePrefix,
	kDSNAttrProtocols,
	kDSNAttrRecordName,
	kDSNAttrRelationships,
	kDSNAttrResourceInfo,
	kDSNAttrResourceType,
	kDSNAttrState,
	kDSNAttrStreet,
	kDSNAttrNameSuffix,
	kDSNAttrURL,
	kDSNAttrURLForNSL,
	kDSNAttrVFSOpts,
	NULL
};


//sorted alphabetically for ease of source updates.
//ie. used mainly for reference purposes and display by
//Directory Access app but note this does not mean that
//this list is ready for display since the prefix of
// "dsRecTypeStandard:" will usually be stripped for display
//and some of the constants do not follow naming conventions.
static const char* sRecTypes[] =
{
	kDSStdRecordTypeAccessControls,
	kDSStdRecordTypeAFPServer,
	kDSStdRecordTypeAFPUserAliases,
	kDSStdRecordTypeAliases,
	kDSStdRecordTypeAugments,
	kDSStdRecordTypeAutomount,
	kDSStdRecordTypeAutomountMap,
	kDSStdRecordTypeAutoServerSetup,
	kDSStdRecordTypeBootp,
	kDSStdRecordTypeCertificateAuthorities,
	kDSStdRecordTypeComputerLists,
	kDSStdRecordTypeComputerGroups,
	kDSStdRecordTypeComputers,
	kDSStdRecordTypeConfig,
	kDSStdRecordTypeEthernets,
	kDSStdRecordTypeFileMakerServers,
	kDSStdRecordTypeFTPServer,
	kDSStdRecordTypeGroups,
	kDSStdRecordTypeHostServices,
	kDSStdRecordTypeHosts,
	kDSStdRecordTypeLDAPServer,
	kDSStdRecordTypeLocations,
	kDSStdRecordTypeMachines,
	"dsRecTypeStandard:Maps",
	kDSStdRecordTypeMeta,
	kDSStdRecordTypeMounts,
	kDSStdRecordTypeNeighborhoods,
	kDSStdRecordTypeNFS,
	kDSStdRecordTypeNetDomains,
	kDSStdRecordTypeNetGroups,
	kDSStdRecordTypeNetworks,
	// The following will not published in DirServicesConst.h, but are here so that nodes know they exist
	"dsRecTypeStandard:OLCBDBConfig",
	"dsRecTypeStandard:OLCFrontEndConfig",
	"dsRecTypeStandard:OLCGlobalConfig",
	"dsRecTypeStandard:OLCOverlayDynamicID",	
	"dsRecTypeStandard:OLCSchemaConfig",
	// end special types
	kDSStdRecordTypePasswordServer,
	kDSStdRecordTypePeople,
	"dsRecTypeStandard:Places",
	kDSStdRecordTypePresetComputers,
	kDSStdRecordTypePresetComputerGroups,
	kDSStdRecordTypePresetComputerLists,
	kDSStdRecordTypePresetGroups,
	kDSStdRecordTypePresetUsers,
	kDSStdRecordTypePrintService,
	kDSStdRecordTypePrintServiceUser,
	kDSStdRecordTypePrinters,
	kDSStdRecordTypeProtocols,
	kDSStdRecordTypeQTSServer,
	kDSStdRecordTypeResources,
	kDSStdRecordTypeRPC,
	kDSStdRecordTypeSMBServer,
	kDSStdRecordTypeServer,
	kDSStdRecordTypeServices,
	kDSStdRecordTypeSharePoints,
	kDSStdRecordTypeUsers,
	kDSStdRecordTypeWebServer,
	NULL
};

// Note that the ordering of the array below must match the sRecTypes array.
// In other words, the associated attribute types must be in the same index 
// as the appropriate record type above.
static const char* sRecTypeAttributes[][82] =
{
	// kDSStdRecordTypeAccessControls
	{ kDSNAttrRecordName, kDSNAttrAccessControlEntry, NULL },
	// kDSStdRecordTypeAFPServer
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeAFPUserAliases
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeAliases
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeAugments
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeAutomount
	{ kDSNAttrRecordName, kDSNAttrAutomountInformation, kDS1AttrComment, NULL },
	// kDSStdRecordTypeAutomountMap
	{ kDSNAttrRecordName, kDS1AttrComment, NULL },
	// kDSStdRecordTypeAutoServerSetup
	{ kDSNAttrRecordName, kDS1AttrXMLPlist, NULL },
	// kDSStdRecordTypeBootp
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeCertificateAuthorities
	{ kDSNAttrRecordName, kDS1AttrAuthorityRevocationList, kDS1AttrCertificateRevocationList,
	  kDS1AttrCACertificate, kDS1AttrCrossCertificatePair, NULL },
	// kDSStdRecordTypeComputerLists
	{ kDSNAttrRecordName, kDS1AttrMCXFlags, kDSNAttrMCXSettings, kDSNAttrComputers,
	  kDSNAttrGroup, kDS1AttrGeneratedUID, kDSNAttrKeywords, NULL },
	// kDSStdRecordTypeComputerGroups
	{ kDSNAttrRecordName, kDSNAttrGroupMembers, kDSNAttrGroupMembership,
	  kDS1AttrPrimaryGroupID, kDS1AttrPrimaryComputerGUID, kDSNAttrHomeDirectory, 
	  kDS1AttrHomeLocOwner, kDS1AttrMCXFlags, kDSNAttrMCXSettings, 
	  kDSNAttrNestedGroups, kDS1AttrDistinguishedName, kDS1AttrComment, 
	  kDSNAttrEMailAddress, kDS1AttrPicture, kDSNAttrKeywords, kDS1AttrGeneratedUID, 
	  kDS1AttrSMBRID, kDS1AttrSMBGroupRID, kDS1AttrSMBSID, kDS1AttrTimeToLive,
	  kDSNAttrJPEGPhoto, kDSNAttrGroupServices, kDS1AttrXMLPlist,
	  kDS1AttrContactGUID, kDS1AttrOwnerGUID,
	  kDSNAttrURL, kDSNAttrServicesLocator, NULL },
	// kDSStdRecordTypeComputers
	{ kDSNAttrRecordName, kDS1AttrDistinguishedName, kDS1AttrCategory, kDS1AttrComment,
	  kDS1AttrENetAddress, kDSNAttrKeywords, kDS1AttrMCXFlags, kDSNAttrMCXSettings,
	  kDS1AttrNetworkView, kDSNAttrGroup, kDS1AttrUniqueID, kDS1AttrPrimaryGroupID, kDS1AttrPrimaryComputerList,
	  kDSNAttrAuthenticationAuthority, kDS1AttrGeneratedUID, kDS1AttrSMBAcctFlags,
	  kDS1AttrSMBPWDLastSet, kDS1AttrSMBLogonTime, kDS1AttrSMBLogoffTime, 
	  kDS1AttrSMBKickoffTime, kDS1AttrSMBRID, kDS1AttrSMBGroupRID, kDS1AttrSMBSID,
	  kDS1AttrSMBPrimaryGroupSID, kDS1AttrTimeToLive, kDSNAttrURL, kDS1AttrXMLPlist, 
	  kDSNAttrIPAddress, kDSNAttrIPv6Address, kDSNAttrIPAddressAndENetAddress, NULL },
	// kDSStdRecordTypeConfig
	{ kDSNAttrRecordName, kDS1AttrDistinguishedName, kDS1AttrComment,
	  kDS1AttrDataStamp, kDSNAttrKDCAuthKey, kDS1AttrKDCConfigData,
	  kDSNAttrKeywords, kDSNAttrLDAPReadReplicas, kDSNAttrLDAPWriteReplicas,
	  kDS1AttrPasswordServerList, kDS1AttrPasswordServerLocation, kDS1AttrTimeToLive,
	  kDS1AttrXMLPlist, NULL },
	// kDSStdRecordTypeEthernets
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeFileMakerServers
	{ kDSNAttrRecordName, kDSNAttrCity, kDS1AttrComment, kDSNAttrDNSName,
	  kDSNAttrEMailAddress, kDSNAttrKeywords, kDSNAttrPhoneNumber, kDS1AttrOwner,
	  NULL },
	// kDSStdRecordTypeFTPServer
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeGroups
	{ kDSNAttrRecordName, kDSNAttrGroupMembers, kDSNAttrGroupMembership,
	  kDS1AttrPrimaryGroupID, kDSNAttrHomeDirectory, 
	  kDS1AttrHomeLocOwner, kDS1AttrMCXFlags, kDSNAttrMCXSettings, 
	  kDSNAttrNestedGroups, kDS1AttrDistinguishedName, kDS1AttrComment, 
	  kDSNAttrEMailAddress, kDS1AttrPicture, kDSNAttrKeywords, kDS1AttrGeneratedUID, 
	  kDS1AttrSMBRID, kDS1AttrSMBGroupRID, kDS1AttrSMBSID, kDS1AttrTimeToLive,
	  kDSNAttrJPEGPhoto, kDSNAttrGroupServices, kDS1AttrContactGUID, kDS1AttrOwnerGUID,
	  kDSNAttrURL, kDSNAttrServicesLocator, kDS1AttrXMLPlist, NULL },
	// kDSStdRecordTypeHostServices
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeHosts
	{ kDSNAttrRecordName, kDSNAttrIPAddress, kDSNAttrIPv6Address, NULL },
	// kDSStdRecordTypeLDAPServer
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeLocations
	{ kDSNAttrRecordName, kDS1AttrDNSDomain, kDS1AttrDNSNameServer, NULL },
	// kDSStdRecordTypeMachines
	{ kDSNAttrRecordName, kDS1AttrComment, kDSNAttrIPAddress, kDS1AttrENetAddress,
	  kDS1AttrBootFile, kDSNAttrBootParams, kDS1AttrContactPerson, 
	  kDSNAttrMachineServes, kDSNAttrIPv6Address, NULL },
	// dsRecTypeStandard:Maps
	{ kDSNAttrRecordName, kDS1AttrGeneratedUID, kDS1AttrContactGUID,  kDS1AttrOwnerGUID, kDSNAttrCountry, 
	  kDSNAttrResourceInfo, kDSNAttrResourceType, kDS1AttrCapacity, kDSNAttrURL, kDSNAttrKeywords,
	  kDS1AttrComment,  kDSNAttrJPEGPhoto, kDSNAttrServicesLocator,kDSNAttrPhoneContacts,
	  kDS1AttrMapGUID, kDSNAttrMapCoordinates, kDS1AttrXMLPlist, NULL },
	// kDSStdRecordTypeMeta
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeMounts
	{ kDSNAttrRecordName, kDS1AttrVFSLinkDir, kDSNAttrVFSOpts, kDS1AttrVFSType,
	  kDS1AttrVFSDumpFreq, kDS1AttrVFSPassNo, NULL },
	// kDSStdRecordTypeNeighborhoods
	{ kDSNAttrRecordName, kDS1AttrDistinguishedName, kDS1AttrGeneratedUID, 
	  kDS1AttrCategory, kDS1AttrComment, kDSNAttrKeywords, kDSNAttrNodePathXMLPlist,
	  kDSNAttrNeighborhoodAlias, kDSNAttrComputerAlias, kDS1AttrTimeToLive,
	  kDS1AttrXMLPlist, NULL },
	// kDSStdRecordTypeNFS
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeNetDomains
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeNetGroups
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeNetworks
	{ kDSNAttrRecordName, NULL },
	// dsRecTypeStandard:OLCBDBConfig
	{ kDSNAttrRecordName, "dsAttrTypeStandard:OLCDatabase", "dsAttrTypeStandard:OLCDatabaseIndex", NULL },
	// dsRecTypeStandard:OLCFrontEndConfig
	{ kDSNAttrRecordName, "dsAttrTypeStandard:OLCDatabase", kDSNAttrAccessControlEntry, NULL },
	// dsRecTypeStandard:OLCGlobalConfig
	{ kDSNAttrRecordName, "dsAttrTypeStandard:OLCDatabase", kDSNAttrAccessControlEntry, NULL },
	// dsRecTypeStandard:OLCOverlayDynamicID
	{ kDSNAttrRecordName, NULL },
	// dsRecTypeStandard:OLCSchemaConfig
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypePasswordServer
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypePeople
	{ kDSNAttrRecordName, kDS1AttrDistinguishedName, kDS1AttrMailAttribute, 
	  kDS1AttrPicture, kDSNAttrJPEGPhoto, kDS1AttrComment,
	  kDSNAttrKeywords, kDS1AttrLastName, kDS1AttrFirstName,
	  kDSNAttrNamePrefix, kDSNAttrNameSuffix, kDSNAttrNickName,
	  kDSNAttrEMailAddress, kDSNAttrHomePhoneNumber, kDSNAttrIMHandle, kDSNAttrURL, kDS1AttrWeblogURI,
	  kDSNAttrPhoneNumber, kDSNAttrFaxNumber, kDSNAttrMobileNumber, kDSNAttrPagerNumber,
	  kDSNAttrAddressLine1, kDSNAttrPostalAddress, kDSNAttrStreet, kDSNAttrCity,
	  kDSNAttrState, kDSNAttrPostalCode, kDSNAttrCountry, kDSNAttrOrganizationName, kDSNAttrOrganizationInfo,
	  kDSNAttrDepartment, kDSNAttrJobTitle, kDSNAttrBuilding, kDSNAttrCompany,
	  kDS1AttrBirthday,  kDSNAttrRelationships, kDSNAttrPhoneContacts, kDSNAttrEMailContacts, 
	  kDSNAttrPostalAddressContacts, kDSNAttrMapCoordinates, kDS1AttrMapGUID, kDSNAttrMapURI,
	  kDSNAttrOrganizationInfo, kDSNAttrServicesLocator, kDS1AttrXMLPlist, NULL },
	// dsRecTypeStandard:Places
	{ kDSNAttrRecordName, kDS1AttrGeneratedUID, kDS1AttrContactGUID,  kDS1AttrOwnerGUID, kDSNAttrCountry, 
	  kDSNAttrResourceInfo, kDSNAttrResourceType, kDS1AttrCapacity, kDSNAttrURL, kDSNAttrKeywords,
	  kDS1AttrComment,  kDSNAttrJPEGPhoto, kDSNAttrServicesLocator,kDSNAttrPhoneContacts,
	  kDS1AttrMapGUID, kDSNAttrMapCoordinates, kDS1AttrXMLPlist, NULL },
	// kDSStdRecordTypePresetComputers
	{ kDSNAttrRecordName, kDS1AttrMCXFlags, kDSNAttrMCXSettings, kDSNAttrGroup,
	  kDS1AttrComment, kDS1AttrPrimaryComputerList, kDS1AttrNetworkView,
	  kDSNAttrKeywords, NULL },
	// kDSStdRecordTypePresetComputerGroups
	{ kDSNAttrRecordName, kDS1AttrMCXFlags, kDSNAttrMCXSettings, kDS1AttrPrimaryGroupID,
	  kDS1AttrComment, kDSNAttrGroupMembership, kDSNAttrNestedGroups, kDSNAttrJPEGPhoto,
	  kDSNAttrKeywords, NULL },
	// kDSStdRecordTypePresetComputerLists
	{ kDSNAttrRecordName, kDS1AttrMCXFlags, kDSNAttrMCXSettings, kDSNAttrGroup,
	  kDSNAttrKeywords, NULL },
	// kDSStdRecordTypePresetGroups
	{ kDSNAttrRecordName, kDSNAttrGroupMembership, kDS1AttrPrimaryGroupID, 
	  kDSNAttrHomeDirectory, kDS1AttrHomeLocOwner, kDS1AttrMCXFlags, 
	  kDSNAttrMCXSettings, kDSNAttrNestedGroups, kDS1AttrDistinguishedName, 
	  kDS1AttrComment, kDSNAttrKeywords, kDSNAttrJPEGPhoto, kDSNAttrGroupServices, kDSNAttrURL, kDSNAttrServicesLocator, NULL },
	// kDSStdRecordTypePresetUsers
	{ kDSNAttrRecordName, kDS1AttrDistinguishedName, kDSNAttrGroupMembership,
	  kDS1AttrPrimaryGroupID, kDS1AttrNFSHomeDirectory, kDSNAttrHomeDirectory,
	  kDS1AttrHomeDirectoryQuota, kDS1AttrHomeDirectorySoftQuota, kDS1AttrMailAttribute, 
	  kDS1AttrPrintServiceUserData, kDS1AttrMCXFlags, kDSNAttrMCXSettings, 
	  kDS1AttrAdminLimits, kDS1AttrPassword, kDS1AttrPicture, kDS1AttrUserShell,
	  kDS1AttrComment, kDS1AttrChange, kDS1AttrExpire, kDSNAttrAuthenticationAuthority,
	  kDS1AttrPasswordPolicyOptions, kDSNAttrJPEGPhoto, kDSNAttrServicesLocator, NULL },
	// kDSStdRecordTypePrintService
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypePrintServiceUser
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypePrinters
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeProtocols
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeQTSServer
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeResources
	{ kDSNAttrRecordName, kDS1AttrGeneratedUID, kDS1AttrContactGUID,  kDS1AttrOwnerGUID, kDSNAttrCountry, 
	  kDSNAttrResourceInfo, kDSNAttrResourceType, kDS1AttrCapacity, kDSNAttrURL, kDSNAttrKeywords,
	  kDS1AttrComment,  kDSNAttrJPEGPhoto, kDSNAttrServicesLocator,kDSNAttrPhoneContacts,
	  kDS1AttrMapGUID, kDSNAttrMapCoordinates, kDS1AttrXMLPlist, NULL },
	// kDSStdRecordTypeRPC
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeSMBServer
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeServer
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeServices
	{ kDSNAttrRecordName, kDS1AttrComment, kDS1AttrPort, kDSNAttrProtocols, NULL },
	// kDSStdRecordTypeSharePoints
	{ kDSNAttrRecordName, NULL },
	// kDSStdRecordTypeUsers
	{ kDSNAttrRecordName, kDS1AttrDistinguishedName, kDS1AttrUniqueID, 
	  kDS1AttrPrimaryGroupID, kDS1AttrNFSHomeDirectory, kDSNAttrHomeDirectory,
	  kDS1AttrHomeDirectoryQuota, kDS1AttrHomeDirectorySoftQuota, kDS1AttrMailAttribute, 
	  kDS1AttrPrintServiceUserData, kDS1AttrMCXFlags, kDSNAttrMCXSettings, 
	  kDS1AttrAdminLimits, kDS1AttrPassword, kDS1AttrPicture, kDS1AttrUserShell,
	  kDS1AttrComment, kDS1AttrChange, kDS1AttrExpire, kDSNAttrAuthenticationAuthority,
	  kDS1AttrAuthenticationHint, kDS1AttrPasswordPolicyOptions, kDS1AttrSMBAcctFlags,
	  kDS1AttrSMBPWDLastSet, kDS1AttrSMBLogonTime, kDS1AttrSMBLogoffTime, 
	  kDS1AttrSMBKickoffTime, kDS1AttrSMBHomeDrive, kDS1AttrSMBScriptPath, 
	  kDS1AttrSMBProfilePath, kDS1AttrSMBUserWorkstations, kDS1AttrSMBHome, 
	  kDS1AttrSMBRID, kDS1AttrSMBGroupRID, kDS1AttrSMBSID, kDS1AttrSMBPrimaryGroupSID,
	  kDSNAttrKeywords, kDS1AttrGeneratedUID, kDS1AttrLastName, kDS1AttrFirstName,
	  kDSNAttrNamePrefix, kDSNAttrNameSuffix, kDSNAttrNickName,
	  kDSNAttrEMailAddress, kDSNAttrHomePhoneNumber, kDSNAttrIMHandle, kDSNAttrURL, kDS1AttrWeblogURI,
	  kDSNAttrPhoneNumber, kDSNAttrFaxNumber, kDSNAttrMobileNumber, kDSNAttrPagerNumber,
	  kDSNAttrAddressLine1, kDSNAttrPostalAddress, kDSNAttrStreet, kDSNAttrCity,
	  kDSNAttrState, kDSNAttrPostalCode, kDSNAttrCountry, kDSNAttrOrganizationName, kDSNAttrOrganizationInfo,
	  kDSNAttrDepartment, kDSNAttrJobTitle, kDSNAttrCompany, kDSNAttrBuilding, kDS1AttrUserCertificate,
	  kDS1AttrUserPKCS12Data, kDS1AttrUserSMIMECertificate, kDSNAttrJPEGPhoto,
	  kDS1AttrBirthday,  kDSNAttrRelationships, kDSNAttrPhoneContacts, kDSNAttrEMailContacts, 
	  kDSNAttrPostalAddressContacts, kDSNAttrMapCoordinates, kDS1AttrMapGUID, kDSNAttrMapURI,
	  kDSNAttrOrganizationInfo, kDSNAttrServicesLocator, kDS1AttrXMLPlist, NULL },
	// kDSStdRecordTypeWebServer
	{ kDSNAttrRecordName, NULL },
	NULL
};


char *CopyComponentBuildVersion( const char *inVersionPlistFilePath, const char *inDictionaryKey, UInt32 inMaxStringSize);
char *CopyComponentBuildVersion( const char *inVersionPlistFilePath, const char *inDictionaryKey, UInt32 inMaxStringSize)
{
	SInt32				versResult			= eDSNoErr;
	struct stat			statResult;
	CFStringRef			sPath				= NULL;
	CFURLRef			versionFileURL		= NULL;
	CFDataRef			xmlData				= NULL;
    CFStringRef			errorString			= NULL;
	CFPropertyListRef   configPropertyList	= NULL;
	CFDictionaryRef		versionDict			= NULL;
	char			   *outVersion			= nil;
	
	versResult = stat( inVersionPlistFilePath, &statResult );
	if (versResult == eDSNoErr)
	{
		sPath = CFStringCreateWithCString( kCFAllocatorDefault, inVersionPlistFilePath, kCFStringEncodingUTF8 );
		if (sPath != NULL)
		{
			versionFileURL = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );
			CFRelease( sPath );
			if (versionFileURL != NULL)
			{
				if (CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, versionFileURL, &xmlData, NULL, NULL, &versResult) )
				{
					if (versResult == eDSNoErr)
					{
						if (xmlData != nil)
						{
							// extract the dictionary from the XML data.
							configPropertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData, kCFPropertyListImmutable, &errorString);
							if (configPropertyList != nil )
							{
								//make the propertylist a dict
								if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
								{
									versionDict = (CFDictionaryRef) configPropertyList;
									CFStringRef key = NULL;
									key = CFStringCreateWithCString(kCFAllocatorDefault, inDictionaryKey, kCFStringEncodingUTF8);
									if ( (versionDict != nil) && (key != NULL) )
									{
										if ( CFDictionaryContainsKey( versionDict, key ) )
										{
											CFStringRef cfStringRef = NULL;
											cfStringRef = (CFStringRef)CFDictionaryGetValue( versionDict, key );
											if ( cfStringRef != nil )
											{
												if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
												{
													char *tmpBuff = (char *)calloc(1, inMaxStringSize);
													if (CFStringGetCString(cfStringRef, tmpBuff, inMaxStringSize, kCFStringEncodingUTF8))
													{
														outVersion = strdup(tmpBuff);
													}
													free( tmpBuff );
												}
											}
										}
										CFRelease(key);
									}//if (versionDict != nil)
								}
								CFRelease(configPropertyList);
							}//if (configPropertyList != nil )
							if (errorString != NULL) CFRelease(errorString);
							CFRelease(xmlData);
						}//if (xmlData != nil)
					}
				}//was able to read plist file and create xml data
				CFRelease(versionFileURL);
			}//if (versionFileURL != NULL)
		}
	}//file exists
	return(outVersion);
}//GetComponentBuildVersion



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
		gConfigNodeRef = new CPlugInRef( CConfigurePlugin::ContextDeallocProc, 16 );
		if ( gConfigNodeRef == nil ) throw((SInt32)eMemoryAllocError);
	}

	if ( gConfigContinue == nil )
	{
		gConfigContinue = new CContinue( CConfigurePlugin::ContinueDeallocProc, 16 );
		if ( gConfigContinue == nil ) throw((SInt32)eMemoryAllocError);
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

SInt32 CConfigurePlugin::Validate ( const char *inVersionStr, const UInt32 inSignature )
{
	fPlugInSignature = inSignature;

	return( eDSNoErr );
} // Validate


// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

SInt32 CConfigurePlugin::SetPluginState ( const UInt32 inState )
{
    if ( inState & kActive )
    {
		//tell everyone we are ready to go
		WakeUpRequests();
    }
	return( eDSNoErr );
} // SetPluginState


// --------------------------------------------------------------------------------
//	* PeriodicTask ()
// --------------------------------------------------------------------------------

SInt32 CConfigurePlugin::PeriodicTask ( void )
{
//does nothing yet
	return( eDSNoErr );
} // PeriodicTask


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

SInt32 CConfigurePlugin::Initialize ( void )
{
	SInt32			siResult		= eDSNoErr;

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
	gKickConfigRequests.PostEvent();
} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CConfigurePlugin::WaitForInit ( void )
{
    // we wait for 2 minutes before giving up
    gKickConfigRequests.WaitForEvent( (UInt32)(2 * 60 * kMilliSecsPerSec) );
} // WaitForInit


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

SInt32 CConfigurePlugin::ProcessRequest ( void *inData )
{
	SInt32		siResult	= eDSNoErr;
	char	   *pathStr		= nil;

	try
	{
		if ( inData == nil )
		{
			throw( (SInt32)ePlugInDataError );
		}

		if (((sHeader *)inData)->fType == kOpenDirNode)
		{
			if (((sOpenDirNode *)inData)->fInDirNodeName != nil)
			{
				pathStr = ::dsGetPathFromListPriv( ((sOpenDirNode *)inData)->fInDirNodeName, "/" );
				if ( (pathStr != nil) && (strncmp(pathStr,"/Configure",10) != 0) )
				{
					throw( (SInt32)eDSOpenNodeFailed);
				}
			}
		}
		else if( ((sHeader *)inData)->fType == kKerberosMutex || ((sHeader *)inData)->fType == kServerRunLoop )
		{
			// we don't care about Kerberos mutexes here
			return eDSNoErr;
		}		
		
		WaitForInit();

		if (fState == kUnknownState)
		{
			throw( (SInt32)ePlugInCallTimedOut );
		}

        if ( (fState & kFailedToInit) || !(fState & kInitialized) )
        {
            throw( (SInt32)ePlugInFailedToInitialize );
        }

        if ( (fState & kInactive) || !(fState & kActive) )
        {
            throw( (SInt32)ePlugInNotActive );
        }
        
		siResult = HandleRequest( inData );
	}

	catch( SInt32 err )
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

SInt32 CConfigurePlugin::HandleRequest ( void *inData )
{
	SInt32		siResult	= eDSNoErr;
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

            case kDoPlugInCustomCall:
                siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
                break;
                
            case kDoDirNodeAuth:
                siResult = DoDirNodeAuth( (sDoDirNodeAuth *)inData );
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

	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // HandleRequest


//------------------------------------------------------------------------------------
//	* ReleaseContinueData
//------------------------------------------------------------------------------------

SInt32 CConfigurePlugin::ReleaseContinueData ( sReleaseContinueData *inData )
{
	SInt32	siResult	= eDSNoErr;

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

SInt32 CConfigurePlugin::OpenDirNode ( sOpenDirNode *inData )
{
	SInt32			siResult	= eDSOpenNodeFailed;
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

					if (pContext == nil ) throw( (SInt32)eMemoryAllocError);

					gConfigNodeRef->AddItem( inData->fOutNodeRef, pContext );
				}

				delete( pathStr );
				pathStr = nil;
			}
		}

	}

	catch( SInt32 err )
	{
		siResult = err;
	}
	return( siResult );

} // OpenDirNode


//------------------------------------------------------------------------------------
//	* CloseDirNode
//------------------------------------------------------------------------------------

SInt32 CConfigurePlugin::CloseDirNode ( sCloseDirNode *inData )
{
	SInt32			siResult		= eDSNoErr;
	sConfigContextData   *pContext		= nil;

	try
	{
		pContext = (sConfigContextData *) gConfigNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (SInt32)eDSInvalidNodeRef );

		gConfigNodeRef->RemoveItem( inData->fInNodeRef );
		gConfigContinue->RemoveItems( inData->fInNodeRef );
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // CloseDirNode


//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

SInt32 CConfigurePlugin::GetDirNodeInfo ( sGetDirNodeInfo *inData )
{
	SInt32				siResult		= eDSNoErr;
	UInt32				uiOffset		= 0;
	UInt32				uiCntr			= 1;
	UInt32				uiAttrCnt		= 0;
	CAttributeList	   *inAttrList		= nil;
	char			   *pAttrName		= nil;
	char			   *pData			= nil;
	sConfigContextData	   *pContext		= nil;
	sConfigContextData	   *pAttrContext	= nil;
	CBuff				outBuff;
	CDataBuff		   *aRecData		= nil;
	CDataBuff		   *aAttrData		= nil;
	CDataBuff		   *aTmpData		= nil;
	SInt32				buffResult		= eDSNoErr;

// Can extract here the following:
// kDS1AttrENetAddress
// kDSNAttrIPAddress
// kDS1AttrBuildVersion
// kDS1AttrFWVersion
// kDS1AttrCoreFWVersion
// kDS1AttrRefNumTableList returns data from gRefTable
// kDS1AttrReadOnlyNode

	try
	{
		if ( inData  == nil ) throw( (SInt32) eMemoryError );

		pContext = (sConfigContextData *)gConfigNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (SInt32)eDSBadContextData );

		inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
		if ( inAttrList == nil ) throw( (SInt32)eDSNullNodeInfoTypeList );
		if (inAttrList->GetCount() == 0) throw( (SInt32)eDSEmptyNodeInfoTypeList );

		siResult = outBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff.SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw( siResult );

		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (SInt32) eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (SInt32) eMemoryError );
		aTmpData = new CDataBuff();
		if ( aTmpData  == nil ) throw( (SInt32) eMemoryError );

		// Set the record name and type
		aRecData->AppendShort( ::strlen( kDSStdRecordTypeDirectoryNodeInfo ) );
		aRecData->AppendString( (char *)kDSStdRecordTypeDirectoryNodeInfo );
		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"DirectoryNodeInfo" );

		while ( inAttrList->GetAttribute( uiCntr++, &pAttrName ) == eDSNoErr )
		{
			//package up all the dir node attributes dependant upon what was asked for
			
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrReadOnlyNode ) == 0) )
			{
				uiAttrCnt++;

				//possible for a node to be ReadOnly, ReadWrite, WriteOnly
				//note that ReadWrite does not imply fully readable or writable
				buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, inData->fInAttrInfoOnly, kDS1AttrReadOnlyNode, "ReadOnly" );
			}

			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrENetAddress ) == 0) )
			{
				uiAttrCnt++;

				char *stringValue = nil;
				if ( inData->fInAttrInfoOnly == false )
				{
					CFStringRef aLZMACAddress = NULL;
					CFStringRef aNLZMACAddress = NULL;
					stringValue = (char *)calloc(1, sizeof("00:00:00:00:00:00")); //format leading zeroes
					GetMACAddress( &aLZMACAddress, &aNLZMACAddress, true );
					CFStringGetCString(aLZMACAddress, stringValue, sizeof("00:00:00:00:00:00"), kCFStringEncodingUTF8);
					
					if ( DSIsStringEmpty(stringValue) )
					{
						strcpy(stringValue, "unknown");
					}
					if (aLZMACAddress != NULL) CFRelease(aLZMACAddress);
					if (aNLZMACAddress != NULL) CFRelease(aNLZMACAddress);
				}

				buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, inData->fInAttrInfoOnly, kDS1AttrENetAddress, stringValue );
				
				DSFreeString(stringValue);
			}

			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDSNAttrIPAddress ) == 0) )
			{
				uiAttrCnt++;
				
				const char * ipAddressString = nil;
				
				if ( inData->fInAttrInfoOnly == false )
				{
					DSNetworkUtilities::Initialize();
					DSNetworkUtilities::GetOurIPAddress(0); //init network class if required
					ipAddressString = DSNetworkUtilities::GetOurIPAddressString(0); //only get first one
					
					if (ipAddressString == nil)
					{
						ipAddressString = "unknown";
					}
				}
				buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, inData->fInAttrInfoOnly, kDSNAttrIPAddress, ipAddressString );
			}

			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrBuildVersion ) == 0) )
			{
				uiAttrCnt++;

				const char* buildVersion = gStrDaemonBuildVersion;
				if ( inData->fInAttrInfoOnly == false )
				{
					if (buildVersion == nil)
					{
						buildVersion = "unknown";
					}
				}
				buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, inData->fInAttrInfoOnly, kDS1AttrBuildVersion, buildVersion );
			}

			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrFWVersion ) == 0) )
			{
				uiAttrCnt++;

				char * buildVersion = nil;
				if ( inData->fInAttrInfoOnly == false )
				{
					//look in /System/Library/Frameworks/DirectoryService.framework/Versions/Current/Resources/version.plist
					//"SourceVersion" dictionary key
					buildVersion = CopyComponentBuildVersion( "/System/Library/Frameworks/DirectoryService.framework/Versions/Current/Resources/version.plist", "CFBundleVersion", 16);
					if (buildVersion == nil)
					{
						buildVersion = strdup("unknown");
					}
				}
				buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, inData->fInAttrInfoOnly, kDS1AttrFWVersion, buildVersion );
				DSFreeString(buildVersion);
			}

			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrCoreFWVersion ) == 0) )
			{
				uiAttrCnt++;

				char * buildVersion = nil;
				if ( inData->fInAttrInfoOnly == false )
				{
					//look in /System/Library/PrivateFrameworks/DirectoryServiceCore.framework/Versions/Current/Resources/version.plist
					//"SourceVersion" dictionary key
					buildVersion = CopyComponentBuildVersion( "/System/Library/PrivateFrameworks/DirectoryServiceCore.framework/Versions/Current/Resources/version.plist", "CFBundleVersion", 16);
					if (buildVersion == nil)
					{
						buildVersion = strdup("unknown");
					}
				}
				buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, inData->fInAttrInfoOnly, kDS1AttrCoreFWVersion, buildVersion );
				DSFreeString(buildVersion);
			}
            
            if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDSNAttrRecordType ) == 0) )
			{
				uiAttrCnt++;

				buffResult = dsCDataBuffFromAttrTypeAndStringValues( aAttrData, aTmpData, inData->fInAttrInfoOnly, kDSNAttrRecordType, 
                    kDSStdRecordTypeAttributeTypes, kDSStdRecordTypePlugins, kDSStdRecordTypeRecordTypes, kDSStdRecordTypeRefTableEntries, NULL );
			}
//TODO  remove (::strcmp( pAttrName, kDSAttributesAll ) == 0) after dscl bug is fixed
			if ( ( gRefTable != nil ) && ( (::strcmp( pAttrName, kDS1AttrRefNumTableList) == 0) || (::strcmp( pAttrName, kDSAttributesAll ) == 0) ) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrRefNumTableList ) );
				aTmpData->AppendString( kDS1AttrRefNumTableList );

				if ( inData->fInAttrInfoOnly == true )
				{
					// Attribute value count
					aTmpData->AppendShort( 0 );
				}
				else
				{
					//access gRefTable
					CDataBuff	*aSubData = nil;
					aSubData = new CDataBuff();
					if ( aSubData  == nil ) throw( (SInt32) eMemoryError );
				
					char* attrValue = nil;
					UInt32 attrCount = 0;
					tIPPIDDirRefMap::iterator theIPEntry;
					tPIDDirRefMap::iterator thePIDEntry;
					gRefTable->LockClientPIDList();
					attrValue = gRefTable->CreateNextClientPIDListString( true, theIPEntry, thePIDEntry );
					while(attrValue != nil)
					{
						attrCount++;
						aSubData->AppendLong( ::strlen( attrValue ) );
						aSubData->AppendString( attrValue );
						DSFreeString(attrValue);
						attrValue = gRefTable->CreateNextClientPIDListString( false, theIPEntry, thePIDEntry );
					}
					gRefTable->UnlockClientPIDList();
					aTmpData->AppendShort( attrCount );
					aTmpData->AppendBlock( aSubData->GetData(), aSubData->GetLength() );
					delete( aSubData );
					aSubData = nil;
				}

				// Add the attribute length and data
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
			}

		} // while

		aRecData->AppendShort( uiAttrCnt );
		if (uiAttrCnt > 0)
		{
			aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
		}

		outBuff.AddData( aRecData->GetData(), aRecData->GetLength() );
		inData->fOutAttrInfoCount = uiAttrCnt;

		pData = outBuff.GetDataBlock( 1, &uiOffset );
		if ( pData != nil )
		{
			pAttrContext = MakeContextData();
			if ( pAttrContext  == nil ) throw( (SInt32) eMemoryAllocError );
			
		//add to the offset for the attr list the length of the GetDirNodeInfo fixed record labels
//		record length = 4
//		aRecData->AppendShort( ::strlen( kDSStdRecordTypeDirectoryNodeInfo ) ); = 2
//		aRecData->AppendString( kDSStdRecordTypeDirectoryNodeInfo ); = 35
//		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "DirectoryNodeInfo" ); = 17
//		total adjustment = 4 + 2 + 35 + 2 + 17 = 60

			pAttrContext->offset = uiOffset + 60;

			gConfigNodeRef->AddItem( inData->fOutAttrListRef, pAttrContext );
		}
        else
        {
            siResult = eDSBufferTooSmall;
        }
	}

	catch ( SInt32 err )
	{
		siResult = err;
	}

	if ( inAttrList != nil )
	{
		delete( inAttrList );
		inAttrList = nil;
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

} // GetDirNodeInfo

//------------------------------------------------------------------------------------
//	* GetRecordList
//------------------------------------------------------------------------------------

SInt32 CConfigurePlugin::GetRecordList ( sGetRecordList *inData )
{
	SInt32						siResult		= eDSNoErr;
	UInt32						i				= 0;
	UInt32						uiTotal			= 0;
	UInt32						uiCount			= 0;
	char					   *pRecName		= nil;
	char					   *pRecType		= nil;
	char					   *pNIRecType		= nil;
	bool						bAttribOnly		= false;
	tDirPatternMatch			pattMatch		= eDSNoMatch1;
	CAttributeList 			   *cpRecNameList	= nil;
	CAttributeList 			   *cpRecTypeList	= nil;
	CAttributeList 			   *cpAttrTypeList 	= nil;
	sConfigContextData		   *pContext		= nil;
	sConfigContinueData		   *pContinue		= nil;
	CBuff					   *outBuff			= nil;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	const char				   *typeName		= nil;
    SInt32						siValCnt		= 0;
	UInt32						fillIndex		= 0;
	CDataBuff				   *aRecData		= nil;
	CDataBuff				   *aAttrData		= nil;
	CDataBuff				   *aTmpData		= nil;
	SInt32						buffResult		= eDSNoErr;

	try
	{
		aRecData	= new CDataBuff();
		if ( aRecData == nil ) throw((SInt32)eMemoryAllocError);

		aAttrData	= new CDataBuff();
		if ( aAttrData == nil ) throw((SInt32)eMemoryAllocError);

		aTmpData	= new CDataBuff();
		if ( aTmpData == nil ) throw((SInt32)eMemoryAllocError);

		// Verify all the parameters
		if ( inData == nil ) throw( (SInt32)eMemoryError );
		if ( inData->fInDataBuff == nil ) throw( (SInt32)eDSEmptyBuffer );
		if (inData->fInDataBuff->fBufferSize == 0) throw( (SInt32)eDSEmptyBuffer );

		if ( inData->fInRecNameList == nil ) throw( (SInt32)eDSEmptyRecordNameList );
		if ( inData->fInRecTypeList == nil ) throw( (SInt32)eDSEmptyRecordTypeList );
		if ( inData->fInAttribTypeList == nil ) throw( (SInt32)eDSEmptyAttributeTypeList );

		// Node context data
		pContext = (sConfigContextData *)gConfigNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (SInt32)eDSInvalidNodeRef );

		if ( inData->fIOContinueData == nil )
		{
			pContinue = new sConfigContinueData;
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
				throw( (SInt32)eDSInvalidContinueData );
			}
		}

		inData->fIOContinueData = nil;

		outBuff = new CBuff();
		if ( outBuff == nil ) throw( (SInt32)eMemoryError );

		siResult = outBuff->Initialize( inData->fInDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff->GetBuffStatus();
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff->SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw( siResult );

		// Get the record name list
		cpRecNameList = new CAttributeList( inData->fInRecNameList );
		if ( cpRecNameList == nil ) throw( (SInt32)eDSEmptyRecordNameList );
		if (cpRecNameList->GetCount() == 0) throw( (SInt32)eDSEmptyRecordNameList );

		// Get the record pattern match
		pattMatch = inData->fInPatternMatch;

		// Get the record type list
		cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
		if ( cpRecTypeList == nil ) throw( (SInt32)eDSEmptyRecordTypeList );
		if (cpRecTypeList->GetCount() == 0) throw( (SInt32)eDSEmptyRecordTypeList );

		// Get the attribute list
		cpAttrTypeList = new CAttributeList( inData->fInAttribTypeList );
		if ( cpAttrTypeList == nil ) throw( (SInt32)eDSEmptyAttributeTypeList );
		if (cpAttrTypeList->GetCount() == 0) throw( (SInt32)eDSEmptyAttributeTypeList );

		// Get the attribute info only flag
		bAttribOnly = inData->fInAttribInfoOnly;

		// get these type of records
		while ( cpRecTypeList->GetAttribute( pContinue->fRecTypeIndex, &pRecType ) == eDSNoErr )
		{
			bool bUseOldConst = strncmp( pRecType, kDSStdRecordTypePrefix, sizeof(kDSStdRecordTypePrefix) - 1 ) != 0;
			// get this record type
			if ( ( ::strcmp( pRecType, kDSConfigPluginsRecType ) == 0 ) || ( ::strcmp( pRecType, kDSStdRecordTypePlugins ) == 0 ) )
			{
				// get these names
				while ( cpRecNameList->GetAttribute( pContinue->fRecNameIndex, &pRecName ) == eDSNoErr )
				{
					// Get all records of this name
					if ( ( ::strcmp( pRecName, kDSConfigRecordsAll ) == 0 ) || ( ::strcmp( pRecName, kDSRecordsAll ) == 0 ) )
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

                                if ( (pPIInfo->fName != nil) && ( ::strcmp(pPIInfo->fName,"Cache") != 0) && ( ::strcmp(pPIInfo->fName,"Configure") != 0) && ( ::strcmp(pPIInfo->fName,"Search") != 0) )
                                {
                                    // Add the record type which in this case is config node
                                    aRecData->AppendShort( ::strlen( kDSStdRecordTypePlugins ) );
                                    aRecData->AppendString( kDSStdRecordTypePlugins );

                                    // Add the record name which in this case is the config node name
                                    aRecData->AppendShort( ::strlen( pPIInfo->fName ) );
                                    aRecData->AppendString( pPIInfo->fName );

                                    aAttrData->Clear();

                                    //let's get the attributes in this order
                                    //plugin name, plugin table index, plugin status, plugin software version, plugin config HI avail, config file

                                    siValCnt = 6;
                                    
									buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, false, kDSNAttrRecordName, pPIInfo->fName );
                                    
                                    aTmpData->Clear();

									if (bUseOldConst)
									{
										//append the plugin table index attr name
										aTmpData->AppendShort( ::strlen( kDSConfigAttrPlugInIndex ) );
										aTmpData->AppendString( kDSConfigAttrPlugInIndex );
									}
									else
									{
										//append the plugin table index attr name
										aTmpData->AppendShort( ::strlen( kDS1AttrPluginIndex ) );
										aTmpData->AppendString( kDS1AttrPluginIndex );
									}
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

									if (bUseOldConst)
									{
										//append the plugin status attr name
										aTmpData->AppendShort( ::strlen( kDSConfigAttrState ) );
										aTmpData->AppendString( kDSConfigAttrState );
									}
									else
									{
										//append the plugin status attr name
										aTmpData->AppendShort( ::strlen( kDS1AttrFunctionalState ) );
										aTmpData->AppendString( kDS1AttrFunctionalState );
									}
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

									if (bUseOldConst)
									{
										//append the plugin version attr name
										aTmpData->AppendShort( ::strlen( kDSConfigAttrVersion ) );
										aTmpData->AppendString( kDSConfigAttrVersion );
									}
									else
									{
										//append the plugin version attr name
										aTmpData->AppendShort( ::strlen( kDS1AttrVersion ) );
										aTmpData->AppendString( kDS1AttrVersion );
									}
                                    // Append the attribute value count
                                    aTmpData->AppendShort( 1 );
                                    // Append attribute value
                                    aTmpData->AppendLong( ::strlen( pPIInfo->fVersion ));
                                    aTmpData->AppendString( pPIInfo->fVersion );
									
									// Add the attribute length
									aAttrData->AppendLong( aTmpData->GetLength() );
									aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                                    aTmpData->Clear();

									if (bUseOldConst)
									{
										//append the plugin config avail attr name
										aTmpData->AppendShort( ::strlen( kDSConfigAttrConfigAvail ) );
										aTmpData->AppendString( kDSConfigAttrConfigAvail );
									}
									else
									{
										//append the plugin config avail attr name
										aTmpData->AppendShort( ::strlen( kDS1AttrConfigAvail ) );
										aTmpData->AppendString( kDS1AttrConfigAvail );
									}
                                    // Append the attribute value count
                                    aTmpData->AppendShort( 1 );
                                    // Append attribute value
                                    aTmpData->AppendLong( ::strlen( pPIInfo->fConfigAvail ));
                                    aTmpData->AppendString( pPIInfo-> fConfigAvail );
									
									// Add the attribute length
									aAttrData->AppendLong( aTmpData->GetLength() );
									aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                                    aTmpData->Clear();

									if (bUseOldConst)
									{
										//append the plugin config file attr name
										aTmpData->AppendShort( ::strlen( kDSConfigAttrConfigFile ) );
										aTmpData->AppendString( kDSConfigAttrConfigFile );
									}
									else
									{
										//append the plugin config file attr name
										aTmpData->AppendShort( ::strlen( kDS1AttrConfigFile ) );
										aTmpData->AppendString( kDS1AttrConfigFile );
									}
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
								throw( (SInt32)eDSBufferTooSmall );
							}
							else
							{
								inData->fIOContinueData = pContinue;
								inData->fOutRecEntryCount = uiTotal;
								outBuff->SetLengthToSize();

								throw( (SInt32)eDSNoErr );
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
			else if ( ( ::strcmp( pRecType, kDSConfigRecordsType ) == 0 ) || ( ::strcmp( pRecType, kDSStdRecordTypeRecordTypes ) == 0 ) )
			{
				// get these names
				while ( cpRecNameList->GetAttribute( pContinue->fRecNameIndex, &pRecName ) == eDSNoErr )
				{
					// Get all records of this name
					if ( ( ::strcmp( pRecName, kDSConfigRecordsAll ) == 0 ) || ( ::strcmp( pRecName, kDSRecordsAll ) == 0 ) )
					{
						//setup to work with the continue data
						i = pContinue->fAllRecIndex;

						//search over all the record types in the table
						while ( sRecTypes[i] != NULL )
						{

                            typeName = sRecTypes[i];

                            if ( typeName != nil )
                            {
								aRecData->Clear();

								// Add the record type which in this case is config node
								aRecData->AppendShort( ::strlen( pRecType ) );
								aRecData->AppendString( pRecType );

								// Add the record name which in this case is the config node name
								aRecData->AppendShort( ::strlen( typeName ) );
								aRecData->AppendString( typeName );

								aAttrData->Clear();
                                
								if (!bUseOldConst)
								{
									siResult = dsCDataBuffFromAttrTypeAndStringValues( aAttrData, aTmpData, bAttribOnly, kDSNAttrAttributeTypes, 
										(const char**)sRecTypeAttributes[i] );
									siValCnt = 1;
								}
								else
								{
									siValCnt = 0;
								}
								// Attribute count
								aRecData->AppendShort( siValCnt );
								aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );

								//add the record data to the buffer
								siResult = outBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
								if ( siResult != eDSNoErr )
								{
									pContinue->fAllRecIndex = i;
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
								throw( (SInt32)eDSBufferTooSmall );
							}
							else
							{
								inData->fIOContinueData = pContinue;
								inData->fOutRecEntryCount = uiTotal;
								outBuff->SetLengthToSize();

								throw( (SInt32)eDSNoErr );
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
			else if ( ( ::strcmp( pRecType, kDSConfigAttributesType ) == 0 ) || ( ::strcmp( pRecType, kDSStdRecordTypeAttributeTypes ) == 0 ) )
			{
				// get these names
				while ( cpRecNameList->GetAttribute( pContinue->fRecNameIndex, &pRecName ) == eDSNoErr )
				{
					// Get all records of this name
					if ( ( ::strcmp( pRecName, kDSConfigRecordsAll ) == 0 ) || ( ::strcmp( pRecName, kDSRecordsAll ) == 0 ) )
					{
						//setup to work with the continue data
						i = pContinue->fAllRecIndex;

						//search over all the attr types in the table
						while ( sAttrTypes[i] != NULL )
						{
                            typeName = sAttrTypes[i];

                            if ( typeName != nil )
                            {
								aRecData->Clear();

								// Add the record type which in this case is config node
								aRecData->AppendShort( ::strlen( pRecType ) );
								aRecData->AppendString( pRecType );

								// Add the record name which in this case is the config node name
								aRecData->AppendShort( ::strlen( typeName ) );
								aRecData->AppendString( typeName );

								aAttrData->Clear();

								siValCnt = 0;
								// Attribute count
								aRecData->AppendShort( siValCnt );
								aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );

								//add the record (plugin) data to the buffer
								siResult = outBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
								if ( siResult != eDSNoErr )
								{
									pContinue->fAllRecIndex = i;
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
								throw( (SInt32)eDSBufferTooSmall );
							}
							else
							{
								inData->fIOContinueData = pContinue;
								inData->fOutRecEntryCount = uiTotal;
								outBuff->SetLengthToSize();

								throw( (SInt32)eDSNoErr );
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
			else if ( ::strcmp( pRecType, kDSStdRecordTypeRefTableEntries ) == 0 )
			{
				// get these names
				while ( cpRecNameList->GetAttribute( pContinue->fRecNameIndex, &pRecName ) == eDSNoErr )
				{
					// Get all records of this name
					if ( ::strcmp( pRecName, kDSRecordsAll ) == 0 )
					{
						//does NOT work with the continue data ie. send back eDSBufferTooSmall error if required
						//why? Because can't guarantee that the ref table stays the same across continue calls
						//nor can we guarantee any order to the records
						//and since we need to build the record names out of the IP and PID since they are not fixed

						char* listRecNameValue = nil;
						tIPPIDDirRefMap::iterator theIPEntry;
						tPIDDirRefMap::iterator thePIDEntry;
						char* anIPAddress = nil;
						SInt32 anIP = 0;
						char* aPIDValue = nil;
						UInt32 aPID = 0;
						char* aProcessName = nil;
						UInt32 aTotalRefCount = 0;
						char* outString = nil;
						UInt32 aDirRefCount = 0;
						char** theDirRefs = nil;
						UInt32 aNodeRefCount = 0;
						char** theNodeRefs = nil;
						UInt32 aRecRefCount = 0;
						char** theRecRefs = nil;
						UInt32 aAttrListRefCount = 0;
						char** theAttrListRefs = nil;
						UInt32 aAttrListValueRefCount = 0;
						char** theAttrListValueRefs = nil;
						
						gRefTable->Lock(); //table mutex
						gRefTable->LockClientPIDList(); //list mutex
						
						listRecNameValue = gRefTable->CreateNextClientPIDListRecordName( true, theIPEntry, thePIDEntry,
													&anIPAddress, &aPIDValue, anIP, aPID, aTotalRefCount, aDirRefCount, theDirRefs );
						while(listRecNameValue != nil)
						{
							gRefTable->RetrieveRefDataPerClientPIDAndIP(anIP, aPID, theDirRefs,
																		aNodeRefCount, theNodeRefs,
																		aRecRefCount, theRecRefs,
																		aAttrListRefCount, theAttrListRefs,
																		aAttrListValueRefCount, theAttrListValueRefs );

							//use the listRecNameValue to build the record name
							
							aRecData->Clear();

							// Add the record type which in this case is config node
							aRecData->AppendShort( ::strlen( pRecType ) );
							aRecData->AppendString( pRecType );

							// Add the record name which in this case is the config node name
							aRecData->AppendShort( ::strlen( listRecNameValue ) );
							aRecData->AppendString( listRecNameValue );

							//let's get the attributes in this order
							//record name - kDSNAttrRecordName
							//IP address - kDSNAttrIPAddress
							//PID - kDS1AttrPIDValue
							//Process name - kDS1AttrProcessName
							//total ref count - kDS1AttrTotalRefCount
							//dir ref count - kDS1AttrDirRefCount
							//dir refs - kDSNAttrDirRefs
							//node ref count - kDS1AttrNodeRefCount
							//node refs - kDSNAttrNodeRefs
							//record ref count - kDS1AttrRecRefCount
							//record refs - kDSNAttrRecRefs
							//attr list ref count - kDS1AttrAttrListRefCount
							//attr list refs - kDSNAttrAttrListRefs
							//attr list value ref count - kDS1AttrAttrListValueRefCount
							//attr list value refs - kDSNAttrAttrListValueRefs
							
							//now get all the attrs
							
							aAttrData->Clear();

							buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, false, kDSNAttrRecordName, listRecNameValue );
							
							buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, false, kDSNAttrIPAddress, anIPAddress );
							
							buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, false, kDS1AttrPIDValue, aPIDValue );
							
							if (strcmp(anIPAddress, "localhost") == 0)
							{
								if (aPID == 0)
								{
									aProcessName = dsGetNameForProcessID(getpid());
								}
								else
								{
									aProcessName = dsGetNameForProcessID(aPID);
								}
							}
							buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, false, kDS1AttrProcessName, aProcessName );

							outString = (char *)calloc(8+1,sizeof(char*));
							sprintf(outString, "%u", (unsigned int)aTotalRefCount);
							buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, false, kDS1AttrTotalRefCount, outString );
							DSFreeString(outString);
							
							outString = (char *)calloc(8+1,sizeof(char*));
							sprintf(outString, "%u", (unsigned int)aDirRefCount);
							buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, false, kDS1AttrDirRefCount, outString );
							DSFreeString(outString);
							
							buffResult = dsCDataBuffFromAttrTypeAndStringValues( aAttrData, aTmpData, false, kDSNAttrDirRefs, (const char **)theDirRefs );
							
							outString = (char *)calloc(8+1,sizeof(char*));
							sprintf(outString, "%u", (unsigned int)aNodeRefCount);
							buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, false, kDS1AttrNodeRefCount, outString );
							DSFreeString(outString);
							
							buffResult = dsCDataBuffFromAttrTypeAndStringValues( aAttrData, aTmpData, false, kDSNAttrNodeRefs, (const char **)theNodeRefs );
							
							outString = (char *)calloc(8+1,sizeof(char*));
							sprintf(outString, "%u", (unsigned int)aRecRefCount);
							buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, false, kDS1AttrRecRefCount, outString );
							DSFreeString(outString);
							
							buffResult = dsCDataBuffFromAttrTypeAndStringValues( aAttrData, aTmpData, false, kDSNAttrRecRefs, (const char **)theRecRefs );
							
							outString = (char *)calloc(8+1,sizeof(char*));
							sprintf(outString, "%u", (unsigned int)aAttrListRefCount);
							buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, false, kDS1AttrAttrListRefCount, outString );
							DSFreeString(outString);
							
							buffResult = dsCDataBuffFromAttrTypeAndStringValues( aAttrData, aTmpData, false, kDSNAttrAttrListRefs, (const char **)theAttrListRefs );
							
							outString = (char *)calloc(8+1,sizeof(char*));
							sprintf(outString, "%u", (unsigned int)aAttrListValueRefCount);
							buffResult = dsCDataBuffFromAttrTypeAndStringValue( aAttrData, aTmpData, false, kDS1AttrAttrListValueRefCount, outString );
							DSFreeString(outString);
							
							buffResult = dsCDataBuffFromAttrTypeAndStringValues( aAttrData, aTmpData, false, kDSNAttrAttrListValueRefs, (const char **)theAttrListValueRefs );
							
							siValCnt = 15;
							// Attribute count
							aRecData->AppendShort( siValCnt );
							aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );

							//add the record data to the buffer
							siResult = outBuff->AddData( aRecData->GetData(), aRecData->GetLength() );

							DSFreeString(listRecNameValue);
							DSFreeString(anIPAddress);
							anIP = 0;
							DSFreeString(aPIDValue);
							aPID = 0;
							DSFreeString(aProcessName);
							aTotalRefCount = 0;
							aDirRefCount = 0;
							DSFreeStringList(theDirRefs);
							aNodeRefCount = 0;
							DSFreeStringList(theNodeRefs);
							aRecRefCount = 0;
							DSFreeStringList(theRecRefs);
							aAttrListRefCount = 0;
							DSFreeStringList(theAttrListRefs);
							aAttrListValueRefCount = 0;
							DSFreeStringList(theAttrListValueRefs);

							if ( siResult != eDSNoErr )
							{
								break;
							}

							listRecNameValue = gRefTable->CreateNextClientPIDListRecordName( false, theIPEntry, thePIDEntry,
													&anIPAddress, &aPIDValue, anIP, aPID, aTotalRefCount, aDirRefCount, theDirRefs );
						}
						
						gRefTable->UnlockClientPIDList(); //list mutex
						gRefTable->Unlock(); //table mutex
						
						outBuff->GetDataBlockCount( &uiCount );

						if ( siResult == CBuff::kBuffFull )
						{
							throw( (SInt32)eDSBufferTooSmall );
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
			}
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

	catch( SInt32 err )
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

SInt32 CConfigurePlugin::GetRecordEntry ( sGetRecordEntry *inData )
{
	SInt32					siResult		= eDSNoErr;
	UInt32					uiIndex			= 0;
	UInt32					uiCount			= 0;
	UInt32					uiOffset		= 0;
	UInt32					uberOffset		= 0;
	char 				   *pData			= nil;
	tRecordEntryPtr			pRecEntry		= nil;
	sConfigContextData		   *pContext		= nil;
	CBuff					inBuff;
	UInt32					offset			= 0;
	UInt16					usTypeLen		= 0;
	char				   *pRecType		= nil;
	UInt16					usNameLen		= 0;
	char				   *pRecName		= nil;
	UInt16					usAttrCnt		= 0;
	UInt32					buffLen			= 0;

	try
	{
		if ( inData  == nil ) throw( (SInt32)eMemoryError );
		if ( inData->fInOutDataBuff  == nil ) throw( (SInt32)eDSEmptyBuffer );
		if (inData->fInOutDataBuff->fBufferSize == 0) throw( (SInt32)eDSEmptyBuffer );

		siResult = inBuff.Initialize( inData->fInOutDataBuff );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = inBuff.GetDataBlockCount( &uiCount );
		if ( siResult != eDSNoErr ) throw( siResult );

		uiIndex = inData->fInRecEntryIndex;
		if ( uiIndex == 0 ) throw( (SInt32)eDSInvalidIndex );

		if ( uiIndex > uiCount ) throw( (SInt32)eDSIndexOutOfRange );

		pData = inBuff.GetDataBlock( uiIndex, &uberOffset );
		if ( pData  == nil ) throw( (SInt32)eDSCorruptBuffer );

		//assume that the length retrieved is valid
		buffLen = inBuff.GetDataBlockLength( uiIndex );
		
		// Skip past the record length
		pData	+= 4;
		offset	= 0; //buffLen does not include first four bytes

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record type
		::memcpy( &usTypeLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecType = pData;
		
		pData	+= usTypeLen;
		offset	+= usTypeLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record name
		::memcpy( &usNameLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecName = pData;
		
		pData	+= usNameLen;
		offset	+= usNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
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
		if ( pContext  == nil ) throw( (SInt32)eMemoryAllocError );

		pContext->offset = uberOffset + offset + 4;	// context used by next calls of GetAttributeEntry
													// include the four bytes of the buffLen
		
		gConfigNodeRef->AddItem( inData->fOutAttrListRef, pContext );

		inData->fOutRecEntryPtr = pRecEntry;
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetRecordEntry


//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

SInt32 CConfigurePlugin::GetAttributeEntry ( sGetAttributeEntry *inData )
{
	SInt32					siResult			= eDSNoErr;
	UInt16					usAttrTypeLen		= 0;
	UInt16					usAttrCnt			= 0;
	UInt32					usAttrLen			= 0;
	UInt16					usValueCnt			= 0;
	UInt32					usValueLen			= 0;
	UInt32					i					= 0;
	UInt32					uiIndex				= 0;
	UInt32					uiAttrEntrySize		= 0;
	UInt32					uiOffset			= 0;
	UInt32					uiTotalValueSize	= 0;
	UInt32					offset				= 0;
	UInt32					buffSize			= 0;
	UInt32					buffLen				= 0;
	char				   *p			   		= nil;
	char				   *pAttrType	   		= nil;
	tDataBuffer			   *pDataBuff			= nil;
	tAttributeValueListRef	attrValueListRef	= 0;
	tAttributeEntryPtr		pAttribInfo			= nil;
	sConfigContextData		   *pAttrContext		= nil;
	sConfigContextData		   *pValueContext		= nil;

	try
	{
		if ( inData  == nil ) throw( (SInt32)eMemoryError );

		pAttrContext = (sConfigContextData *)gConfigNodeRef->GetItemData( inData->fInAttrListRef );
		if ( pAttrContext  == nil ) throw( (SInt32)eDSBadContextData );

		uiIndex = inData->fInAttrInfoIndex;
		if (uiIndex == 0) throw( (SInt32)eDSInvalidIndex );
		
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (SInt32)eDSNullDataBuff );
		
		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pAttrContext->offset;
		offset	= pAttrContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the attribute count
		::memcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt) throw( (SInt32) eDSIndexOutOfRange );

		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		// Skip to the attribute that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
		
			// Get the length for the attribute
			::memcpy( &usAttrLen, p, 4 );

			// Move the offset past the length word and the length of the data
			p		+= 4 + usAttrLen;
			offset	+= 4 + usAttrLen;
		}

		// Get the attribute offset
		uiOffset = offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute block
		::memcpy( &usAttrLen, p, 4 );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		::memcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
		
		for ( i = 0; i < usValueCnt; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
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
		pAttribInfo->fAttributeValueMaxSize				= 512;				// KW this is not used anywhere
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		attrValueListRef = inData->fOutAttrValueListRef;

		pValueContext = MakeContextData();
		if ( pValueContext  == nil ) throw( (SInt32)eMemoryAllocError );

		pValueContext->offset = uiOffset;

		gConfigNodeRef->AddItem( inData->fOutAttrValueListRef, pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

SInt32 CConfigurePlugin::GetAttributeValue ( sGetAttributeValue *inData )
{
	SInt32						siResult		= eDSNoErr;
	UInt16						usValueCnt		= 0;
	UInt32						usValueLen		= 0;
	UInt16						usAttrNameLen	= 0;
	UInt32						i				= 0;
	UInt32						uiIndex			= 0;
	UInt32						offset			= 0;
	char					   *p				= nil;
	tDataBuffer				   *pDataBuff		= nil;
	tAttributeValueEntry	   *pAttrValue		= nil;
	sConfigContextData			   *pValueContext	= nil;
	UInt32						buffSize		= 0;
	UInt32						buffLen			= 0;
	UInt32						attrLen			= 0;

	try
	{
		pValueContext = (sConfigContextData *)gConfigNodeRef->GetItemData( inData->fInAttrValueListRef );
		if ( pValueContext  == nil ) throw( (SInt32)eDSBadContextData );

		uiIndex = inData->fInAttrValueIndex;
		if (uiIndex == 0) throw( (SInt32)eDSInvalidIndex );

		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (SInt32)eDSNullDataBuff );

		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pValueContext->offset;
		offset	= pValueContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
				
		// Get the buffer length
		::memcpy( &attrLen, p, 4 );

		//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
		//AND add the length of the buffer length var as stored ie. 4 bytes
		buffLen		= attrLen + pValueContext->offset + 4;
		if (buffLen > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the attribute name length
		::memcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the value count
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt)  throw( (SInt32) eDSIndexOutOfRange );

		// Skip to the value that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		::memcpy( &usValueLen, p, 4 );
		
		p		+= 4;
		offset	+= 4;

		//if (usValueLen == 0)  throw( (SInt32)eDSInvalidBuffFormat ); //if zero is it okay?

		pAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );

		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( usValueLen + offset > buffLen ) throw( (SInt32)eDSInvalidBuffFormat );
		
		::memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );

		// Set the attribute value ID
		pAttrValue->fAttributeValueID = 0x00;

		inData->fOutAttrValue = pAttrValue;
	}

	catch( SInt32 err )
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

SInt32 CConfigurePlugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	SInt32						siResult	= eDSNoErr;
	UInt32						aRequest	= 0;
	UInt32						pluginIndex	= 0;
	CPlugInList::sTableData    *pPIInfo		= nil;
	UInt32						thePIState	= 0;
	UInt32						bufLen		= 0;
	AuthorizationExternalForm   authExtForm;
	AuthorizationRef			authRef		= 0;
	AuthorizationItemSet	   *resultRightSet = NULL;
	sConfigContextData		   *pContext	= nil;

	try
	{
		bzero(&authExtForm,sizeof(AuthorizationExternalForm));
		pContext = (sConfigContextData *)gConfigNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (SInt32)eDSInvalidNodeRef );
		if ( inData == nil ) throw( (SInt32)eDSNullParameter );
		if ( inData->fInRequestData == nil ) throw( (SInt32)eDSNullDataBuff );
		if ( inData->fOutRequestResponse == nil ) throw( (SInt32)eDSNullDataBuff );
		if ( inData->fInRequestData->fBufferData == nil ) throw( (SInt32)eDSEmptyBuffer );

		aRequest = inData->fInRequestCode;
		AuthorizationItem rights[] = { {"system.services.directory.configure", 0, 0, 0} };
		AuthorizationItemSet rightSet = { sizeof(rights)/ sizeof(*rights), rights };
		bufLen = inData->fInRequestData->fBufferLength;
		
		if ( aRequest == eDSCustomCallConfigureGetAuthRef )
		{
			// we need to get an authref set up in this case
			// support for Directory Setup over proxy
			UInt32 userNameLength = 0;
			char* userName = NULL;
			UInt32 passwordLength = 0;
			char* password = NULL;
			char* current = inData->fInRequestData->fBufferData;
			UInt32 offset = 0;
			if ( bufLen < 2 * sizeof( UInt32 ) + 1 ) throw( (SInt32)eDSInvalidBuffFormat );
			
			memcpy( &userNameLength, current, sizeof( UInt32 ) );
			current += sizeof( UInt32 );
			offset += sizeof( UInt32 );
			if ( bufLen - offset < userNameLength ) throw( (SInt32)eDSInvalidBuffFormat );
			
			userName = current; //don't free this
			current += userNameLength;
			offset += userNameLength;
			if ( bufLen - offset < sizeof( UInt32 ) ) throw( (SInt32)eDSInvalidBuffFormat );
			
			memcpy( &passwordLength, current, sizeof( UInt32 ) );
			current += sizeof( UInt32 );
			offset += sizeof( UInt32 );
			if ( passwordLength == 0 )
			{
				password = "";
			}
			else
			{
				if ( bufLen - offset < passwordLength ) throw( (SInt32)eDSInvalidBuffFormat );
				password = current;
			}
			
			AuthorizationItem params[] = { {"username", userNameLength, (void*)userName, 0}, {"password", passwordLength, (void*)password, 0} };
			AuthorizationEnvironment environment = { sizeof(params)/ sizeof(*params), params };
			
			siResult = AuthorizationCreate( &rightSet, &environment, kAuthorizationFlagExtendRights, &authRef);
			if (siResult != errAuthorizationSuccess)
			{
				DbgLog( kLogPlugin, "CConfigure: AuthorizationCreate returned error %d", siResult );
				throw( (SInt32)eDSPermissionError );
			}
			if ( inData->fOutRequestResponse->fBufferSize < sizeof( AuthorizationExternalForm ) ) throw( (SInt32)eDSInvalidBuffFormat );
			siResult = AuthorizationMakeExternalForm(authRef, (AuthorizationExternalForm*)inData->fOutRequestResponse->fBufferData);
			if (siResult != errAuthorizationSuccess)
			{
				DbgLog( kLogPlugin, "CConfigure: AuthorizationMakeExternalForm returned error %d", siResult );
				throw( (SInt32)eDSPermissionError );
			}
			// should we free this authRef? probably not since it will be coming back to us
			inData->fOutRequestResponse->fBufferLength = sizeof( AuthorizationExternalForm );
			siResult = eDSNoErr;
			authRef = 0;
		}
		else if (aRequest == eDSCustomCallConfigureCheckVersion)
		{
			// version check, no AuthRef required
			UInt32 versLength = strlen( "1" );
			char* current = inData->fOutRequestResponse->fBufferData;
			inData->fOutRequestResponse->fBufferLength = 0;
			if ( inData->fOutRequestResponse->fBufferSize < sizeof(versLength) + versLength ) throw( (SInt32)eDSInvalidBuffFormat );
			memcpy(current, &versLength, sizeof(versLength));
			current += sizeof(versLength);
			inData->fOutRequestResponse->fBufferLength += sizeof(versLength);
			memcpy(current, "1", versLength);
			current += versLength;
			inData->fOutRequestResponse->fBufferLength += versLength;
		}
		else if (aRequest == eDSCustomCallConfigureSCGetKeyPathValueSize 
				 || aRequest == eDSCustomCallConfigureSCGetKeyPathValueData)
		{
			// read SystemConfiguration key, no authref required
			// for Remote Directory Setup
			UInt32 keyLength = bufLen;
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
				if (aRequest == eDSCustomCallConfigureSCGetKeyPathValueSize)
				{
					if ( inData->fOutRequestResponse->fBufferSize < sizeof(CFIndex) ) throw( (SInt32)eDSBufferTooSmall );
					memcpy(inData->fOutRequestResponse->fBufferData,&aRange.length,sizeof(CFIndex));
				}
				else
				{
					if ( inData->fOutRequestResponse->fBufferSize < (UInt32)aRange.length ) throw( (SInt32)eDSBufferTooSmall );
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
		else if (aRequest == eDSCustomCallConfigureSCGetKeyValueSize 
				 || aRequest == eDSCustomCallConfigureSCGetKeyValueData)
		{
			// read SystemConfiguration key, no authref required
			// for Remote Directory Setup
			UInt32 keyLength = bufLen;
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
				if (aRequest == eDSCustomCallConfigureSCGetKeyValueSize)
				{
					if ( inData->fOutRequestResponse->fBufferSize < sizeof(CFIndex) ) throw( (SInt32)eDSBufferTooSmall );
					memcpy(inData->fOutRequestResponse->fBufferData,&aRange.length,sizeof(CFIndex));
				}
				else
				{
					if ( inData->fOutRequestResponse->fBufferSize < (UInt32)aRange.length ) throw( (SInt32)eDSBufferTooSmall );
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
		else if ( aRequest == eDSCustomCallActivatePerfMonitor )
		{
			// this is a request to turn on performance stat gathering - auth?
			gSrvrCntl->ActivatePeformanceStatGathering();
			
		}
		else if ( aRequest == eDSCustomCallDeactivatePerfMonitor )
		{
			// this is a request to turn off performance stat gathering - auth?
			gSrvrCntl->DeactivatePeformanceStatGathering();			
		}
#endif
        else
		{
			if ( bufLen < sizeof( AuthorizationExternalForm ) ) throw( (SInt32)eDSInvalidBuffFormat );
			if (!(pContext->fEffectiveUID == 0 && 
				memcmp(inData->fInRequestData->fBufferData,&authExtForm,
						sizeof(AuthorizationExternalForm)) == 0)) {
				siResult = AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)inData->fInRequestData->fBufferData,
					&authRef);
				if (siResult != errAuthorizationSuccess)
				{
					DbgLog( kLogPlugin, "CConfigure: AuthorizationCreateFromExternalForm returned error %d", siResult );
					if (aRequest != eDSCustomCallConfigureCheckAuthRef)
					{
						syslog( LOG_ALERT, "Configure Custom Call <%d> AuthorizationCreateFromExternalForm returned error %d", aRequest, siResult );
					}
					throw( (SInt32)eDSPermissionError );
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
					DbgLog( kLogPlugin, "CConfigure: AuthorizationCopyRights returned error %d", siResult );
					if (aRequest != eDSCustomCallConfigureCheckAuthRef)
					{
						syslog( LOG_ALERT, "AuthorizationCopyRights returned error %d", siResult );
					}
					throw( (SInt32)eDSPermissionError );
				}
			}
		}
		
        //request to toggle the active versus inactive state of a plugin comes in with the plugin table index plus 1000
        //index could be zero
        if (aRequest >= eDSCustomCallTogglePlugInStateBase)
        {
			//might want to pass in the plugin name within the buffer and check it instead of using an offset from the 1000 value
            pluginIndex = aRequest - eDSCustomCallTogglePlugInStateBase;
            if (pluginIndex < CPlugInList::kMaxPlugIns)
            {
                pPIInfo = gPlugins->GetPlugInInfo( pluginIndex );
				if ( pPIInfo != NULL )
				{
					if (pPIInfo->fState & kActive)
					{
						thePIState = pPIInfo->fState;
						thePIState += (UInt32)kInactive;
						thePIState -= (UInt32)kActive;
						gPlugins->SetState( pPIInfo->fName, thePIState );

						gPluginConfig->SetPluginState( pPIInfo->fName, kInactive);
						gPluginConfig->SaveConfigData();

						SrvrLog( kLogApplication, "Plug-in %s state is now set inactive.", pPIInfo->fName );
					}
					else if (pPIInfo->fState & kInactive)
					{
						thePIState = pPIInfo->fState;
						thePIState -= kInactive;
						thePIState += kActive;
						gPlugins->SetState( pPIInfo->fName, thePIState );

						gPluginConfig->SetPluginState( pPIInfo->fName, kActive);
						gPluginConfig->SaveConfigData();

						SrvrLog( kLogApplication, "Plug-in %s state is now set active.", pPIInfo->fName );
					}
				}
            }
        }
		else
		{
			switch ( aRequest )
			{
				case eDSCustomCallConfigureDestroyAuthRef:
					// destroy the auth ref
					if (authRef != 0)
					{
						AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
						authRef = 0;
					}
					break;
				
				case eDSCustomCallConfigureWriteSCConfigData:
				{
					// write SystemConfiguration
					// for Remote Directory Setup
					bool			success		= false;
					SInt32			xmlDataLength	= 0;
					CFDataRef   	xmlData			= nil;
					CFPropertyListRef propList		= nil;

					//here we accept an XML blob to replace the "/Sets" configuration
					//need to make xmlData large enough to receive the data
					xmlDataLength = (SInt32) bufLen - sizeof( AuthorizationExternalForm );
					if ( xmlDataLength <= 0 ) throw( (SInt32)eDSInvalidBuffFormat );
					
					xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm )),xmlDataLength);
					if ( xmlData == nil ) throw( (SInt32)eMemoryError );
					propList = CFPropertyListCreateFromXMLData(NULL,xmlData,0,NULL);
					if ( propList == nil ) throw( (SInt32)eMemoryError );
				
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
					break;
				}
				
				case eDSCustomCallConfigureToggleDSProxy:
				{
					//toggle whether TCP Listener is active or not
					struct stat		statResult;

					siResult = ::stat( "/Library/Preferences/DirectoryService/.DSTCPListening", &statResult );
					if (siResult != eDSNoErr)
					{
						dsTouch( "/Library/Preferences/DirectoryService/.DSTCPListening" );
						gSrvrCntl->StartTCPListener(kDSDefaultListenPort);
					}
					else
					{
						dsRemove( "/Library/Preferences/DirectoryService/.DSTCPListening" );
						gSrvrCntl->StopTCPListener();
					}
					break;
				}
				
				case eDSCustomCallConfigureIsBSDLocalUsersAndGroupsEnabled:
				{
					// safe to assume that there is always a dictionary present
					CFMutableDictionaryRef	dictionary = gPlugins->CopyRecordTypeRestrictionsDictionary();
					CFDictionaryRef			bsdRestrictions = (CFDictionaryRef) CFDictionaryGetValue( dictionary, CFSTR("BSD") );
					
					// default to not found
					siResult = eDSAttributeNotFound;

					if ( bsdRestrictions != NULL )
					{
						CFDictionaryRef	bsdLocalRestrictions = (CFDictionaryRef) CFDictionaryGetValue( bsdRestrictions, CFSTR("/BSD/local") );
						
						if ( bsdLocalRestrictions != NULL )
						{
							if ( CFDictionaryContainsKey(bsdLocalRestrictions, CFSTR(kRTRDenyKey)) )
							{
								siResult = eDSNoErr;
							}
						}
					}
					
					DSCFRelease( dictionary );
					break;
				}
				
				case eDSCustomCallConfigureEnableBSDLocalUsersAndGroups:
				{
					//safe to assume that there is always a dictionary present
					CFMutableDictionaryRef newDictionary = gPlugins->CopyRecordTypeRestrictionsDictionary();
					
					//we do not take into account any manually editted restrictions for the BSD node
					CFDictionaryRemoveValue(newDictionary, CFSTR("BSD"));
					
					//set the updated dictionary
					gPlugins->SetRecordTypeRestrictionsDictionary( newDictionary );
					
					// should be retained by SetRecordTypeRestrictionsDictionary
					CFRelease( newDictionary );
					break;
				}
				
				case eDSCustomCallConfigureDisableBSDLocalUsersAndGroups:
				{
					//safe to assume that there is always a dictionary present
					CFMutableDictionaryRef newDictionary = gPlugins->CopyRecordTypeRestrictionsDictionary();
					
					//create XML data from the default disable config
					CFStringRef errorString = NULL;
					CFDataRef xmlData = CFDataCreate( nil, (const UInt8 *)kDefaultDisableBSDUsersAndGroups, sizeof(kDefaultDisableBSDUsersAndGroups) );
					CFDictionaryRef aBSDDict = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData, kCFPropertyListImmutable, &errorString);
					DSCFRelease(errorString);
					DSCFRelease(xmlData);
					
					//we do not take into account any manually editted restrictions for the BSD node
					CFDictionarySetValue(newDictionary, CFSTR("BSD"), aBSDDict);
					DSCFRelease(aBSDDict);
					
					//set the updated dictionary
					gPlugins->SetRecordTypeRestrictionsDictionary( newDictionary );
					
					// should be retained by SetRecordTypeRestrictionsDictionary
					CFRelease( newDictionary );
					break;
				}
				
				case eDSCustomCallConfigureLocalMountRecordsChanged:
					gSrvrCntl->SearchPolicyChangedNotify();
					break;
			}
		} // else
    } // try

    catch( SInt32 err )
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
//	* DoDirNodeAuth
//------------------------------------------------------------------------------------

SInt32 CConfigurePlugin::DoDirNodeAuth( sDoDirNodeAuth *inData )
{
    int                     siResult		= eDSAuthFailed;
    sConfigContextData      *pContext		= NULL;
    sConfigContinueData		*pContinue      = NULL;
    char                    *pServicePrinc  = NULL;

    // let's lock kerberos now
    gKerberosMutex->WaitLock();

    // do GSSAPI authentication fInAuthStepData buffer
    //      dsAuthMethodStandard:GSSAPI
    // -------------
    // 4 bytes  = export security context back to calling application
    // 4 bytes  = length of service principal string
    // string   = service principal string
    // 4 bytes  = length of incoming key block
    // r        = incoming key block
    
    // while eDSContinue returned - fInAuthStepData and fOutAuthStepDataResponse buffer
    // -------------
    // 4 bytes  = response length - total fOutAuthStepDataResponse size should always be network block length as precaution (1500)
    // r        = response block
    
    // when eDSNoErr returned - fOutAuthStepDataResponse buffer
    // -------------
    // 4 bytes  = client name length
    // r        = client name string
    // 4 bytes  = service principal used length
    // r        = service principal string used
    // 4 bytes  = response block length
    // r        = response block
    // 4 bytes  = export context block length
    // r        = export context block
    
    // eDSNoErr if successful
    
    try
    {
        char                *pBuffer        = inData->fInAuthStepData->fBufferData;
        UInt32              bufLen          = inData->fInAuthStepData->fBufferLength;
        OM_uint32           minorStatus     = 0;
        OM_uint32           majorStatus     = 0;
        gss_buffer_desc     recvToken       = { 0, NULL };
        
        if ( inData == NULL ) throw( (SInt32) eMemoryError );
        
        pContext = (sConfigContextData *)gConfigNodeRef->GetItemData( inData->fInNodeRef );
        if ( pContext == NULL ) throw( (SInt32) eDSBadContextData );
        
        if ( inData->fInDirNodeAuthOnlyFlag == false ) throw( (SInt32) eDSAuthMethodNotSupported );
        if ( inData->fOutAuthStepDataResponse->fBufferSize < 1500 ) throw ( (SInt32) eDSBufferTooSmall );
        
        if ( inData->fIOContinueData == NULL )
        {
            pContinue = new sConfigContinueData;
            gConfigContinue->AddItem( pContinue, inData->fInNodeRef );
            
            if( strcmp(inData->fInAuthMethod->fBufferData, "dsAuthMethodStandard:GSSAPI") != 0 )
            {
                throw( (SInt32) eDSAuthMethodNotSupported );
            }
            
            // get export context flag
            if( bufLen < 4 ) throw((SInt32)eDSInvalidBuffFormat );
            
            pContinue->exportContext = *((u_int32_t *)pBuffer);
            pBuffer += sizeof(u_int32_t);
            bufLen -= sizeof(u_int32_t);
            
            // get service principal
            if( bufLen < 4 ) throw((SInt32)eDSInvalidBuffFormat );
            
            u_int32_t ulTempLen = *((u_int32_t*)pBuffer);
            pBuffer += sizeof(u_int32_t);
            bufLen -= sizeof(u_int32_t);
            
            if( ulTempLen > bufLen ) throw((SInt32)eDSInvalidBuffFormat );
            
            if( ulTempLen )
            {
                pServicePrinc = (char *)calloc( 1, ulTempLen + 1 );
                bcopy( pBuffer, pServicePrinc, ulTempLen );
                
                pBuffer += ulTempLen;
                bufLen -= ulTempLen;
            }
            
            // get keyblock
            if( bufLen < 4 ) throw((SInt32)eDSInvalidBuffFormat );
            
            ulTempLen = *((UInt32*)pBuffer);
            pBuffer += sizeof(SInt32);
            bufLen -= sizeof(SInt32);
            
            if( ulTempLen > bufLen ) throw((SInt32)eDSInvalidBuffFormat );
            
            recvToken.value = pBuffer;
            recvToken.length = ulTempLen;
            
            pBuffer += ulTempLen;
            bufLen -= ulTempLen;
            
            // if we supply a principal then let's preflight the principal
            if( pServicePrinc && strlen(pServicePrinc) )
            {
                // we need to acquire our credentials, etc.
                gss_buffer_desc credentialsDesc;
                
                credentialsDesc.value = pServicePrinc;
                credentialsDesc.length = strlen( pServicePrinc );
                
                // put the name in the context information
                majorStatus = gss_import_name( &minorStatus, &credentialsDesc, (gss_OID) GSS_KRB5_NT_PRINCIPAL_NAME, &pContinue->gssServicePrincipal );
                if( majorStatus != GSS_S_COMPLETE ) throw ( (SInt32) eDSUnknownHost );
                
                // let's get credentials at this point
                majorStatus = gss_acquire_cred( &minorStatus, pContinue->gssServicePrincipal, 0, GSS_C_NULL_OID_SET, GSS_C_ACCEPT, &pContinue->gssCredentials, NULL, NULL );
                if( majorStatus != GSS_S_COMPLETE ) throw ( (SInt32) eDSUnknownHost );
            }
        }
        else
        {
            pContinue = (sConfigContinueData *)inData->fIOContinueData;
            if ( gConfigContinue->VerifyItem( pContinue ) == false ) throw( (SInt32)eDSAuthContinueDataBad );
            
            recvToken.value = inData->fInAuthStepData->fBufferData;
            recvToken.length = inData->fInAuthStepData->fBufferLength;
        }
        
        inData->fIOContinueData = NULL; // NULL out the continue data we'll set it later if necessary
        
        if( pContinue->gssFinished == false )
        {
            gss_buffer_desc sendToken;
            
            majorStatus = gss_accept_sec_context( &minorStatus, &pContinue->gssContext, pContinue->gssCredentials, &recvToken, GSS_C_NO_CHANNEL_BINDINGS, &pContinue->gssClientPrincipal, NULL, &sendToken, NULL, NULL, NULL );
            
            // if we don't have a continue or a complete we failed.. let's debug log it
            if( majorStatus != GSS_S_CONTINUE_NEEDED && majorStatus != GSS_S_COMPLETE ) 
            {
                OM_uint32 statusList[] = { majorStatus, minorStatus };
                OM_uint32 mechType[] = { GSS_C_GSS_CODE, GSS_C_MECH_CODE };
                char *statusString[] = { "Major", "Minor" };
                
                for( int ii = 0; ii < 2; ii++ )
                {
                    OM_uint32 msg_context = 0;
                    OM_uint32 min_status = 0;
                    gss_buffer_desc errBuf;
                    
                    // loop until context != 0 and we don't have an error..
                    msg_context = 0;
                    do
                    {
                        // if we fail for some reason, we should break out..
                        if( gss_display_status(&min_status, statusList[ii], mechType[ii], GSS_C_NULL_OID, &msg_context, &errBuf) == GSS_S_COMPLETE )
                        {
                            DbgLog( kLogPlugin, "CConfigure: dsDoDirNodeAuth GSS %s Status error - %s", statusString[ii], errBuf.value );
                            gss_release_buffer( &min_status, &errBuf );	
                        }
                        else
                        {
                            break;
                        }
                    } while( msg_context != 0 );
                }
                
                // throw out of here..
                throw( (SInt32) eDSAuthFailed );
            }
            
            // let's reset our out step data for now..
            pBuffer = inData->fOutAuthStepDataResponse->fBufferData;
            inData->fOutAuthStepDataResponse->fBufferLength = 0;
            
            // if this is a continue, then let's send the response back.. if they didn't expect a continue, then they can fail..
            if( majorStatus == GSS_S_CONTINUE_NEEDED )
            {
                if( sendToken.length && sendToken.value )
                {
                    *((u_int32_t*)pBuffer) = sendToken.length;
                    pBuffer += sizeof( u_int32_t );
                    
                    bcopy( sendToken.value, pBuffer, sendToken.length );
                    pBuffer += sendToken.length;
                }
                
                gss_release_buffer( &minorStatus, &sendToken );
                
                // get buffer length by subtracting where we ended up
                inData->fOutAuthStepDataResponse->fBufferLength = pBuffer - inData->fOutAuthStepDataResponse->fBufferData;
                
                inData->fIOContinueData = pContinue;
                siResult = eDSContinue;
            }
            else // this is GSS_S_COMPLETE
            {
                bool    bExportContext = pContinue->exportContext;
                
                // second let's put the client's name in the buffer
                gss_buffer_desc nameToken = GSS_C_EMPTY_BUFFER;
                
                majorStatus = gss_display_name( &minorStatus, pContinue->gssClientPrincipal, &nameToken, NULL );
                
                if( majorStatus == GSS_S_COMPLETE ) 
                {
                    *((u_int32_t*)pBuffer) = nameToken.length;
                    pBuffer += sizeof( u_int32_t );

                    bcopy( nameToken.value, pBuffer, nameToken.length );
                    pBuffer += nameToken.length;

                    gss_release_buffer( &minorStatus, &nameToken );
                }
                else
                {
                    *((u_int32_t *) pBuffer) = 0;
                    pBuffer += sizeof( u_int32_t );
                }
                
                // export the credentials that were used to make the connection (i.e., http/server@REALM, server@REALM, etc.)
                gss_name_t servicePrincipal = GSS_C_NO_NAME;
                
                majorStatus = gss_inquire_context( &minorStatus, pContinue->gssContext, NULL, &servicePrincipal, NULL, NULL, NULL, NULL, NULL );
                if( majorStatus == GSS_S_COMPLETE )
                {
                    gss_buffer_desc name2Token = GSS_C_EMPTY_BUFFER;
                    
                    majorStatus = gss_display_name( &minorStatus, servicePrincipal, &name2Token, NULL );
                    
                    if( majorStatus == GSS_S_COMPLETE ) 
                    {
                        *((u_int32_t*)pBuffer) = name2Token.length;
                        pBuffer += sizeof( u_int32_t );

                        bcopy( name2Token.value, pBuffer, name2Token.length );
                        pBuffer += name2Token.length;
                        
                        gss_release_buffer( &minorStatus, &name2Token );
                    }
                    else
                    {
                        *((u_int32_t *) pBuffer) = 0;
                        pBuffer += sizeof( u_int32_t );
                    }
                }
                else
                {
                    *((u_int32_t *) pBuffer) = 0;
                    pBuffer += sizeof( u_int32_t );
                }
                
                // let's put any token that needs to be sent into the buffer
                if( sendToken.length && sendToken.value )
                {
                    *((u_int32_t*)pBuffer) = sendToken.length;
                    pBuffer += sizeof( u_int32_t );
                    
                    bcopy( sendToken.value, pBuffer, sendToken.length );
                    pBuffer += sendToken.length;
                }
                else
                {
                    *((u_int32_t*)pBuffer) = 0;
                    pBuffer += sizeof( u_int32_t );
                }
                gss_release_buffer( &minorStatus, &sendToken );
                
                // if export context requested.. let's stuff it in the buffer..
                if( bExportContext )
                {
                    gss_buffer_desc contextToken;
                    
                    majorStatus = gss_export_sec_context( &minorStatus, &pContinue->gssContext, &contextToken );
                    if( majorStatus == GSS_S_COMPLETE )
                    {
                        *((u_int32_t*)pBuffer) = contextToken.length;
                        pBuffer += sizeof( u_int32_t );

                        bcopy( contextToken.value, pBuffer, contextToken.length );
                        pBuffer += contextToken.length + sizeof(contextToken.length);
                        
                        gss_release_buffer( &minorStatus, &contextToken );
                    }
                    else
                    {
                        bExportContext = false; // set to false cause we failed... so we can zero the buffer
                    }
                }
				
                if( bExportContext == false )
                {
                    *((u_int32_t *) pBuffer) = 0;
                    pBuffer += sizeof( u_int32_t );
                }
                
                // get buffer length by subtracting where we ended up
                inData->fOutAuthStepDataResponse->fBufferLength = pBuffer - inData->fOutAuthStepDataResponse->fBufferData;
                
                pContinue->gssFinished = true;
                siResult = eDSNoErr;
            }
        }
        
    } catch( SInt32 error ) {
        siResult = error;
    } catch( ... ) {
        siResult = eUndefinedError;
    }
    
    DSFreeString( pServicePrinc );
    
    if ( (inData->fIOContinueData == NULL) && (pContinue != NULL) )
    {
        // we've decided not to return continue data, so we should clean up
        gConfigContinue->RemoveItem( pContinue );
        pContinue = nil;
    }
    
    gKerberosMutex->SignalLock();
    
    return siResult;
} // DoDirNodeAuth


//------------------------------------------------------------------------------------
//	* CloseAttributeList
//------------------------------------------------------------------------------------

SInt32 CConfigurePlugin::CloseAttributeList ( sCloseAttributeList *inData )
{
	SInt32				siResult		= eDSNoErr;
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

SInt32 CConfigurePlugin::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
	SInt32				siResult		= eDSNoErr;
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
		delete pContinue;
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
