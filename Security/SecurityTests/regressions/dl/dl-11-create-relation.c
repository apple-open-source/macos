/*
 * Copyright (c) 2005 Apple Computer, Inc. All Rights Reserved.
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
 * 01DL_CreateReleation.c
 */

#include <stdlib.h>
#include <unistd.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>

#include "testmore.h"
#include "testenv.h"
#include "testcssm.h"

#define DBNAME "testdl"

CSSM_APPLEDL_OPEN_PARAMETERS openParameters =
{
	sizeof(CSSM_APPLEDL_OPEN_PARAMETERS),
	CSSM_APPLEDL_OPEN_PARAMETERS_VERSION,
	CSSM_FALSE,
	kCSSM_APPLEDL_MASK_MODE,
	0600
};
CSSM_DBINFO dbInfo =
{
	0 /* NumberOfRecordTypes */,
	NULL,
	NULL,
	NULL,
	CSSM_TRUE /* IsLocal */,
	NULL, /* AccessPath - URL, dir path, etc. */
	NULL /* reserved */
};
CSSM_DB_SCHEMA_ATTRIBUTE_INFO attributeInfo[] =
{
	{
		1,
		"One",
		{},
		CSSM_DB_ATTRIBUTE_FORMAT_STRING
	},
	{
		2,
		"Two",
		{},
		CSSM_DB_ATTRIBUTE_FORMAT_STRING
	}
};
CSSM_DB_SCHEMA_INDEX_INFO indexInfo[] =
{
	{
		1,
		0,
		CSSM_DB_INDEX_UNIQUE,
		CSSM_DB_INDEX_ON_ATTRIBUTE
	},
	{
		1,
		1,
		CSSM_DB_INDEX_NONUNIQUE,
		CSSM_DB_INDEX_ON_ATTRIBUTE
	},
	{
		2,
		2,
		CSSM_DB_INDEX_NONUNIQUE,
		CSSM_DB_INDEX_ON_ATTRIBUTE
	}
};
CSSM_DATA values[] =
{
	{ 5, (uint8 *)"value" }
};
CSSM_DB_ATTRIBUTE_DATA attributeData[] =
{
	{
		{
			CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
			{ "One" },
			CSSM_DB_ATTRIBUTE_FORMAT_STRING
		},
		sizeof(values) / sizeof(CSSM_DATA),
		values
	},
	{
		{
			CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
			{ "Two" },
			CSSM_DB_ATTRIBUTE_FORMAT_STRING
		},
		sizeof(values) / sizeof(CSSM_DATA),
		values
	}
};
CSSM_DB_RECORD_ATTRIBUTE_DATA attributes =
{
	42,
	0,
	sizeof(attributeData) / sizeof(CSSM_DB_ATTRIBUTE_DATA),
	attributeData
};

int
static test1(CSSM_DL_HANDLE dl)
{
    int pass = 1;
    CSSM_DL_DB_HANDLE dldb = { dl };
    CSSM_DB_UNIQUE_RECORD_PTR uniqueId;

    pass &= ok_status(CSSM_DL_DbCreate(dl, DBNAME, NULL /* DbLocation */,
                  &dbInfo,
                  CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
                  NULL /* CredAndAclEntry */,
                  &openParameters,
                  &dldb.DBHandle),
        "CSSM_DL_DbCreate");
    
    pass &= is_status(CSSM_DL_DataInsert(dldb,
        attributes.DataRecordType,
        &attributes,
        NULL,
        &uniqueId),
        CSSMERR_DL_INVALID_RECORDTYPE, "CSSM_DL_DataInsert no table");
    
    pass &= ok_status(CSSM_DL_CreateRelation(dldb,
        42,
        "Fourty Two",
        sizeof(attributeInfo) / sizeof(CSSM_DB_SCHEMA_ATTRIBUTE_INFO),
        attributeInfo,
        sizeof(indexInfo) / sizeof(CSSM_DB_SCHEMA_INDEX_INFO),
        indexInfo), "CSSM_DL_CreateRelation");

    pass &= ok_status(CSSM_DL_DataInsert(dldb,
        attributes.DataRecordType,
        &attributes,
        NULL,
        &uniqueId), "CSSM_DL_DataInsert");
        
    pass &= is_status(CSSM_DL_DataInsert(dldb,
        attributes.DataRecordType,
        &attributes,
        NULL,
        &uniqueId),
        CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA, "CSSM_DL_DataInsert dupe");
    
    pass &= ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
        "CSSM_DL_FreeUniqueRecord");
            
    pass &= ok_status(CSSM_DL_DbClose(dldb),
        "CSSM_DL_DbClose");

    return pass;
}   

int
static test2(CSSM_DL_HANDLE dl)
{
	int pass = 1;
	CSSM_DL_DB_HANDLE dldb = { dl };
	CSSM_DB_UNIQUE_RECORD_PTR uniqueId;

    pass &= ok_status(CSSM_DL_DbCreate(dl, DBNAME, NULL /* DbLocation */,
                  &dbInfo,
                  CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
                  NULL /* CredAndAclEntry */,
                  NULL,
                  &dldb.DBHandle),
        "CSSM_DL_DbCreate");

    pass &= ok_status(CSSM_DL_DbClose(dldb),
        "CSSM_DL_DbClose");
    
	pass &= ok_status(CSSM_DL_DbOpen(dl, DBNAME, NULL /* DbLocation */,
                  CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
                  NULL /* CredAndAclEntry */,
                  &openParameters,
                  &dldb.DBHandle),
		"CSSM_DL_DbOpen");

	pass &= is_status(CSSM_DL_DataInsert(dldb,
		attributes.DataRecordType,
		&attributes,
		NULL,
		&uniqueId),
		CSSMERR_DL_INVALID_RECORDTYPE, "CSSM_DL_DataInsert no table");

	pass &= ok_status(CSSM_DL_CreateRelation(dldb,
		42,
		"Fourty Two",
		sizeof(attributeInfo) / sizeof(CSSM_DB_SCHEMA_ATTRIBUTE_INFO),
		attributeInfo,
		sizeof(indexInfo) / sizeof(CSSM_DB_SCHEMA_INDEX_INFO),
		indexInfo), "CSSM_DL_CreateRelation");

	pass &= ok_status(CSSM_DL_DataInsert(dldb,
		attributes.DataRecordType,
		&attributes,
		NULL,
		&uniqueId), "CSSM_DL_DataInsert fails unless 4039735 is fixed");

	pass &= is_status(CSSM_DL_DataInsert(dldb,
		attributes.DataRecordType,
		&attributes,
		NULL,
		&uniqueId),
		CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA,
		"CSSM_DL_DataInsert dupe");

	pass &= ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
		"CSSM_DL_FreeUniqueRecord");

	pass &= ok_status(CSSM_DL_DbClose(dldb),
		"CSSM_DL_DbDelete");

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

	int pass = 1;

	plan_tests(23);

	CSSM_DL_HANDLE dl;
	pass &= ok(cssm_attach(guid, &dl), "cssm_attach");
	pass &= ok(tests_begin(argc, argv), "tests_begin");

	pass &= ok(test1(dl), "insert record in new table with ac off");
	pass &= ok_status(CSSM_DL_DbDelete(dl, DBNAME, NULL /* DbLocation */,
		NULL /* AccessCred */), "CSSM_DL_DbDelete");
	pass &= ok(test2(dl),
		"insert record in existing db in new table with ac off");
	pass &= ok_status(CSSM_DL_DbDelete(dl, DBNAME, NULL /* DbLocation */,
		NULL /* AccessCred */), "CSSM_DL_DbDelete");
	pass &= ok(cssm_detach(guid, dl), "cssm_detach");

	return !tests_end(pass);
}
