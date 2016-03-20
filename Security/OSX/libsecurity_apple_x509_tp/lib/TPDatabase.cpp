/*
 * Copyright (c) 2002-2009,2011-2012,2014 Apple Inc. All Rights Reserved.
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
 * TPDatabase.cpp - TP's DL/DB access functions.
 *
 */

#include <Security/cssmtype.h>
#include <Security/cssmapi.h>
#include <security_cdsa_utilities/Schema.h>		/* private API */
#include <security_keychain/TrustKeychains.h>	/* private SecTrustKeychainsGetMutex() */
#include <Security/SecCertificatePriv.h>		/* private SecInferLabelFromX509Name() */
#include <Security/oidscert.h>
#include "TPDatabase.h"
#include "tpdebugging.h"
#include "certGroupUtils.h"
#include "TPCertInfo.h"
#include "TPCrlInfo.h"
#include "tpCrlVerify.h"
#include "tpTime.h"


/*
 * Given a DL/DB, look up cert by subject name. Subsequent
 * certs can be found using the returned result handle.
 */
static CSSM_DB_UNIQUE_RECORD_PTR tpCertLookup(
	CSSM_DL_DB_HANDLE	dlDb,
	const CSSM_DATA		*subjectName,	// DER-encoded
	CSSM_HANDLE_PTR		resultHand,		// RETURNED
	CSSM_DATA_PTR		cert)			// RETURNED
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;

	cert->Data = NULL;
	cert->Length = 0;

	/* SWAG until cert schema nailed down */
	predicate.DbOperator = CSSM_DB_EQUAL;
	predicate.Attribute.Info.AttributeNameFormat =
		CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = (char*) "Subject";
	predicate.Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	predicate.Attribute.Value = const_cast<CSSM_DATA_PTR>(subjectName);
	predicate.Attribute.NumberOfValues = 1;

	query.RecordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	query.SelectionPredicate = &predicate;
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = 0;				// FIXME - used?

	CSSM_DL_DataGetFirst(dlDb,
		&query,
		resultHand,
		NULL,				// don't fetch attributes
		cert,
		&record);

	return record;
}

/*
 * Search a list of DBs for a cert which verifies specified subject item.
 * Just a boolean return - we found it, or not. If we did, we return
 * TPCertInfo associated with the raw cert.
 * A true partialIssuerKey on return indicates that caller must deal
 * with partial public key processing later.
 * If verifyCurrent is true, we will not return a cert which is not
 * temporally valid; else we may well do so.
 */
TPCertInfo *tpDbFindIssuerCert(
	Allocator				&alloc,
	CSSM_CL_HANDLE			clHand,
	CSSM_CSP_HANDLE			cspHand,
	const TPClItemInfo		*subjectItem,
	const CSSM_DL_DB_LIST	*dbList,
	const char 				*verifyTime,		// may be NULL
	bool					&partialIssuerKey,	// RETURNED
    TPCertInfo              *oldRoot)
{
	StLock<Mutex> _(SecTrustKeychainsGetMutex());

	uint32						dbDex;
	CSSM_HANDLE					resultHand;
	CSSM_DATA					cert;
	CSSM_DL_DB_HANDLE			dlDb;
	CSSM_DB_UNIQUE_RECORD_PTR	record;
	TPCertInfo 					*issuerCert = NULL;
	bool 						foundIt;
	TPCertInfo					*expiredIssuer = NULL;
	TPCertInfo					*nonRootIssuer = NULL;

	partialIssuerKey = false;
	if(dbList == NULL) {
		return NULL;
	}
	for(dbDex=0; dbDex<dbList->NumHandles; dbDex++) {
		dlDb = dbList->DLDBHandle[dbDex];
		cert.Data = NULL;
		cert.Length = 0;
		resultHand = 0;
		record = tpCertLookup(dlDb,
			subjectItem->issuerName(),
			&resultHand,
			&cert);
		/* remember we have to:
		 * -- abort this query regardless, and
		 * -- free the CSSM_DATA cert regardless, and
		 * -- free the unique record if we don't use it
		 *    (by placing it in issuerCert)...
		 */
		if(record != NULL) {
			/* Found one */
			assert(cert.Data != NULL);
			tpDbDebug("tpDbFindIssuerCert: found cert record (1) %p", record);
			issuerCert = NULL;
			CSSM_RETURN crtn = CSSM_OK;
			try {
				issuerCert = new TPCertInfo(clHand, cspHand, &cert, TIC_CopyData, verifyTime);
			}
			catch(...) {
				crtn = CSSMERR_TP_INVALID_CERTIFICATE;
			}

			/* we're done with raw cert data */
			tpFreePluginMemory(dlDb.DLHandle, cert.Data);
			cert.Data = NULL;
			cert.Length = 0;

			/* Does it verify the subject cert? */
			if(crtn == CSSM_OK) {
				crtn = subjectItem->verifyWithIssuer(issuerCert);
			}

			/*
			 * Handle temporal invalidity - if so and this is the first one
			 * we've seen, hold on to it while we search for better one.
			 */
			if(crtn == CSSM_OK) {
				if(issuerCert->isExpired() || issuerCert->isNotValidYet()) {
					/*
					 * Exact value not important here, this just uniquely identifies
					 * this situation in the switch below.
					 */
					tpDbDebug("tpDbFindIssuerCert: holding expired cert (1)");
					crtn = CSSM_CERT_STATUS_EXPIRED;
					/* Delete old stashed expired issuer */
					if (expiredIssuer) {
						expiredIssuer->freeUniqueRecord();
						delete expiredIssuer;
					}
					expiredIssuer = issuerCert;
					expiredIssuer->dlDbHandle(dlDb);
					expiredIssuer->uniqueRecord(record);
				}
			}
			/*
			 * Prefer a root over an intermediate issuer if we can get one
			 * (in case a cross-signed intermediate and root are both available)
			 */
			if(crtn == CSSM_OK && !issuerCert->isSelfSigned()) {
				/*
				 * Exact value not important here, this just uniquely identifies
				 * this situation in the switch below.
				 */
				tpDbDebug("tpDbFindIssuerCert: holding non-root cert (1)");
				crtn = CSSM_CERT_STATUS_IS_ROOT;
				/*
				 * If the old intermediate was temporally invalid, replace it.
				 * (Regardless of temporal validity of new one we found, because
				 * as far as this code is concerned they're equivalent.)
				 */
				if(!nonRootIssuer ||
				   (nonRootIssuer && (nonRootIssuer->isExpired() || nonRootIssuer->isNotValidYet()))) {
					if(nonRootIssuer) {
						nonRootIssuer->freeUniqueRecord();
						delete nonRootIssuer;
					}
					nonRootIssuer = issuerCert;
					nonRootIssuer->dlDbHandle(dlDb);
					nonRootIssuer->uniqueRecord(record);
				}
				else {
					delete issuerCert;
					CSSM_DL_FreeUniqueRecord(dlDb, record);
					issuerCert = NULL;
				}
			}
			switch(crtn) {
				case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:
					partialIssuerKey = true;
					break;
                case CSSM_OK:
                    if((oldRoot == NULL) ||
                       !tp_CompareCerts(issuerCert->itemData(), oldRoot->itemData())) {
                        /* We found a new root cert which does not match the old one */
                        break;
                    }
                    /* else fall through to search for a different one */
				default:
					if(issuerCert != NULL) {
						/* either holding onto this cert, or done with it. */
						if(crtn != CSSM_CERT_STATUS_EXPIRED &&
						   crtn != CSSM_CERT_STATUS_IS_ROOT) {
							delete issuerCert;
							CSSM_DL_FreeUniqueRecord(dlDb, record);
						}
						issuerCert = NULL;
					}

					/*
					 * Continue searching this DB. Break on finding the holy
					 * grail or no more records found.
					 */
					for(;;) {
						cert.Data = NULL;
						cert.Length = 0;
						record = NULL;
						CSSM_RETURN crtn = CSSM_DL_DataGetNext(dlDb,
							resultHand,
							NULL,		// no attrs
							&cert,
							&record);
						if(crtn) {
							/* no more, done with this DB */
							assert(cert.Data == NULL);
							break;
						}
						assert(cert.Data != NULL);
						tpDbDebug("tpDbFindIssuerCert: found cert record (2) %p", record);

						/* found one - does it verify subject? */
						try {
							issuerCert = new TPCertInfo(clHand, cspHand, &cert, TIC_CopyData,
									verifyTime);
						}
						catch(...) {
							crtn = CSSMERR_TP_INVALID_CERTIFICATE;
						}
						/* we're done with raw cert data */
						tpFreePluginMemory(dlDb.DLHandle, cert.Data);
						cert.Data = NULL;
						cert.Length = 0;

						if(crtn == CSSM_OK) {
							crtn = subjectItem->verifyWithIssuer(issuerCert);
						}

						/* temporal validity check, again */
						if(crtn == CSSM_OK) {
							if(issuerCert->isExpired() || issuerCert->isNotValidYet()) {
								tpDbDebug("tpDbFindIssuerCert: holding expired cert (2)");
								crtn = CSSM_CERT_STATUS_EXPIRED;
								/* Delete old stashed expired issuer */
								if (expiredIssuer) {
									expiredIssuer->freeUniqueRecord();
									delete expiredIssuer;
								}
								expiredIssuer = issuerCert;
								expiredIssuer->dlDbHandle(dlDb);
								expiredIssuer->uniqueRecord(record);
							}
						}
						/* self-signed check, again */
						if(crtn == CSSM_OK && !issuerCert->isSelfSigned()) {
							tpDbDebug("tpDbFindIssuerCert: holding non-root cert (2)");
							crtn = CSSM_CERT_STATUS_IS_ROOT;
							/*
							 * If the old intermediate was temporally invalid, replace it.
							 * (Regardless of temporal validity of new one we found, because
							 * as far as this code is concerned they're equivalent.)
							 */
							if(!nonRootIssuer ||
							   (nonRootIssuer && (nonRootIssuer->isExpired() || nonRootIssuer->isNotValidYet()))) {
								if(nonRootIssuer) {
									nonRootIssuer->freeUniqueRecord();
									delete nonRootIssuer;
								}
								nonRootIssuer = issuerCert;
								nonRootIssuer->dlDbHandle(dlDb);
								nonRootIssuer->uniqueRecord(record);
							}
							else {
								delete issuerCert;
								CSSM_DL_FreeUniqueRecord(dlDb, record);
								issuerCert = NULL;
							}
						}

						foundIt = false;
						switch(crtn) {
							case CSSM_OK:
                                /* duplicate check, again */
                                if((oldRoot == NULL) ||
                                   !tp_CompareCerts(issuerCert->itemData(), oldRoot->itemData())) {
                                    foundIt = true;
                                }
                                break;
							case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:
								partialIssuerKey = true;
								foundIt = true;
								break;
							default:
								break;
						}
						if(foundIt) {
							/* yes! */
							break;
						}
						if(issuerCert != NULL) {
							/* either holding onto this cert, or done with it. */
							if(crtn != CSSM_CERT_STATUS_EXPIRED &&
							   crtn != CSSM_CERT_STATUS_IS_ROOT) {
								delete issuerCert;
								CSSM_DL_FreeUniqueRecord(dlDb, record);
							}
							issuerCert = NULL;
						}
					} /* searching subsequent records */
			}	/* switch verify */

			if(record != NULL) {
				/* NULL record --> end of search --> DB auto-aborted */
				crtn = CSSM_DL_DataAbortQuery(dlDb, resultHand);
				assert(crtn == CSSM_OK);
			}
			if(issuerCert != NULL) {
				/* successful return */
				tpDbDebug("tpDbFindIssuer: returning record %p", record);
				issuerCert->dlDbHandle(dlDb);
				issuerCert->uniqueRecord(record);
				if(expiredIssuer != NULL) {
					/* We found a replacement */
					tpDbDebug("tpDbFindIssuer: discarding expired cert");
					expiredIssuer->freeUniqueRecord();
					delete expiredIssuer;
				}
				/* Avoid deleting the non-root cert if same as expired cert */
				if(nonRootIssuer != NULL && nonRootIssuer != expiredIssuer) {
					/* We found a replacement */
					tpDbDebug("tpDbFindIssuer: discarding non-root cert");
					nonRootIssuer->freeUniqueRecord();
					delete nonRootIssuer;
				}
				return issuerCert;
			}
		}	/* tpCertLookup, i.e., CSSM_DL_DataGetFirst, succeeded */
		else {
			assert(cert.Data == NULL);
			assert(resultHand == 0);
		}
	}	/* main loop searching dbList */

	if(nonRootIssuer != NULL) {
		/* didn't find root issuer, so use this one */
		tpDbDebug("tpDbFindIssuer: taking non-root issuer cert, record %p",
			nonRootIssuer->uniqueRecord());
		if(expiredIssuer != NULL && expiredIssuer != nonRootIssuer) {
			expiredIssuer->freeUniqueRecord();
			delete expiredIssuer;
		}
		return nonRootIssuer;
	}

	if(expiredIssuer != NULL) {
		/* OK, we'll take this one */
		tpDbDebug("tpDbFindIssuer: taking expired cert after all, record %p",
			expiredIssuer->uniqueRecord());
		return expiredIssuer;
	}
	/* issuer not found */
	return NULL;
}

/*
 * Given a DL/DB, look up CRL by issuer name and validity time.
 * Subsequent CRLs can be found using the returned result handle.
 */
#define SEARCH_BY_DATE		1

static CSSM_DB_UNIQUE_RECORD_PTR tpCrlLookup(
	CSSM_DL_DB_HANDLE	dlDb,
	const CSSM_DATA		*issuerName,	// DER-encoded
	CSSM_TIMESTRING 	verifyTime,		// may be NULL, implies "now"
	CSSM_HANDLE_PTR		resultHand,		// RETURNED
	CSSM_DATA_PTR		crl)			// RETURNED
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		pred[3];
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	char							timeStr[CSSM_TIME_STRLEN + 1];

	crl->Data = NULL;
	crl->Length = 0;

	/* Three predicates...first, the issuer name */
	pred[0].DbOperator = CSSM_DB_EQUAL;
	pred[0].Attribute.Info.AttributeNameFormat =
		CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	pred[0].Attribute.Info.Label.AttributeName = (char*) "Issuer";
	pred[0].Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	pred[0].Attribute.Value = const_cast<CSSM_DATA_PTR>(issuerName);
	pred[0].Attribute.NumberOfValues = 1;

	/* now before/after. Cook up an appropriate time string. */
	if(verifyTime != NULL) {
		/* Caller spec'd tolerate any format */
		int rtn = tpTimeToCssmTimestring(verifyTime, (unsigned)strlen(verifyTime), timeStr);
		if(rtn) {
			tpErrorLog("tpCrlLookup: Invalid VerifyTime string\n");
			return NULL;
		}
	}
	else {
		/* right now */
		StLock<Mutex> _(tpTimeLock());
		timeAtNowPlus(0, TIME_CSSM, timeStr);
	}
	CSSM_DATA timeData;
	timeData.Data = (uint8 *)timeStr;
	timeData.Length = CSSM_TIME_STRLEN;

	#if SEARCH_BY_DATE
	pred[1].DbOperator = CSSM_DB_LESS_THAN;
	pred[1].Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	pred[1].Attribute.Info.Label.AttributeName = (char*) "NextUpdate";
	pred[1].Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	pred[1].Attribute.Value = &timeData;
	pred[1].Attribute.NumberOfValues = 1;

	pred[2].DbOperator = CSSM_DB_GREATER_THAN;
	pred[2].Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	pred[2].Attribute.Info.Label.AttributeName = (char*) "ThisUpdate";
	pred[2].Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	pred[2].Attribute.Value = &timeData;
	pred[2].Attribute.NumberOfValues = 1;
	#endif

	query.RecordType = CSSM_DL_DB_RECORD_X509_CRL;
	query.Conjunctive = CSSM_DB_AND;
	#if SEARCH_BY_DATE
	query.NumSelectionPredicates = 3;
	#else
	query.NumSelectionPredicates = 1;
	#endif
	query.SelectionPredicate = pred;
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = 0;				// FIXME - used?

	CSSM_DL_DataGetFirst(dlDb,
		&query,
		resultHand,
		NULL,				// don't fetch attributes
		crl,
		&record);
	return record;
}

/*
 * Search a list of DBs for a CRL from the specified issuer and (optional)
 * TPVerifyContext.verifyTime.
 * Just a boolean return - we found it, or not. If we did, we return a
 * TPCrlInfo which has been verified with the specified TPVerifyContext.
 */
TPCrlInfo *tpDbFindIssuerCrl(
	TPVerifyContext		&vfyCtx,
	const CSSM_DATA		&issuer,
	TPCertInfo			&forCert)
{
	StLock<Mutex> _(SecTrustKeychainsGetMutex());

	uint32						dbDex;
	CSSM_HANDLE					resultHand;
	CSSM_DATA					crl;
	CSSM_DL_DB_HANDLE			dlDb;
	CSSM_DB_UNIQUE_RECORD_PTR	record;
	TPCrlInfo 					*issuerCrl = NULL;
	CSSM_DL_DB_LIST_PTR 		dbList = vfyCtx.dbList;
	CSSM_RETURN					crtn;

	if(dbList == NULL) {
		return NULL;
	}
	for(dbDex=0; dbDex<dbList->NumHandles; dbDex++) {
		dlDb = dbList->DLDBHandle[dbDex];
		crl.Data = NULL;
		crl.Length = 0;
		record = tpCrlLookup(dlDb,
			&issuer,
			vfyCtx.verifyTime,
			&resultHand,
			&crl);
		/* remember we have to:
		 * -- abort this query regardless, and
		 * -- free the CSSM_DATA crl regardless, and
		 * -- free the unique record if we don't use it
		 *    (by placing it in issuerCert)...
		 */
		if(record != NULL) {
			/* Found one */
			assert(crl.Data != NULL);
			issuerCrl = new TPCrlInfo(vfyCtx.clHand,
				vfyCtx.cspHand,
				&crl,
				TIC_CopyData,
				vfyCtx.verifyTime);
			/* we're done with raw CRL data */
			/* FIXME this assumes that vfyCtx.alloc is the same as the
			 * allocator associated with DlDB...OK? */
			tpFreeCssmData(vfyCtx.alloc, &crl, CSSM_FALSE);
			crl.Data = NULL;
			crl.Length = 0;

			/* and we're done with the record */
			CSSM_DL_FreeUniqueRecord(dlDb, record);

			/* Does it verify with specified context? */
			crtn = issuerCrl->verifyWithContextNow(vfyCtx, &forCert);
			if(crtn) {

				delete issuerCrl;
				issuerCrl = NULL;

				/*
				 * Verify fail. Continue searching this DB. Break on
				 * finding the holy grail or no more records found.
				 */
				for(;;) {
					crl.Data = NULL;
					crl.Length = 0;
					crtn = CSSM_DL_DataGetNext(dlDb,
						resultHand,
						NULL,		// no attrs
						&crl,
						&record);
					if(crtn) {
						/* no more, done with this DB */
						assert(crl.Data == NULL);
						break;
					}
					assert(crl.Data != NULL);

					/* found one - is it any good? */
					issuerCrl = new TPCrlInfo(vfyCtx.clHand,
						vfyCtx.cspHand,
						&crl,
						TIC_CopyData,
						vfyCtx.verifyTime);
					/* we're done with raw CRL data */
					/* FIXME this assumes that vfyCtx.alloc is the same as the
					* allocator associated with DlDB...OK? */
					tpFreeCssmData(vfyCtx.alloc, &crl, CSSM_FALSE);
					crl.Data = NULL;
					crl.Length = 0;

					CSSM_DL_FreeUniqueRecord(dlDb, record);

					crtn = issuerCrl->verifyWithContextNow(vfyCtx, &forCert);
					if(crtn == CSSM_OK) {
						/* yes! */
						break;
					}
					delete issuerCrl;
					issuerCrl = NULL;
				} /* searching subsequent records */
			}	/* verify fail */
			/* else success! */

			if(issuerCrl != NULL) {
				/* successful return */
				CSSM_DL_DataAbortQuery(dlDb, resultHand);
				tpDebug("tpDbFindIssuerCrl: found CRL record %p", record);
				return issuerCrl;
			}
		}	/* tpCrlLookup, i.e., CSSM_DL_DataGetFirst, succeeded */
		else {
			assert(crl.Data == NULL);
		}
		/* in any case, abort the query for this db */
		CSSM_DL_DataAbortQuery(dlDb, resultHand);

	}	/* main loop searching dbList */

	/* issuer not found */
	return NULL;
}

