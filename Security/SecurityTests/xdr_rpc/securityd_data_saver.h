/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

#ifndef _SECURITYD_DATA_SAVER_H_
#define _SECURITYD_DATA_SAVER_H_

#include <mach/message.h>
#include <security_utilities/unix++.h>
#include <security_cdsa_utilities/context.h>
#include <security_cdsa_utilities/cssmdb.h>

//
// Possible enhancement: have class silently do nothing on write if the 
// file already exists.  This would keep us from writing > 1 of the given 
// object.  
//
// XXX/gh  this should handle readPreamble() as well
//
class SecuritydDataSave: public Security::UnixPlusPlus::AutoFileDesc
{
public:
	// For x-platform consistency, sentry and version must be fixed-size
	static const uint32_t sentry = 0x1234;		// byte order sentry value
	static const uint32_t version = 1;
	// define type of data saved; naming convention is to strip CSSM_
	// most of these will probably never be used <shrug>
	enum
	{
		s32     = 32,	// signed 32-bit
		u32     = 33,	// unsigned 32-bit
		s64     = 64,
		u64     = 65,
		// leave some space: we might need to discriminate among the 
		// various integer types (although I can't see why we'd care to 
		// save them by themselves)
		DATA    = 1000,
		GUID    = 1001,
		VERSION = 1002,
		SUBSERVICE_UID = 1003,
		NET_ADDRESS    = 1004,
		CRYPTO_DATA    = 1005,
		LIST           = 1006,
		LIST_ELEMENT   = 1007,
		TUPLE          = 1008,
		TUPLEGROUP     = 1009,
		SAMPLE         = 1010,
		SAMPLEGROUP    = 1011,
		MEMORY_FUNCS   = 1012,
		ENCODED_CERT   = 1013,
		PARSED_CERT    = 1014,
		CERT_PAIR      = 1015,
		CERTGROUP      = 1016,
		BASE_CERTS     = 1017,
		ACCESS_CREDENTIALS = 1018,
		AUTHORIZATIONGROUP = 1019,
		ACL_VALIDITY_PERIOD = 1020,
		ACL_ENTRY_PROTOTYPE = 1021,
		ACL_OWNER_PROTOTYPE = 1022,
		ACL_ENTRY_INPUT     = 1023,
		RESOURCE_CONTROL_CONTEXT = 1024,
		ACL_ENTRY_INFO           = 1025,
		ACL_EDIT                 = 1026,
		FUNC_NAME_ADDR           = 1027,
		DATE                     = 1028,
		RANGE                    = 1029,
		QUERY_SIZE_DATA          = 1030,
		KEY_SIZE                 = 1031,
		KEYHEADER                = 1032,
		KEY                      = 1033,
		DL_DB_HANDLE             = 1034,
		CONTEXT_ATTRIBUTE        = 1035,
		CONTEXT                  = 1036,
		PKCS1_OAEP_PARAMS        = 1037,
		CSP_OPERATIONAL_STATISTICS = 1038,
		PKCS5_PBKDF1_PARAMS        = 1039,
		PKCS5_PBKDF2_PARAMS        = 1040,
		KEA_DERIVE_PARAMS          = 1041,
		TP_AUTHORITY_ID            = 1042,
		FIELD                      = 1043,
		TP_POLICYINFO              = 1044,
		DL_DB_LIST                 = 1045,
		TP_CALLERAUTH_CONTEXT      = 1046,
		ENCODED_CRL                = 1047,
		PARSED_CRL                 = 1048,
		CRL_PAIR                   = 1049,
		CRLGROUP                   = 1050,
		FIELDGROUP                 = 1051,
		EVIDENCE                   = 1052,
		TP_VERIFY_CONTEXT          = 1053,
		TP_VERIFY_CONTEXT_RESULT   = 1054,
		TP_REQUEST_SET             = 1055,
		TP_RESULT_SET              = 1056,
		TP_CONFIRM_RESPONSE        = 1057,
		TP_CERTISSUE_INPUT         = 1058,
		TP_CERTISSUE_OUTPUT        = 1059,
		TP_CERTCHANGE_INPUT        = 1060,
		TP_CERTCHANGE_OUTPUT       = 1061,
		TP_CERTVERIFY_INPUT        = 1062,
		TP_CERTVERIFY_OUTPUT       = 1063,
		TP_CERTNOTARIZE_INPUT      = 1064,
		TP_CERTNOTARIZE_OUTPUT     = 1065,
		TP_CERTRECLAIM_INPUT       = 1066,
		TP_CERTRECLAIM_OUTPUT      = 1067,
		TP_CRLISSUE_INPUT          = 1068,
		TP_CRLISSUE_OUTPUT         = 1069,
		CERT_BUNDLE_HEADER         = 1070,
		CERT_BUNDLE                = 1071,
		DB_ATTRIBUTE_INFO          = 1072,
		DB_ATTRIBUTE_DATA          = 1073,
		DB_RECORD_ATTRIBUTE_INFO   = 1074,
		DB_RECORD_ATTRIBUTE_DATA   = 1075,
		DB_PARSING_MODULE_INFO     = 1076,
		DB_INDEX_INFO              = 1077,
		DB_UNIQUE_RECORD           = 1078,
		DB_RECORD_INDEX_INFO       = 1079,
		DBINFO                     = 1080,
		SELECTION_PREDICATE        = 1081,
		QUERY_LIMITS               = 1082,
		QUERY                      = 1083,
		DL_PKCS11_ATTRIBUTE        = 1084,	// a pointer
		NAME_LIST                  = 1085,
		DB_SCHEMA_ATTRIBUTE_INFO   = 1086,
		DB_SCHEMA_INDEX_INFO       = 1087
	};
	static const int sdsFlags = O_RDWR|O_CREAT|O_APPEND;

public:
	SecuritydDataSave(const char *file) : AutoFileDesc(file, sdsFlags, 0644), mFile(file) 
	{ 
		writePreamble();
	}
	SecuritydDataSave(const SecuritydDataSave &sds) : AutoFileDesc(sds.fd()), mFile(sds.file()) { }

	~SecuritydDataSave() { }

	const char *file() const { return mFile; }

	void writeContext(Security::Context *context, intptr_t attraddr, 
					  mach_msg_type_number_t attrSize);
	void writeAclEntryInfo(AclEntryInfo *acls, 
						   mach_msg_type_number_t aclsLength);
	void writeAclEntryInput(AclEntryInput *acl, 
							mach_msg_type_number_t aclLength);
	void writeQuery(Security::CssmQuery *query, 
					mach_msg_type_number_t queryLength)
	{
		// finish the preamble
		uint32_t dtype = QUERY;
		writeAll(&dtype, sizeof(dtype));

		writeDataWithBase(query, queryLength);
	}

private:
	// slightly misleading, in that the saved data type is also part of the
	// preamble but must be written by the appropriate write...() routine
	void writePreamble()
	{
		uint32_t value = sentry;
		writeAll(&value, sizeof(value));
		value = version;
		writeAll(&value, sizeof(value));
	}

	// The usual pattern for data structures that include pointers is 
	// (1) let securityd relocate() the RPC-delivered raw data, thus
	//     transforming the raw data into walked (flattened) data
	// (2) write the size of the data pointer
	// (3) write the data pointer (for xdr_test reconstitution)
	// (4) write the length (in bytes) of the flattened data, and finally
	// (5) write the flattened data 
	//
	// writeDataWithBase() does (2) - (5)
	void writeDataWithBase(void *data, mach_msg_type_number_t datalen)
	{
		uint32_t ptrsize = sizeof(data);
		writeAll(&ptrsize, sizeof(ptrsize));
		writeAll(&data, ptrsize);
		writeAll(&datalen, sizeof(datalen));
		writeAll(data, datalen);
	}

private:
	const char *mFile;
};


#endif	/* _SECURITYD_DATA_SAVER_H_ */
