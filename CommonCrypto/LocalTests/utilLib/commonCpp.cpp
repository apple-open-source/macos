//
// throw  C++-dependent stuff in here
//
#include <stdio.h>
#include <Security/cssm.h>
#include "common.h"
#include <Security/SecBasePriv.h>
#include <security_cdsa_client/keychainacl.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_client/aclclient.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmalloc.h>
#include <security_utilities/devrandom.h>
#include <CoreFoundation/CFString.h>
#include "cssmErrorStrings.h"		/* generated error string table */

/*
 * Log CSSM error.
 */
void printError(const char *op, CSSM_RETURN err)
{
	cssmPerror(op, err);
}

const char *cssmErrToStr(CSSM_RETURN err)
{
	const ErrString *esp;
	
	for(esp=errStrings; esp->errStr!=NULL; esp++) {
		if(esp->errCode == err) {
			return esp->errStr;
		}
	}
	
	static char outbuf[512];
	sprintf(outbuf, "UNKNOWN ERROR CODE %d", (int)err);
	return outbuf;
}


/*
 * Open a DB, optionally:
 *
 *		-- ensuring it's empty
 *		-- creating it 
 *		-- Specifying optional password to avoid SecurityAgent UI.
 */
CSSM_RETURN dbCreateOpen(
	CSSM_DL_HANDLE		dlHand,			// from dlStartup()
	const char 			*dbName,
	CSSM_BOOL			doCreate,		// if false, must already exist	
	CSSM_BOOL			deleteExist,
	const char			*pwd,			// optional
	CSSM_DB_HANDLE		*dbHand)
{
	CSSM_RETURN		crtn;
	CSSM_DBINFO		dbInfo;
	
	if(deleteExist) {
		/* first delete possible existing DB, ignore error */
		crtn = dbDelete(dlHand, dbName);
		switch(crtn) {
			/* only allowed error is "no such file" */
			case CSSM_OK:
			case CSSMERR_DL_DATASTORE_DOESNOT_EXIST:
				break;
			default:
				printError("CSSM_DL_DbDelete", crtn);
				return crtn;
		}
		if(!doCreate) {
			printf("***Hey! dbCreateOpen with deleteExist and !doCreate\n");
			exit(1);
		}
	}
	else {
		/* 
		 * Try to open existing DB. This does not have a means
		 * to specify password (yet). 
		 */
		crtn = CSSM_DL_DbOpen(dlHand,
			dbName, 
			NULL,			// DbLocation
			CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
			NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
			NULL,			// void *OpenParameters
			dbHand);
		if(crtn == CSSM_OK) {
			return crtn;
		}
		if(!doCreate) {
			printError("CSSM_DL_DbOpen", crtn);
			printf("Error opening %s\n", dbName);
			return crtn;
		}
	}
	memset(&dbInfo, 0, sizeof(CSSM_DBINFO));
	
	/* now create it */
	if(pwd) {
		/*
		 * This glorious code copied from crlRefresh. I didn't pretend
		 * to understand it when I put it there either.
		 */
		Allocator &alloc = Allocator::standard();
		CssmClient::AclFactory::PasswordChangeCredentials 
			pCreds((StringData(pwd)), alloc);
		const AccessCredentials* aa = pCreds;
		TypedList subject(alloc, CSSM_ACL_SUBJECT_TYPE_ANY);
		AclEntryPrototype protoType(subject);
		AuthorizationGroup &authGroup = protoType.authorization();
		CSSM_ACL_AUTHORIZATION_TAG tag = CSSM_ACL_AUTHORIZATION_ANY;
		authGroup.NumberOfAuthTags = 1;
		authGroup.AuthTags = &tag;
	
		const ResourceControlContext rcc(protoType, 
			const_cast<AccessCredentials *>(aa));

		crtn = CSSM_DL_DbCreate(dlHand, 
			dbName,
			NULL,						// DbLocation
			&dbInfo,
			// &Security::KeychainCore::Schema::DBInfo,
			CSSM_DB_ACCESS_PRIVILEGED,
			&rcc,						// CredAndAclEntry
			NULL,						// OpenParameters
			dbHand);
	}
	else {
		crtn = CSSM_DL_DbCreate(dlHand, 
			dbName,
			NULL,						// DbLocation
			&dbInfo,
			// &Security::KeychainCore::Schema::DBInfo,
			CSSM_DB_ACCESS_PRIVILEGED,
			NULL,						// CredAndAclEntry
			NULL,						// OpenParameters
			dbHand);
	}
	if(crtn) {
		printError("CSSM_DL_DbCreate", crtn);
	}
	return crtn;
}

/*
 * *The* way for all tests to get random data.
 */
void appGetRandomBytes(void *buf, unsigned len)
{
	try {
		Security::DevRandomGenerator devRand(false);
		devRand.random(buf, len);
	}
	catch(...) {
		printf("***Hey! DevRandomGenerator threw an exception!\n");
		/* Yes, exit - I'd really like to catch one of these */
		exit(1);
	}
}
