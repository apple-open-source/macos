/* 
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Tridgell              1992-2000
   Copyright (C) Luke Kenneth Casson Leighton 1996-2000
   Copyright (C) Andrew Bartlett              2001
   
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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesUtils.h>
#include <libopendirectorycommon.h>

tDirNodeReference getusernode(tDirReference dirRef, const char *userName)
{
    tDirStatus			status			= eDSNoErr;
    long			bufferSize		= 1024 * 10;
    long			returnCount		= 0;
    tDataBufferPtr	dataBuffer		= NULL;
  	tDirNodeReference		searchNodeRef		= NULL;
    tDataListPtr		searchNodeName		= NULL;
    tDirNodeReference		userNodeRef		= NULL;
    tDataListPtr		userNodePath		= NULL;
    char			userNodePathStr[256]	= {0};
    char			recUserName[128]	= {0};
    tDataListPtr		recName			= NULL;
    tDataListPtr		recType			= NULL;
    tDataListPtr		attrType		= NULL;

    tAttributeListRef		attributeListRef	= 0;
    tRecordEntryPtr		outRecordEntryPtr	= NULL;
    tAttributeEntryPtr		attributeInfo		= NULL;
    tAttributeValueListRef	attributeValueListRef	= 0;
    tAttributeValueEntryPtr	attrValue		= NULL;
    long			i			= 0;
    
    dataBuffer = dsDataBufferAllocate(dirRef, bufferSize);
    if (dataBuffer == NULL) goto cleanup;
    status = dsFindDirNodes(dirRef, dataBuffer, NULL, eDSSearchNodeName, &returnCount, NULL);
   if ((status != eDSNoErr) || (returnCount <= 0)) goto cleanup;

    status = dsGetDirNodeName(dirRef, dataBuffer, 1, &searchNodeName);
    if (status != eDSNoErr) goto cleanup;
    status = dsOpenDirNode(dirRef, searchNodeName, &searchNodeRef);
    if (status != eDSNoErr) goto cleanup;

    recName = dsBuildListFromStrings(dirRef, userName, NULL);
    recType = dsBuildListFromStrings(dirRef, kDSStdRecordTypeUsers, NULL);	
    attrType = dsBuildListFromStrings(dirRef, kDSNAttrMetaNodeLocation, kDSNAttrRecordName, NULL);

   	status = dsGetRecordList(searchNodeRef, dataBuffer, recName, eDSiExact, recType, attrType, 0, &returnCount, NULL);
   if (status != eDSNoErr) goto cleanup;

    status = dsGetRecordEntry(searchNodeRef, dataBuffer, 1, &attributeListRef, &outRecordEntryPtr);
    if (status == eDSNoErr)
    {
        for (i = 1 ; i <= outRecordEntryPtr->fRecordAttributeCount; i++)
        {
            status = dsGetAttributeEntry(searchNodeRef, dataBuffer, attributeListRef, i, &attributeValueListRef, &attributeInfo);
            status = dsGetAttributeValue(searchNodeRef, dataBuffer, 1, attributeValueListRef, &attrValue);
            if (status == eDSNoErr)
            {
                if (strncmp(attributeInfo->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation, strlen(kDSNAttrMetaNodeLocation)) == 0)
                {
                    strncpy(userNodePathStr, attrValue->fAttributeValueData.fBufferData, attrValue->fAttributeValueData.fBufferSize);
                    ///DEBUG(0,("getusernode: [%d]userNodePathStr (%s)\n", i, userNodePathStr ));
                } else if (strncmp(attributeInfo->fAttributeSignature.fBufferData, kDSNAttrRecordName, strlen(kDSNAttrRecordName)) == 0) {
                    strncpy(recUserName, attrValue->fAttributeValueData.fBufferData, attrValue->fAttributeValueData.fBufferSize);
                    ///DEBUG(0,("getusernode: [%d]recUserName (%s)\n", i, recUserName ));
                }
            }
            if (attrValue != NULL) {
                  ///DEBUG(0,("getusernode: [%d]dsDeallocAttributeValueEntry dirRef(%d) attrValue(%d)\n", i, dirRef, attrValue ));
                  dsDeallocAttributeValueEntry(dirRef, attrValue);
                    attrValue = NULL;
            }				
            if (attributeValueListRef != 0)
            {
                    dsCloseAttributeValueList(attributeValueListRef);
                    attributeValueListRef = 0;
            }	
            if (attributeInfo != NULL) {
                   ///DEBUG(0,("getusernode: [%d]dsDeallocAttributeEntry dirRef(%d) attributeInfo(%d)\n", i, dirRef, attributeInfo ));
                   dsDeallocAttributeEntry(dirRef, attributeInfo);
                    attributeInfo = NULL;
            }
        }
        if (outRecordEntryPtr != NULL) {
            dsDeallocRecordEntry(dirRef, outRecordEntryPtr);
            outRecordEntryPtr = NULL;
        }
        if (strlen(userNodePathStr) != 0 && strlen(recUserName) != 0)
        {
            userNodePath = dsBuildFromPath(dirRef, userNodePathStr, "/");
            status = dsOpenDirNode(dirRef, userNodePath, &userNodeRef);
            dsDataListDeallocate( dirRef, userNodePath);
            free(userNodePath);
        }
    }
cleanup:
     if (dataBuffer != NULL)
        dsDataBufferDeAllocate(dirRef, dataBuffer);
   if (searchNodeName != NULL)
    {
        dsDataListDeallocate(dirRef, searchNodeName);
        free(searchNodeName);
    }
    if (searchNodeRef != NULL)
        dsCloseDirNode(searchNodeRef);
    if (recName != NULL)
    {
        dsDataListDeallocate(dirRef, recName);
        free(recName);
    }
    if (recType != NULL)
    {
        dsDataListDeallocate(dirRef, recType);
        free(recType);
    }
    if (attrType != NULL)
    {
        dsDataListDeallocate(dirRef, attrType);
        free(attrType);
    }

    return userNodeRef;
}

static NTSTATUS map_dserr_to_nterr(tDirStatus dirStatus)
{
	switch (dirStatus) {
		case (eDSAuthFailed):
		case (eDSAuthBadPassword): return NT_STATUS_WRONG_PASSWORD;
		
		case (eDSAuthAccountInactive): return NT_STATUS_ACCOUNT_DISABLED;
		
		case (eDSAuthNewPasswordRequired):
		case (eDSAuthPasswordExpired): return NT_STATUS_PASSWORD_MUST_CHANGE;
		
		default: return NT_STATUS_WRONG_PASSWORD;
	
	};
}

tDirStatus opendirectory_auth_user(tDirReference dirRef, tDirNodeReference userNode, const char* user, char *challenge, char *password, char *inAuthMethod)
{
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long		curr			= 0;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;			
	
        authBuff = dsDataBufferAllocate( dirRef, 2048 );
        if ( authBuff != NULL )
        {
                stepBuff = dsDataBufferAllocate( dirRef, 2048 );
                if ( stepBuff != NULL )
                {
                        authType = dsDataNodeAllocateString( dirRef,  inAuthMethod);
                        if ( authType != NULL )
                        {
                                // User Name
                                len = strlen( user );
                                memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
                                curr += sizeof( long );
                                memcpy( &(authBuff->fBufferData[ curr ]), user, len );
                                curr += len;
                                // C8
                                len = 8;
                                memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
                                curr += sizeof (long );
                                memcpy( &(authBuff->fBufferData[ curr ]), challenge, len );
                                curr += len;
                                // P24
                                len = 24;
                                memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
                                curr += sizeof (long );
                                memcpy( &(authBuff->fBufferData[ curr ]), password, len );
                                curr += len;
                                
                                authBuff->fBufferLength = curr;
                                status = dsDoDirNodeAuth( userNode, authType, True, authBuff, stepBuff, NULL );
                                if ( status == eDSNoErr )
                                {
                                        DEBUG(1,("User \"%s\" authenticated successfully with \"%s\" :)\n", user, inAuthMethod ));
                                }
                                else
                                {
                                        DEBUG(1,("User \"%s\" failed to authenticate with \"%s\" (%d) :(\n", user, inAuthMethod,status) );
                                }
                        }
                        bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
                        if ( bufferStatus != eDSNoErr )
                        {
                                DEBUG(1,("*** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
                        }
                }
                else
                {
                        DEBUG(1,("*** dsDataBufferAllocate(2) faild with \n" ));
                }
                bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
                if ( bufferStatus != eDSNoErr )
                {
                        DEBUG(1,( "*** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus ));
                }
        }
        else
        {
                DEBUG(1,("*** dsDataBufferAllocate(1) faild with \n" ));
        }

    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
	return status;


}

/****************************************************************************
core of smb password checking routine.
****************************************************************************/
static tDirStatus opendirectory_smb_pwd_check_ntlmv1(tDirReference dirRef, tDirNodeReference userNode, const char *user, char *inAuthMethod,
				DATA_BLOB nt_response,
				 DATA_BLOB sec_blob,
				 uint8 user_sess_key[16])
{
	/* Finish the encryption of part_passwd. */
	uchar p24[24];
    tDirStatus	status	= eDSAuthFailed;
    tDirStatus	keyStatus = eDSNoErr;

	
	if (sec_blob.length != 8) {
		DEBUG(0, ("opendirectory_smb_pwd_check_ntlmv1: incorrect challenge size (%ld)\n", sec_blob.length));
		return eDSAuthFailed;
	}
	
	if (nt_response.length != 24) {
		DEBUG(0, ("opendirectory_smb_pwd_check_ntlmv1: incorrect password length (%ld)\n", nt_response.length));
		return eDSAuthFailed;
	}

    status = opendirectory_auth_user(dirRef, userNode, user, sec_blob.data, nt_response.data, inAuthMethod);
	DEBUG(0, ("opendirectory_smb_pwd_check_ntlmv1: [%d]opendirectory_auth_user\n", status));
	
	if (eDSNoErr == status && user_sess_key != NULL)
	{
		become_root();
		keyStatus = opendirectory_user_session_key(user, user_sess_key, NULL);
		unbecome_root();
		DEBUG(2, ("opendirectory_smb_pwd_check_ntlmv1: [%d]opendirectory_user_session_key\n", keyStatus));
	}
		
#if DEBUG_PASSWORD
	DEBUG(100,("Password from client was |"));
	dump_data(100, nt_response.data, nt_response.length);
	DEBUG(100,("Given challenge was |"));
	dump_data(100, sec_blob.data, sec_blob.length);
	DEBUG(100,("Value from encryption was |"));
	dump_data(100, p24, 24);
#endif

 return (status);
}


/****************************************************************************
 Do a specific test for an smb password being correct, given a smb_password and
 the lanman and NT responses.
****************************************************************************/
static NTSTATUS opendirectory_password_ok(tDirReference dirRef, tDirNodeReference userNode,
				const struct auth_context *auth_context,
				TALLOC_CTX *mem_ctx,
				SAM_ACCOUNT *sampass, 
				const auth_usersupplied_info *user_info, 
				uint8 user_sess_key[16])
{
	uint16 acct_ctrl;
	uint32 auth_flags;
	tDirStatus dirStatus = eDSAuthFailed;
	
	acct_ctrl = pdb_get_acct_ctrl(sampass);
	if (acct_ctrl & ACB_PWNOTREQ) 
	{
		if (lp_null_passwords()) 
		{
			DEBUG(3,("Account for user '%s' has no password and null passwords are allowed.\n", pdb_get_username(sampass)));
			return(NT_STATUS_OK);
		} 
		else 
		{
			DEBUG(3,("Account for user '%s' has no password and null passwords are NOT allowed.\n", pdb_get_username(sampass)));
			return(NT_STATUS_LOGON_FAILURE);
		}		
	}

	auth_flags = user_info->auth_flags;

	if (auth_flags & AUTH_FLAG_NTLMv2_RESP) {
		/* We have the NT MD4 hash challenge available - see if we can
		   use it (ie. does it exist in the smbpasswd file).
		*/
		DEBUG(4,("opendirectory_password_ok: Checking NTLMv2 password\n"));
#if 0
		if (opendirectory_smb_pwd_check_ntlmv2( user_info->nt_resp, 
					  auth_context->challenge, 
					  user_info->smb_name.str, 
					  user_info->client_domain.str,
					  user_sess_key))
		{
			return NT_STATUS_OK;
		} else {
			DEBUG(3,("opendirectory_password_ok: NTLMv2 password check failed\n"));
			return NT_STATUS_WRONG_PASSWORD;
		}
#endif
		return NT_STATUS_LOGON_FAILURE; /* NTLMv2 not available */
	} else if (auth_flags & AUTH_FLAG_NTLM_RESP) {
		if (lp_ntlm_auth()) {		
			/* We have the NT MD4 hash challenge available - see if we can
			   use it
			*/
			DEBUG(4,("opendirectory_password_ok: Checking NT MD4 password\n"));
			if ((dirStatus = opendirectory_smb_pwd_check_ntlmv1(dirRef, userNode, pdb_get_username(sampass), kDSStdAuthSMB_NT_Key,
						user_info->nt_resp, 
						 auth_context->challenge,
						 user_sess_key)) == eDSNoErr) 
			{
				return NT_STATUS_OK;
			} else {
				DEBUG(3,("opendirectory_password_ok: NT MD4 password check failed for user %s\n",pdb_get_username(sampass)));
				
				//return NT_STATUS_WRONG_PASSWORD;
				return map_dserr_to_nterr(dirStatus);
			}
		} else {
			DEBUG(2,("opendirectory_password_ok: NTLMv1 passwords NOT PERMITTED for user %s\n",pdb_get_username(sampass)));			
			/* no return, becouse we might pick up LMv2 in the LM field */
		}
	}
	
	if (auth_flags & AUTH_FLAG_LM_RESP) {
		if (user_info->lm_resp.length != 24) {
			DEBUG(2,("opendirectory_password_ok: invalid LanMan password length (%d) for user %s\n", 
				 user_info->nt_resp.length, pdb_get_username(sampass)));		
		}
		
		if (!lp_lanman_auth()) {
			DEBUG(3,("opendirectory_password_ok: Lanman passwords NOT PERMITTED for user %s\n",pdb_get_username(sampass)));
		} else {			
			DEBUG(4,("opendirectory_password_ok: Checking LM password\n"));
			if ((dirStatus =  opendirectory_smb_pwd_check_ntlmv1(dirRef, userNode, pdb_get_username(sampass), kDSStdAuthSMB_LM_Key,
						user_info->lm_resp, 
						 auth_context->challenge,
						 user_sess_key)) == eDSNoErr) 
			{
				return NT_STATUS_OK;
			} else {
				DEBUG(3,("opendirectory_password_ok: LM password check failed for user %s\n",pdb_get_username(sampass)));
				
				//return NT_STATUS_WRONG_PASSWORD;
				return map_dserr_to_nterr(dirStatus);
			}
		}
#if 0
		if (IS_SAM_DEFAULT(sampass, PDB_NTPASSWD)) {
			DEBUG(4,("opendirectory_password_ok: LM password check failed for user, no NT password %s\n",pdb_get_username(sampass)));
			return NT_STATUS_WRONG_PASSWORD;
		} 
#endif		

		/* This is for 'LMv2' authentication.  almost NTLMv2 but limited to 24 bytes.
		   - related to Win9X, legacy NAS pass-through authentication
		*/
		DEBUG(4,("opendirectory_password_ok: Checking LMv2 password\n"));
#if 0
		if (opendirectory_smb_pwd_check_ntlmv2( user_info->lm_resp, 
					  auth_context->challenge, 
					  user_info->smb_name.str, 
					  user_info->client_domain.str,
					  user_sess_key))
		{
			return NT_STATUS_OK;
		} else {
			DEBUG(3,("opendirectory_password_ok: LMv2 password check failed for user %s\n",pdb_get_username(sampass)));
			
			//return NT_STATUS_WRONG_PASSWORD;
			return map_dserr_to_nterr(dirStatus);
		}
#endif
		/* Apparently NT accepts NT responses in the LM field
		   - I think this is related to Win9X pass-through authentication
		*/
		DEBUG(4,("opendirectory_password_ok: Checking NT MD4 password in LM field\n"));
		if (lp_ntlm_auth()) 
		{
			if ((dirStatus =  opendirectory_smb_pwd_check_ntlmv1(dirRef, userNode, pdb_get_username(sampass), kDSStdAuthSMB_NT_Key,
						user_info->lm_resp, 
						 auth_context->challenge,
						 user_sess_key)) == eDSNoErr) 
			{
				return NT_STATUS_OK;
			}
			DEBUG(3,("opendirectory_password_ok: LM password, NT MD4 password in LM field and LMv2 failed for user %s\n",pdb_get_username(sampass)));
			//return NT_STATUS_WRONG_PASSWORD;
			return map_dserr_to_nterr(dirStatus);
		} else {
			DEBUG(3,("opendirectory_password_ok: LM password and LMv2 failed for user %s, and NT MD4 password in LM field not permitted\n",pdb_get_username(sampass)));
			return NT_STATUS_WRONG_PASSWORD;
		}
			
	}
		
	/* Should not be reached, but if they send nothing... */
	DEBUG(3,("opendirectory_password_ok: NEITHER LanMan nor NT password supplied for user %s\n",pdb_get_username(sampass)));
	return NT_STATUS_WRONG_PASSWORD;
}

/****************************************************************************
 Do a specific test for a SAM_ACCOUNT being vaild for this connection 
 (ie not disabled, expired and the like).
****************************************************************************/
static NTSTATUS opendirectory_account_ok(TALLOC_CTX *mem_ctx,
			       SAM_ACCOUNT *sampass, 
			       const auth_usersupplied_info *user_info)
{
	uint16	acct_ctrl = pdb_get_acct_ctrl(sampass);
	char *workstation_list;
	time_t kickoff_time;
	
	DEBUG(4,("opendirectory_account_ok: Checking SMB password for user %s\n",pdb_get_username(sampass)));

	/* Quit if the account was disabled. */
	if (acct_ctrl & ACB_DISABLED) {
		DEBUG(1,("Account for user '%s' was disabled.\n", pdb_get_username(sampass)));
		return NT_STATUS_ACCOUNT_DISABLED;
	}

	/* Test account expire time */
	
	kickoff_time = pdb_get_kickoff_time(sampass);
	if (kickoff_time != 0 && time(NULL) > kickoff_time) {
		DEBUG(1,("Account for user '%s' has expired.\n", pdb_get_username(sampass)));
		DEBUG(3,("Account expired at '%ld' unix time.\n", (long)kickoff_time));
		return NT_STATUS_ACCOUNT_EXPIRED;
	}

	if (!(pdb_get_acct_ctrl(sampass) & ACB_PWNOEXP)) {
		time_t must_change_time = pdb_get_pass_must_change_time(sampass);
		time_t last_set_time = pdb_get_pass_last_set_time(sampass);

		/* check for immediate expiry "must change at next logon" */
		if (must_change_time == 0 && last_set_time != 0) {
			DEBUG(1,("Account for user '%s' password must change!.\n", pdb_get_username(sampass)));
			return NT_STATUS_PASSWORD_MUST_CHANGE;
		}

		/* check for expired password */
		if (must_change_time < time(NULL) && must_change_time != 0) {
			DEBUG(1,("Account for user '%s' password expired!.\n", pdb_get_username(sampass)));
			DEBUG(1,("Password expired at '%s' (%ld) unix time.\n", http_timestring(must_change_time), (long)must_change_time));
			return NT_STATUS_PASSWORD_EXPIRED;
		}
	}

	/* Test workstation. Workstation list is comma separated. */

	workstation_list = talloc_strdup(mem_ctx, pdb_get_workstations(sampass));

	if (!workstation_list) return NT_STATUS_NO_MEMORY;

	if (*workstation_list) {
		BOOL invalid_ws = True;
		const char *s = workstation_list;
			
		fstring tok;
			
		while (next_token(&s, tok, ",", sizeof(tok))) {
			DEBUG(10,("checking for workstation match %s and %s (len=%d)\n",
				  tok, user_info->wksta_name.str, user_info->wksta_name.len));
			if(strequal(tok, user_info->wksta_name.str)) {
				invalid_ws = False;
				break;
			}
		}
		
		if (invalid_ws) 
			return NT_STATUS_INVALID_WORKSTATION;
	}

	if (acct_ctrl & ACB_DOMTRUST) {
		DEBUG(2,("opendirectory_account_ok: Domain trust account %s denied by server\n", pdb_get_username(sampass)));
		return NT_STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT;
	}
	
	if (acct_ctrl & ACB_SVRTRUST) {
		DEBUG(2,("opendirectory_account_ok: Server trust account %s denied by server\n", pdb_get_username(sampass)));
		return NT_STATUS_NOLOGON_SERVER_TRUST_ACCOUNT;
	}
	
	if (acct_ctrl & ACB_WSTRUST) {
		DEBUG(4,("opendirectory_account_ok: Wksta trust account %s denied by server\n", pdb_get_username(sampass)));
		return NT_STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT;
	}
	
	return NT_STATUS_OK;
}


/****************************************************************************
check if a username/password is OK assuming the password is a 24 byte
SMB hash supplied in the user_info structure
return an NT_STATUS constant.
****************************************************************************/

static NTSTATUS check_opendirectory_security(const struct auth_context *auth_context,
				   void *my_private_data, 
				   TALLOC_CTX *mem_ctx,
				   const auth_usersupplied_info *user_info, 
				   auth_serversupplied_info **server_info)
{
	SAM_ACCOUNT *sampass=NULL;
	BOOL ret = False;
	NTSTATUS nt_status = NT_STATUS_OK;
	uint8 user_sess_key[16];
	const uint8* lm_hash;
    tDirStatus		dirStatus		= eDSNoErr;
    tDirReference	dirRef		= NULL;
    tDirNodeReference	userNodeRef	= NULL;

	if (!user_info || !auth_context) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Can't use the talloc version here, becouse the returned struct gets
	   kept on the server_info */
	if (!NT_STATUS_IS_OK(nt_status = pdb_init_sam(&sampass))) {
		return nt_status;
	}

	/* get the account information */

	if (user_info->internal_username.str && strlen(user_info->internal_username.str)) {
		become_root();
		ret = pdb_getsampwnam(sampass, user_info->internal_username.str);
		unbecome_root();
	}
	DEBUG(0,("check_opendirectory_security: after pdb_getsampwnam [%s]\n", user_info->internal_username.str));
	if (ret == False)
	{
		DEBUG(3,("Couldn't find user '%s' in passdb file.\n", user_info->internal_username.str));
		pdb_free_sam(&sampass);
		return NT_STATUS_NO_SUCH_USER;
	}

	DEBUG(0,("check_opendirectory_security: after pdb_free_sam [%s]\n", user_info->internal_username.str));

	nt_status = opendirectory_account_ok(mem_ctx, sampass, user_info);
	DEBUG(0,("check_opendirectory_security: after opendirectory_account_ok [%s]\n", user_info->internal_username.str));
	
	if (!NT_STATUS_IS_OK(nt_status)) {
		pdb_free_sam(&sampass);
		return nt_status;
	}

    dirStatus = dsOpenDirService(&dirRef);
    if (dirStatus != eDSNoErr)
    {
		DEBUG(0,("check_opendirectory_security: [%d]dsOpenDirService error\n", dirStatus));
		pdb_free_sam(&sampass);
		return NT_STATUS_UNSUCCESSFUL;
	}
	
    userNodeRef = getusernode(dirRef, pdb_get_username(sampass));
    
    if (userNodeRef != NULL)
    {
		nt_status = opendirectory_password_ok(dirRef, userNodeRef, auth_context, mem_ctx, sampass, user_info, user_sess_key);
        dsCloseDirNode( userNodeRef );
    	dsCloseDirService(dirRef);
    } else {
    	dsCloseDirService(dirRef);
		pdb_free_sam(&sampass);
    	return NT_STATUS_NO_SUCH_USER;
    }


	if (!NT_STATUS_IS_OK(nt_status)) {
		pdb_free_sam(&sampass);
		return nt_status;
	}

	if (!NT_STATUS_IS_OK(nt_status = make_server_info_sam(server_info, sampass))) {		
		DEBUG(0,("check_opendirectory_security: make_server_info_sam() failed with '%s'\n", nt_errstr(nt_status)));
		return nt_status;
	}

	lm_hash = pdb_get_lanman_passwd((*server_info)->sam_account);
	if (lm_hash) {
		memcpy((*server_info)->first_8_lm_hash, lm_hash, 8);
	}
	
	memcpy((*server_info)->session_key, user_sess_key, sizeof(user_sess_key));

	return nt_status;
}

/* module initialisation */
NTSTATUS auth_init_opendirectory(struct auth_context *auth_context, const char *param, auth_methods **auth_method) 
{
	if (!make_auth_methods(auth_context, auth_method)) {
		return NT_STATUS_NO_MEMORY;
	}

	(*auth_method)->auth = check_opendirectory_security;	
	(*auth_method)->name = "opendirectory";
	return NT_STATUS_OK;
}

NTSTATUS init_module(void)
{
	return smb_register_auth(AUTH_INTERFACE_VERSION, "opendirectory", auth_init_opendirectory);
}
