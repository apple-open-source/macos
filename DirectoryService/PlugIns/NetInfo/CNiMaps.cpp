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

#include "CNiMaps.h"
#include "CLog.h"

//re-ordered more closely in terms of frequency of use
//TODO KW put this in a CFDictionary or STL map class for keyed access
//NOTE: Updating this list likely requires a corressponding update in DSAgent
static const char *sAttrMap[  ][ 2 ] =
{
				{ kDSNAttrRecordName,			"name" },
				{ kDS1AttrDistinguishedName,	"realname" },
				{ kDS1AttrPassword,				"passwd" },
				{ kDS1AttrPasswordPlus,			"passwd" },  //forward mapping only as reverse mapping uses entry above
				{ kDS1AttrUniqueID,				"uid" },
				{ kDS1AttrPrimaryGroupID,		"gid" },
				{ kDSNAttrGroupMembers,			"groupmembers" },
				{ kDSNAttrNestedGroups,			"nestedgroups" },
				{ kDS1AttrUserShell,			"shell" },
				{ kDS1AttrNFSHomeDirectory,		"home" },
				{ kDSNAttrAuthenticationAuthority, "authentication_authority" },
				{ kDSNAttrHomeDirectory,		"home_loc" }, // or HomeLocation
				{ kDS1AttrHomeLocOwner,			"home_loc_owner" },
				{ kDS1AttrCreationTimestamp,	"creationtimestamp" },
				{ kDS1AttrModificationTimestamp,"modificationtimestamp" },
				{ kDS1AttrTimeToLive,			"timetolive" },
				{ kDS1AttrHomeDirectoryQuota,	"homedirectoryquota" },
				{ kDS1AttrHomeDirectorySoftQuota, "homedirectorysoftquota" },
				{ kDS1AttrPicture,				"picture" },
				{ kDS1AttrAuthenticationHint,	"hint" },
				{ kDS1AttrRARA,					"RARA" },
				{ kDS1AttrGeneratedUID,			"generateduid" },
				{ kDS1AttrRealUserID,			"ruid" },
				{ kDSNAttrGroupMembership,		"users" },
				{ kDSNAttrEMailAddress,			"mail" },
				{ kDS1AttrAlternateDatastoreLocation, "alternatedatastorelocation" },
				{ kDSNAttrURL,					"URL" },
				{ kDSNAttrURLForNSL,			"URL" },
				{ kDSNAttrMIME,					"mime" },
				{ kDSNAttrHTML,					"htmldata" },
				{ kDSNAttrIPAddress,			"ip_address" },
				{ kDS1AttrENetAddress,			"en_address" },
				{ kDS1AttrContactPerson,		"owner" },
				{ kDSNAttrMachineServes,		"serves" },
				{ kDSNAttrKeywords,				"keywords" },
				{ kDSNAttrComputers,			"computers" },
				{ kDS1AttrMCXFlags,				"mcx_flags" },
				{ kDS1AttrMCXSettings,			"mcx_settings" },
				{ kDSNAttrMCXSettings,			"mcx_settings" },
				{ kDS1AttrCopyTimestamp,		"copy_timestamp" },
				{ kDS1AttrOriginalNodeName,		"original_node_name" },
				{ kDS1AttrOriginalNFSHomeDirectory,	"original_home" },
				{ kDSNAttrOriginalHomeDirectory,"original_home_loc" },
				{ kDS1AttrPasswordPolicyOptions,"passwordpolicyoptions" },
				{ kDS1AttrPasswordServerList,	"passwordserverlist" },
				{ kDS1AttrXMLPlist,				"XMLPlist" },
				{ kDS1AttrDateRecordCreated,	"dateCreate" },
				{ kDS1AttrDataStamp,			"data_stamp" },
				{ kDS1AttrPrintServiceInfoText,	"PrintServiceInfoText" },
				{ kDS1AttrPrintServiceInfoXML,	"PrintServiceInfoXML" },
				{ kDS1AttrPrintServiceUserData,	"appleprintservice" },
				{ kDS1AttrNote,					"note" },
				{ kDS1AttrPrinterLPRHost,		"rm" },
				{ kDS1AttrPrinterLPRQueue,		"rp" },
				{ kDS1AttrPrinterType,			"ty" },
				{ kDS1AttrPrinter1284DeviceID,  "1284deviceid" },
				{ kDS1AttrPrinterMakeAndModel,  "makeandmodel" },
				{ kDS1AttrPrinterURI,			"uri" },
				{ kDSNAttrPrinterXRISupported,  "xrisupported" },
				{ kDS1AttrVFSType,				"vfstype" },
				{ kDS1AttrVFSPassNo,			"passno" },
				{ kDS1AttrVFSDumpFreq,			"dump_freq" },
				{ kDS1AttrVFSLinkDir,			"dir" },
				{ kDSNAttrVFSOpts,				"opts" },
				{ kDS1AttrAliasData,			"alias_data" },
				{ kDSNAttrPhoneNumber,			"phonenumber" },
				{ kDSNAttrMember,				"users" },
				{ kDSNAttrAllNames,				"dsAttrTypeStandard:AllNames" },
				{ kDSNAttrMetaNodeLocation,		"dsAttrTypeStandard:AppleMetaNodeLocation" },
				{ kDSNAttrRecordType,			"dsAttrTypeStandard:RecordType" },
				{ kDS1AttrComment,				"comment" },
				{ kDS1AttrChange,				"change" },
				{ kDS1AttrExpire,				"expire" },
				{ kDSNAttrGroup,				"groups" },
				{ kDS1AttrFirstName,			"firstname" },
				{ kDS1AttrMiddleName,			"middlename" },
				{ kDS1AttrLastName,				"lastname" },
				{ kDSNAttrAreaCode ,			"areacode" },
				{ kDSNAttrAddressLine1,			"address1" },
				{ kDSNAttrAddressLine2,			"address2" },
				{ kDSNAttrAddressLine3,			"address3" },
				{ kDSNAttrCity,					"city" },
				{ kDSNAttrState,				"state" },
				{ kDSNAttrPostalCode,			"zip" },
				{ kDSNAttrOrganizationName,		"orgname" },
				{ kDS1AttrSetupOccupation,		"occupation" },
				{ kDSNAttrFaxNumber,			"faxnumber" },
				{ kDSNAttrMobileNumber,			"mobilenumber" },
				{ kDSNAttrPagerNumber,			"pagernumber" },
				{ kDSNAttrPostalAddress,		"postaladdress" },
				{ kDSNAttrStreet,				"street" },
				{ kDSNAttrDepartment,			"department" },
				{ kDSNAttrNickName,				"nickname" },
				{ kDSNAttrJobTitle,				"jobtitle" },
				{ kDSNAttrIMHandle,				"imhandle" },
				{ kDSNAttrBuilding,				"building" },
				{ kDSNAttrCountry,				"country" },
				{ kDSNAttrNamePrefix,			"nameprefix" },
				{ kDSNAttrNameSuffix,			"namesuffix" },
				{ kDS1AttrSetupLocation,		"location" },
				{ kDS1AttrSetupAdvertising,		"spam" },
				{ kDS1AttrSetupAutoRegister,	"autoregister" },
				{ kDSNAttrKDCAuthKey,			"kdcauthkey" },
				{ kDS1AttrKDCConfigData,		"kdcconfigdata" },
				{ kDS1AttrPresetUserIsAdmin,	"preset_user_is_admin" },
				{ kDS1AttrPasswordServerLocation, "passwordserverlocation" },
				{ kDS1AttrBootFile,				"bootfile" },
				{ kDSNAttrBootParams,			"bootparams" },
				{ kDSNAttrNetGroups,			"netgroups" },
				{ kDS1AttrSMBRID,				"smb_rid" },
				{ kDS1AttrSMBGroupRID,			"smb_group_rid" },
				{ kDS1AttrSMBHomeDrive,			"smb_home_drive" },
				{ kDS1AttrSMBHome,				"smb_home" },
				{ kDS1AttrSMBScriptPath,		"smb_script_path" },
				{ kDS1AttrSMBProfilePath,		"smb_profile_path" },
				{ kDS1AttrSMBUserWorkstations,	"smb_user_workstations" },
				{ kDS1AttrSMBAcctFlags,			"smb_acctFlags" },
				{ kDS1AttrSMBPWDLastSet,		"smb_pwd_last_set" },
				{ kDS1AttrSMBLogonTime,			"smb_logon_time" },
				{ kDS1AttrSMBLogoffTime,		"smb_logoff_time" },
				{ kDS1AttrSMBKickoffTime,		"smb_kickoff_time" },
				{ kDS1AttrSMBSID,				"smb_sid" },
				{ kDS1AttrSMBPrimaryGroupSID,   "smb_primary_group_sid" },
				{ kDS1AttrDNSDomain,			"domain" },
				{ kDS1AttrDNSNameServer,		"nameserver" },
				{ "dsAttrTypeStandard:NIAutoSwitchToLDAP",	"niautoswitchtoldap" },
				{ kDS1AttrLocation,				"location" },
				{ kDS1AttrPort,					"port" },
				{ kDS1AttrServiceType,			"servicetype" },
				{ kDSNAttrPGPPublicKey,			"pgppublickey" },
				{ kDS1AttrInternetAlias,		"InetAlias" },
				{ kDS1AttrMailAttribute,		"applemail" },
				{ kDSNAttrNBPEntry,				"NBPEntry" },
				{ kDSNAttrDNSName,				"dnsname" },
				{ kDS1AttrAdminStatus,			"AdminStatus" },
				{ kDS1AttrAdminLimits,			"admin_limits" },
				{ kDS1AttrCapabilities,			"capabilities" },
				{ kDSNAttrProtocols,			"protocols" },
				{ kDS1AttrPwdAgingPolicy,		"PwdAgingPolicy" },
				{ kDS1AttrNeighborhoodType,		"neighborhoodtype" },
				{ kDSNAttrNeighborhoodAlias,	"neighborhoodalias" },
				{ kDS1AttrNetworkView,			"networkview" },
				{ kDS1AttrCategory,				"category" },
				{ kDSNAttrComputerAlias,		"computeralias" },
				{ kDSNAttrNodePathXMLPlist,		"nodepathxmlplist" },
				{ kDS1AttrWeblogURI,			"webloguri" },
				{ NULL,							NULL }
};


static const char *sRecMap[  ][ 2 ] =
{
				{ kDSStdRecordTypeUsers,			"users" },
				{ kDSStdRecordTypeGroups,			"groups" },
				{ kDSStdRecordTypeMachines,			"machines" },
				{ kDSStdRecordTypeComputers,		"computers" },	
				{ kDSStdRecordTypeComputerLists,	"computer_lists" },
				{ kDSStdRecordTypePrinters,			"printers" },
				{ kDSStdRecordTypeHosts,			"machines" },
				{ kDSStdRecordTypeNeighborhoods,	"neighborhoods" },
				{ kDSStdRecordTypeAliases,			"aliases" },
				{ kDSStdRecordTypeNetworks,			"networks" },
				{ kDSStdRecordTypeServer,			"servers" },
				{ kDSStdRecordTypePasswordServer,	"passwordservers" },
				{ kDSStdRecordTypeWebServer,   		"httpservers" },
				{ kDSStdRecordTypeFTPServer,		"ftpservers" },
				{ kDSStdRecordTypeAFPServer,		"afpservers" },
				{ kDSStdRecordTypeLDAPServer,		"ldapservers" },
				{ kDSStdRecordTypeSMBServer,		"smbservers" },
				{ kDSStdRecordTypeQTSServer,		"qtsservers" },
				{ kDSStdRecordTypeNFS,				"mounts" },
				{ kDSStdRecordTypeServices,			"services" },
				{ kDSStdRecordTypePrintService,		"PrintService" },
				{ kDSStdRecordTypeConfig,			"config" },
				{ kDSStdUserNamesMeta,				"dsRecTypeStandard:MetaUserNames" }, //?
				{ kDSStdRecordTypeMeta,				"dsRecTypeStandard:AppleMetaRecord" },
				{ "dsRecTypeStandard:UsreNames",	"users" }, //?
				{ kDSStdRecordTypeAFPUserAliases,	"afpuser_aliases" },
				{ kDSStdRecordTypeMounts,			"mounts" },
				{ kDSStdRecordTypePrintServiceUser,	"printserviceusers" },
				{ kDSStdRecordTypePresetUsers,		"presets_users" },
				{ kDSStdRecordTypePresetGroups,		"presets_groups" },
				{ kDSStdRecordTypePresetComputerLists,	"presets_computer_lists" },
				{ kDSStdRecordTypeHosts,			"hosts" },
				{ kDSStdRecordTypeProtocols,		"protocols" },
				{ kDSStdRecordTypeRPC,				"rpcs" },
				{ kDSStdRecordTypeBootp,			"bootp" },
				{ kDSStdRecordTypeNetDomains,		"netdomains" },
				{ kDSStdRecordTypeEthernets,		"ethernets" },
				{ kDSStdRecordTypeNetGroups,		"netgroups" },
				{ kDSStdRecordTypeHostServices,		"hostservices" },
				{ kDSStdRecordTypeAutoServerSetup,	"autoserversetup" },
				{ kDSStdRecordTypeLocations,		"locations" },
				{ kDSStdRecordTypeSharePoints,		"config/SharePoints" },
				{ kDSStdRecordTypePeople,			"people" },
				{ NULL,								NULL }
};

// ---------------------------------------------------------------------------
//	* MapAttrToNetInfoType
// ---------------------------------------------------------------------------

char* MapAttrToNetInfoType ( const char *inAttrType )
{
	char	   *outResult	= nil;
	uInt32		uiStrLen	= 0;
	uInt32		uiNativeLen	= sizeof( kDSNativeAttrTypePrefix ) - 1;
	uInt32		uiStdLen	= sizeof( kDSStdAttrTypePrefix ) - 1;
	
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
				DBGLOG1( kLogPlugin, "MapAttrToNetInfoType:: Warning:Native attribute type <%s> is being used", outResult );
			}
		}
		else if ( ::strncmp( inAttrType, kDSStdAttrTypePrefix, uiStdLen ) == 0 )
		{
			for ( int i = 0; sAttrMap[ i ][ 0 ] != NULL; i++ )
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
			outResult = new char[ sizeof( kDSAttributesAll ) ];
			::strcpy( outResult, kDSAttributesAll );
		}
		else if ( ::strcmp( inAttrType, kDSAttributesStandardAll ) == 0 )
		{
			outResult = new char[ sizeof( kDSAttributesStandardAll ) ];
			::strcpy( outResult, kDSAttributesStandardAll );
		}
	}

	return( outResult );

} // MapAttrToNetInfoType


// ---------------------------------------------------------------------------
//	* MapNetInfoAttrToDSType
// ---------------------------------------------------------------------------

char* MapNetInfoAttrToDSType ( const char *inAttrType )
{
	char	   *outResult	= nil;

	if ( inAttrType != nil )
	{
		// Look for a standard type
		for ( int i = 0; sAttrMap[ i ][ 1 ] != NULL; i++ )
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
			outResult = new char[ sizeof(kDSNativeAttrTypePrefix) + ::strlen(inAttrType) ];
			::strcpy( outResult, kDSNativeAttrTypePrefix );
			::strcat( outResult, inAttrType );
		}
	}

	return( outResult );

} // MapNetInfoAttrToDSType


// ---------------------------------------------------------------------------
//	* MapRecToNetInfoType
// ---------------------------------------------------------------------------

char* MapRecToNetInfoType ( const char *inRecType )
{
	char	   *outResult	= nil;
	uInt32		uiStrLen	= 0;
	uInt32		uiNativeLen	= sizeof( kDSNativeRecordTypePrefix ) - 1;
	uInt32		uiStdLen	= sizeof( kDSStdRecordTypePrefix ) - 1;

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
				DBGLOG1( kLogPlugin, "MapRecToNetInfoType:: Warning:Native record type <%s> is being used", outResult );
			}
		}
		else if ( ::strncmp( inRecType, kDSStdRecordTypePrefix, uiStdLen ) == 0 )
		{
			for ( int i = 0; sRecMap[ i ][ 0 ] != NULL; i++ )
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

char* MapNetInfoRecToDSType ( const char *inRecType )
{
	char	   *outResult	= nil;

	if ( inRecType != nil )
	{
		// Look for a standard type
		for ( int i = 0; sRecMap[ i ][ 1 ] != NULL; i++ )
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
			outResult = new char[ sizeof(kDSNativeRecordTypePrefix) + ::strlen( inRecType ) ];
			::strcpy( outResult, kDSNativeRecordTypePrefix );
			::strcat( outResult, inRecType );
		}
	}

	return( outResult );

} // MapNetInfoRecToDSType


// ---------------------------------------------------------------------------
//	* MapNetInfoErrors
//
//		- xxxx make sure i do the right thing with the errors
// ---------------------------------------------------------------------------

sInt32 MapNetInfoErrors ( sInt32 inNiError )
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

