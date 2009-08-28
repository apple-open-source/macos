/*
 * Copyright (c) 2009 Apple Inc. All Rights Reserved.
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

#include "ODBridge.h"

static const char* kEmailName = "Alias";
static const char* kPrintName = "PrintName";
static const char *dbname =  "Open Directory Data Library";
static long long OD_query_time_out = 5LL * NSEC_PER_SEC;  // 5 second timeout for all OD queries.

// If we want to get more specific translations for OD Errors this would be the place.  For now we'll settle for CSSMERR_DL_INTERNAL_ERROR
CSSM_RETURN cssme_for_OD(CFErrorRef ODReturn)
{
	switch (CFErrorGetCode(ODReturn))
	{
		case 0:
			return CSSM_OK;
		default:
			return CSSMERR_DL_INTERNAL_ERROR;
	}
}


DirectoryService::DirectoryService()
{
	this->db_name = (char *)dbname;
	CFErrorRef ODReturn;
	if((this->node = ODNodeCreateWithNodeType(NULL, kODSessionDefault, kODNodeTypeContacts, &ODReturn)) == NULL)
		throw DirectoryServiceException (ODReturn);
}

DirectoryService::~DirectoryService()
{	
	/* Walk the list of results and close them all. */

	CFIndex i, count;
	for (i = 0, count = CFArrayGetCount(this->all_open_queries); i < count; i++) {
		ODdl_results_handle result;
		result = (ODdl_results_handle) CFArrayGetValueAtIndex(this->all_open_queries, i);
		CFRelease(result->query);
		CFRelease(result->certificates);
	}
	CFRelease(this->node);
}

// Print the Cert Bytes

#define LINEWIDTH 80
#define BYTESPERBLOB 4
#define BUFFERSIZE (LINEWIDTH+1)
#define BLOBSPERLINE (LINEWIDTH / ((BYTESPERBLOB * 2) +1))
#define BYTESPERLINE (BLOBSPERLINE * BYTESPERBLOB)


// These two functions handle the callbacks from the OD query - cert_query_callback is called first with a record entry; getCertsFromArray can take multiple cert returns from that entry.
static void
getCertsFromArray(const void *value, void *context)
{
	ODdl_results_handle results = (ODdl_results_handle) context;
	CFRetain((CFDataRef) value);
	CFArrayAppendValue(results->certificates, value);
}

void
cert_query_callback(ODQueryRef query, CFArrayRef qresults, CFErrorRef error, void *context)
{
	ODdl_results_handle results = (ODdl_results_handle) context;
	
	if (qresults == NULL) {
		if(error == NULL) {
			if(results->results_done) dispatch_semaphore_signal(results->results_done);
		}
		return;
	}
	
	CFIndex i, count;
	for (i = 0, count = CFArrayGetCount(qresults); i < count; i++) {
		ODRecordRef rec = (ODRecordRef)CFArrayGetValueAtIndex(qresults, i);
		CFArrayRef certs = ODRecordCopyValues(rec, kODAttributeTypeUserCertificate, &error);
		if(certs && CFArrayGetCount(certs)) {
			CFArrayApplyFunction(certs, CFRangeMake( 0, CFArrayGetCount(certs)) , getCertsFromArray, context);
			CFRelease(certs);
		}
	}
}

// simplistic e-mail address filter

static bool isValidEmailString(CFStringRef s)
{
	char buf[256];
	
	if(CFStringGetCString(s, buf, 256, kCFStringEncodingASCII) == 0) return false;
	if(CFStringGetLength(s) < 10) return false;  // must be at least 3 chars (arbitrary) of local, an @ sign, then 3 letters (arbitrary) dot(.) 2 letters (like in .ru)
	if(index(buf, '@') == NULL) return false;
	if(index(buf, '.') == NULL) return false;
	return true;
}

ODdl_results_handle
DirectoryService::translate_cssm_query_to_OD_query(const CSSM_QUERY *Query, CSSM_RETURN *error)
{
	ODMatchType  matchType;
	CFIndex      ODmaxResults = 0;
	ODdl_results_handle results;
	int	i, searchPred;
	CFErrorRef ODerror;
	
	*error = 0;

	// make sure we're asked to look for something we can find.
	CSSM_SELECTION_PREDICATE_PTR pred;
	for (i=0, searchPred = -1; searchPred == -1 && i < (int) Query->NumSelectionPredicates; i++) {
		pred = (CSSM_SELECTION_PREDICATE_PTR) &(Query->SelectionPredicate[i]);
		if(pred->Attribute.Info.AttributeNameFormat == CSSM_DB_ATTRIBUTE_NAME_AS_STRING) {
			if(strncmp(pred->Attribute.Info.Label.AttributeName, kEmailName, strlen(kEmailName)) == 0 ||
			   strncmp(pred->Attribute.Info.Label.AttributeName, kEmailName, strlen(kPrintName)) == 0) {
				searchPred = i;
			}
		}
	}

	if (searchPred == -1) {
		*error = CSSMERR_DL_INVALID_QUERY;
		return NULL;
	}
	
	pred = (CSSM_SELECTION_PREDICATE_PTR) &(Query->SelectionPredicate[searchPred]);
	if (pred->Attribute.NumberOfValues != 1) {
		*error = CSSMERR_DL_INVALID_QUERY;
		return NULL;
	}
	
	// translate cssm comparisons into OD comparisons - we will only ever match - no fishing allowed.
	switch(pred->DbOperator) {
		case CSSM_DB_EQUAL:
		case CSSM_DB_LESS_THAN:
		case CSSM_DB_GREATER_THAN:
		case CSSM_DB_CONTAINS:
		case CSSM_DB_CONTAINS_INITIAL_SUBSTRING:
		case CSSM_DB_CONTAINS_FINAL_SUBSTRING:
			matchType = kODMatchEqualTo;
			break;
		default:
			*error = CSSMERR_DL_INVALID_QUERY;
			return NULL;
	}	
	
	if((results = (ODdl_results_handle) malloc(sizeof(struct ODdl_results))) == NULL) {
		*error = CSSMERR_DL_MEMORY_ERROR;
		return NULL;
	}
	
	// Bookkeeping for this query
	results->recordid = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	results->certificates = CFArrayCreateMutable(NULL, 20, &kCFTypeArrayCallBacks);
	results->currentRecord = 0;
	results->results_done = dispatch_semaphore_create(0);

	// Get remaining query parameters
	if (Query->QueryLimits.SizeLimit != CSSM_QUERY_SIZELIMIT_NONE) ODmaxResults = Query->QueryLimits.SizeLimit;
	results->searchString = CFStringCreateWithBytes(NULL, pred->Attribute.Value->Data, pred->Attribute.Value->Length, kCFStringEncodingUTF8, false);	
	
	// Make sure the e-mail address looks sane.  Keychain Access at least will bombard us with character-by-character lookups and we don't want to spam the server until we have something reasonable.
	if(!isValidEmailString(results->searchString)) {
		*error = CSSMERR_DL_ENDOFDATA;
		return NULL;
	}

	// Create the query looking for the e-mail address and returning that certificate.
	if((results->query = ODQueryCreateWithNode(NULL, this->node, kODRecordTypeUsers, kODAttributeTypeEMailAddress, matchType, results->searchString, kODAttributeTypeUserCertificate, ODmaxResults, &ODerror)) == NULL) {
		*error =  cssme_for_OD(ODerror);
		return NULL;
	}
	
	// Prepare callback for query
	ODQuerySetCallback(results->query, cert_query_callback, (void *)results);
	
	// Setup a dispatch queue and go - the semaphore will ping us when the last result is retrieved or after the timeout OD_query_time_out
	this->query_dispatch_queue = dispatch_queue_create("com.apple.ldapdl", NULL);
	ODQuerySetDispatchQueue( results->query, this->query_dispatch_queue );
	dispatch_semaphore_wait(results->results_done, dispatch_time(DISPATCH_TIME_NOW, OD_query_time_out));
	CFRelease(results->query);

	return results;
}

// return certificates sequentially as if they are individual records.
CFDataRef
DirectoryService::getNextCertFromResults(ODdl_results_handle results)
{
	CFDataRef	retval;
	if(!results) return NULL;
	if(!results->certificates) return NULL;
	
	if(results->currentRecord < CFArrayGetCount(results->certificates)) {
		retval = (CFDataRef) CFArrayGetValueAtIndex(results->certificates, results->currentRecord);
		results->currentRecord++;
	} else {
		return NULL;
	}
	return retval;
}

