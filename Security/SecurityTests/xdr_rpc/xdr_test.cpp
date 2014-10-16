/* 
 * g++ -F/usr/local/SecurityPieces/Frameworks -g -Wall path/to/xdr_test.cpp 
 * -framework [securityd_client, security_cdsa_utilities, security_utilities]
 *
 * -W triggers a lot of warnings in Security code....
 */
#include <stdio.h>
#include <rpc/types.h>	/* bool_t */
#include <stdlib.h>		/* exit(3), malloc(3) */
#include <string.h>		/* memcmp(3), memset(3), strdup(3) */
#include <fcntl.h>		/* open(2) */
#include <errno.h>		/* errno */
#include <sys/types.h>	/* read() */
#include <sys/uio.h>	/* read() */
#include <unistd.h>		/* read(), getopt(3) */
#include <Security/cssmtype.h>
#include <security_cdsa_utilities/walkers.h>
#include <security_cdsa_utilities/context.h>
#include <security_cdsa_utilities/cssmaclpod.h>
#include <securityd_client/xdr_cssm.h>
#include "securityd_data_saver.h"

using Security::DataWalkers::Copier;

const char *testString = "FOOBAR";

/*
 * securityd is extremely sloppy about zeroing out data fields it doesn't
 * use.  It's unlikely that sloppiness will be cleaned up in one fell
 * swoop, so we'll never be able to set this to 0; instead, search on the 
 * symbol name and selectively unbracket code as securityd is fixed.  
 */
#define SECURITYD_SENDS_GARBAGE	1


/* 
 * The securityd RPC protocol can't handle 64-bit quantities until both
 * securityd itself and our XDR implementation do.  Ergo, don't be tempted 
 * to make ALIGNMENT depend on __LP64__.  
 *
 * ALIGNMENT and ALIGNUP borrowed from libsecurityd, sec_xdr.c.  They
 * really should be defined once in a more central location, like
 * libsecurity_utilities.  
 */
#define LP64_FULLY_SUPPORTED	0

#if LP64_FULLY_SUPPORTED
#define ALIGNMENT 8
#else
#define ALIGNMENT 4
#endif	// LP64_FULLY_SUPPORTED

#define ALIGNUP(LEN)		(((LEN - 1) & ~(ALIGNMENT - 1)) + ALIGNMENT)
#define ALIGNSIZE(TYPE)		ALIGNUP(sizeof(TYPE))

#define NULLCHECK(a, b, func) \
	if (!(a) && !(b))\
		return OK;\
	else if (!(a) && (b) || (a) && !(b))\
	{\
		fprintf(stderr, "%s (NULL and non-NULL parameter)\n", (func));\
		return MISMATCH;\
	}

#define N_ITERS			3		/* # of encode/decode cycles */

#define OK				  0
#define XDR_ENCODE_ERROR -1
#define XDR_DECODE_ERROR -2
#define MISMATCH		 -3
#define UNKNOWN_TYPE	 -4		/* invalid union discriminator */
#define MEM_ERROR		-10
#define BAD_PARAM		-11
#define BAD_FILE		-12
#define BAD_READ		-13
#define READ_EOF		-14		/* not an error per se */
#define INCOMPATIBLE	-98		/* input data format not recognized */
#define NOT_IMPLEMENTED	-99

/* This isn't publicly exposed */
#ifdef __cplusplus
extern "C" {
extern unsigned long xdr_sizeof(xdrproc_t, void *);
}
#endif

/*
 * CSSM data structures of interest.  Chosen either for their complexity 
 * (e.g., CSSM_CONTEXT) or their ubiquity (CSSM_DATA).  
 *
 * CSSM_DATA														
 * CSSM_GUID
 * CSSM_CRYPTO_DATA (possible callback)								
 * CSSM_LIST_ELEMENT (union)										
 * CSSM_LIST (pointers)												
 * CSSM_SAMPLEGROUP (array of CSSM_SAMPLEs)							
 * CSSM_DATE (fixed-size arrays handled via xdr_vector())			
 * CSSM_KEY (heavily used)											
 * CSSM_DB_RECORD_ATTRIBUTE_DATA (exercises a lot of the above)
 * CSSM_CONTEXT (exercises a lot of the above)						
 * AccessCredentials
 * CSSM_AUTHORIZATIONGROUP
 * CSSM_ACL_VALIDITY_PERIOD
 * CSSM_ACL_ENTRY_PROTOTYPE
 * CSSM_ACL_OWNER_PROTOTYPE
 * CSSM_ACL_ENTRY_INPUT
 * CSSM_ACL_ENTRY_INFO
 * CSSM_QUERY
 */

/* utility routines */
bool_t xdr_stdio(void *data, xdrproc_t proc, enum xdr_op op);	/* XXX/gh  needed? */
bool_t xdr_mem_encode(void *input, void **output, u_int *outlen, 
					  xdrproc_t proc);
bool_t xdr_mem_decode(void *input, void *output, u_int bytesNeeded, 
					  xdrproc_t proc);
void flip(uint8_t *addr, size_t size);
int readPreamble(int fd, uint32_t *doflip, uint32_t *vers, uint32_t *type);
int fill_CSSM_CONTEXT_ATTRIBUTE(CSSM_CONTEXT_ATTRIBUTE *attr,
								CSSM_ATTRIBUTE_TYPE type,
								uint32_t attrlen,
								void *attrval);

/* byte reordering routines */
void hostorder_CSSM_DATA(CSSM_DATA *data, off_t offset);
void hostorder_CSSM_GUID(CSSM_GUID *guid);
void hostorder_CSSM_VERSION(CSSM_VERSION *version);
void hostorder_CSSM_SUBSERVICE_UID(CSSM_SUBSERVICE_UID *ssuid);
void hostorder_CSSM_CRYPTO_DATA(CSSM_CRYPTO_DATA *crypto, off_t offset);
void hostorder_CSSM_LIST(CSSM_LIST *list, off_t offset);
void hostorder_CSSM_LIST_ELEMENT(CSSM_LIST_ELEMENT *element, off_t offset);
void hostorder_CSSM_SAMPLE(CSSM_SAMPLE *sample, off_t offset);
void hostorder_CSSM_SAMPLEGROUP(CSSM_SAMPLEGROUP *sgrp, off_t offset);
void hostorder_CSSM_ENCODED_CERT(CSSM_ENCODED_CERT *cert, off_t offset);
void hostorder_CSSM_CERTGROUP(CSSM_CERTGROUP *grp, off_t offset);
void hostorder_CSSM_BASE_CERTS(CSSM_BASE_CERTS *certs, off_t offset);
void hostorder_CSSM_ACCESS_CREDENTIALS(CSSM_ACCESS_CREDENTIALS *creds,
									   off_t offset);
void hostorder_CSSM_AUTHORIZATIONGROUP(CSSM_AUTHORIZATIONGROUP *grp,
									   off_t offset);
void hostorder_CSSM_ACL_VALIDITY_PERIOD(CSSM_ACL_VALIDITY_PERIOD *period,
										off_t offset);
void hostorder_CSSM_ACL_ENTRY_PROTOTYPE(CSSM_ACL_ENTRY_PROTOTYPE *proto, 
										off_t offset);
void hostorder_CSSM_ACL_OWNER_PROTOTYPE(CSSM_ACL_OWNER_PROTOTYPE *proto, 
										off_t offset);
void hostorder_CSSM_ACL_ENTRY_INPUT(CSSM_ACL_ENTRY_INPUT *input, off_t offset);
void hostorder_CSSM_ACL_ENTRY_INFO(CSSM_ACL_ENTRY_INFO *info, off_t offset);
void hostorder_CSSM_KEYHEADER(CSSM_KEYHEADER *key);
void hostorder_CSSM_KEY(CSSM_KEY *key, off_t offset);
void hostorder_CSSM_DL_DB_HANDLE(CSSM_DL_DB_HANDLE *handle);
void hostorder_CSSM_RANGE(CSSM_RANGE *range);
void hostorder_CSSM_CONTEXT_ATTRIBUTE(CSSM_CONTEXT_ATTRIBUTE *attr, 
									  off_t offset);
void hostorder_CSSM_CONTEXT(CSSM_CONTEXT *ctx, CSSM_CONTEXT_ATTRIBUTE *attrs);
void hostorder_CSSM_OID(CSSM_OID *oid, off_t offset);
void hostorder_CSSM_DB_ATTRIBUTE_INFO(CSSM_DB_ATTRIBUTE_INFO *attrinfo, 
									  off_t offset);
void hostorder_CSSM_DB_ATTRIBUTE_DATA(CSSM_DB_ATTRIBUTE_DATA *attrdata, 
									  off_t offset);
void hostorder_CSSM_SELECTION_PREDICATE(CSSM_SELECTION_PREDICATE *pred, 
										off_t offset);
void hostorder_CSSM_QUERY_LIMITS(CSSM_QUERY_LIMITS *limits);
void hostorder_CSSM_QUERY(CSSM_QUERY *range, off_t offset);

/* comparators */
int compare_CSSM_DATA(CSSM_DATA *data1, CSSM_DATA *data2);
int compare_CSSM_SUBSERVICE_UID(const CSSM_SUBSERVICE_UID *ssuid1, 
								const CSSM_SUBSERVICE_UID *ssuid2);
int compare_CSSM_CRYPTO_DATA(CSSM_CRYPTO_DATA *data1, CSSM_CRYPTO_DATA *data2);
int compare_CSSM_LIST(const CSSM_LIST *list1, const CSSM_LIST *list2);
int compare_CSSM_SAMPLE(const CSSM_SAMPLE *sample1, const CSSM_SAMPLE *sample2);
int compare_CSSM_SAMPLEGROUP(CSSM_SAMPLEGROUP *sgrp1, CSSM_SAMPLEGROUP *sgrp2);
int compare_CSSM_ENCODED_CERT(CSSM_ENCODED_CERT *cert1, 
							  CSSM_ENCODED_CERT *cert2);
int compare_CSSM_CERTGROUP(CSSM_CERTGROUP *grp1, CSSM_CERTGROUP *grp2);
int compare_CSSM_BASE_CERTS(CSSM_BASE_CERTS *bases1, CSSM_BASE_CERTS *bases2);
int compare_CSSM_ACCESS_CREDENTIALS(CSSM_ACCESS_CREDENTIALS *creds1,
									CSSM_ACCESS_CREDENTIALS *creds2);
int compare_CSSM_AUTHORIZATIONGROUP(CSSM_AUTHORIZATIONGROUP *grp1,
									CSSM_AUTHORIZATIONGROUP *grp2);
int compare_CSSM_ACL_VALIDITY_PERIOD(CSSM_ACL_VALIDITY_PERIOD *period1,
									 CSSM_ACL_VALIDITY_PERIOD *period2);
int compare_CSSM_ACL_ENTRY_PROTOTYPE(CSSM_ACL_ENTRY_PROTOTYPE *proto1,
									 CSSM_ACL_ENTRY_PROTOTYPE *proto2,
									 int skipGarbage);
int compare_CSSM_ACL_OWNER_PROTOTYPE(CSSM_ACL_OWNER_PROTOTYPE *proto1,
									 CSSM_ACL_OWNER_PROTOTYPE *proto2);
int compare_CSSM_ACL_ENTRY_INPUT(CSSM_ACL_ENTRY_INPUT *input1, 
								 CSSM_ACL_ENTRY_INPUT *input2);
int compare_CSSM_ACL_ENTRY_INFO(CSSM_ACL_ENTRY_INFO *info1,
								CSSM_ACL_ENTRY_INFO *info2);
int compare_CSSM_DATE(CSSM_DATE *date1, CSSM_DATE *date2);
int compare_CSSM_KEYHEADER(CSSM_KEYHEADER *hdr1, CSSM_KEYHEADER *hdr2);
int compare_CSSM_KEY(CSSM_KEY *key1, CSSM_KEY *key2);
int compare_CSSM_RANGE(CSSM_RANGE *range1, CSSM_RANGE *range2);
int compare_CSSM_CONTEXT_ATTRIBUTE(CSSM_CONTEXT_ATTRIBUTE *attr1,
								   CSSM_CONTEXT_ATTRIBUTE *attr2);
int compare_CSSM_OID(CSSM_OID *oid1, CSSM_OID *oid2);
int compare_CSSM_CONTEXT(CSSM_CONTEXT *ctx1, CSSM_CONTEXT *ctx2);
int compare_CSSM_DB_ATTRIBUTE_INFO(CSSM_DB_ATTRIBUTE_INFO *attrinfo1, 
								   CSSM_DB_ATTRIBUTE_INFO *attrinfo2);
int compare_CSSM_DB_ATTRIBUTE_DATA(CSSM_DB_ATTRIBUTE_DATA *attrdata1, 
								   CSSM_DB_ATTRIBUTE_DATA *attrdata2);
int compare_CSSM_SELECTION_PREDICATE(CSSM_SELECTION_PREDICATE *pred1, 
									 CSSM_SELECTION_PREDICATE *pred2);
int compare_CSSM_QUERY_LIMITS(CSSM_QUERY_LIMITS *limits1, 
							  CSSM_QUERY_LIMITS *limits2);
int compare_CSSM_QUERY(CSSM_QUERY *query1, CSSM_QUERY *query2);

CSSM_RETURN dummyCSSMCallback(CSSM_DATA *data, void *context);
CSSM_RETURN dummyACLSubjectCallback(const CSSM_LIST *subjectRequest,
									void *callerContext,
									const CSSM_MEMORY_FUNCS *MemFuncs);

/* the actual test functions */
int test_CSSM_DB_RECORD_ATTRIBUTE_DATA(const char *srcfile);	/* TODO/gh */
int test_xdrwalk_CSSM_CONTEXT(CSSM_CONTEXT *ctx, int dbglvl);
int test_CSSM_CONTEXT(int fd, int doflip, int dbglvl);
int test_xdrwalk_CSSM_ACL_OWNER_PROTOTYPE(CSSM_ACL_OWNER_PROTOTYPE *aclOwnerPrototype, int dbglvl);
int test_CSSM_ACL_OWNER_PROTOTYPE(int fd, int doflip, int dbglvl);
int test_xdrwalk_CSSM_ACL_ENTRY_INPUT(CSSM_ACL_ENTRY_INPUT *aclEntryInput, 
									  int dbglvl);
int test_CSSM_ACL_ENTRY_INPUT(int fd, int doflip, int dbglvl);
int test_xdrwalk_CSSM_ACL_ENTRY_INFO(CSSM_ACL_ENTRY_INFO *aclEntryInfo,
									 int dbglvl);
int test_CSSM_ACL_ENTRY_INFO(int fd, int doflip, int dbglvl);
int test_xdrwalk_CSSM_QUERY(CSSM_QUERY *query, int dbglvl);
int test_CSSM_QUERY(int fd, int doflip, int dbglvl);


/**************************************************************************
 * misc utility functions
 **************************************************************************/

/* XXX/gh  needed? */
/* XXX/gh  should "data" be a uint8_t *? */
bool_t xdr_stdio(void *data, xdrproc_t proc, enum xdr_op op)
{
	XDR xdr;
	/* should we call xdrstdio_create(...,..., XDR_FREE) when done? */
	xdrstdio_create(&xdr, stdout, op);
	if (proc(&xdr, data))
		return (FALSE);
	return (TRUE);
}

/* note no error-checking of parameters */
bool_t xdr_mem_encode(void *input, void **output, u_int *outlen, 
					  xdrproc_t proc)
{
	XDR xdr;
	char *data;
	u_int length;

	length = xdr_sizeof(proc, input);
	if ((data = (char *)malloc(length)) == NULL)
	{
		fprintf(stderr, "xdr_mem_encode(): malloc() error\n");
		return (FALSE);
	}
	xdrmem_create(&xdr, data, length, XDR_ENCODE);
	if (!proc(&xdr, input))
	{
		fprintf(stderr, "xdr_mem_encode(): XDR error\n");
		free(data);
		return (FALSE);
	}
	*output = data;
	if (outlen)
		*outlen = length;
	return (TRUE);
}

/* note no error-checking of parameters */
bool_t xdr_mem_decode(void *input, void *output, u_int bytesNeeded, 
					  xdrproc_t proc)
{
	XDR xdr;

	xdrmem_create(&xdr, (char *)input, bytesNeeded, XDR_DECODE);
	if (!proc(&xdr, output))
		return (FALSE);
	return (TRUE);
}

/* 
 * Because sometimes ntoh*() isn't enough.  Stolen from securityd and 
 * slightly modified to avoid type dependencies. 
 */
void flip(void *inaddr, size_t size)
{
	uint8 *addr = reinterpret_cast<uint8 *>(inaddr);
	size_t n;

	assert(size > 1 && (size % 2 == 0));
	uint8_t *word = addr;
	for (n = 0; n < size/2; n++) 
	{
		uint8_t b = word[n];
		word[n] = word[size-1-n];
		word[size-1-n] = b;
	}
}

/* 
 * note that if this returns prematurely, you can make no assumption about
 * the value of "doflip," "vers," or "type"
 */
int readPreamble(int fd, uint32_t *doflip, uint32_t *vers, uint32_t *type)
{
	const char *func = "readPreamble()";
	uint32_t value;
	ssize_t bytesRead;

	/* byte order sentry value */
	if ((bytesRead = read(fd, &value, sizeof(value))) != sizeof(value))
	{
		if (bytesRead == 0)
			return READ_EOF;
		fprintf(stderr, "%s: error reading byte order sentry\n", func);
		return BAD_READ;
	}
	if (value == SecuritydDataSave::sentry)
		*doflip = 0;
	else if (value == 0x34120000)
		*doflip = 1;
	else
	{
		fprintf(stderr, "%s: unrecognized sentry value %d\n", func, value);
		return INCOMPATIBLE;
	}

	/* version info (for this disk-saving protocol) */
	if (read(fd, &value, sizeof(value)) != sizeof(value))
	{
		fprintf(stderr, "%s: error reading data format version\n", func);
		return BAD_READ;
	}
	if (*doflip)
		flip(&value, sizeof(value));
	*vers = value;

	switch(*vers)
	{
		case 1:
			/* type of record */
			if (read(fd, &value, sizeof(value)) != sizeof(value))
			{
				fprintf(stderr, "%s: error reading data type\n", func);
				return BAD_READ;
			}
			if (*doflip)
				flip(&value, sizeof(value));
			*type = value;
			break;
		default:
			fprintf(stderr, "%s: incompatible version (expected <= %d, got %d)\n", 
					func, SecuritydDataSave::version, *vers);
			return INCOMPATIBLE;
			break;
	}
	return OK;
}

int fill_CSSM_CONTEXT_ATTRIBUTE(CSSM_CONTEXT_ATTRIBUTE *attr,
								CSSM_ATTRIBUTE_TYPE type,
								uint32_t attrlen,
								void *attrval)
{
	if (!attr || !attrval)
		return BAD_PARAM;
	attr->AttributeType = type;
	attr->AttributeLength = attrlen;
	/* XXX  copy instead of assigning */
	switch (type & CSSM_ATTRIBUTE_TYPE_MASK)
	{
		case CSSM_ATTRIBUTE_DATA_UINT32:
			attr->Attribute.Uint32 = *(reinterpret_cast<uint32_t *>(attrval));
			break;
		case CSSM_ATTRIBUTE_DATA_CSSM_DATA:
			attr->Attribute.Data = (CSSM_DATA_PTR)attrval;
			break;
		case CSSM_ATTRIBUTE_DATA_CRYPTO_DATA:
			attr->Attribute.CryptoData = (CSSM_CRYPTO_DATA_PTR)attrval;
			break;
		case CSSM_ATTRIBUTE_DATA_KEY:
			attr->Attribute.Key = (CSSM_KEY_PTR)attrval;
			break;
		case CSSM_ATTRIBUTE_DATA_STRING:
			attr->Attribute.String = (char *)attrval;
			break;
		case CSSM_ATTRIBUTE_DATA_DATE:
			attr->Attribute.Date = (CSSM_DATE_PTR)attrval;
			break;
		case CSSM_ATTRIBUTE_DATA_RANGE:
			attr->Attribute.Range = (CSSM_RANGE_PTR)attrval;
			break;
		case CSSM_ATTRIBUTE_DATA_ACCESS_CREDENTIALS:
			attr->Attribute.AccessCredentials = (CSSM_ACCESS_CREDENTIALS_PTR)attrval;
			break;
		case CSSM_ATTRIBUTE_DATA_VERSION:
			attr->Attribute.Version = (CSSM_VERSION_PTR)attrval;
			break;
		case CSSM_ATTRIBUTE_DATA_DL_DB_HANDLE:
			attr->Attribute.DLDBHandle = (CSSM_DL_DB_HANDLE_PTR)attrval;
			break;
		/* _KR_PROFILE not supported? */
		default:
			return BAD_PARAM;
	}
	return OK;
}


/**************************************************************************
 * These do their best to handle byte-ordering issues.  
 *
 * Note that pointers are generally (maybe always) byte-order swapped in
 * these routines, since you generally need to do some kind of arithmetic
 * with them (relocation).  
 **************************************************************************/

void hostorder_CSSM_DATA(CSSM_DATA *data, off_t offset)
{
	intptr_t newaddr;		/* for readability */

	if (!data) return;
	flip(&data->Length, sizeof(data->Length));
	if (data->Data)
	{
		flip(&data->Data, sizeof(data->Data));
		newaddr = reinterpret_cast<intptr_t>(data->Data) + offset;
		data->Data = reinterpret_cast<uint8 *>(newaddr);
	}
}

void hostorder_CSSM_GUID(CSSM_GUID *guid)
{
	if (!guid) return;
	flip(&guid->Data1, sizeof(guid->Data1));
	flip(&guid->Data2, sizeof(guid->Data2));
	flip(&guid->Data3, sizeof(guid->Data3));
}

void hostorder_CSSM_VERSION(CSSM_VERSION *version)
{
	if (!version) return;
	flip(&version->Major, sizeof(version->Major));
	flip(&version->Minor, sizeof(version->Minor));
}

void hostorder_CSSM_SUBSERVICE_UID(CSSM_SUBSERVICE_UID *ssuid)
{
	if (!ssuid) return;
	hostorder_CSSM_GUID(&ssuid->Guid);
	hostorder_CSSM_VERSION(&ssuid->Version);
	flip(&ssuid->SubserviceId, sizeof(ssuid->SubserviceId));
	flip(&ssuid->SubserviceType, sizeof(ssuid->SubserviceType));
}

void hostorder_CSSM_CRYPTO_DATA(CSSM_CRYPTO_DATA *crypto, off_t offset)
{
	if (!crypto) return;
	hostorder_CSSM_DATA(&crypto->Param, offset);
	flip(&crypto->Callback, sizeof(crypto->Callback));
	flip(&crypto->CallerCtx, sizeof(crypto->CallerCtx));
}

void hostorder_CSSM_LIST(CSSM_LIST *list, off_t offset)
{
	CSSM_LIST_ELEMENT_PTR ptr;
	intptr_t newaddr;			/* for readability */

	if (!list) return;

	flip(&list->ListType, sizeof(list->ListType));

	if (list->Head)
	{
		flip(&list->Head, sizeof(list->Head));
		newaddr = reinterpret_cast<intptr_t>(list->Head) + offset;
		list->Head = reinterpret_cast<CSSM_LIST_ELEMENT *>(newaddr);
	}

	if (list->Tail)
	{
		flip(&list->Tail, sizeof(list->Tail));
		newaddr = reinterpret_cast<intptr_t>(list->Tail) + offset;
		list->Tail = reinterpret_cast<CSSM_LIST_ELEMENT *>(newaddr);
	}

	for (ptr = list->Head; ptr != NULL; ptr = ptr->NextElement)
	{
		hostorder_CSSM_LIST_ELEMENT(ptr, offset);
	}
}

void hostorder_CSSM_LIST_ELEMENT(CSSM_LIST_ELEMENT *element, off_t offset)
{
	intptr_t newaddr;			/* for readability */

	if (!element) return;

	if (element->NextElement)
	{
		flip(&element->NextElement, sizeof(element->NextElement));
		newaddr = reinterpret_cast<intptr_t>(element->NextElement) + offset;
		element->NextElement = reinterpret_cast<CSSM_LIST_ELEMENT_PTR>(newaddr);
	}

	flip(&element->WordID, sizeof(element->WordID));
	flip(&element->ElementType, sizeof(element->ElementType));
	switch (element->ElementType)
	{
		case CSSM_LIST_ELEMENT_DATUM:
			hostorder_CSSM_DATA(&element->Element.Word, offset);
			break;
		case CSSM_LIST_ELEMENT_SUBLIST:
			hostorder_CSSM_LIST(&element->Element.Sublist, offset);
			break;
		case CSSM_LIST_ELEMENT_WORDID:
			break;
		default:
			fprintf(stderr, "hostorder_CSSM_LIST_ELEMENT() (unknown ListElement type)\n");
	}
}

void hostorder_CSSM_SAMPLE(CSSM_SAMPLE *sample, off_t offset)
{
	CSSM_SUBSERVICE_UID *ptr;

	if (!sample) return;
	hostorder_CSSM_LIST(&sample->TypedSample, offset);
	if (sample->Verifier)
	{
		ptr = const_cast<CSSM_SUBSERVICE_UID *>(sample->Verifier);
		flip(&ptr, sizeof(CSSM_SUBSERVICE_UID *));
		sample->Verifier = reinterpret_cast<CSSM_SUBSERVICE_UID *>(reinterpret_cast<intptr_t>(ptr) + offset);
		/* Verifier had better not be really and truly immutable... */
		hostorder_CSSM_SUBSERVICE_UID(const_cast<CSSM_SUBSERVICE_UID *>(sample->Verifier));
	}
}

void hostorder_CSSM_SAMPLEGROUP(CSSM_SAMPLEGROUP *sgrp, off_t offset)
{
	u_int i;
	CSSM_SAMPLE *ptr;

	if (!sgrp) return;
	flip(&sgrp->NumberOfSamples, sizeof(sgrp->NumberOfSamples));
	if (sgrp->Samples)
	{
		ptr = const_cast<CSSM_SAMPLE *>(sgrp->Samples);
		flip(&ptr, sizeof(ptr));
		sgrp->Samples = reinterpret_cast<CSSM_SAMPLE *>(reinterpret_cast<intptr_t>(ptr) + offset);
		for (i = 0; i < sgrp->NumberOfSamples; ++i)
		{
			hostorder_CSSM_SAMPLE(const_cast<CSSM_SAMPLE *>(&sgrp->Samples[i]), offset);
		}
	}
}

void hostorder_CSSM_ENCODED_CERT(CSSM_ENCODED_CERT *cert, off_t offset)
{
	if (!cert) return;
	flip(&cert->CertType, sizeof(cert->CertType));
	flip(&cert->CertEncoding, sizeof(cert->CertEncoding));
	hostorder_CSSM_DATA(&cert->CertBlob, offset);
}

void hostorder_CSSM_CERTGROUP(CSSM_CERTGROUP *grp, off_t offset)
{
	const char *func = "hostorder_CSSM_CERTGROUP()";
	intptr_t newaddr;			/* for readability */
	u_int i;

	if (!grp) return;
	flip(&grp->CertType, sizeof(grp->CertType));
	flip(&grp->CertEncoding, sizeof(grp->CertEncoding));
	flip(&grp->NumCerts, sizeof(grp->NumCerts));
	/* Any field in the union will do; CertList is the shortest to type */
	if (grp->GroupList.CertList)
	{
		flip(&grp->GroupList.CertList, sizeof(CSSM_DATA *));
		newaddr = reinterpret_cast<intptr_t>(grp->GroupList.CertList) + offset;
		grp->GroupList.CertList = reinterpret_cast<CSSM_DATA_PTR>(newaddr);
	}
	/* handled out of order of definition since for() loop depends on it */
	flip(&grp->CertGroupType, sizeof(grp->CertGroupType));

	/* Note: we will crash if GroupList contains NULL and NumCerts > 0 */
	for (i = 0; i < grp->NumCerts; ++i)
	{
		char *err = NULL;

		switch (grp->CertGroupType)
		{
			case CSSM_CERTGROUP_DATA:
				hostorder_CSSM_DATA(&grp->GroupList.CertList[i], offset);
				break;
			/* damned if I can find an example of the others */
			case CSSM_CERTGROUP_ENCODED_CERT:
				/* See the cautionary note in compare_CSSM_CERTGROUP() */
				hostorder_CSSM_ENCODED_CERT(&grp->GroupList.EncodedCertList[i],
											offset);
				break;
			case CSSM_CERTGROUP_PARSED_CERT:
				err = "CSSM_CERTGROUP_PARSED_CERT unimplemented";
				break;
			case CSSM_CERTGROUP_CERT_PAIR:
				err = "CSSM_CERTGROUP_CERT_PAIR unimplemented";
				break;
			default:
				err = "unknown type";
				break;
		}
		if (err)
		{
			fprintf(stderr, "%s (%s)\n", func, err);
			return;
		}
	}
	flip(&grp->Reserved, sizeof(grp->Reserved));
	/* Depending on how Reserved is used, this code might be required
	newaddr = reinterpret_cast<intptr_t>(grp->Reserved) + offset;
	grp->Reserved = reinterpret_cast<void *>(newaddr);
	*/
}

void hostorder_CSSM_BASE_CERTS(CSSM_BASE_CERTS *certs, off_t offset)
{
	if (!certs) return;
	flip(&certs->TPHandle, sizeof(certs->TPHandle));
	flip(&certs->CLHandle, sizeof(certs->CLHandle));
	hostorder_CSSM_CERTGROUP(&certs->Certs, offset);
}

void hostorder_CSSM_AUTHORIZATIONGROUP(CSSM_AUTHORIZATIONGROUP *grp,
									   off_t offset)
{
	intptr_t newaddr;		/* for readability */
	uint32_t i;

	flip(&grp->NumberOfAuthTags, sizeof(grp->NumberOfAuthTags));
	if (grp->AuthTags)
	{
		flip(&grp->AuthTags, sizeof(grp->AuthTags));
		newaddr = reinterpret_cast<intptr_t>(grp->AuthTags) + offset;
		grp->AuthTags = reinterpret_cast<CSSM_ACL_AUTHORIZATION_TAG *>(newaddr);
	}
	for (i = 0; i < grp->NumberOfAuthTags; ++i)
	{
		flip(&grp->AuthTags[i], sizeof(CSSM_ACL_AUTHORIZATION_TAG));
	}
}

void hostorder_CSSM_ACL_VALIDITY_PERIOD(CSSM_ACL_VALIDITY_PERIOD *period,
										off_t offset)
{
	hostorder_CSSM_DATA(&period->StartDate, offset);
	hostorder_CSSM_DATA(&period->EndDate, offset);
}

void hostorder_CSSM_ACL_OWNER_PROTOTYPE(CSSM_ACL_OWNER_PROTOTYPE *proto, 
										off_t offset)
{
	hostorder_CSSM_LIST(&proto->TypedSubject, offset);
	flip(&proto->Delegate, sizeof(proto->Delegate));
}

void hostorder_CSSM_ACL_ENTRY_PROTOTYPE(CSSM_ACL_ENTRY_PROTOTYPE *proto, 
										off_t offset)
{
	hostorder_CSSM_LIST(&proto->TypedSubject, offset);
	flip(&proto->Delegate, sizeof(proto->Delegate));
	hostorder_CSSM_AUTHORIZATIONGROUP(&proto->Authorization, offset);
	hostorder_CSSM_ACL_VALIDITY_PERIOD(&proto->TimeRange, offset);
}

void hostorder_CSSM_ACL_ENTRY_INPUT(CSSM_ACL_ENTRY_INPUT *input, off_t offset)
{
	hostorder_CSSM_ACL_ENTRY_PROTOTYPE(&input->Prototype, offset);
	flip(&input->Callback, sizeof(input->Callback));
	flip(&input->CallerContext, sizeof(input->CallerContext));
}

void hostorder_CSSM_ACL_ENTRY_INFO(CSSM_ACL_ENTRY_INFO *info, off_t offset)
{
	hostorder_CSSM_ACL_ENTRY_PROTOTYPE(&info->EntryPublicInfo, offset);
	flip(&info->EntryHandle, sizeof(info->EntryHandle));
}

void hostorder_CSSM_ACCESS_CREDENTIALS(CSSM_ACCESS_CREDENTIALS *creds,
									   off_t offset)
{
	if (!creds) return;
	hostorder_CSSM_BASE_CERTS(&creds->BaseCerts, offset);
	hostorder_CSSM_SAMPLEGROUP(&creds->Samples, offset);
	flip(&creds->Callback, sizeof(creds->Callback));
	flip(&creds->CallerCtx, sizeof(creds->CallerCtx));
}

void hostorder_CSSM_KEYHEADER(CSSM_KEYHEADER *hdr)
{
	if (!hdr) return;
	flip(&hdr->HeaderVersion, sizeof(hdr->HeaderVersion));
	flip(&hdr->BlobType, sizeof(hdr->BlobType));
	flip(&hdr->Format, sizeof(hdr->Format));
	flip(&hdr->AlgorithmId, sizeof(hdr->AlgorithmId));
	flip(&hdr->KeyClass, sizeof(hdr->KeyClass));
	flip(&hdr->LogicalKeySizeInBits, sizeof(hdr->LogicalKeySizeInBits));
	flip(&hdr->KeyAttr, sizeof(hdr->KeyAttr));
	flip(&hdr->KeyUsage, sizeof(hdr->KeyUsage));
	flip(&hdr->WrapAlgorithmId, sizeof(hdr->WrapAlgorithmId));
	flip(&hdr->WrapMode, sizeof(hdr->WrapMode));
	flip(&hdr->Reserved, sizeof(hdr->Reserved));
}

void hostorder_CSSM_KEY(CSSM_KEY *key, off_t offset)
{
	if (!key) return;
	hostorder_CSSM_KEYHEADER(&key->KeyHeader);
	hostorder_CSSM_DATA(&key->KeyData, offset);
}

void hostorder_CSSM_DL_DB_HANDLE(CSSM_DL_DB_HANDLE *handle)
{
	if (!handle) return;
	/* 
	 * XXX/gh  offset is needed if these values are being treated as
	 * as pointers!
	 */
	flip(&handle->DLHandle, sizeof(handle->DLHandle));
	flip(&handle->DBHandle, sizeof(handle->DBHandle));
}

void hostorder_CSSM_RANGE(CSSM_RANGE *range)
{
	if (!range) return;
	flip(&range->Min, sizeof(range->Min));
	flip(&range->Max, sizeof(range->Max));
}

void hostorder_CSSM_CONTEXT_ATTRIBUTE(CSSM_CONTEXT_ATTRIBUTE *attr, 
									  off_t offset)
{
	if (!attr) return;
	flip(&attr->AttributeType, sizeof(attr->AttributeType));
	flip(&attr->AttributeLength, sizeof(attr->AttributeLength));
	if ((attr->AttributeType & CSSM_ATTRIBUTE_TYPE_MASK) == CSSM_ATTRIBUTE_DATA_UINT32)
	{
		flip(&attr->Attribute.Uint32, sizeof(attr->Attribute.Uint32));
	}
	else
	{
		intptr_t newaddr;			/* for readability */

		/* any pointer accessor of the union will do */
		if (attr->Attribute.String)
		{
			flip(&attr->Attribute.String, sizeof(attr->Attribute.String));
			newaddr = reinterpret_cast<intptr_t>(attr->Attribute.String) + offset;
			attr->Attribute.String = reinterpret_cast<char *>(newaddr);
		}
		switch (attr->AttributeType & CSSM_ATTRIBUTE_TYPE_MASK)
		{
			case CSSM_ATTRIBUTE_DATA_CSSM_DATA:
				hostorder_CSSM_DATA(attr->Attribute.Data, offset);
				break;
			case CSSM_ATTRIBUTE_DATA_CRYPTO_DATA:
				hostorder_CSSM_CRYPTO_DATA(attr->Attribute.CryptoData, offset);
				break;
			case CSSM_ATTRIBUTE_DATA_KEY:
				hostorder_CSSM_KEY(attr->Attribute.Key, offset);
				break;
			case CSSM_ATTRIBUTE_DATA_STRING:
			case CSSM_ATTRIBUTE_DATA_DATE:
				break;
			case CSSM_ATTRIBUTE_DATA_RANGE:
				hostorder_CSSM_RANGE(attr->Attribute.Range);
				break;
			case CSSM_ATTRIBUTE_DATA_ACCESS_CREDENTIALS:
				hostorder_CSSM_ACCESS_CREDENTIALS(attr->Attribute.AccessCredentials, offset);
				break;
			case CSSM_ATTRIBUTE_DATA_VERSION:
				hostorder_CSSM_VERSION(attr->Attribute.Version);
				break;
			case CSSM_ATTRIBUTE_DATA_DL_DB_HANDLE:
				hostorder_CSSM_DL_DB_HANDLE(attr->Attribute.DLDBHandle);
				break;
			/* _KR_PROFILE not supported? */
			default:
				fprintf(stderr, "hostorder_CSSM_CONTEXT_ATTRIBUTE(): unrecognized attribute type\n");
		}
	}	/* end if (CSSM_ATTRIBUTE_DATA_UINT32 */
}

void hostorder_CSSM_CONTEXT(CSSM_CONTEXT *ctx, CSSM_CONTEXT_ATTRIBUTE *attrs)
{
	off_t offset;
	uint32_t i;

	if (!ctx) return;
	flip(&ctx->ContextType, sizeof(ctx->ContextType));
	flip(&ctx->AlgorithmType, sizeof(ctx->AlgorithmType));
	flip(&ctx->NumberOfAttributes, sizeof(ctx->NumberOfAttributes));
	if (ctx->ContextAttributes)
	{
		flip(&ctx->ContextAttributes, sizeof(ctx->ContextAttributes));
		offset = reinterpret_cast<intptr_t>(attrs) - reinterpret_cast<intptr_t>(ctx->ContextAttributes);
		ctx->ContextAttributes = reinterpret_cast<CSSM_CONTEXT_ATTRIBUTE *>(attrs);
	}
	for (i = 0; i < ctx->NumberOfAttributes; ++i)
	{
		hostorder_CSSM_CONTEXT_ATTRIBUTE(&ctx->ContextAttributes[i], offset);
	}
	flip(&ctx->CSPHandle, sizeof(ctx->CSPHandle));
	flip(&ctx->Privileged, sizeof(ctx->Privileged));
	flip(&ctx->EncryptionProhibited, sizeof(ctx->EncryptionProhibited));
	flip(&ctx->WorkFactor, sizeof(ctx->WorkFactor));
	flip(&ctx->Reserved, sizeof(ctx->Reserved));
}

void hostorder_CSSM_OID(CSSM_OID *oid, off_t offset)
{
	hostorder_CSSM_DATA(reinterpret_cast<CSSM_DATA *>(oid), offset);
}

void hostorder_CSSM_DB_ATTRIBUTE_INFO(CSSM_DB_ATTRIBUTE_INFO *attrinfo, 
									  off_t offset)
{
	if (!attrinfo) return;
	flip(&attrinfo->AttributeNameFormat, sizeof(attrinfo->AttributeNameFormat));
	switch(attrinfo->AttributeNameFormat)
	{
		case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
		{
			intptr_t newaddr;			/* for readability */
			flip(&attrinfo->Label.AttributeName, sizeof(attrinfo->Label.AttributeName));
			newaddr = reinterpret_cast<intptr_t>(attrinfo->Label.AttributeName) + offset;
			attrinfo->Label.AttributeName = reinterpret_cast<char *>(newaddr);
			break;
		}
		case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
			hostorder_CSSM_OID(&attrinfo->Label.AttributeOID, offset);
			break;
		case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
			flip(&attrinfo->Label.AttributeID, sizeof(attrinfo->Label.AttributeID));
			break;
		default:
			break;		/* error, but no way to tell caller */
	}
	flip(&attrinfo->AttributeFormat, sizeof(attrinfo->AttributeFormat));
}

void hostorder_CSSM_DB_ATTRIBUTE_DATA(CSSM_DB_ATTRIBUTE_DATA *attrdata, 
									  off_t offset)
{
	uint32_t i;

	if (!attrdata) return;
	hostorder_CSSM_DB_ATTRIBUTE_INFO(&attrdata->Info, offset);
	flip(&attrdata->NumberOfValues, sizeof(attrdata->NumberOfValues));
	for (i = 0; i < attrdata->NumberOfValues; ++i)
	{
		hostorder_CSSM_DATA(&attrdata->Value[i], offset);
	}
}

void hostorder_CSSM_SELECTION_PREDICATE(CSSM_SELECTION_PREDICATE *pred, 
										off_t offset)
{
	if (!pred) return;
	flip(&pred->DbOperator, sizeof(pred->DbOperator));
	hostorder_CSSM_DB_ATTRIBUTE_DATA(&pred->Attribute, offset);
}

void hostorder_CSSM_QUERY_LIMITS(CSSM_QUERY_LIMITS *limits)
{
	if (!limits) return;
	flip(&limits->TimeLimit, sizeof(limits->TimeLimit));
	flip(&limits->SizeLimit, sizeof(limits->SizeLimit));
}

void hostorder_CSSM_QUERY(CSSM_QUERY *query, off_t offset)
{
	uint32_t i;

	if (!query) return;
	flip(&query->RecordType, sizeof(query->RecordType));
	flip(&query->Conjunctive, sizeof(query->Conjunctive));
	flip(&query->NumSelectionPredicates, sizeof(query->NumSelectionPredicates));
	if (query->SelectionPredicate)
	{
		intptr_t newaddr;		/* for readability */

		flip(&query->SelectionPredicate, sizeof(query->SelectionPredicate));
		newaddr = reinterpret_cast<intptr_t>(query->SelectionPredicate) + offset;
		query->SelectionPredicate = reinterpret_cast<CSSM_SELECTION_PREDICATE *>(newaddr);
	}
	for (i = 0; i < query->NumSelectionPredicates; ++i)
	{
		hostorder_CSSM_SELECTION_PREDICATE(&query->SelectionPredicate[i], offset);
	}
	hostorder_CSSM_QUERY_LIMITS(&query->QueryLimits);
	flip(&query->QueryFlags, sizeof(query->QueryFlags));
}


/**************************************************************************
 * Comparators--data integrity checking routines.  
 *
 * Each comparator compares two of the same high-level data structure, one 
 * of which is presumed to have been through at least one 
 * encoding/decoding cycle; these comparators check for errors introduced
 * during that cycle.  
 *
 * TODO/gh  A hand-crafted function per type seems sloppy, not well thought
 * out.  I bet I could leverage part of the walker machinery to create a
 * more elegant solution (a "comparison walker," anyone?).  Whether the
 * result would be maintainable is, of course, a different question....  
 **************************************************************************/

int compare_CSSM_DATA(CSSM_DATA *data1, CSSM_DATA *data2)
{
	const char *func = "compare_CSSM_DATA()";

	NULLCHECK(data1, data2, func);
	if (data1->Length != data2->Length ||
		memcmp(data1->Data, data2->Data, data1->Length))
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_SUBSERVICE_UID(const CSSM_SUBSERVICE_UID *ssuid1, 
								const CSSM_SUBSERVICE_UID *ssuid2) 
{
	const char *func = "compare_CSSM_SUBSERVICE_UID()";

	NULLCHECK(ssuid1, ssuid2, func);
	if (memcmp(&ssuid1->Guid, &ssuid2->Guid, sizeof(CSSM_GUID))          ||
		memcmp(&ssuid1->Version, &ssuid2->Version, sizeof(CSSM_VERSION)) ||
		ssuid1->SubserviceId   != ssuid2->SubserviceId                   ||
		ssuid1->SubserviceType != ssuid2->SubserviceType)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_CRYPTO_DATA(CSSM_CRYPTO_DATA *data1, CSSM_CRYPTO_DATA *data2)
{
	const char *func = "compare_CSSM_CRYPTO_DATA()";
	int ret;

	NULLCHECK(data1, data2, func);
	if ((ret = compare_CSSM_DATA(&data1->Param, &data2->Param)) != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	if (data1->Callback  != data2->Callback  ||
		data1->CallerCtx != data2->CallerCtx)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_LIST(const CSSM_LIST *list1, const CSSM_LIST *list2)
{
	const char *func = "compare_CSSM_LIST()";
	CSSM_LIST_ELEMENT_PTR p1, p2;
	int ret;

	NULLCHECK(list1, list2, func);
	if (list1->ListType != list2->ListType)
	{
		fprintf(stderr, "%s (ListType)\n", func);
		return MISMATCH;
	}
	for (p1 = list1->Head, p2 = list2->Head; 
		 p1 != NULL && p2 != NULL; 
		 p1 = p1->NextElement, p2 = p2->NextElement)
	{
		if (p1->ElementType != p2->ElementType)
		{
			fprintf(stderr, "%s (ListElements' ElementType)\n", func);
			return MISMATCH;
		}
		switch (p1->ElementType)
		{
			case CSSM_LIST_ELEMENT_DATUM:
				ret = compare_CSSM_DATA(&p1->Element.Word, &p2->Element.Word);
				if (ret != OK)
				{
					fprintf(stderr, "%s\n", func);
					return ret;
				}
				break;
			case CSSM_LIST_ELEMENT_SUBLIST:
				ret = compare_CSSM_LIST(&p1->Element.Sublist, &p2->Element.Sublist);
				if (ret != OK)
				{
					fprintf(stderr, "%s\n", func);
					return ret;
				}
				break;
			case CSSM_LIST_ELEMENT_WORDID:
				if (p1->WordID != p2->WordID)
				{
					fprintf(stderr, "%s (ListElements' WordID)\n", func);
					return MISMATCH;
				}
				break;
			default:
				fprintf(stderr, "%s (unknown ListElement type)\n", func);
				return UNKNOWN_TYPE;
		}
		if ((p1->NextElement == NULL && p1 != list1->Tail) ||
			(p2->NextElement == NULL && p2 != list2->Tail))
		{
			fprintf(stderr, "%s (tail mismatch)\n", func);
			return MISMATCH;
		}
	}
	if (p1 != NULL || p2 != NULL)	/* lists didn't both terminate */
	{
		fprintf(stderr, "%s (unequal lists)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_SAMPLE(const CSSM_SAMPLE *sample1, const CSSM_SAMPLE *sample2)
{
	const char *func = "compare_CSSM_SAMPLE()";
	int ret;
	
	NULLCHECK(sample1, sample2, func);
	ret = compare_CSSM_LIST(&sample1->TypedSample, &sample2->TypedSample);
	if (ret != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	if (sample1->Verifier && sample2->Verifier)
	{
		ret = compare_CSSM_SUBSERVICE_UID(sample1->Verifier, sample2->Verifier);
		if (ret != OK)
		{
			fprintf(stderr, "%s\n", func);
			return ret;
		}
	}
	else if (sample1->Verifier && !sample2->Verifier ||
			 !sample1->Verifier && sample2->Verifier)
	{
		fprintf(stderr, "%s (Verifier mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_SAMPLEGROUP(CSSM_SAMPLEGROUP *sgrp1, CSSM_SAMPLEGROUP *sgrp2)
{
	const char *func = "compare_CSSM_SAMPLEGROUP()";
	int ret;
	u_int i;

	NULLCHECK(sgrp1, sgrp2, func);
	if (sgrp1->NumberOfSamples != sgrp2->NumberOfSamples)
	{
		fprintf(stderr, "%s (NumberOfSamples mismatch)\n", func);
		return MISMATCH;
	}
	for (i = 0; i < sgrp1->NumberOfSamples; ++i)
	{
		ret = compare_CSSM_SAMPLE(&sgrp1->Samples[i], &sgrp2->Samples[i]);
		if (ret != OK)
		{
			fprintf(stderr, "%s\n", func);
			return ret;
		}
	}
	return OK;
}

int compare_CSSM_ENCODED_CERT(CSSM_ENCODED_CERT *cert1, 
							  CSSM_ENCODED_CERT *cert2)
{
	const char *func = "compare_CSSM_ENCODED_CERT()";
	int ret;

	NULLCHECK(cert1, cert2, func);
	if (cert1->CertType     != cert2->CertType     ||
		cert1->CertEncoding != cert2->CertEncoding)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	if ((ret = compare_CSSM_DATA(&cert1->CertBlob, &cert2->CertBlob)) != OK)
		fprintf(stderr, "%s\n", func);
	return ret;
}

int compare_CSSM_CERTGROUP(CSSM_CERTGROUP *grp1, CSSM_CERTGROUP *grp2)
{
	const char *func = "compare_CSSM_CERTGROUP()";
	int ret;
	u_int i;

	NULLCHECK(grp1, grp2, func);
	if (grp1->CertType      != grp2->CertType      ||
		grp1->CertEncoding  != grp2->CertEncoding  ||
		grp1->NumCerts      != grp2->NumCerts      ||
		grp1->CertGroupType != grp2->CertGroupType ||
		grp1->Reserved      != grp2->Reserved)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	for (i = 0; i < grp1->NumCerts; ++i)
	{
		char *err = NULL;

		switch (grp1->CertGroupType)
		{
			case CSSM_CERTGROUP_DATA:
				ret = compare_CSSM_DATA(&grp1->GroupList.CertList[i],
										&grp2->GroupList.CertList[i]);
				break;
			/* damned if I can find an example of the others */
			case CSSM_CERTGROUP_ENCODED_CERT:
				/* 
				 * This is apparently in use (see CertGroup in 
				 * cdsa_utilities, cssmcert.{cpp,h}), but it's just a 
				 * guess that it's implemented in the same way as 
				 * CSSM_CERTGROUP_DATA...
				 */
				ret = compare_CSSM_ENCODED_CERT(&grp1->GroupList.EncodedCertList[i],
												&grp2->GroupList.EncodedCertList[i]);
				break;
			case CSSM_CERTGROUP_PARSED_CERT:
				err = "CSSM_CERTGROUP_PARSED_CERT unimplemented";
				ret = NOT_IMPLEMENTED;
				break;
			case CSSM_CERTGROUP_CERT_PAIR:
				err = "CSSM_CERTGROUP_CERT_PAIR unimplemented";
				ret = NOT_IMPLEMENTED;
				break;
			default:
				err = "unknown type";
				ret = UNKNOWN_TYPE;
				break;
		}
		if (ret != OK)
		{
			if (err)
				fprintf(stderr, "%s (%s)\n", func, err);
			else
				fprintf(stderr, "%s\n", func);
			return ret;
		}
	}
	return OK;
}

int compare_CSSM_BASE_CERTS(CSSM_BASE_CERTS *bases1, CSSM_BASE_CERTS *bases2)
{
	const char *func = "compare_CSSM_BASE_CERTS()";
	int ret;

	NULLCHECK(bases1, bases2, func);
	if (bases1->TPHandle != bases2->TPHandle ||
		bases1->CLHandle != bases2->CLHandle)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	ret = compare_CSSM_CERTGROUP(&bases1->Certs, &bases2->Certs);
	if (ret != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	return OK;
}

int compare_CSSM_ACCESS_CREDENTIALS(CSSM_ACCESS_CREDENTIALS *creds1,
									CSSM_ACCESS_CREDENTIALS *creds2)
{
	const char *func = "compare_CSSM_ACCESS_CREDENTIALS()";
	int ret; 

	NULLCHECK(creds1, creds2, func);
	if (memcmp(creds1->EntryTag, creds2->EntryTag, sizeof(CSSM_STRING)) ||
		creds1->Callback  != creds2->Callback                           ||
		creds1->CallerCtx != creds2->CallerCtx)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	ret = compare_CSSM_BASE_CERTS(&creds1->BaseCerts, &creds2->BaseCerts);
	if (ret != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	ret = compare_CSSM_SAMPLEGROUP(&creds1->Samples, &creds2->Samples);
	if (ret != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	return OK;
}

int compare_CSSM_AUTHORIZATIONGROUP(CSSM_AUTHORIZATIONGROUP *grp1,
									CSSM_AUTHORIZATIONGROUP *grp2)
{
	const char *func = "compare_CSSM_AUTHORIZATIONGROUP()";

	NULLCHECK(grp1, grp2, func);
	if (grp1->NumberOfAuthTags != grp2->NumberOfAuthTags ||
		memcmp(grp1->AuthTags, grp2->AuthTags, grp1->NumberOfAuthTags*ALIGNSIZE(CSSM_ACL_AUTHORIZATION_TAG)))
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_ACL_VALIDITY_PERIOD(CSSM_ACL_VALIDITY_PERIOD *period1,
									 CSSM_ACL_VALIDITY_PERIOD *period2)
{
	const char *func = "compare_CSSM_ACL_VALIDITY_PERIOD()";
	int ret;

	NULLCHECK(period1, period2, func);
	ret = compare_CSSM_DATA(&period1->StartDate, &period1->StartDate);
	if (ret != OK)
	{
		fprintf(stderr, "%s (StartDate)\n", func);
		return ret;
	}
	ret = compare_CSSM_DATA(&period1->EndDate, &period1->EndDate);
	if (ret != OK)
		fprintf(stderr, "%s (EndDate)\n", func);
	return ret;
}

int compare_CSSM_ACL_ENTRY_PROTOTYPE(CSSM_ACL_ENTRY_PROTOTYPE *proto1,
									 CSSM_ACL_ENTRY_PROTOTYPE *proto2,
									 int skipGarbage)
{
	const char *func = "compare_CSSM_ACL_ENTRY_PROTOTYPE()";
	int ret;

	NULLCHECK(proto1, proto2, func);
	ret = compare_CSSM_LIST(&proto1->TypedSubject, &proto2->TypedSubject);
	if (ret != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	if (!skipGarbage)
	{
		if (proto1->Delegate != proto2->Delegate)
		{
			fprintf(stderr, "%s (Delegate mismatch)\n", func);
			return MISMATCH;
		}
	}
	ret = compare_CSSM_AUTHORIZATIONGROUP(&proto1->Authorization, &proto2->Authorization);
	if (ret != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	if (!skipGarbage)
	{
		ret = compare_CSSM_ACL_VALIDITY_PERIOD(&proto1->TimeRange, &proto2->TimeRange);
		if (ret != OK)
		{
			fprintf(stderr, "%s\n", func);
			return ret;
		}
	}
	if (memcmp(proto1->EntryTag, proto2->EntryTag, sizeof(CSSM_STRING)))
	{
		fprintf(stderr, "%s (EntryTag mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_ACL_OWNER_PROTOTYPE(CSSM_ACL_OWNER_PROTOTYPE *proto1,
									 CSSM_ACL_OWNER_PROTOTYPE *proto2)
{
	const char *func = "compare_CSSM_ACL_OWNER_PROTOTYPE()";

	NULLCHECK(proto1, proto2, func);
	int ret = compare_CSSM_LIST(&proto1->TypedSubject, &proto2->TypedSubject);
	if (ret != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	if (proto1->Delegate != proto2->Delegate)
	{
		fprintf(stderr, "%s (Delegate mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_ACL_ENTRY_INPUT(CSSM_ACL_ENTRY_INPUT *input1, 
								 CSSM_ACL_ENTRY_INPUT *input2)
{
	const char *func = "compare_CSSM_ACL_ENTRY_INPUT()";
	int ret, skipGarbage = 0;

	NULLCHECK(input1, input2, func);
	ret = compare_CSSM_ACL_ENTRY_PROTOTYPE(&input1->Prototype,
										   &input2->Prototype, 
										   skipGarbage);
	if (ret != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	if (input1->Callback      != input2->Callback      ||
		input1->CallerContext != input2->CallerContext)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_ACL_ENTRY_INFO(CSSM_ACL_ENTRY_INFO *info1,
								CSSM_ACL_ENTRY_INFO *info2)
{
	const char *func = "compare_CSSM_ACL_ENTRY_INFO()";
	int ret, skipGarbage = 0;

	NULLCHECK(info1, info2, func);
#if SECURITYD_SENDS_GARBAGE
	skipGarbage = 1;
	/* fprintf(stderr, "%s: skipping garbage\n", func); */
#endif
	ret = compare_CSSM_ACL_ENTRY_PROTOTYPE(&info1->EntryPublicInfo, 
										   &info2->EntryPublicInfo, 
										   skipGarbage);
	if (ret != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	if (info1->EntryHandle != info2->EntryHandle)
	{
		fprintf(stderr, "%s (EntryHandle mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_DATE(CSSM_DATE *date1, CSSM_DATE *date2)
{
	const char *func = "compare_CSSM_DATE()";

	NULLCHECK(date1, date2, func);
	if (memcmp(date1, date2, sizeof(CSSM_DATE)))
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_KEYHEADER(CSSM_KEYHEADER *hdr1, CSSM_KEYHEADER *hdr2)
{
	const char *func = "compare_CSSM_KEYHEADER()";
	int ret;

	NULLCHECK(hdr1, hdr2, func);
	if (hdr1->HeaderVersion        != hdr2->HeaderVersion        ||
		memcmp(&hdr1->CspId, &hdr2->CspId, sizeof(CSSM_GUID))    ||
		hdr1->BlobType             != hdr2->BlobType             ||
		hdr1->Format               != hdr2->Format               ||
		hdr1->AlgorithmId          != hdr2->AlgorithmId          ||
		hdr1->KeyClass             != hdr2->KeyClass             ||
		hdr1->LogicalKeySizeInBits != hdr2->LogicalKeySizeInBits ||
		hdr1->KeyUsage             != hdr2->KeyUsage             ||
		hdr1->WrapAlgorithmId      != hdr2->WrapAlgorithmId      ||
		hdr1->WrapMode             != hdr2->WrapMode             ||
		hdr1->Reserved             != hdr2->Reserved)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	if ((ret = compare_CSSM_DATE(&hdr1->StartDate, &hdr2->StartDate)) != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	if ((ret = compare_CSSM_DATE(&hdr1->EndDate, &hdr2->EndDate)) != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	return OK;
}

int compare_CSSM_KEY(CSSM_KEY *key1, CSSM_KEY *key2)
{
	const char *func = "compare_CSSM_KEY()";
	int ret;

	NULLCHECK(key1, key2, func);
	if ((ret = compare_CSSM_KEYHEADER(&key1->KeyHeader, &key1->KeyHeader)) != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	if ((ret = compare_CSSM_DATA(&key1->KeyData, &key2->KeyData)) != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	return OK;
}

int compare_CSSM_RANGE(CSSM_RANGE *range1, CSSM_RANGE *range2)
{
	const char *func = "compare_CSSM_RANGE()";

	NULLCHECK(range1, range2, func);
	if (memcmp(range1, range2, sizeof(CSSM_RANGE)))
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_CONTEXT_ATTRIBUTE(CSSM_CONTEXT_ATTRIBUTE *attr1,
								   CSSM_CONTEXT_ATTRIBUTE *attr2)
{
	const char *func = "compare_CSSM_CONTEXT_ATTRIBUTE()";
	char *err = NULL;
	int ret = OK;

	NULLCHECK(attr1, attr2, func);
	if (attr1->AttributeType   != attr2->AttributeType   ||
		attr1->AttributeLength != attr2->AttributeLength)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	switch (attr1->AttributeType & CSSM_ATTRIBUTE_TYPE_MASK)
	{
		case CSSM_ATTRIBUTE_DATA_UINT32:
			if (attr1->Attribute.Uint32 != attr2->Attribute.Uint32)
			{
				err = "Uint32 mismatch";
				ret = MISMATCH;
			}
			break;
		case CSSM_ATTRIBUTE_DATA_CSSM_DATA:
			ret = compare_CSSM_DATA(attr1->Attribute.Data, attr2->Attribute.Data);
			break;
		case CSSM_ATTRIBUTE_DATA_CRYPTO_DATA:
			ret = compare_CSSM_CRYPTO_DATA(attr1->Attribute.CryptoData, attr2->Attribute.CryptoData);
			break;
		case CSSM_ATTRIBUTE_DATA_KEY:
			ret = compare_CSSM_KEY(attr1->Attribute.Key, attr2->Attribute.Key);
			break;
		case CSSM_ATTRIBUTE_DATA_STRING:
			if (memcmp(attr1->Attribute.String, attr2->Attribute.String, attr1->AttributeLength))
			{
				err = "String mismatch";
				ret = MISMATCH;
			}
			break;
		case CSSM_ATTRIBUTE_DATA_DATE:
			ret = compare_CSSM_DATE(attr1->Attribute.Date, attr2->Attribute.Date);
			break;
		case CSSM_ATTRIBUTE_DATA_RANGE:
			ret = compare_CSSM_RANGE(attr1->Attribute.Range, attr2->Attribute.Range);
			break;
		case CSSM_ATTRIBUTE_DATA_ACCESS_CREDENTIALS:
			ret = compare_CSSM_ACCESS_CREDENTIALS(attr1->Attribute.AccessCredentials,
												  attr2->Attribute.AccessCredentials);
			break;
		case CSSM_ATTRIBUTE_DATA_VERSION:
			if (memcmp(&attr1->Attribute.Version, &attr2->Attribute.Version, sizeof(CSSM_VERSION)))
			{
				err = "Version mismatch";
				ret = MISMATCH;
			}
			break;
		case CSSM_ATTRIBUTE_DATA_DL_DB_HANDLE:
			if (memcmp(&attr1->Attribute.DLDBHandle, &attr2->Attribute.DLDBHandle, sizeof(CSSM_DL_DB_HANDLE)))
			{
				err = "DLDBHandle mismatch";
				ret = MISMATCH;
			}
			break;
		/* _PADDING and _KR_PROFILE not supported? */
		default:
			err = "unknown type";
			ret = UNKNOWN_TYPE;
			break;
	}
	if (ret != OK)
	{
		if (err)
			fprintf(stderr, "%s (%s)\n", func, err);
		else
			fprintf(stderr, "%s\n", func);
		return ret;
	}
	return OK;
}

int compare_CSSM_CONTEXT(CSSM_CONTEXT *ctx1, CSSM_CONTEXT *ctx2)
{
	const char *func = "compare_CSSM_CONTEXT()";
	u_int i, ret;

	NULLCHECK(ctx1, ctx2, func);
	if (ctx1->ContextType          != ctx2->ContextType          ||
		ctx1->AlgorithmType        != ctx2->AlgorithmType        ||
		ctx1->NumberOfAttributes   != ctx2->NumberOfAttributes   ||
		ctx1->CSPHandle            != ctx2->CSPHandle            ||
		ctx1->Privileged           != ctx2->Privileged           ||
		ctx1->EncryptionProhibited != ctx2->EncryptionProhibited ||
		ctx1->WorkFactor           != ctx2->WorkFactor           ||
		ctx1->Reserved             != ctx2->Reserved)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	for (i = 0; i < ctx1->NumberOfAttributes; ++i)
	{
		ret = compare_CSSM_CONTEXT_ATTRIBUTE(&ctx1->ContextAttributes[i],
											 &ctx2->ContextAttributes[i]);
		if (ret != OK)
		{
			fprintf(stderr, "%s\n", func);
			return ret;
		}
	}
	return OK;
}

int compare_CSSM_OID(CSSM_OID *oid1, CSSM_OID *oid2)
{
	return compare_CSSM_DATA(reinterpret_cast<CSSM_DATA *>(oid1), 
							 reinterpret_cast<CSSM_DATA *>(oid2));
}

/* oy -- see cdsa_utilities, cssmdb.cpp: CompareAttributeInfos() */
int compare_CSSM_DB_ATTRIBUTE_INFO(CSSM_DB_ATTRIBUTE_INFO *attrinfo1, 
								   CSSM_DB_ATTRIBUTE_INFO *attrinfo2)
{
	const char *func = "compare_CSSM_DB_ATTRIBUTE_INFO()";

	NULLCHECK(attrinfo1, attrinfo2, func);
	if (attrinfo1->AttributeNameFormat != attrinfo2->AttributeNameFormat ||
		attrinfo1->AttributeFormat     != attrinfo2->AttributeFormat)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	switch(attrinfo1->AttributeNameFormat)
	{
		case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
			if (strcmp(attrinfo1->Label.AttributeName, attrinfo2->Label.AttributeName))
			{
				fprintf(stderr, "%s (string mismatch)\n", func);
				return MISMATCH;
			}
			break;
		case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
			return compare_CSSM_OID(&attrinfo1->Label.AttributeOID,
									&attrinfo2->Label.AttributeOID);
			break;
		case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
			if (attrinfo1->Label.AttributeID != attrinfo2->Label.AttributeID)
			{
				fprintf(stderr, "%s (integer mismatch)\n", func);
				return MISMATCH;
			}
			break;
		default:
			fprintf(stderr, "%s (unknown type)\n", func);
			return UNKNOWN_TYPE;
	}
	return OK;
}

int compare_CSSM_DB_ATTRIBUTE_DATA(CSSM_DB_ATTRIBUTE_DATA *attrdata1, 
								   CSSM_DB_ATTRIBUTE_DATA *attrdata2)
{
	const char *func = "compare_CSSM_DB_ATTRIBUTE_DATA()";
	int ret;
	uint32_t i;

	NULLCHECK(attrdata1, attrdata2, func);
	ret = compare_CSSM_DB_ATTRIBUTE_INFO(&attrdata1->Info, &attrdata2->Info);
	if (ret != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	if (attrdata1->NumberOfValues != attrdata2->NumberOfValues)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	for (i = 0; i < attrdata1->NumberOfValues; ++i)
	{
		ret = compare_CSSM_DATA(&attrdata1->Value[i], &attrdata2->Value[i]);
		if (ret != OK)
		{
			fprintf(stderr, "%s (Value %d)\n", func, i+1);
			return ret;
		}
	}
	return OK;
}

int compare_CSSM_SELECTION_PREDICATE(CSSM_SELECTION_PREDICATE *pred1, 
									 CSSM_SELECTION_PREDICATE *pred2)
{
	const char *func = "compare_CSSM_SELECTION_PREDICATE()";
	int ret;

	NULLCHECK(pred1, pred2, func);
	if (pred1->DbOperator != pred2->DbOperator)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	ret = compare_CSSM_DB_ATTRIBUTE_DATA(&pred1->Attribute, &pred2->Attribute);
	if (ret != OK)
		fprintf(stderr, "%s\n", func);
	return ret;
}

int compare_CSSM_QUERY_LIMITS(CSSM_QUERY_LIMITS *limits1, 
							  CSSM_QUERY_LIMITS *limits2)
{
	const char *func = "compare_CSSM_QUERY_LIMITS()";

	NULLCHECK(limits1, limits2, func);
	if (limits1->TimeLimit != limits2->TimeLimit || 
		limits1->SizeLimit != limits2->SizeLimit)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	return OK;
}

int compare_CSSM_QUERY(CSSM_QUERY *query1, CSSM_QUERY *query2)
{
	const char *func = "compare_CSSM_QUERY()";
	int ret;
	uint32_t i;

	NULLCHECK(query1, query2, func);
	if (query1->RecordType             != query2->RecordType             ||
		query1->Conjunctive            != query2->Conjunctive            ||
		query1->NumSelectionPredicates != query2->NumSelectionPredicates ||
		query1->QueryFlags             != query2->QueryFlags)
	{
		fprintf(stderr, "%s (mismatch)\n", func);
		return MISMATCH;
	}
	for (i = 0; i < query1->NumSelectionPredicates; ++i)
	{
		ret = compare_CSSM_SELECTION_PREDICATE(&query1->SelectionPredicate[i],
											   &query2->SelectionPredicate[i]);
	}
	ret = compare_CSSM_QUERY_LIMITS(&query1->QueryLimits, &query2->QueryLimits);
	if (ret != OK)
		fprintf(stderr, "%s\n", func);
	return ret;
}

/**************************************************************************
 * Support routines for test_...() functions.  
 **************************************************************************/

CSSM_RETURN dummyACLSubjectCallback(const CSSM_LIST *subjectRequest,
									void *callerContext,
									const CSSM_MEMORY_FUNCS *MemFuncs)
{
	return CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED;	/* XXX/gh */
}

/* 
 * Dummy func to make sure CSSM_CRYPTO_DATA isn't being corrupted.  Kindly
 * note the requirement that "context" be a CSSM_CRYPTO_DATA.  
 */
CSSM_RETURN dummyCSSMCallback(CSSM_DATA *data, void *context)
{
	CSSM_CRYPTO_DATA_PTR crypto = (CSSM_CRYPTO_DATA *)context;
	data->Length = crypto->Param.Length;
	data->Data = (uint8 *)malloc(data->Length);	/* XXX/gh  leaked */
	/* XXX/gh  yeah, should check if the malloc() failed */
	memcpy(data->Data, crypto->Param.Data, data->Length);
	return CSSM_OK;
}

/**************************************************************************
 * test_CSSM_...() routines read sample data from disk (obtained from
 * securityd via the SecuritydDataSave class), set up the named
 * top-level structure by byte-reordering (if needed) and pointer
 * relocating (using libsecurity_utilities walkers), and let the
 * corresponding test_xdrwalk_...() routine test the XDR encoding/decoding
 * routines against themselves and against the equivalent walker-generated
 * output.  
 *
 * General test methodology:
 *
 * encode/decode x 3, then compare (1) the encoded original vs. the decoded
 * copy, and (2) the flattened encoded version with the equivalent walker's 
 * flattened output.  
 **************************************************************************/

/* TODO/gh  don't worry about this until we get smart cards working */
int test_CSSM_DB_RECORD_ATTRIBUTE_DATA(const char *srcfile)
{
	CSSM_DB_RECORD_ATTRIBUTE_DATA *data = NULL;

	if (srcfile)
	{
		/* read binary data from disk */
	}
	else
	{
		/* dummy something up */
		data = (CSSM_DB_RECORD_ATTRIBUTE_DATA *)malloc(sizeof(CSSM_DB_RECORD_ATTRIBUTE_DATA));
		if (!data)
			return MEM_ERROR;
		data->DataRecordType = CSSM_DL_DB_RECORD_CERT;
		/* TODO/gh  pick up from here */
	}
	if (data)
		free(data);
	return NOT_IMPLEMENTED;		/* TODO/gh */
}

int test_xdrwalk_CSSM_CONTEXT(CSSM_CONTEXT *ctx, int dbglvl)
{
	const char *func = "test_xdrwalk_CSSM_CONTEXT()";
	CSSM_CONTEXT *walkcopy, *xdrctxcopy = NULL;
	CSSM_CONTEXT_ATTRIBUTE *attrs;
	void *flattenedCtxPtr = NULL;
	u_int flattenedCtxLen = 0, i;
	int ret, iter;
	size_t attrsSize, walkedAttrsSize;

	/* 
	 * Reimplement Context::Builder so we control where the memory is
	 * allocated, thus what pointer values are.  
	 */
	SizeWalker sizer;
	for (i = 0; i < ctx->NumberOfAttributes; ++i)
	{
		walk(sizer, ctx->ContextAttributes[i]);
	}
	attrsSize = ALIGNUP(ctx->NumberOfAttributes * sizeof(CSSM_CONTEXT_ATTRIBUTE));
	walkedAttrsSize = attrsSize + ALIGNUP(sizer);

	/* create a *flat* copy of ctx for direct memcmp() w/ XDR copy */
	walkcopy = reinterpret_cast<CSSM_CONTEXT *>(calloc(1, sizeof(CSSM_CONTEXT) + walkedAttrsSize));
	if (walkcopy == NULL)
	{
		fprintf(stderr, "%s: error allocating walked context\n", func);
		return MEM_ERROR;
	}
	memcpy(walkcopy, ctx, sizeof(CSSM_CONTEXT));
	attrs = reinterpret_cast<CSSM_CONTEXT_ATTRIBUTE *>(reinterpret_cast<char *>(walkcopy) + sizeof(CSSM_CONTEXT));
	CopyWalker copier = LowLevelMemoryUtilities::increment(attrs, attrsSize);
	for (i = 0; i < ctx->NumberOfAttributes; ++i)
	{
		attrs[i] = ctx->ContextAttributes[i];	/* shallow copy */
		walk(copier, attrs[i]);					/* deep copy */
	}
	walkcopy->ContextAttributes = attrs;

	for (iter = 0; iter < N_ITERS; ++iter)
	{
		if (!xdr_mem_encode(ctx, &flattenedCtxPtr, &flattenedCtxLen, 
							(xdrproc_t)xdr_CSSM_CONTEXT))
		{
			fprintf(stderr, "%s, round %d (encode error)\n", func, iter+1);
			return XDR_ENCODE_ERROR;
		}
		/* always zero out memory before attempting a decode */
		if ((xdrctxcopy = (CSSM_CONTEXT *)calloc(1, sizeof(CSSM_CONTEXT))) == NULL)
		{
			fprintf(stderr, "%s, round %d (allocation error)\n", func, iter+1);
			return MEM_ERROR;
		}
		if (!xdr_mem_decode(flattenedCtxPtr, xdrctxcopy, flattenedCtxLen,
							(xdrproc_t)xdr_CSSM_CONTEXT))
		{
			fprintf(stderr, "%s, round %d (decode error)\n", func, iter+1);
			return XDR_DECODE_ERROR;
		}
		if (dbglvl >= 3)
			printf("comparing XDR-generated structs...\n");
		if ((ret = compare_CSSM_CONTEXT(ctx, xdrctxcopy)) != OK)
		{
			fprintf(stderr, "%s: CSSM_CONTEXT old/new XDR comparison, round %d, failed\n", 
					func, iter+1);
			return ret;
		}
		if (dbglvl >= 3)
			printf("comparing walked- and XDR-generated structs...\n");
		if ((ret = compare_CSSM_CONTEXT(xdrctxcopy, walkcopy)) != OK)
		{
			fprintf(stderr, "%s: CSSM_CONTEXT XDR/walker comparison, round %d, failed\n", 
					func, iter+1);
			return ret;
		}
		if (dbglvl >= 2)
			printf("CSSM_CONTEXT compared OK, round %d\n", iter+1);
#if 0
		if (dbglvl >= 3)
			printf("Starting XDR/walker comparison...\n");
		/*
		 * XXX/gh  xdrctxcopy and walkcopy should be identical except for 
		 * pointer offsets.  However, xdrctxcopy has extra bytes following
		 * CSSM_CONTEXT's Reserved field; still investigating why.  
		 */
		/* XXX/gh  relocate somebody's pointers */
		if (memcmp(walkcopy, xdrctxcopy, walkedAttrsSize+sizeof(CSSM_CONTEXT)))
		{
			fprintf(stderr, "%s, round %d (comparison failed)\n", func, iter+1);
			return MISMATCH;
		}
#endif
		if (iter > 0)
			free(ctx);
		ctx = xdrctxcopy;
		free(flattenedCtxPtr);
		flattenedCtxPtr = NULL;
		flattenedCtxLen = 0;
	}
	if (dbglvl >= 1)
		printf("Successfully finished CSSM_CONTEXT check\n");
	return OK;
}

int test_CSSM_CONTEXT(int fd, int doflip, int dbglvl)
{
	const char *func = "test_CSSM_CONTEXT()";
	CSSM_CONTEXT ctx;
	CSSM_CONTEXT_ATTRIBUTE *attrs;
	int ret;
	CSSM_CRYPTO_DATA crypto;

	if (fd > -1)	/* cheesy hack, but what ya gonna do? */
	{
		int csize, attrSize;
		u_int i;
		intptr_t attraddr;
		off_t offset;
		/* 
		 * Saved format: 
		 * - size (of CSSM_CONTEXT)
		 * - CSSM_CONTEXT
		 * - size (of starting address for attributes)
		 * - starting address for CSSM_CONTEXT_ATTRIBUTEs
		 * - total size of CSSM_CONTEXT_ATTRIBUTEs
		 * - CSSM_CONTEXT_ATTRIBUTEs (contiguous)
		 */
		
		/* context size; not really needed */
		if (read(fd, &csize, sizeof(csize)) != static_cast<ssize_t>(sizeof(csize)))
		{
			fprintf(stderr, "%s: Error reading context size\n", func);
			return BAD_READ;
		}
		if (doflip) flip(&csize, sizeof(csize));
		if (read(fd, &ctx, csize) != static_cast<ssize_t>(csize))
		{
			fprintf(stderr, "Error reading context\n");
			return BAD_READ;
		}
		/* Defer reorder of CSSM_CONTEXT until attributes have been read */
		/* attribute array starting address */
		if (read(fd, &csize, sizeof(csize)) != static_cast<ssize_t>(sizeof(csize)))
		{
			fprintf(stderr, "Error reading attribute address size\n");
			return BAD_READ;
		}
		if (doflip) flip(&csize, sizeof(csize));
		if (read(fd, &attraddr, csize) != csize)
		{
			fprintf(stderr, "Error reading attribute address\n");
			return BAD_READ;
		}
		/* 
		 * byte reorder of old attribute address, if needed, handled in
		 * hostorder_CSSM_CONTEXT() 
		 */
		/* size of attributes */
		if (read(fd, &attrSize, sizeof(attrSize)) != static_cast<ssize_t>(sizeof(attrSize)))
		{
			fprintf(stderr, "Error reading attribute size\n");
			return BAD_READ;
		}
		if (doflip) flip(&attrSize, sizeof(attrSize));
		if ((attrs = (CSSM_CONTEXT_ATTRIBUTE *)malloc(attrSize)) == NULL)
			return MEM_ERROR;
		/* attributes */
		if (read(fd, attrs, attrSize) != attrSize)
		{
			fprintf(stderr, "Error reading attributes\n");
			return BAD_READ;
		}
		if (doflip)
		{
			ctx.ContextAttributes = reinterpret_cast<CSSM_CONTEXT_ATTRIBUTE *>(attraddr);
			hostorder_CSSM_CONTEXT(&ctx, attrs);
		}
		else
		{
			/* NB: this was the working code before byte-reordering */
			offset = reinterpret_cast<intptr_t>(attrs) - attraddr;
			ReconstituteWalker relocator(offset);
			for (i = 0; i < ctx.NumberOfAttributes; ++i)
			{
				walk(relocator, attrs[i]);
			}
			ctx.ContextAttributes = attrs;
		}
		(void)close(fd);
	}
	else
	{
		int err;
		uint32_t intattr;

		/* 
		 * dummy something up; this is from FakeContext usages in 
		 * securityd/tests/ 
		 */
		ctx.ContextType = CSSM_ALGCLASS_KEYGEN;
		ctx.AlgorithmType = CSSM_ALGID_DES;
#define N_TEST_ATTRS	2
		ctx.NumberOfAttributes = N_TEST_ATTRS;
		attrs = (CSSM_CONTEXT_ATTRIBUTE *)malloc(N_TEST_ATTRS*sizeof(CSSM_CONTEXT_ATTRIBUTE));
		if (!attrs)
			return MEM_ERROR;
		ctx.ContextAttributes = attrs;
		intattr = 64;
		err = fill_CSSM_CONTEXT_ATTRIBUTE(&ctx.ContextAttributes[0],
										  CSSM_ATTRIBUTE_KEY_LENGTH,
										  sizeof(uint32_t),
										  &intattr);
		if (err != OK)
			return err;
		crypto.Param.Length = strlen(testString);
		crypto.Param.Data = (uint8 *)testString;
#if 0
		crypto.Callback = dummyCSSMCallback;
		crypto.CallerCtx = &crypto;	/* dummy cb needs crypto.Param */
#endif
		crypto.Callback = NULL;
		crypto.CallerCtx = NULL;	/* dummy cb needs crypto.Param */
		err = fill_CSSM_CONTEXT_ATTRIBUTE(&ctx.ContextAttributes[1],
										  CSSM_ATTRIBUTE_SEED,
										  sizeof(CSSM_CRYPTO_DATA),
										  (void *)&crypto);
		if (err != OK)
			return err;
		ctx.CSPHandle = 13;
		ctx.Privileged = CSSM_TRUE;	/* ! 0 */
		ctx.EncryptionProhibited = CSSM_TRUE;
		ctx.WorkFactor = 41;
		ctx.Reserved = 0xfeefee;	/* sentry value */
	}

	if ((ret = test_xdrwalk_CSSM_CONTEXT(&ctx, dbglvl)) != OK)
	{
		fprintf(stderr, "%s\n", func);
		return ret;
	}
	return OK;
}

int test_xdrwalk_CSSM_ACL_OWNER_PROTOTYPE(CSSM_ACL_OWNER_PROTOTYPE *aclOwnerPrototype,
										  int dbglvl)
{
	const char *func = "test_xdrwalk_CSSM_ACL_OWNER_PROTOTYPE()";
	CSSM_ACL_OWNER_PROTOTYPE *walkcopy, *xdrcopy;
	void *flattenedAclOwnerPtr = NULL;
	u_int flattenedAclOwnerLen = 0;
	int ret, iter;

	/* save off aclOwnerPrototype because we're going to reuse the pointer */
	walkcopy = reinterpret_cast<CSSM_ACL_OWNER_PROTOTYPE *>(calloc(1, sizeof(CSSM_ACL_OWNER_PROTOTYPE)));
	if (walkcopy == NULL)
	{
		fprintf(stderr, "%s: error allocating walked CSSM_ACL_OWNER_PROTOTYPE\n", func);
		return MEM_ERROR;
	}
	memcpy(walkcopy, aclOwnerPrototype, sizeof(CSSM_ACL_OWNER_PROTOTYPE));
	/* aclOwnerPrototype *is* a walked copy, so no need to re-walk it */

	for (iter = 0; iter < N_ITERS; ++iter)
	{
		if (!xdr_mem_encode(aclOwnerPrototype, &flattenedAclOwnerPtr,
							&flattenedAclOwnerLen, 
							reinterpret_cast<xdrproc_t>(xdr_CSSM_ACL_OWNER_PROTOTYPE)))
		{
			fprintf(stderr, "%s, round %d (encode error)\n", func, iter+1);
			return XDR_ENCODE_ERROR;
		}
		/* always zero out memory before attempting a decode */
		if ((xdrcopy = (CSSM_ACL_OWNER_PROTOTYPE *)calloc(1, sizeof(CSSM_ACL_OWNER_PROTOTYPE))) == NULL)
		{
			fprintf(stderr, "%s, round %d (allocation error)\n", func, iter+1);
			return MEM_ERROR;
		}
		if (!xdr_mem_decode(flattenedAclOwnerPtr, xdrcopy, flattenedAclOwnerLen,
							(xdrproc_t)xdr_CSSM_ACL_OWNER_PROTOTYPE))
		{
			fprintf(stderr, "%s, round %d (decode error)\n", func, iter+1);
			return XDR_DECODE_ERROR;
		}
		if (dbglvl >= 3)
			printf("comparing XDR-generated structs...\n");
		if ((ret = compare_CSSM_ACL_OWNER_PROTOTYPE(aclOwnerPrototype, xdrcopy)) != OK)
		{
			fprintf(stderr, "%s: CSSM_ACL_OWNER_PROTOTYPE old/new XDR comparison, round %d, failed\n", 
					func, iter+1);
			return ret;
		}
		if (dbglvl >= 3)
			printf("comparing walked- and XDR-generated structs...\n");
		if ((ret = compare_CSSM_ACL_OWNER_PROTOTYPE(xdrcopy, walkcopy)) != OK)
		{
			fprintf(stderr, "%s: CSSM_ACL_OWNER_PROTOTYPE XDR/walker comparison, round %d, failed\n", 
					func, iter+1);
			return ret;
		}
		if (dbglvl >= 2)
			printf("CSSM_ACL_OWNER_PROTOTYPE compared OK, round %d\n", iter+1);
		if (iter > 0)
			free(aclOwnerPrototype);
		aclOwnerPrototype = xdrcopy;
		free(flattenedAclOwnerPtr);
		flattenedAclOwnerPtr = NULL;
		flattenedAclOwnerLen = 0;
	}

	if (dbglvl >= 1)
		printf("Successfully finished CSSM_ACL_OWNER_PROTOTYPE check\n");
	return OK;
}

int test_CSSM_ACL_OWNER_PROTOTYPE(int fd, int doflip, int dbglvl)
{
	const char *func = "test_CSSM_ACL_OWNER_PROTOTYPE()";
	CSSM_ACL_OWNER_PROTOTYPE *aclOwnerPrototype;
	int ret;

	if (fd > -1)	/* cheesy hack, but what ya gonna do? */
	{
		int aclsize;
		uint32_t ptrsize;	/* AKA mach_msg_type_number_t, AKA natural_t */
		intptr_t baseptr;
		off_t offset;
		/* 
		 * Saved format: 
		 * - sizeof(base pointer)
		 * - base pointer
		 * - length
		 * - CSSM_ACL_OWNER_PROTOTYPE
		 */
		if (read(fd, &ptrsize, sizeof(ptrsize)) < static_cast<ssize_t>(sizeof(ptrsize)))
		{
			fprintf(stderr, "%s: Error reading base pointer size\n", func);
			return BAD_READ;
		}
		if (doflip) flip(&ptrsize, sizeof(ptrsize));
		if (read(fd, &baseptr, ptrsize) < static_cast<ssize_t>(ptrsize))
		{
			fprintf(stderr, "%s: Error reading base pointer\n", func);
			return BAD_READ;
		}
		if (doflip) flip(&baseptr, sizeof(baseptr));
		if (read(fd, &aclsize, sizeof(aclsize)) < static_cast<ssize_t>(sizeof(aclsize)))
		{
			fprintf(stderr, "%s: Error reading AclOwnerPrototype size\n", func);
			return BAD_READ;
		}
		if (doflip) flip(&aclsize, sizeof(aclsize));
		aclOwnerPrototype = (CSSM_ACL_OWNER_PROTOTYPE *)malloc(aclsize);
		if (aclOwnerPrototype == NULL)
			return MEM_ERROR;
		if (read(fd, aclOwnerPrototype, aclsize) < aclsize)
		{
			fprintf(stderr, "Error reading CSSM_ACL_OWNER_PROTOTYPE\n");
			return BAD_READ;
		}
		offset = reinterpret_cast<intptr_t>(aclOwnerPrototype) - baseptr;
		if (doflip)
		{
			hostorder_CSSM_ACL_OWNER_PROTOTYPE(aclOwnerPrototype, offset);
		}
		else
		{
			ReconstituteWalker relocator(offset);
			walk(relocator, reinterpret_cast<AclOwnerPrototype *&>(baseptr));
		}
		(void)close(fd);
	}
	else
	{
		/* TODO/gh  cobble something up */
		return NOT_IMPLEMENTED;
	}

	ret = test_xdrwalk_CSSM_ACL_OWNER_PROTOTYPE(aclOwnerPrototype, dbglvl);
	if (ret != OK)
		fprintf(stderr, "%s\n", func);
	free(aclOwnerPrototype);
	return ret;
}

int test_xdrwalk_CSSM_ACL_ENTRY_INPUT(CSSM_ACL_ENTRY_INPUT *aclEntryInput, 
									  int dbglvl)
{
	const char *func = "test_xdrwalk_CSSM_ACL_ENTRY_INPUT()";
	CSSM_ACL_ENTRY_INPUT *walkcopy, *xdrcopy;
	void *flattenedAclEIPtr = NULL;
	u_int flattenedAclEILen = 0;
	int ret, iter;

	/* save off aclEntryInput because we're going to reuse the pointer */
	walkcopy = reinterpret_cast<CSSM_ACL_ENTRY_INPUT *>(calloc(1, sizeof(CSSM_ACL_ENTRY_INPUT)));
	if (walkcopy == NULL)
	{
		fprintf(stderr, "%s: error allocating walked CSSM_ACL_ENTRY_INPUT\n", func);
		return MEM_ERROR;
	}
	memcpy(walkcopy, aclEntryInput, sizeof(CSSM_ACL_ENTRY_INPUT));
	/* aclEntryInput *is* a walked copy, so no need to re-walk it */

	for (iter = 0; iter < N_ITERS; ++iter)
	{
		if (!xdr_mem_encode(aclEntryInput, &flattenedAclEIPtr, &flattenedAclEILen, 
							(xdrproc_t)xdr_CSSM_ACL_ENTRY_INPUT))
		{
			fprintf(stderr, "%s, round %d\n", func, iter+1);
			return XDR_ENCODE_ERROR;
		}
		/* always zero out memory before attempting a decode */
		if ((xdrcopy = (CSSM_ACL_ENTRY_INPUT *)calloc(1, sizeof(CSSM_ACL_ENTRY_INPUT))) == NULL)
		{
			fprintf(stderr, "%s, round %d (allocation error)\n", func, iter+1);
			return MEM_ERROR;
		}
		if (!xdr_mem_decode(flattenedAclEIPtr, xdrcopy, flattenedAclEILen,
							(xdrproc_t)xdr_CSSM_ACL_ENTRY_INPUT))
		{
			fprintf(stderr, "%s, round %d\n", func, iter+1);
			return XDR_DECODE_ERROR;
		}
		if (dbglvl >= 3)
			printf("comparing XDR-generated structs...\n");
		if ((ret = compare_CSSM_ACL_ENTRY_INPUT(aclEntryInput, xdrcopy)) != OK)
		{
			fprintf(stderr, "%s: CSSM_ACL_ENTRY_INPUT old/new XDR comparison, round %d, failed\n", 
					func, iter+1);
			return ret;
		}
		if (dbglvl >= 3)
			printf("comparing walked- and XDR-generated structs...\n");
		if ((ret = compare_CSSM_ACL_ENTRY_INPUT(xdrcopy, walkcopy)) != OK)
		{
			fprintf(stderr, "%s: CSSM_ACL_ENTRY_INPUT XDR/walker comparison, round %d, failed\n", 
					func, iter+1);
			return ret;
		}
		if (dbglvl >= 2)
			printf("CSSM_ACL_ENTRY_INPUT compared OK, round %d\n", iter+1);
		if (iter > 0)
			free(aclEntryInput);
		aclEntryInput = xdrcopy;
		free(flattenedAclEIPtr);
		flattenedAclEIPtr = NULL;
		flattenedAclEILen = 0;
	}

	if (dbglvl >= 1)
		printf("Successfully finished CSSM_ACL_ENTRY_INPUT check\n");
	return OK;
}

int test_CSSM_ACL_ENTRY_INPUT(int fd, int doflip, int dbglvl)
{
	const char *func = "test_CSSM_ACL_ENTRY_INPUT()";
	CSSM_ACL_ENTRY_INPUT *aclEntryInput;
	int ret;

	if (fd > -1)	/* cheesy hack, but what ya gonna do? */
	{
		int aclsize;
		uint32_t ptrsize;	/* AKA mach_msg_type_number_t, AKA natural_t */
		intptr_t baseptr;
		off_t offset;
		/* 
		 * Saved format: 
		 * - sizeof(base pointer)
		 * - base pointer
		 * - length
		 * - CSSM_ACL_ENTRY_INPUT
		 */
		if (read(fd, &ptrsize, sizeof(ptrsize)) < static_cast<ssize_t>(sizeof(ptrsize)))
		{
			fprintf(stderr, "%s: Error reading base pointer size\n", func);
			return BAD_READ;
		}
		if (doflip) flip(&ptrsize, sizeof(ptrsize));
		if (read(fd, &baseptr, ptrsize) < static_cast<ssize_t>(ptrsize))
		{
			fprintf(stderr, "%s: Error reading base pointer\n", func);
			return BAD_READ;
		}
		if (doflip) flip(&baseptr, sizeof(baseptr));
		if (read(fd, &aclsize, sizeof(aclsize)) < static_cast<ssize_t>(sizeof(aclsize)))
		{
			fprintf(stderr, "%s: Error reading AclEntryInput size\n", func);
			return BAD_READ;
		}
		if (doflip) flip(&aclsize, sizeof(aclsize));
		aclEntryInput = (CSSM_ACL_ENTRY_INPUT *)malloc(aclsize);
		if (aclEntryInput == NULL)
			return MEM_ERROR;
		if (read(fd, aclEntryInput, aclsize) < aclsize)
		{
			fprintf(stderr, "Error reading CSSM_ACL_ENTRY_INPUT\n");
			return BAD_READ;
		}
		offset = reinterpret_cast<intptr_t>(aclEntryInput) - baseptr;
		if (doflip)
		{
			hostorder_CSSM_ACL_ENTRY_INPUT(aclEntryInput, offset);
		}
		else
		{
			ReconstituteWalker relocator(offset);
			walk(relocator, reinterpret_cast<AclEntryInput *&>(baseptr));
		}
		(void)close(fd);
	}
	else
	{
		/* TODO/gh  cobble something up */
		fprintf(stderr, "%s: hard-coded test not implemented yet\n", func);
		return NOT_IMPLEMENTED;
	}

	if ((ret = test_xdrwalk_CSSM_ACL_ENTRY_INPUT(aclEntryInput, dbglvl)) != OK)
		fprintf(stderr, "%s\n", func);
	free(aclEntryInput);
	return ret;
}

int test_xdrwalk_CSSM_ACL_ENTRY_INFO(CSSM_ACL_ENTRY_INFO *aclEntryInfo,
									 int dbglvl)
{
	const char *func = "test_xdrwalk_CSSM_ACL_ENTRY_INFO()";

	CSSM_ACL_ENTRY_INFO *walkcopy, *xdrcopy;
	void *flattenedAclEIPtr = NULL;
	u_int flattenedAclEILen = 0;
	int ret, iter;

	/* save off aclEntryInfo because we're going to reuse the pointer */
	walkcopy = reinterpret_cast<CSSM_ACL_ENTRY_INFO *>(calloc(1, sizeof(CSSM_ACL_ENTRY_INFO)));
	if (walkcopy == NULL)
	{
		fprintf(stderr, "%s: error allocating walked CSSM_ACL_ENTRY_INFO\n", func);
		return MEM_ERROR;
	}
	memcpy(walkcopy, aclEntryInfo, sizeof(CSSM_ACL_ENTRY_INFO));
	/* right now aclEntryInfo *is* a walked copy, so no need to re-walk it */
	for (iter = 0; iter < N_ITERS; ++iter)
	{
		if (!xdr_mem_encode(aclEntryInfo, &flattenedAclEIPtr, &flattenedAclEILen, 
							(xdrproc_t)xdr_CSSM_ACL_ENTRY_INFO))
		{
			fprintf(stderr, "%s, round %d\n", func, iter+1);
			return XDR_ENCODE_ERROR;
		}
		/* always zero out memory before attempting a decode */
		if ((xdrcopy = (CSSM_ACL_ENTRY_INFO *)calloc(1, sizeof(CSSM_ACL_ENTRY_INFO))) == NULL)
		{
			fprintf(stderr, "%s, round %d (allocation error)\n", func, iter+1);
			return MEM_ERROR;
		}
		if (!xdr_mem_decode(flattenedAclEIPtr, xdrcopy, flattenedAclEILen,
							(xdrproc_t)xdr_CSSM_ACL_ENTRY_INFO))
		{
			fprintf(stderr, "%s, round %d\n", func, iter+1);
			return XDR_DECODE_ERROR;
		}
		if (dbglvl >= 3)
			printf("comparing XDR-generated structs...\n");
		if ((ret = compare_CSSM_ACL_ENTRY_INFO(aclEntryInfo, xdrcopy)) != OK)
		{
			fprintf(stderr, "%s: CSSM_ACL_ENTRY_INFO old/new XDR comparison, round %d, failed\n", 
					func, iter+1);
			return ret;
		}
		if (dbglvl >= 3)
			printf("comparing walked- and XDR-generated structs...\n");
		if ((ret = compare_CSSM_ACL_ENTRY_INFO(xdrcopy, walkcopy)) != OK)
		{
			fprintf(stderr, "%s: CSSM_ACL_ENTRY_INFO XDR/walker comparison, round %d, failed\n", 
					func, iter+1);
			return ret;
		}
		if (dbglvl >= 2)
			printf("CSSM_ACL_ENTRY_INFO compared OK, round %d\n", iter+1);
		if (iter > 0)
			free(aclEntryInfo);
		aclEntryInfo = xdrcopy;
		free(flattenedAclEIPtr);
		flattenedAclEIPtr = NULL;
		flattenedAclEILen = 0;
	}
	if (dbglvl >= 1)
		printf("Successfully finished CSSM_ACL_ENTRY_INFO check\n");
	return OK;
}

int test_CSSM_ACL_ENTRY_INFO(int fd, int doflip, int dbglvl)
{
	const char *func = "test_CSSM_ACL_ENTRY_INFO()";
	CSSM_ACL_ENTRY_INFO *aclEntryInfo;
	int ret, aclsize;

	if (fd > -1)	/* cheesy hack, but what ya gonna do? */
	{
		uint32_t ptrsize;	/* AKA mach_msg_type_number_t, AKA natural_t */
		intptr_t baseptr;
		off_t offset;
		/* 
		 * Saved format (v. 1 of saved-data protocol): 
		 * - preamble
		 * - sizeof(base pointer)
		 * - base pointer
		 * - length
		 * - CSSM_ACL_ENTRY_INFO
		 */

		if (read(fd, &ptrsize, sizeof(ptrsize)) < static_cast<ssize_t>(sizeof(ptrsize)))
		{
			fprintf(stderr, "%s: Error reading base pointer size\n", func);
			return BAD_READ;
		}
		if (doflip) flip(&ptrsize, sizeof(ptrsize));
		if (read(fd, &baseptr, ptrsize) < static_cast<ssize_t>(ptrsize))
		{
			fprintf(stderr, "%s: Error reading base pointer\n", func);
			return BAD_READ;
		}
		if (doflip) flip(&baseptr, sizeof(baseptr));
		if (read(fd, &aclsize, sizeof(aclsize)) < static_cast<ssize_t>(sizeof(aclsize)))
		{
			fprintf(stderr, "%s: Error reading AclEntryInput size\n", func);
			return BAD_READ;
		}
		if (doflip) flip(&aclsize, sizeof(aclsize));
		aclEntryInfo = (CSSM_ACL_ENTRY_INFO *)malloc(aclsize);
		if (aclEntryInfo == NULL)
			return MEM_ERROR;
		if (read(fd, aclEntryInfo, aclsize) < aclsize)
		{
			fprintf(stderr, "Error reading CSSM_ACL_ENTRY_INFO\n");
			return BAD_READ;
		}
		offset = reinterpret_cast<intptr_t>(aclEntryInfo) - baseptr;
		if (doflip)
		{
			hostorder_CSSM_ACL_ENTRY_INFO(aclEntryInfo, offset);
		}
		else
		{
			ReconstituteWalker relocator(offset);
			walk(relocator, reinterpret_cast<AclEntryInput *&>(baseptr));
		}
		(void)close(fd);
	}
	else
	{
		/* TODO/gh  cobble something up */
		fprintf(stderr, "%s: hard-coded test not implemented yet\n", func);
		return NOT_IMPLEMENTED;
	}
	if ((ret = test_xdrwalk_CSSM_ACL_ENTRY_INFO(aclEntryInfo, dbglvl)) != OK)
		fprintf(stderr, "%s\n", func);
	free(aclEntryInfo);
	return ret;
}

int test_xdrwalk_CSSM_QUERY(CSSM_QUERY *query, int dbglvl)
{
	const char *func = "test_xdrwalk_CSSM_QUERY()";

	CSSM_QUERY *walkcopy, *xdrcopy;
	void *flattenedQueryPtr = NULL;
	u_int flattenedQueryLen = 0;
	int ret, iter;

	/* save off query because we're going to reuse the pointer */
	walkcopy = reinterpret_cast<CSSM_QUERY *>(calloc(1, sizeof(CSSM_QUERY)));
	if (walkcopy == NULL)
	{
		fprintf(stderr, "%s: error allocating walked CSSM_QUERY\n", func);
		return MEM_ERROR;
	}
	memcpy(walkcopy, query, sizeof(CSSM_QUERY));
	/* right now query *is* a walked copy, so no need to re-walk it */
	for (iter = 0; iter < N_ITERS; ++iter)
	{
		if (!xdr_mem_encode(query, &flattenedQueryPtr, &flattenedQueryLen, 
							(xdrproc_t)xdr_CSSM_QUERY))
		{
			fprintf(stderr, "%s, round %d\n", func, iter+1);
			return XDR_ENCODE_ERROR;
		}
		/* always zero out memory before attempting a decode */
		if ((xdrcopy = (CSSM_QUERY *)calloc(1, sizeof(CSSM_QUERY))) == NULL)
		{
			fprintf(stderr, "%s, round %d (allocation error)\n", func, iter+1);
			return MEM_ERROR;
		}
		if (!xdr_mem_decode(flattenedQueryPtr, xdrcopy, flattenedQueryLen,
							(xdrproc_t)xdr_CSSM_QUERY))
		{
			fprintf(stderr, "%s, round %d\n", func, iter+1);
			return XDR_DECODE_ERROR;
		}
		if (dbglvl >= 3)
			printf("comparing XDR-generated structs...\n");
		if ((ret = compare_CSSM_QUERY(query, xdrcopy)) != OK)
		{
			fprintf(stderr, "%s: CSSM_QUERY old/new XDR comparison, round %d, failed\n", 
					func, iter+1);
			return ret;
		}
		if (dbglvl >= 3)
			printf("comparing walked- and XDR-generated structs...\n");
		if ((ret = compare_CSSM_QUERY(xdrcopy, walkcopy)) != OK)
		{
			fprintf(stderr, "%s: CSSM_QUERY XDR/walker comparison, round %d, failed\n", 
					func, iter+1);
			return ret;
		}
		if (dbglvl >= 2)
			printf("CSSM_QUERY compared OK, round %d\n", iter+1);
		if (iter > 0)
			free(query);
		query = xdrcopy;
		free(flattenedQueryPtr);
		flattenedQueryPtr = NULL;
		flattenedQueryLen = 0;
	}
	if (dbglvl >= 1)
		printf("Successfully finished CSSM_QUERY check\n");
	return OK;
}

int test_CSSM_QUERY(int fd, int doflip, int dbglvl)
{
	const char *func = "test_CSSM_QUERY()";
	CSSM_QUERY *query;
	int ret, querysize;

	if (fd > -1)	/* cheesy hack, but what ya gonna do? */
	{
		uint32_t ptrsize;	/* AKA mach_msg_type_number_t, AKA natural_t */
		intptr_t baseptr;
		off_t offset;
		ssize_t readPtr;
		int nq = 0;		/* # of queries */
		/* 
		 * Saved format (v. 1 of saved-data protocol): 
		 * - preamble
		 * - sizeof(base pointer)
		 * - base pointer
		 * - length
		 * - CSSM_QUERY
		 */
		do 
		{
			if (nq)		/* first readPreamble() was in main() */
			{
				/* dummy vars -- let the func params govern */
				uint32_t d1, d2, d3 = 0;
				if ((ret = readPreamble(fd, &d1, &d2, &d3)) != OK)
				{
					if (ret == READ_EOF)
					{
						readPtr = -1;
						continue;
					}
					return ret;
				}
			}
			readPtr = read(fd, &ptrsize, sizeof(ptrsize));
			if (readPtr == 0) break;	/* we're done */
			if (readPtr < static_cast<ssize_t>(sizeof(ptrsize)))
			{
				fprintf(stderr, "%s: Error reading base pointer size\n", func);
				return BAD_READ;
			}
			if (doflip) flip(&ptrsize, sizeof(ptrsize));
			if (read(fd, &baseptr, ptrsize) < static_cast<ssize_t>(ptrsize))
			{
				fprintf(stderr, "%s: Error reading base pointer\n", func);
				return BAD_READ;
			}
			if (doflip) flip(&baseptr, sizeof(baseptr));
			if (read(fd, &querysize, sizeof(querysize)) < static_cast<ssize_t>(sizeof(querysize)))
			{
				fprintf(stderr, "%s: Error reading CSSM_QUERY size\n", func);
				return BAD_READ;
			}
			if (doflip) flip(&querysize, sizeof(querysize));
			query = (CSSM_QUERY *)malloc(querysize);
			if (query == NULL)
				return MEM_ERROR;
			if (read(fd, query, querysize) < querysize)
			{
				fprintf(stderr, "Error reading CSSM_QUERY\n");
				return BAD_READ;
			}
			offset = reinterpret_cast<intptr_t>(query) - baseptr;
			if (doflip)
			{
				hostorder_CSSM_QUERY(query, offset);
			}
			else
			{
				ReconstituteWalker relocator(offset);
				walk(relocator, reinterpret_cast<CssmQuery *&>(baseptr));
			}
			++nq;
			if (dbglvl >= 2)
				printf("%s: read a new CSSM_QUERY (%d)\n", func, nq);
			if ((ret = test_xdrwalk_CSSM_QUERY(query, dbglvl)) != OK)
				fprintf(stderr, "%s\n", func);
			free(query);
		} 
		while (readPtr != -1);
		(void)close(fd);
	}
	else
	{
		/* TODO/gh  cobble something up */
		fprintf(stderr, "%s: hard-coded test not implemented yet\n", func);
		return NOT_IMPLEMENTED;
	}
	return OK;
}

void usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [-c|-I|-i] <FILE>\n", progname);
	fprintf(stderr, "    FILE is binary data saved from securityd\n");
	fprintf(stderr, "    -c\trun hard-coded CSSM_CONTEXT test\n");
	fprintf(stderr, "    -i\trun hard-coded CSSM_ACL_ENTRY_INFO test\n");
	fprintf(stderr, "    -I\trun hard-coded CSSM_ACL_ENTRY_INPUT test\n");
	fprintf(stderr, "    -v\tverbose (more -v options mean more output\n");
	fprintf(stderr, "    If FILE is not provided, %s will try to run any requested hard-coded test\n", progname);
}

int main(int ac, char **av)
{
	const char *optstring = "chIiv";
	char *infile = NULL;
	int c, fd, ret = OK, debuglevel = 0; 
	uint32_t doflip, version, type = 0;

	while ((c = getopt(ac, av, optstring)) != EOF)
	{
		switch(c)
		{
			case 'c':
				type = SecuritydDataSave::CONTEXT;
				break;
			case 'I':
				type = SecuritydDataSave::ACL_ENTRY_INPUT;
				break;
			case 'i':
				type = SecuritydDataSave::ACL_ENTRY_INFO;
				break;
			case 'h':
				usage(av[0]);
				exit(0);
				break;
			case 'v':
				debuglevel++;
				break;
			default:
				break;
		}
	}
	ac -= optind;
	av += optind;
	if (ac >= 1)
	{
		infile = av[0];		/* XXX/gh  need to validate av[0]? */
		if ((fd = open(infile, O_RDONLY, 0)) < 0)
		{
			fprintf(stderr, "Couldn't open %s (%s)\n", infile, strerror(errno));
			return 2;
		}
		if ((ret = readPreamble(fd, &doflip, &version, &type)) != OK)
			return ret;
	}
	else
	{
		fd = -1;				/* use a hard-coded test */
		if (!type)
		{
			type = SecuritydDataSave::CONTEXT;		/* the only test that has hard-coded data...*/
			fprintf(stderr, "*** running hard-coded CSSM_CONTEXT test\n");
		}
	}
	switch (type)
	{
		case SecuritydDataSave::CONTEXT:
			ret = test_CSSM_CONTEXT(fd, doflip, debuglevel);
			break;
		case SecuritydDataSave::ACL_OWNER_PROTOTYPE:
			ret = test_CSSM_ACL_OWNER_PROTOTYPE(fd, doflip, debuglevel);
			break;
		case SecuritydDataSave::ACL_ENTRY_INPUT:
			ret = test_CSSM_ACL_ENTRY_INPUT(fd, doflip, debuglevel);
			break;
		case SecuritydDataSave::ACL_ENTRY_INFO:
			ret = test_CSSM_ACL_ENTRY_INFO(fd, doflip, debuglevel);
			break;
		case SecuritydDataSave::QUERY:
			ret = test_CSSM_QUERY(fd, doflip, debuglevel);
			break;
		default:
			fprintf(stderr, "Unrecognized test\n");
			ret = NOT_IMPLEMENTED;
	}
	return ret;
}
