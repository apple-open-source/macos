/*
 * Copyright (c) 2005-2006 Apple Computer, Inc. All Rights Reserved.
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
 *
 * 00DL_Create-Delete.c
 */

#include <stdlib.h>
#include <unistd.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>

#include "testmore.h"
#include "testenv.h"
#include "testcssm.h"
#include "testleaks.h"

#define DBNAME "testdl.db"

static CSSM_APPLEDL_OPEN_PARAMETERS openParameters =
{
	sizeof(CSSM_APPLEDL_OPEN_PARAMETERS),
	CSSM_APPLEDL_OPEN_PARAMETERS_VERSION,
	CSSM_FALSE,
	kCSSM_APPLEDL_MASK_MODE,
	0600
};
static CSSM_DBINFO dbInfo =
{
	0 /* NumberOfRecordTypes */,
	NULL,
	NULL,
	NULL,
	CSSM_TRUE /* IsLocal */,
	NULL, /* AccessPath - URL, dir path, etc. */
	NULL /* reserved */
};

int
static test1(CSSM_DL_HANDLE dl)
{
	int pass = 1;
	CSSM_DL_DB_HANDLE dldb = { dl };

	/* Case 1 regular create close delete. */
	pass &= ok_status(CSSM_DL_DbCreate(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */,
			&dbInfo,
			CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
			NULL /* CredAndAclEntry */,
			NULL /* &openParameters */,
			&dldb.DBHandle),
		"CSSM_DL_DbCreate");

	pass &= ok_status(CSSM_DL_DbClose(dldb), "CSSM_DL_DbClose");
	pass &= ok_status(CSSM_DL_DbDelete(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */,
			NULL /* AccessCred */), "CSSM_DL_DbDelete");
	pass &= is_unix(unlink(DBNAME), ENOENT, "unlink");

	return pass;
}

int
static test2(CSSM_DL_HANDLE dl)
{
	int pass = 1;
	CSSM_DL_DB_HANDLE dldb = { dl };

	/* Case 2 regular create delete then close. */
	pass &= ok_status(CSSM_DL_DbCreate(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */,
			&dbInfo,
			CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
			NULL /* CredAndAclEntry */,
			NULL /* &openParameters */,
			&dldb.DBHandle),
		"CSSM_DL_DbCreate");

	pass &= ok_status(CSSM_DL_DbDelete(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */,
			NULL /* AccessCred */), "CSSM_DL_DbDelete");
	pass &= ok_status(CSSM_DL_DbClose(dldb), "CSSM_DL_DbClose");
	pass &= is_unix(unlink(DBNAME), ENOENT, "unlink");

	return pass;
}

int
static test3(CSSM_DL_HANDLE dl)
{
	int pass = 1;
	CSSM_DL_DB_HANDLE dldb = { dl };

	/* Case 3 non autocommit create close delete. */
	pass &= ok_status(CSSM_DL_DbCreate(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */,
			&dbInfo,
			CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
			NULL /* CredAndAclEntry */,
			&openParameters,
			&dldb.DBHandle),
		"CSSM_DL_DbCreate");

	pass &= ok_status(CSSM_DL_DbClose(dldb), "CSSM_DL_DbClose");
	pass &= ok_status(CSSM_DL_DbDelete(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */,
			NULL /* AccessCred */), "CSSM_DL_DbDelete");
	pass &= is_unix(unlink(DBNAME), ENOENT, "unlink");

	return pass;
}

int
static test4(CSSM_DL_HANDLE dl)
{
	int pass = 1;
	CSSM_DL_DB_HANDLE dldb = { dl };

	/* Case 4 non autocommit create delete then close. */
	pass &= ok_status(CSSM_DL_DbCreate(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */,
			&dbInfo,
			CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
			NULL /* CredAndAclEntry */,
			&openParameters,
			&dldb.DBHandle),
		"CSSM_DL_DbCreate");

	pass &= ok_status(CSSM_DL_DbDelete(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */,
			NULL /* AccessCred */), "CSSM_DL_DbDelete");
	pass &= ok_status(CSSM_DL_DbClose(dldb), "CSSM_DL_DbClose");
	pass &= is_unix(unlink(DBNAME), ENOENT, "unlink");

	return pass;
}

int
static test5(CSSM_DL_HANDLE dl)
{
	int pass = 1;
	CSSM_DL_DB_HANDLE dldb = { dl };

	/* Case 5 non autocommit create rollback close delete. */
	pass &= ok_status(CSSM_DL_DbCreate(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */,
			&dbInfo,
			CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
			NULL /* CredAndAclEntry */,
			&openParameters,
			&dldb.DBHandle),
		"CSSM_DL_DbCreate");
	pass &= ok_status(CSSM_DL_PassThrough(dldb, CSSM_APPLEFILEDL_ROLLBACK,
			NULL, NULL), "CSSM_APPLEFILEDL_ROLLBACK");
	pass &= ok_status(CSSM_DL_DbClose(dldb), "CSSM_DL_DbClose");
	pass &= is_status(CSSM_DL_DbDelete(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */, NULL /* AccessCred */),
		CSSMERR_DL_DATASTORE_DOESNOT_EXIST, "CSSM_DL_DbDelete");
	pass &= is_unix(unlink(DBNAME), ENOENT, "unlink");

	return pass;
}

int
static test6(CSSM_DL_HANDLE dl)
{
	int pass = 1;
	CSSM_DL_DB_HANDLE dldb = { dl };

	/* Case 6 non autocommit create rollback delete then close. */
	pass &= ok_status(CSSM_DL_DbCreate(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */,
			&dbInfo,
			CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
			NULL /* CredAndAclEntry */,
			&openParameters,
			&dldb.DBHandle),
		"CSSM_DL_DbCreate");

	pass &= ok_status(CSSM_DL_PassThrough(dldb, CSSM_APPLEFILEDL_ROLLBACK,
			NULL, NULL), "CSSM_APPLEFILEDL_ROLLBACK");
	pass &= is_status(CSSM_DL_DbDelete(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */, NULL /* AccessCred */),
		CSSMERR_DL_DATASTORE_DOESNOT_EXIST, "CSSM_DL_DbDelete");
	pass &= ok_status(CSSM_DL_DbClose(dldb), "CSSM_DL_DbClose");
	pass &= is_unix(unlink(DBNAME), ENOENT, "unlink");

	return pass;
}

int
static test7(CSSM_DL_HANDLE dl)
{
	int pass = 1;
	CSSM_DL_DB_HANDLE dldb = { dl };

	/* Case 7 non autocommit create delete rollback then close. */
	pass &= ok_status(CSSM_DL_DbCreate(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */,
			&dbInfo,
			CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
			NULL /* CredAndAclEntry */,
			&openParameters,
			&dldb.DBHandle),
		"CSSM_DL_DbCreate");

	pass &= ok_status(CSSM_DL_DbDelete(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */, NULL /* AccessCred */), "CSSM_DL_DbDelete");
	pass &= ok_status(CSSM_DL_PassThrough(dldb, CSSM_APPLEFILEDL_ROLLBACK,
			NULL, NULL), "CSSM_APPLEFILEDL_ROLLBACK");
	pass &= ok_status(CSSM_DL_DbClose(dldb), "CSSM_DL_DbClose");
	pass &= is_unix(unlink(DBNAME), ENOENT, "unlink");

	return pass;
}

int
static test8(CSSM_DL_HANDLE dl)
{
	int pass = 1;
	CSSM_DL_DB_HANDLE dldb = { dl };

	/* Case 8 delete non existant db. */
	pass &= is_status(CSSM_DL_DbDelete(dldb.DLHandle, DBNAME,
			NULL /* DbLocation */, NULL /* AccessCred */),
		CSSMERR_DL_DATASTORE_DOESNOT_EXIST, "CSSM_DL_DbDelete");


	return pass;
}

int
main(int argc, char * const *argv)
{
	int guid_alt = argc > 1 && !strcmp(argv[1], "-g"); 
	/* {2cb56191-ee6f-432d-a377-853d3c6b949e} */
	CSSM_GUID s3dl_guid =
	{
		0x2cb56191, 0xee6f, 0x432d,
		{ 0xa3, 0x77, 0x85, 0x3d, 0x3c, 0x6b, 0x94, 0x9e }
	};
	const CSSM_GUID *guid = guid_alt ? & s3dl_guid : &gGuidAppleFileDL;
	CSSM_DL_HANDLE dl;

	/* Total number of test cases in this file. */
	plan_tests(45);

	ok(cssm_attach(guid, &dl), "cssm_attach");
	ok(tests_begin(argc, argv), "setup");

	/* Run tests. */
	ok(test1(dl), "create close delete");
	ok(test2(dl), "create delete close");
	ok(test3(dl), "autocommit off create close delete");
	ok(test4(dl), "autocommit off create delete close");
	ok(test5(dl), "autocommit off create rollback close delete");
	ok(test6(dl), "autocommit off create rollback delete close");
	ok(test7(dl), "autocommit off create delete rollback close");
	ok(test8(dl), "delete non existant db");

	ok(cssm_detach(guid, dl), "cssm_detach");
	ok(tests_end(1), "cleanup");
	TODO: {ok_leaks("leaks");}
	
	return 0;
}
