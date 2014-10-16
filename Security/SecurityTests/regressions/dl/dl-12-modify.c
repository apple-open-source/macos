/*
 * Copyright (c) 2005-2006,2010 Apple Inc. All Rights Reserved.
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
#include "testleaks.h"

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
		"string-1",
		{},
		CSSM_DB_ATTRIBUTE_FORMAT_STRING
	},
	{
		2,
		"sint32-2",
		{},
		CSSM_DB_ATTRIBUTE_FORMAT_SINT32
	},
	{
		3,
		"uint32-3",
		{},
		CSSM_DB_ATTRIBUTE_FORMAT_UINT32
	},
	{
		4,
		"big_num-4",
		{},
		CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM
	},
	{
		5,
		"real-5",
		{},
		CSSM_DB_ATTRIBUTE_FORMAT_REAL
	},
	{
		6,
		"time-date-6",
		{},
		CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE
	},
	{
		7,
		"blob-7",
		{},
		CSSM_DB_ATTRIBUTE_FORMAT_BLOB
	},
	{
		8,
		"multi-uint32-8",
		{},
		CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32
	},
#if 0
	/* @@@ DL bug if you create a relation with a 
	   CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX type attribute it succeeds but
	   subsequent inserts in that table fail.  */
	{
		9,
		"complex-9",
		{},
		CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX
	}
#endif
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
CSSM_DATA values_str_1[] =
{
	{ 7, (uint8 *)"value-1" }
};
CSSM_DATA values_str_2[] =
{
	{ 7, (uint8 *)"value-2" }
};
CSSM_DATA values_sint32_1[] =
{
	{ sizeof(sint32), (uint8 *)"1111" }
};
CSSM_DATA values_sint32_2[] =
{
	{ sizeof(sint32), (uint8 *)"2222" }
};

CSSM_DB_ATTRIBUTE_DATA attributeData[] =
{
	{
		{
			CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER,
			{ (char *)((uint64_t)1<<32|1) },
			CSSM_DB_ATTRIBUTE_FORMAT_STRING
		},
		sizeof(values_str_1) / sizeof(CSSM_DATA),
		values_str_1
	},
	{
		{
			CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER,
			{ (char *)((uint64_t)2<<32|2) },
			CSSM_DB_ATTRIBUTE_FORMAT_SINT32
		},
		sizeof(values_sint32_1) / sizeof(CSSM_DATA),
		values_sint32_1
	}
};
CSSM_DB_RECORD_ATTRIBUTE_DATA attributes =
{
	42,
	0x00008000,
	sizeof(attributeData) / sizeof(CSSM_DB_ATTRIBUTE_DATA),
	attributeData
};

CSSM_DB_ATTRIBUTE_DATA newAttributeData[] =
{
	{
		{
			CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER,
			{ (char *)((uint64_t)1<<32|1) },
			CSSM_DB_ATTRIBUTE_FORMAT_STRING
		},
		sizeof(values_str_2) / sizeof(CSSM_DATA),
		values_str_2
	},
	{
		{
			CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER,
			{ (char *)((uint64_t)2<<32|2) },
			CSSM_DB_ATTRIBUTE_FORMAT_SINT32
		},
		sizeof(values_sint32_2) / sizeof(CSSM_DATA),
		values_sint32_2
	}
};
CSSM_DB_RECORD_ATTRIBUTE_DATA newAttributes =
{
	42,
	0x80000001, /* Semantic Information. */
	sizeof(newAttributeData) / sizeof(CSSM_DB_ATTRIBUTE_DATA),
	newAttributeData
};

static void free_attributes_data(const CSSM_API_MEMORY_FUNCS *memfuncs,
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes, CSSM_DATA_PTR data)
{
	if (data && data->Data)
	{
		memfuncs->free_func(data->Data, memfuncs->AllocRef);
		data->Data = NULL;
	}

	if (attributes && attributes->AttributeData)
	{
		uint32 aix;
		for (aix = 0; aix < attributes->NumberOfAttributes; ++aix)
		{
			if (attributes->AttributeData[aix].Value)
			{
				uint32 vix;
				for (vix = 0;
					vix < attributes->AttributeData[aix].NumberOfValues; ++vix)
				{
					if (attributes->AttributeData[aix].Value[vix].Data)
					{
						memfuncs->free_func(
                            attributes->AttributeData[aix].Value[vix].Data,
							memfuncs->AllocRef);
					}
				}

				memfuncs->free_func(attributes->AttributeData[aix].Value,
					memfuncs->AllocRef);
				attributes->AttributeData[aix].NumberOfValues = 0;
				attributes->AttributeData[aix].Value = NULL;
			}
		}
	}
}

static int test_is_attributes_data(
	const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes1, const CSSM_DATA *data1,
	const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes2, const CSSM_DATA *data2,
	const char *description, const char *directive,
	const char *reason, const char *file, unsigned line)
{
	if (attributes1 || attributes2)
	{
		if (!attributes1 || !attributes2)
			return test_ok(0, description, directive, reason, file, line,
				"#             got CSSM_DB_RECORD_ATTRIBUTE_DATA %p\n"
				"#        expected CSSM_DB_RECORD_ATTRIBUTE_DATA %p\n",
				attributes1, attributes2);

		if (attributes1->DataRecordType != attributes2->DataRecordType ||
			attributes1->SemanticInformation !=
				attributes2->SemanticInformation ||
			attributes1->NumberOfAttributes != attributes2->NumberOfAttributes)
			return test_ok(0, description, directive, reason, file, line,
				"#         got CSSM_DB_RECORD_ATTRIBUTE_DATA:\n"
				"#         DataRecordType: %08x\n"
				"#    SemanticInformation: %08x\n"
				"#     NumberOfAttributes: %lu\n"
				"#    expected CSSM_DB_RECORD_ATTRIBUTE_DATA:\n"
				"#         DataRecordType: %08x\n"
				"#    SemanticInformation: %08x\n"
				"#     NumberOfAttributes: %lu\n",
				attributes1->DataRecordType,
				attributes1->SemanticInformation,
				attributes1->NumberOfAttributes,
				attributes2->DataRecordType,
				attributes2->SemanticInformation,
				attributes2->NumberOfAttributes);
		uint32 ai;
		for (ai = 0; ai < attributes1->NumberOfAttributes; ++ai)
		{
			const CSSM_DB_ATTRIBUTE_DATA *a1 = &attributes1->AttributeData[ai];
			const CSSM_DB_ATTRIBUTE_DATA *a2 = &attributes2->AttributeData[ai];
			if (a1->Info.AttributeFormat != a2->Info.AttributeFormat ||
				a1->NumberOfValues != a2->NumberOfValues)
				return test_ok(0, description, directive, reason, file, line,
					"#         got AttributeData[%lu]:\n"
					"#         AttributeFormat: %08x\n"
					"#          NumberOfValues: %lu\n"
					"#    expected AttributeData[%lu]:\n"
					"#         AttributeFormat: %08x\n"
					"#          NumberOfValues: %lu\n",
					ai, a1->Info.AttributeFormat, a1->NumberOfValues,
					ai, a2->Info.AttributeFormat, a2->NumberOfValues);
			uint32 vi;
			for (vi = 0; vi < a1->NumberOfValues; ++vi)
			{
				const CSSM_DATA *d1 = &a1->Value[vi];
				const CSSM_DATA *d2 = &a2->Value[vi];
				if (d1->Length != d2->Length || !d1->Data || !d2->Data ||
					memcmp(d1->Data, d2->Data, d1->Length))
					return test_ok(d1->Data == d2->Data, description,
                        directive, reason, file, line,
					   "#         got AttributeData[%lu].Value[%lu]:\n"
					   "#                 length: %lu\n"
					   "#                   data: '%.*s'\n"
					   "#    expected AttributeData[%lu].Value[%lu]:\n"
					   "#                 length: %lu\n"
					   "#                   data: '%.*s'\n",
					   ai, vi, d1->Length, (int)d1->Length, d1->Data,
					   ai, vi, d2->Length, (int)d2->Length, d2->Data);
			}
		}
	}
	if (data1 || data2)
	{
		if (!data1 || !data2)
			return test_ok(0, description, directive, reason, file, line,
				"#                   got CSSM_DATA %p\n"
				"#              expected CSSM_DATA %p\n", data1, data2);
        if (data1->Length != data2->Length || !data1->Data || !data2->Data ||
			memcmp(data1->Data, data2->Data, data1->Length))
			return test_ok(data1->Data == data2->Data, description, directive,
                reason, file, line,
			   "#                   got CSSM_DATA:\n"
			   "#                 length: %lu\n"
			   "#                   data: '%.*s'\n"
			   "#              expected CSSM_DATA:\n"
			   "#                 length: %lu\n"
			   "#                   data: '%.*s'\n",
			   data1->Length, (int)data1->Length, data1->Data,
			   data2->Length, (int)data2->Length, data2->Data);
	}

	return test_ok(1, description, directive, reason, file, line, NULL);
}

#define is_attributes_data(A1, D1, A2, D2, TESTNAME) \
( \
	test_is_attributes_data((A1), (D1), (A2), (D2), \
		TESTNAME, test_directive, test_reason, __FILE__, __LINE__) \
)

static void test1(CSSM_DL_HANDLE dl)
{
    CSSM_DL_DB_HANDLE dldb = { dl };
    CSSM_DB_UNIQUE_RECORD_PTR uniqueId;

	CSSM_DATA data = { 4, (uint8 *)"test" };
    ok_status(CSSM_DL_DbCreate(dl, DBNAME, NULL /* DbLocation */,
                  &dbInfo,
                  CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
                  NULL /* CredAndAclEntry */,
                  NULL /* &openParameters */,
                  &dldb.DBHandle),
        "CSSM_DL_DbCreate");
    
    is_status(CSSM_DL_DataInsert(dldb,
        attributes.DataRecordType,
        &attributes,
        &data,
        &uniqueId),
        CSSMERR_DL_INVALID_RECORDTYPE, "CSSM_DL_DataInsert no table");
    
    ok_status(CSSM_DL_CreateRelation(dldb,
        42,
        "Fourty Two",
        sizeof(attributeInfo) / sizeof(CSSM_DB_SCHEMA_ATTRIBUTE_INFO),
        attributeInfo,
        sizeof(indexInfo) / sizeof(CSSM_DB_SCHEMA_INDEX_INFO),
        indexInfo), "CSSM_DL_CreateRelation");

    ok_status(CSSM_DL_DataInsert(dldb,
        attributes.DataRecordType,
        &attributes,
        &data,
        &uniqueId), "CSSM_DL_DataInsert");
        
    is_status(CSSM_DL_DataInsert(dldb,
        attributes.DataRecordType,
        &attributes,
        &data,
        &uniqueId),
        CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA, "CSSM_DL_DataInsert dupe");

    ok_status(CSSM_DL_DataModify(dldb,
        attributes.DataRecordType,
		uniqueId,
        &newAttributes,
        &data,
        CSSM_DB_MODIFY_ATTRIBUTE_REPLACE), "CSSM_DL_DataModify");

    ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
        "CSSM_DL_FreeUniqueRecord");

    ok_status(CSSM_DL_DataInsert(dldb,
        attributes.DataRecordType,
        &attributes,
        &data,
        &uniqueId), "CSSM_DL_DataInsert old one again");

	CSSM_API_MEMORY_FUNCS memfuncs = {};
	ok_status(CSSM_GetAPIMemoryFunctions(dldb.DLHandle, &memfuncs),
		"CSSM_GetAPIMemoryFunctions");

    ok_status(CSSM_DL_DataGetFromUniqueRecordId(dldb, uniqueId, NULL, NULL),
		"CSSM_DL_DataGetFromUniqueRecordId get nothing");

	CSSM_DATA resultData = {};
    ok_status(CSSM_DL_DataGetFromUniqueRecordId(dldb, uniqueId,
		NULL, &resultData),
		"CSSM_DL_DataGetFromUniqueRecordId get data");
	is_attributes_data(NULL, &resultData, NULL, &data, "Does data match?");
	free_attributes_data(&memfuncs, NULL, &resultData);

	CSSM_DB_RECORD_ATTRIBUTE_DATA baseNoAttrs = attributes;
	baseNoAttrs.NumberOfAttributes = 0;
	baseNoAttrs.AttributeData = NULL;
	CSSM_DB_RECORD_ATTRIBUTE_DATA resultNoAttrs = {};
    ok_status(CSSM_DL_DataGetFromUniqueRecordId(dldb, uniqueId,
		&resultNoAttrs, NULL),
		"CSSM_DL_DataGetFromUniqueRecordId get 0 attributes");
	is_attributes_data(&resultNoAttrs, NULL, &baseNoAttrs, NULL,
		"Do attrs match?");
	free_attributes_data(&memfuncs, &resultNoAttrs, NULL);

    ok_status(CSSM_DL_DataGetFromUniqueRecordId(dldb, uniqueId,
		&resultNoAttrs, &resultData),
		"CSSM_DL_DataGetFromUniqueRecordId get data and 0 attributes");
	is_attributes_data(&resultNoAttrs, &resultData, &baseNoAttrs, &data,
		"Do attrs and data match?");
	free_attributes_data(&memfuncs, &resultNoAttrs, &resultData);

	CSSM_DB_ATTRIBUTE_DATA resultAttributeData[] =
	{
		{{ CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER, { (char *)((uint64_t)1<<32|1) } }},
		{{ CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER, { (char *)((uint64_t)2<<32|2) } }}
	};
	CSSM_DB_RECORD_ATTRIBUTE_DATA resultAttrs =
	{
		0, 0,
		sizeof(resultAttributeData) / sizeof(*resultAttributeData),
		resultAttributeData
	};
    ok_status(CSSM_DL_DataGetFromUniqueRecordId(dldb, uniqueId,
		&resultAttrs, &resultData),
		"CSSM_DL_DataGetFromUniqueRecordId get data and 2 attributes");
	is_attributes_data(&resultAttrs, &resultData, &attributes, &data,
		"Do attrs and data match?");
	free_attributes_data(&memfuncs, &resultAttrs, &resultData);

    ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
        "CSSM_DL_FreeUniqueRecord");

    CSSM_SELECTION_PREDICATE predicates[] =
    {
        {
            CSSM_DB_EQUAL,
            {
                { CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER, { (char *)((uint64_t)1<<32|1) } },
                1, values_str_1
            }
        }
    };
	CSSM_QUERY query =
	{
		attributes.DataRecordType,
		CSSM_DB_AND,
		sizeof(predicates) / sizeof(*predicates),
		predicates,
		{ CSSM_QUERY_TIMELIMIT_NONE, CSSM_QUERY_SIZELIMIT_NONE },
		0 /* CSSM_QUERY_RETURN_DATA -- for keys only to return raw key bits */
	};
	CSSM_HANDLE search = CSSM_INVALID_HANDLE;
    is_status(CSSM_DL_DataGetFirst(dldb, &query, NULL,
		NULL, NULL, NULL), CSSM_ERRCODE_INVALID_POINTER,
        "CSSM_DL_DataGetFirst no search handle, no unique_record");
    is_status(CSSM_DL_DataGetFirst(dldb, &query, &search,
		NULL, NULL, NULL), CSSM_ERRCODE_INVALID_POINTER,
        "CSSM_DL_DataGetFirst no unique_record");
    is_status(CSSM_DL_DataGetFirst(dldb, &query, NULL,
		NULL, NULL, &uniqueId), CSSM_ERRCODE_INVALID_POINTER,
        "CSSM_DL_DataGetFirst no search handle");

    ok_status(CSSM_DL_DataGetFirst(dldb, &query, &search,
		NULL, NULL, &uniqueId),
        "CSSM_DL_DataGetFirst no data no attrs");
    ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
        "CSSM_DL_FreeUniqueRecord");
    ok_status(CSSM_DL_DataAbortQuery(dldb, search),
        "CSSM_DL_DataAbortQuery");

    ok_status(CSSM_DL_DataGetFirst(dldb, &query, &search,
		&resultNoAttrs, NULL, &uniqueId),
        "CSSM_DL_DataGetFirst 0 attrs");
	is_attributes_data(&resultNoAttrs, NULL, &baseNoAttrs, NULL,
		"Do attrs match?");
	free_attributes_data(&memfuncs, &resultNoAttrs, NULL);
    ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
        "CSSM_DL_FreeUniqueRecord");
    ok_status(CSSM_DL_DataAbortQuery(dldb, search),
        "CSSM_DL_DataAbortQuery");

    ok_status(CSSM_DL_DataGetFirst(dldb, &query, &search,
		NULL, &resultData, &uniqueId),
        "CSSM_DL_DataGetFirst data");
	is_attributes_data(NULL, &resultData, NULL, &data, "Does data match?");
	free_attributes_data(&memfuncs, NULL, &resultData);
    ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
        "CSSM_DL_FreeUniqueRecord");
    ok_status(CSSM_DL_DataAbortQuery(dldb, search),
        "CSSM_DL_DataAbortQuery");

    ok_status(CSSM_DL_DataGetFirst(dldb, &query, &search,
		&resultNoAttrs, &resultData, &uniqueId),
        "CSSM_DL_DataGetFirst 0 attrs and data");
	is_attributes_data(&resultNoAttrs, &resultData, &baseNoAttrs, &data,
		"Do attrs and data match?");
	free_attributes_data(&memfuncs, &resultNoAttrs, &resultData);
    ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
        "CSSM_DL_FreeUniqueRecord");
    ok_status(CSSM_DL_DataAbortQuery(dldb, search),
        "CSSM_DL_DataAbortQuery");

    ok_status(CSSM_DL_DataGetFirst(dldb, &query, &search,
		&resultAttrs, &resultData, &uniqueId),
        "CSSM_DL_DataGetFirst 2 attrs and data");
    is_attributes_data(&resultAttrs, &resultData, &attributes, &data,
        "Do attrs and data match?");
	free_attributes_data(&memfuncs, &resultAttrs, &resultData);
    ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
        "CSSM_DL_FreeUniqueRecord");
    ok_status(CSSM_DL_DataAbortQuery(dldb, search),
        "CSSM_DL_DataAbortQuery");

    SKIP: {
        skip("nothing to free", 2, 
            ok_status(CSSM_DL_DataGetFirst(dldb, &query, &search,
                &resultAttrs, &resultData, &uniqueId),
                "CSSM_DL_DataGetFirst 2 attrs and data"));
        is_attributes_data(&resultAttrs, &resultData, &attributes, &data,
            "Do attrs and data match?");
        free_attributes_data(&memfuncs, &resultAttrs, &resultData);
        ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
            "CSSM_DL_FreeUniqueRecord");
    }

    is_status(CSSM_DL_DataGetNext(dldb, search,
		&resultAttrs, &resultData, &uniqueId),
        CSSMERR_DL_ENDOFDATA, "CSSM_DL_DataGetNext returns eod");

	CSSM_QUERY query2 =
	{
		attributes.DataRecordType,
		CSSM_DB_NONE,
		0,
		NULL,
		{ CSSM_QUERY_TIMELIMIT_NONE, CSSM_QUERY_SIZELIMIT_NONE },
		0 /* CSSM_QUERY_RETURN_DATA -- for keys only to return raw key bits */
	};
    ok_status(CSSM_DL_DataGetFirst(dldb, &query2, &search,
        &resultAttrs, &resultData, &uniqueId),
        "CSSM_DL_DataGetFirst 2 attrs and data");
    free_attributes_data(&memfuncs, &resultAttrs, &resultData);
    ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
        "CSSM_DL_FreeUniqueRecord");
    ok_status(CSSM_DL_DataGetNext(dldb, search,
		&resultAttrs, &resultData, &uniqueId),
        "CSSM_DL_DataGetNext 2 attrs and data");
    free_attributes_data(&memfuncs, &resultAttrs, &resultData);
    ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
        "CSSM_DL_FreeUniqueRecord");
    ok_status(CSSM_DL_DataAbortQuery(dldb, search),
        "CSSM_DL_DataAbortQuery");

    ok_status(CSSM_DL_DbClose(dldb),
        "CSSM_DL_DbClose");
}   

static void test2(CSSM_DL_HANDLE dl)
{
	CSSM_DL_DB_HANDLE dldb = { dl };
	CSSM_DB_UNIQUE_RECORD_PTR uniqueId;

    ok_status(CSSM_DL_DbCreate(dl, DBNAME, NULL /* DbLocation */,
                  &dbInfo,
                  CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
                  NULL /* CredAndAclEntry */,
                  NULL,
                  &dldb.DBHandle),
        "CSSM_DL_DbCreate");

    ok_status(CSSM_DL_DbClose(dldb),
        "CSSM_DL_DbClose");
    
	ok_status(CSSM_DL_DbOpen(dl, DBNAME, NULL /* DbLocation */,
                  CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
                  NULL /* CredAndAclEntry */,
                  &openParameters,
                  &dldb.DBHandle),
		"CSSM_DL_DbOpen");

	is_status(CSSM_DL_DataInsert(dldb,
		attributes.DataRecordType,
		&attributes,
		NULL,
		&uniqueId),
		CSSMERR_DL_INVALID_RECORDTYPE, "CSSM_DL_DataInsert no table");

	ok_status(CSSM_DL_CreateRelation(dldb,
		42,
		"Fourty Two",
		sizeof(attributeInfo) / sizeof(CSSM_DB_SCHEMA_ATTRIBUTE_INFO),
		attributeInfo,
		sizeof(indexInfo) / sizeof(CSSM_DB_SCHEMA_INDEX_INFO),
		indexInfo), "CSSM_DL_CreateRelation");

	ok_status(CSSM_DL_DataInsert(dldb,
		attributes.DataRecordType,
		&attributes,
		NULL,
		&uniqueId), "CSSM_DL_DataInsert fails unless 4039735 is fixed");

	is_status(CSSM_DL_DataInsert(dldb,
		attributes.DataRecordType,
		&attributes,
		NULL,
		&uniqueId),
		CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA,
		"CSSM_DL_DataInsert dupe");

	ok_status(CSSM_DL_DataDelete(dldb, uniqueId),
		"CSSM_DL_Delete");

	is_status(CSSM_DL_DataDelete(dldb, uniqueId),
		CSSMERR_DL_RECORD_NOT_FOUND, "delete again should fail");

	ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
		"CSSM_DL_FreeUniqueRecord");

	ok_status(CSSM_DL_DataInsert(dldb,
		attributes.DataRecordType,
		&attributes,
		NULL,
		&uniqueId), "Insert again after delete");

	ok_status(CSSM_DL_FreeUniqueRecord(dldb, uniqueId),
		"CSSM_DL_FreeUniqueRecord");

	ok_status(CSSM_DL_DbClose(dldb),
		"CSSM_DL_DbDelete");
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

	plan_tests(70);

	CSSM_DL_HANDLE dl;
	ok(cssm_attach(guid, &dl), "cssm_attach");
	ok(tests_begin(argc, argv), "tests_begin");

	test1(dl);
	ok_status(CSSM_DL_DbDelete(dl, DBNAME, NULL /* DbLocation */,
		NULL /* AccessCred */), "CSSM_DL_DbDelete");
	test2(dl);
	ok_status(CSSM_DL_DbDelete(dl, DBNAME, NULL /* DbLocation */,
		NULL /* AccessCred */), "CSSM_DL_DbDelete");
	ok(cssm_detach(guid, dl), "cssm_detach");
	ok_leaks("no leaks");

	return !tests_end(1);
}
