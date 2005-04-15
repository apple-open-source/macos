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

#include <DirectoryService/DirectoryService.h>
#include <libopendirectorycommon.h>
#if WITH_SACL
#include <membershipPriv.h>
#define kSMBServiceACL "smb"
#endif

tDirNodeReference getusernode(tDirReference dirRef, const char *userName)
{
    tDirStatus			status			= eDSNoErr;
    long			bufferSize		= 1024 * 10;
    long			returnCount		= 0;
    tDataBufferPtr	dataBuffer		= NULL;
  	tDirNodeReference		searchNodeRef		= 0;
    tDataListPtr		searchNodeName		= NULL;
    tDirNodeReference		userNodeRef		= 0;
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
    if (searchNodeRef != 0)
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
#if WITH_SACL
/*
	check_sacl(const char *inUser, const char *inService) - Check Service ACL
		inUser - username in utf-8
		inService - name of the service in utf-8
		
		NOTE: the service name is not the group name, the transformation currently goes like
			this: "service" -> "com.apple.access_service"
			
	returns
		1 if the user is authorized (or no ACL exists)
		0 if the user is not authorized or does not exist

*/
int		check_sacl(const char *inUser, const char *inService)
{
	uuid_t	user_uuid;
	int		isMember = 0;
	int		mbrErr = 0;
	
	// get the uuid
	if(mbr_user_name_to_uuid(inUser, user_uuid))
	{
		return 0;
	}	
	
	// check the sacl
	if((mbrErr = mbr_check_service_membership(user_uuid, inService, &isMember)))
	{
		if(mbrErr == ENOENT)	// no ACL exists
		{
			return 1;	
		} else {
			return 0;
		}
	}
	if(isMember == 1)
	{
		return 1;
	} else {
		return 0;
	}
}
#endif
tDirStatus opendirectory_ntlmv2_auth_user(tDirReference dirRef, tDirNodeReference userNode, const char* user, const char* domain,
										const DATA_BLOB *sec_blob, const DATA_BLOB *ntv2_response, DATA_BLOB *user_sess_key)
{
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long		curr			= 0;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;			

/*


	The auth method constant is: dsAuthMethodStandard:dsAuthNodeNTLMv2
	The format for data in the step buffer is:
	4 byte len + directory-services name
	4 byte len + server challenge
	4 byte len + client "blob" - 16 bytes of client digest + the blob data
	4 byte len + user name used in the digest (usually the same as item #1 in the buffer)
	4 byte len + domain


*/
	
        authBuff = dsDataBufferAllocate( dirRef, 2048 );
        if ( authBuff != NULL )
        {
                stepBuff = dsDataBufferAllocate( dirRef, 2048 );
                if ( stepBuff != NULL )
                {
                        authType = dsDataNodeAllocateString( dirRef,  "dsAuthMethodStandard:dsAuthNodeNTLMv2");
                        if ( authType != NULL )
                        {
                                // directory-services name
                                len = strlen( user );
                                memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
                                curr += sizeof( long );
                                memcpy( &(authBuff->fBufferData[ curr ]), user, len );
                                curr += len;
                                // server challenge
                                len = 8;
                                memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
                                curr += sizeof (long );
                                memcpy( &(authBuff->fBufferData[ curr ]), sec_blob->data, len );
                                curr += len;
                                // client "blob" - 16 bytes of client digest + the blob_data
                                len = ntv2_response->length;
                                memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
                                curr += sizeof (long );
                                memcpy( &(authBuff->fBufferData[ curr ]), ntv2_response->data, len );
                                curr += len;
                                 // user name used in the digest (usually the same as item #1 in the buffer)
                                len = strlen( user );
                                memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
                                curr += sizeof( long );
                                memcpy( &(authBuff->fBufferData[ curr ]), user, len );
                                curr += len;
                                // domain 
                                len = strlen( domain );
                                memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
                                curr += sizeof( long );
                                memcpy( &(authBuff->fBufferData[ curr ]), domain, len );
                                curr += len;
                               
                                authBuff->fBufferLength = curr;
                                status = dsDoDirNodeAuth( userNode, authType, True, authBuff, stepBuff, NULL );
                                if ( status == eDSNoErr )
                                {
                                        DEBUG(1,("User \"%s\" authenticated successfully with \"%s\" :)\n", user, "dsAuthMethodStandard:dsAuthNodeNTLMv2" ));
                                }
                                else
                                {
                                        DEBUG(1,("User \"%s\" failed to authenticate with \"%s\" (%d) :(\n", user, "dsAuthMethodStandard:dsAuthNodeNTLMv2",status) );
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
				const DATA_BLOB *nt_response,
				const DATA_BLOB *sec_blob,
				DATA_BLOB *user_sess_key)
{
	/* Finish the encryption of part_passwd. */
	uchar p24[24];
    tDirStatus	status	= eDSAuthFailed;
    tDirStatus	keyStatus = eDSNoErr;

	
	if (sec_blob->length != 8) {
		DEBUG(0, ("opendirectory_smb_pwd_check_ntlmv1: incorrect challenge size (%ld)\n", sec_blob->length));
		return eDSAuthFailed;
	}
	
	if (nt_response->length != 24) {
		DEBUG(0, ("opendirectory_smb_pwd_check_ntlmv1: incorrect password length (%ld)\n", nt_response->length));
		return eDSAuthFailed;
	}

    status = opendirectory_auth_user(dirRef, userNode, user, sec_blob->data, nt_response->data, inAuthMethod);
	
	if (eDSNoErr == status)
	{
		if (user_sess_key != NULL)
		{
			*user_sess_key = data_blob(NULL, 16);
			become_root();
			keyStatus = opendirectory_user_session_key(user, user_sess_key->data, NULL);
			unbecome_root();
			DEBUG(2, ("opendirectory_smb_pwd_check_ntlmv1: [%d]opendirectory_user_session_key\n", keyStatus));
		}
	} else {
		DEBUG(1, ("opendirectory_smb_pwd_check_ntlmv1: [%d]opendirectory_auth_user\n", status));	
	}
		
#if DEBUG_PASSWORD
	DEBUG(100,("Password from client was |"));
	dump_data(100, nt_response->data, nt_response->length);
	DEBUG(100,("Given challenge was |"));
	dump_data(100, sec_blob->data, sec_blob->length);
	DEBUG(100,("Value from encryption was |"));
	dump_data(100, p24, 24);
#endif

 return (status);
}

/****************************************************************************
 Core of smb password checking routine. (NTLMv2, LMv2)
 Note:  The same code works with both NTLMv2 and LMv2.
****************************************************************************/

static tDirStatus opendirectory_smb_pwd_check_ntlmv2(tDirReference dirRef, tDirNodeReference userNode,
				const DATA_BLOB *ntv2_response,
				 const DATA_BLOB *sec_blob,
				 const char *user, const char *domain,
				 BOOL upper_case_domain, /* should the domain be transformed into upper case? */
				 DATA_BLOB *user_sess_key)
{
    tDirStatus	status	= eDSAuthFailed;
    tDirStatus	keyStatus = eDSNoErr;
	u_int32_t session_key_len = 0;
	if (sec_blob->length != 8) {
		DEBUG(0, ("opendirectory_smb_pwd_check_ntlmv2: incorrect challenge size (%lu)\n", 
			  (unsigned long)sec_blob->length));
		return False;
	}
	
	if (ntv2_response->length < 24) {
		/* We MUST have more than 16 bytes, or the stuff below will go
		   crazy.  No known implementation sends less than the 24 bytes
		   for LMv2, let alone NTLMv2. */
		DEBUG(0, ("opendirectory_smb_pwd_check_ntlmv2: incorrect password length (%lu)\n", 
			  (unsigned long)ntv2_response->length));
		return False;
	}

    status = opendirectory_ntlmv2_auth_user(dirRef, userNode, user, domain, sec_blob, ntv2_response, user_sess_key );
	DEBUG(0, ("opendirectory_smb_pwd_check_ntlmv2:  [%d]opendirectory_[%s]_auth_user\n", status, ntv2_response->length == 24 ?  "LMv2" : "NTLMv2"));
	DEBUG(0, ("opendirectory_smb_pwd_check_ntlmv2:  [%X]user_sess_key\n", user_sess_key));


	if (eDSNoErr == status)
	{
		if (user_sess_key != NULL) {
			*user_sess_key = data_blob(NULL, 16);
			become_root();
			keyStatus = opendirectory_ntlmv2user_session_key(user, ntv2_response->length, ntv2_response->data, domain, &session_key_len, user_sess_key->data, NULL);
			unbecome_root();
			DEBUG(2, ("opendirectory_smb_pwd_check_ntlmv2: [%d]opendirectory_[%s]user_session_key len(%d)\n", keyStatus, ntv2_response->length == 24 ? "LMv2" : "NTLMv2", session_key_len));
		}
	}

#if DEBUG_PASSWORD
	DEBUGADD(100,("Password from client was |\n"));
	dump_data(100, ntv2_response->data, ntv2_response->length);
	DEBUGADD(100,("Variable data from client was |\n"));
	dump_data(100, sec_blob->data, sec_blob->length);
#endif

 return (status);

}


/**
 * Check a challenge-response password against the value of the NT or
 * LM password hash.
 *
 * @param mem_ctx talloc context
 * @param challenge 8-byte challenge.  If all zero, forces plaintext comparison
 * @param nt_response 'unicode' NT response to the challenge, or unicode password
 * @param lm_response ASCII or LANMAN response to the challenge, or password in DOS code page
 * @param username internal Samba username, for log messages
 * @param client_username username the client used
 * @param client_domain domain name the client used (may be mapped)
 * @param nt_pw MD4 unicode password from our passdb or similar
 * @param lm_pw LANMAN ASCII password from our passdb or similar
 * @param user_sess_key User session key
 * @param lm_sess_key LM session key (first 8 bytes of the LM hash)
 */

NTSTATUS opendirectory_opendirectory_ntlm_password_check(tDirReference dirRef, tDirNodeReference userNode, TALLOC_CTX *mem_ctx,
			     const DATA_BLOB *challenge,
			     const DATA_BLOB *lm_response,
			     const DATA_BLOB *nt_response,
			     const DATA_BLOB *lm_interactive_pwd,
			     const DATA_BLOB *nt_interactive_pwd,
			     const char *username, 
			     const char *client_username, 
			     const char *client_domain,
			     DATA_BLOB *user_sess_key, 
			     DATA_BLOB *lm_sess_key)
{
	tDirStatus dirStatus = eDSAuthFailed;

	if (nt_response->length != 0 && nt_response->length < 24) {
		DEBUG(2,("opendirectory_ntlm_password_check: invalid NT password length (%lu) for user %s\n", 
			 (unsigned long)nt_response->length, username));		
	}
	
	if (nt_response->length >= 24) {
		if (nt_response->length > 24) {
			/* We have the NT MD4 hash challenge available - see if we can
			   use it 
			*/
			DEBUG(4,("opendirectory_ntlm_password_check: Checking NTLMv2 password with domain [%s]\n", client_domain));
			if ((dirStatus = opendirectory_smb_pwd_check_ntlmv2( dirRef, userNode, nt_response, 
						  challenge, 
						  client_username, 
						  client_domain,
						  False,
						  user_sess_key)) == eDSNoErr)
			{
#if DEBUG_LMv2
				DEBUG(4,("opendirectory_ntlm_password_check: Checking LMv2 password with domain [%s]\n", client_domain));
				if ((dirStatus = opendirectory_smb_pwd_check_ntlmv2( dirRef, userNode, lm_response, 
							  challenge, 
							  client_username, 
							  client_domain,
							  False,
							  user_sess_key)) == eDSNoErr)
				{
					DEBUG(4,("opendirectory_ntlm_password_check: Checking LMv2 password with domain [%s]\n", client_domain));
				} else {
					DEBUG(4,("opendirectory_ntlm_password_check: Checking LMv2 password with domain [%s]\n", client_domain));
				}
#endif			
				return NT_STATUS_OK;
			}
			DEBUG(4,("opendirectory_ntlm_password_check: Checking NTLMv2 password with uppercased version of domain [%s]\n", client_domain));
			if ((dirStatus = opendirectory_smb_pwd_check_ntlmv2( dirRef, userNode, nt_response, 
						  challenge, 
						  client_username, 
						  client_domain,
						  True,
						  user_sess_key)) == eDSNoErr)
			{
				return NT_STATUS_OK;
			}

			DEBUG(4,("opendirectory_ntlm_password_check: Checking NTLMv2 password without a domain\n"));
			if ((dirStatus = opendirectory_smb_pwd_check_ntlmv2( dirRef, userNode, nt_response, 
						  challenge, 
						  client_username, 
						  "",
						  False,
						  user_sess_key)) == eDSNoErr)
			{

				return NT_STATUS_OK;
			} else {
				DEBUG(3,("opendirectory_ntlm_password_check: NTLMv2 password check failed\n"));
				return map_dserr_to_nterr(dirStatus);
			}
		}

		if (lp_ntlm_auth() || (nt_interactive_pwd && nt_interactive_pwd->length)) {		
			/* We have the NT MD4 hash challenge available - see if we can
			   use it (ie. does it exist in the smbpasswd file).
			*/
			DEBUG(4,("opendirectory_ntlm_password_check: Checking NT MD4 password\n"));
			if ((dirStatus =  opendirectory_smb_pwd_check_ntlmv1(dirRef, userNode, username, kDSStdAuthSMB_NT_Key,
						nt_response, 
						 challenge,
						 user_sess_key)) == eDSNoErr) 
			{
				return NT_STATUS_OK;
			} else {
				DEBUG(3,("opendirectory_ntlm_password_check: NT MD4 password check failed for user %s\n",
					 username));
				return map_dserr_to_nterr(dirStatus);
			}
		} else {
			DEBUG(2,("opendirectory_ntlm_password_check: NTLMv1 passwords NOT PERMITTED for user %s\n",
				 username));			
			/* no return, because we might pick up LMv2 in the LM field */
		}
	}
	
	if (lm_response->length == 0) {
		DEBUG(3,("opendirectory_ntlm_password_check: NEITHER LanMan nor NT password supplied for user %s\n",
			 username));
		return NT_STATUS_WRONG_PASSWORD;
	}
	
	if (lm_response->length < 24) {
		DEBUG(2,("opendirectory_ntlm_password_check: invalid LanMan password length (%lu) for user %s\n", 
			 (unsigned long)nt_response->length, username));		
		return NT_STATUS_WRONG_PASSWORD;
	}
		
	if (!lp_lanman_auth()) {
		DEBUG(3,("opendirectory_ntlm_password_check: Lanman passwords NOT PERMITTED for user %s\n",
			 username));
	} else {
		DEBUG(4,("opendirectory_ntlm_password_check: Checking LM password\n"));
		if ((dirStatus =  opendirectory_smb_pwd_check_ntlmv1(dirRef, userNode, username, kDSStdAuthSMB_LM_Key,
					lm_response, 
					 challenge,
					 NULL)) == eDSNoErr) 
		{
			return NT_STATUS_OK;
		} else {
			DEBUG(3,("opendirectory_ntlm_password_check: LM password check failed for user %s\n",username));
			
//			return map_dserr_to_nterr(dirStatus);
		}
	}
/*	
	if (!nt_pw) {
		DEBUG(4,("opendirectory_ntlm_password_check: LM password check failed for user, no NT password %s\n",username));
		return NT_STATUS_WRONG_PASSWORD;
	}
*/	
	/* This is for 'LMv2' authentication.  almost NTLMv2 but limited to 24 bytes.
	   - related to Win9X, legacy NAS pass-though authentication
	*/
	DEBUG(4,("opendirectory_ntlm_password_check: Checking LMv2 password with domain %s\n", client_domain));
	if ((dirStatus = opendirectory_smb_pwd_check_ntlmv2( dirRef, userNode, lm_response, 
				  challenge, 
				  client_username, 
				  client_domain,
				  False,
				  user_sess_key)) == eDSNoErr)
	{
		return NT_STATUS_OK;
	}
	
	DEBUG(4,("opendirectory_ntlm_password_check: Checking LMv2 password with upper-cased version of domain %s\n", client_domain));
	if ((dirStatus = opendirectory_smb_pwd_check_ntlmv2( dirRef, userNode, lm_response, 
				  challenge, 
				  client_username, 
				  client_domain,
				  True,
				  user_sess_key)) == eDSNoErr)
	{
		return NT_STATUS_OK;
	}
	
	DEBUG(4,("opendirectory_ntlm_password_check: Checking LMv2 password without a domain\n"));
	if ((dirStatus = opendirectory_smb_pwd_check_ntlmv2( dirRef, userNode, lm_response, 
				  challenge, 
				  client_username, 
				  "",
				  False,
				  user_sess_key)) == eDSNoErr)
	{
		return NT_STATUS_OK;
	}

	/* Apparently NT accepts NT responses in the LM field
	   - I think this is related to Win9X pass-though authentication
	*/
	DEBUG(4,("opendirectory_ntlm_password_check: Checking NT MD4 password in LM field\n"));
	if (lp_ntlm_auth()) {
		if ((dirStatus =  opendirectory_smb_pwd_check_ntlmv1(dirRef, userNode, username, kDSStdAuthSMB_NT_Key,
					lm_response, 
					 challenge,
					 user_sess_key)) == eDSNoErr) 
		{
			return NT_STATUS_OK;
		} 
		DEBUG(3,("opendirectory_ntlm_password_check: LM password, NT MD4 password in LM field and LMv2 failed for user %s\n",username));
	} else {
		DEBUG(3,("opendirectory_ntlm_password_check: LM password and LMv2 failed for user %s, and NT MD4 password in LM field not permitted\n",username));
	}
	return NT_STATUS_WRONG_PASSWORD;
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
				DATA_BLOB *user_sess_key, 
				DATA_BLOB *lm_sess_key)
{
	uint16 acct_ctrl;
	const char *username = pdb_get_username(sampass);

	acct_ctrl = pdb_get_acct_ctrl(sampass);
	if (acct_ctrl & ACB_PWNOTREQ) {
		if (lp_null_passwords()) {
			DEBUG(3,("Account for user '%s' has no password and null passwords are allowed.\n", username));
			return NT_STATUS_OK;
		} else {
			DEBUG(3,("Account for user '%s' has no password and null passwords are NOT allowed.\n", username));
			return NT_STATUS_LOGON_FAILURE;
		}		
	}

	return opendirectory_opendirectory_ntlm_password_check(dirRef, userNode, mem_ctx, &auth_context->challenge, 
				   &user_info->lm_resp, &user_info->nt_resp, 
				   &user_info->lm_interactive_pwd, &user_info->nt_interactive_pwd,
				   username, 
				   user_info->smb_name.str, 
				   user_info->client_domain.str, 
				   user_sess_key, lm_sess_key);
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
	DATA_BLOB user_sess_key = data_blob(NULL, 0);
	DATA_BLOB lm_sess_key = data_blob(NULL, 0);
    tDirStatus		dirStatus		= eDSNoErr;
    tDirReference	dirRef		= 0;
    tDirNodeReference	userNodeRef	= 0;

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

	if (ret == False)
	{
		DEBUG(3,("Couldn't find user '%s' in passdb file.\n", user_info->internal_username.str));
		pdb_free_sam(&sampass);
		return NT_STATUS_NO_SUCH_USER;
	}

	nt_status = opendirectory_account_ok(mem_ctx, sampass, user_info);
	
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
    
    if (userNodeRef != 0)
    {
		nt_status = opendirectory_password_ok(dirRef, userNodeRef, auth_context, mem_ctx, sampass, user_info, &user_sess_key, &lm_sess_key);
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
#if WITH_SACL
	if (check_sacl(pdb_get_username(sampass), kSMBServiceACL) == 0)
	{
		DEBUG(0,("check_opendirectory_security: check_sacl(%s, smb) failed \n", pdb_get_username(sampass)));
		return NT_STATUS_WRONG_PASSWORD;	
	}
#endif
	if (!NT_STATUS_IS_OK(nt_status = make_server_info_sam(server_info, sampass))) {		
		DEBUG(0,("check_opendirectory_security: make_server_info_sam() failed with '%s'\n", nt_errstr(nt_status)));
		return nt_status;
	}

	(*server_info)->user_session_key = user_sess_key;
	(*server_info)->lm_session_key = lm_sess_key;

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
