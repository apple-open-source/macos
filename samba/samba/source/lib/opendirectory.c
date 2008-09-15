/*
 * opendirectory.c
 *
 * Copyright (C) 2003-2007 Apple Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include "opendirectory.h"

/* ======================================================================= */
/* Miscellaneous APIs and utilities					   */
/* ======================================================================= */

static CFDictionaryRef sam_searchattr_first(
				struct opendirectory_session *session,
				const char *type,
				const char *attr,
				const char *value);
static CFDictionaryRef sam_searchname_first(
				struct opendirectory_session *session,
				const char *type,
				const char *name);

/* Data free API. Most (all?) of the ds* API will safely ignore a NULL or zero
 * argument. The only reason we need this is that tDataListPtr needs
 * to be freed twice (!?!).
 */
 void opendirectory_free_list(struct opendirectory_session *session,
			    tDataListPtr list)
{
	if (list != NULL) {
		dsDataListDeallocate(session->ref, list);
		free(list);
	}
}

 void opendirectory_free_node(struct opendirectory_session *session,
			    tDataNodePtr node)
{
	 if (node != NULL) {
		dsDataNodeDeAllocate(session->ref, node);
	 }
}

 void opendirectory_free_buffer(struct opendirectory_session *session,
			    tDataBufferPtr buffer)
{
	if (buffer != NULL) {
		dsDataBufferDeAllocate(session->ref, buffer);
	}
}

/* Connection / reconnection API. */

 tDirStatus opendirectory_open_node(struct opendirectory_session *session,
                        const char* nodeName,
                        tDirNodeReference *nodeReference)
{
        tDirStatus      status  = eDSInvalidReference;
        tDataListPtr    node    = NULL;

        SMB_ASSERT(nodeReference != NULL);

        node = dsBuildFromPath(session->ref, nodeName, "/");
	if (node == NULL) {
		return eDSAllocationFailed;
	}

        status = dsOpenDirNode(session->ref, node, nodeReference);
        LOG_DS_ERROR(DS_TRACE_ERRORS, status, "dsOpenDirNode");

        opendirectory_free_list(session, node);
        return status;
}

 tDirStatus opendirectory_connect(struct opendirectory_session *session)
{
	tDirStatus dirStatus;

	ZERO_STRUCTP(session);
	session->pid = sys_getpid();
	dirStatus = dsOpenDirService(&session->ref);
	return dirStatus;
}

 tDirStatus opendirectory_reconnect(struct opendirectory_session *session)
{
	pid_t current = sys_getpid();

	/* We need to reconnect if a session survived across a fork.
	 * NOTE: the search node is NOT preserved.
	 */
	if (session->ref == 0 || current != session->pid) {
		DS_CLOSE_NODE(session->search);
		return opendirectory_connect(session);
	}

#if 0
	/* We really should call dsVerifyDirRefNum here, but it's far to
	 * expensive to do on a regular basis.
	 */

	if (dsVerifyDirRefNum(session->ref) != eDSNoErr) {
		return opendirectory_connect(session);
	}
#endif

	return eDSNoErr;
}

 void opendirectory_disconnect(struct opendirectory_session *session)
{
	DS_CLOSE_NODE(session->search);
	dsCloseDirService(session->ref);
	ZERO_STRUCTP(session);
}

/* ======================================================================= */
/* Authentication APIs and utilities					   */
/* ======================================================================= */

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

#define credentialfile "/var/db/samba/opendirectorysam"
#define opendirectory_secret_sig 'odsa'

typedef struct opendirectory_secret_header {
    u_int32_t signature;
    u_int32_t authenticator_len;
    u_int32_t secret_len;
    u_int32_t authenticatorid_len;
} opendirectory_secret_header;

#ifdef USES_KEYCHAIN
/* Stop the Security framework defining fixed-sized types that we already
 * define. Our definitions are incompatible since they are #defined rather
 * that typedef'd.
 */
#define _UINT64
#define _UINT32
#define _UINT16
#define _UINT8
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <SystemConfiguration/SystemConfiguration.h>
//#include <CoreServices/CoreServices.h>

#define KEYCHAIN_SERVICE "com.apple.samba"
#define SAMBA_APP_ID CFSTR("com.apple.samba")

static char * get_admin_account(void)
{
	CFPropertyListRef	pref = NULL;
	char *			account = NULL;
	int			accountLength = 1024;

	pref = CFPreferencesCopyAppValue (CFSTR("DomainAdmin"), SAMBA_APP_ID);
	if (pref != 0) {
		if (CFGetTypeID(pref) == CFStringGetTypeID()) {
			account = calloc(1, accountLength);
			if (!CFStringGetCString((CFStringRef)pref, account,
				    accountLength, kCFStringEncodingUTF8)) {
				free(account);
				account = NULL;
			}
		}
		CFRelease(pref);
	}

	return account;
}

static void *get_password_from_keychain(char *account, int accountLength)
{
	OSStatus status ;
	SecKeychainItemRef item;
	void *passwordData = NULL;
	UInt32 passwordLength = 0;
	void *password = NULL;

	// Set the domain to System (daemon)
	status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
	status = SecKeychainGetUserInteractionAllowed (False);

	 status = SecKeychainFindGenericPassword (
					 NULL,           // default keychain
					 strlen(KEYCHAIN_SERVICE),             // length of service name
					 KEYCHAIN_SERVICE,   // service name
					 accountLength,             // length of account name
					 account,   // account name
					 &passwordLength,  // length of password
					 &passwordData,   // pointer to password data
					 &item         // the item reference
					);

	if ((status == noErr) && (item != NULL) && passwordLength)
	{
		password = calloc(1, passwordLength + 1);
		memcpy(password, (const void *)passwordData, passwordLength);
	}
	return password;
}

#endif /* USES_KEYCHAIN */

static opendirectory_secret_header *
get_opendirectory_secret_header(void *authenticator)
{
	opendirectory_secret_header *hdr;

	if (authenticator) {
		hdr = (opendirectory_secret_header *)authenticator;
		if (hdr->signature == opendirectory_secret_sig) {
			return hdr;
		}
	}
	return NULL;
}

static void * get_opendirectory_authenticator(void)
{
	void *authenticator = NULL;
	int authentriessize = 0;

#ifdef USES_KEYCHAIN
	opendirectory_secret_header *odhdr = NULL;
	char *password = NULL;
	char *account = NULL;

	account = get_admin_account();
	if (!account) {
		return NULL;
	}

	password = get_password_from_keychain(account, strlen(account));
	if (!password) {
		free(account);
		return NULL;
	}

	authentriessize = strlen(account) + strlen(password);
	authenticator = calloc(1, sizeof(opendirectory_secret_header) +
					authentriessize);
	memcpy((uint8_t *)authenticator + sizeof(opendirectory_secret_header),
		account, strlen(account));
	memcpy((uint8_t *)authenticator + sizeof(opendirectory_secret_header)
					+ strlen(account),
		password, strlen(password));
	odhdr = (opendirectory_secret_header*)authenticator;
	odhdr->authenticator_len = strlen(account);
	odhdr->secret_len = strlen(password);
	odhdr->signature = opendirectory_secret_sig;

	free(password);
	return authenticator;

#else /* USES_KEYCHAIN */

	int fd = -1;
	ssize_t len;
	opendirectory_secret_header hdr;

	fd = open(credentialfile, O_RDONLY,0);
	if (fd == -1) {
		DEBUG(0, ("unable to open %s (%s)\n",
			    credentialfile, strerror(errno)));
		return NULL;
	}

	len = read(fd, &hdr, sizeof(opendirectory_secret_header));
	if (len != sizeof(opendirectory_secret_header)) {
		goto cleanup;
	}

	if (hdr.signature != opendirectory_secret_sig) {
		goto cleanup;
	}

	authentriessize = hdr.authenticator_len + hdr.secret_len;
	authenticator = malloc(sizeof(opendirectory_secret_header) + authentriessize);
	memset(authenticator, 0, sizeof(opendirectory_secret_header) + authentriessize);
	memcpy(authenticator, &hdr, sizeof(opendirectory_secret_header));

	len = read(fd, (uint8_t *)authenticator +
			sizeof(opendirectory_secret_header), authentriessize);
	if (len != authentriessize) {
		goto cleanup;
	}

	return authenticator;

cleanup:
	if (authenticator) {
		free(authenticator);
	}

	close(fd);
	return NULL;

#endif /* USES_KEYCHAIN */
}


static u_int32_t
authenticator_accountlen(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	u_int32_t len = 0;

	if (hdr)
		len = hdr->authenticator_len;

	return len;
}

static void *
authenticator_account(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	void *result = NULL;

	if (hdr)
		result = (char *)authenticator + sizeof(opendirectory_secret_header);

	return result;
}

static u_int32_t
authenticator_secretlen(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	u_int32_t len = 0;

	if (hdr)
		len = hdr->secret_len;

	return len;

}
static void *
authenticator_secret(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	void *result = NULL;

	if (hdr)
		result = (char *)authenticator + sizeof(opendirectory_secret_header) + hdr->authenticator_len;

	return result;
}

static void
delete_opendirectory_authenticator(void*authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);

	if (hdr) {
		bzero(authenticator, sizeof(opendirectory_secret_header) + hdr->authenticator_len + hdr->secret_len);
		free(authenticator);
	}
}

static tDirStatus get_node_ref_and_name(
			struct opendirectory_session *session,
			const char *name, const char *recordType,
			tDirNodeReference *nodeRef, char *outRecordName)
{
	tDirStatus		status			= eDSNoErr;
	unsigned long		returnCount		= 0;
	tDataBufferPtr		nodeBuffer		= NULL;
	tDataListPtr		NodePath		= NULL;
	char			NodePathStr[256]	= {0};

	tDataListPtr		recName			= NULL;
	tDataListPtr		recType			= NULL;
	tDataListPtr		attrType		= NULL;

	tAttributeListRef	attributeListRef	= 0;
	tRecordEntryPtr		outRecordEntryPtr	= NULL;
	tAttributeEntryPtr	attributeInfo		= NULL;
	tAttributeValueListRef	attributeValueListRef	= 0;
	tAttributeValueEntryPtr	attrValue		= NULL;
	long			i			= 0;

	*nodeRef = 0;
	*outRecordName = '\0';

	nodeBuffer = DS_DEFAULT_BUFFER(session->ref);
	if (nodeBuffer == NULL) {
		goto cleanup;
	}

	status = opendirectory_searchnode(session);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	recName = dsBuildListFromStrings(session->ref, name, NULL);
	recType = dsBuildListFromStrings(session->ref, recordType, NULL);
	attrType = dsBuildListFromStrings(session->ref,
			kDSNAttrMetaNodeLocation, kDSNAttrRecordName, NULL);

	status = dsGetRecordList(session->search, nodeBuffer, recName,
			eDSiExact, recType, attrType, 0, &returnCount, NULL);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	status = dsGetRecordEntry(session->search, nodeBuffer, 1,
		&attributeListRef, &outRecordEntryPtr);
	if (status != eDSNoErr) {
		goto cleanup;
	}

        for (i = 1 ; i <= outRecordEntryPtr->fRecordAttributeCount; i++) {
            status = dsGetAttributeEntry(session->search, nodeBuffer,
				    attributeListRef, i,
				    &attributeValueListRef, &attributeInfo);
            status = dsGetAttributeValue(session->search, nodeBuffer, 1,
				    attributeValueListRef, &attrValue);
            if (attributeValueListRef != 0) {
                    dsCloseAttributeValueList(attributeValueListRef);
                    attributeValueListRef = 0;
            }

	    if (status != eDSNoErr) {
		    goto cleanup;
	    }

	    if (strncmp(attributeInfo->fAttributeSignature.fBufferData,
			kDSNAttrMetaNodeLocation,
			strlen(kDSNAttrMetaNodeLocation)) == 0) {
		    SMB_ASSERT(attrValue->fAttributeValueData.fBufferSize < sizeof(NodePathStr));
		    NodePathStr[sizeof(NodePathStr) -1] = '\0';
		    strncpy(NodePathStr,
			    attrValue->fAttributeValueData.fBufferData,
			    sizeof(NodePathStr) -1);
	    } else if (strncmp(attributeInfo->fAttributeSignature.fBufferData,
			kDSNAttrRecordName, strlen(kDSNAttrRecordName)) == 0) {

		    /* XXX: We should be passing the record size. */
		    SMB_ASSERT(attrValue->fAttributeValueData.fBufferSize < 256);
		    outRecordName[255 - 1] = '\0';
		    strncpy(outRecordName,
			    attrValue->fAttributeValueData.fBufferData,
			    256 - 1);
	    }

            if (attrValue != NULL) {
                    dsDeallocAttributeValueEntry(session->ref, attrValue);
                    attrValue = NULL;
            }
            if (attributeInfo != NULL) {
                    dsDeallocAttributeEntry(session->ref, attributeInfo);
                    attributeInfo = NULL;
            }
        }

        if (outRecordEntryPtr != NULL) {
            dsDeallocRecordEntry(session->ref, outRecordEntryPtr);
            outRecordEntryPtr = NULL;
        }

        if (strlen(NodePathStr) != 0 && strlen(outRecordName) != 0) {
		NodePath = dsBuildFromPath(session->ref, NodePathStr, "/");
		status = dsOpenDirNode(session->ref, NodePath, nodeRef);
		opendirectory_free_list(session, NodePath);
        }

cleanup:

	opendirectory_free_buffer(session, nodeBuffer);
	opendirectory_free_list(session, recName);
	opendirectory_free_list(session, recType);
	opendirectory_free_list(session, attrType);

	return status;
}

static void
opendirectory_add_data_buffer_item(tDataBufferPtr dataBuffer,
			    u_int32_t len, const void *buffer)
{

	if (dataBuffer->fBufferLength + len + 4 > dataBuffer->fBufferSize) {
		return;
	}

	memcpy(&(dataBuffer->fBufferData[dataBuffer->fBufferLength]), &len, 4);
	dataBuffer->fBufferLength += 4;

	if (len != 0) {
		memcpy(&(dataBuffer->fBufferData[dataBuffer->fBufferLength]),
			buffer, len);
		dataBuffer->fBufferLength += len;
	}
}

 tDirStatus opendirectory_cred_session_key(const DOM_CHAL *client_challenge,
			    const DOM_CHAL *server_challenge,
			    const char *machine_acct,
			    char *session_key)
{
	struct opendirectory_session session;
	tDirStatus 		status			= eDSNoErr;
	unsigned long		curr			= 0;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = 0;
	const char *targetaccount = NULL;
	char recordName[256] = {0};

	status = opendirectory_connect(&session);
	if (status != eDSNoErr) {
		return status;
	}

	status = get_node_ref_and_name(&session, machine_acct,
			kDSStdRecordTypeComputers, &nodeRef, recordName);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	status = opendirectory_authenticate_node_r(&session, nodeRef);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	targetaccount = recordName;

	authBuff = DS_DEFAULT_BUFFER(session.ref);
	if (authBuff == NULL) {
		goto cleanup;
	}

	authBuff->fBufferLength = 0;
	stepBuff = DS_DEFAULT_BUFFER(session.ref);
	if (stepBuff == NULL) {
		goto cleanup;
	}

	authType = dsDataNodeAllocateString(session.ref,
			    kDSStdAuthSMBWorkstationCredentialSessionKey);
	recordType = dsDataNodeAllocateString(session.ref,
			    kDSStdRecordTypeComputers);
	if (authType == NULL || recordType == NULL) {
		goto cleanup;
	}

	// Target account
	len = strlen(targetaccount);
	memcpy(&(authBuff->fBufferData[ curr ]), &len, 4);
	curr += sizeof(long);
	memcpy(&(authBuff->fBufferData[ curr ]), targetaccount, len);
	curr += len;
	// Client Challenge and Server Challenge
	len = 16;
	memcpy(&(authBuff->fBufferData[ curr ]), &len, 4);
	curr += sizeof(long);
	memcpy(&(authBuff->fBufferData[ curr ]), server_challenge->data, 8);
	curr += 8;
	memcpy(&(authBuff->fBufferData[ curr ]), client_challenge->data, 8);
	curr += 8;

	authBuff->fBufferLength = curr;
	status = dsDoDirNodeAuthOnRecordType(nodeRef, authType, 1, authBuff,
			    stepBuff, NULL,  recordType);

	if (status == eDSNoErr) {
		memcpy(&len, stepBuff->fBufferData, 4);
		stepBuff->fBufferData[len+4] = '\0';
		memcpy(session_key,stepBuff->fBufferData+4, 8);
	}

cleanup:

	opendirectory_free_buffer(&session, stepBuff);
	opendirectory_free_buffer(&session, authBuff);
	opendirectory_free_node(&session, authType);
	opendirectory_free_node(&session, recordType);

	DS_CLOSE_NODE(nodeRef);
	opendirectory_disconnect(&session);

	return status;
}

 tDirStatus opendirectory_user_auth_and_session_key(
			struct opendirectory_session *session,
			tDirNodeReference inUserNodeRef,
			const char *account_name,
			u_int8_t *challenge,
			u_int8_t *client_response,
			u_int8_t *session_key,
			u_int32_t *key_length)
{
	tDirStatus 		status			= eDSNoErr;
	u_int32_t		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	void *			authenticator = NULL;

	authenticator = get_opendirectory_authenticator();

#ifdef AUTH_NODE_BEFORE_AUTH_AND_SESS_KEY
	status = opendirectory_authenticate_node_r(session->ref, inUserNodeRef);

	if (status != eDSNoErr)
		goto cleanup;
#endif

	authBuff = DS_DEFAULT_BUFFER(session->ref);
	if (authBuff == NULL) {
		goto cleanup;
	}
	authBuff->fBufferLength = 0;
	stepBuff = DS_DEFAULT_BUFFER(session->ref);
	if (stepBuff == NULL) {
		goto cleanup;
	}

	/*
	 * New auth method:
	 * "dsAuthMethodStandard:dsAuthNTWithSessionKey"
	 *
	 * Buffer format:
	 * 4 byte len + user name/ID
	 * 4 byte len + challenge C8
	 * 4 byte len + response P24
	 * 4 byte len + authenticator name/ID
	 * 4 byte len + authenticator password
	 */

	authType = dsDataNodeAllocateString(session->ref,
			    "dsAuthMethodStandard:dsAuthNTWithSessionKey");
	recordType = dsDataNodeAllocateString(session->ref,
			    kDSStdRecordTypeUsers);
	if (authType == NULL || recordType == NULL) {
		goto cleanup;
	}

	// Target account
	opendirectory_add_data_buffer_item(authBuff, strlen(account_name),
					account_name);
	opendirectory_add_data_buffer_item(authBuff, 8, challenge);
	opendirectory_add_data_buffer_item(authBuff, 24, client_response);

	if (authenticator != NULL) {
		opendirectory_add_data_buffer_item(authBuff,
			authenticator_accountlen(authenticator),
			authenticator_account(authenticator));
		opendirectory_add_data_buffer_item(authBuff,
			authenticator_secretlen(authenticator),
			authenticator_secret(authenticator));
	}

	status = dsDoDirNodeAuthOnRecordType(inUserNodeRef, authType, 1,
		    authBuff, stepBuff, NULL,  recordType);
	LOG_DS_ERROR(DS_TRACE_ERRORS, status, "dsDoDirNodeAuthOnRecordType");
	if (status == eDSNoErr && stepBuff->fBufferLength >= 4) {
		memcpy(&len, stepBuff->fBufferData, 4);
		assert (len == 16);
		*key_length = len;
		stepBuff->fBufferData[len+4] = '\0';
		memcpy(session_key,stepBuff->fBufferData+4, len);
	} else {
		*key_length = 0;
	}

cleanup:

	opendirectory_free_buffer(session, stepBuff);
	opendirectory_free_buffer(session, authBuff);
	opendirectory_free_node(session, authType);
	opendirectory_free_node(session, recordType);

	return status;
}

 tDirStatus opendirectory_user_session_key(
			struct opendirectory_session *session,
			tDirNodeReference inUserNodeRef,
			const char *account_name,
			u_int8_t *session_key)
{
	tDirStatus 		status		= eDSNoErr;
	unsigned long		len		= 0;
	tDataBufferPtr		authBuff  	= NULL;
	tDataBufferPtr		stepBuff  	= NULL;
	tDataNodePtr		authType	= NULL;
	tDataNodePtr		recordType	= NULL;
	tDirNodeReference nodeRef = inUserNodeRef;

	status = opendirectory_authenticate_node_r(session, nodeRef);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	authBuff = DS_DEFAULT_BUFFER(session->ref);
	if (authBuff == NULL) {
		goto cleanup;
	}

	authBuff->fBufferLength = 0;
	stepBuff = DS_DEFAULT_BUFFER(session->ref);
	if (stepBuff == NULL) {
		goto cleanup;
	}

	authType = dsDataNodeAllocateString(session->ref,
			    kDSStdAuthSMB_NT_UserSessionKey);
	recordType = dsDataNodeAllocateString(session->ref,
			    kDSStdRecordTypeUsers);
	if (authType == NULL) {
		goto cleanup;
	}

	// Target account
	opendirectory_add_data_buffer_item(authBuff, strlen(account_name),
				    account_name);
	/* null terminate ?? */
	opendirectory_add_data_buffer_item(authBuff, 1, "");

	status = dsDoDirNodeAuthOnRecordType(nodeRef, authType, 1, authBuff,
				stepBuff, NULL,  recordType);
	if (status == eDSNoErr) {
		memcpy(&len, stepBuff->fBufferData, 4);
		stepBuff->fBufferData[len+4] = '\0';
		memcpy(session_key, stepBuff->fBufferData + 4, len);
	}

cleanup:

	opendirectory_free_buffer(session, stepBuff);
	opendirectory_free_buffer(session, authBuff);
	opendirectory_free_node(session, authType);
	opendirectory_free_node(session, recordType);

	return status;
}

 tDirStatus opendirectory_ntlmv2user_session_key(const char *account_name,
			u_int32_t ntv2response_len,
			u_int8_t *ntv2response,
			const char *domain,
			u_int32_t *session_key_len,
			u_int8_t *session_key)
{
	struct opendirectory_session session;
	tDirStatus 		status			= eDSNoErr;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = 0;
	char recordName[256] = {0};

	status = opendirectory_connect(&session);
	if (status != eDSNoErr) {
		return status;
	}

	status = get_node_ref_and_name(&session, account_name,
			    kDSStdRecordTypeUsers, &nodeRef, recordName);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	status = opendirectory_authenticate_node_r(&session, nodeRef);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	authBuff = DS_DEFAULT_BUFFER(session.ref);
	if (authBuff == NULL) {
		goto cleanup;
	}

	authBuff->fBufferLength = 0;
	stepBuff = DS_DEFAULT_BUFFER(session.ref);
	if (stepBuff == NULL) {
		goto cleanup;
	}

	authType = dsDataNodeAllocateString(session.ref,
			/* "dsAuthMethodStandard:dsAuthNodeNTLMv2SessionKey" */
			kDSStdAuthSMBNTv2UserSessionKey);
	recordType = dsDataNodeAllocateString(session.ref,
			kDSStdRecordTypeUsers);
	if (authType == NULL || recordType == NULL) {
		goto cleanup;
	}

	/*
	 * The buffer format is:
	 * 4 byte len + Directory Services user name
	 * 4 byte len + client blob
	 * 4 byte len + username
	 * 4 byte len + domain
	 */
	// Target account
	opendirectory_add_data_buffer_item(authBuff, strlen(recordName),
					    recordName);
	opendirectory_add_data_buffer_item(authBuff, ntv2response_len,
					    ntv2response);
	opendirectory_add_data_buffer_item(authBuff, strlen(recordName),
					    recordName);
	opendirectory_add_data_buffer_item(authBuff, strlen(domain), domain);

	status = dsDoDirNodeAuthOnRecordType(nodeRef, authType, 1, authBuff,
					    stepBuff, NULL,  recordType);
	LOG_DS_ERROR(DS_TRACE_ERRORS, status, "dsDoDirNodeAuthOnRecordType");
	if (status == eDSNoErr) {
		memcpy(&len, stepBuff->fBufferData, 4);
		stepBuff->fBufferData[len+4] = '\0';
		*session_key_len = len;
		memcpy(session_key,stepBuff->fBufferData+4, len);
	}

cleanup:

	opendirectory_free_buffer(&session, stepBuff);
	opendirectory_free_buffer(&session, authBuff);
	opendirectory_free_node(&session, authType);
	opendirectory_free_node(&session, recordType);

	DS_CLOSE_NODE(nodeRef);
	opendirectory_disconnect(&session);
	return status;
}

 tDirStatus opendirectory_set_workstation_nthash(const char *account_name,
		    const char *nt_hash)
{
	struct opendirectory_session session;
	tDirStatus 		status		= eDSNoErr;
	tDataBufferPtr		authBuff  	= NULL;
	tDataBufferPtr		stepBuff  	= NULL;
	tDataNodePtr		authType	= NULL;
	tDataNodePtr		recordType	= NULL;
	tDirNodeReference nodeRef = 0;
	char recordName[256] = {0};

	status = opendirectory_connect(&session);
	if (status != eDSNoErr) {
		return status;
	}

	status = get_node_ref_and_name(&session, account_name,
			kDSStdRecordTypeComputers, &nodeRef, recordName);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	status = opendirectory_authenticate_node_r(&session, nodeRef);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	authBuff = DS_DEFAULT_BUFFER(session.ref);
	if (authBuff == NULL) {
		goto cleanup;
	}

	authBuff->fBufferLength = 0;
	stepBuff = DS_DEFAULT_BUFFER(session.ref);
	if (stepBuff == NULL) {
		goto cleanup;
	}

	authType = dsDataNodeAllocateString(session.ref,
				/* kDSStdAuthSetNTHash */
				kDSStdAuthSetWorkstationPasswd);
	recordType = dsDataNodeAllocateString(session.ref,
				kDSStdRecordTypeComputers);

	if (authType == NULL || recordType == NULL) {
		goto cleanup;
	}

	DEBUG(4, ("setting NT hash for %s\n", recordName));
	opendirectory_add_data_buffer_item(authBuff, strlen(recordName),
						recordName);
	opendirectory_add_data_buffer_item(authBuff, 16, nt_hash);

	status = dsDoDirNodeAuthOnRecordType(nodeRef, authType, 1,
			    authBuff, stepBuff, NULL,  recordType);
	LOG_DS_ERROR(DS_TRACE_ERRORS, status, "dsDoDirNodeAuthOnRecordType");

cleanup:

	opendirectory_free_buffer(&session, stepBuff);
	opendirectory_free_buffer(&session, authBuff);
	opendirectory_free_node(&session, authType);
	opendirectory_free_node(&session, recordType);

	DS_CLOSE_NODE(nodeRef);
	opendirectory_disconnect(&session);
	return status;
}

 tDirStatus opendirectory_lmchap2changepasswd(const char *account_name,
		    const char *passwordData,
		    const char *passwordHash,
		    u_int8_t passwordFormat,
		    const char *slot_id)
{
	struct opendirectory_session session;
	tDirStatus 		status			= eDSNoErr;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = 0;
	const char *targetaccount = NULL;
	char recordName[256] = {0};

	status = opendirectory_connect(&session);
	if (status != eDSNoErr) {
		return status;
	}

	status = get_node_ref_and_name(&session, account_name,
			    kDSStdRecordTypeUsers, &nodeRef, recordName);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	status = opendirectory_authenticate_node_r(&session, nodeRef);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	if (slot_id && *slot_id) {
		targetaccount = slot_id;
	} else {
		targetaccount = recordName;
	}

	authBuff = DS_DEFAULT_BUFFER(session.ref);
	if (authBuff == NULL) {
		goto cleanup;
	}

	authBuff->fBufferLength = 0;
	stepBuff = DS_DEFAULT_BUFFER(session.ref);
	if (stepBuff == NULL) {
		goto cleanup;
	}

	authType = dsDataNodeAllocateString(session.ref,
		    /*kDSStdAuthMSLMCHAP2ChangePasswd*/
		    "dsAuthMethodStandard:dsAuthMSLMCHAP2ChangePasswd");
	recordType = dsDataNodeAllocateString(session.ref,
		kDSStdRecordTypeUsers);
	if (authType == NULL) {
		goto cleanup;
	}

	// Target account
	opendirectory_add_data_buffer_item(authBuff, strlen(targetaccount),
						targetaccount);
	opendirectory_add_data_buffer_item(authBuff, 1, &passwordFormat);
	opendirectory_add_data_buffer_item(authBuff, 516, passwordData);

	status = dsDoDirNodeAuthOnRecordType(nodeRef, authType, 1,
		    authBuff, stepBuff, NULL,  recordType);

cleanup:

	opendirectory_free_buffer(&session, stepBuff);
	opendirectory_free_buffer(&session, authBuff);
	opendirectory_free_node(&session, authType);
	opendirectory_free_node(&session, recordType);

	DS_CLOSE_NODE(nodeRef);
	opendirectory_disconnect(&session);
	return status;
}

 tDirStatus opendirectory_authenticate_node(
			struct opendirectory_session *session,
			tDirNodeReference nodeRef)
{
	tDirStatus 		status		= eDSNoErr;
	tDataBufferPtr		authBuff  	= NULL;
	tDataBufferPtr		stepBuff  	= NULL;
	tDataNodePtr		authType	= NULL;
	tDataNodePtr		recordType	= NULL;
	void *			authenticator = NULL;

	authenticator = get_opendirectory_authenticator();

	if (authenticator == NULL ||
	    authenticator_accountlen(authenticator) == 0 ||
	    authenticator_secretlen(authenticator) == 0) {
		return eDSNullParameter;
	}

	authBuff = DS_DEFAULT_BUFFER(session->ref);
	if (authBuff == NULL) {
		status = eDSDeAllocateFailed;
		goto cleanup;
	}

	authBuff->fBufferLength = 0;
	stepBuff = DS_DEFAULT_BUFFER(session->ref);
	if (stepBuff == NULL) {
		status = eDSDeAllocateFailed;
		goto cleanup;
	}

	authType = dsDataNodeAllocateString(session->ref,
			kDSStdAuthNodeNativeClearTextOK);
	recordType = dsDataNodeAllocateString(session->ref,
			kDSStdRecordTypeUsers);

	if (authType == NULL || recordType == NULL) {
		status = eDSDeAllocateFailed;
		goto cleanup;
	}

	// Account Name (authenticator)
	opendirectory_add_data_buffer_item(authBuff,
		authenticator_accountlen(authenticator),
		authenticator_account(authenticator));

	// Password (authenticator password)
	opendirectory_add_data_buffer_item(authBuff,
		authenticator_secretlen(authenticator),
		authenticator_secret(authenticator));

	status = dsDoDirNodeAuthOnRecordType(nodeRef, authType, 0,
			authBuff, stepBuff, NULL,  recordType);

cleanup:

	opendirectory_free_buffer(session, stepBuff);
	opendirectory_free_buffer(session,authBuff);
	opendirectory_free_node(session, authType);
	opendirectory_free_node(session, recordType);

	delete_opendirectory_authenticator(authenticator);
	return status;
}
/*
 * opendirectory_authenticate_node_r uses dsAuthMethodStandard:dsAuthNodeNativeRetainCredential
 * to provide another level of granularity where you need multi-master access to Password Server authentication methods
 * but NOT write access to the single write enabled LDAP server on an OD Master.
 */
 tDirStatus opendirectory_authenticate_node_r(
			struct opendirectory_session *session,
			tDirNodeReference nodeRef)
{
	tDirStatus 		status		= eDSNoErr;
	tDataBufferPtr		authBuff  	= NULL;
	tDataBufferPtr		stepBuff  	= NULL;
	tDataNodePtr		authType	= NULL;
	tDataNodePtr		recordType	= NULL;
	void *			authenticator = NULL;

	authenticator = get_opendirectory_authenticator();

	if (authenticator == NULL ||
	    authenticator_accountlen(authenticator) == 0 ||
	    authenticator_secretlen(authenticator) == 0) {
		return eDSNullParameter;
	}

	authBuff = DS_DEFAULT_BUFFER(session->ref);
	if (authBuff == NULL) {
		status = eDSDeAllocateFailed;
		goto cleanup;
	}

	authBuff->fBufferLength = 0;
	stepBuff = DS_DEFAULT_BUFFER(session->ref);
	if (stepBuff == NULL) {
		status = eDSDeAllocateFailed;
		goto cleanup;
	}

	authType = dsDataNodeAllocateString(session->ref,
			"dsAuthMethodStandard:dsAuthNodeNativeRetainCredential");
	recordType = dsDataNodeAllocateString(session->ref,
			kDSStdRecordTypeUsers);

	if (authType == NULL || recordType == NULL) {
		status = eDSDeAllocateFailed;
		goto cleanup;
	}

	// Account Name (authenticator)
	opendirectory_add_data_buffer_item(authBuff,
		authenticator_accountlen(authenticator),
		authenticator_account(authenticator));

	// Password (authenticator password)
	opendirectory_add_data_buffer_item(authBuff,
		authenticator_secretlen(authenticator),
		authenticator_secret(authenticator));

	status = dsDoDirNodeAuthOnRecordType(nodeRef, authType, 1,
			authBuff, stepBuff, NULL,  recordType);

cleanup:

	opendirectory_free_buffer(session, stepBuff);
	opendirectory_free_buffer(session,authBuff);
	opendirectory_free_node(session, authType);
	opendirectory_free_node(session, recordType);

	delete_opendirectory_authenticator(authenticator);
	return status;
}

/* ======================================================================= */
/* ID mapping APIs and utilities					   */
/* ======================================================================= */

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

static CFStringRef
opendirectory_attr_to_cfstr(tAttributeEntryPtr attr)
{
	if (attr == NULL ||
	    attr->fAttributeSignature.fBufferLength == 0) {
		return NULL;
	}

	return CFStringCreateWithBytes(NULL,
			(const u_int8_t*)attr->fAttributeSignature.fBufferData,
			attr->fAttributeSignature.fBufferLength,
			kCFStringEncodingUTF8,
			False);
}

static CFStringRef
opendirectory_valattr_to_cfstr(tAttributeValueEntryPtr valattr)
{
	if (valattr == NULL ||
	    valattr->fAttributeValueData.fBufferLength == 0) {
		return NULL;
	}

	return CFStringCreateWithBytes(NULL,
		    (const u_int8_t*)valattr->fAttributeValueData.fBufferData,
		    valattr->fAttributeValueData.fBufferLength,
		    kCFStringEncodingUTF8,
		    False);
}

static CFMutableDictionaryRef create_sam_record(tDirNodeReference node,
				    tDataBufferPtr dataBuffer,
				    tAttributeListRef attributeList,
				    tRecordEntryPtr recordEntry)
{
	CFMutableDictionaryRef  dsrecord = NULL;
	tAttributeValueListRef 	valueList = 0;
	tAttributeEntryPtr 	attributeEntry = NULL;

	unsigned long		attributeIndex;
	unsigned long		valueIndex;
	tDirStatus		status;

	dsrecord = CFDictionaryCreateMutable(NULL, 0,
			    &kCFCopyStringDictionaryKeyCallBacks,
			    &kCFTypeDictionaryValueCallBacks);
	if (dsrecord == NULL) {
		return NULL;
	}

	/* Walk the returned attributes and build a dictionary of string keys
	 * to array values. This is a best-effort approach - if something
	 * fails, we still want to return as many attributes as possible.
	 */
	for (attributeIndex = 1;
	     attributeIndex <= recordEntry->fRecordAttributeCount;
	     ++attributeIndex) {

		CFMutableArrayRef valueArray = NULL;
		CFStringRef key = NULL;

		/* Get the next attribute from the list. */
		status = dsGetAttributeEntry(node, dataBuffer, attributeList,
					    attributeIndex, &valueList,
					    &attributeEntry);
		LOG_DS_ERROR(DS_TRACE_ERRORS, status,
			"dsGetAttributeEntry");
		if (status != eDSNoErr) {
			goto next_attribute;
		}

		valueArray = CFArrayCreateMutable(NULL, 0,
					&kCFTypeArrayCallBacks);
		if (valueArray == NULL) {
			goto next_attribute;
		}

		/* Build an array of values for this key. */
		for (valueIndex = 1;
		     valueIndex <= attributeEntry->fAttributeValueCount;
		     valueIndex++) {
			tAttributeValueEntryPtr valueEntry = NULL;
			CFStringRef value = NULL;

			status = dsGetAttributeValue(node, dataBuffer,
					valueIndex, valueList, &valueEntry);
			LOG_DS_ERROR(DS_TRACE_ERRORS, status,
				"dsGetAttributeValue");
			if (status != eDSNoErr) {
				continue;
			}

			value = opendirectory_valattr_to_cfstr(valueEntry);
			if (value != NULL) {
				CFArrayAppendValue(valueArray, value);
			}

			if (value) {
				CFRelease(value);
				value = NULL;
			}

			if (valueEntry) {
				dsDeallocAttributeValueEntry(node, valueEntry);
				valueEntry = NULL;
			}
		}

		/* Stash the key/value pair in our record. */
		key = opendirectory_attr_to_cfstr(attributeEntry);
		if (key && CFArrayGetCount(valueArray)) {
			CFDictionaryAddValue(dsrecord, key, valueArray);
		}

next_attribute:
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
			dsDeallocAttributeEntry(node, attributeEntry);
			attributeEntry = NULL;
		}
	}


	if (attributeList) {
		dsCloseAttributeList(attributeList);
	}

	if (recordEntry) {
		dsDeallocRecordEntry(node, recordEntry);
	}

	/* Bail if we were't able to add any record attributes. */
	if (CFDictionaryGetCount(dsrecord) == 0) {
		CFRelease(dsrecord);
		return NULL;
	}

	return dsrecord;
}

 tDirStatus opendirectory_insert_search_results(tDirNodeReference node,
				    CFMutableArrayRef recordsArray,
				    const unsigned long recordCount,
				    tDataBufferPtr dataBuffer)
{
	tAttributeListRef	attributeList;
	tRecordEntryPtr		recordEntry;
	unsigned long		recordIndex;
	CFMutableDictionaryRef	dsrecord;
	tDirStatus status;

	for (recordIndex = 1;
	     recordIndex <= recordCount;
	     recordIndex++) {
		status = dsGetRecordEntry(node, dataBuffer,
				recordIndex, &attributeList,
				&recordEntry);
		LOG_DS_ERROR(DS_TRACE_ERRORS, status,
			"dsGetRecordEntry");
		if (status != eDSNoErr) {
			return status;
		}

		dsrecord = create_sam_record(node,
					    dataBuffer,
					    attributeList,
					    recordEntry);
		if (dsrecord) {
			CFArrayAppendValue(recordsArray, dsrecord);
			CFRelease(dsrecord);
		}
	}

	return eDSNoErr;
}

/* Search the directory for a exact matches given by the record type, search
 * type and search value.
 *
 * If successful, recordsArray is filled in with a list of dictionaries. Each
 * dictionary corresponds to a search result and consists of a number of
 * key/array pairs. The names of the keys correspond to the requested
 * attributes.
 */
static tDirStatus opendirectory_search_attributes(
			struct opendirectory_session *session,
			CFMutableArrayRef recordsArray,
			tDataListPtr recordType,
			tDataNodePtr searchAttr,
			tDataNodePtr searchValue,
			tDataListPtr requestedAttrs)
{
	tDirStatus		status;
	tDataBufferPtr		dataBuffer;

	tContextData		currentContextData = NULL;

	dataBuffer = DS_DEFAULT_BUFFER(session->ref);
	if (dataBuffer == NULL) {
		status = eDSAllocationFailed;
		goto cleanup;
	}

	do {
		unsigned long recordCount;

		status = dsDoAttributeValueSearchWithData(
				session->search, dataBuffer,
				recordType, searchAttr, eDSExact, searchValue,
				requestedAttrs,
				False /* we want attributes and values */,
				&recordCount, &currentContextData);

		LOG_DS_ERROR(DS_TRACE_ERRORS, status,
			"dsDoAttributeValueSearchWithData");
		if (status != eDSNoErr) {
			break;
		}

		opendirectory_insert_search_results(session->search,
				    recordsArray, recordCount, dataBuffer);
	} while (status == eDSNoErr && currentContextData != NULL);

cleanup:
	opendirectory_free_buffer(session, dataBuffer);
	return status;
}

/* Search the directory for a case-insensitive matches given by the
 * record type, and name.
 *
 * If successful, recordsArray is filled in with a list of dictionaries. Each
 * dictionary corresponds to a search result and consists of a number of
 * key/array pairs. The keys correspond the the names of the requested
 * attributes.
 */
static tDirStatus opendirectory_search_names(
			struct opendirectory_session *session,
			tDirNodeReference node,
			CFMutableArrayRef recordsArray,
			tDataListPtr recordType,
			tDataListPtr searchName,
			tDataListPtr requestedAttrs)
{
	tDirStatus		status;
	tDataBufferPtr		dataBuffer;

	tContextData		currentContextData = NULL;

	dataBuffer = DS_DEFAULT_BUFFER(session->ref);
	if (dataBuffer == NULL) {
		status = eDSAllocationFailed;
		goto cleanup;
	}

	do {
		unsigned long recordCount;

		status = dsGetRecordList(node, dataBuffer,
				searchName, eDSiExact, recordType,
				requestedAttrs,
				False /* we want attributes and values */,
				&recordCount, &currentContextData);

		LOG_DS_ERROR(DS_TRACE_ERRORS, status, "dsGetRecordList");
		if (status != eDSNoErr) {
			break;
		}

		opendirectory_insert_search_results(node, recordsArray,
					    recordCount, dataBuffer);

	} while (status == eDSNoErr && currentContextData != NULL);

cleanup:
	opendirectory_free_buffer(session, dataBuffer);
	return status;
}

 tDirStatus opendirectory_sam_searchattr(
				struct opendirectory_session *session,
                            	CFMutableArrayRef *records,
				const char *type,
				const char *attr,
				const char *value)
{
        tDirStatus status;
        tDataListPtr recordType = NULL;
        tDataListPtr samAttributes = NULL;
        tDataNodePtr searchAttr = NULL;
        tDataNodePtr searchValue = NULL;

	DEBUG(8, ("searching %s records for %s = %s\n",
		    type, attr, value));

	*records = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (*records == NULL) {
		return eDSAllocationFailed;
	}

	status = opendirectory_searchnode(session);
	if (status != eDSNoErr) {
		return status;
	}

	SMB_ASSERT(session->ref != 0);
	SMB_ASSERT(session->search != 0);
	if (session->ref == 0 || session->search == 0) {
		return eDSInvalidRefType;
	}

	samAttributes = opendirectory_sam_attrlist(session);
	if (samAttributes == NULL) {
		status = eDSAllocationFailed;
		goto cleanup;
	}

        recordType = dsBuildListFromStrings(session->ref, type, NULL);
        searchAttr = dsDataNodeAllocateString(session->ref, attr);
        searchValue = dsDataNodeAllocateString(session->ref, value);

        if (recordType && searchAttr && searchValue) {
               status = opendirectory_search_attributes(session,
				    *records, recordType,
				    searchAttr, searchValue,
				    samAttributes);
        } else {
		status = eDSAllocationFailed;
	}

	/* We guarantee a result with a successful return. */
	if (CFArrayGetCount(*records) == 0) {
		CFRelease(*records);
		*records = NULL;

		if (status == eDSNoErr) {
			status = eDSRecordNotFound;
		}
	}

cleanup:

	/* Make sure caller doesn't have to clean up on failure. */
	if (status != eDSNoErr && *records != NULL) {
		CFRelease(*records);
		*records = NULL;
	}

        opendirectory_free_list(session, recordType);
        opendirectory_free_list(session, samAttributes);
        opendirectory_free_node(session, searchAttr);
        opendirectory_free_node(session, searchValue);
        return status;
}

/* Look up a record of the given name and type. */
 tDirStatus opendirectory_sam_searchname(
				struct opendirectory_session *session,
                            	CFMutableArrayRef *records,
				const char *type,
				const char *name)
{
        tDirStatus status;
        tDataListPtr recordType = NULL;
        tDataListPtr searchName = NULL;
        tDataListPtr samAttributes = NULL;

	*records = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (*records == NULL) {
		return eDSAllocationFailed;
	}

	status = opendirectory_searchnode(session);
	if (status != eDSNoErr) {
		return status;
	}

	samAttributes = opendirectory_sam_attrlist(session);
	if (samAttributes == NULL) {
		status = eDSAllocationFailed;
		goto cleanup;
	}

	name = name ? name : kDSRecordsAll;
        recordType = dsBuildListFromStrings(session->ref, type, NULL);
        searchName = dsBuildListFromStrings(session->ref, name, NULL);

        if (recordType && searchName) {
               status = opendirectory_search_names(session, session->search,
				*records, recordType, searchName,
				samAttributes);
        } else {
		status = eDSAllocationFailed;
	}

	/* We guarantee a result with a successful return. */
	if (CFArrayGetCount(*records) == 0) {
		CFRelease(*records);
		*records = NULL;
		if (status == eDSNoErr) {
			status = eDSRecordNotFound;
		}
	}

cleanup:

	/* Make sure caller doesn't have to clean up on failure. */
	if (status != eDSNoErr && *records != NULL) {
		CFRelease(*records);
		*records = NULL;
		status = eDSRecordNotFound;
	}

        opendirectory_free_list(session, recordType);
        opendirectory_free_list(session, searchName);
        opendirectory_free_list(session, samAttributes);
        return status;
}

 char * opendirectory_talloc_cfstr(void *talloc_ctx, CFStringRef cfstring)
{
	char * data;
	size_t maxsz;

	if ((data = (char *)CFStringGetCStringPtr(cfstring, kCFStringEncodingUTF8))) {
		return talloc_strdup(talloc_ctx, data);
	}

	maxsz = (CFStringGetLength(cfstring) + 1) * sizeof(UniChar);;
	data = talloc_array(talloc_ctx, char, maxsz);
	if (data == NULL) {
		return NULL;
	}

	/* Note: this might fail because the length is the number of UTF-16
	 * code pairs and we don't know how many bytes each code pair might
	 * consume when converted to UTF8. Generally, the string should be in
	 * UTF8 already, so we will be OK.
	 */
	if (!CFStringGetCString(cfstring, data, maxsz,
				kCFStringEncodingUTF8)) {
		talloc_free(data);
		return NULL;
	}

	return data;
}

/* Walk the record data structure returned by the search API and pull out
 * the named attribute.
 */
 char *opendirectory_get_record_attribute(void *talloc_ctx,
				    CFDictionaryRef record,
				    const char *attribute)
{
        CFStringRef attrRef = NULL;
        const void  *opaque_value = NULL;
        char * result = NULL;

        attrRef = CFStringCreateWithCString(NULL, attribute,
					    kCFStringEncodingUTF8);
	if (!attrRef) {
		return NULL;
	}

        if (!CFDictionaryGetValueIfPresent(record, attrRef, &opaque_value)) {
                goto cleanup;
        }

        if (CFGetTypeID(opaque_value) == CFArrayGetTypeID()) {
                CFArrayRef valueList = (CFArrayRef)opaque_value;
                CFStringRef cfstrRef;

                if (CFArrayGetCount(valueList) == 0) {
                        goto cleanup;
                }

		/* We should not be using this API to look at multi-valued
		 * attributes.
		 */
		if (CFArrayGetCount(valueList) > 1 &&
		    strcmp(attribute, kDSNAttrRecordName) != 0) {
			DEBUG(0, ("WARNING: returning first of %d values "
				"for %s attribute\n",
				CFArrayGetCount(valueList), attribute));
		}

                cfstrRef = (CFStringRef)CFArrayGetValueAtIndex(valueList, 0);
                if (cfstrRef == NULL) {
                        goto cleanup;
                }

		result = opendirectory_talloc_cfstr(talloc_ctx, cfstrRef);

        } else if (CFGetTypeID(opaque_value) == CFStringGetTypeID()) {
                CFStringRef cfstrRef = (CFStringRef)opaque_value;

                if (cfstrRef == NULL) {
                        goto cleanup;
                }

		result = opendirectory_talloc_cfstr(talloc_ctx, cfstrRef);
        }

cleanup:

	CFRelease(attrRef);
	return result;
}

/* Walk the record data structure returned by the search API and return True
 * if "value" matches any of the values of "attribute".
 */
 BOOL opendirectory_match_record_attribute(CFDictionaryRef record,
				    const char *attribute,
				    const char * value)
{
        CFStringRef attrRef = NULL;
        CFStringRef valRef = NULL;
        const void  *opaque_value = NULL;

	BOOL ret = False;

        attrRef = CFStringCreateWithCString(NULL, attribute,
					    kCFStringEncodingUTF8);
	if (!attrRef) {
                goto cleanup;
	}

        valRef = CFStringCreateWithCString(NULL, value,
					    kCFStringEncodingUTF8);
	if (!valRef) {
                goto cleanup;
	}

        if (!CFDictionaryGetValueIfPresent(record, attrRef, &opaque_value)) {
                goto cleanup;
        }

        if (CFGetTypeID(opaque_value) == CFArrayGetTypeID()) {
                CFArrayRef valueList = (CFArrayRef)opaque_value;
                CFStringRef cfstrRef;
		int i;
		CFComparisonResult eq;

		for (i = 0; i < CFArrayGetCount(valueList); ++i) {
			cfstrRef =
			    (CFStringRef)CFArrayGetValueAtIndex(valueList, 0);

			if (cfstrRef == NULL) {
				continue;
			}

			eq = CFStringCompare(valRef, cfstrRef,
				    kCFCompareCaseInsensitive);
			if (eq == kCFCompareEqualTo) {
				ret = True;
				goto cleanup;
			}

		}
        } else if (CFGetTypeID(opaque_value) == CFStringGetTypeID()) {
                CFStringRef cfstrRef = (CFStringRef)opaque_value;
		CFComparisonResult eq;

		eq = CFStringCompare(valRef, cfstrRef,
			    kCFCompareCaseInsensitive);
		if (eq == kCFCompareEqualTo) {
			ret = True;
			goto cleanup;
		}
        }

cleanup:

	if (attrRef) {
		CFRelease(attrRef);
	}

	if (valRef) {
		CFRelease(valRef);
	}

	return ret;
}

 tDirStatus opendirectory_map_unix_to_sid(
				struct opendirectory_session *session,
				const struct unixid *user,
				DOM_SID *sid)
{
	return eNotYetImplemented;
}

 tDirStatus opendirectory_map_sid_to_unix(
				struct opendirectory_session *session,
				const DOM_SID *sid,
				struct unixid *user)
{
	return eNotYetImplemented;
}

 tDataListPtr opendirectory_sam_attrlist(struct opendirectory_session *session)
{
	return dsBuildListFromStrings(session->ref,
			    kDSNAttrRecordName,
			    kDSNAttrRecordType,
			    kDSNAttrMetaNodeLocation,
			    kDSNAttrAuthenticationAuthority,
			    kDS1AttrUniqueID,
			    kDS1AttrPrimaryGroupID,
			    kDS1AttrNFSHomeDirectory,
			    kDS1AttrDistinguishedName,
			    kDS1AttrComment,

			    kDS1AttrSMBRID,
			    kDS1AttrSMBGroupRID,

			    kDS1AttrSMBSID,
			    kDS1AttrSMBPrimaryGroupSID,

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
}

/* Open the Open Directory search node. */
 tDirStatus opendirectory_searchnode(struct opendirectory_session *session)
{
        tDirStatus              status = eDSNoErr;
        unsigned long           returnCount     = 0;
        tDataBufferPtr          nodeBuffer      = NULL;
        tDataListPtr            searchNodeName  = NULL;

	if (session->search != 0) {
		return eDSNoErr;
	}

        nodeBuffer = dsDataBufferAllocate(session->ref, SMALL_DS_BUFFER_SIZE);
        if (nodeBuffer == NULL) {
		return eDSAllocationFailed;
        }

        status = dsFindDirNodes(session->ref, nodeBuffer, NULL,
                        eDSSearchNodeName, &returnCount, NULL);
	LOG_DS_ERROR(DS_TRACE_ERRORS, status, "dsFindDirNodes");
        if (status != eDSNoErr) {
                goto cleanup;
        }

	/* This should never happen. */
	if (returnCount == 0) {
		status = eDSNodeNotFound;
		goto cleanup;
	}

        status = dsGetDirNodeName(session->ref, nodeBuffer, 1,
                                &searchNodeName);
	LOG_DS_ERROR(DS_TRACE_ERRORS, status, "dsGetDirNodeName");
        if (status != eDSNoErr) {
                goto cleanup;
        }

        status = dsOpenDirNode(session->ref, searchNodeName, &session->search);
	LOG_DS_ERROR(DS_TRACE_ERRORS, status, "dsOpenDirNode");

cleanup:
        opendirectory_free_buffer(session, nodeBuffer);
	opendirectory_free_list(session, searchNodeName);
        return status;
}

/* Return a NULL-terminated addar of node paths that are local. This can be
 * used to figure out whether records came from a local DS node or a remote
 * one.
 */
 const char ** opendirectory_local_paths(struct opendirectory_session *session)
{
	tDirStatus	status;
	tDataBufferPtr	nodeBuffer;
	tDataListPtr	localNodeName;
	unsigned long	returnCount;

	int current, i;
	const char ** path_list = NULL;

	nodeBuffer = DS_DEFAULT_BUFFER(session->ref);
	if (!nodeBuffer) {
		return NULL;
	}

	status = dsFindDirNodes(session->ref, nodeBuffer, NULL,
	    eDSLocalNodeNames, &returnCount, NULL);
	LOG_DS_ERROR(DS_TRACE_ERRORS, status, "dsFindDirNodes");
	if (status != eDSNoErr || returnCount == 0) {
		goto cleanup;
	}

	path_list = SMB_CALLOC_ARRAY(const char *, returnCount + 1);
	if (!path_list) {
		goto cleanup;
	}

	for (i = 1, current = 0; i <= returnCount; ++i) {
		status = dsGetDirNodeName(session->ref, nodeBuffer, i,
					&localNodeName);
		LOG_DS_ERROR(DS_TRACE_ERRORS, status, "dsGetDirNodeName");
		if (status != eDSNoErr) {
			continue;
		}

		/* FYI: dsGetPathFromList mallocs it's result. */
		path_list[current] = dsGetPathFromList(session->ref,
						localNodeName, "/" );
		if (path_list[current]) {
			++current;
		}

		opendirectory_free_list(session, localNodeName);
	}

cleanup:
	opendirectory_free_buffer(session, nodeBuffer);
	return path_list;
}


static CFMutableDictionaryRef convert_xml_to_dict(const char *xml)
{
	CFPropertyListRef xmlDict;
	CFDataRef xmlData;

	xmlData = CFDataCreate(NULL, (const uint8_t *)xml, strlen(xml));
	if (!xmlData) {
		return NULL;
	}

	xmlDict = CFPropertyListCreateFromXMLData(NULL, xmlData,
		kCFPropertyListMutableContainersAndLeaves, NULL);

	CFRelease(xmlData);

	/* The return value of the CFPropertyListCreateFromXMLData function
	 * depends on the contents of the given XML data. CFPropertyListRef can
	 * be a reference to any of the property list objects: CFData,
	 * CFString, CFArray, CFDictionary, CFDate, CFBoolean, and CFNumber.
	 */
	return (CFMutableDictionaryRef)xmlDict;
}

/* Given a string containing a SID, truncate it after the end of any valid SID
 * characters. This is needed for cases when we pull the SID from the
 * CIFSServer config record and it contains trailing junk.
 */
static BOOL chop_sid_string(char * sid_string)
{
	char * c;

	if (*sid_string != 'S' && *sid_string != 's') {
		return False;
	}

	for (c = &sid_string[1]; *c != '\0' && (*c == '-' || isdigit(*c)); ++c) {
	    /* skip it */
	}

	*c = '\0';
	return True;
}

static BOOL allocate_domain_sid_cache(struct opendirectory_session *session)
{

	if (session->domain_sid_cache == NULL) {
		session->domain_sid_cache =
		    CFDictionaryCreateMutable(NULL, 0,
			    &kCFCopyStringDictionaryKeyCallBacks,
			    &kCFTypeDictionaryValueCallBacks);
	}

	if (session->domain_sid_cache == NULL) {
		DEBUG(4, ("unable to allocate domain SID cache\n"));
		return False;
	}

	return True;
}

/* Given a raw /Config/CIFSServer record, update the domain_sid cache. */
static BOOL update_domain_sid_cache(void *mem_ctx,
			    struct opendirectory_session *session,
			    CFDictionaryRef configRecord,
			    DOM_SID *samsid)
{
	BOOL ret = False;
	CFMutableDictionaryRef xmlDict = NULL;
	CFStringRef nodeLocation = NULL;
	char * xml_string;
	char * domain_string;
	char * domain_sid_string;
	char * node_path;

	if (configRecord == NULL) {
		return False;
	}

	xml_string = opendirectory_get_record_attribute(mem_ctx,
				    configRecord, kDS1AttrXMLPlist);
	if (!xml_string) {
		goto cleanup;
	}

	/* The cache is keyed by the path of the DS node that owns the config
	 * record.
	 */
	node_path = opendirectory_get_record_attribute(mem_ctx,
				    configRecord, kDSNAttrMetaNodeLocation);
	if (node_path) {
		nodeLocation = CFStringCreateWithCString(NULL, node_path,
						kCFStringEncodingUTF8);
	}

	/* Convert the XML to a CFDictionary so we can find records in it. */
	if (!(xmlDict = convert_xml_to_dict(xml_string))) {
		goto cleanup;
	}

	/* The plist should have "domain" and "SID" keys. It's noce if we can
	 * get the domain key, because we can dump it to the log.
	 */
	domain_string = opendirectory_get_record_attribute(mem_ctx,
				    xmlDict, "domain");

	domain_sid_string = opendirectory_get_record_attribute(mem_ctx,
				    xmlDict, "SID");
	if (domain_sid_string &&
	    chop_sid_string(domain_sid_string) &&
	    string_to_sid(samsid, domain_sid_string)) {
		/* We have a result. Also try and cache it. */
		ret = True;

		if (session->domain_sid_cache == NULL) {
			DEBUG(4, ("unable to cache domain SID for %s: %s\n",
				    node_path ? node_path : "unknown node",
				    domain_sid_string));
			goto cleanup;
		}

		DEBUG(4, ("caching domain SID for %s: %s\n",
			    domain_string ? domain_string : "unknown domain",
			    domain_sid_string));

		if (nodeLocation) {
			CFDictionaryAddValue(session->domain_sid_cache,
					nodeLocation, xmlDict);
		}
	}

cleanup:
	if (nodeLocation) {
		CFRelease(nodeLocation);
	}

	if (xmlDict) {
		CFRelease(xmlDict);
	}

	return ret;
}

typedef  CFDictionaryRef (*domain_sid_search_func) (
				struct opendirectory_session *session,
				const DOM_SID *domain_sid,
				const DOM_SID *match_sid);

struct search_context {
	domain_sid_search_func		match;
	struct opendirectory_session *	session;
	const DOM_SID *			match_sid;
	CFDictionaryRef			result;
};

static void search_domain_sid_cache_cb(const CFStringRef nodeLocation,
					const CFDictionaryRef domainRecord,
					struct search_context ctx)
{
	DOM_SID domain_sid;
	char *	domain_sid_string;

	if (ctx.result != NULL) {
	    return;
	}

	domain_sid_string = opendirectory_get_record_attribute(NULL,
						    domainRecord, "SID");
	if (domain_sid_string &&
	    chop_sid_string(domain_sid_string) &&
	    string_to_sid(&domain_sid, domain_sid_string)) {
		ctx.result = ctx.match(ctx.session, &domain_sid,
					ctx.match_sid);
	}

	TALLOC_FREE(domain_sid_string);

}

/* Apply the given function to each entry on the domain SID cache. We use this
 * to try any figure out the RID when all we have is an unadorned SID.
 */
static CFDictionaryRef search_domain_sid_cache(
			    struct opendirectory_session *session,
			    const DOM_SID *match_sid,
			    domain_sid_search_func match)
{
	struct search_context ctx =
	{
	    .match = match,
	    .session = session,
	    .match_sid = match_sid,
	    .result = NULL
	};

	if (session->domain_sid_cache == NULL) {
		return NULL;
	}

	CFDictionaryApplyFunction(session->domain_sid_cache,
		(CFDictionaryApplierFunction)search_domain_sid_cache_cb,
		&ctx);

	return ctx.result;
}

/* Fill the domain SID cache with the /Congif/CIFSServer records of all
 * the domains that the directory knows about.
 */
static void fill_domain_sid_cache(void *mem_ctx,
			    struct opendirectory_session *session)
{
	tDirStatus status;
	CFMutableArrayRef records = NULL;
	DOM_SID sid;
	int i;

	/* Reset the cache is we had already filled it. */
	if (session->domain_sid_cache != NULL) {
		CFRelease(session->domain_sid_cache);
		session->domain_sid_cache = NULL;
	}

	allocate_domain_sid_cache(session);
	if (session->domain_sid_cache == NULL) {
	    DEBUG(0, ("failed to initialize domain SID cache\n"));
	    return;
	}

	/* Find all the /Config/CIFSServer records. */
	status = opendirectory_sam_searchname(session, &records,
				kDSStdRecordTypeConfig, "CIFSServer");

	LOG_DS_ERROR(DS_TRACE_ERRORS, status,
		"opendirectory_sam_searchname[/Config/CIFSServer]");
	if (status != eDSNoErr) {
		return;
	}

	for (i = 0; i < CFArrayGetCount(records); ++i) {
		CFDictionaryRef r = CFArrayGetValueAtIndex(records, i);
		update_domain_sid_cache(mem_ctx, session, r, &sid);
	}

	CFRelease(records);
}

static BOOL get_domain_sid_from_path(void *mem_ctx,
				struct opendirectory_session *session,
				const char * node_path,
				DOM_SID *samsid)
{
	CFMutableArrayRef recordsArray = NULL;
	tDirStatus status;
	tDirNodeReference nodeReference = 0;
	tDataListPtr recordName = NULL;
	tDataListPtr recordType = NULL;
	tDataListPtr attributes = NULL;
	CFMutableDictionaryRef configRecord = NULL;
	CFStringRef nodelocationRef;
	CFDictionaryRef domainInfo = NULL;
	char * domain_sid_string;

	BOOL ret = False;

	nodelocationRef = CFStringCreateWithCString(NULL, node_path,
					kCFStringEncodingUTF8);
	if (!nodelocationRef) {
		return False;
	}

	if (session->domain_sid_cache == NULL) {
		fill_domain_sid_cache(mem_ctx, session);
	}

	/* Check whether we have cached this domain SID already. */
	if (session->domain_sid_cache &&
	    (domainInfo = CFDictionaryGetValue(session->domain_sid_cache,
					       nodelocationRef))) {
		domain_sid_string =
			opendirectory_get_record_attribute(mem_ctx,
						    domainInfo, "SID");
		if (domain_sid_string &&
		    chop_sid_string(domain_sid_string) &&
		    string_to_sid(samsid, domain_sid_string)) {
			DEBUG(4, ("found cached domain SID for %s: %s\n",
				    node_path, domain_sid_string));
			ret = True;
			goto cleanup;
		}
	}

	/* SID for this node path was not in the cache. We need to find it from
	 * the /<node_path>/Config/CIFSServer records.
	 */
	status = opendirectory_open_node(session, node_path, &nodeReference);
	if (status != eDSNoErr) {
		goto cleanup;
	}

	recordsArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	recordType = dsBuildListFromStrings(session->ref,
				    kDSStdRecordTypeConfig, NULL);
	recordName = dsBuildListFromStrings(session->ref,
				    "CIFSServer", NULL);
	attributes = dsBuildListFromStrings(session->ref,
				    kDS1AttrXMLPlist , NULL);

	if (!recordName || !recordType || !attributes || !recordsArray) {
		goto cleanup;
	}

	status = opendirectory_search_names(session,
				    nodeReference, recordsArray,
				    recordType, recordName, attributes);

	LOG_DS_ERROR(DS_TRACE_ERRORS, status,
		"opendirectory_search_names[CIFSServer]");
	if (status != eDSNoErr || CFArrayGetCount(recordsArray) == 0) {
		goto cleanup;
	}

	/* Retrieve the CIFSServer record as XML data. */
	configRecord =
	    (CFMutableDictionaryRef) CFArrayGetValueAtIndex(recordsArray, 0);

	ret = update_domain_sid_cache(mem_ctx, session, configRecord, samsid);

cleanup:
	DS_CLOSE_NODE(nodeReference);
	opendirectory_free_list(session, recordName);
	opendirectory_free_list(session, recordType);
	opendirectory_free_list(session, attributes);

	if (nodelocationRef) {
		CFRelease(nodelocationRef);
	}

	if (recordsArray) {
		CFRelease(recordsArray);
	}

	return ret;
}

/* Match the given node path against our cached set of local paths to
 * figure out whether it it local or not.
 */
static BOOL node_path_is_local(const char ** local_paths,
				const char * node_path)
{
        const char ** path;

        for (path = local_paths; path && *path; ++path) {
                /* XXX: should this reallly be case-sensitive? -- jpeach */
                if (strcmp(node_path, *path) == 0) {
                        return True;
                }
        }

        return False;
}

static BOOL get_sid_for_samrecord(void *mem_ctx,
				struct opendirectory_session *session,
				CFDictionaryRef sam_record,
				DOM_SID *record_sid)
{
	fstring sidstr;
	char * record_name;
	char * record_path;

	record_path = opendirectory_get_record_attribute(mem_ctx,
				    sam_record, kDSNAttrMetaNodeLocation);
	record_name = opendirectory_get_record_attribute(mem_ctx,
				    sam_record, kDSNAttrRecordName);

	if (!record_path || !record_name) {
		goto done;
	}

	DEBUG (5, ("resolving SID for record=%s within %s\n",
		    record_name, record_path));

	/* By default, we will make this RID relative to the server SID. If
	 * the user record came from a directory path where we can resolve a
	 * domain SID, we will use that instead.
	 */
	sid_copy(record_sid, get_global_sam_sid());

	/* Local Node - All Users are relative to the server sid */
	if (node_path_is_local(session->local_path_cache, record_path)) {
		DEBUG(4, ("record=%s is local\n", record_name));
		goto done;
 	}

	if (get_domain_sid_from_path(mem_ctx, session,
					record_path, record_sid)) {
		DEBUG(4, ("record=%s is relative to domain SID=%s\n",
			    record_name, sid_to_string(sidstr, record_sid)));
	} else {
		DEBUG(4, ("no domain SID for %s, assuming local SID=%s\n",
			record_path, sid_to_string(sidstr, record_sid)));
	}

done:
	return True;
}

 BOOL opendirectory_find_usersid_from_record(
				struct opendirectory_session *session,
				CFDictionaryRef sam_record,
				DOM_SID *sid)
{
	void * mem_ctx = talloc_init("opendirectory_find_usersid_from_record");
	DOM_SID samsid;
	char * strval;
	BOOL ret = False;

	char * record_type;

	/* Make sure we are dealing with a user record. */
	record_type = opendirectory_get_record_attribute(mem_ctx,
				    sam_record, kDSNAttrRecordType);
	if (!record_type) {
		goto done;
	}

	DEBUG(6, ("determining user SID for %s record\n", record_type));
	if (strcmp(record_type, kDSStdRecordTypeUsers) != 0 &&
	    strcmp(record_type, kDSStdRecordTypeComputers) != 0) {
		goto done;
	}

	if (!session->local_path_cache) {
		session->local_path_cache = opendirectory_local_paths(session);
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrSMBSID);
	if (strval) {
		string_to_sid(sid, strval);
		ret = True;
		goto done;
	}

	/* Since there is no SID attribute available, we will need to
	 * construct something by using an attribute as a RID. We can't do
	 * this unless we have a base SID for this record.
	 */
	if (!get_sid_for_samrecord(mem_ctx, session, sam_record, &samsid)) {
		goto done;
	}

	sid_copy(sid, &samsid);

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrSMBRID);
	if (strval) {
		uint32_t rid = (uint32_t)strtoul(strval, NULL, 10 /* base */);
		sid_append_rid(sid, rid);
		ret = True;
		goto done;
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrUniqueID);
	if (strval) {
		int32_t uid = (int32_t)strtol(strval, NULL, 10 /* base */);
		uint32_t rid;

		if (opendirectory_match_record_attribute(sam_record,
				    kDSNAttrRecordName, lp_guestaccount())) {
			rid = DOMAIN_USER_RID_GUEST;
		} else {
			rid = algorithmic_pdb_uid_to_user_rid(uid);
		}

		sid_append_rid(sid, rid);
		ret = True;
		goto done;
	}

	/* Nothing in the directory that we can map this record with. */

done:
	TALLOC_FREE(mem_ctx);
	return ret;
}

static BOOL find_groupsid_from_user(void *mem_ctx,
				struct opendirectory_session *session,
				CFDictionaryRef sam_record,
				DOM_SID *group_sid)
{
	CFDictionaryRef group_record;
	char * strval;

	/* Both user and group records can have a kDS1AttrSMBPrimaryGroupSID
	 * attribute.
	 */
	strval = opendirectory_get_record_attribute(mem_ctx,
				sam_record, kDS1AttrSMBPrimaryGroupSID);
	if (strval) {
		string_to_sid(group_sid, strval);
		return True;
	}

	/* The group SID wasn't explicitly overridden, so we have to look up
	 * the group record and work from there.
	 */
	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrPrimaryGroupID);
	if (!strval) {
		return False;
	}

	group_record = sam_searchattr_first(session, kDSStdRecordTypeGroups,
			    kDS1AttrPrimaryGroupID, strval);
	if (group_record) {
		BOOL ret;

		ret = opendirectory_find_groupsid_from_record(session,
			    group_record, group_sid);
		CFRelease(group_record);
		return ret;
	}

	/* Nothing in the directory that we can map this record with, but
	 * it's a user record so we can put them in Domain Users.
	 */
	sid_copy(group_sid, get_global_sam_sid());
	sid_append_rid(group_sid, DOMAIN_GROUP_RID_USERS);

	return True;
}

 BOOL opendirectory_find_groupsid_from_record(
				struct opendirectory_session *session,
				CFDictionaryRef sam_record,
				DOM_SID *sid)
{
	void * mem_ctx = talloc_init("opendirectory_find_groupsid_from_record");
	DOM_SID samsid;
	char * strval;
	BOOL ret = False;

	char * record_type;

	record_type = opendirectory_get_record_attribute(mem_ctx,
				    sam_record, kDSNAttrRecordType);
	if (!record_type) {
		goto done;
	}

	DEBUG(6, ("determining group SID for %s record\n", record_type));

	/* Make sure we are dealing with a user, group or computer record. */
	if (strcmp(record_type, kDSStdRecordTypeGroups) != 0 &&
	    strcmp(record_type, kDSStdRecordTypeUsers) != 0 &&
	    strcmp(record_type, kDSStdRecordTypeComputers) != 0) {
		goto done;
	}

	if (!session->local_path_cache) {
		session->local_path_cache = opendirectory_local_paths(session);
	}

	/* If we are finding the primary group sid with only a user record,
	 * punt it becasue we might need to look up the group record.
	 */
	if (strcmp(record_type, kDSStdRecordTypeUsers) == 0) {
		ret = find_groupsid_from_user(mem_ctx, session,
						sam_record, sid);
		goto done;
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
				sam_record, kDS1AttrSMBPrimaryGroupSID);
	if (strval) {
		string_to_sid(sid, strval);
		ret = True;
		goto done;
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrSMBSID);
	if (strval) {
		string_to_sid(sid, strval);
		ret = True;
		goto done;
	}

	/* Since there is no SID attribute available, we will need to
	 * construct something by using an attribute as a RID. We can't do
	 * this unless we have a base SID for this record.
	 */
	if (!get_sid_for_samrecord(mem_ctx, session, sam_record, &samsid)) {
		goto done;
	}

	sid_copy(sid, &samsid);

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrSMBGroupRID);
	if (strval) {
		uint32_t rid = (uint32_t)strtoul(strval, NULL, 10 /* base */);
		sid_append_rid(sid, rid);
		ret = True;
		goto done;
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrSMBRID);
	if (strval) {
		uint32_t rid = (uint32_t)strtoul(strval, NULL, 10 /* base */);
		sid_append_rid(sid, rid);
		ret = True;
		goto done;
	}

	strval = opendirectory_get_record_attribute(mem_ctx,
					sam_record, kDS1AttrPrimaryGroupID);
	if (strval) {
		int32_t gid = (int32_t)strtol(strval, NULL, 10 /* base */);
		uint32_t rid;

		/* Note that this implies that we have matching user and group
		 * names for lp_guestaccount(). This is true for common choices
		 * like "unknown" and "nobody".
		 */
		if (opendirectory_match_record_attribute(sam_record,
				    kDSNAttrRecordName, lp_guestaccount())) {
			rid = DOMAIN_GROUP_RID_GUESTS;
		} else {
			rid = algorithmic_pdb_gid_to_group_rid(gid);
		}

		sid_append_rid(sid, rid);
		ret = True;
		goto done;
	}

	if (strcmp(record_type, kDSStdRecordTypeComputers) == 0) {
		sid_append_rid(sid, DOMAIN_GROUP_RID_COMPUTERS);
		ret = True;
	}

done:
	TALLOC_FREE(mem_ctx);
	return ret;
}

static CFDictionaryRef sam_searchname_first(
				struct opendirectory_session *session,
				const char *type,
				const char *name)
{
	CFMutableArrayRef records = NULL;
	tDirStatus status;

	status = opendirectory_sam_searchname(session,
			    &records, type, name);

	if (records) {
		CFDictionaryRef first =
		    (CFDictionaryRef)CFArrayGetValueAtIndex(records, 0);

		CFRetain(first);
		CFRelease(records);
		return first;
	}

	return NULL;
}

static CFDictionaryRef sam_searchattr_first(
				struct opendirectory_session *session,
				const char *type,
				const char *attr,
				const char *value)
{
	CFMutableArrayRef records = NULL;
	tDirStatus status;

	status = opendirectory_sam_searchattr(session,
			    &records, type, attr, value);

	if (records) {
		CFDictionaryRef first =
		    (CFDictionaryRef)CFArrayGetValueAtIndex(records, 0);

		CFRetain(first);
		CFRelease(records);
		return first;
	}

	return NULL;
}

static CFDictionaryRef find_record_from_usersid_and_domsid(
				struct opendirectory_session *session,
				const DOM_SID *domain_sid,
				const DOM_SID *sid)
{
	CFDictionaryRef sam_record;
	fstring rid_string;
	fstring uid_string;
	uint32_t rid;

	if (!sid_peek_check_rid(domain_sid, sid, &rid)) {
		return NULL;
	}

	if (rid == DOMAIN_USER_RID_GUEST) {
		return sam_searchname_first(session, kDSStdRecordTypeUsers,
					 lp_guestaccount());
	}

	snprintf(rid_string, sizeof(rid_string) - 1, "%u", rid);
	snprintf(uid_string, sizeof(uid_string) - 1, "%u",
			    algorithmic_pdb_user_rid_to_uid(rid));

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeUsers,
					 kDS1AttrSMBRID, rid_string);
	if (sam_record) {
		return sam_record;
	}

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeComputers,
					 kDS1AttrSMBRID, rid_string);
	if (sam_record) {
		return sam_record;
	}

	/* If it's not a generated user RID, searching won't help. */
	if (!algorithmic_pdb_rid_is_user(rid)) {
		return NULL;
	}

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeUsers,
					 kDS1AttrUniqueID, uid_string);
	if (sam_record) {
		return sam_record;
	}

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeComputers,
					 kDS1AttrUniqueID, uid_string);
	if (sam_record) {
		return sam_record;
	}

	return NULL;
}

/* Map SIDS that are relative to the SAM SID to the "well-known" Apple builtin
 * SID of the form S-1-5-21-RID. Arguably we should also do this for the domain
 * SID if we are a domain controller.
 */
static BOOL apple_wellknown_sid(const DOM_SID *sid, DOM_SID *apple_sid)
{
	uint32_t rid;
	DOM_SID apple_wellknown =
	    { 1, 1, {0,0,0,0,0,5}, {21,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};

	if (!sid_peek_check_rid(get_global_sam_sid(), sid, &rid)) {
		return False;
	}

	return sid_compose(apple_sid, &apple_wellknown, rid);
}

 CFDictionaryRef opendirectory_find_record_from_usersid(
				struct opendirectory_session *session,
				const DOM_SID *sid)
{
	CFDictionaryRef sam_record;
	fstring sid_string;

	DOM_SID domain_sid = {0};
	DOM_SID apple_user_sid = {0};
	DOM_SID sam_sid = {0};

	secrets_fetch_domain_sid(lp_workgroup(), &domain_sid);
	sam_sid = *get_global_sam_sid();

	sid_to_string(sid_string, sid);

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeUsers,
					 kDS1AttrSMBSID, sid_string);
	if (sam_record) {
		return sam_record;
	}

	/* Check whether this SID is an Apple "well-known" SID. Since there are
	 * no well-known Computers, we don't need to repeat this check.
	 */
	if (apple_wellknown_sid(sid, &apple_user_sid)) {
		sid_to_string(sid_string, &apple_user_sid);
		sam_record = sam_searchattr_first(session,
					kDSStdRecordTypeUsers,
					kDS1AttrSMBSID, sid_string);
		if (sam_record) {
			return sam_record;
		}
	}

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeComputers,
					 kDS1AttrSMBSID, sid_string);
	if (sam_record) {
		return sam_record;
	}

	/* The SID might be in a domain we know about, so we can try poking
	 * around for something to match the RID.
	 */

	sam_record = find_record_from_usersid_and_domsid(session,
						    &sam_sid, sid);
	if (sam_record) {
		return sam_record;
	}

	/* Also try the domain SID if it is not the same as our SAM SID. */
	if (sid_compare_domain(&domain_sid, &sam_sid) != 0) {
		sam_record = find_record_from_usersid_and_domsid(session,
							&domain_sid, sid);
		if (sam_record) {
			return sam_record;
		}
	}

	/* As a last resort, iterate over all the domains that are available in
	 * the directory and see whether we can match the SID in any of those.
	 */
	if (session->domain_sid_cache == NULL) {
		void * mem_ctx = talloc_init(__FUNCTION__);
		fill_domain_sid_cache(mem_ctx, session);
		TALLOC_FREE(mem_ctx);
	}

	return search_domain_sid_cache(session, sid,
		    find_record_from_usersid_and_domsid);
}

static CFDictionaryRef find_record_from_groupsid_and_domsid(
				struct opendirectory_session *session,
				const DOM_SID *domain_sid,
				const DOM_SID *group_sid)
{
	CFDictionaryRef sam_record;
	fstring rid_string;
	fstring gid_string;
	uint32_t rid;

	if (!sid_peek_check_rid(domain_sid, group_sid, &rid)) {
		return NULL;
	}

	DEBUG(8, ("searching domain %s for group record with SID %s\n",
		sid_to_string(gid_string, domain_sid),
		sid_to_string(rid_string, group_sid)));

	snprintf(rid_string, sizeof(rid_string) - 1, "%u", rid);

	/* First, search for a record with a matching group RID. If that fails,
	 * we can check to see whether it's a well-known RID for which we have
	 * a builtin default.
	 */
	sam_record = sam_searchattr_first(session, kDSStdRecordTypeGroups,
					 kDS1AttrSMBRID, rid_string);
	if (sam_record) {
		return sam_record;
	}

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeComputers,
					 kDS1AttrSMBGroupRID, rid_string);
	if (sam_record) {
		return sam_record;
	}

	switch (rid) {
	case DOMAIN_GROUP_RID_USERS:
	case BUILTIN_ALIAS_RID_USERS:
		/* The local group "staff" has a SID of S-1-5-32-545, which is
		 * the well-known SID for "Users". Additionally, the "staff"
		 * group has a RealName attribute of "Users", so this seems
		 * like a good match.
		 */
		return sam_searchname_first(session, kDSStdRecordTypeGroups,
					 "staff");
	case DOMAIN_GROUP_RID_GUESTS:
	case BUILTIN_ALIAS_RID_GUESTS:
	case DOMAIN_GROUP_RID_COMPUTERS:
		/* The default config has "guest account = nobody", so we can
		 * use this for both the guest user and the guest group. We
		 * can't guarantee that the guest account is also a group
		 * however.
		 *
		 * Computer accounts get mapped to nobody as well because they
		 * are untrusted as far as filesystem access goes.
		 */
		sam_record = sam_searchname_first(session,
			    kDSStdRecordTypeGroups, lp_guestaccount());

		if (!sam_record) {
			sam_record = sam_searchname_first(session,
					kDSStdRecordTypeGroups, "nobody");
		}

		return sam_record;

	case DOMAIN_GROUP_RID_ADMINS:
	case DOMAIN_GROUP_RID_CONTROLLERS:
	case DOMAIN_GROUP_RID_CERT_ADMINS:
	case DOMAIN_GROUP_RID_SCHEMA_ADMINS:
	case DOMAIN_GROUP_RID_ENTERPRISE_ADMINS:
	case BUILTIN_ALIAS_RID_ADMINS:
	case BUILTIN_ALIAS_RID_POWER_USERS:
		return sam_searchname_first(session, kDSStdRecordTypeGroups,
					 "admin");
	}

	if (rid >= BASE_RID) {
		snprintf(gid_string, sizeof(gid_string) - 1, "%u",
				    pdb_group_rid_to_gid(rid));

		/* If it a generated user RID, searching won't help because
		 * we are looking for a group.
		 */
		if (algorithmic_pdb_rid_is_user(rid)) {
			return NULL;
		}

		sam_record = sam_searchattr_first(session,
					kDSStdRecordTypeGroups,
					kDS1AttrPrimaryGroupID, gid_string);
		if (sam_record) {
			return sam_record;
		}
	}

	return NULL;
}

 CFDictionaryRef opendirectory_find_record_from_groupsid(
				struct opendirectory_session *session,
				const DOM_SID *group_sid)
{
	CFDictionaryRef sam_record;
	fstring sid_string;

	DOM_SID domain_sid = {0};
	DOM_SID sam_sid = {0};
	DOM_SID apple_group_sid = {0};

	secrets_fetch_domain_sid(lp_workgroup(), &domain_sid);
	sam_sid = *get_global_sam_sid();

	sid_to_string(sid_string, group_sid);

	sam_record = sam_searchattr_first(session, kDSStdRecordTypeGroups,
					 kDS1AttrSMBSID, sid_string);
	if (sam_record) {
		return sam_record;
	}

	if (apple_wellknown_sid(group_sid, &apple_group_sid)) {
		sid_to_string(sid_string, &apple_group_sid);
		sam_record = sam_searchattr_first(session,
					kDSStdRecordTypeGroups,
					kDS1AttrSMBSID, sid_string);
		if (sam_record) {
			return sam_record;
		}
	}

	sam_record = find_record_from_groupsid_and_domsid(session,
				&sam_sid, group_sid);
	if (sam_record) {
		return sam_record;
	}

	if (sid_compare(&domain_sid, &sam_sid) != 0) {
		sam_record = find_record_from_groupsid_and_domsid(session,
					    &domain_sid, group_sid);
		if (sam_record) {
			return sam_record;
		}
	}

	if (session->domain_sid_cache == NULL) {
		void * mem_ctx = talloc_init(__FUNCTION__);
		fill_domain_sid_cache(mem_ctx, session);
		TALLOC_FREE(mem_ctx);
	}

	return search_domain_sid_cache(session, group_sid,
		    find_record_from_groupsid_and_domsid);
}

