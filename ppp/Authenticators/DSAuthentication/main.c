/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  plugin to add support for authentication through Directory Services.
 *
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */
#include <stdio.h>
 
#include <syslog.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <Security/Security.h>
#include "../../Helpers/vpnd/RASSchemaDefinitions.h"
#include "../../Helpers/pppd/pppd.h"
#include "../../Helpers/pppd/chap-new.h"
#include "../../Helpers/pppd/chap_ms.h"
#include "../../Family/ppp_comp.h"
#include "DSUser.h"

#define BUF_LEN 1024

char serviceName[] = "com.apple.ras";

static CFBundleRef 	bundle = 0;

extern u_char mppe_send_key[MPPE_MAX_KEY_LEN];
extern u_char mppe_recv_key[MPPE_MAX_KEY_LEN];
extern int mppe_keys_set;		/* Have the MPPE keys been set? */
extern CFPropertyListRef 		systemOptions;

static int dsauth_check(void);
static int dsauth_ip_allowed_address(u_int32_t addr);
static int dsauth_pap(char *user, char *passwd, char **msgp, struct wordlist **paddrs, struct wordlist **popts);
static int dsauth_chap(char *name, char *ourname, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *response,
			unsigned char *message, int message_space);
static void dsauth_set_mppe_keys(tDirReference dirRef, tDirNodeReference userNode, char* user, u_char *remmd, 
			int remmd_len, tAttributeValueEntryPtr authAuthorityAttr);
static void dsauth_get_admin_acct(u_int32_t *acctNameSize, char** acctName, u_int32_t *passwordSize, char **password);
static int dsauth_get_admin_password(u_int32_t acctlen, char* acctname, u_int32_t *password_len, char **password);
static int dsauth_find_user_node(tDirReference dirRef, char *user_name, tDirNodeReference *user_node, 
            tAttributeValueEntryPtr *recordNameAttr, tAttributeValueEntryPtr *authAuthorityAttr);

/* -----------------------------------------------------------------------------
plugin entry point, called by pppd
----------------------------------------------------------------------------- */

int start(CFBundleRef ref)
{

    bundle = ref;
    CFRetain(bundle);

    pap_check_hook = dsauth_check;
    pap_auth_hook = dsauth_pap;

    chap_check_hook = dsauth_check;
    chap_verify_hook = dsauth_chap;
   
    allowed_address_hook = dsauth_ip_allowed_address;

    //add_options(Options);

    info("Directory Services Authentication plugin initialized");
        
    return 0;
}

//----------------------------------------------------------------------
//	dsauth_check
//----------------------------------------------------------------------
static int dsauth_check(void)
{
    return 1;
}

//----------------------------------------------------------------------
//    dsauth_ip_allowed_address
//----------------------------------------------------------------------
static int dsauth_ip_allowed_address(u_int32_t addr)
{
    // any address is OK
    return 1;    
}

//----------------------------------------------------------------------
//      dsauth_pap
//----------------------------------------------------------------------
static int dsauth_pap(char *user, char *passwd, char **msgp, struct wordlist **paddrs, struct wordlist **popts)
{
    tDirReference				dirRef;
    tDirNodeReference			userNode = 0;
    tDataNodePtr				authTypeDataNodePtr = 0;
    tDataBufferPtr				authDataBufPtr = 0;
    tDataBufferPtr				responseDataBufPtr = 0;
    tAttributeValueEntryPtr 	recordNameAttr = 0;
    tAttributeValueEntryPtr 	authAuthorityAttr = 0;
    tDirStatus					dsResult = eDSNoErr;
    char						*ptr;
    int							authResult = 0;
    u_int32_t					userShortNameSize;
    
    u_int32_t					passwordSize = strlen(passwd);
    u_int32_t					authDataSize;

    if ((dsResult = dsOpenDirService(&dirRef)) == eDSNoErr) {    

        if ((responseDataBufPtr = dsDataBufferAllocate(dirRef, BUF_LEN)) == 0) {
            error("DSAuth plugin: Could not allocate data buffer\n");
            goto cleanup;
        }
        if ((authTypeDataNodePtr = dsDataNodeAllocateString(dirRef, kDSStdAuthNodeNativeNoClearText)) == 0) {
            error("DSAuth plugin: Could not allocate data buffer\n");
            goto cleanup;
        }

        if (dsauth_find_user_node(dirRef, user, &userNode, &recordNameAttr, &authAuthorityAttr) == 0) {            
            userShortNameSize = recordNameAttr->fAttributeValueData.fBufferLength;
            authDataSize = userShortNameSize + passwordSize + (2 * sizeof(u_int32_t));
            
            if ((authDataBufPtr = dsDataBufferAllocate(dirRef, authDataSize)) != 0) {   
                authDataBufPtr->fBufferLength = authDataSize;
            
                /* store user name and password into the auth buffer in the correct format */
                ptr = (char*)(authDataBufPtr->fBufferData);
                
                // 4 byte length & user name
                *((u_int32_t*)ptr) =  userShortNameSize;
                ptr += sizeof(u_int32_t);
                memcpy(ptr, recordNameAttr->fAttributeValueData.fBufferData, userShortNameSize);
                ptr += userShortNameSize;
                
                // 4 byte length & password
                *((u_int32_t*)ptr) = passwordSize;
                ptr += sizeof(u_int32_t);
                memcpy(ptr, passwd, passwordSize);
                
                if ((dsResult = dsDoDirNodeAuth(userNode, authTypeDataNodePtr, TRUE, authDataBufPtr, 
                        responseDataBufPtr, 0)) == eDSNoErr) {						
                    authResult = 1;
                    info("DSAuth plugin: user authentication successful\n");
                }
                
                bzero(authDataBufPtr->fBufferData, authDataSize);	// don't leave password in buffer
            }
            dsCloseDirNode(userNode); 					// returned from dsauth_find_user()
            dsDeallocAttributeValueEntry(dirRef, recordNameAttr); 
            dsDeallocAttributeValueEntry(dirRef, authAuthorityAttr); 
        }
    
cleanup:

        if (responseDataBufPtr)
            dsDataBufferDeAllocate(dirRef, responseDataBufPtr);
        if (authTypeDataNodePtr)
            dsDataNodeDeAllocate(dirRef, authTypeDataNodePtr);
        if (authDataBufPtr)
            dsDataBufferDeAllocate(dirRef, authDataBufPtr);

        dsCloseDirService(dirRef);
    }
    
    return authResult;

}

//----------------------------------------------------------------------
//	dsauth_chap
//----------------------------------------------------------------------

#define CHALLENGE_SIZE		16
#define NT_RESPONSE_SIZE	24

static int dsauth_chap(char *name, char *ourname, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *response,
			unsigned char *message, int message_space)
{
    tDirReference				dirRef;
    tDirNodeReference			userNode = 0;
    tDataNodePtr				authTypeDataNodePtr = 0;
    tDataBufferPtr				authDataBufPtr = 0;
    tDataBufferPtr				responseDataBufPtr = 0;
    tAttributeValueEntryPtr 	recordNameAttr = 0;
    tAttributeValueEntryPtr 	authAuthorityAttr = 0;
    tDirStatus					dsResult = eDSNoErr;
    int							authResult = 0;
    char						*ptr;
    MS_Chap2Response			*resp;  
    u_int32_t					userShortNameSize;
    u_int32_t					userNameSize = strlen(name);
    u_int32_t					authDataSize;
    int							challenge_len, response_len;
	CFMutableDictionaryRef		serviceInfo = 0;
	CFMutableDictionaryRef		eventDetail;
	CFDictionaryRef				interface;
	CFStringRef					subtypeRef;
	CFStringRef					addrRef;

    
    challenge_len = *challenge++;	/* skip length, is 16 */
    response_len = *response++;

    // currently only support MS-CHAPv2
    if (digest->code != CHAP_MICROSOFT_V2
    	||	response_len != MS_CHAP2_RESPONSE_LEN
    	||	challenge_len != CHALLENGE_SIZE)
        return 0;

    resp = (MS_Chap2Response*)response;
    if ((dsResult = dsOpenDirService(&dirRef)) == eDSNoErr) {
	
        if ((authTypeDataNodePtr = dsDataNodeAllocateString(dirRef, kDSStdAuthMSCHAP2)) == 0) {
            error("DSAuth plugin: Could not allocate data buffer\n");
            goto cleanup;
        }
		
		//  setup service info
		interface = CFDictionaryGetValue(systemOptions, kRASEntInterface);
		if (interface && CFGetTypeID(interface) == CFDictionaryGetTypeID()) {
			subtypeRef = CFDictionaryGetValue(interface, kRASPropInterfaceSubType);		
			if (subtypeRef && CFGetTypeID(subtypeRef) == CFStringGetTypeID()) {	
				serviceInfo = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				if (serviceInfo) {
					eventDetail = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
					if (eventDetail) {
						addrRef = CFStringCreateWithCString(0, remoteaddress, kCFStringEncodingUTF8);
						if (addrRef) {
							CFDictionaryAddValue(eventDetail, CFSTR("ClientIP"),  addrRef);
							CFRelease(addrRef);	
						}
						if (CFStringCompare(subtypeRef, kRASValInterfaceSubTypeL2TP, 0) == kCFCompareEqualTo) {
							CFDictionaryAddValue(eventDetail, CFSTR("HostPort"),  CFSTR("1701"));
							CFDictionaryAddValue(eventDetail, CFSTR("ProtocolName"),  CFSTR("L2TP"));
							CFDictionaryAddValue(eventDetail, CFSTR("ProtocolVersion"),  CFSTR("2"));
						} else if (CFStringCompare(subtypeRef, kRASValInterfaceSubTypePPTP, 0) == kCFCompareEqualTo) {
							CFDictionaryAddValue(eventDetail, CFSTR("HostPort"),  CFSTR("1723"));
							CFDictionaryAddValue(eventDetail, CFSTR("ProtocolName"),  CFSTR("PPTP"));
							CFDictionaryAddValue(eventDetail, CFSTR("ProtocolVersion"),  CFSTR("1"));
						} else
							CFDictionaryAddValue(eventDetail, CFSTR("ProtocolName"),  subtypeRef);
						
						CFDictionaryAddValue(eventDetail, CFSTR("ServiceName"),  CFSTR("VPN"));
						
						// add eventDetail to serviceInfo dict
						CFDictionaryAddValue(serviceInfo, CFSTR("ServiceInformation"), eventDetail);
						CFRelease(eventDetail);
						
						// allocate response buffer with service info
						if (dsServiceInformationAllocate(serviceInfo, BUF_LEN, &responseDataBufPtr) != eDSNoErr) {
							error("DSAuth plugin: Unable to allocate service info buffer\n");
							goto cleanup;
						}
					} else {
						error("DSAuth plugin: Unable to allocate eventDetail dictionary\n");
						goto cleanup;
					}
				} else {
					error("DSAuth plugin: Unable to allocate serviceInfo dictionary\n");
					goto cleanup;
				}
			} else {
				error("DSAccessControl plugin: No Interface subtype found\n");
				goto cleanup;
			}
		} else {
			error("DSAccessControl plugin: No Interface dictionary found\n");
			goto cleanup;
		}

        if (dsauth_find_user_node(dirRef, name, &userNode, &recordNameAttr, &authAuthorityAttr) == 0) {  
            userShortNameSize = recordNameAttr->fAttributeValueData.fBufferLength;
            authDataSize = userNameSize + userShortNameSize + NT_RESPONSE_SIZE + (2 * CHALLENGE_SIZE) + (5 * sizeof(u_int32_t));   
            if ((authDataBufPtr = dsDataBufferAllocate(dirRef, authDataSize)) != 0) {   
                authDataBufPtr->fBufferLength = authDataSize;
                 
                // setup the response buffer           
                ptr = (char*)(authDataBufPtr->fBufferData);
                
                // 4 byte length & user name
                *((u_int32_t*)ptr) = userShortNameSize;
                ptr += sizeof(u_int32_t);
                memcpy(ptr, recordNameAttr->fAttributeValueData.fBufferData, userShortNameSize);
                ptr += userShortNameSize;
                
                // 4 byte length & server challenge
                *((u_int32_t*)ptr) = CHALLENGE_SIZE;
                ptr += sizeof(u_int32_t);
                memcpy(ptr, challenge, CHALLENGE_SIZE);
                ptr += CHALLENGE_SIZE;
                
                // 4 byte length & peer challenge
                *((u_int32_t*)ptr) = CHALLENGE_SIZE;
                ptr += sizeof(u_int32_t);
                memcpy(ptr, resp->PeerChallenge, CHALLENGE_SIZE);
                ptr += CHALLENGE_SIZE;
                
                // 4 byte length & client digest
                *((u_int32_t*)ptr) = NT_RESPONSE_SIZE;
                ptr += sizeof(u_int32_t);
                memcpy(ptr, resp->NTResp, NT_RESPONSE_SIZE);
                ptr += NT_RESPONSE_SIZE;
                
                // 4 byte length & user name (repeated)
                *((u_int32_t*)ptr) = userNameSize;
                ptr += sizeof(u_int32_t);
                memcpy(ptr, name, userNameSize);
                
                if ((dsResult = dsDoDirNodeAuth(userNode, authTypeDataNodePtr, TRUE, authDataBufPtr, 
                        responseDataBufPtr, 0)) == eDSNoErr) {						
                    
                    // setup return data
                    if ((responseDataBufPtr->fBufferLength == MS_AUTH_RESPONSE_LENGTH + 4)
                                && *((u_int32_t*)(responseDataBufPtr->fBufferData)) == MS_AUTH_RESPONSE_LENGTH) {

                        responseDataBufPtr->fBufferData[4 + MS_AUTH_RESPONSE_LENGTH] = 0;
                         if (resp->Flags[0])
                                slprintf(message, message_space, "S=%s", responseDataBufPtr->fBufferData + 4);
                        else
                                slprintf(message, message_space, "S=%s M=%s",
                                        responseDataBufPtr->fBufferData + 4, "Access granted");
                        authResult = 1;
                        dsauth_set_mppe_keys(dirRef, userNode, recordNameAttr->fAttributeValueData.fBufferData, response, response_len, authAuthorityAttr);
                    } 
                } 
            }
            dsCloseDirNode(userNode);
            dsDeallocAttributeValueEntry(dirRef, recordNameAttr);
            dsDeallocAttributeValueEntry(dirRef, authAuthorityAttr);
        }
    
cleanup:
		if (serviceInfo)
			CFRelease(serviceInfo);
        if (responseDataBufPtr)
            dsDataBufferDeAllocate(dirRef, responseDataBufPtr);
        if (authTypeDataNodePtr)
            dsDataNodeDeAllocate(dirRef, authTypeDataNodePtr);
        if (authDataBufPtr)
            dsDataBufferDeAllocate(dirRef, authDataBufPtr);

        dsCloseDirService(dirRef);
    }

    return authResult;
}

//----------------------------------------------------------------------
//	dsauth_set_mppe_keys
//		get the mppe keys from the password server
//		if this fails - do nothing.  there is no way to
//		know if the keys are actually going to be needed
//		at this point.
//----------------------------------------------------------------------
static void dsauth_set_mppe_keys(tDirReference dirRef, tDirNodeReference userNode, char* user, u_char *remmd, 
		int remmd_len, tAttributeValueEntryPtr authAuthorityAttr)
{
    tDataNodePtr		authTypeDataNodePtr = 0;
    tDataNodePtr		authKeysDataNodePtr = 0;
    tDataBufferPtr		authDataBufPtr = 0;
    tDataBufferPtr		responseDataBufPtr = 0;
    tDirStatus 			dsResult = eDSNoErr;
    char				*ptr, *tagStart;
    MS_Chap2Response	*resp;  
    char				*keyaccessPassword = 0;
    char				*keyaccessName = 0;
    u_int32_t			keyaccessNameSize = 0;
    u_int32_t			keyaccessPasswordSize;
    int					len, useKeyAgent, i;
    u_int32_t			userNameSize = strlen(user);

    mppe_keys_set = 0;
	useKeyAgent = 0;

	// parse authAuthorityAttribute to determine if key agent needs to be used.
	// auth authority tag will be between 2 semicolons.
	tagStart = 0;
	ptr = authAuthorityAttr->fAttributeValueData.fBufferData;
	for (i = 0; i < authAuthorityAttr->fAttributeValueData.fBufferLength; i++) {
		if (*ptr == ';') {
			if (tagStart == 0)
				tagStart = ptr + 1;
			else
				break;
		}
		ptr++;
	}
	
	if (*ptr != ';')
		return;
	if (strncmp(tagStart, kDSTagAuthAuthorityPasswordServer, ptr - tagStart) == 0) {
		useKeyAgent = 1;
		dsauth_get_admin_acct(&keyaccessNameSize, &keyaccessName, &keyaccessPasswordSize, &keyaccessPassword);
		if (keyaccessName == 0) {
			error("DSAuth plugin: Could not retrieve key agent account information.\n");
			return;
		}
	} else if (strncmp(tagStart, kDSTagAuthAuthorityShadowHash, ptr - tagStart) == 0) {
		useKeyAgent = 0;
	}
	else
		return;		// unsupported authentication authority - don't set the keys
        
    resp = (MS_Chap2Response*)remmd;
    
    if ((responseDataBufPtr = dsDataBufferAllocate(dirRef, BUF_LEN)) == 0) {
        error("DSAuth plugin: Could not allocate data buffer\n");
        goto cleanup;
    }
    if ((authTypeDataNodePtr = dsDataNodeAllocateString(dirRef, kDSStdAuthNodeNativeNoClearText)) == 0) {
        error("DSAuth plugin: Could not allocate data buffer\n");
        goto cleanup;
    }
    if ((authKeysDataNodePtr = dsDataNodeAllocateString(dirRef, 
                    "dsAuthMethodStandard:dsAuthVPN_MPPEMasterKeys" /* kDSStdAuthVPN_MPPEMasterKeys */)) == 0) {
        error("DSAuth plugin: Could not allocate data buffer\n");
        goto cleanup;
    }
    if ((authDataBufPtr = dsDataBufferAllocate(dirRef, BUF_LEN)) == 0) {   
        error("DSAuth plugin: Could not allocate data buffer\n");
        goto cleanup;
    }

	if (useKeyAgent) {
	
		// need to use key agent to get MPPE keys for user in LDAP domain	
		ptr = (char*)(authDataBufPtr->fBufferData);
		
		// 4 byte length & admin name
		*((u_int32_t*)ptr) = keyaccessNameSize;
		ptr += sizeof(u_int32_t);
		memcpy(ptr, keyaccessName, keyaccessNameSize);
		ptr += keyaccessNameSize;

		// 4 byte length & password
		*((u_int32_t*)ptr) = keyaccessPasswordSize;
		ptr += sizeof(u_int32_t);
		memcpy(ptr, keyaccessPassword, keyaccessPasswordSize);
		
		authDataBufPtr->fBufferLength = keyaccessNameSize + keyaccessPasswordSize + (2 * sizeof(u_int32_t));
		
		// authenticate to the user node          
		if ((dsResult = dsDoDirNodeAuth(userNode, authTypeDataNodePtr, false, authDataBufPtr, 
				responseDataBufPtr, 0)) != eDSNoErr) {
			error("DSAuth plugin: Could not authenticate key agent for encryption key retrieval.\n");
			goto cleanup;
		}
	}
			
	ptr = (char*)(authDataBufPtr->fBufferData);
	// get the mppe keys
	
	// 4 byte length & user name
	*((u_int32_t*)ptr) = userNameSize;
	ptr += sizeof(u_int32_t);
	memcpy(ptr, user, userNameSize);
	ptr += userNameSize;

	// 4 byte length & client digest
	*((u_int32_t*)ptr) = NT_RESPONSE_SIZE;
	ptr += sizeof(u_int32_t);
	memcpy(ptr, resp->NTResp, NT_RESPONSE_SIZE);
	ptr += NT_RESPONSE_SIZE;

	// 4 byte length and master key len - always 128
	*((u_int32_t*)ptr) = 1;
	ptr += sizeof(u_int32_t);
	*ptr = MPPE_MAX_KEY_LEN;

	authDataBufPtr->fBufferLength = userNameSize + NT_RESPONSE_SIZE + 1 + (3 * sizeof(u_int32_t));

	// get the keys
	if ((dsResult = dsDoDirNodeAuth(userNode, authKeysDataNodePtr, false, authDataBufPtr, 
		responseDataBufPtr, 0)) == eDSNoErr) {						
		if (responseDataBufPtr->fBufferLength == (2 * sizeof(u_int32_t)) + (2 * MPPE_MAX_KEY_LEN)) {
			ptr = (char*)(responseDataBufPtr->fBufferData);
			len = *((u_int32_t*)ptr);
			ptr += sizeof(u_int32_t);
			if (len == sizeof(mppe_send_key))
					memcpy(mppe_send_key, ptr, sizeof(mppe_send_key));
			ptr += len;
			len = *((u_int32_t*)ptr);
			ptr += sizeof(u_int32_t);
			if (len == sizeof(mppe_recv_key))
					memcpy(mppe_recv_key, ptr, sizeof(mppe_recv_key));
			mppe_keys_set = 1;
		}
	} else
		error("DSAuth plugin: Failed to retrieve MPPE encryption keys from the password server.\n");
        
cleanup:
    if (keyaccessPassword) {
		bzero(keyaccessPassword, keyaccessPasswordSize);	// clear the admin password from memory
#ifndef TARGET_EMBEDDED_OS
        SecKeychainItemFreeContent(NULL, keyaccessPassword);
#endif /* TARGET_EMBEDDED_OS */
    }
    if (keyaccessName) {
        free(keyaccessName);
    }
    if (responseDataBufPtr)
        dsDataBufferDeAllocate(dirRef, responseDataBufPtr);
    if (authTypeDataNodePtr)
        dsDataNodeDeAllocate(dirRef, authTypeDataNodePtr);
    if (authKeysDataNodePtr)
        dsDataNodeDeAllocate(dirRef, authKeysDataNodePtr);
    if (authDataBufPtr)
        dsDataBufferDeAllocate(dirRef, authDataBufPtr);
   
}

//----------------------------------------------------------------------
//	dsauth_get_admin_acct
//----------------------------------------------------------------------
static void dsauth_get_admin_acct(u_int32_t *acctNameSize, char** acctName, u_int32_t *passwordSize, char **password)
{

    SCPreferencesRef 	prefs;
    CFPropertyListRef	globals;
    CFStringRef			acctNameRef;
    char				namestr[256];
    u_int32_t			namelen;
       
    *passwordSize = 0;
    *password = 0;
    *acctNameSize = 0;
    *acctName = 0;

    //
    // get the acct name from the plist
    //
    if ((prefs = SCPreferencesCreate(0, CFSTR("pppd"), kRASServerPrefsFileName)) != 0) {
        // get globals dict from the plist
        if ((globals = SCPreferencesGetValue(prefs, kRASGlobals)) != 0) {
            // retrieve the password server account id
            if ((acctNameRef = CFDictionaryGetValue(globals, KRASGlobPSKeyAccount)) != 0) {
                namestr[0] = 0;
                CFStringGetCString(acctNameRef, namestr, 256, kCFStringEncodingMacRoman);
                if (namestr[0]) {
                    namelen = strlen(namestr);

                    if (dsauth_get_admin_password(namelen, namestr, passwordSize, password) == 0) {
                        *acctNameSize = namelen;
                        *acctName = malloc(namelen + 1);
                        if (acctName != 0)
                            memcpy(*acctName, namestr, namelen + 1);
                       }
                } else
					error("DSAuth plugin: Key access user name not valid.\n");
            }
        }
        CFRelease(prefs);
    }
}

//----------------------------------------------------------------------
//	dsauth_get_admin_password
//----------------------------------------------------------------------
static int dsauth_get_admin_password(u_int32_t acctlen, char* acctname, u_int32_t *password_len, char **password)
{
#ifndef TARGET_EMBEDDED_OS
    SecKeychainRef	keychain = 0;
    OSStatus		status;
    
    if ((status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem)) == noErr) {
    	if ((status = SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem, &keychain)) == noErr) {
            status = SecKeychainFindGenericPassword(keychain, strlen(serviceName), serviceName,
                        acctlen, acctname, (UInt32*)password_len, (void**)password, NULL); 
        }
    }
    if (keychain)
      CFRelease(keychain);
      
    if (status == noErr)
        return 0;
    else {
		error("DSAuth plugin: Error %d while retrieving key agent password from the system keychain.\n", status);
        return -1;
	}
#else
	error("System Keychain support not available");
	return -1;
#endif /* TARGET_EMBEDDED_OS */
}
    
//----------------------------------------------------------------------
//	dsauth_find_user_node
//----------------------------------------------------------------------
static int dsauth_find_user_node(tDirReference dirRef, char *user_name, tDirNodeReference *user_node,
                        tAttributeValueEntryPtr *recordNameAttr, tAttributeValueEntryPtr *authAuthorityAttr)
{
    
    tDirStatus				dsResult = eDSNoErr;
    tDataListPtr			userPathDataListPtr = 0;
    UInt32					searchNodeCount;
    tAttributeValueEntryPtr	userNodePath = 0;
    tDirNodeReference 		searchNodeRef = 0;

    
    *user_node = 0;	// init return values
    *recordNameAttr = 0;
    *authAuthorityAttr = 0;

    // get search node ref
    if ((dsResult = dsauth_get_search_node_ref(dirRef, 1, &searchNodeRef, &searchNodeCount)) == eDSNoErr) {
        // get the meta node location attribute from the user's record
        dsResult =  dsauth_get_user_attr(dirRef, searchNodeRef, user_name, kDSNAttrMetaNodeLocation, &userNodePath);
        if (dsResult == eDSNoErr && userNodePath != 0) {
            // open the user node and return the node ref
            if (userPathDataListPtr = dsBuildFromPath(dirRef, userNodePath->fAttributeValueData.fBufferData, "/")) {
                dsResult = dsOpenDirNode(dirRef, userPathDataListPtr, user_node);
                dsDataListDeallocate(dirRef, userPathDataListPtr);
            }
            if (dsResult == eDSNoErr)
                dsResult = dsauth_get_user_attr(dirRef, searchNodeRef, user_name, kDSNAttrRecordName, recordNameAttr);
			if (dsResult == eDSNoErr)
                dsResult = dsauth_get_user_attr(dirRef, searchNodeRef, user_name, kDSNAttrAuthenticationAuthority, authAuthorityAttr);
        }
        if (userNodePath)
            dsDeallocAttributeValueEntry(dirRef, userNodePath);
        dsCloseDirNode(searchNodeRef);		// close the search node ref
    }
    if (dsResult != eDSNoErr || *user_node == 0 || *recordNameAttr == 0 || *authAuthorityAttr == 0) {
        if (*user_node)
            dsCloseDirNode(*user_node);
        if (*recordNameAttr)
            dsDeallocAttributeValueEntry(dirRef, *recordNameAttr);
        if (*authAuthorityAttr)
            dsDeallocAttributeValueEntry(dirRef, *authAuthorityAttr);
        return -1;
    }
    return 0;
}


