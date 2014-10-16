/*
 * extenCooker.cpp - module to cook up random (but reasonable)
 *                   versions of cert extensions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extenCooker.h"
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>

CSSM_BOOL randBool()
{
	unsigned r = genRand(1, 0x10000000);
	return (r & 0x1) ? CSSM_TRUE : CSSM_FALSE;
}

/* Fill a CSSM_DATA with random data. Its referent is allocd with malloc. */
void randData(	
	CSSM_DATA_PTR	data,
	uint8			maxLen)
{
	data->Data = (uint8 *)malloc(maxLen);
	simpleGenData(data, 1, maxLen);
}

/*
 * Various compare tests
 */
int compBool(
	CSSM_BOOL pre,
	CSSM_BOOL post,
	const char *desc)
{
	if(pre == post) {
		return 0;
	}
	printf("***Boolean miscompare on %s\n", desc);
	/* in case a CSSM_TRUE isn't exactly right... */
	switch(post) {
		case CSSM_FALSE:
		case CSSM_TRUE:
			break;
		default:
			printf("*** post value is %d expected %d\n", 
				(int)post, (int)pre);
			break;
	}
	return 1;
}

int compCssmData(
	CSSM_DATA &d1,
	CSSM_DATA &d2,
	const char *desc)
{
	if(appCompareCssmData(&d1, &d2)) {
		return 0;
	}
	printf("CSSM_DATA miscompare on %s\n", desc);
	return 1;
}	

#pragma mark ----- individual extension tests -----

#pragma mark --- CE_KeyUsage ---
void kuCreate(void *arg)
{
	CE_KeyUsage *ku = (CE_KeyUsage *)arg;
	
	/* set two random valid bits */
	*ku = 0;
	*ku |= 1 << genRand(7, 15);
	*ku |= 1 << genRand(7, 15);
}

unsigned kuCompare(const void *pre, const void *post)
{
	const CE_KeyUsage *kuPre = (CE_KeyUsage *)pre;
	const CE_KeyUsage *kuPost = (CE_KeyUsage *)post;
	if(*kuPre != *kuPost) {
		printf("***Miscompare in CE_KeyUsage\n");
		return 1;
	}
	return 0;
}

#pragma mark --- CE_BasicConstraints ---
void bcCreate(void *arg)
{
	CE_BasicConstraints *bc = (CE_BasicConstraints *)arg;
	bc->cA = randBool();
	bc->pathLenConstraintPresent = randBool();
	if(bc->pathLenConstraintPresent) {
		bc->pathLenConstraint = genRand(1,10);
	}
}

unsigned bcCompare(const void *pre, const void *post)
{
	const CE_BasicConstraints *bcpre = (CE_BasicConstraints *)pre;
	const CE_BasicConstraints *bcpost = (CE_BasicConstraints *)post;
	unsigned rtn = 0;
	
	rtn += compBool(bcpre->cA, bcpost->cA, "BasicConstraints.cA");
	rtn += compBool(bcpre->pathLenConstraintPresent, 
		bcpost->pathLenConstraintPresent, 
		"BasicConstraints.pathLenConstraintPresent");
	if(bcpre->pathLenConstraint != bcpost->pathLenConstraint) {
		printf("BasicConstraints.pathLenConstraint mismatch\n");
		rtn++;
	}
	return rtn;
}

#pragma mark --- CE_SubjectKeyID ---
void skidCreate(void *arg)
{
	CSSM_DATA_PTR skid = (CSSM_DATA_PTR)arg;
	randData(skid, 16);
}

unsigned skidCompare(const void *pre, const void *post)
{
	CSSM_DATA_PTR spre = (CSSM_DATA_PTR)pre;
	CSSM_DATA_PTR spost = (CSSM_DATA_PTR)post;
	return compCssmData(*spre, *spost, "SubjectKeyID");
}

void skidFree(void *arg)
{
	CSSM_DATA_PTR skid = (CSSM_DATA_PTR)arg;
	free(skid->Data);
}

#pragma mark --- CE_NetscapeCertType ---
void nctCreate(void *arg)
{
	CE_NetscapeCertType *nct = (CE_NetscapeCertType *)arg;
	
	/* set two random valid bits */
	*nct = 0;
	*nct |= 1 << genRand(8, 15);
	*nct |= 1 << genRand(8, 15);
}

unsigned nctCompare(const void *pre, const void *post)
{
	const CE_NetscapeCertType *nPre = (CE_NetscapeCertType *)pre;
	const CE_NetscapeCertType *nPost = (CE_NetscapeCertType *)post;
	if(*nPre != *nPost) {
		printf("***Miscompare in CE_NetscapeCertType\n");
		return 1;
	}
	return 0;
}

#pragma mark --- CE_ExtendedKeyUsage ---

/* a static array of meaningless OIDs, use 1.. NUM_SKU_OIDS */
CSSM_OID ekuOids[] = {
	CSSMOID_CrlNumber,
	CSSMOID_CrlReason,
	CSSMOID_HoldInstructionCode,
	CSSMOID_InvalidityDate
};
#define NUM_SKU_OIDS 4

void ekuCreate(void *arg)
{
	CE_ExtendedKeyUsage *eku = (CE_ExtendedKeyUsage *)arg;
	eku->numPurposes = genRand(1, NUM_SKU_OIDS);
	eku->purposes = ekuOids;
}

unsigned ekuCompare(const void *pre, const void *post)
{
	CE_ExtendedKeyUsage *ekupre = (CE_ExtendedKeyUsage *)pre;
	CE_ExtendedKeyUsage *ekupost = (CE_ExtendedKeyUsage *)post;
	
	if(ekupre->numPurposes != ekupost->numPurposes) {
		printf("CE_ExtendedKeyUsage.numPurposes miscompare\n");
		return 1;
	}
	unsigned rtn = 0;
	for(unsigned dex=0; dex<ekupre->numPurposes; dex++) {
		rtn += compCssmData(ekupre->purposes[dex],
			ekupost->purposes[dex], "CE_ExtendedKeyUsage.purposes");
	}
	return rtn;
}


#pragma mark --- general purpose X509 name generator ---

/* Attr/Value pairs, pick one of NUM_ATTR_STRINGS */
static char *attrStrings[] = {
	(char *)"thisName",
	(char *)"anotherName",
	(char *)"someOtherName"
};
#define NUM_ATTR_STRINGS	3

/* A/V type, pick one of NUM_ATTR_TYPES */
static CSSM_OID attrTypes[] = {
	CSSMOID_Surname,
	CSSMOID_CountryName,
	CSSMOID_OrganizationName,
	CSSMOID_Description
};
#define NUM_ATTR_TYPES	4

/* A/V tag, pick one of NUM_ATTR_TAGS */
static char attrTags[] = {
	BER_TAG_PRINTABLE_STRING,
	BER_TAG_IA5_STRING,
	BER_TAG_T61_STRING
};
#define NUM_ATTR_TAGS	3

void rdnCreate(
	CSSM_X509_RDN_PTR rdn)
{
	unsigned numPairs = genRand(1,4);
	rdn->numberOfPairs = numPairs;
	unsigned len = numPairs * sizeof(CSSM_X509_TYPE_VALUE_PAIR);
	rdn->AttributeTypeAndValue = 
		(CSSM_X509_TYPE_VALUE_PAIR_PTR)malloc(len);
	memset(rdn->AttributeTypeAndValue, 0, len);
	
	for(unsigned atvDex=0; atvDex<numPairs; atvDex++) {
		CSSM_X509_TYPE_VALUE_PAIR &pair = 
			rdn->AttributeTypeAndValue[atvDex];
		unsigned die = genRand(1, NUM_ATTR_TYPES);
		pair.type = attrTypes[die - 1];
		die = genRand(1, NUM_ATTR_STRINGS);
		char *str = attrStrings[die - 1];
		pair.value.Data = (uint8 *)str;
		pair.value.Length = strlen(str);
		die = genRand(1, NUM_ATTR_TAGS);
		pair.valueType = attrTags[die - 1];
	}
}

unsigned rdnCompare(
	CSSM_X509_RDN_PTR rdn1,
	CSSM_X509_RDN_PTR rdn2)
{
	if(rdn1->numberOfPairs != rdn2->numberOfPairs) {
		printf("***Mismatch in numberOfPairs\n");
		return 1;
	}
	unsigned rtn = 0;
	for(unsigned atvDex=0; atvDex<rdn1->numberOfPairs; atvDex++) {
		CSSM_X509_TYPE_VALUE_PAIR &p1 = 
			rdn1->AttributeTypeAndValue[atvDex];
		CSSM_X509_TYPE_VALUE_PAIR &p2 = 
			rdn2->AttributeTypeAndValue[atvDex];
		if(p1.valueType != p2.valueType) {
			printf("***valueType miscompare\n");
			rtn++;
		}
		if(compCssmData(p1.type, p2.type, "ATV.type")) {
			rtn++;
		}
		if(compCssmData(p1.value, p2.value, "ATV.value")) {
			rtn++;
		}
	}
	return rtn;
}

void rdnFree(
	CSSM_X509_RDN_PTR rdn)
{
	free(rdn->AttributeTypeAndValue);
}

void x509NameCreate(
	CSSM_X509_NAME_PTR x509Name)
{
	memset(x509Name, 0, sizeof(*x509Name));
	unsigned numRdns = genRand(1,4);
	x509Name->numberOfRDNs = numRdns;
	unsigned len = numRdns * sizeof(CSSM_X509_RDN);
	x509Name->RelativeDistinguishedName = (CSSM_X509_RDN_PTR)malloc(len);
	memset(x509Name->RelativeDistinguishedName, 0, len);
	
	for(unsigned rdnDex=0; rdnDex<numRdns; rdnDex++) {
		CSSM_X509_RDN &rdn = x509Name->RelativeDistinguishedName[rdnDex];
		rdnCreate(&rdn);
	}
}

unsigned x509NameCompare(
	const CSSM_X509_NAME_PTR n1,
	const CSSM_X509_NAME_PTR n2)
{
	if(n1->numberOfRDNs != n2->numberOfRDNs) {
		printf("***Mismatch in numberOfRDNs\n");
		return 1;
	}
	unsigned rtn = 0;
	for(unsigned rdnDex=0; rdnDex<n1->numberOfRDNs; rdnDex++) {
		CSSM_X509_RDN &rdn1 = n1->RelativeDistinguishedName[rdnDex];
		CSSM_X509_RDN &rdn2 = n2->RelativeDistinguishedName[rdnDex];
		rtn += rdnCompare(&rdn1, &rdn2);
	}
	return rtn;
}

void x509NameFree(
	CSSM_X509_NAME_PTR n)
{
	for(unsigned rdnDex=0; rdnDex<n->numberOfRDNs; rdnDex++) {
		CSSM_X509_RDN &rdn = n->RelativeDistinguishedName[rdnDex];
		rdnFree(&rdn);
	}
	free(n->RelativeDistinguishedName);
}

#pragma mark --- general purpose GeneralNames generator ---

#define SOME_URL_1	"http://foo.bar.com"
#define SOME_URL_2	"http://bar.foo.com"
#define SOME_DNS_1	"Some DNS"
#define SOME_DNS_2	"Another DNS"
unsigned char	someIpAdr_1[] = {208, 161, 124, 209 };
unsigned char	someIpAdr_2[] = {10, 0, 61, 5};

void genNamesCreate(void *arg)
{
	CE_GeneralNames *names = (CE_GeneralNames *)arg;
	names->numNames = genRand(1, 3);
	// one at a time
	//names->numNames = 1;
	names->generalName = (CE_GeneralName *)malloc(names->numNames * 
		sizeof(CE_GeneralName));
	memset(names->generalName, 0, names->numNames * sizeof(CE_GeneralName));
	const char *src;
	unsigned char *usrc;
	
	for(unsigned i=0; i<names->numNames; i++) {
		CE_GeneralName *name = &names->generalName[i];
		unsigned type = genRand(1, 5);
		// unsigned type = 5;
		switch(type) {
			case 1:
				name->nameType = GNT_URI;
				name->berEncoded = CSSM_FALSE;
				src = randBool() ? SOME_URL_1 : SOME_URL_2;
				appCopyData(src, strlen(src), &name->name);
				break;

			case 2:
				name->nameType = GNT_RegisteredID;
				name->berEncoded = CSSM_FALSE;
				appCopyData(CSSMOID_SubjectDirectoryAttributes.Data,
					CSSMOID_SubjectDirectoryAttributes.Length,
					&name->name);
				break;
				
			case 3:
				name->nameType = GNT_DNSName;
				name->berEncoded = CSSM_FALSE;
				src = randBool() ? SOME_DNS_1 : SOME_DNS_2;
				appCopyData(src, strlen(src), &name->name);
				break;
				
			case 4:
				name->nameType = GNT_IPAddress;
				name->berEncoded = CSSM_FALSE;
				usrc = randBool() ? someIpAdr_1 : someIpAdr_2;
				appCopyData(usrc, 4, &name->name);
				break;
				
			case 5:
			{
				/* X509_NAME, the hard one */
				name->nameType = GNT_DirectoryName;
				name->berEncoded = CSSM_FALSE;
				appSetupCssmData(&name->name, sizeof(CSSM_X509_NAME));
				x509NameCreate((CSSM_X509_NAME_PTR)name->name.Data);
			}
		}
	}
}

unsigned genNamesCompare(const void *pre, const void *post)
{
	const CE_GeneralNames *gnPre = (CE_GeneralNames *)pre;
	const CE_GeneralNames *gnPost = (CE_GeneralNames *)post;
	unsigned rtn = 0;
	
	if((gnPre == NULL) || (gnPost == NULL)) {
		printf("***Bad GenNames pointer\n");
		return 1;
	}
	if(gnPre->numNames != gnPost->numNames) {
		printf("***CE_GeneralNames.numNames miscompare\n");
		return 1;
	}
	for(unsigned dex=0; dex<gnPre->numNames; dex++) {
		CE_GeneralName *npre  = &gnPre->generalName[dex];
		CE_GeneralName *npost = &gnPost->generalName[dex];
		if(npre->nameType != npost->nameType) {
			printf("***CE_GeneralName.nameType miscompare\n");
			rtn++;
		}
		if(compBool(npre->berEncoded, npost->berEncoded, 
				"CE_GeneralName.berEncoded")) {
			rtn++;
		}
		
		/* nameType-specific compare */
		switch(npre->nameType) {
			case GNT_RFC822Name:
				rtn += compCssmData(npre->name, npost->name,
					"CE_GeneralName.RFC822Name");
				break;
			case GNT_DNSName:
				rtn += compCssmData(npre->name, npost->name,
					"CE_GeneralName.DNSName");
				break;
			case GNT_URI:
				rtn += compCssmData(npre->name, npost->name,
					"CE_GeneralName.URI");
				break;
			case GNT_IPAddress:
				rtn += compCssmData(npre->name, npost->name,
					"CE_GeneralName.RFIPAddressC822Name");
				break;
			case GNT_RegisteredID:
				rtn += compCssmData(npre->name, npost->name,
					"CE_GeneralName.RegisteredID");
				break;
			case GNT_DirectoryName:
				rtn += x509NameCompare((CSSM_X509_NAME_PTR)npre->name.Data,
					(CSSM_X509_NAME_PTR)npost->name.Data);
				break;
			default:
				printf("****BRRZAP! genNamesCompare needs work\n");
				rtn++;
		}
	}
	return rtn;
}

void genNamesFree(void *arg)
{
	const CE_GeneralNames *gn = (CE_GeneralNames *)arg;
	for(unsigned dex=0; dex<gn->numNames; dex++) {
		CE_GeneralName *n = (CE_GeneralName *)&gn->generalName[dex];
		switch(n->nameType) {
			case GNT_DirectoryName:
				x509NameFree((CSSM_X509_NAME_PTR)n->name.Data);
				CSSM_FREE(n->name.Data);
				break;
			default:
				CSSM_FREE(n->name.Data);
				break;
		}
	}
	free(gn->generalName);
}

#pragma mark --- CE_CRLDistPointsSyntax ---
void cdpCreate(void *arg)
{
	CE_CRLDistPointsSyntax *cdp = (CE_CRLDistPointsSyntax *)arg;
	//cdp->numDistPoints = genRand(1,3);
	// one at a time
	cdp->numDistPoints = 1;
	unsigned len = sizeof(CE_CRLDistributionPoint) * cdp->numDistPoints;
	cdp->distPoints = (CE_CRLDistributionPoint *)malloc(len);
	memset(cdp->distPoints, 0, len);
	
	for(unsigned dex=0; dex<cdp->numDistPoints; dex++) {
		CE_CRLDistributionPoint *pt = &cdp->distPoints[dex];
		
		/* all fields optional */
		if(randBool()) {
			CE_DistributionPointName *dpn = pt->distPointName =
				(CE_DistributionPointName *)malloc(
					sizeof(CE_DistributionPointName));
			memset(dpn, 0, sizeof(CE_DistributionPointName));
			
			/* CE_DistributionPointName has two flavors */
			if(randBool()) {
				dpn->nameType = CE_CDNT_FullName;
				dpn->dpn.fullName = (CE_GeneralNames *)malloc(
					sizeof(CE_GeneralNames));
				memset(dpn->dpn.fullName, 0, sizeof(CE_GeneralNames));
				genNamesCreate(dpn->dpn.fullName);
			}
			else {
				dpn->nameType = CE_CDNT_NameRelativeToCrlIssuer;
				dpn->dpn.rdn = (CSSM_X509_RDN_PTR)malloc(
					sizeof(CSSM_X509_RDN));
				memset(dpn->dpn.rdn, 0, sizeof(CSSM_X509_RDN));
				rdnCreate(dpn->dpn.rdn);
			}
		}	/* creating CE_DistributionPointName */
		
		pt->reasonsPresent = randBool();
		if(pt->reasonsPresent) {
			CE_CrlDistReasonFlags *cdr = &pt->reasons;
			/* set two random valid bits */
			*cdr = 0;
			*cdr |= 1 << genRand(0,7);
			*cdr |= 1 << genRand(0,7);
		}
		
		/* make sure at least one present */
		if((!pt->distPointName && !pt->reasonsPresent) || randBool()) {
			pt->crlIssuer = (CE_GeneralNames *)malloc(sizeof(CE_GeneralNames));
			memset(pt->crlIssuer, 0, sizeof(CE_GeneralNames));
			genNamesCreate(pt->crlIssuer);
		}
	}
}

unsigned cdpCompare(const void *pre, const void *post)
{
	CE_CRLDistPointsSyntax *cpre = (CE_CRLDistPointsSyntax *)pre;
	CE_CRLDistPointsSyntax *cpost = (CE_CRLDistPointsSyntax *)post;
	
	if(cpre->numDistPoints != cpost->numDistPoints) {
		printf("***CE_CRLDistPointsSyntax.numDistPoints miscompare\n");
		return 1;
	}
	unsigned rtn = 0;
	for(unsigned dex=0; dex<cpre->numDistPoints; dex++) {
		CE_CRLDistributionPoint *ptpre  = &cpre->distPoints[dex];
		CE_CRLDistributionPoint *ptpost = &cpost->distPoints[dex];
		
		if(ptpre->distPointName) {
			if(ptpost->distPointName == NULL) {
				printf("***NULL distPointName post decode\n");
				rtn++;
				goto checkReason;
			}
			CE_DistributionPointName *dpnpre = ptpre->distPointName;
			CE_DistributionPointName *dpnpost = ptpost->distPointName;
			if(dpnpre->nameType != dpnpost->nameType) {
				printf("***CE_DistributionPointName.nameType miscompare\n");
				rtn++;
				goto checkReason;
			}
			if(dpnpre->nameType == CE_CDNT_FullName) {
				rtn += genNamesCompare(dpnpre->dpn.fullName, dpnpost->dpn.fullName);
			}
			else {
				rtn += rdnCompare(dpnpre->dpn.rdn, dpnpost->dpn.rdn);
			}
				
		}
		else if(ptpost->distPointName != NULL) {
			printf("***NON NULL distPointName post decode\n");
			rtn++;
		}
		
	checkReason:
		if(ptpre->reasons != ptpost->reasons) {
			printf("***CE_CRLDistributionPoint.reasons miscompare\n");
			rtn++;
		}
		
		if(ptpre->crlIssuer) {
			if(ptpost->crlIssuer == NULL) {
				printf("***NULL crlIssuer post decode\n");
				rtn++;
				continue;
			}
			CE_GeneralNames *gnpre = ptpre->crlIssuer;
			CE_GeneralNames *gnpost = ptpost->crlIssuer;
			rtn += genNamesCompare(gnpre, gnpost);
		}
		else if(ptpost->crlIssuer != NULL) {
			printf("***NON NULL crlIssuer post decode\n");
			rtn++;
		}
	}
	return rtn;
}

void cdpFree(void *arg)
{
	CE_CRLDistPointsSyntax *cdp = (CE_CRLDistPointsSyntax *)arg;
	for(unsigned dex=0; dex<cdp->numDistPoints; dex++) {
		CE_CRLDistributionPoint *pt = &cdp->distPoints[dex];
		if(pt->distPointName) {
			CE_DistributionPointName *dpn = pt->distPointName;
			if(dpn->nameType == CE_CDNT_FullName) {
				genNamesFree(dpn->dpn.fullName);
				free(dpn->dpn.fullName);
			}
			else {
				rdnFree(dpn->dpn.rdn);
				free(dpn->dpn.rdn);
			}
			free(dpn);
		}
		
		if(pt->crlIssuer) {
			genNamesFree(pt->crlIssuer);
			free(pt->crlIssuer);
		}
	}	
	free(cdp->distPoints);
}

#pragma mark --- CE_AuthorityKeyID ---
void authKeyIdCreate(void *arg)
{
	CE_AuthorityKeyID *akid = (CE_AuthorityKeyID *)arg;
	
	/* all three fields optional */
	
	akid->keyIdentifierPresent = randBool();
	if(akid->keyIdentifierPresent) {
		randData(&akid->keyIdentifier, 16);
	}
	
	akid->generalNamesPresent = randBool();
	if(akid->generalNamesPresent) {
		akid->generalNames = 
			(CE_GeneralNames *)malloc(sizeof(CE_GeneralNames));
		memset(akid->generalNames, 0, sizeof(CE_GeneralNames));
		genNamesCreate(akid->generalNames);
	}
	
	if(!akid->keyIdentifierPresent & !akid->generalNamesPresent) {
		/* force at least one to be present */
		akid->serialNumberPresent = CSSM_TRUE;
	}
	else  {
		akid->serialNumberPresent = randBool();
	}
	if(akid->serialNumberPresent) {
		randData(&akid->serialNumber, 16);
	}

}

unsigned authKeyIdCompare(const void *pre, const void *post)
{
	CE_AuthorityKeyID *akpre = (CE_AuthorityKeyID *)pre;
	CE_AuthorityKeyID *akpost = (CE_AuthorityKeyID *)post;
	unsigned rtn = 0;
	
	if(compBool(akpre->keyIdentifierPresent, akpost->keyIdentifierPresent,
			"CE_AuthorityKeyID.keyIdentifierPresent")) {
		rtn++;
	}
	else if(akpre->keyIdentifierPresent) {
		rtn += compCssmData(akpre->keyIdentifier,
			akpost->keyIdentifier, "CE_AuthorityKeyID.keyIdentifier");
	}
	
	if(compBool(akpre->generalNamesPresent, akpost->generalNamesPresent,
			"CE_AuthorityKeyID.generalNamesPresent")) {
		rtn++;
	}
	else if(akpre->generalNamesPresent) {
		rtn += genNamesCompare(akpre->generalNames,
			akpost->generalNames);
	}

	if(compBool(akpre->serialNumberPresent, akpost->serialNumberPresent,
			"CE_AuthorityKeyID.serialNumberPresent")) {
		rtn++;
	}
	else if(akpre->serialNumberPresent) {
		rtn += compCssmData(akpre->serialNumber,
			akpost->serialNumber, "CE_AuthorityKeyID.serialNumber");
	}
	return rtn;
}

void authKeyIdFree(void *arg)
{
	CE_AuthorityKeyID *akid = (CE_AuthorityKeyID *)arg;

	if(akid->keyIdentifier.Data) {
		free(akid->keyIdentifier.Data);
	}
	if(akid->generalNames) {
		genNamesFree(akid->generalNames);		// genNamesCreate mallocd
		free(akid->generalNames);				// we mallocd
	}
	if(akid->serialNumber.Data) {
		free(akid->serialNumber.Data);
	}
}

#pragma mark --- CE_CertPolicies ---

/* random OIDs, pick 1..NUM_CP_OIDS */
static CSSM_OID cpOids[] = 
{
	CSSMOID_EmailAddress,
	CSSMOID_UnstructuredName,
	CSSMOID_ContentType,
	CSSMOID_MessageDigest
};
#define NUM_CP_OIDS 4

/* CPS strings, pick one of NUM_CPS_STR */
static char *someCPSs[] = 
{
	(char *)"http://www.apple.com",
	(char *)"https://cdnow.com",
	(char *)"ftp:backwards.com"
};
#define NUM_CPS_STR		3

/* make these looks like real sequences */
static uint8 someUnotice[] = {0x30, 0x03, BER_TAG_BOOLEAN, 1, 0xff};
static uint8 someOtherData[] = {0x30, 0x02, BER_TAG_NULL, 0};

void cpCreate(void *arg)
{
	CE_CertPolicies *cp = (CE_CertPolicies *)arg;
	cp->numPolicies = genRand(1,3);
	//cp->numPolicies = 1;
	unsigned len = sizeof(CE_PolicyInformation) * cp->numPolicies;
	cp->policies = (CE_PolicyInformation *)malloc(len);
	memset(cp->policies, 0, len);
	
	for(unsigned polDex=0; polDex<cp->numPolicies; polDex++) {
		CE_PolicyInformation *pi = &cp->policies[polDex];
		unsigned die = genRand(1, NUM_CP_OIDS);
		pi->certPolicyId = cpOids[die - 1];
		unsigned numQual = genRand(1,3);
		pi->numPolicyQualifiers = numQual;
		len = sizeof(CE_PolicyQualifierInfo) * numQual;
		pi->policyQualifiers = (CE_PolicyQualifierInfo *)
			malloc(len);
		memset(pi->policyQualifiers, 0, len);
		for(unsigned cpiDex=0; cpiDex<numQual; cpiDex++) {
			CE_PolicyQualifierInfo *qi = 
				&pi->policyQualifiers[cpiDex];
			if(randBool()) {
				qi->policyQualifierId = CSSMOID_QT_CPS;
				die = genRand(1, NUM_CPS_STR);
				qi->qualifier.Data = (uint8 *)someCPSs[die - 1];
				qi->qualifier.Length = strlen((char *)qi->qualifier.Data);
			}
			else {
				qi->policyQualifierId = CSSMOID_QT_UNOTICE;
				if(randBool()) {
					qi->qualifier.Data = someUnotice;
					qi->qualifier.Length = 5;
				}
				else {
					qi->qualifier.Data = someOtherData;
					qi->qualifier.Length = 4;
				}
			}
		}
	}
}

unsigned cpCompare(const void *pre, const void *post)
{
	CE_CertPolicies *cppre  = (CE_CertPolicies *)pre;
	CE_CertPolicies *cppost = (CE_CertPolicies *)post;
	
	if(cppre->numPolicies != cppost->numPolicies) {
		printf("CE_CertPolicies.numPolicies mismatch\n");
		return 1;
	}
	unsigned rtn = 0;
	for(unsigned polDex=0; polDex<cppre->numPolicies; polDex++) {
		CE_PolicyInformation *pipre  = &cppre->policies[polDex];
		CE_PolicyInformation *pipost = &cppost->policies[polDex];
		rtn += compCssmData(pipre->certPolicyId, pipost->certPolicyId,
			"CE_PolicyInformation.certPolicyId");
		if(pipre->numPolicyQualifiers != pipost->numPolicyQualifiers) {
			printf("CE_PolicyInformation.CE_PolicyInformation mismatch\n");
			rtn++;
			continue;
		}
		
		for(unsigned qiDex=0; qiDex<pipre->numPolicyQualifiers; qiDex++) {
			CE_PolicyQualifierInfo *qipre  = &pipre->policyQualifiers[qiDex];
			CE_PolicyQualifierInfo *qipost = &pipost->policyQualifiers[qiDex];
			rtn += compCssmData(qipre->policyQualifierId,
				qipost->policyQualifierId,
				"CE_PolicyQualifierInfo.policyQualifierId");
			rtn += compCssmData(qipre->qualifier,
				qipost->qualifier,
				"CE_PolicyQualifierInfo.qualifier");
		}
	}
	return rtn;
}

void cpFree(void *arg)
{
	CE_CertPolicies *cp = (CE_CertPolicies *)arg;
	for(unsigned polDex=0; polDex<cp->numPolicies; polDex++) {
		CE_PolicyInformation *pi = &cp->policies[polDex];
		free(pi->policyQualifiers);
	}
	free(cp->policies);
}
