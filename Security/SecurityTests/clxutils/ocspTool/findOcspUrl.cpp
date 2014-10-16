/*
 * Copyright (c) 2002,2005 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * findOcspUrl.cpp - find URL for OCSP for aspecified cert.
 */
 
#include <Security/SecAsn1Coder.h>
#include <clAppUtils/CertParser.h>
#include <utilLib/common.h>

/* 
 * Examine cert, looking for AuthorityInfoAccess, with id-ad-ocsp URIs. Return
 * the first URL found.
 */
CSSM_DATA *ocspUrlFromCert(
	CertParser &subject, 
	SecAsn1CoderRef coder)
{
	CE_AuthorityInfoAccess *aia = (CE_AuthorityInfoAccess *)
		subject.extensionForOid(CSSMOID_AuthorityInfoAccess);
	if(aia == NULL) {
		printf("***No AIA extension found; OCSP aborted.\n");
		return NULL;
	}
	CSSM_DATA *url = NULL;
	for(unsigned dex=0; dex<aia->numAccessDescriptions; dex++) {
		CE_AccessDescription *ad = &aia->accessDescriptions[dex];
		if(!appCompareCssmData(&ad->accessMethod, &CSSMOID_AD_OCSP)) {
			continue;
		}
		CE_GeneralName *genName = &ad->accessLocation;
		if(genName->nameType != GNT_URI) {
			printf("tpOcspUrlsFromCert: CSSMOID_AD_OCSP, but not type URI");
			continue;
		}
		
		/* got one */
		url = (CSSM_DATA *)SecAsn1Malloc(coder, sizeof(CSSM_DATA));
		SecAsn1AllocCopyItem(coder, &genName->name, url);
		return url;
	}
	
	printf("***No AIA extension with URI found; OCSP aborted.\n");
	return NULL;
}
