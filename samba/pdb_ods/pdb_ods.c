/* 
   Unix SMB/CIFS implementation.
   passdb opendirectory backend

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

//#ifdef WITH_ODS_SAM
#define USE_SETATTRIBUTEVALUE 1

static int odssam_debug_level = DBGC_ALL;

#undef DBGC_CLASS
#define DBGC_CLASS odssam_debug_level

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesUtils.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libopendirectorycommon.h>

struct odssam_privates {
    tDirReference	dirRef;
    tDirNodeReference	searchNodeRef;
    tDirNodeReference	localNodeRef;
    const char *odssam_location;
	/* saved state from last search */
	CFMutableArrayRef usersArray;
	int usersIndex;
	tContextData contextData;
	tContextData localContextData;
	tDataListPtr samAttributes;
	smb_event_id_t event_id;
};

#ifndef kDS1AttrSMBRID
#define kDS1AttrSMBRID				"dsAttrTypeNative:smb_rid"
#define kDS1AttrSMBGroupRID			"dsAttrTypeNative:smb_group_rid"
#define kDS1AttrSMBPWDLastSet		"dsAttrTypeNative:smb_pwd_last_set"
#define kDS1AttrSMBLogonTime		"dsAttrTypeNative:smb_logon_time"
#define kDS1AttrSMBLogoffTime		"dsAttrTypeNative:smb_logoff_time"
#define kDS1AttrSMBKickoffTime		"dsAttrTypeNative:smb_kickoff_time"
#define kDS1AttrSMBHomeDrive		"dsAttrTypeNative:smb_home_drive"
#define kDS1AttrSMBHome				"dsAttrTypeNative:smb_home"
#define kDS1AttrSMBScriptPath		"dsAttrTypeNative:smb_script_path"
#define kDS1AttrSMBProfilePath		"dsAttrTypeNative:smb_profile_path"
#define kDS1AttrSMBUserWorkstations	"dsAttrTypeNative:smb_user_workstations"
#define kDS1AttrSMBAcctFlags		"dsAttrTypeNative:smb_acctFlags"
#endif

#define kDS1AttrSMBLMPassword		"dsAttrTypeNative:smb_lmPassword"
#define kDS1AttrSMBNTPassword		"dsAttrTypeNative:smb_ntPassword"
#define kDS1AttrSMBPWDCanChange   	"dsAttrTypeNative:smb_pwd_can_change"
#define kDS1AttrSMBPWDMustChange	"dsAttrTypeNative:smb_pwd_must_change"

#define kPlainTextPassword			"plaintextpassword"

/* Password Policy Attributes */
#define kPWSIsDisabled				"isDisabled"
#define kPWSIsAdmin					"isAdminUser"
#define kPWSNewPasswordRequired		"newPasswordRequired"
#define kPWSIsUsingHistory			"usingHistory"
#define kPWSCanChangePassword		"canModifyPasswordforSelf"
#define kPWSExpiryDateEnabled		"usingHardExpirationDate"
#define kPWSRequiresAlpha			"requiresAlpha"
#define kPWSExpiryDate				"expirationDateGMT"
#define kPWSHardExpiryDate			"hardExpireDateGMT"
#define kPWSMaxMinChgPwd			"maxMinutesUntilChangePassword"
#define kPWSMaxMinActive			"maxMinutesUntilDisabled"
#define kPWSMaxMinInactive			"maxMinutesOfNonUse"
#define kPWSMaxFailedLogins			"maxFailedLoginAttempts"
#define kPWSMinChars				"minChars"
#define kPWSMaxChars				"maxChars"
#define kPWSPWDCannotBeName			"passwordCannotBeName"

#define kPWSPWDLastSetTime			"passwordLastSetTime"
#define kPWSLastLoginTime			"lastLoginTime"
#define kPWSLogOffTime				"logOffTime"
#define kPWSKickOffTime				"kickOffTime"

/* Open Directory Service Utilities */
tDirStatus delete_data_list(struct odssam_privates *ods_state,tDataListPtr dataList)
{
    tDirStatus status =	eDSNoErr;
    if (dataList != NULL) {
		status = dsDataListDeallocate(ods_state->dirRef, dataList);
		free(dataList);
    }
	return status;
}

tDirStatus delete_data_node(struct odssam_privates *ods_state,tDataNodePtr dataNode)
{
    tDirStatus status =	eDSNoErr;
    if (dataNode != NULL)
		status = dsDataNodeDeAllocate(ods_state->dirRef, dataNode);
    return status;
}

tDirStatus delete_data_buffer(struct odssam_privates *ods_state, tDataBufferPtr dataBuffer)
{
    tDirStatus status =	eDSNoErr;
    if (dataBuffer != NULL)
        status = dsDataBufferDeAllocate(ods_state->dirRef, dataBuffer);
	return status;
}

u_int32_t add_data_buffer_item(tDataBufferPtr dataBuffer, u_int32_t len, void *buffer)
{
	u_int32_t result = 0;
	
	memcpy( &(dataBuffer->fBufferData[ dataBuffer->fBufferLength ]), &len, 4);
	dataBuffer->fBufferLength += 4;
	if (len != 0) {
		memcpy( &(dataBuffer->fBufferData[ dataBuffer->fBufferLength ]), buffer, len);
		dataBuffer->fBufferLength += len;
	}
	
	return result;	
}

bool GetCString(CFStringRef cfstr, char *outbuffer, int size)
{
    bool isString = false;
    
    if (cfstr)
        isString = CFStringGetCString(cfstr, outbuffer, size, kCFStringEncodingUTF8);
    
    return isString;
}
bool is_machine_name(const char *name)
{
	bool result = false;
	
    if (name) {
	 result = (name[strlen (name) -1] == '$');
	}
	
	return result;
}
char *get_record_type(const char *name)
{
	if (is_machine_name(name)) {
		return kDSStdRecordTypeComputers;			
	} else {
		return kDSStdRecordTypeUsers;
	}
}

static tDirStatus odssam_open(struct odssam_privates *ods_state)
{
    tDirStatus dirStatus = eDSNoErr;

    if (ods_state->dirRef != NULL) {
    	dirStatus = dsVerifyDirRefNum(ods_state->dirRef);
    	if (dirStatus == eDSNoErr) {
    		return eDSNoErr;
    	} else {
    		ods_state->dirRef = NULL;
    	}
	}

    dirStatus = dsOpenDirService(&ods_state->dirRef);
    DEBUG(5,("odssam_open: [%d]dsOpenDirService error [%ld]\n",dirStatus, ods_state->dirRef));		
	ods_state->samAttributes = dsBuildListFromStrings(ods_state->dirRef,
										kDSNAttrRecordName, 
										kDSNAttrMetaNodeLocation,
										kDSNAttrAuthenticationAuthority,
										kDS1AttrUniqueID,
										kDS1AttrPrimaryGroupID,
										kDS1AttrNFSHomeDirectory,
										kDS1AttrDistinguishedName,
										kDS1AttrComment,
																				
										kDS1AttrSMBRID,
										kDS1AttrSMBGroupRID,
										kDS1AttrSMBPWDLastSet,
										kDS1AttrSMBLogonTime,
										kDS1AttrSMBLogoffTime,
										kDS1AttrSMBKickoffTime,
										//kDS1AttrSMBPWDCanChange,
										//kDS1AttrSMBPWDMustChange,
										kDS1AttrSMBHome,
										kDS1AttrSMBHomeDrive,
										kDS1AttrSMBScriptPath,
										kDS1AttrSMBProfilePath,
										kDS1AttrSMBUserWorkstations,
										kDS1AttrSMBLMPassword,
										kDS1AttrSMBNTPassword,
										kDS1AttrSMBAcctFlags,
										
										NULL);
    return dirStatus;
}

static tDirStatus odssam_close(struct odssam_privates *ods_state)
{
    tDirStatus dirStatus = eDSNoErr;

    if (ods_state->dirRef != NULL) {
		dirStatus = dsCloseDirService(ods_state->dirRef);
		if (dirStatus == eDSNoErr)
        	DEBUG(0,("odssam_close: [%d]dsCloseDirService error\n",dirStatus));		
    	DEBUG(5,("odssam_close: [%d] dirRef [%ld]\n",dirStatus, ods_state->dirRef));		
		ods_state->dirRef = NULL;
    }
    return dirStatus;
}

static tDirStatus odssam_open_search_node(struct odssam_privates *ods_state)
{
    tDirStatus			status			= eDSInvalidReference;
    long			bufferSize		= 1024 * 10;
    long			returnCount		= 0;
    tDataBufferPtr		nodeBuffer		= NULL;
    tDataListPtr		searchNodeName		= NULL;

    if (status != eDSNoErr)
        status = odssam_open(ods_state);
    if (status != eDSNoErr) goto cleanup;

    if (ods_state->searchNodeRef == NULL) {
        nodeBuffer = dsDataBufferAllocate(ods_state->dirRef, bufferSize);
        if (nodeBuffer == NULL) goto cleanup;
        status = dsFindDirNodes(ods_state->dirRef, nodeBuffer, NULL, eDSSearchNodeName, &returnCount, NULL);
        if ((status != eDSNoErr) || (returnCount <= 0)) goto cleanup;
    
        status = dsGetDirNodeName(ods_state->dirRef, nodeBuffer, 1, &searchNodeName);
        if (status != eDSNoErr) goto cleanup;
        status = dsOpenDirNode(ods_state->dirRef, searchNodeName, &(ods_state->searchNodeRef));
        if (status != eDSNoErr) goto cleanup;
    } else {
    	status = eDSNoErr;
    }
    
 cleanup:
    delete_data_buffer(ods_state, nodeBuffer);
    delete_data_list(ods_state, searchNodeName);

    return status;
}
static tDirStatus odssam_close_search_node(struct odssam_privates *ods_state)
{
    tDirStatus dirStatus = eDSNoErr;
    if (ods_state->searchNodeRef != NULL) {
		dirStatus = dsCloseDirNode(ods_state->searchNodeRef);
		if (dirStatus == eDSNoErr)
			DEBUG(0,("odssam_close_search_node: [%d]dsCloseDirNode error\n",dirStatus));		
		 ods_state->searchNodeRef = NULL;
    }
    return dirStatus;
}

#if 0
static tDirStatus odssam_open_local_node(struct odssam_privates *ods_state)
{
    tDirStatus			status			= eDSInvalidReference;
    long			bufferSize		= 1024 * 10;
    long			returnCount		= 0;
    tDataBufferPtr		nodeBuffer		= NULL;
    tDataListPtr		localNodeName		= NULL;

    if (status != eDSNoErr)
        status = odssam_open(ods_state);
    if (status != eDSNoErr) goto cleanup;

    if (ods_state->localNodeRef == NULL) {
        nodeBuffer = dsDataBufferAllocate(ods_state->dirRef, bufferSize);
        if (nodeBuffer == NULL) goto cleanup;
        status = dsFindDirNodes(ods_state->dirRef, nodeBuffer, NULL, eDSLocalNodeNames, &returnCount, NULL);
        if ((status != eDSNoErr) || (returnCount <= 0)) goto cleanup;
    
        status = dsGetDirNodeName(ods_state->dirRef, nodeBuffer, 1, &localNodeName);
        if (status != eDSNoErr) goto cleanup;
        status = dsOpenDirNode(ods_state->dirRef, localNodeName, &(ods_state->localNodeRef));
        if (status != eDSNoErr) goto cleanup;
    } else {
    	status = eDSNoErr;
    }
    
 cleanup:
    delete_data_buffer(ods_state, nodeBuffer);
    delete_data_list(ods_state, localNodeName);

    return status;
}
#endif
static tDirStatus odssam_open_node(struct odssam_privates *ods_state, const char* nodeName, tDirNodeReference *nodeReference)
{
    tDirStatus			status			= eDSInvalidReference;
    tDataListPtr		node		= NULL;

    if (status != eDSNoErr)
        status = odssam_open(ods_state);
    if (status != eDSNoErr) goto cleanup;

    if (nodeReference != NULL) {
		node = dsBuildFromPath(ods_state->dirRef, nodeName, "/");
        status = dsOpenDirNode(ods_state->dirRef, node, nodeReference);
        if (status != eDSNoErr)
        	DEBUG(0,("odssam_open_node: [%d]dsOpenDirNode error\n",status));
    }
    
 cleanup:
    delete_data_list(ods_state, node);

    return status;
}
static tDirStatus odssam_close_node(struct odssam_privates *ods_state, tDirNodeReference *nodeReference)
{
    tDirStatus dirStatus = eDSNoErr;
    if (nodeReference != NULL && *nodeReference != NULL) {
		dirStatus = dsCloseDirNode(*nodeReference);
		if (dirStatus != eDSNoErr)
		   DEBUG(0,("odssam_close_node: [%d]dsCloseDirNode error\n",dirStatus));
		*nodeReference = NULL;
    }
    return dirStatus;
}

static tDirStatus get_password_policy(struct odssam_privates *ods_state, tDirNodeReference pwsNode, char* userid, char* policy, char* type)
{
	tDirStatus 		status			= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
			
	
        authBuff = dsDataBufferAllocate( ods_state->dirRef, bufferSize );
        if ( authBuff != NULL )
        {
                stepBuff = dsDataBufferAllocate( ods_state->dirRef, bufferSize );
                if ( stepBuff != NULL )
                {
                        authType = dsDataNodeAllocateString( ods_state->dirRef,  "dsAuthMethodStandard:dsAuthGetEffectivePolicy" /*kDSStdAuthGetEffectivePolicy*/);
                        recordType = dsDataNodeAllocateString( ods_state->dirRef,  type);
                       if ( authType != NULL )
                        {
                                // User Name (target account )
								add_data_buffer_item(authBuff, strlen( userid ), userid);

                                status = dsDoDirNodeAuthOnRecordType( pwsNode, authType, True, authBuff, stepBuff, NULL, recordType );
                                if ( status == eDSNoErr )
                                {
                                        DEBUG(2,("kDSStdAuthGetEffectivePolicy was successful for user  \"%s\" :)\n", userid));
                                        memcpy(&len, stepBuff->fBufferData, 4);
                    					stepBuff->fBufferData[len+4] = '\0';
                    					safe_strcpy(policy,stepBuff->fBufferData+4, 1024);
                                        DEBUG(2,("kDSStdAuthGetEffectivePolicy policy  \"%s\" :)\n", policy));

                                }
                                else
                                {
                                        DEBUG(0,("kDSStdAuthGetEffectivePolicy FAILED for user \"%s\" (%d) :(\n", userid, status) );
                                }
                        }
                }
                else
                {
                        DEBUG(0,("get_password_policy: *** dsDataBufferAllocate(2) faild with \n" ));
                }
        }
        else
        {
                DEBUG(0,("get_password_policy: *** dsDataBufferAllocate(1) faild with \n" ));
        }

    delete_data_node(ods_state, authType);
    delete_data_node(ods_state, recordType);
    delete_data_buffer(ods_state, authBuff);
    delete_data_buffer(ods_state, stepBuff);

	return status;
}

static tDirStatus set_password_policy(struct odssam_privates *ods_state, tDirNodeReference pwsNode, char* userid, char* policy, char* type)
{
	tDirStatus 		status			= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
			
	
	authBuff = dsDataBufferAllocate( ods_state->dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			stepBuff = dsDataBufferAllocate( ods_state->dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( ods_state->dirRef,  "dsAuthMethodStandard:dsAuthSetPolicyAsRoot" /*kDSStdAuthSetPolicyAsRoot*/);
					recordType = dsDataNodeAllocateString( ods_state->dirRef,  type);
				   if ( authType != NULL )
					{
							add_data_buffer_item(authBuff, strlen( userid ), userid);
							add_data_buffer_item(authBuff, strlen( policy ), policy);

							status = dsDoDirNodeAuthOnRecordType( pwsNode, authType, True, authBuff, stepBuff, NULL, recordType );
							if ( status == eDSNoErr )
							{
									DEBUG(2,("kDSStdAuthSetPolicyAsRoot was successful for user  \"%s\" :)\n", userid));
							}
							else
							{
									DEBUG(0,("kDSStdAuthSetPolicyAsRoot FAILED for user \"%s\" (%d) :(\n", userid, status) );
							}
					}
			}
			else
			{
					DEBUG(0,("set_password_policy: *** dsDataBufferAllocate(2) faild with \n" ));
			}
	}
	else
	{
			DEBUG(0,("set_password_policy: *** dsDataBufferAllocate(1) faild with \n" ));
	}

    delete_data_node(ods_state, authType);
    delete_data_node(ods_state, recordType);
   	delete_data_buffer(ods_state, authBuff);
    delete_data_buffer(ods_state, stepBuff);

	return status;
}

static tDirStatus odssam_authenticate_node(struct odssam_privates *ods_state, tDirNodeReference userNode)
{
	tDirStatus 			status			= eDSNullParameter;
	unsigned long		bufferSize		= 1024 * 10;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	void				*authenticator	= NULL;
	
	become_root();
	authenticator = get_opendirectory_authenticator();
	unbecome_root();
	
	if (authenticator == NULL || 
		get_opendirectory_authenticator_accountlen(authenticator) == 0 || 
		get_opendirectory_authenticator_secretlen(authenticator) == 0) {
		return eDSNullParameter;
	}
				
	authBuff = dsDataBufferAllocate( ods_state->dirRef, bufferSize );
	if ( authBuff != NULL )
	{
		stepBuff = dsDataBufferAllocate( ods_state->dirRef, bufferSize );
		if ( stepBuff != NULL )
		{
		   authType = dsDataNodeAllocateString( ods_state->dirRef,  kDSStdAuthNodeNativeClearTextOK);
		   if ( authType != NULL)
			{
				// Account Name (authenticator)
				add_data_buffer_item(authBuff, get_opendirectory_authenticator_accountlen(authenticator), get_opendirectory_authenticator_account(authenticator));
				// Password (authenticator password)
				add_data_buffer_item(authBuff, get_opendirectory_authenticator_secretlen(authenticator), get_opendirectory_authenticator_secret(authenticator));

				status = dsDoDirNodeAuth( userNode, authType, False, authBuff, stepBuff, NULL);
				DEBUG(2,("[%d]dsDoDirNodeAuthOnRecordType kDSStdAuthNodeNativeClearTextOK \n", status));
			}
		} else {
				DEBUG(0,("authenticate_node: *** dsDataBufferAllocate(2) faild with \n" ));
		}
	}
	else
	{
			DEBUG(0,("*** dsDataBufferAllocate(1) faild with \n" ));
	}

	delete_opendirectory_authenticator(authenticator);
	delete_data_buffer(ods_state, authBuff);
	delete_data_buffer(ods_state, stepBuff);
	delete_data_node(ods_state, authType);
	
	return status;


}

static tDirStatus set_password(struct odssam_privates *ods_state, tDirNodeReference userNode, char* user, char *password, char *type)
{
	tDirStatus 		status			= eDSNullParameter;
	unsigned long	bufferSize		= 1024 * 10;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
				
        authBuff = dsDataBufferAllocate( ods_state->dirRef, bufferSize );
        if ( authBuff != NULL )
        {
                stepBuff = dsDataBufferAllocate( ods_state->dirRef, bufferSize );
                if ( stepBuff != NULL )
                {
                        authType = dsDataNodeAllocateString( ods_state->dirRef,  kDSStdAuthSetPasswdAsRoot);
                         recordType = dsDataNodeAllocateString( ods_state->dirRef,  type);
                       if ( authType != NULL )
                        {
                                // User Name
								add_data_buffer_item(authBuff, strlen( user ), user);
                                // Password
								add_data_buffer_item(authBuff, strlen( password ), password);

                                status = dsDoDirNodeAuthOnRecordType( userNode, authType, True, authBuff, stepBuff, NULL, recordType );
                                if ( status == eDSNoErr )
                                {
                                        DEBUG(2,("Set password was successful for user  \"%s\" :)\n", user));
                                }
                                else
                                {
                                        DEBUG(0,("Set password FAILED for user \"%s\" (%d) :(\n", user, status) );
                                }
                        }
                }
                else
                {
                        DEBUG(0,("set_password: *** dsDataBufferAllocate(2) faild with \n" ));
                }
        }
        else
        {
                DEBUG(0,("set_password: *** dsDataBufferAllocate(1) faild with \n" ));
        }


	delete_data_buffer(ods_state, authBuff);
	delete_data_buffer(ods_state, stepBuff);
	delete_data_node(ods_state, authType);
	delete_data_node(ods_state, recordType);

	return status;
}

tDirStatus get_record_ref(struct odssam_privates *ods_state, tDirNodeReference nodeReference, tRecordReference *ref, const char *recordType, const char *recordName)
{
	tDirStatus status = eDSNoErr;
	tDataNodePtr recordNameNode = NULL;
	tDataNodePtr recordTypeNode = NULL;
	
	recordNameNode = dsDataNodeAllocateString(ods_state->dirRef, recordName);
	recordTypeNode = dsDataNodeAllocateString(ods_state->dirRef, recordType);
	status = dsOpenRecord(nodeReference, recordTypeNode, recordNameNode, ref);
	if (status != eDSNoErr) {
        DEBUG(0,("get_record_ref: [%d]dsOpenRecord error\n",status));
	}
	
	delete_data_node(ods_state, recordNameNode);
	delete_data_node(ods_state, recordTypeNode);
	
	return status;
}

tDirStatus set_recordname(struct odssam_privates *ods_state, tRecordReference recordReference, const char *value)
{
	tDirStatus status = eDSNullParameter;
	tDataNodePtr attributeValue = NULL;
	
	if (value && strlen(value)) {
		attributeValue = dsDataNodeAllocateString(ods_state->dirRef, value);
		status = 	dsSetRecordName	( recordReference, attributeValue	);
	}
	delete_data_node(ods_state, attributeValue);
	return status;
}

BOOL isPWSAttribute(char *attribute)
{
	BOOL result = false;
	
	if ((strcmp(attribute, kPWSIsDisabled) == 0) ||
		(strcmp(attribute, kPWSIsAdmin) == 0) ||	
		(strcmp(attribute, kPWSExpiryDateEnabled) == 0) ||
		(strcmp(attribute, kPWSNewPasswordRequired) == 0) ||
		(strcmp(attribute, kPWSIsUsingHistory) == 0) ||	
		(strcmp(attribute, kPWSCanChangePassword) == 0) ||	
		(strcmp(attribute, kPWSExpiryDateEnabled) == 0) ||	
		(strcmp(attribute, kPWSRequiresAlpha) == 0) ||	
		(strcmp(attribute, kPWSExpiryDate) == 0) ||	
		(strcmp(attribute, kPWSHardExpiryDate) == 0) ||	
		(strcmp(attribute, kPWSMaxMinChgPwd) == 0) ||	
		(strcmp(attribute, kPWSMaxMinActive) == 0) ||	
		(strcmp(attribute, kPWSMaxFailedLogins) == 0) ||	
		(strcmp(attribute, kPWSMinChars) == 0) ||	
		(strcmp(attribute, kPWSMaxChars) == 0) ||	
		(strcmp(attribute, kPWSPWDCannotBeName) == 0) ||	
		(strcmp(attribute, kPWSPWDLastSetTime) == 0) ||	
		(strcmp(attribute, kPWSLastLoginTime) == 0) ||	
		(strcmp(attribute, kPWSLogOffTime) == 0) ||	
		(strcmp(attribute, kPWSKickOffTime) == 0))	
			result = true;
			
	return result;
}

void add_password_policy_attribute(char *policy, char *attribute, char *value)
{
	char *entry = NULL;
	
	entry = policy + strlen(policy);
	snprintf(entry, strlen(policy),"%s= %s ", attribute, value);
}

tDirStatus add_attribute_with_value(struct odssam_privates *ods_state, tRecordReference recordReference, const char *attribute, const char *value, BOOL addValue)
{
	tDirStatus status = eDSNoErr;
	tDataNodePtr attributeType = NULL;
	tDataNodePtr attributeValue = NULL;
	tAttributeValueEntryPtr currentAttributeValueEntry = NULL;
	tAttributeValueEntryPtr newAttributeValueEntry = NULL;
	
	attributeType = dsDataNodeAllocateString(ods_state->dirRef, attribute);
	if (addValue) {
		attributeValue = dsDataNodeAllocateString(ods_state->dirRef, value);
		status = dsAddAttributeValue(recordReference, attributeType, attributeValue);
		if (status != eDSNoErr) DEBUG(3,("add_attribute_with_value: [%d]dsAddAttributeValue error\n",status));
	} else {
#ifdef USE_SETATTRIBUTEVALUE		
		status = dsGetRecordAttributeValueByIndex( recordReference, attributeType, 1, &currentAttributeValueEntry );
		if (eDSNoErr == status) {
			newAttributeValueEntry = dsAllocAttributeValueEntry(ods_state->dirRef, currentAttributeValueEntry->fAttributeValueID, (void*)value, strlen(value));	
			///DEBUG(3,("add_attribute_with_value: dsAllocAttributeValueEntry newAttributeValueEntry(%d) valueid(%d)\n",newAttributeValueEntry, currentAttributeValueEntry->fAttributeValueID));		
			status = dsSetAttributeValue(recordReference, attributeType, newAttributeValueEntry);
			dsDeallocAttributeValueEntry(ods_state->dirRef, newAttributeValueEntry);
			if (status != eDSNoErr) DEBUG(3,("add_attribute_with_value: [%d]dsSetAttributeValue error\n",status));
		} else {
			DEBUG(0,("add_attribute_with_value: [%d]dsGetRecordAttributeValueByIndex error\n",status));		
		}
		if (currentAttributeValueEntry)
			dsDeallocAttributeValueEntry(ods_state->dirRef, currentAttributeValueEntry);
#else
		status = dsRemoveAttribute( recordReference, attributeType );
		DEBUG(3,("add_attribute_with_value: [%d]dsRemoveAttribute\n",status));
		attributeValue = dsDataNodeAllocateString(ods_state->dirRef, value);
		status = dsAddAttribute(recordReference, attributeType, 0, attributeValue);
		if (status != eDSNoErr) DEBUG(0,("add_attribute_with_value: [%d]dsAddAttribute error\n",status));				
#endif
	}
	delete_data_node(ods_state, attributeType);
	delete_data_node(ods_state, attributeValue);
	return status;
}

tDirStatus get_records(struct odssam_privates *ods_state, CFMutableArrayRef recordsArray, tDataListPtr recordName, tDataListPtr recordType, tDataListPtr attributes, BOOL continueSearch)
{
	tDirStatus		status				=	eDSNoErr;
	unsigned long		bufferSize			=	10 * 1024;
	tDataBufferPtr		dataBuffer			=	NULL;

	unsigned long		recordCount			=	0;
	unsigned long		recordIndex			=	0;
	tRecordEntryPtr		recordEntry			=	NULL;

//	tContextData 		localContextData;
	tContextData		*currentContextData = NULL;
	unsigned long		attributeIndex			=	0;
	tAttributeListRef	attributeList			= 	NULL;
	tAttributeEntryPtr 	attributeEntry			=	NULL;
	
	unsigned long		valueIndex			=	0;
	tAttributeValueEntryPtr valueEntry			=	NULL;
	tAttributeValueListRef 	valueList			= 	NULL;
	CFStringRef key = NULL;
	CFStringRef value = NULL;
	
	if (NULL == ods_state->dirRef || NULL == ods_state->searchNodeRef)
	    return status;    

	if (continueSearch)
		currentContextData = &ods_state->localContextData;
	else
		currentContextData = &ods_state->contextData;
	
	dataBuffer = dsDataBufferAllocate(ods_state->dirRef, bufferSize);
	do {
        status = dsGetRecordList(ods_state->searchNodeRef, dataBuffer, recordName, eDSiExact, recordType, attributes, false, &recordCount, currentContextData);
		if (status != eDSNoErr) {
             DEBUG(1,("dsGetRecordList error (%d)",status));
		    break; 
		}
		for (recordIndex = 1; recordIndex <= recordCount; recordIndex++) {
			status = dsGetRecordEntry(ods_state->searchNodeRef, dataBuffer, recordIndex, &attributeList, &recordEntry);
			if (status != eDSNoErr) {
             	DEBUG(1,("dsGetRecordEntry error (%d)",status));
			    break; 
			}
			CFMutableDictionaryRef dsrecord = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			for (attributeIndex = 1; attributeIndex <= recordEntry->fRecordAttributeCount; attributeIndex++) {
				status = dsGetAttributeEntry(ods_state->searchNodeRef, dataBuffer, attributeList, attributeIndex, &valueList, &attributeEntry);
				if (status != eDSNoErr) {
             		DEBUG(1,("dsGetAttributeEntry error (%d)",status));
				    break; 
				}
				CFMutableArrayRef valueArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				if (attributeEntry->fAttributeSignature.fBufferLength != 0)
					key = CFStringCreateWithBytes(NULL,
												(const UInt8*)attributeEntry->fAttributeSignature.fBufferData,
												attributeEntry->fAttributeSignature.fBufferLength,
												kCFStringEncodingUTF8,
												false);
				else
					key = NULL;
             	DEBUG(4,("get_records key(%s)\n",attributeEntry->fAttributeSignature.fBufferData));
				for (valueIndex = 1; valueIndex <= attributeEntry->fAttributeValueCount; valueIndex++) {
					status = dsGetAttributeValue(ods_state->searchNodeRef, dataBuffer, valueIndex, valueList, &valueEntry);
					if (status != eDSNoErr) {
             			DEBUG(1,("dsGetAttributeValue error (%d)",status));
					    break; 
					}
					if (valueEntry->fAttributeValueData.fBufferLength != 0)
						value = CFStringCreateWithBytes(NULL,
													(const UInt8*)valueEntry->fAttributeValueData.fBufferData,
													valueEntry->fAttributeValueData.fBufferLength,
													kCFStringEncodingUTF8,
													false);
					else
						value = NULL;
					
					if (value != NULL) {
             			DEBUG(4,("\tget_records value(%s)\n",valueEntry->fAttributeValueData.fBufferData));
						CFArrayAppendValue(valueArray, value);
						CFRelease(value);
					}

					dsDeallocAttributeValueEntry(ods_state->searchNodeRef, valueEntry);
					valueEntry = NULL;
				}
				if (key && CFArrayGetCount(valueArray))
					CFDictionaryAddValue(dsrecord, key, valueArray);
					
				if(key)
					CFRelease(key);
				if (valueArray)
					CFRelease(valueArray);
				dsCloseAttributeValueList(valueList);
				key = NULL;
				value = NULL;
				valueList = NULL;
				dsDeallocAttributeEntry(ods_state->searchNodeRef, attributeEntry);
				attributeEntry = NULL;
			}
			dsCloseAttributeList(attributeList);
			attributeList = NULL;
			dsDeallocRecordEntry(ods_state->searchNodeRef, recordEntry);
            CFArrayAppendValue(recordsArray, dsrecord);
            CFRelease(dsrecord);
		}
    } while ((status == eDSNoErr) && (continueSearch && *currentContextData != NULL));
    delete_data_buffer(ods_state, dataBuffer);
	
    return status;
}

tDirStatus search_record_attributes(struct odssam_privates *ods_state, CFMutableArrayRef recordsArray, tDataListPtr recordType, tDataNodePtr searchType,  tDataNodePtr searchValue, BOOL continueSearch)
{
	tDirStatus		status				=	eDSNoErr;
	unsigned long		bufferSize			=	10 * 1024;
	tDataBufferPtr		dataBuffer			=	NULL;

	unsigned long		recordCount			=	0;
	unsigned long		recordIndex			=	0;
	tRecordEntryPtr		recordEntry			=	NULL;

//	tContextData 		localContextData;
	tContextData		*currentContextData = NULL;
	unsigned long		attributeIndex			=	0;
	tAttributeListRef	attributeList			= 	NULL;
	tAttributeEntryPtr 	attributeEntry			=	NULL;
	
	unsigned long		valueIndex			=	0;
	tAttributeValueEntryPtr valueEntry			=	NULL;
	tAttributeValueListRef 	valueList			= 	NULL;
	CFStringRef key = NULL;
	CFStringRef value = NULL;
	CFMutableDictionaryRef dsrecord = NULL;
	CFMutableArrayRef valueArray = NULL;
	
	if (NULL == ods_state->dirRef || NULL == ods_state->searchNodeRef)
	    return status;    

	if (continueSearch)
		currentContextData = &ods_state->localContextData;
	else
		currentContextData = &ods_state->contextData;
	
	dataBuffer = dsDataBufferAllocate(ods_state->dirRef, bufferSize);
	do {
		status = dsDoAttributeValueSearchWithData(ods_state->searchNodeRef, dataBuffer, recordType, searchType, eDSExact, searchValue,  ods_state->samAttributes, false, &recordCount, currentContextData);
		if (status != eDSNoErr) {
             DEBUG(1,("dsDoAttributeValueSearchWithData error (%d)",status));
		    break; 
		}

		for (recordIndex = 1; recordIndex <= recordCount; recordIndex++) {
			status = dsGetRecordEntry(ods_state->searchNodeRef, dataBuffer, recordIndex, &attributeList, &recordEntry);
			if (status != eDSNoErr) {
             	DEBUG(1,("dsGetRecordEntry error (%d)",status));
			    break; 
			}
			dsrecord = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			for (attributeIndex = 1; attributeIndex <= recordEntry->fRecordAttributeCount; attributeIndex++) {
				status = dsGetAttributeEntry(ods_state->searchNodeRef, dataBuffer, attributeList, attributeIndex, &valueList, &attributeEntry);
				if (status != eDSNoErr) {
             		DEBUG(1,("dsGetAttributeEntry error (%d)",status));
				    break; 
				}
				valueArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				if (attributeEntry->fAttributeSignature.fBufferLength != 0)
					key = CFStringCreateWithBytes(NULL,
												(const UInt8*)attributeEntry->fAttributeSignature.fBufferData,
												attributeEntry->fAttributeSignature.fBufferLength,
												kCFStringEncodingUTF8,
												false);
				else
					key = NULL;
             	DEBUG(1,("search_records key(%s)\n",attributeEntry->fAttributeSignature.fBufferData));
				for (valueIndex = 1; valueIndex <= attributeEntry->fAttributeValueCount; valueIndex++) {
					status = dsGetAttributeValue(ods_state->searchNodeRef, dataBuffer, valueIndex, valueList, &valueEntry);
					if (status != eDSNoErr) {
             			DEBUG(1,("dsGetAttributeValue error (%d)",status));
					    break; 
					}
					if (valueEntry->fAttributeValueData.fBufferLength != 0)
						value = CFStringCreateWithBytes(NULL,
													(const UInt8*)valueEntry->fAttributeValueData.fBufferData,
													valueEntry->fAttributeValueData.fBufferLength,
													kCFStringEncodingUTF8,
													false);
					else
						value = NULL;
					
					if (value != NULL) {
             			DEBUG(1,("\tsearch_records value(%s)\n",valueEntry->fAttributeValueData.fBufferData));
						CFArrayAppendValue(valueArray, value);
						CFRelease(value);
						value = NULL;
					}

					dsDeallocAttributeValueEntry(ods_state->searchNodeRef, valueEntry);
					valueEntry = NULL;
				}
				if (key && CFArrayGetCount(valueArray))
					CFDictionaryAddValue(dsrecord, key, valueArray);
					
				if (key) {
					CFRelease(key);
					key = NULL;
				}
				if (valueArray) {
					CFRelease(valueArray);
					valueArray = NULL;
				}
				if (valueList) {
					dsCloseAttributeValueList(valueList);
					valueList = NULL;
				}
				if (attributeEntry) {
					dsDeallocAttributeEntry(ods_state->searchNodeRef, attributeEntry);
					attributeEntry = NULL;
				}
			}
			if (attributeList) {
				dsCloseAttributeList(attributeList);
				attributeList = NULL;
			}
			if (recordEntry) {
				dsDeallocRecordEntry(ods_state->searchNodeRef, recordEntry);
				recordEntry = NULL;
			}
			if (dsrecord) {
            	CFArrayAppendValue(recordsArray, dsrecord);
            	CFRelease(dsrecord);
            	dsrecord = NULL;
            }
		}
    } while ((status == eDSNoErr) && (continueSearch && *currentContextData != NULL));
    delete_data_buffer(ods_state, dataBuffer);
	
    return status;
}

tDirStatus get_sam_record_by_attr(struct odssam_privates *ods_state, CFMutableArrayRef recordsArray, const char *type, const char *attr, const char *value, BOOL continueSearch)
{
    tDirStatus status =	eDSNoErr;
    tDataListPtr recordType = NULL;
    tDataNodePtr searchType = NULL;
   	tDataNodePtr searchValue = NULL;

    status = odssam_open_search_node(ods_state);
    if (status == eDSNoErr) {
        recordType = dsBuildListFromStrings(ods_state->dirRef, type, NULL); 
        searchType = dsDataNodeAllocateString(ods_state->dirRef, attr);
        searchValue = dsDataNodeAllocateString(ods_state->dirRef, value);
      if (recordType && searchType && searchValue) {
               status = search_record_attributes(ods_state, recordsArray, recordType, searchType, searchValue, continueSearch);
        }
        
        delete_data_list(ods_state, recordType);
        delete_data_node(ods_state, searchType);
        delete_data_node(ods_state, searchValue);
    }
    return status;
}

tDirStatus get_sam_record_attributes(struct odssam_privates *ods_state, CFMutableArrayRef recordsArray, const char *type, const char *name, BOOL continueSearch)
{
    tDirStatus status =	eDSNoErr;
    tDataListPtr recordName = NULL;
    tDataListPtr recordType = NULL;
//    tDataListPtr attributes = NULL;

    status = odssam_open_search_node(ods_state);
    if (status == eDSNoErr) {
        if (name == NULL)
            recordName = dsBuildListFromStrings(ods_state->dirRef, kDSRecordsAll, NULL); 
        else
            recordName = dsBuildListFromStrings(ods_state->dirRef, name, NULL);     
        recordType = dsBuildListFromStrings(ods_state->dirRef, type, NULL); 
        if (recordName && recordType && ods_state->samAttributes) {
                status = get_records(ods_state, recordsArray, recordName, recordType, ods_state->samAttributes, continueSearch);
				if (status != eDSNoErr)
					DEBUG (5, ("[%d]get_records: [<does not exist>]\n",status));
        }
        
        delete_data_list(ods_state, recordName);
        delete_data_list(ods_state, recordType);
    }
    return status;
}
/*******************************************************************
search an attribute and return the first value found.
******************************************************************/
static BOOL get_single_attribute (CFDictionaryRef entry,
				  char *attribute, pstring value)
{
	CFStringRef cfstrRef = NULL;
	CFStringRef attrRef = NULL;
	CFArrayRef	valueList = NULL;
	char	buffer[512];
	BOOL	result = False;
	
	attrRef = CFStringCreateWithCString(NULL, attribute, kCFStringEncodingUTF8);
	valueList = (CFArrayRef)CFDictionaryGetValue(entry, attrRef);
	
	if (valueList != NULL && CFArrayGetCount(valueList) != 0) {
		cfstrRef = (CFStringRef)CFArrayGetValueAtIndex(valueList, 0);
		if (!GetCString(cfstrRef, buffer, 512)) {
			value = NULL;
			DEBUG (3, ("get_single_attribute: [%s] = [<does not exist>]\n", attribute));
		} else {
			pstrcpy(value, buffer);
			result = True;
		}
	}
#ifdef DEBUG_PASSWORDS
	DEBUG (0, ("get_single_attribute: [%s] = [%s]\n", attribute, value));
#endif
	if (attrRef)
		CFRelease(attrRef);
		
	return result;
}

static BOOL get_attributevalue_list (CFDictionaryRef entry,
				  char *attribute, CFArrayRef *valueList)
{
	CFStringRef attrRef = NULL;
	BOOL result = False;
	
	attrRef = CFStringCreateWithCString(NULL, attribute, kCFStringEncodingUTF8);
	*valueList = (CFArrayRef)CFDictionaryGetValue(entry, attrRef);
	
	if (*valueList != NULL && CFArrayGetCount(*valueList)) {
		result = True;
	}
	
	if (attrRef)
		CFRelease(attrRef);

	return result;
}

static BOOL parse_password_server(CFArrayRef authAuthorities, char *pwsServerIP, char *userid)
{
	BOOL result = False;
	int arrayCount, arrayIndex;
	char *tmp = NULL;
	char *current = NULL;
	char *pwdsrvr = "PasswordServer";
	char *delimiter = ";";
	CFStringRef authEntry = NULL;
	char authStr[512];
	
	for (arrayIndex = 0, arrayCount = CFArrayGetCount(authAuthorities); arrayIndex < arrayCount; arrayIndex++) {
		authEntry = CFArrayGetValueAtIndex(authAuthorities, arrayIndex);
		if (GetCString(authEntry, authStr, 512)) {
			if (strstr(authStr, pwdsrvr) != NULL) { 
				//;ApplePasswordServer;0x3e5d410b7be9c1bd0000000200000002:17.221.41.95;
				current = authStr;
				tmp = strsep(&current, delimiter); // tmp = ;
				if (NULL == tmp)
					break;
				tmp = strsep(&current, delimiter); // tmp = ApplePassworServer 
				if (NULL == tmp)
					break;
				tmp = strsep(&current, ":"); // tmp = 0xXXXXX
				if (NULL == tmp)
					break;
				safe_strcpy(userid,tmp, 1024);
				DEBUG(3, ("parse_password_server: userid(%s)\n",userid));
				tmp = strsep(&current, ";"); // tmp = xx.xx.xx
				if (NULL == tmp)
					break;
				safe_strcpy(pwsServerIP,tmp, 1024);
				DEBUG(3, ("parse_password_server: pwsServerIP(%s)\n",pwsServerIP));
				result = True;
				break;
			}
		}
		
	}
	
	return result;
}
static BOOL parse_passwordpolicy_attributes(CFMutableDictionaryRef entry, char *policy)
{
	BOOL result = False;
	// attr=value attr=value

    char *tmp = NULL;
    char *delimiter = "=";
    char *current = NULL;
    char *original = NULL;
    CFStringRef key = NULL;
    CFStringRef value = NULL;
	CFMutableArrayRef valueArray = NULL; 
    
    current = strdup(policy);
    original = current;
	do {
		tmp = strsep(&current, delimiter);
		DEBUG(3, ("parse_passwordpolicy_attributes: key(%s)\n",tmp));
		if (tmp != NULL)
			key = CFStringCreateWithCString(NULL, tmp, kCFStringEncodingUTF8);
		else
			key = NULL;

		tmp = strsep(&current, " ");
		DEBUG(3, ("parse_passwordpolicy_attributes: value(%s)\n",tmp));
		if (tmp != NULL)
			value = CFStringCreateWithCString(NULL, tmp, kCFStringEncodingUTF8);
		else
			value = NULL;
			
		if (key && value) {
			valueArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			CFArrayAppendValue(valueArray, value);
			CFDictionaryAddValue(entry, key, valueArray);
			CFRelease(valueArray);
			result = true;
		}
		if (key)
			CFRelease(key);
		if (value)
			CFRelease(value);
			
	} while (current != NULL);
	free(original);
	return result;
}

static tDirStatus get_passwordpolicy_attributes(struct odssam_privates *ods_state,
											SAM_ACCOUNT * sampass,
											CFMutableDictionaryRef entry, char *userName)
{
	tDirStatus status = eDSNoErr;
	char pwsServerIP[1024];
	char userid[1024];
	CFArrayRef authAuthorities = NULL;
	char *recordType = NULL;
	char dirNode[512] = {0};
	tDirNodeReference nodeReference = NULL;
	tRecordReference recordReference = NULL;
	char policy[1024];
	
	if (get_attributevalue_list(entry, kDSNAttrAuthenticationAuthority, &authAuthorities) && 
		parse_password_server(authAuthorities, pwsServerIP, userid)) {
		
		if (!get_single_attribute(entry, kDSNAttrMetaNodeLocation, dirNode)) {
			status =  eDSInvalidNodeRef;
		} else {
			status = odssam_open_node(ods_state, dirNode, &nodeReference);
			if (eDSNoErr != status) goto cleanup;
			status = odssam_authenticate_node(ods_state, nodeReference);
			if (eDSNoErr != status) goto cleanup;
	
			if (eDSNoErr == status) {
				recordType = get_record_type((const char *)userName);
				status = get_record_ref(ods_state, nodeReference, &recordReference, recordType, userName);
			}
		}
		if (eDSNoErr == status && nodeReference != NULL) {
			status = get_password_policy(ods_state, nodeReference, userName, policy, recordType);
			DEBUG(3, ("get_passwordpolicy_attributes: [%d]get_password_policy (%s, %s)\n", status, policy, recordType));
			if (eDSNoErr == status)
				parse_passwordpolicy_attributes(entry, policy);
		}
	}

cleanup:
	odssam_close_node(ods_state, &nodeReference);

	return status;
}

tDirStatus add_user_attributes(struct odssam_privates *ods_state, CFDictionaryRef samAttributes, CFDictionaryRef userCurrent)
{
	tDirStatus status = eDSNoErr;

    int attributeIndex;
    int count = CFDictionaryGetCount(samAttributes); 
    CFStringRef values[count];
    CFStringRef keys[count];
	char key[255] = {0};
	char value[255] = {0};
    Boolean isKey,isValue;
	char dirNode[512] = {0};
	char temp[512] = {0};
	char userName[128] = {0};
	char policy[1024] = {0};
	BOOL addValue;
	
	tDirNodeReference nodeReference = NULL;
	tRecordReference recordReference = NULL;
	char *recordType = NULL;
	
	if (!get_single_attribute(userCurrent, kDSNAttrRecordName, userName)) {
		status = eDSNullParameter;
		goto cleanup;
	}
	if (!get_single_attribute(userCurrent, kDSNAttrMetaNodeLocation, dirNode)) {
		status =  eDSInvalidNodeRef;
	} else {
		status = odssam_open_node(ods_state, dirNode, &nodeReference);
		if (eDSNoErr == status) {
			status = odssam_authenticate_node(ods_state, nodeReference);
			if (eDSNoErr == status) {
				recordType = get_record_type((const char *)userName);
				status = get_record_ref(ods_state, nodeReference, &recordReference, recordType, userName);
			}
		}
	}
	
	if (eDSNoErr == status) {
		if (eDSNoErr == status) {
			for (attributeIndex = 0, CFDictionaryGetKeysAndValues(samAttributes, (const void**)keys, (const void**)values); attributeIndex < count; attributeIndex++) {
				isKey = CFStringGetCString(keys[attributeIndex], key, sizeof(key), kCFStringEncodingUTF8);
				isValue = CFStringGetCString(values[attributeIndex], value, sizeof(value), kCFStringEncodingUTF8);

				if (get_single_attribute(userCurrent,key, temp))
					addValue = false;
				else
					addValue = true;
	
				if (isKey && isValue) {
					if (strcmp(key, kPlainTextPassword) == 0) {
						status = set_password(ods_state, nodeReference, userName, value, recordType);
					#ifdef DEBUG_PASSWORDS
						DEBUG (100, ("add_user_attributes: [%d]SetPassword(%s, %s, %s, %s)\n",status, userName, key, value, recordType));
					else
						DEBUG (100, ("add_user_attributes: [%d]SetPassword(%s, %s, %s)\n",status, userName, key, recordType));
					#endif
					} else if (strcmp(key, kDSNAttrRecordName) == 0) {
						
						if (strcmp(userName, value) != 0) {
							status = set_recordname(ods_state, recordReference, value);
							DEBUG (3, ("add_user_attributes: [%d]set_recordname(%s, %s, %s)\n",status, userName, key, value));
						}
					} else if (isPWSAttribute(key)) {
						add_password_policy_attribute(policy, key, value);
					} else {
						status = add_attribute_with_value(ods_state, recordReference, key, value, addValue);
						if (status != eDSNoErr)
							DEBUG (3, ("[%d]add_user_attributes: add_attribute_with_value(%s,%s,%s) error\n",status, userName, key, value));
					}
					if (status != eDSNoErr)
						break;
				}
			}
			if (strlen(policy) > 0)
				status =  set_password_policy(ods_state, nodeReference, userName, policy, recordType);
		} else {
			DEBUG (0, ("[%d]add_user_attributes: authenticate_node error\n",status));
		}
    }
cleanup:
	if (NULL != recordReference)
		dsCloseRecord(recordReference);
	odssam_close_node(ods_state, &nodeReference);    
	return status;
}

/**********************************************************************
Initialize SAM_ACCOUNT from an Open Directory query
(Based on init_sam_from_buffer in pdb_tdb.c)
*********************************************************************/
static BOOL init_sam_from_ods (struct odssam_privates *ods_state, 
				SAM_ACCOUNT * sampass,
				CFMutableDictionaryRef entry)
{
	time_t  logon_time,
			logoff_time,
			kickoff_time,
			pass_last_set_time, 
			pass_can_change_time, 
			pass_must_change_time;
	pstring 	username, 
			domain,
			nt_username,
			fullname,
			homedir,
			dir_drive,
			logon_script,
			profile_path,
			acct_desc,
			munged_dial,
			workstations;
	uint32	is_disabled;
	uint32 	user_rid, 
			group_rid;
#if WITH_PASSWORD_HASH
	uint8 	smblmpwd[LM_HASH_LEN],
			smbntpwd[NT_HASH_LEN];
#endif
	uint16 		acct_ctrl, 
			logon_divs;
	uint32 hours_len;
	uint8 		hours[MAX_HOURS_LEN];
	pstring temp;
	pstring boolFlag;
	uid_t		uid = 99; /* 'unknown' user account == guest*/
	gid_t 		gid = getegid();

	/*
	 * do a little initialization
	 */
	username[0] 	= '\0';
	domain[0] 	= '\0';
	nt_username[0] 	= '\0';
	fullname[0] 	= '\0';
	homedir[0] 	= '\0';
	dir_drive[0] 	= '\0';
	logon_script[0] = '\0';
	profile_path[0] = '\0';
	acct_desc[0] 	= '\0';
	munged_dial[0] 	= '\0';
	workstations[0] = '\0';
	 

	if (sampass == NULL || ods_state == NULL || entry == NULL) {
		DEBUG(0, ("init_sam_from_ods: NULL parameters found!\n"));
		return False;
	}
	
	get_single_attribute(entry, kDSNAttrRecordName, username);
	DEBUG(2, ("Entry found for user: %s\n", username));
	
	get_single_attribute(entry, kDS1AttrDistinguishedName, nt_username);

	get_passwordpolicy_attributes(ods_state, sampass, entry, username);
//	pstrcpy(nt_username, username);

	pstrcpy(domain, lp_workgroup());
	
	pdb_set_username(sampass, username, PDB_SET);

	pdb_set_domain(sampass, domain, PDB_DEFAULT);
	pdb_set_nt_username(sampass, nt_username, PDB_SET);

	if (!get_single_attribute(entry, kDS1AttrUniqueID, temp)) {
		/* leave as default */
	} else {
		uid = atol(temp);
        ///pdb_set_uid(sampass, uid, PDB_SET);
	}

	if (get_single_attribute(entry, kDS1AttrSMBRID, temp)) {
		DEBUG(3, ("init_sam_from_ods: kDS1AttrSMBRID (%s)\n", temp));
		user_rid = (uint32)atol(temp);
	} else if (get_single_attribute(entry, kDS1AttrUniqueID, temp)) {
		if (uid == 99)
			user_rid = DOMAIN_USER_RID_GUEST;
		else
			user_rid = fallback_pdb_uid_to_user_rid((uint32)uid);
		DEBUG(3, ("init_sam_from_ods: use kDS1AttrUniqueID (%s) -> RID(%d)\n", temp, user_rid));

	} else {
		DEBUG(3, ("init_sam_from_ods: kDS1AttrSMBRID mapped to DOMAIN_USER_RID_GUEST\n"));
		user_rid = DOMAIN_USER_RID_GUEST;
	}
		
	pdb_set_user_sid_from_rid(sampass, user_rid, PDB_SET);

	if (!get_single_attribute(entry, kDS1AttrSMBGroupRID, temp)) {
		group_rid = 0;
	} else {
		group_rid = (uint32)atol(temp);
		pdb_set_group_sid_from_rid(sampass, group_rid, PDB_SET);
	}


	if (!get_single_attribute(entry, kDS1AttrPrimaryGroupID, temp)) {
		/* leave as default */
	} else {
		gid = atol(temp);
///                pdb_set_gid(sampass, gid, PDB_SET);
	}
	if (!get_single_attribute(entry, kDS1AttrNFSHomeDirectory, temp)) {
		/* leave as default */
	} else {
                pdb_set_unix_homedir(sampass, temp, PDB_SET);
	}

	if (group_rid == 0) {
		GROUP_MAP map;
		
		get_single_attribute(entry, kDS1AttrPrimaryGroupID, temp);
		gid = atol(temp);
		/* call the mapping code here */
		if(pdb_getgrgid(&map, gid)) {
			pdb_set_group_sid(sampass, &map.sid, PDB_SET);
		} 
		else {
			pdb_set_group_sid_from_rid(sampass, pdb_gid_to_group_rid(gid), PDB_SET);
		}
	}

	if (!get_single_attribute(entry, kPWSPWDLastSetTime, temp)) {
		/* leave as default */
	} else {
		pass_last_set_time = (time_t) atol(temp);
		pdb_set_pass_last_set_time(sampass, pass_last_set_time, PDB_SET);
	}

	if (!get_single_attribute(entry, kPWSLastLoginTime, temp)) {
		/* leave as default */
	} else {
		logon_time = (time_t) atol(temp);
		pdb_set_logon_time(sampass, logon_time, PDB_SET);
	}

	if (!get_single_attribute(entry, kPWSLogOffTime, temp)) {
		/* leave as default */
	} else {
		logoff_time = (time_t) atol(temp);
		pdb_set_logoff_time(sampass, logoff_time, PDB_SET);
	}

	if (!get_single_attribute(entry, kPWSKickOffTime, temp)) {
		/* leave as default */
	} else {
		kickoff_time = (time_t) atol(temp);
		pdb_set_kickoff_time(sampass, kickoff_time, PDB_SET);
	}

	if (!get_single_attribute(entry, kPWSExpiryDate, temp)) {
		/* leave as default */
	} else {
		pass_can_change_time = (time_t) atol(temp);
		pdb_set_pass_can_change_time(sampass, pass_can_change_time, PDB_SET);
	}

	if (!get_single_attribute(entry, kPWSHardExpiryDate, temp)) {
		/* leave as default */
	} else {
		if (get_single_attribute(entry, kPWSExpiryDateEnabled, boolFlag) && strcmp(boolFlag,"1") == 0) {
			if (get_single_attribute(entry, kPWSNewPasswordRequired, boolFlag) && strcmp(boolFlag,"1") == 0) {
				struct timeval tv;
				GetTimeOfDay(&tv);
				pass_must_change_time = (time_t) tv.tv_sec;
			} else
				pass_must_change_time = (time_t) atol(temp);
			pdb_set_pass_must_change_time(sampass, pass_must_change_time, PDB_SET);
		}
	}

	if (!get_single_attribute(entry, kDS1AttrDistinguishedName, fullname)) {
			/* leave as default */
	} else {
			pdb_set_fullname(sampass, fullname, PDB_SET);
	}

	if (!get_single_attribute(entry, kDS1AttrSMBHomeDrive, dir_drive)) {
		pdb_set_dir_drive(sampass, talloc_sub_specified(sampass->mem_ctx, 
								  lp_logon_drive(),
								  username, domain, 
								  uid, gid),
				  PDB_DEFAULT);
	} else {
		pdb_set_dir_drive(sampass, dir_drive, PDB_SET);
	}

	if (!get_single_attribute(entry, kDS1AttrSMBHome, homedir)) {
		pdb_set_homedir(sampass, talloc_sub_specified(sampass->mem_ctx, 
								  lp_logon_home(),
								  username, domain, 
								  uid, gid), 
				  PDB_DEFAULT);
	} else {
		pdb_set_homedir(sampass, homedir, PDB_SET);
	}

	if (!get_single_attribute(entry, kDS1AttrSMBScriptPath, logon_script)) {
		pdb_set_logon_script(sampass, talloc_sub_specified(sampass->mem_ctx, 
								     lp_logon_script(),
								     username, domain, 
								     uid, gid), 
				     PDB_DEFAULT);
	} else {
		pdb_set_logon_script(sampass, logon_script, PDB_SET);
	}

	if (!get_single_attribute(entry, kDS1AttrSMBProfilePath, profile_path)) {
		pdb_set_profile_path(sampass, talloc_sub_specified(sampass->mem_ctx, 
								     lp_logon_path(),
								     username, domain, 
								     uid, gid), 
				     PDB_DEFAULT);
	} else {
		pdb_set_profile_path(sampass, profile_path, PDB_SET);
	}

	if (!get_single_attribute(entry, kDS1AttrComment, acct_desc)) {
		/* leave as default */
	} else {
		pdb_set_acct_desc(sampass, acct_desc, PDB_SET);
	}

	if (!get_single_attribute(entry, kDS1AttrSMBUserWorkstations, workstations)) {
		/* leave as default */;
	} else {
		pdb_set_workstations(sampass, workstations, PDB_SET);
	}

	/* FIXME: hours stuff should be cleaner */
	
	logon_divs = 168;
	hours_len = 21;
	memset(hours, 0xff, hours_len);
#if WITH_PASSWORD_HASH
	if (!get_single_attribute (entry, kDS1AttrSMBLMPassword, temp)) {
		/* leave as default */
	} else {
		pdb_gethexpwd(temp, smblmpwd);
		memset((char *)temp, '\0', strlen(temp)+1);
		if (!pdb_set_lanman_passwd(sampass, smblmpwd, PDB_SET))
			return False;
		ZERO_STRUCT(smblmpwd);
	}

	if (!get_single_attribute (entry, kDS1AttrSMBNTPassword, temp)) {
		/* leave as default */
	} else {
		pdb_gethexpwd(temp, smbntpwd);
		memset((char *)temp, '\0', strlen(temp)+1);
		if (!pdb_set_nt_passwd(sampass, smbntpwd, PDB_SET))
			return False;
		ZERO_STRUCT(smbntpwd);
	}
#endif
	if (!get_single_attribute (entry, kDS1AttrSMBAcctFlags, temp)) {
		acct_ctrl |= ACB_NORMAL;
	} else {
		acct_ctrl = pdb_decode_acct_ctrl(temp);

		if (acct_ctrl == 0)
			acct_ctrl |= ACB_NORMAL;
		
		if (get_single_attribute (entry, kPWSIsDisabled, temp)) {
			is_disabled = atol(temp);
			if (is_disabled == 1)
				acct_ctrl |= ACB_DISABLED;
		}
			
		pdb_set_acct_ctrl(sampass, acct_ctrl, PDB_SET);
	}

	pdb_set_hours_len(sampass, hours_len, PDB_SET);
	pdb_set_logon_divs(sampass, logon_divs, PDB_SET);

	pdb_set_munged_dial(sampass, munged_dial, PDB_SET);
	
	/* pdb_set_unknown_3(sampass, unknown3, PDB_SET); */
	/* pdb_set_unknown_5(sampass, unknown5, PDB_SET); */
	/* pdb_set_unknown_6(sampass, unknown6, PDB_SET); */

	pdb_set_hours(sampass, hours, PDB_SET);


	return True;
}

static void make_a_mod (CFMutableDictionaryRef userEntry, const char *attribute, const char *value) {
	CFStringRef cfKey = NULL;
	CFStringRef cfValue = NULL;

	if (attribute != NULL && strlen(attribute))
		cfKey = CFStringCreateWithCString(NULL, attribute, kCFStringEncodingUTF8);
	else
		DEBUG(0, ("make_a_mod: INVALID ATTRIBUTE!!!\n"));
	
	if (value != NULL && strlen(value))
		cfValue = CFStringCreateWithCString(NULL, value, kCFStringEncodingUTF8);
	else
		DEBUG(0, ("make_a_mod: INVALID VALUE!!!\n"));
	
	if (cfKey && cfValue)
		CFDictionaryAddValue(userEntry, cfKey, cfValue);
	if(cfKey)
		CFRelease(cfKey);
	if(cfValue)
		CFRelease(cfValue);
}

static BOOL need_ods_mod(BOOL pdb_add, const SAM_ACCOUNT * sampass, enum pdb_elements element) {
	if (pdb_add) {
		return (!IS_SAM_DEFAULT(sampass, element));
	} else {
		return IS_SAM_CHANGED(sampass, element);
	}
}

/**********************************************************************
Initialize Open Directory account from SAM_ACCOUNT 
(Based on init_buffer_from_sam in pdb_tdb.c)
*********************************************************************/
static BOOL init_ods_from_sam (struct odssam_privates *ods_state, BOOL pdb_add, SAM_ACCOUNT * sampass, CFMutableDictionaryRef userEntry)

{
	pstring temp;
	uint32 rid;

	if (sampass == NULL) {
		DEBUG(0, ("init_ods_from_sam: NULL parameters found!\n"));
		return False;
	}

	if (need_ods_mod(pdb_add, sampass, PDB_USERNAME)) {
		make_a_mod(userEntry, kDSNAttrRecordName, pdb_get_username(sampass));
		DEBUG(2, ("Setting entry for user: %s\n", pdb_get_username(sampass)));
	}
	
	if ((rid = pdb_get_user_rid(sampass))!=0 ) {
		if (need_ods_mod(pdb_add, sampass, PDB_USERSID)) {		
			slprintf(temp, sizeof(temp) - 1, "%i", rid);
			make_a_mod(userEntry, kDS1AttrSMBRID, temp);
		}
	}
#ifdef STORE_ALGORITHMIC_RID
	 else if (!IS_SAM_DEFAULT(sampass, PDB_UID)) {
		rid = fallback_pdb_uid_to_user_rid(pdb_get_uid(sampass));
		slprintf(temp, sizeof(temp) - 1, "%i", rid);
		make_a_mod(userEntry, kDS1AttrSMBRID, temp);
	} else {
		DEBUG(0, ("NO user RID specified on account %s, cannot store!\n", pdb_get_username(sampass)));
		return False;
	}

#endif

	if ((rid = pdb_get_group_rid(sampass))!=0 ) {
		if (need_ods_mod(pdb_add, sampass, PDB_GROUPSID)) {		
			slprintf(temp, sizeof(temp) - 1, "%i", rid);
			make_a_mod(userEntry, kDS1AttrSMBGroupRID, temp);
		}
	} 
	
#ifdef STORE_ALGORITHMIC_RID
	else if (!IS_SAM_DEFAULT(sampass, PDB_GID)) {
		rid = pdb_gid_to_group_rid(pdb_get_gid(sampass));
		slprintf(temp, sizeof(temp) - 1, "%i", rid);
		make_a_mod(userEntry, kDS1AttrSMBGroupRID, temp);
	} else {
		DEBUG(0, ("NO group RID specified on account %s, cannot store!\n", pdb_get_username(sampass)));
		return False;
	}
#endif

	/* displayName, cn, and gecos should all be the same
	 *  most easily accomplished by giving them the same OID
	 *  gecos isn't set here b/c it should be handled by the 
	 *  add-user script
	 */
	if (need_ods_mod(pdb_add, sampass, PDB_FULLNAME)) {
		make_a_mod(userEntry, kDS1AttrDistinguishedName, pdb_get_fullname(sampass));
	}
	if (need_ods_mod(pdb_add, sampass, PDB_ACCTDESC)) {	
		make_a_mod(userEntry, kDS1AttrComment, pdb_get_acct_desc(sampass));
	}
	if (need_ods_mod(pdb_add, sampass, PDB_WORKSTATIONS)) {	
		make_a_mod(userEntry,  kDS1AttrSMBUserWorkstations, pdb_get_workstations(sampass));
	}
	/*
	 * Only updates fields which have been set (not defaults from smb.conf)
	 */

	if (need_ods_mod(pdb_add, sampass, PDB_SMBHOME)) {
		make_a_mod(userEntry, kDS1AttrSMBHome, pdb_get_homedir(sampass));
	}
			
	if (need_ods_mod(pdb_add, sampass, PDB_DRIVE)) {
		make_a_mod(userEntry, kDS1AttrSMBHomeDrive, pdb_get_dir_drive(sampass));
	}
	
	if (need_ods_mod(pdb_add, sampass, PDB_LOGONSCRIPT)) {
		make_a_mod(userEntry, kDS1AttrSMBScriptPath, pdb_get_logon_script(sampass));
	}
	
	if (need_ods_mod(pdb_add, sampass, PDB_PROFILE))
		make_a_mod(userEntry, kDS1AttrSMBProfilePath, pdb_get_profile_path(sampass));

	if (need_ods_mod(pdb_add, sampass, PDB_LOGONTIME)) {
		slprintf(temp, sizeof(temp) - 1, "%li", pdb_get_logon_time(sampass));
		make_a_mod(userEntry, kPWSLastLoginTime, temp);
	}

	if (need_ods_mod(pdb_add, sampass, PDB_LOGOFFTIME)) {
		slprintf(temp, sizeof(temp) - 1, "%li", pdb_get_logoff_time(sampass));
		make_a_mod(userEntry, kPWSLogOffTime, temp);
	}

	if (need_ods_mod(pdb_add, sampass, PDB_KICKOFFTIME)) {
		slprintf (temp, sizeof (temp) - 1, "%li", pdb_get_kickoff_time(sampass));
		make_a_mod(userEntry, kPWSKickOffTime, temp);
	}


	if (need_ods_mod(pdb_add, sampass, PDB_CANCHANGETIME)) {
		slprintf (temp, sizeof (temp) - 1, "%li", pdb_get_pass_can_change_time(sampass));
		make_a_mod(userEntry, kPWSExpiryDate, temp);
	}

	if (need_ods_mod(pdb_add, sampass, PDB_MUSTCHANGETIME)) {
		slprintf (temp, sizeof (temp) - 1, "%li", pdb_get_pass_must_change_time(sampass));
		make_a_mod(userEntry, kPWSHardExpiryDate, temp);
	}
//	if ((pdb_get_acct_ctrl(sampass)&(ACB_WSTRUST|ACB_SVRTRUST|ACB_DOMTRUST))) {
	if ((pdb_get_acct_ctrl(sampass)&(ACB_WSTRUST|ACB_SVRTRUST|ACB_DOMTRUST|ACB_NORMAL))) {

#if WITH_PASSWORD_HASH
		if (need_ods_mod(pdb_add, sampass, PDB_LMPASSWD)) {
			pdb_sethexpwd (temp, pdb_get_lanman_passwd(sampass), pdb_get_acct_ctrl(sampass));
			make_a_mod (userEntry, kDS1AttrSMBLMPassword, temp);
		}
		
		if (need_ods_mod(pdb_add, sampass, PDB_NTPASSWD)) {
			pdb_sethexpwd (temp, pdb_get_nt_passwd(sampass), pdb_get_acct_ctrl(sampass));
			make_a_mod (userEntry, kDS1AttrSMBNTPassword, temp);
		}
#endif		
		if (need_ods_mod(pdb_add, sampass, PDB_PLAINTEXT_PW)) {
			make_a_mod (userEntry, kPlainTextPassword, pdb_get_plaintext_passwd(sampass));
		}
#if USES_PWS
		if (need_ods_mod(pdb_add, sampass, PDB_PASSLASTSET)) {
			slprintf (temp, sizeof (temp) - 1, "%li", pdb_get_pass_last_set_time(sampass));
			make_a_mod(userEntry, kPWSPWDLastSetTime, temp);
		}
#endif
	}

	/* FIXME: Hours stuff goes in directory  */
	if (need_ods_mod(pdb_add, sampass, PDB_ACCTCTRL)) {
		make_a_mod (userEntry, kDS1AttrSMBAcctFlags, pdb_encode_acct_ctrl (pdb_get_acct_ctrl(sampass),
			NEW_PW_FORMAT_SPACE_PADDED_LEN));
	}

	return True;
}

/* passdb functions */


/**********************************************************************
End enumeration of the Open Directory Service password list 
*********************************************************************/
static void odssam_endsampwent(struct pdb_methods *my_methods)
{
	struct odssam_privates *ods_state = (struct odssam_privates *)my_methods->private_data;

	CFArrayRemoveAllValues(ods_state->usersArray);        
	ods_state->usersIndex = 0;        
}

static NTSTATUS odssam_setsampwent(struct pdb_methods *my_methods, BOOL update)
{
	odssam_endsampwent(my_methods);
	DEBUG(0,("odssam_setsampwent: update(%d)\n", update));

	return NT_STATUS_OK;
}

/**********************************************************************
Get the next entry in the Open Directory Service password database 
*********************************************************************/
static NTSTATUS odssam_getsampwent(struct pdb_methods *my_methods, SAM_ACCOUNT *user)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	tDirStatus dirStatus = eDSNoErr;
	int entriesAvailable = 0;
	CFMutableDictionaryRef entry = NULL;
	
	struct odssam_privates *ods_state = (struct odssam_privates *)my_methods->private_data;

	if (ods_state->usersArray != NULL)
		entriesAvailable = CFArrayGetCount(ods_state->usersArray);
	else
		return 	NT_STATUS_UNSUCCESSFUL; // allocate array???
			
	if (entriesAvailable == 0 || ods_state->usersIndex >= entriesAvailable) {
		DEBUG(0,("odssam_getsampwent: entriesAvailable(%d) contextData(%p)\n", entriesAvailable, ods_state->contextData));
		CFArrayRemoveAllValues(ods_state->usersArray);
		
		if (entriesAvailable && ods_state->usersIndex >= entriesAvailable && ods_state->contextData == NULL) {
			odssam_endsampwent(my_methods);
			return NT_STATUS_UNSUCCESSFUL;
		}
			        
		if ((dirStatus = get_sam_record_attributes(ods_state, ods_state->usersArray, kDSStdRecordTypeUsers, NULL, false)) != eDSNoErr) {
			ret = NT_STATUS_UNSUCCESSFUL;
		} else {
			entriesAvailable = CFArrayGetCount(ods_state->usersArray);
			DEBUG(0,("odssam_getsampwent: entriesAvailable Take 2(%d) contextData(%p)\n", entriesAvailable, ods_state->contextData));
			ods_state->usersIndex = 0;
		}
	}
	
	if (dirStatus == eDSNoErr && entriesAvailable) {
		entry = (CFDictionaryRef) CFArrayGetValueAtIndex(ods_state->usersArray, ods_state->usersIndex);
		ods_state->usersIndex++;
		if (!init_sam_from_ods(ods_state, user, entry)) {
			DEBUG(1,("odssam_getsampwent: init_sam_from_ods failed for user index(%d)\n", ods_state->usersIndex));
//			CFRelease(entry);
			ret = NT_STATUS_UNSUCCESSFUL;
		}
//        CFRelease(entry);
		ret = NT_STATUS_OK;
	}
	

	return ret;
}

static NTSTATUS odssam_getsampwnam(struct pdb_methods *my_methods, SAM_ACCOUNT *user, const char *sname)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	tDirStatus dirStatus = eDSNoErr;
	char *recordType = NULL;
	CFMutableDictionaryRef entry = NULL;
	
	struct odssam_privates *ods_state = (struct odssam_privates *)my_methods->private_data;

    CFMutableArrayRef usersArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    
	recordType = get_record_type(sname);
	if (((dirStatus = get_sam_record_attributes(ods_state, usersArray, recordType, sname, true)) != eDSNoErr) || (CFArrayGetCount(usersArray) == 0)) {
		DEBUG(0,("odssam_getsampwnam: [%d]get_sam_record_attributes %s no account for '%s'!\n", dirStatus, recordType, sname));
		ret = NT_STATUS_UNSUCCESSFUL;
	}
/* handle duplicates - currently uses first match in search policy*/
	if (dirStatus == eDSNoErr && CFArrayGetCount(usersArray)) {
            entry = (CFDictionaryRef) CFArrayGetValueAtIndex(usersArray, 0);
		if (!init_sam_from_ods(ods_state, user, entry)) {
            DEBUG(1,("odssam_getsampwnam: init_sam_from_ods failed for account '%s'!\n", sname));
            ret = NT_STATUS_UNSUCCESSFUL;
		}    
        //CFRelease(entry);
		ret = NT_STATUS_OK;
	}
	
	CFRelease(usersArray);

	return ret;
}

static NTSTATUS odssam_getsampwrid(struct pdb_methods *my_methods, SAM_ACCOUNT *user, uint32 rid)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	struct odssam_privates *ods_state = (struct odssam_privates *)my_methods->private_data;
    CFMutableArrayRef usersArray = NULL;
	int numRecords = 0;
	uint32 uid = 99;
	CFMutableDictionaryRef entry = NULL;
	pstring filter;
	snprintf(filter, sizeof(filter) - 1, "%i", rid);
	
	DEBUG(1,("odssam_getsampwrid: rid<%d> rid str<%s>\n", rid, filter));
	
	if (rid == DOMAIN_USER_RID_GUEST) {
		const char *guest_account = lp_guestaccount();
		if (!(guest_account && *guest_account)) {
			DEBUG(0, ("Guest account not specified!\n"));
			return ret;
		}
		return odssam_getsampwnam(my_methods, user, guest_account);
	}

    usersArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    
	if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeUsers, kDS1AttrSMBRID, filter, True) != eDSNoErr || (CFArrayGetCount(usersArray) == 0)) {
		if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeComputers, kDS1AttrSMBRID, filter, True) != eDSNoErr || (CFArrayGetCount(usersArray) == 0)) {
			DEBUG(4,("We didn't find this rid [%i] count=%d \n", rid, numRecords));
			uid = fallback_pdb_user_rid_to_uid(rid);
			snprintf(filter, sizeof(filter) - 1, "%i", uid);
			DEBUG(4,("Look up by algorithmic rid using uid [%i]\n", uid));
			if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeUsers, kDS1AttrUniqueID, filter, True) != eDSNoErr || (CFArrayGetCount(usersArray) == 0)){
				if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeComputers, kDS1AttrUniqueID, filter, True) != eDSNoErr || (CFArrayGetCount(usersArray) == 0)) {
					ret = NT_STATUS_NO_SUCH_USER;
					goto cleanup;
				}
			}
		}
	}

	entry = (CFDictionaryRef) CFArrayGetValueAtIndex(usersArray, 0);
	if (entry) {
		if (!init_sam_from_ods(ods_state, user, entry)) {
			DEBUG(1,("odssam_getsampwrid: init_sam_from_ods failed!\n"));
			ret = NT_STATUS_NO_SUCH_USER;
			goto cleanup;
		}
		ret = NT_STATUS_OK;
		//CFRelease(entry);
	} else {
		ret = NT_STATUS_NO_SUCH_USER;
		goto cleanup;
	}

cleanup:
	CFRelease(usersArray);
	return ret;
}

static NTSTATUS odssam_getsampwsid(struct pdb_methods *my_methods, SAM_ACCOUNT * account, const DOM_SID *sid)
{
	uint32 rid;
	if (!sid_peek_check_rid(get_global_sam_sid(), sid, &rid))
		return NT_STATUS_NO_SUCH_USER;
	return odssam_getsampwrid(my_methods, account, rid);
}	

static NTSTATUS odssam_add_sam_account(struct pdb_methods *my_methods, SAM_ACCOUNT * newpwd)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	tDirStatus dirStatus = eDSNoErr;
	char *recordType = NULL;
	struct odssam_privates *ods_state = (struct odssam_privates *)my_methods->private_data;
	int 		ops = 0;
    CFMutableArrayRef usersArray = NULL;
    CFMutableDictionaryRef userMods = NULL;
	
	const char *username = pdb_get_username(newpwd);
	if (!username || !*username) {
		DEBUG(0, ("Cannot add user without a username!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	recordType = get_record_type((const char *)username);
    usersArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if ((dirStatus = get_sam_record_attributes(ods_state, usersArray, recordType, username, true)) != eDSNoErr || (CFArrayGetCount(usersArray) == 0)) {
			ret = NT_STATUS_UNSUCCESSFUL;
			goto cleanup;
	}

// check for SMB Attributes and bail if already added
	
#if 0 /* skip duplicates for now */ 
	if (CFArrayGetCount(usersArray) > 1) {
		DEBUG (0, ("More than one user with that uid exists: bailing out!\n"));
		ret =  NT_STATUS_UNSUCCESSFUL;
		goto cleanup;
	}
#endif
	/* Check if we need to update an existing entry */

	userMods = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!init_ods_from_sam(ods_state, ops, newpwd, userMods)) {
		DEBUG(0, ("odssam_add_sam_account: init_ods_from_sam failed!\n"));
		ret =  NT_STATUS_UNSUCCESSFUL;
		goto cleanup;
	}	

	if (CFDictionaryGetCount(userMods) == 0) {
		DEBUG(0,("mods is empty: nothing to add for user: %s\n",pdb_get_username(newpwd)));
		return NT_STATUS_UNSUCCESSFUL;
	}
	
	CFDictionaryRef userCurrent = (CFDictionaryRef) CFArrayGetValueAtIndex(usersArray, 0);
	dirStatus = add_user_attributes(ods_state, userMods, userCurrent);
	if (eDSNoErr != dirStatus) {
		ret = NT_STATUS_UNSUCCESSFUL;
		DEBUG(0, ("odssam_add_sam_account: [%d]add_user_attributes\n", dirStatus));
	} else {
		ret = NT_STATUS_OK;
	}

cleanup:
	if (usersArray)
		CFRelease(usersArray);
	if (userMods)
		CFRelease(userMods);

	return ret;
}

static NTSTATUS odssam_update_sam_account(struct pdb_methods *my_methods, SAM_ACCOUNT * newpwd)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;

	ret = odssam_add_sam_account(my_methods, newpwd);
	
	return ret;
}

///
#if 0
static NTSTATUS odssam_getgrsid(struct pdb_methods *methods, GROUP_MAP *map,
				 DOM_SID sid, BOOL with_priv)
{
	return get_group_map_from_sid(sid, map, with_priv) ?
		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS odssam_getgrgid(struct pdb_methods *methods, GROUP_MAP *map,
				 gid_t gid, BOOL with_priv)
{
//	DEBUG(0, ("odssam_getgrgid (%d)\n", gid));
	return get_group_map_from_gid(gid, map, with_priv) ?
		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS odssam_getgrnam(struct pdb_methods *methods, GROUP_MAP *map,
				 char *name, BOOL with_priv)
{
	return get_group_map_from_ntname(name, map, with_priv) ?
		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS odssam_add_group_mapping_entry(struct pdb_methods *methods,
						GROUP_MAP *map)
{
	return add_mapping_entry(map, TDB_INSERT) ?
		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS odssam_update_group_mapping_entry(struct pdb_methods *methods,
						   GROUP_MAP *map)
{
	return add_mapping_entry(map, TDB_REPLACE) ?
		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS odssam_delete_group_mapping_entry(struct pdb_methods *methods,
						   DOM_SID sid)
{
	return group_map_remove(sid) ?
		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS odssam_enum_group_mapping(struct pdb_methods *methods,
					   enum SID_NAME_USE sid_name_use,
					   GROUP_MAP **rmap, int *num_entries,
					   BOOL unix_only, BOOL with_priv)
{
	return enum_group_mapping(sid_name_use, rmap, num_entries, unix_only,
				  with_priv) ?
		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}
#endif

static void odssam_free_private_data(void **data)
{	
	struct odssam_privates *ods_state = (struct odssam_privates *)(*data);

	odssam_close_search_node(ods_state);
	odssam_close(ods_state);
	DEBUG(5, ("opendirectorysam->free_private_data !!!!\n"));	
}

static NTSTATUS odssam_init(PDB_CONTEXT *pdb_context, PDB_METHODS **pdb_method, const char *location)
{
	NTSTATUS nt_status;
	struct odssam_privates *ods_state = NULL;
	
	if (!NT_STATUS_IS_OK(nt_status = make_pdb_methods(pdb_context->mem_ctx, pdb_method))) {
		return nt_status;
	}

	(*pdb_method)->name = "opendirectorysam";

	/* Functions your pdb module doesn't provide should be set 
	 * to NULL */

	(*pdb_method)->setsampwent = odssam_setsampwent;
	(*pdb_method)->endsampwent =  odssam_endsampwent;
	(*pdb_method)->getsampwent =  odssam_getsampwent;
	(*pdb_method)->getsampwnam = odssam_getsampwnam;
	(*pdb_method)->getsampwsid =  odssam_getsampwsid;
	(*pdb_method)->add_sam_account =  odssam_add_sam_account;
	(*pdb_method)->update_sam_account =  odssam_update_sam_account;

	ods_state = talloc_zero(pdb_context->mem_ctx, sizeof(struct odssam_privates));

	if (!ods_state) {
		DEBUG(0, ("talloc() failed for opendirectorysam private_data!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ods_state->event_id = smb_register_exit_event(odssam_free_private_data, (void*)ods_state);

	if (ods_state->event_id == SMB_EVENT_ID_INVALID) {
		DEBUG(0,("Failed to register opendirectorysam exit event!\n"));
		return NT_STATUS_INVALID_HANDLE;
	}

    ods_state->usersArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks); 

	(*pdb_method)->private_data = ods_state;
	(*pdb_method)->free_private_data = odssam_free_private_data;

	odssam_debug_level = debug_add_class("opendirectorysam");
	if (location)
            ods_state->odssam_location = talloc_strdup(pdb_context->mem_ctx, location);

	return NT_STATUS_OK;
}

NTSTATUS init_module(void) {
	return smb_register_passdb(PASSDB_INTERFACE_VERSION, "opendirectorysam", odssam_init);
}
