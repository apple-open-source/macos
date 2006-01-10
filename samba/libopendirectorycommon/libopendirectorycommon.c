/*
 *  libopendirectorycommon.c
 *
 */

#include "libopendirectorycommon.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* Private */

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
#include <Security/Security.h>
#include <SystemConfiguration/SystemConfiguration.h>
//#include <CoreServices/CoreServices.h>

#define SERVICE	"com.apple.samba"
#define SAMBA_APP_ID CFSTR("com.apple.samba")

char *get_account()
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

	SecAccessRef access;
	status = SecAccessCreateFromOwnerAndACL(&owner,
		sizeof(acls) / sizeof(acls[0]), acls, &access);
	return access;
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
	status = SecKeychainGetUserInteractionAllowed (FALSE);

	 status = SecKeychainFindGenericPassword (
					 NULL,           // default keychain
					 strlen(SERVICE),             // length of service name
					 SERVICE,   // service name
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

OSStatus set_password_in_keychain(char *account, char *accountLen, char *password, u_int32_t passwordLen)
{
	SecKeychainItemRef item = NULL;
	OSStatus status = noErr;
	SecKeychainAttribute attributes[2];
	SecKeychainAttributeList attributeList = {
		sizeof(attributes) / sizeof(attributes[0]),
		attributes
	};
	SecKeychainRef keychain;
	SecKeychainStatus keychainStatus;
	
	attributes[0].tag = kSecServiceItemAttr;
	attributes[0].length = strlen(SERVICE);
	attributes[0].data = SERVICE;

	attributes[1].tag = kSecAccountItemAttr;
	attributes[1].length = accountLen;
	attributes[1].data = (void *)account;

	// Set the domain to System (daemon)
	status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
	status = SecKeychainGetUserInteractionAllowed (FALSE);

	status = SecKeychainFindGenericPassword(NULL, strlen(SERVICE), SERVICE, accountLen, account, NULL, NULL, &item);

	if (status == errSecItemNotFound)
	{
		status = SecKeychainItemCreateFromContent(kSecGenericPasswordItemClass,
				&attributeList,
				passwordLen, password,
				NULL,
				make_uid_access(0),
				NULL);
			
	}
	else if ((status == noErr) && (item != NULL))
	{
		status = SecKeychainItemModifyContent(item, NULL, strlen(password), password);
	}
	return status;
}

#endif

opendirectory_secret_header *get_opendirectory_secret_header(void *authenticator)
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

int set_opendirectory_authenticator(u_int32_t authenticatorlen, char *authenticator, u_int32_t secretlen, char *secret)
{
#ifdef USES_KEYCHAIN
	OSStatus status = noErr;

	status = set_password_in_keychain(authenticator, authenticatorlen, secret, secretlen);
	if (status != noErr) {
		return -1;	
	} else {
		add_to_sambaplist (authenticator);
		return status;
	}
#else
	int fd = 0;
	int result = -1;
	struct flock lock;
	opendirectory_secret_header hdr;
	int initialized = 0;
	
	if (authenticatorlen && authenticator && secretlen && secret) {
		fd = open(credentialfile, O_CREAT | O_WRONLY | O_TRUNC,0600);
		if (fd == -1) {
			fprintf(stderr,"unable to open file (%s)\n",strerror(errno));
		} else {
			lock.l_type = F_WRLCK;
			lock.l_start = 0;
			lock.l_whence = SEEK_CUR;
			lock.l_len = 0;
			if (fcntl(fd, F_SETFL,&lock) == -1)
				result = errno;
			else {
				memset(&hdr, 0, sizeof(opendirectory_secret_header));
				hdr.signature = sig;
				hdr.authenticator_len = authenticatorlen;
				hdr.secret_len = secretlen;
				if (write(fd, &hdr, sizeof(opendirectory_secret_header)) != sizeof(opendirectory_secret_header))
					goto cleanup;
				if (write(fd, authenticator, authenticatorlen) != authenticatorlen)
					goto cleanup;
				if (write(fd, secret, secretlen) != secretlen)
					goto cleanup;
				initialized = 1;
				result = 0;
			}
				
		}
	}
cleanup:
	if (fd)
		close(fd);
	if (!initialized)
		unlink(credentialfile);
	return result;
#endif
}

void  *get_opendirectory_authenticator()
{
	void *authenticator = NULL;
	int authentriessize = 0;
	int initialized = 0;
#ifdef USES_KEYCHAIN
	opendirectory_secret_header *odhdr = NULL;
	char *password = NULL;
	char *account = NULL;
	
	account = get_account();
	if (account) {
		password = get_password_from_keychain(account, strlen(account));
		
		if (password) {
			authentriessize = strlen(account) + strlen(password);
			authenticator = calloc(1,sizeof(opendirectory_secret_header) + authentriessize);
			memcpy(authenticator + sizeof(opendirectory_secret_header) , account, strlen(account));
			memcpy(authenticator + sizeof(opendirectory_secret_header) + strlen(account), password, strlen(password));
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
		
//		if(pread(fd, &hdr, sizeof(opendirectory_secret_header), 0) != sizeof(opendirectory_secret_header)) {
		if(read(fd, &hdr, sizeof(opendirectory_secret_header)) != sizeof(opendirectory_secret_header)) {
			goto cleanup;
		}
		if (hdr.signature != sig) {
			goto cleanup;
		}
		authentriessize = hdr.authenticator_len + hdr.secret_len;
		authenticator = malloc(sizeof(opendirectory_secret_header) + authentriessize);
		memset(authenticator, 0, sizeof(opendirectory_secret_header) + authentriessize);
		memcpy(authenticator, &hdr, sizeof(opendirectory_secret_header));
//		if(pread(fd, authenticator + sizeof(hdr), authentriessize, sizeof(hdr)) != authentriessize) {
		if(read(fd, authenticator + sizeof(opendirectory_secret_header), authentriessize) != authentriessize) {
			goto cleanup;
		}
		initialized = 1;
	} else {
		printf("unable to open file (%s)\n",strerror(errno));
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


u_int32_t get_opendirectory_authenticator_accountlen(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	u_int32_t len = 0;
	
	if (hdr)
		len = hdr->authenticator_len;
		
	return len;
}

void *get_opendirectory_authenticator_account(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	void *result = NULL;

	if (hdr)
		result = authenticator + sizeof(opendirectory_secret_header);
		
	return result;
}

u_int32_t get_opendirectory_authenticator_secretlen(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	u_int32_t len = 0;
	
	if (hdr)
		len = hdr->secret_len;
		
	return len;

}
void *get_opendirectory_authenticator_secret(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	void *result = NULL;
	
	if (hdr)
		result = authenticator + sizeof(opendirectory_secret_header) + hdr->authenticator_len;

	return result;
}

void delete_opendirectory_authenticator(void*authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	
	if (hdr) {
		bzero(authenticator, sizeof(opendirectory_secret_header) + hdr->authenticator_len + hdr->secret_len);
		free(authenticator);
	}
}

tDirStatus get_node_ref_and_name(tDirReference dirRef, char *name, char *recordType, tDirNodeReference *nodeRef, char *outRecordName)
{
    tDirStatus			status			= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
    long			returnCount		= 0;
    tDataBufferPtr		nodeBuffer		= NULL;
    tDirNodeReference		searchNodeRef		= NULL;
    tDataListPtr		searchNodeName		= NULL;
    tDataListPtr		NodePath		= NULL;
    char			NodePathStr[256]	= {0};
//    char			nameBuffer[128]	= {0};

    tDataListPtr		recName			= NULL;
    tDataListPtr		recType			= NULL;
    tDataListPtr		attrType		= NULL;

    tAttributeListRef		attributeListRef	= 0;
    tRecordEntryPtr		outRecordEntryPtr	= NULL;
    tAttributeEntryPtr		attributeInfo		= NULL;
    tAttributeValueListRef	attributeValueListRef	= 0;
    tAttributeValueEntryPtr	attrValue		= NULL;
    long			i			= 0;
    
    nodeBuffer = dsDataBufferAllocate(dirRef, bufferSize);
    if (nodeBuffer == NULL) goto cleanup;
    status = dsFindDirNodes(dirRef, nodeBuffer, NULL, eDSSearchNodeName, &returnCount, NULL);
    if ((status != eDSNoErr) || (returnCount <= 0)) goto cleanup;

    status = dsGetDirNodeName(dirRef, nodeBuffer, 1, &searchNodeName);
    if (status != eDSNoErr) goto cleanup;
    status = dsOpenDirNode(dirRef, searchNodeName, &searchNodeRef);
    if (status != eDSNoErr) goto cleanup;

    recName = dsBuildListFromStrings(dirRef, name, NULL);
    recType = dsBuildListFromStrings(dirRef, recordType, NULL);	
    attrType = dsBuildListFromStrings(dirRef, kDSNAttrMetaNodeLocation, kDSNAttrRecordName, NULL);

    status = dsGetRecordList(searchNodeRef, nodeBuffer, recName, eDSiExact, recType, attrType, 0, &returnCount, NULL);
    if (status != eDSNoErr) goto cleanup;

    status = dsGetRecordEntry(searchNodeRef, nodeBuffer, 1, &attributeListRef, &outRecordEntryPtr);
    if (status == eDSNoErr)
    {
        for (i = 1 ; i <= outRecordEntryPtr->fRecordAttributeCount; i++)
        {
            status = dsGetAttributeEntry(searchNodeRef, nodeBuffer, attributeListRef, i, &attributeValueListRef, &attributeInfo);
            status = dsGetAttributeValue(searchNodeRef, nodeBuffer, 1, attributeValueListRef, &attrValue);
            if (attributeValueListRef != 0)
            {
                    dsCloseAttributeValueList(attributeValueListRef);
                    attributeValueListRef = 0;
            }	
            if (status == eDSNoErr)
            {
                if (strncmp(attributeInfo->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation, strlen(kDSNAttrMetaNodeLocation)) == 0)
                    strncpy(NodePathStr, attrValue->fAttributeValueData.fBufferData, attrValue->fAttributeValueData.fBufferSize);
                else if (strncmp(attributeInfo->fAttributeSignature.fBufferData, kDSNAttrRecordName, strlen(kDSNAttrRecordName)) == 0)
                    strncpy(outRecordName, attrValue->fAttributeValueData.fBufferData, attrValue->fAttributeValueData.fBufferSize);
            }
            if (attrValue != NULL) {
                    dsDeallocAttributeValueEntry(dirRef, attrValue);
                    attrValue = NULL;
            }				
            if (attributeInfo != NULL) {
                    dsDeallocAttributeEntry(dirRef, attributeInfo);
                    attributeInfo = NULL;
            }
        }
        if (outRecordEntryPtr != NULL) {
            dsDeallocRecordEntry(dirRef, outRecordEntryPtr);
            outRecordEntryPtr = NULL;
        }
        if (strlen(NodePathStr) != 0 && strlen(outRecordName) != 0)
        {
 			//DEBUG(0,("getnode: kDSNAttrMetaNodeLocation(%s)\n", NodePathStr));
           NodePath = dsBuildFromPath(dirRef, NodePathStr, "/");
            status = dsOpenDirNode(dirRef, NodePath, nodeRef);
            dsDataListDeallocate( dirRef, NodePath);
            free(NodePath);
        }
    }
cleanup:
    if (nodeBuffer != NULL)
        dsDataBufferDeAllocate(dirRef, nodeBuffer);
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
        dsDataListDeAllocate(dirRef, recType, 0);
        free(recType);
    }
    if (attrType != NULL)
    {
        dsDataListDeAllocate(dirRef, attrType, 0);
        free(attrType);
    }

    return status;
}

u_int32_t opendirectory_add_data_buffer_item(tDataBufferPtr dataBuffer, u_int32_t len, void *buffer)
{
	u_int32_t result = 0;
	
	if (dataBuffer->fBufferLength + len + 4 <= dataBuffer->fBufferSize) {
		memcpy( &(dataBuffer->fBufferData[ dataBuffer->fBufferLength ]), &len, 4);
		dataBuffer->fBufferLength += 4;
		if (len != 0) {
			memcpy( &(dataBuffer->fBufferData[ dataBuffer->fBufferLength ]), buffer, len);
			dataBuffer->fBufferLength += len;
		}
	}
	
	return result;	
}

tDirStatus opendirectory_cred_session_key(char *client_challenge, char *server_challenge, char *machine_acct, char *session_key, char *slot_id)
{
    tDirReference	dirRef = NULL;
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	unsigned long		curr			= 0;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = NULL;
	char *targetaccount = NULL;
	char recordName[255] = {0};
	
    status = dsOpenDirService(&dirRef);
	
	if (status != eDSNoErr)
		return status;
	
	status = get_node_ref_and_name(dirRef, machine_acct, kDSStdRecordTypeComputers, &nodeRef, recordName);

	if (status != eDSNoErr)
		goto cleanup;

	status = opendirectory_authenticate_node(dirRef, nodeRef);

	if (status != eDSNoErr)
		goto cleanup;

	if (slot_id && strlen(slot_id))
		targetaccount = slot_id;
	else
		targetaccount = recordName;

	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef,  kDSStdAuthSMBWorkstationCredentialSessionKey);
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeComputers);
					if ( authType != NULL )
					{
							// Target account
							len = strlen( targetaccount );
							memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
							curr += sizeof( long );
							memcpy( &(authBuff->fBufferData[ curr ]), targetaccount, len );
							curr += len;
							// Client Challenge and Server Challenge
							len = 16;
							memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
							curr += sizeof( long );
							memcpy( &(authBuff->fBufferData[ curr ]), server_challenge, 8 );
							curr += 8;
							memcpy( &(authBuff->fBufferData[ curr ]), client_challenge, 8 );
							curr += 8;

							authBuff->fBufferLength = curr;
							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 1, authBuff, stepBuff, NULL,  recordType);
							if ( status == eDSNoErr )
							{
									//DEBUG(1,("kDSStdAuthSMBWorkstationCredentialSessionKey was successful for  \"%s\" :)\n", machine_acct));
									memcpy(&len, stepBuff->fBufferData, 4);
									stepBuff->fBufferData[len+4] = '\0';
									memcpy(session_key,stepBuff->fBufferData+4, 8);
									//DEBUG(1,("session key  \"%s\" :)\n", policy));

							}
							else
							{
									// DEBUG(1,("kDSStdAuthGetPolicy FAILED for user \"%s\" (%d) :(\n", userid, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							// DEBUG(1,("get_password_policy: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					// DEBUG(1,("get_password_policy: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					// DEBUG(1,( "get_password_policy: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			// DEBUG(1,("get_password_policy: *** dsDataBufferAllocate(1) faild with \n" ));
	}
cleanup:
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	if (dirRef)
		dsCloseDirService(dirRef);
	if (nodeRef)
		dsCloseDirNode(nodeRef);
	return status;
}

tDirStatus opendirectory_user_session_key(const char *account_name, char *session_key, char *slot_id)
{
    tDirReference	dirRef = NULL;
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = NULL;
	char *targetaccount = NULL;
	char recordName[255] = {0};
		
    status = dsOpenDirService(&dirRef);
	
	if (status != eDSNoErr)
		return status;
	
	status = get_node_ref_and_name(dirRef, account_name, kDSStdRecordTypeUsers, &nodeRef, recordName);

	if (status != eDSNoErr)
		goto cleanup;

	status = opendirectory_authenticate_node(dirRef, nodeRef);

	if (status != eDSNoErr)
		goto cleanup;

	if (slot_id && strlen(slot_id))
		targetaccount = slot_id;
	else
		targetaccount = recordName;

	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef,  kDSStdAuthSMB_NT_UserSessionKey);
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeUsers);
					if ( authType != NULL )
					{
							// Target account
							opendirectory_add_data_buffer_item(authBuff, strlen( targetaccount ), targetaccount);
							opendirectory_add_data_buffer_item(authBuff, 1, ""); // null terminate ??
							
							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 1, authBuff, stepBuff, NULL,  recordType);
							if ( status == eDSNoErr )
							{
									//DEBUG(4,("kDSStdAuthSMB_NT_UserSessionKey was successful for  \"%s\" :)\n", targetaccount));
									memcpy(&len, stepBuff->fBufferData, 4);
									stepBuff->fBufferData[len+4] = '\0';
									memcpy(session_key,stepBuff->fBufferData+4, len);
									//DEBUG(4,("session key  \"%s\" :)\n", policy));

							}
							else
							{
									//DEBUG(4,("kDSStdAuthSMB_NT_UserSessionKey FAILED for user \"%s\" (%d) :(\n", targetaccount, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					//DEBUG(0,( "kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferAllocate(1) faild with \n" ));
	}
cleanup:
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	if (dirRef)
		dsCloseDirService(dirRef);
	if (nodeRef)
		dsCloseDirNode(nodeRef);
	return status;
}

tDirStatus opendirectory_ntlmv2user_session_key(const char *account_name, u_int32_t ntv2response_len, char* ntv2response, char* domain, u_int32_t *session_key_len, char *session_key, char 
*slot_id)
{
    tDirReference	dirRef = NULL;
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = NULL;
	char *targetaccount = NULL;
	char recordName[255] = {0};
		
    status = dsOpenDirService(&dirRef);
	
	if (status != eDSNoErr)
		return status;
	
	status = get_node_ref_and_name(dirRef, account_name, kDSStdRecordTypeUsers, &nodeRef, recordName);

	if (status != eDSNoErr)
		goto cleanup;

	status = opendirectory_authenticate_node(dirRef, nodeRef);

	if (status != eDSNoErr)
		goto cleanup;

	if (slot_id && strlen(slot_id))
		targetaccount = slot_id;
	else
		targetaccount = recordName;

	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef, kDSStdAuthSMBNTv2UserSessionKey ); /* "dsAuthMethodStandard:dsAuthNodeNTLMv2SessionKey" */
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeUsers);
					if ( authType != NULL )
					{
/*
The buffer format is:
4 byte len + Directory Services user name
4 byte len + client blob
4 byte len + username
4 byte len + domain
*/
							// Target account
							opendirectory_add_data_buffer_item(authBuff, strlen( targetaccount ), targetaccount);
							opendirectory_add_data_buffer_item(authBuff, ntv2response_len, ntv2response);
							opendirectory_add_data_buffer_item(authBuff, strlen( targetaccount ), targetaccount);
							opendirectory_add_data_buffer_item(authBuff, strlen( domain ), domain);
							
							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 1, authBuff, stepBuff, NULL,  recordType);
							if ( status == eDSNoErr )
							{
									//DEBUG(4,("kDSStdAuthSMBNTv2UserSessionKey was successful for  \"%s\" :)\n", targetaccount));
									memcpy(&len, stepBuff->fBufferData, 4);
									stepBuff->fBufferData[len+4] = '\0';
									*session_key_len = len;
									memcpy(session_key,stepBuff->fBufferData+4, len);
									//DEBUG(4,("session key  \"%s\" :)\n", policy));

							}
							else
							{
									//DEBUG(4,("kDSStdAuthSMB_NT_UserSessionKey FAILED for user \"%s\" (%d) :(\n", targetaccount, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					//DEBUG(0,( "kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferAllocate(1) faild with \n" ));
	}
cleanup:
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	if (dirRef)
		dsCloseDirService(dirRef);
	if (nodeRef)
		dsCloseDirNode(nodeRef);
	return status;
}

tDirStatus opendirectory_set_workstation_nthash(char *account_name, char *nt_hash, char *slot_id)
{
    tDirReference	dirRef = NULL;
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	unsigned long		curr			= 0;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = NULL;
	char *targetaccount = NULL;
	char recordName[255] = {0};
	
    status = dsOpenDirService(&dirRef);
	
	if (status != eDSNoErr)
		return status;
	
	status = get_node_ref_and_name(dirRef, account_name, kDSStdRecordTypeComputers, &nodeRef, recordName);
	
	if (status != eDSNoErr)
		goto cleanup;

	status = opendirectory_authenticate_node(dirRef, nodeRef);

	if (status != eDSNoErr)
		goto cleanup;

	if (slot_id && strlen(slot_id))
		targetaccount = slot_id;
	else
		targetaccount = recordName;

	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef,   kDSStdAuthSetWorkstationPasswd    ); // kDSStdAuthSetNTHash
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeComputers);
					if ( authType != NULL )
					{
							printf("opendirectory_set_workstation_nthash: recordName (%s)\n", recordName);
							opendirectory_add_data_buffer_item(authBuff, strlen( targetaccount ), targetaccount);
							opendirectory_add_data_buffer_item(authBuff, 16, nt_hash);

							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 1, authBuff, stepBuff, NULL,  recordType);
							printf("opendirectory_set_workstation_nthash: dsDoDirNodeAuthOnRecordType status (%d)\n", status);
							if ( status == eDSNoErr )
							{
									//DEBUG(1,("kDSStdAuthSetWorkstationPasswd was successful for  \"%s\" :)\n", machine_acct));
							}
							else
							{
									// DEBUG(1,("kDSStdAuthSetWorkstationPasswd FAILED for user \"%s\" (%d) :(\n", userid, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							// DEBUG(1,("kDSStdAuthSetWorkstationPasswd: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					// DEBUG(1,("kDSStdAuthSetWorkstationPasswd: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					// DEBUG(1,( "kDSStdAuthSetWorkstationPasswd: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			// DEBUG(1,("kDSStdAuthSetWorkstationPasswd: *** dsDataBufferAllocate(1) faild with \n" ));
	}
cleanup:
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	if (dirRef)
		dsCloseDirService(dirRef);
	if (nodeRef)
		dsCloseDirNode(nodeRef);
	return status;
}

tDirStatus opendirectory_lmchap2changepasswd(char *account_name, char *passwordData, char *passwordHash, u_int8_t passwordFormat, char *slot_id)
{
    tDirReference	dirRef = NULL;
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = NULL;
	char *targetaccount = NULL;
	char recordName[255] = {0};
		
    status = dsOpenDirService(&dirRef);
	
	if (status != eDSNoErr)
		return status;
	
	status = get_node_ref_and_name(dirRef, account_name, kDSStdRecordTypeUsers, &nodeRef, recordName);

	if (status != eDSNoErr)
		goto cleanup;

	status = opendirectory_authenticate_node(dirRef, nodeRef);

	if (status != eDSNoErr)
		goto cleanup;

	if (slot_id && strlen(slot_id))
		targetaccount = slot_id;
	else
		targetaccount = recordName;

	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef,  "dsAuthMethodStandard:dsAuthMSLMCHAP2ChangePasswd" /*kDSStdAuthMSLMCHAP2ChangePasswd*/);
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeUsers);
					if ( authType != NULL )
					{
							// Target account
							opendirectory_add_data_buffer_item(authBuff, strlen( targetaccount ), targetaccount);
							opendirectory_add_data_buffer_item(authBuff, 1, &passwordFormat);
							opendirectory_add_data_buffer_item(authBuff, 516, passwordData);
							
							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 1, authBuff, stepBuff, NULL,  recordType);
							if ( status == eDSNoErr )
							{
									//DEBUG(1,("kDSStdAuthMSLMCHAP2ChangePasswd was successful for  \"%s\" :)\n", machine_acct));

							}
							else
							{
									// DEBUG(1,("kDSStdAuthMSLMCHAP2ChangePasswd FAILED for user \"%s\" (%d) :(\n", userid, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							// DEBUG(1,("kDSStdAuthMSLMCHAP2ChangePasswd: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					// DEBUG(1,("kDSStdAuthMSLMCHAP2ChangePasswd: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					// DEBUG(1,( "kDSStdAuthMSLMCHAP2ChangePasswd: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			// DEBUG(1,("kDSStdAuthMSLMCHAP2ChangePasswd: *** dsDataBufferAllocate(1) faild with \n" ));
	}
cleanup:
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	if (dirRef)
		dsCloseDirService(dirRef);
	if (nodeRef)
		dsCloseDirNode(nodeRef);
	return status;
}

tDirStatus opendirectory_authenticate_node(tDirReference	dirRef, tDirNodeReference nodeRef)
{
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	void				*authenticator = NULL;
	
	authenticator = get_opendirectory_authenticator();
	
	if (authenticator == NULL || 
		get_opendirectory_authenticator_accountlen(authenticator) == 0 || 
		get_opendirectory_authenticator_secretlen(authenticator) == 0) {
		return eDSNullParameter;
	}
		
	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef,  kDSStdAuthNodeNativeClearTextOK);
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeUsers);
					if ( authType != NULL )
					{
							// Account Name (authenticator)
							opendirectory_add_data_buffer_item(authBuff, get_opendirectory_authenticator_accountlen(authenticator), get_opendirectory_authenticator_account(authenticator));
							// Password (authenticator password)
							opendirectory_add_data_buffer_item(authBuff, get_opendirectory_authenticator_secretlen(authenticator), get_opendirectory_authenticator_secret(authenticator));

							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 0, authBuff, stepBuff, NULL,  recordType);
							if ( status == eDSNoErr )
							{
									//DEBUG(1,("kDSStdAuthNodeNativeClearTextOK was successful for  \"%s\" :)\n", machine_acct));
							}
							else
							{
									// DEBUG(1,("kDSStdAuthNodeNativeClearTextOK FAILED for user \"%s\" (%d) :(\n", userid, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							// DEBUG(1,("kDSStdAuthNodeNativeClearTextOK: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					// DEBUG(1,("kDSStdAuthNodeNativeClearTextOK: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					// DEBUG(1,( "kDSStdAuthNodeNativeClearTextOK: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			// DEBUG(1,("kDSStdAuthNodeNativeClearTextOK: *** dsDataBufferAllocate(1) faild with \n" ));
	}
	
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	delete_opendirectory_authenticator(authenticator);
	return status;
}
