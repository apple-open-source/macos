/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */

/*
 * ldapFetch.cpp - fetch an entity via LDAP
 */

#include "ldapFetch.h"
#include <LDAP/ldap.h>
#include <Security/cssmapple.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>

#define DEBUG_PRINT		1
#if		DEBUG_PRINT
#define dprintf(args...)	printf(args)
#else
#define dprintf(args...)
#endif

/*
 * LDAP attribute names, used if not present in URI.
 */
#define LDAP_ATTR_CERT		"cacertificate;binary"
#define LDAP_ATTR_CRL		"certificaterevocationlist;binary"

/*
 * Default LDAP options.
 */
#define LDAP_REFERRAL_DEFAULT	LDAP_OPT_ON

static CSSM_RETURN ldapRtnToCssm(
	int rtn)
{
	switch(rtn) {
		case LDAP_SERVER_DOWN:
		case LDAP_TIMEOUT:
		case LDAP_CONNECT_ERROR:
			return CSSMERR_APPLETP_CRL_SERVER_DOWN;
		case LDAP_PARAM_ERROR:
		case LDAP_FILTER_ERROR:
			return CSSMERR_APPLETP_CRL_BAD_URI;
		default:
			return CSSMERR_APPLETP_CRL_NOT_FOUND;
	}
}

CSSM_RETURN ldapFetch(
	const CSSM_DATA 	&url,
	LF_Type				lfType,
	CSSM_DATA			&fetched)	// mallocd and RETURNED
{
	BerValue 		**value = NULL;
	LDAPURLDesc 	*urlDesc = NULL;
	int 			rtn;
	LDAPMessage 	*msg = NULL;
	LDAP 			*ldap = NULL;
	LDAPMessage 	*entry = NULL;
	bool 			mallocdString = false;
	char 			*urlStr;
	int 			numEntries;
	CSSM_RETURN 	ourRtn = CSSM_OK;
	/* attr input to ldap_search_s() */
	char			*attrArray[2];
	char			**attrArrayP = NULL;
	
	/* don't assume URL string is NULL terminated */
	if(url.Data[url.Length - 1] == '\0') {
		urlStr = (char *)url.Data;
	}
	else {
		urlStr = (char *)malloc(url.Length + 1);
		memmove(urlStr, url.Data, url.Length);
		urlStr[url.Length] = '\0';
		mallocdString = true;
	}
	
	/* break up the URL into something usable */
	rtn = ldap_url_parse(urlStr, &urlDesc);
	if(rtn) {
		dprintf("ldap_url_parse returned %d", rtn);
		return CSSMERR_APPLETP_CRL_BAD_URI;
	}
	
	/*
	 * Determine what attr we're looking for.
	 */
	if((urlDesc->lud_attrs != NULL) &&		// attrs present in URL
	   (urlDesc->lud_attrs[0] != NULL) &&	// at least one attr present
	   (urlDesc->lud_attrs[1] == NULL))	{
		/*
		 * Exactly one attr present in the caller-specified URL;
		 * assume that this is exactly what we want. 
		 */
		attrArrayP = &urlDesc->lud_attrs[0];
	}
	else {
		/* use caller-specified attr */
		switch(lfType) {
			case LT_Crl:
				attrArray[0] = (char *)LDAP_ATTR_CRL;
				break;
			case LT_Cert:
				attrArray[0] = (char *)LDAP_ATTR_CERT;
				break;
			default:
				printf("***ldapFetch screwup: bogus lfType (%d)\n",
					(int)lfType);
				return CSSMERR_CSSM_INTERNAL_ERROR;
		}
		attrArray[1] = NULL;
		attrArrayP = &attrArray[0];
	}
	
	/* establish connection */
	rtn = ldap_initialize(&ldap, urlStr);
	if(rtn) {
		dprintf("ldap_initialize returned %d\n", rtn);
		return ldapRtnToCssm(rtn);
	}
	/* subsequent errors to cleanup: */
	rtn = ldap_simple_bind_s(ldap, NULL, NULL);
	if(rtn) {
		dprintf("ldap_simple_bind_s returned %d\n", rtn);
		ourRtn = ldapRtnToCssm(rtn);
		goto cleanup;
	}
	
	rtn = ldap_set_option(ldap, LDAP_OPT_REFERRALS, LDAP_REFERRAL_DEFAULT);
	if(rtn) {
		dprintf("ldap_set_option(referrals) returned %d\n", rtn);
		ourRtn = ldapRtnToCssm(rtn);
		goto cleanup;
	}
	
	rtn = ldap_search_s(
		ldap, 
		urlDesc->lud_dn, 
		LDAP_SCOPE_SUBTREE,
		urlDesc->lud_filter, 
		urlDesc->lud_attrs, 
		0, 			// attrsonly
		&msg);
	if(rtn) {
		dprintf("ldap_search_s returned %d\n", rtn);
		ourRtn = ldapRtnToCssm(rtn);
		goto cleanup;
	}

	/* 
	 * We require exactly one entry (for now).
	 */
	numEntries = ldap_count_entries(ldap, msg);
	if(numEntries != 1) {
		dprintf("tpCrlViaLdap: numEntries %d\n", numEntries);
		ourRtn = CSSMERR_APPLETP_CRL_NOT_FOUND;
		goto cleanup;
	}
	
	entry = ldap_first_entry(ldap, msg);
	value = ldap_get_values_len(ldap, msg, attrArrayP[0]);
	if(value == NULL) {
		dprintf("Error on ldap_get_values_len\n");
		ourRtn = CSSMERR_APPLETP_CRL_NOT_FOUND;
		goto cleanup;
	}
	
	fetched.Length = value[0]->bv_len;
	fetched.Data = (uint8 *)malloc(fetched.Length);
	memmove(fetched.Data, value[0]->bv_val, fetched.Length);

	ldap_value_free_len(value);
	ourRtn = CSSM_OK;
cleanup:
	if(msg) {
		ldap_msgfree(msg);
	}
	if(mallocdString) {
		free(urlStr);
	}
	ldap_free_urldesc(urlDesc);
	rtn = ldap_unbind(ldap);
	if(rtn) {
		dprintf("Error %d on ldap_unbind\n", rtn);
		/* oh well */
	}
	return ourRtn;
}

/* fetch via HTTP */
CSSM_RETURN httpFetch(
	const CSSM_DATA 	&url,
	CSSM_DATA			&fetched)	// mallocd and RETURNED
{
	/* trim off possible NULL terminator */
	CSSM_DATA theUrl = url;
	if(theUrl.Data[theUrl.Length - 1] == '\0') {
		theUrl.Length--;
	}
	CFURLRef cfUrl = CFURLCreateWithBytes(NULL,
		theUrl.Data, theUrl.Length,
		kCFStringEncodingUTF8,		// right?
		//kCFStringEncodingASCII,		// right?
		NULL);						// this is absolute path 
	if(cfUrl == NULL) {
		dprintf("CFURLCreateWithBytes returned NULL\n");
		return CSSMERR_APPLETP_CRL_BAD_URI;
	}
	CFDataRef urlData = NULL;
	SInt32 errorCode;
	Boolean brtn = CFURLCreateDataAndPropertiesFromResource(NULL,
		cfUrl,
		&urlData, 
		NULL,			// no properties
		NULL,
		&errorCode);
	CFRelease(cfUrl);
	if(!brtn) {
		dprintf("CFURLCreateDataAndPropertiesFromResource err: %d\n",
			(int)errorCode);
		if(urlData) {
			return CSSMERR_APPLETP_CRL_BAD_URI;
		}
	}
	if(urlData == NULL) {
		dprintf("CFURLCreateDataAndPropertiesFromResource: no data\n");
		return CSSMERR_APPLETP_CRL_BAD_URI;
	}
	CFIndex len = CFDataGetLength(urlData);
	fetched.Data = (uint8 *)malloc(len);
	fetched.Length = len;
	memmove(fetched.Data, CFDataGetBytePtr(urlData), len);
	CFRelease(urlData);
	return CSSM_OK;
}

/* Fetch from net, we figure out the schema */
CSSM_RETURN netFetch(
	const CSSM_DATA 	&url,
	LF_Type				lfType,
	CSSM_DATA			&fetched)	// mallocd and RETURNED
{
	if(url.Length < 5) {
		return CSSMERR_APPLETP_CRL_BAD_URI;
	}
	if(!strncmp((char *)url.Data, "ldap:", 5)) {
		return ldapFetch(url, lfType, fetched);
	}
	if(!strncmp((char *)url.Data, "http:", 5) ||
	   !strncmp((char *)url.Data, "https:", 6)) {	
		return httpFetch(url, fetched);
	}
	return CSSMERR_APPLETP_CRL_BAD_URI;
}

