
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
#ifdef USES_KEYCHAIN
#include <Security/Security.h>
#endif

#include "includes.h"

//#ifdef WITH_ODS_SAM
#define USE_SETATTRIBUTEVALUE 1
#define AUTH_GET_POLICY 0
#define GLOBAL_DEFAULT 1
#define WITH_PASSWORD_HASH 1

static int odssam_debug_level = DBGC_ALL;

#undef DBGC_CLASS
#define DBGC_CLASS odssam_debug_level

#include <DirectoryService/DirectoryService.h>
#include <CoreFoundation/CoreFoundation.h>

struct odssam_privates {
    tDirReference	dirRef;
    tDirNodeReference	searchNodeRef;
    tDirNodeReference	localNodeRef;
    const char *odssam_location;
	/* saved state from last search */
	CFMutableArrayRef usersArray;
	CFMutableArrayRef groupsArray;
	CFMutableDictionaryRef domainInfo;
	int usersIndex;
	int groupsIndex;
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

#define credentialfile "/var/db/samba/opendirectorysam"

#define sig 'odsa'

typedef struct opendirectory_secret_header {
    u_int32_t signature;
    u_int32_t authenticator_len;
    u_int32_t secret_len;
    u_int32_t authenticatorid_len;
} opendirectory_secret_header;

#ifdef USES_KEYCHAIN
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
//#include <CoreServices/CoreServices.h>

#define KEYCHAINSERVICE	"com.apple.samba"
#define SAMBA_APP_ID CFSTR("com.apple.samba")

char *odssam_get_account()
{
	CFPropertyListRef		pref = NULL;
	char			*account = NULL;
	int				accountLength = 1024;
		
	if ((pref = CFPreferencesCopyAppValue (CFSTR("DomainAdmin"), SAMBA_APP_ID)) != 0) {
		if (CFGetTypeID(pref) == CFStringGetTypeID()) {
			account = calloc(1, accountLength);
			if (!CFStringGetCString( (CFStringRef)pref, account, accountLength, kCFStringEncodingUTF8 )) {
				free(account);
				account = NULL;
			}
        }
        CFRelease(pref);
	}
    
    DEBUG(10,("oddsam_get_account (%s)\n",account));		
	return account;
}


int add_to_sambaplist (char *acctName)
{
	CFStringRef			acctNameRef = NULL;
	int				err = 0;
	
	acctNameRef = CFStringCreateWithCString(NULL, acctName, kCFStringEncodingUTF8);

	if (acctNameRef) {
		CFPreferencesSetValue(CFSTR("DomainAdmin"), acctNameRef, SAMBA_APP_ID, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
		CFPreferencesSynchronize(SAMBA_APP_ID, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
		CFRelease(acctNameRef);
	} else {
		err = -1;
	}
	
    DEBUG(10,("[%d]add_to_smbserverplist\n",err));		
	return err;
}

SecAccessRef make_uid_access(uid_t uid)
{
	OSStatus	status;
	// make the "uid/gid" ACL subject
	// this is a CSSM_LIST_ELEMENT chain
	CSSM_ACL_PROCESS_SUBJECT_SELECTOR selector = {
		CSSM_ACL_PROCESS_SELECTOR_CURRENT_VERSION,	// selector version
		CSSM_ACL_MATCH_UID,	// set mask: match uids (only)
		uid,				// uid to match
		0					// gid (not matched here)
	};
	CSSM_LIST_ELEMENT subject2 = { NULL, 0 };
	subject2.Element.Word.Data = (UInt8 *)&selector;
	subject2.Element.Word.Length = sizeof(selector);
	CSSM_LIST_ELEMENT subject1 = {
		&subject2, CSSM_ACL_SUBJECT_TYPE_PROCESS, CSSM_LIST_ELEMENT_WORDID
	};

	// rights granted (replace with individual list if desired)
	CSSM_ACL_AUTHORIZATION_TAG rights[] = {
		CSSM_ACL_AUTHORIZATION_ANY	// everything
	};
	// owner component (right to change ACL)
	CSSM_ACL_OWNER_PROTOTYPE owner = {
		// TypedSubject
		{ CSSM_LIST_TYPE_UNKNOWN, &subject1, &subject2 },
		// Delegate
		false
	};
	// ACL entries (any number, just one here)
	CSSM_ACL_ENTRY_INFO acls[] = {
		{
			// prototype
			{
				// TypedSubject
				{ CSSM_LIST_TYPE_UNKNOWN, &subject1, &subject2 },
				false,	// Delegate
				// rights for this entry
				{ sizeof(rights) / sizeof(rights[0]), rights },
				// rest is defaulted
			}
		}
	};

	SecAccessRef secaccess;
	status = SecAccessCreateFromOwnerAndACL(&owner,
		sizeof(acls) / sizeof(acls[0]), acls, &secaccess);
    DEBUG(10,("[%ld]SecAccessCreateFromOwnerAndACL\n",status));		
	return secaccess;
}

void *get_password_from_keychain(char *account, int accountLength)
{
	OSStatus status ;
	SecKeychainItemRef item;
	void *passwordData = NULL;
	UInt32 passwordLength = 0;
	void *password = NULL;	

	// Set the domain to System (daemon)
	status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
     DEBUG(10,("[%ld]SecKeychainSetPreferenceDomain \n",status));		
	status = SecKeychainGetUserInteractionAllowed (False);
     DEBUG(10,("[%ld]SecKeychainGetUserInteractionAllowed \n",status));		

	 status = SecKeychainFindGenericPassword (
					 NULL,           // default keychain
					 strlen(KEYCHAINSERVICE),             // length of service name
					 KEYCHAINSERVICE,   // service name
					 accountLength,             // length of account name
					 account,   // account name
					 &passwordLength,  // length of password
					 &passwordData,   // pointer to password data
					 &item         // the item reference
					);
     DEBUG(10,("[%ld]SecKeychainFindGenericPassword \n",status));		

	if ((status == noErr) && (item != NULL) && passwordLength)
	{
		password = calloc(1, passwordLength + 1);
		memcpy(password, (const void *)passwordData, passwordLength);
	}
	return password;
}

#endif

opendirectory_secret_header *get_odssam_secret_header(void *authenticator)
{
	opendirectory_secret_header *hdr;
	
	if (authenticator) {
		hdr = (opendirectory_secret_header *)authenticator;
		if (hdr->signature == sig) {
			return hdr;
		}
	}
	return NULL;
}

/* Public */

void  *get_odssam_authenticator()
{
	void *authenticator = NULL;
	int authentriessize = 0;
	int initialized = 0;
#ifdef USES_KEYCHAIN
	opendirectory_secret_header *odhdr = NULL;
	char *password = NULL;
	char *account = NULL;
	
	account = odssam_get_account();
	if (account) {
		password = get_password_from_keychain(account, strlen(account));
		
		if (password) {
			authentriessize = strlen(account) + strlen(password);
			authenticator = calloc(1,sizeof(opendirectory_secret_header) + authentriessize);
			memcpy((uint8*)authenticator + sizeof(opendirectory_secret_header) , account, strlen(account));
			memcpy((uint8*)authenticator + sizeof(opendirectory_secret_header) + strlen(account), password, strlen(password));
			odhdr = (opendirectory_secret_header*)authenticator;
			odhdr->authenticator_len = strlen(account);
			odhdr->secret_len = strlen(password);
			odhdr->signature = sig;
			initialized = 1;
			
			free(password);
		}
		free(account);
	}
#else
	int fd = 0;
	opendirectory_secret_header hdr;
	
	fd = open(credentialfile, O_RDONLY,0);
	if (fd != -1) {
		
		if(read(fd, &hdr, sizeof(opendirectory_secret_header)) != sizeof(opendirectory_secret_header)) {
			DEBUG(10,("get_odssam_authenticator: bad hdr(%ld)\n", sizeof(opendirectory_secret_header)));
			goto cleanup;
		}
		if (hdr.signature != sig) {
			DEBUG(10,("get_odssam_authenticator: bad signature(%X)\n", hdr.signature));
			goto cleanup;
		}
		authentriessize = hdr.authenticator_len + hdr.secret_len;
		authenticator = malloc(sizeof(opendirectory_secret_header) + authentriessize);
		memset(authenticator, 0, sizeof(opendirectory_secret_header) + authentriessize);
		memcpy(authenticator, &hdr, sizeof(opendirectory_secret_header));
		if(read(fd, authenticator + sizeof(opendirectory_secret_header), authentriessize) != authentriessize) {
			DEBUG(10,("get_odssam_authenticator: bad authentriessize(%d)\n", authentriessize));
			goto cleanup;
		}
		initialized = 1;
	} else {
		DEBUG(10,("unable to open file (%s)\n",strerror(errno)));
	}
cleanup:
	if (fd)
		close(fd);
#endif
	if (!initialized) {
		if (authenticator)
			free(authenticator);
		return NULL;
	} else 
		return authenticator;
}


u_int32_t get_odssam_authenticator_accountlen(void *authenticator)
{
	opendirectory_secret_header *hdr = get_odssam_secret_header(authenticator);
	u_int32_t len = 0;
	
	if (hdr)
		len = hdr->authenticator_len;
		
	return len;
}

void *get_odssam_authenticator_account(void *authenticator)
{
	opendirectory_secret_header *hdr = get_odssam_secret_header(authenticator);
	void *result = NULL;

	if (hdr)
		result = (uint8*)authenticator + sizeof(opendirectory_secret_header);
		
	return result;
}

u_int32_t get_odssam_authenticator_secretlen(void *authenticator)
{
	opendirectory_secret_header *hdr = get_odssam_secret_header(authenticator);
	u_int32_t len = 0;
	
	if (hdr)
		len = hdr->secret_len;
		
	return len;

}
void *get_odssam_authenticator_secret(void *authenticator)
{
	opendirectory_secret_header *hdr = get_odssam_secret_header(authenticator);
	void *result = NULL;
	
	if (hdr)
		result = (uint8*)authenticator + sizeof(opendirectory_secret_header) + hdr->authenticator_len;

	return result;
}

void delete_odssam_authenticator(void*authenticator)
{
	opendirectory_secret_header *hdr = get_odssam_secret_header(authenticator);
	
	if (hdr) {
		bzero(authenticator, sizeof(opendirectory_secret_header) + hdr->authenticator_len + hdr->secret_len);
		free(authenticator);
	}
}

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

    if (ods_state->dirRef != 0) {
    	dirStatus = dsVerifyDirRefNum(ods_state->dirRef);
    	if (dirStatus == eDSNoErr) {
    		return eDSNoErr;
    	} else {
    		ods_state->dirRef = 0;
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
										
#if defined(kDS1AttrSMBSID) && defined(kDS1AttrSMBPrimaryGroupSID)
										kDS1AttrSMBSID,
										kDS1AttrSMBPrimaryGroupSID,
#endif
										kDS1AttrSMBPWDLastSet,
										kDS1AttrSMBLogonTime,
										kDS1AttrSMBLogoffTime,
										kDS1AttrSMBKickoffTime,
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

    if (ods_state->dirRef != 0) {
		dirStatus = dsCloseDirService(ods_state->dirRef);
        DEBUG(4,("odssam_close: [%d]dsCloseDirService\n",dirStatus));		
		ods_state->dirRef = 0;
    }
    return dirStatus;
}

static tDirStatus odssam_open_search_node(struct odssam_privates *ods_state)
{
    tDirStatus			status			= eDSInvalidReference;
    long			bufferSize		= 1024 * 10;
    unsigned long			returnCount		= 0;
    tDataBufferPtr		nodeBuffer		= NULL;
    tDataListPtr		searchNodeName		= NULL;

    if (status != eDSNoErr)
        status = odssam_open(ods_state);
    if (status != eDSNoErr) goto cleanup;

    if (ods_state->searchNodeRef == 0) {
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
    if (ods_state->searchNodeRef != 0) {
		dirStatus = dsCloseDirNode(ods_state->searchNodeRef);
		DEBUG(4,("odssam_close_search_node: [%d]dsCloseDirNode\n",dirStatus));		
		 ods_state->searchNodeRef = 0;
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
    if (nodeReference != NULL && *nodeReference != 0) {
		dirStatus = dsCloseDirNode(*nodeReference);
		 DEBUG(4,("odssam_close_node: [%d]dsCloseDirNode\n",dirStatus));
		*nodeReference = 0;
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
                                        DEBUG(4,("kDSStdAuthGetEffectivePolicy was successful for user  \"%s\" :)\n", userid));
                                        memcpy(&len, stepBuff->fBufferData, 4);
                    					stepBuff->fBufferData[len+4] = '\0';
                    					safe_strcpy(policy,stepBuff->fBufferData+4, 1024);
                                        DEBUG(4,("kDSStdAuthGetEffectivePolicy policy  \"%s\" :)\n", policy));

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
									DEBUG(4,("kDSStdAuthSetPolicyAsRoot was successful for user  \"%s\" :)\n", userid));
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
	authenticator = get_odssam_authenticator();
	unbecome_root();
	
	if (authenticator == NULL || 
		get_odssam_authenticator_accountlen(authenticator) == 0 || 
		get_odssam_authenticator_secretlen(authenticator) == 0) {
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
				add_data_buffer_item(authBuff, get_odssam_authenticator_accountlen(authenticator), get_odssam_authenticator_account(authenticator));
				// Password (authenticator password)
				add_data_buffer_item(authBuff, get_odssam_authenticator_secretlen(authenticator), get_odssam_authenticator_secret(authenticator));

				status = dsDoDirNodeAuth( userNode, authType, False, authBuff, stepBuff, NULL);
				DEBUG(4,("[%d]dsDoDirNodeAuthOnRecordType kDSStdAuthNodeNativeClearTextOK \n", status));
			}
		} else {
				DEBUG(0,("authenticate_node: *** dsDataBufferAllocate(2) faild with \n" ));
		}
	}
	else
	{
			DEBUG(0,("*** dsDataBufferAllocate(1) faild with \n" ));
	}

	delete_odssam_authenticator(authenticator);
	delete_data_buffer(ods_state, authBuff);
	delete_data_buffer(ods_state, stepBuff);
	delete_data_node(ods_state, authType);
	
	return status;


}

static tDirStatus set_password(struct odssam_privates *ods_state, tDirNodeReference userNode, char* user, char *passwordstring, char *passwordType, char *type)
{
	tDirStatus 		status			= eDSNullParameter;
	unsigned long	bufferSize		= 1024 * 10;
	tDataBufferPtr	authBuff  		= NULL;
	tDataBufferPtr	stepBuff  		= NULL;
	tDataNodePtr	authType		= NULL;
	tDataNodePtr	recordType		= NULL;
	char			*password		= NULL;
	unsigned long	passwordLen		= 0;
#if defined(kDSStdAuthSetWorkstationPasswd) && defined(kDSStdAuthSetLMHash)
	uint8			binarypwd[NT_HASH_LEN];
#endif

	if (strcmp(passwordType, kDSStdAuthSetPasswdAsRoot) == 0) {
		password = passwordstring;
		passwordLen = strlen( password );
#if defined(kDSStdAuthSetWorkstationPasswd) && defined(kDSStdAuthSetLMHash)
	} else if (strcmp(passwordType, kDSStdAuthSetWorkstationPasswd) == 0 || strcmp(passwordType, kDSStdAuthSetLMHash) == 0) {
		if (pdb_gethexpwd(passwordstring, binarypwd)) {
			password = (char*)binarypwd;
			passwordLen = NT_HASH_LEN;
		}
#endif
	} else {
		return status;
	}
	
	authBuff = dsDataBufferAllocate( ods_state->dirRef, bufferSize );
	if ( authBuff != NULL  && password && passwordLen)
	{
			stepBuff = dsDataBufferAllocate( ods_state->dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( ods_state->dirRef,  passwordType);
					 recordType = dsDataNodeAllocateString( ods_state->dirRef,  type);
				   if ( authType != NULL )
					{
							// User Name
							add_data_buffer_item(authBuff, strlen( user ), user);
							// Password
							add_data_buffer_item(authBuff, passwordLen, password);
							DEBUG(4,("set_password len (%ld), password (%s) \n", passwordLen, password));

							status = dsDoDirNodeAuthOnRecordType( userNode, authType, True, authBuff, stepBuff, NULL, recordType );
							if ( status == eDSNoErr )
							{
									DEBUG(4,("Set password (%s) was successful for account  \"%s\" accountType (%s) :)\n", passwordType, user, type));
							}
							else
							{
									DEBUG(0,("Set password (%s) FAILED for account \"%s\" accountType (%s) (%d) :(\n", passwordType, user, type, status) );
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
			newAttributeValueEntry = dsAllocAttributeValueEntry(ods_state->dirRef, currentAttributeValueEntry->fAttributeValueID, value, strlen(value));	
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

tDirStatus get_records(struct odssam_privates *ods_state, CFMutableArrayRef recordsArray, tDirNodeReference nodeRef, tDataListPtr recordName, tDataListPtr recordType, tDataListPtr attributes, BOOL continueSearch)
{
	tDirStatus		status				=	eDSNoErr;
    tDirNodeReference	theNodeReference = 0;
	unsigned long		bufferSize			=	10 * 1024;
	tDataBufferPtr		dataBuffer			=	NULL;

	unsigned long		recordCount			=	0;
	unsigned long		recordIndex			=	0;
	tRecordEntryPtr		recordEntry			=	NULL;

//	tContextData 		localContextData;
	tContextData		*currentContextData = NULL;
	unsigned long		attributeIndex			=	0;
	tAttributeListRef	attributeList			= 	0;
	tAttributeEntryPtr 	attributeEntry			=	NULL;
	
	unsigned long		valueIndex			=	0;
	tAttributeValueEntryPtr valueEntry			=	NULL;
	tAttributeValueListRef 	valueList			= 	0;
	CFStringRef key = NULL;
	CFStringRef value = NULL;
	
	if (0 == ods_state->dirRef )
	    return status;    

	if (nodeRef == 0)
		theNodeReference = ods_state->searchNodeRef;
	else 
		theNodeReference = nodeRef;
	if (continueSearch)
		currentContextData = &ods_state->localContextData;
	else
		currentContextData = &ods_state->contextData;
	
	dataBuffer = dsDataBufferAllocate(ods_state->dirRef, bufferSize);
	do {
        status = dsGetRecordList(theNodeReference, dataBuffer, recordName, eDSiExact, recordType, attributes, false, &recordCount, currentContextData);
		if (status != eDSNoErr) {
             DEBUG(0,("dsGetRecordList error (%d)",status));
		    break; 
		}
		
        DEBUG(5,("get_records recordCount (%lu)\n",recordCount));
	
		for (recordIndex = 1; recordIndex <= recordCount; recordIndex++) {
			status = dsGetRecordEntry(theNodeReference, dataBuffer, recordIndex, &attributeList, &recordEntry);
			if (status != eDSNoErr) {
             	DEBUG(0,("dsGetRecordEntry error (%d)",status));
			    break; 
			}
			CFMutableDictionaryRef dsrecord = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			for (attributeIndex = 1; attributeIndex <= recordEntry->fRecordAttributeCount; attributeIndex++) {
				status = dsGetAttributeEntry(theNodeReference, dataBuffer, attributeList, attributeIndex, &valueList, &attributeEntry);
				if (status != eDSNoErr) {
             		DEBUG(0,("dsGetAttributeEntry error (%d)",status));
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
					status = dsGetAttributeValue(theNodeReference, dataBuffer, valueIndex, valueList, &valueEntry);
					if (status != eDSNoErr) {
             			DEBUG(0,("dsGetAttributeValue error (%d)",status));
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
             			DEBUG(4,("\tget_records value(%s) len(%lu)\n",valueEntry->fAttributeValueData.fBufferData, valueEntry->fAttributeValueData.fBufferLength));
						CFArrayAppendValue(valueArray, value);
						CFRelease(value);
					}

					dsDeallocAttributeValueEntry(theNodeReference, valueEntry);
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
				valueList = 0;
				dsDeallocAttributeEntry(theNodeReference, attributeEntry);
				attributeEntry = NULL;
			}
			dsCloseAttributeList(attributeList);
			attributeList = 0;
			dsDeallocRecordEntry(theNodeReference, recordEntry);
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
	tAttributeListRef	attributeList			= 	0;
	tAttributeEntryPtr 	attributeEntry			=	NULL;
	
	unsigned long		valueIndex			=	0;
	tAttributeValueEntryPtr valueEntry			=	NULL;
	tAttributeValueListRef 	valueList			= 	0;
	CFStringRef key = NULL;
	CFStringRef value = NULL;
	CFMutableDictionaryRef dsrecord = NULL;
	CFMutableArrayRef valueArray = NULL;
	
	if (0 == ods_state->dirRef || 0 == ods_state->searchNodeRef)
	    return status;    

	if (continueSearch)
		currentContextData = &ods_state->localContextData;
	else
		currentContextData = &ods_state->contextData;
	
	dataBuffer = dsDataBufferAllocate(ods_state->dirRef, bufferSize);
	do {
		status = dsDoAttributeValueSearchWithData(ods_state->searchNodeRef, dataBuffer, recordType, searchType, eDSExact, searchValue,  ods_state->samAttributes, false, &recordCount, currentContextData);
		if (status != eDSNoErr) {
             DEBUG(0,("dsDoAttributeValueSearchWithData error (%d)",status));
		    break; 
		}

		for (recordIndex = 1; recordIndex <= recordCount; recordIndex++) {
			status = dsGetRecordEntry(ods_state->searchNodeRef, dataBuffer, recordIndex, &attributeList, &recordEntry);
			if (status != eDSNoErr) {
             	DEBUG(0,("dsGetRecordEntry error (%d)",status));
			    break; 
			}
			dsrecord = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			for (attributeIndex = 1; attributeIndex <= recordEntry->fRecordAttributeCount; attributeIndex++) {
				status = dsGetAttributeEntry(ods_state->searchNodeRef, dataBuffer, attributeList, attributeIndex, &valueList, &attributeEntry);
				if (status != eDSNoErr) {
             		DEBUG(0,("dsGetAttributeEntry error (%d)",status));
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
             	DEBUG(4,("search_records key(%s)\n",attributeEntry->fAttributeSignature.fBufferData));
				for (valueIndex = 1; valueIndex <= attributeEntry->fAttributeValueCount; valueIndex++) {
					status = dsGetAttributeValue(ods_state->searchNodeRef, dataBuffer, valueIndex, valueList, &valueEntry);
					if (status != eDSNoErr) {
             			DEBUG(0,("dsGetAttributeValue error (%d)",status));
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
             			DEBUG(4,("\tsearch_records value(%s)\n",valueEntry->fAttributeValueData.fBufferData));
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
					valueList = 0;
				}
				if (attributeEntry) {
					dsDeallocAttributeEntry(ods_state->searchNodeRef, attributeEntry);
					attributeEntry = NULL;
				}
			}
			if (attributeList) {
				dsCloseAttributeList(attributeList);
				attributeList = 0;
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
	DEBUG (4, ("[%d]get_sam_record_by_attr type(%s), attr(%s) value (%s)\n",status, type, attr, value));
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
                status = get_records(ods_state, recordsArray, NULL, recordName, recordType, ods_state->samAttributes, continueSearch);
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
	CFTypeID	object_type;
	void		*opaque_value = NULL;
	char	buffer[PSTRING_LEN];
	BOOL	result = False;
	
	attrRef = CFStringCreateWithCString(NULL, attribute, kCFStringEncodingUTF8);
	opaque_value = CFDictionaryGetValue(entry, attrRef);
	
	if (opaque_value) {
		object_type = CFGetTypeID(opaque_value);
		if (object_type == CFArrayGetTypeID()) {
			valueList = (CFArrayRef)opaque_value;
		} else if (object_type == CFStringGetTypeID()) {
			cfstrRef = (CFStringRef)opaque_value;		
		} else {
			result =  False;
			goto cleanup;
		}
	} else {
		result =  False;
		goto cleanup;
	}
		
	if (valueList != NULL && CFArrayGetCount(valueList) != 0) {
		cfstrRef = (CFStringRef)CFArrayGetValueAtIndex(valueList, 0);
		if (!GetCString(cfstrRef, buffer, PSTRING_LEN)) {
			value = NULL;
			DEBUG (3, ("get_single_attribute: [%s] = [CFArrayRef <does not exist>]\n", attribute));
		} else {
			pstrcpy(value, buffer);
			result = True;
		}
	} else if (cfstrRef) {
		if (!GetCString(cfstrRef, buffer, PSTRING_LEN)) {
			value = NULL;
			DEBUG (3, ("get_single_attribute: [%s] = [CFStringRef <does not exist>]\n", attribute));
		} else {
			pstrcpy(value, buffer);
			result = True;
		}	
	}
	
#ifdef DEBUG_PASSWORDS
	DEBUG (0, ("get_single_attribute: [%s] = [%s]\n", attribute, value));
#endif
cleanup:
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
	tDirNodeReference nodeReference = 0;
	tRecordReference recordReference = 0;
	char policy[1024];
	
	if (get_attributevalue_list(entry, kDSNAttrAuthenticationAuthority, &authAuthorities) && 
		parse_password_server(authAuthorities, pwsServerIP, userid)) {
		
		if (!get_single_attribute(entry, kDSNAttrMetaNodeLocation, dirNode)) {
			status =  eDSInvalidNodeRef;
		} else {
			status = odssam_open_node(ods_state, dirNode, &nodeReference);
			if (eDSNoErr != status) goto cleanup;
#if AUTH_GET_POLICY
			status = odssam_authenticate_node(ods_state, nodeReference);
			if (eDSNoErr != status) goto cleanup;
#endif	
			if (eDSNoErr == status) {
				recordType = get_record_type((const char *)userName);
				status = get_record_ref(ods_state, nodeReference, &recordReference, recordType, userName);
			}
		}
		if (eDSNoErr == status && nodeReference != 0) {
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

static BOOL oddsam_get_record_sid_string(struct odssam_privates *ods_state,
											SAM_ACCOUNT * sampass,
											CFMutableDictionaryRef entry, uint32 rid, fstring record_sid_string)
{
	tDirStatus status = eDSNoErr;
	DOM_SID u_sid;
	pstring nodelocation;
	fstring xml_string;
	fstring domain_sid_string;
	BOOL	is_server_sid = True;
	tDirNodeReference nodeReference = 0;
    tDataListPtr recordName = NULL;
    tDataListPtr recordType = NULL;
	tDataListPtr attributes = NULL;
    CFMutableArrayRef recordsArray = NULL;
	CFMutableDictionaryRef configRecord = NULL;
	CFDataRef xmlData = NULL;
	CFMutableDictionaryRef xmlDict = NULL;
	CFStringRef nodelocationRef = NULL;
	CFDictionaryRef domainInfo = NULL;
	DOM_SID server_sid;
	
	if (!get_single_attribute(entry, kDSNAttrMetaNodeLocation, nodelocation))
		return False;

	DEBUG (5, ("oddsam_get_record_sid_string: kDSNAttrMetaNodeLocation [%s]\n", nodelocation));

 	if (strcmp(nodelocation, "/NetInfo/DefaultLocalNode") == 0) { // Local Node - All Users are relative to the server sid
 		is_server_sid = True;
 	} else {
 		nodelocationRef = CFStringCreateWithCString(NULL, nodelocation, kCFStringEncodingUTF8);
		if (ods_state->domainInfo && nodelocationRef && (domainInfo = CFDictionaryGetValue(ods_state->domainInfo, nodelocationRef))) {
			if (get_single_attribute(domainInfo, "SID", domain_sid_string)) {
				if (string_to_sid(&server_sid, domain_sid_string)) {
					if(sid_append_rid(&server_sid, rid)) {
						if (sid_to_string(record_sid_string, &server_sid)) {
							is_server_sid = False;
							DEBUG (5, ("oddsam_get_record_sid_string: server_sid_string<CACHED> [%s]\n", record_sid_string));
						} /* sid_to_string */
					} /* sid_append_rid */
				} /* string_to_sid */
			}
		} else {
			status = odssam_open_node(ods_state, nodelocation, &nodeReference);
			if (eDSNoErr != status) goto cleanup;
			
			recordType = dsBuildListFromStrings(ods_state->dirRef, kDSStdRecordTypeConfig, NULL); 
			recordName = dsBuildListFromStrings(ods_state->dirRef, "CIFSServer", NULL);     
			attributes = dsBuildListFromStrings(ods_state->dirRef, kDS1AttrXMLPlist , NULL);
			
			recordsArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			
			if (recordName && recordType && attributes && recordsArray) {
				status = get_records(ods_state, recordsArray, nodeReference, recordName, recordType, attributes, true);
				if (eDSNoErr == status) {
					if (CFArrayGetCount(recordsArray) != 0) {
						configRecord = (CFDictionaryRef) CFArrayGetValueAtIndex(recordsArray, 0);
						if (get_single_attribute(configRecord, kDS1AttrXMLPlist, xml_string)) {
							// XML -> CFDict
							xmlData = CFDataCreate(NULL, (UInt8*)xml_string, strlen(xml_string));
							if (xmlData) {
								xmlDict = (CFMutableDictionaryRef)CFPropertyListCreateFromXMLData(NULL, xmlData, kCFPropertyListMutableContainersAndLeaves, NULL);
								if (xmlDict) {
									if (get_single_attribute(xmlDict, "SID", domain_sid_string)) {
										if (nodelocationRef)
											CFDictionaryAddValue(ods_state->domainInfo, nodelocationRef, xmlDict); /* store current setting */
										else 
											DEBUG (5, ("oddsam_get_record_sid_string: NULL parameter (nodelocationRef) - could not store SID  \n"));
										if (string_to_sid(&server_sid, domain_sid_string)) {
											if(sid_append_rid(&server_sid, rid)) {
												if (sid_to_string(record_sid_string, &server_sid)) {
													is_server_sid = False;
													DEBUG (5, ("oddsam_get_record_sid_string: server_sid_string [%s]\n", record_sid_string));
												} /* sid_to_string */
											} /* sid_append_rid */
										} /* string_to_sid */
									} /* get_single_attribute - SID */
								} /* xmlDict */
							} /* xmlData */
						} /* get_single_attribute - kDS1AttrXMLPlist */	
					} else {
						DEBUG (5, ("oddsam_get_record_sid_string: [CFArrayGetCount = 0 <does not exist>]\n"));
					}
				} else {
					DEBUG (5, ("[%d]oddsam_get_record_sid_string: [<does not exist>]\n",status));
				}
			}
		}
	}

cleanup:
	if (recordName)
		delete_data_list(ods_state, recordName);
	if (recordType)
		delete_data_list(ods_state, recordType);
	if (attributes)
		delete_data_list(ods_state, attributes);
	if (nodeReference)
		odssam_close_node(ods_state, &nodeReference);
	if (xmlData)
		CFRelease(xmlData);
	if (xmlDict)
		CFRelease(xmlDict);
	if (recordsArray)
		CFRelease(recordsArray);
	if (nodelocationRef)
		CFRelease(nodelocationRef);
		
	if (is_server_sid == True) {
		sid_copy(&u_sid, get_global_sam_sid());
		sid_append_rid(&u_sid, rid);
		sid_to_string(record_sid_string, &u_sid);
	}
	
	return True;
}

tDirStatus add_record_attributes(struct odssam_privates *ods_state, CFDictionaryRef samAttributes, CFDictionaryRef userCurrent, char *the_record_type)
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
	
	tDirNodeReference nodeReference = 0;
	tRecordReference recordReference = 0;
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
				if (the_record_type)
					recordType = the_record_type;
				else	
					recordType = get_record_type((const char *)userName);
				status = get_record_ref(ods_state, nodeReference, &recordReference, recordType, userName);
			}
		} else {
			DEBUG (0, ("[%d]add_record_attributes: odssam_open_node error\n",status));
			goto cleanup;
		}
	}
	
	if (eDSNoErr == status) {
		for (attributeIndex = 0, CFDictionaryGetKeysAndValues(samAttributes, (const void**)keys, (const void**)values); attributeIndex < count; attributeIndex++) {
			isKey = CFStringGetCString(keys[attributeIndex], key, sizeof(key), kCFStringEncodingUTF8);
			isValue = CFStringGetCString(values[attributeIndex], value, sizeof(value), kCFStringEncodingUTF8);

			if (get_single_attribute(userCurrent,key, temp))
				addValue = false;
			else
				addValue = true;

			if (isKey && isValue) {
				if (strcmp(key, kDSStdAuthSetPasswdAsRoot) == 0 
#if defined(kDSStdAuthSetWorkstationPasswd) && defined(kDSStdAuthSetLMHash)
				|| strcmp(key, kDSStdAuthSetWorkstationPasswd) == 0 || strcmp(key, kDSStdAuthSetLMHash) == 0 
#endif					
				) {
					status = set_password(ods_state, nodeReference, userName, value, key, recordType);
				#ifdef DEBUG_PASSWORDS
					DEBUG (100, ("add_record_attributes: [%d]SetPassword(%s, %s, %s, %s)\n",status, userName, key, value, recordType));
				else
					DEBUG (100, ("add_record_attributes: [%d]SetPassword(%s, %s, %s)\n",status, userName, key, recordType));
				#endif
				} else if (strcmp(key, kDSNAttrRecordName) == 0) {
					
					if (strcmp(userName, value) != 0) {
						status = set_recordname(ods_state, recordReference, value);
						DEBUG (3, ("add_record_attributes: [%d]set_recordname(%s, %s, %s)\n",status, userName, key, value));
					}
				} else if (isPWSAttribute(key)) {
					add_password_policy_attribute(policy, key, value);
				} else {
					status = add_attribute_with_value(ods_state, recordReference, key, value, addValue);
					DEBUG (4, ("[%d]add_record_attributes: add_attribute_with_value(%s,%s,%s) error\n",status, userName, key, value));
				}
				if (status != eDSNoErr)
					break;
			}
		}
		if (strlen(policy) > 0)
			status =  set_password_policy(ods_state, nodeReference, userName, policy, recordType);
	} else {
		DEBUG (0, ("[%d]add_record_attributes: authenticate_node error\n",status));
	}
cleanup:
	if (0 != recordReference)
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
	uint16 		acct_ctrl = 0, 
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
	DEBUG(4, ("Entry found for user: %s\n", username));
	
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

#if defined(kDS1AttrSMBSID)
	if (get_single_attribute(entry, kDS1AttrSMBSID, temp)) {
		DEBUG(3, ("init_sam_from_ods: kDS1AttrSMBSID (%s)\n", temp));
		pdb_set_user_sid_from_string(sampass, temp, PDB_SET);
	} else 
#endif
	if (get_single_attribute(entry, kDS1AttrSMBRID, temp)) {
		DEBUG(3, ("init_sam_from_ods: kDS1AttrSMBRID (%s)\n", temp));
		user_rid = (uint32)atol(temp);
		if(oddsam_get_record_sid_string(ods_state, sampass, entry, user_rid, temp))
			pdb_set_user_sid_from_string(sampass, temp, PDB_SET);
	} else if (get_single_attribute(entry, kDS1AttrUniqueID, temp)) {
		if (uid == 99)
			user_rid = DOMAIN_USER_RID_GUEST;
		else
			user_rid = algorithmic_pdb_uid_to_user_rid((uint32)uid);
		DEBUG(3, ("init_sam_from_ods: use kDS1AttrUniqueID (%s) -> RID(%d)\n", temp, user_rid));
		if(oddsam_get_record_sid_string(ods_state, sampass, entry, user_rid, temp))
			pdb_set_user_sid_from_string(sampass, temp, PDB_SET);
	} else {
		DEBUG(3, ("init_sam_from_ods: kDS1AttrSMBRID mapped to DOMAIN_USER_RID_GUEST\n"));
		user_rid = DOMAIN_USER_RID_GUEST;
		if(oddsam_get_record_sid_string(ods_state, sampass, entry, user_rid, temp))
			pdb_set_user_sid_from_string(sampass, temp, PDB_SET);
	}
		

#if defined(kDS1AttrSMBPrimaryGroupSID)
	if (get_single_attribute(entry, kDS1AttrSMBPrimaryGroupSID, temp)) {
		DEBUG(3, ("init_sam_from_ods: kDS1AttrSMBPrimaryGroupSID (%s)\n", temp));
		pdb_set_group_sid_from_string(sampass, temp, PDB_SET);			
	} else
#endif
	if (get_single_attribute(entry, kDS1AttrSMBGroupRID, temp)) {
		DEBUG(3, ("init_sam_from_ods: kDS1AttrSMBGroupRID (%s)\n", temp));
		group_rid = (uint32)atol(temp);
		if(oddsam_get_record_sid_string(ods_state, sampass, entry, group_rid, temp))
			pdb_set_group_sid_from_string(sampass, temp, PDB_SET);
	} else {
		DEBUG(3, ("init_sam_from_ods: DOMAIN_GROUP_RID_USERS \n"));
		if(oddsam_get_record_sid_string(ods_state, sampass, entry, DOMAIN_GROUP_RID_USERS, temp))
			pdb_set_group_sid_from_string(sampass, temp, PDB_SET);
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
#if 0
	if (group_rid == 0) {
		GROUP_MAP map;
		
		get_single_attribute(entry, kDS1AttrPrimaryGroupID, temp);
		gid = atol(temp);
		/* call the mapping code here */
		if(pdb_getgrgid(&map, gid)) {
			pdb_set_group_sid(sampass, &map.sid, PDB_SET);
		} 
		else {
			group_rid = pdb_gid_to_group_rid(gid);
		if(oddsam_get_record_sid_string(ods_state, sampass, entry, group_rid, temp))
			pdb_set_group_sid_from_string(sampass, temp, PDB_SET);
		}
	}
#endif

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
#ifdef GLOBAL_DEFAULT
		pdb_set_dir_drive(sampass, talloc_sub_specified(sampass->mem_ctx, 
								  lp_logon_drive(),
								  username, domain, 
								  uid, gid),
				  PDB_DEFAULT);
#else
		pdb_set_dir_drive(sampass, NULL, PDB_SET);
#endif
	} else {
		pdb_set_dir_drive(sampass, dir_drive, PDB_SET);
	}

	if (!get_single_attribute(entry, kDS1AttrSMBHome, homedir)) {
#ifdef GLOBAL_DEFAULT
		pdb_set_homedir(sampass, talloc_sub_specified(sampass->mem_ctx, 
								  lp_logon_home(),
								  username, domain, 
								  uid, gid), 
				  PDB_DEFAULT);
#else
		pdb_set_homedir(sampass, NULL, PDB_SET);
#endif
	} else {
		pdb_set_homedir(sampass, homedir, PDB_SET);
	}

	if (!get_single_attribute(entry, kDS1AttrSMBScriptPath, logon_script)) {
#ifdef GLOBAL_DEFAULT
		pdb_set_logon_script(sampass, talloc_sub_specified(sampass->mem_ctx, 
								     lp_logon_script(),
								     username, domain, 
								     uid, gid), 
				     PDB_DEFAULT);
#else
		pdb_set_logon_script(sampass, NULL, PDB_SET);
#endif
	} else {
		pdb_set_logon_script(sampass, logon_script, PDB_SET);
	}

	if (!get_single_attribute(entry, kDS1AttrSMBProfilePath, profile_path)) {
#ifdef GLOBAL_DEFAULT
		pdb_set_profile_path(sampass, talloc_sub_specified(sampass->mem_ctx, 
								     lp_logon_path(),
								     username, domain, 
								     uid, gid), 
				     PDB_DEFAULT);
#else
		pdb_set_profile_path(sampass, NULL, PDB_SET);
#endif
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
		DEBUG(4, ("Setting entry for user: %s\n", pdb_get_username(sampass)));
	}
	
	if (need_ods_mod(pdb_add, sampass, PDB_USERSID)) {
#if defined(kDS1AttrSMBSID)
		fstring sid_string;
		fstring dom_sid_string;
		const DOM_SID *user_sid = pdb_get_user_sid(sampass);
		if(NULL != user_sid) {
			if (!sid_peek_check_rid(get_global_sam_sid(), user_sid, &rid)) {
				DEBUG(0, ("init_ods_from_sam: User's SID (%s) is not for this domain (%s), cannot add to Open Directory!\n", 
					sid_to_string(sid_string, user_sid), 
					sid_to_string(dom_sid_string, get_global_sam_sid())));
				return False;
			}
			sid_to_string(sid_string, user_sid);
			DEBUG(4, ("Setting SID entry for user: %s [%s]\n", pdb_get_username(sampass), sid_string));
			make_a_mod(userEntry, kDS1AttrSMBSID, sid_string);
		} else 	
#endif
		if ((rid = pdb_get_user_rid(sampass))!=0 ) {
			slprintf(temp, sizeof(temp) - 1, "%i", rid);
			DEBUG(4, ("Setting RID entry for user: %s [%s]\n", pdb_get_username(sampass), temp));
			make_a_mod(userEntry, kDS1AttrSMBRID, temp);
		}
#ifdef STORE_ALGORITHMIC_RID
		 else if (!IS_SAM_DEFAULT(sampass, PDB_UID)) {
			rid = algorithmic_pdb_uid_to_user_rid(pdb_get_uid(sampass));
			slprintf(temp, sizeof(temp) - 1, "%i", rid);
			make_a_mod(userEntry, kDS1AttrSMBRID, temp);
		} else {
			DEBUG(0, ("NO user RID specified on account %s, cannot store!\n", pdb_get_username(sampass)));
			return False;
		}
#endif
	}

	if (need_ods_mod(pdb_add, sampass, PDB_GROUPSID)) {		
#if defined(kDS1AttrSMBPrimaryGroupSID)
		fstring sid_string;
		fstring dom_sid_string;
		const DOM_SID *group_sid = pdb_get_group_sid(sampass);


		if(NULL != group_sid) {
			if (!sid_peek_check_rid(get_global_sam_sid(), group_sid, &rid)) {
				DEBUG(1, ("init_ods_from_sam: User's Primary Group SID (%s) is not for this domain (%s), cannot add to Open Directory!\n",
					sid_to_string(sid_string, group_sid),
					sid_to_string(dom_sid_string, get_global_sam_sid())));
				return False;
			}
			sid_to_string(sid_string, group_sid);
			DEBUG(4, ("Setting Primary Group SID entry for user: %s [%s]\n", pdb_get_username(sampass), sid_string));
			make_a_mod(userEntry, kDS1AttrSMBPrimaryGroupSID, sid_string);
		} else	
#endif
		if ((rid = pdb_get_group_rid(sampass))!=0 ) {
			slprintf(temp, sizeof(temp) - 1, "%i", rid);
			make_a_mod(userEntry, kDS1AttrSMBGroupRID, temp);
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
	}

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

#if defined(kDSStdAuthSetWorkstationPasswd) && defined(kDSStdAuthSetLMHash)
		if (need_ods_mod(pdb_add, sampass, PDB_LMPASSWD)) {
			pdb_sethexpwd (temp, pdb_get_lanman_passwd(sampass), pdb_get_acct_ctrl(sampass));
			make_a_mod (userEntry, kDSStdAuthSetLMHash, temp);
		}
		
		if (need_ods_mod(pdb_add, sampass, PDB_NTPASSWD)) {
			pdb_sethexpwd (temp, pdb_get_nt_passwd(sampass), pdb_get_acct_ctrl(sampass));
			make_a_mod (userEntry, kDSStdAuthSetWorkstationPasswd, temp);
		}
#endif		
		if (need_ods_mod(pdb_add, sampass, PDB_PLAINTEXT_PW)) {
			make_a_mod (userEntry, kDSStdAuthSetPasswdAsRoot, pdb_get_plaintext_passwd(sampass));
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
	uint32 uid = 99;

	CFMutableDictionaryRef entry = NULL;
	pstring filter;
	snprintf(filter, sizeof(filter) - 1, "%i", rid);
	
	DEBUG(1,("odssam_getsampwrid: rid <%s>\n", filter));
	
	if (rid == DOMAIN_USER_RID_GUEST) {
		const char *guest_account = lp_guestaccount();
		if (!(guest_account && *guest_account)) {
			DEBUG(0, ("Guest account not specified!\n"));
			return ret;
		}
		return odssam_getsampwnam(my_methods, user, guest_account);
	}

    usersArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    
    // check kDS1AttrSMBSID
	if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeUsers, kDS1AttrSMBRID, filter, True) != eDSNoErr || (CFArrayGetCount(usersArray) == 0)) {
		if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeComputers, kDS1AttrSMBRID, filter, True) != eDSNoErr || (CFArrayGetCount(usersArray) == 0)) {
if (algorithmic_pdb_rid_is_user(rid)) {
			uid = algorithmic_pdb_user_rid_to_uid(rid);
			snprintf(filter, sizeof(filter) - 1, "%i", uid);
			DEBUG(4,("Look up by algorithmic rid using uid [%i]\n", uid));
			if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeUsers, kDS1AttrUniqueID, filter, True) != eDSNoErr || (CFArrayGetCount(usersArray) == 0)){
				if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeComputers, kDS1AttrUniqueID, filter, True) != eDSNoErr || (CFArrayGetCount(usersArray) == 0)) {
					ret = NT_STATUS_NO_SUCH_USER;
					goto cleanup;
				}
			}
} else {
			ret = NT_STATUS_NO_SUCH_USER;
			goto cleanup;
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

	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	struct odssam_privates *ods_state = (struct odssam_privates *)my_methods->private_data;
    CFMutableArrayRef usersArray = NULL;
#ifdef USE_ALGORITHMIC_RID
	uint32 uid = 99;
	pstring uid_string;
#endif
	uint32 rid;
	CFMutableDictionaryRef entry = NULL;
	pstring rid_string;
	fstring sid_string;
	
	if (!sid_peek_check_rid(get_global_sam_sid(), sid, &rid)) {
		DEBUG(4,("odssam_getsampwsid: Not a member of this domain\n"));	
	}	
	
	if (rid == DOMAIN_USER_RID_GUEST) {
		const char *guest_account = lp_guestaccount();
		if (!(guest_account && *guest_account)) {
			DEBUG(0, ("Guest account not specified!\n"));
			return ret;
		}
		return odssam_getsampwnam(my_methods, account, guest_account);
	}

    usersArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
 
 	sid_to_string(sid_string, sid);
	
	DEBUG(4,("odssam_getsampwsid: SID<%s>\n", sid_string));

	snprintf(rid_string, sizeof(rid_string) - 1, "%i", rid);
	DEBUG(4,("odssam_getsampwsid: rid<%s>\n", rid_string));

#ifdef USE_ALGORITHMIC_RID
	uid = algorithmic_pdb_user_rid_to_uid(rid);
	snprintf(uid_string, sizeof(uid_string) - 1, "%i", uid);
#endif

#if defined(kDS1AttrSMBSID)
	if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeUsers, kDS1AttrSMBSID, sid_string, True) == eDSNoErr && (CFArrayGetCount(usersArray) != 0)) {
		DEBUG(4,("odssam_getsampwsid: kDSStdRecordTypeUsers SID<%s>\n", sid_string));
	} else 
#endif
	if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeComputers, kDS1AttrSMBSID, sid_string, True) == eDSNoErr && (CFArrayGetCount(usersArray) != 0)) {
		DEBUG(4,("odssam_getsampwsid: kDSStdRecordTypeComputers SID<%s>\n", sid_string));
	} else if (get_sam_record_by_attr(ods_state, usersArray,  kDSStdRecordTypeUsers, kDS1AttrSMBRID, rid_string, True) == eDSNoErr && (CFArrayGetCount(usersArray) != 0)) {
		DEBUG(4,("odssam_getsampwsid: kDSStdRecordTypeUsers RID<%s>\n", rid_string));
	} else if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeComputers, kDS1AttrSMBRID, sid_string, True) == eDSNoErr && (CFArrayGetCount(usersArray) != 0)) {
		DEBUG(4,("odssam_getsampwsid: kDSStdRecordTypeUsers RID<%s>\n", rid_string));
	} else
#ifdef USE_ALGORITHMIC_RID
	if (algorithmic_pdb_rid_is_user(rid) &&
			((get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeUsers, kDS1AttrUniqueID, uid_string, True) == eDSNoErr) && (CFArrayGetCount(usersArray) != 0))){
		DEBUG(4,("odssam_getsampwsid: kDSStdRecordTypeUsers RID<%s> -> UID<%s>\n", rid_string, uid_string));
	} else  if (algorithmic_pdb_rid_is_user(rid) &&
			((get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeComputers, kDS1AttrUniqueID, uid_string, True) == eDSNoErr) && (CFArrayGetCount(usersArray) != 0))) {
		DEBUG(4,("odssam_getsampwsid: kDSStdRecordTypeComputers RID<%s> -> UID<%s>\n", rid_string, uid_string));
#endif
	} else {
		DEBUG(4,("odssam_getsampwsid: NT_STATUS_NO_SUCH_USER\n"));	
		ret = NT_STATUS_NO_SUCH_USER;
		goto cleanup;
	}

	entry = (CFDictionaryRef) CFArrayGetValueAtIndex(usersArray, 0);
	if (entry) {
		if (!init_sam_from_ods(ods_state, account, entry)) {
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
	dirStatus = add_record_attributes(ods_state, userMods, userCurrent, NULL);
	if (eDSNoErr != dirStatus) {
		ret = NT_STATUS_UNSUCCESSFUL;
		DEBUG(0, ("odssam_add_sam_account: [%d]add_record_attributes\n", dirStatus));
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
	tDirStatus dirStatus = eDSNoErr;
	char *recordType = NULL;
	struct odssam_privates *ods_state = (struct odssam_privates *)my_methods->private_data;
	int 		ops = 0;
    CFMutableArrayRef usersArray = NULL;
    CFMutableDictionaryRef userMods = NULL;
	
	const char *username = pdb_get_username(newpwd);
	const char *ntusername = pdb_get_nt_username(newpwd);
	const char *searchname = username;
	
	if (!username || !*username) {
		DEBUG(0, ("Cannot update a user without a username!\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}
	DEBUG(4, ("odssam_update_sam_account: username(%s) ntusername(%s)\n", username, ntusername));

	if (IS_SAM_CHANGED(newpwd, PDB_USERNAME))
		searchname = ntusername;
	else if (IS_SAM_CHANGED(newpwd, PDB_FULLNAME))
		searchname = username;
	else
		searchname = username;
		
	if (IS_SAM_CHANGED(newpwd, PDB_USERNAME) && IS_SAM_CHANGED(newpwd, PDB_FULLNAME)) {
		// record name changed - lookup by kDS1AttrSMBSID
		DEBUG(4, ("odssam_update_sam_account: PDB_USERNAME && PDB_FULLNAME MODIFIED \n"));
		return ret;
	} else {
		recordType = get_record_type((const char *)searchname);
		usersArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		if ((dirStatus = get_sam_record_attributes(ods_state, usersArray, recordType, searchname, true)) != eDSNoErr || (CFArrayGetCount(usersArray) == 0)) {
				DEBUG(0, ("odssam_add_sam_account: searchname(%s) NOT FOUND\n", searchname));
				ret = NT_STATUS_UNSUCCESSFUL;
				goto cleanup;
		}
	}
// check for SMB Attributes and bail if already added
	
#if 0 /* skip duplicates for now */ 
	if (CFArrayGetCount(usersArray) > 1) {
		DEBUG (0, ("odssam_update_sam_account: More than one user with that uid exists: bailing out!\n"));
		ret =  NT_STATUS_UNSUCCESSFUL;
		goto cleanup;
	}
#endif
	/* Check if we need to update an existing entry */

	userMods = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!init_ods_from_sam(ods_state, ops, newpwd, userMods)) {
		DEBUG(0, ("odssam_update_sam_account: init_ods_from_sam failed!\n"));
		ret =  NT_STATUS_UNSUCCESSFUL;
		goto cleanup;
	}	

	if (CFDictionaryGetCount(userMods) == 0) {
		DEBUG(0,("odssam_update_sam_account: mods is empty: nothing to add for user: %s\n",pdb_get_username(newpwd)));
		return NT_STATUS_UNSUCCESSFUL;
	}
	
	CFDictionaryRef userCurrent = (CFDictionaryRef) CFArrayGetValueAtIndex(usersArray, 0);
	dirStatus = add_record_attributes(ods_state, userMods, userCurrent, NULL);
	if (eDSNoErr != dirStatus) {
		ret = NT_STATUS_UNSUCCESSFUL;
		DEBUG(0, ("odssam_update_sam_account: [%d]add_record_attributes\n", dirStatus));
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

static BOOL init_group_from_ods(struct odssam_privates *ods_state,
				 GROUP_MAP *map, CFMutableDictionaryRef entry)
{
	pstring temp;
	
	if (ods_state == NULL || map == NULL || entry == NULL ) {
		DEBUG(0, ("init_group_from_ods: NULL parameters found!\n"));
		return False;
	}
#if defined(kDS1AttrSMBSID)
	if (get_single_attribute(entry, kDS1AttrSMBSID, temp)) {
		DEBUG(4, ("init_group_from_ods: kDS1AttrSMBSID (%s)\n", temp));
		if (!string_to_sid(&map->sid, temp)) {
			DEBUG(1, ("init_group_from_ods: SID string [%s] could not be read as a valid SID\n", temp));
			return False;
		}
	} else 
#endif	
	if (get_single_attribute(entry, kDS1AttrSMBGroupRID, temp) || get_single_attribute(entry, kDS1AttrSMBRID, temp)) {
		DEBUG(4, ("init_group_from_ods: kDS1AttrPrimaryGroupID/kDS1AttrSMBRID (%s)\n", temp));
		const DOM_SID *global_sam_sid;
		DOM_SID g_sid;
		if (!(global_sam_sid = get_global_sam_sid())) {
			DEBUG(0, ("init_group_from_ods: Could not read global sam sid!\n"));
			return False;
		}
		sid_copy(&g_sid, global_sam_sid);
		uint32 group_rid = (uint32)atol(temp);
		
		if (!sid_append_rid(&g_sid, group_rid)) {
			DEBUG(0, ("init_group_from_ods: sid_append_rid error\n")); 
			return False;
		}
		sid_copy(&map->sid, &g_sid);
	} else if (get_single_attribute(entry, kDS1AttrPrimaryGroupID, temp)) {
		uint32 gid = (uint32)atol(temp);
		algorithmic_gid_to_sid(&map->sid, gid);
	} else {
		DEBUG(0, ("init_group_from_ods: Mandatory attribute not found\n"));
		return False;
	}
	
	if (!get_single_attribute(entry, kDS1AttrPrimaryGroupID, temp)) {
		DEBUG(0, ("init_group_from_ods: Mandatory attribute %s not found\n", kDS1AttrPrimaryGroupID));
		return False;
	}
	
	map->gid = (gid_t)atol(temp);


	if (!get_single_attribute(entry, kDSNAttrMetaNodeLocation, temp)) {
		DEBUG(0, ("init_group_from_ods: Mandatory attribute %s not found\n", kDS1AttrPrimaryGroupID));
		return False;
	} else {
		if (sid_check_is_in_builtin(&(map->sid))) {
			map->sid_name_use = (enum SID_NAME_USE)SID_NAME_WKN_GRP;
		} else if (strcmp(temp, "/LDAPv3/127.0.0.1") == 0) {  //<or> AD Node
			map->sid_name_use = (enum SID_NAME_USE)SID_NAME_DOM_GRP;
		} else if (strcmp(temp, "/NetInfo/DefaultLocalNode") == 0) { 
			map->sid_name_use = (enum SID_NAME_USE)SID_NAME_ALIAS;
		} else { // misc nodes will contain local groups
			map->sid_name_use = (enum SID_NAME_USE)SID_NAME_ALIAS;
//			map->sid_name_use = (enum SID_NAME_USE)SID_NAME_UNKNOWN;
		}		
	}
	
	if (!get_single_attribute(entry, kDS1AttrDistinguishedName, temp)) { 
		DEBUG(4, ("init_group_from_ods: Attribute %s not found\n", kDS1AttrDistinguishedName));
		if(!get_single_attribute(entry, kDSNAttrRecordName, temp)) {
			DEBUG(0, ("init_group_from_ods: Mandatory attribute %s not found\n", kDSNAttrRecordName));
			return False;
		}
	}
	fstrcpy(map->nt_name, temp);

	if (!get_single_attribute(entry, kDS1AttrComment, temp)) {
		temp[0] = '\0';
	}
	fstrcpy(map->comment, temp);

	return True;
}

static void odssam_endgrpwent(struct pdb_methods *my_methods)
{
	struct odssam_privates *ods_state = (struct odssam_privates *)my_methods->private_data;

	CFArrayRemoveAllValues(ods_state->groupsArray);        
	ods_state->groupsIndex = 0;        
}

static NTSTATUS odssam_setgrpwent(struct pdb_methods *my_methods, BOOL update)
{
	odssam_endgrpwent(my_methods);
	DEBUG(0,("odssam_setgrpwent: update(%d)\n", update));

	return NT_STATUS_OK;
}

static NTSTATUS odssam_getgrpwent(struct pdb_methods *my_methods, GROUP_MAP *map)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	tDirStatus dirStatus = eDSNoErr;
	int entriesAvailable = 0;
	CFMutableDictionaryRef entry = NULL;
	
	struct odssam_privates *ods_state = (struct odssam_privates *)my_methods->private_data;

	if (ods_state->groupsArray != NULL)
		entriesAvailable = CFArrayGetCount(ods_state->groupsArray);
	else
		return 	NT_STATUS_UNSUCCESSFUL; // allocate array???
			
	if (entriesAvailable == 0 || ods_state->groupsIndex >= entriesAvailable) {
		DEBUG(0,("odssam_getgrpwent: entriesAvailable(%d) contextData(%p)\n", entriesAvailable, ods_state->contextData));
		CFArrayRemoveAllValues(ods_state->groupsArray);
		
		if (entriesAvailable && ods_state->groupsIndex >= entriesAvailable && ods_state->contextData == NULL) {
			odssam_endsampwent(my_methods);
			return NT_STATUS_UNSUCCESSFUL;
		}
			        
		if ((dirStatus = get_sam_record_attributes(ods_state, ods_state->groupsArray, kDSStdRecordTypeGroups, NULL, false)) != eDSNoErr) {
			ret = NT_STATUS_UNSUCCESSFUL;
		} else {
			entriesAvailable = CFArrayGetCount(ods_state->groupsArray);
			DEBUG(0,("odssam_getgrpwent: entriesAvailable Take 2(%d) contextData(%p)\n", entriesAvailable, ods_state->contextData));
			ods_state->groupsIndex = 0;
		}
	}
	
	if (dirStatus == eDSNoErr && entriesAvailable) {
		entry = (CFDictionaryRef) CFArrayGetValueAtIndex(ods_state->groupsArray, ods_state->groupsIndex);
		ods_state->groupsIndex++;
		if (!init_group_from_ods(ods_state, map, entry)) {
			DEBUG(1,("odssam_getgrpwent: init_group_from_ods failed for group index(%d)\n", ods_state->groupsIndex));
			ret = NT_STATUS_UNSUCCESSFUL;
		}
		ret = NT_STATUS_OK;
	}
	

	return ret;
}

static NTSTATUS odssam_getgrsid(struct pdb_methods *my_methods, GROUP_MAP *map, DOM_SID sid)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	struct odssam_privates *ods_state = (struct odssam_privates *)my_methods->private_data;
    CFMutableArrayRef usersArray = NULL;
#ifdef USE_ALGORITHMIC_RID
	uint32 gid = 99;
	pstring gid_string;
#endif
	CFMutableDictionaryRef entry = NULL;
	pstring rid_string;
	fstring sid_string;
	uint32 rid;

    usersArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);    

	sid_to_string(sid_string, &sid);
	
	DEBUG(4,("odssam_getgrsid: SID<%s>\n", sid_string));

	if (!sid_peek_check_rid(get_global_sam_sid(), &sid, &rid)) {
		DEBUG(4,("odssam_getgrsid: Not a member of this domain\n"));	
		// return NT_STATUS_NO_SUCH_USER;
	}	
	snprintf(rid_string, sizeof(rid_string) - 1, "%i", rid);
	DEBUG(4,("odssam_getgrsid: rid<%s>\n", rid_string));

#ifdef USE_ALGORITHMIC_RID
	gid = pdb_group_rid_to_gid(rid);
	snprintf(gid_string, sizeof(gid_string) - 1, "%i", gid);
	DEBUG(4,("odssam_getgrsid: gid<%s>\n", gid_string));
#endif

#if defined(kDS1AttrSMBSID)
if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeGroups, kDS1AttrSMBSID, sid_string, True) == eDSNoErr && (CFArrayGetCount(usersArray) != 0)) {
		DEBUG(4,("odssam_getgrsid: kDS1AttrSMBSID found\n"));		
	} else
#endif	
		if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeGroups, kDS1AttrSMBRID, rid_string, True) == eDSNoErr && (CFArrayGetCount(usersArray) != 0)) {
		DEBUG(4,("odssam_getgrsid: kDS1AttrSMBRID found\n"));		
#ifdef USE_ALGORITHMIC_RID
	} else  if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeGroups, kDS1AttrPrimaryGroupID, gid_string, True) == eDSNoErr && (CFArrayGetCount(usersArray) != 0)){
		DEBUG(4,("odssam_getgrsid: rid(%d) -> gid(%d)\n", rid, gid));	
#endif
	} else {
		DEBUG(4,("odssam_getgrsid: NT_STATUS_NO_SUCH_USER\n"));	
		ret = NT_STATUS_NO_SUCH_USER;
		goto cleanup;
	}

	entry = (CFDictionaryRef) CFArrayGetValueAtIndex(usersArray, 0);
	if (entry) {
		if (!init_group_from_ods(ods_state, map, entry)) {
			DEBUG(1,("odssam_getgrsid: init_sam_from_ods failed!\n"));
			ret = NT_STATUS_NO_SUCH_USER;
			goto cleanup;
		}
		ret = NT_STATUS_OK;
	} else {
		ret = NT_STATUS_NO_SUCH_USER;
		goto cleanup;
	}

cleanup:
	CFRelease(usersArray);
	return ret;


//	return get_group_map_from_sid(sid, map) ?
//		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS odssam_getgrgid(struct pdb_methods *methods, GROUP_MAP *map, gid_t gid)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	struct odssam_privates *ods_state = (struct odssam_privates *)methods->private_data;
    CFMutableArrayRef usersArray = NULL;
	CFMutableDictionaryRef entry = NULL;
	pstring gid_string;

    usersArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);    

	snprintf(gid_string, sizeof(gid_string) - 1, "%i", gid);
	DEBUG(1,("odssam_getgrgid: gid [%s]\n", gid_string));
	if (get_sam_record_by_attr(ods_state, usersArray, kDSStdRecordTypeGroups, kDS1AttrPrimaryGroupID, gid_string, True) != eDSNoErr || (CFArrayGetCount(usersArray) == 0)){
			ret = NT_STATUS_NO_SUCH_USER;
			goto cleanup;
		}

	entry = (CFDictionaryRef) CFArrayGetValueAtIndex(usersArray, 0);
	if (entry) {
		if (!init_group_from_ods(ods_state, map, entry)) {
			DEBUG(1,("odssam_getgrgid: init_group_from_ods failed!\n"));
			ret = NT_STATUS_NO_SUCH_USER;
			goto cleanup;
		}
		ret = NT_STATUS_OK;
	} else {
		ret = NT_STATUS_NO_SUCH_USER;
		goto cleanup;
	}

cleanup:
	CFRelease(usersArray);
	return ret;


//	return get_group_map_from_gid(gid, map, with_priv) ?
//		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

static NTSTATUS odssam_getgrnam(struct pdb_methods *methods, GROUP_MAP *map, char *name)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	tDirStatus dirStatus = eDSNoErr;
	CFMutableDictionaryRef entry = NULL;
	
	struct odssam_privates *ods_state = (struct odssam_privates *)methods->private_data;

    CFMutableArrayRef recordsArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (get_sam_record_by_attr(ods_state, recordsArray, kDSStdRecordTypeGroups, kDSNAttrRecordName, name, True) == eDSNoErr && (CFArrayGetCount(recordsArray) != 0)) {
		DEBUG(4,("odssam_getgrnam: kDSStdRecordTypeGroups kDSNAttrRecordName<%s>\n", name));
	 } else if (get_sam_record_by_attr(ods_state, recordsArray, kDSStdRecordTypeGroups, kDS1AttrDistinguishedName, name, True) == eDSNoErr && (CFArrayGetCount(recordsArray) != 0)) {
		DEBUG(4,("odssam_getgrnam: kDSStdRecordTypeGroups kDS1AttrDistinguishedName<%s>\n", name));
	} else {
//	if (((dirStatus = get_sam_record_attributes(ods_state, recordsArray, kDSStdRecordTypeGroups, name, true)) != eDSNoErr) || (CFArrayGetCount(recordsArray) == 0)) {
		DEBUG(0,("[%d]odssam_getgrnam: %s no account for '%s'!\n", dirStatus, kDSStdRecordTypeGroups, name));
		ret = NT_STATUS_UNSUCCESSFUL;
	}
/* handle duplicates - currently uses first match in search policy*/
	if (dirStatus == eDSNoErr && CFArrayGetCount(recordsArray)) {
        entry = (CFDictionaryRef) CFArrayGetValueAtIndex(recordsArray, 0);
		if (!init_group_from_ods(ods_state, map, entry)) {
            DEBUG(1,("odssam_getsampwnam: init_group_from_ods failed for account '%s'!\n", name));
            ret = NT_STATUS_UNSUCCESSFUL;
		}    
		ret = NT_STATUS_OK;
	} else {
//		ret = get_group_map_from_ntname(name, map) ?
//		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
         ret = NT_STATUS_UNSUCCESSFUL;
	}
	
	CFRelease(recordsArray);

	return ret;

}


static BOOL init_ods_from_group (struct odssam_privates *ods_state, GROUP_MAP *map, CFMutableDictionaryRef groupEntry, CFMutableDictionaryRef mods)

{
	pstring tmp;

	if (groupEntry == NULL || map == NULL || mods == NULL) {
		DEBUG(0, ("init_ods_from_group: NULL parameters found!\n"));
		return False;
	}

	if (sid_to_string(tmp, &map->sid))
		make_a_mod(mods, kDS1AttrSMBSID, tmp);

	if (map->nt_name)
		make_a_mod(mods, kDS1AttrDistinguishedName, map->nt_name);
	
	if (map->comment)
		make_a_mod(mods, kDS1AttrComment, map->comment);


	return True;
}

static NTSTATUS odssam_add_group_mapping_entry(struct pdb_methods *methods, GROUP_MAP *map)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	struct odssam_privates *ods_state = (struct odssam_privates *)methods->private_data;
	tDirStatus dirStatus = eDSNoErr;
    CFMutableArrayRef recordsArray = NULL;
	CFMutableDictionaryRef entry = NULL, groupMods = NULL;
	pstring gid_string;
    
    recordsArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);    

	snprintf(gid_string, sizeof(gid_string) - 1, "%i", map->gid);
	DEBUG(1,("odssam_add_group_mapping_entry: gid [%s]\n", gid_string));
	if (get_sam_record_by_attr(ods_state, recordsArray, kDSStdRecordTypeGroups, kDS1AttrPrimaryGroupID, gid_string, True) != eDSNoErr || (CFArrayGetCount(recordsArray) == 0)){
			ret = NT_STATUS_UNSUCCESSFUL;
			goto cleanup;
		}

	groupMods = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	entry = (CFDictionaryRef) CFArrayGetValueAtIndex(recordsArray, 0);
	if (entry) {
		if (!init_ods_from_group(ods_state, map, entry, groupMods)) {
			DEBUG(1,("odssam_add_group_mapping_entry: init_ods_from_group failed!\n"));
			if (groupMods)
				CFRelease(groupMods);
			ret = NT_STATUS_UNSUCCESSFUL;
			goto cleanup;
		}
		ret = NT_STATUS_OK;
	} else {
		ret = NT_STATUS_UNSUCCESSFUL;
		goto cleanup;
	}

	if (CFDictionaryGetCount(groupMods) == 0) {
		DEBUG(0, ("odssam_add_group_mapping_entry: mods is empty\n"));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto cleanup;
	}

	dirStatus = add_record_attributes(ods_state, groupMods, entry, kDSStdRecordTypeGroups);
	if (eDSNoErr != dirStatus) {
		DEBUG(0, ("odssam_add_sam_account: [%d]add_record_attributes\n", dirStatus));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto cleanup;
	} else {
		ret = NT_STATUS_OK;
	}

	DEBUG(2, ("odssam_add_group_mapping_entry: successfully modified group %lu in Open Directory\n", (unsigned long)map->gid));
cleanup:
	if (groupMods)
		CFRelease(groupMods);	
	return ret;
}

static NTSTATUS odssam_update_group_mapping_entry(struct pdb_methods *methods, GROUP_MAP *map)
{
	NTSTATUS ret = NT_STATUS_UNSUCCESSFUL;
	struct odssam_privates *ods_state = (struct odssam_privates *)methods->private_data;
	tDirStatus dirStatus = eDSNoErr;
    CFMutableArrayRef recordsArray = NULL;
	CFMutableDictionaryRef entry = NULL;
	pstring gid_string;

    recordsArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);    

	snprintf(gid_string, sizeof(gid_string) - 1, "%i", map->gid);
	DEBUG(1,("odssam_update_group_mapping_entry: gid [%s]\n", gid_string));
	if (get_sam_record_by_attr(ods_state, recordsArray, kDSStdRecordTypeGroups, kDS1AttrPrimaryGroupID, gid_string, True) != eDSNoErr || (CFArrayGetCount(recordsArray) == 0)){
			ret = NT_STATUS_UNSUCCESSFUL;
			goto cleanup;
		}

	CFMutableDictionaryRef groupMods = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	entry = (CFDictionaryRef) CFArrayGetValueAtIndex(recordsArray, 0);
	if (entry) {
		if (!init_ods_from_group(ods_state, map, entry, groupMods)) {
			DEBUG(1,("odssam_update_group_mapping_entry: init_ods_from_group failed!\n"));
			if (groupMods)
				CFRelease(groupMods);
			ret = NT_STATUS_UNSUCCESSFUL;
			goto cleanup;
		}
		ret = NT_STATUS_OK;
	} else {
		ret = NT_STATUS_UNSUCCESSFUL;
		goto cleanup;
	}

	if (CFDictionaryGetCount(groupMods) == 0) {
		DEBUG(0, ("odssam_update_group_mapping_entry: mods is empty\n"));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto cleanup;
	}

	dirStatus = add_record_attributes(ods_state, groupMods, entry, kDSStdRecordTypeGroups);
	if (eDSNoErr != dirStatus) {
		DEBUG(0, ("odssam_add_sam_account: [%d]add_record_attributes\n", dirStatus));
		ret = NT_STATUS_UNSUCCESSFUL;
		goto cleanup;
	} else {
		ret = NT_STATUS_OK;
	}

	DEBUG(2, ("odssam_update_group_mapping_entry: successfully modified group %lu in Open Directory\n", (unsigned long)map->gid));
cleanup:
	if (groupMods)
		CFRelease(groupMods);	
	return ret;
}

static NTSTATUS odssam_enum_group_mapping(struct pdb_methods *methods,
					   enum SID_NAME_USE sid_name_use,
					   GROUP_MAP **rmap, int *num_entries,
					   BOOL unix_only, BOOL with_priv)
{

	GROUP_MAP map;
	GROUP_MAP *mapt;
	int entries = 0;

	*num_entries = 0;
	*rmap = NULL;

	if (!NT_STATUS_IS_OK(odssam_setgrpwent(methods, False))) {
		DEBUG(0, ("odssam_enum_group_mapping: Unable to open passdb\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	while (NT_STATUS_IS_OK(odssam_getgrpwent(methods, &map))) {
		if (sid_name_use != SID_NAME_UNKNOWN &&
		    sid_name_use != map.sid_name_use) {
			DEBUG(11,("odssam_enum_group_mapping: group %s is not of the requested type\n", map.nt_name));
			continue;
		}
		if (unix_only==ENUM_ONLY_MAPPED && map.gid==-1) {
			DEBUG(11,("odssam_enum_group_mapping: group %s is non mapped\n", map.nt_name));
			continue;
		}

		mapt=(GROUP_MAP *)Realloc((*rmap), (entries+1)*sizeof(GROUP_MAP));
		if (!mapt) {
			DEBUG(0,("odssam_enum_group_mapping: Unable to enlarge group map!\n"));
			SAFE_FREE(*rmap);
			return NT_STATUS_UNSUCCESSFUL;
		}
		else
			(*rmap) = mapt;

		mapt[entries] = map;

		entries += 1;

	}
	odssam_endgrpwent(methods);

	*num_entries = entries;

	return NT_STATUS_OK;

//	return enum_group_mapping(sid_name_use, rmap, num_entries, unix_only,
//				  with_priv) ?
//		NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL;
}

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
#ifdef USES_ODGROUPMAPPING
	(*pdb_method)->getgrsid = odssam_getgrsid;
	(*pdb_method)->getgrgid = odssam_getgrgid;
	(*pdb_method)->getgrnam = odssam_getgrnam;
	(*pdb_method)->add_group_mapping_entry = odssam_add_group_mapping_entry;
	(*pdb_method)->update_group_mapping_entry = odssam_update_group_mapping_entry;
//	(*pdb_method)->delete_group_mapping_entry = odssam_delete_group_mapping_entry;
	(*pdb_method)->enum_group_mapping = odssam_enum_group_mapping;
#endif
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
    ods_state->groupsArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks); 
	ods_state->domainInfo = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

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
