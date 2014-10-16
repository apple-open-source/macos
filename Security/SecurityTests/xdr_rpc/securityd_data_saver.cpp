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

#include "securityd_data_saver.h"

/*
 * Please don't use this as an exemplar for new write...() calls.  This was
 * the first, is messy, and probably should be rewritten.  At the very
 * least, its correctness should be revisited.  
 */
void 
SecuritydDataSave::writeContext(Context *context, intptr_t attraddr, 
								mach_msg_type_number_t attrSize)
{
	// finish the preamble
	uint32_t dtype = CONTEXT;
	writeAll(&dtype, sizeof(dtype));

	// save size of a CSSM_CONTEXT (not strictly necessary)
	uint32_t csize = sizeof(CSSM_CONTEXT);
	writeAll(&csize, sizeof(csize));	// write the length first!  
	writeAll(context, csize);

	// save the original base address for relocation
	csize = sizeof(attraddr);
	writeAll(&csize, sizeof(csize));
	writeAll(&attraddr, csize);

	// finally, save off the attributes
	csize = attrSize;
	writeAll(&csize, sizeof(csize));
	writeAll(context->ContextAttributes, csize);
}

void 
SecuritydDataSave::writeAclEntryInfo(AclEntryInfo *acls, 
									 mach_msg_type_number_t aclsLength)
{
	// finish the preamble
	uint32_t dtype = ACL_ENTRY_INFO;
	writeAll(&dtype, sizeof(dtype));

	// write the base pointer, then the ACL itself
	uint32_t ptrsize = sizeof(acls);
	writeAll(&ptrsize, sizeof(ptrsize));
	writeAll(&acls, ptrsize);
	writeAll(&aclsLength, sizeof(aclsLength));
	writeAll(acls, aclsLength);
}

void 
SecuritydDataSave::writeAclEntryInput(AclEntryInput *acl, 
									  mach_msg_type_number_t aclLength)
{
	// finish the preamble
	uint32_t dtype = ACL_ENTRY_INPUT;
	writeAll(&dtype, sizeof(dtype));

	// write the pointer, then the ACL itself
	uint32_t ptrsize = sizeof(acl);
	writeAll(&ptrsize, sizeof(ptrsize));
	writeAll(&acl, ptrsize);
	writeAll(&aclLength, sizeof(aclLength));
	writeAll(acl, aclLength);
}


//
// Excerpts from securityd's transition.cpp showing where SecuritydDataSave
// is (to be) used
//

#if 0
kern_return_t ucsp_server_findFirst(UCSP_ARGS, DbHandle db,
	COPY_IN(CssmQuery, query),
	COPY_IN(CssmDbRecordAttributeData, inAttributes),
	COPY_OUT(CssmDbRecordAttributeData, outAttributes),
	boolean_t getData,
	DATA_OUT(data), KeyHandle *hKey, SearchHandle *hSearch, RecordHandle *hRecord)
{
	BEGIN_IPC
	relocate(query, queryBase, queryLength);
	SecuritydDataSave sds("/var/tmp/Query_findFirst");
	sds.writeQuery(query, queryLength);
	relocate(inAttributes, inAttributesBase, inAttributesLength);

	RefPointer<Database::Search> search;
	RefPointer<Database::Record> record;
	RefPointer<Key> key;
	CssmData outData; //OutputData outData(data, dataLength);
	CssmDbRecordAttributeData *outAttrs; mach_msg_type_number_t outAttrsLength;
	Server::database(db)->findFirst(*query, inAttributes, inAttributesLength,
		getData ? &outData : NULL, key, search, record, outAttrs, outAttrsLength);
	
	// handle nothing-found case without exceptions
	if (!record) {
		*hRecord = noRecord;
		*hSearch = noSearch;
		*hKey = noKey;
	} else {
		// return handles
		*hRecord = record->handle();
		*hSearch = search->handle();
		*hKey = key ? key->handle() : noKey;

		// return attributes (assumes relocated flat blob)
		flips(outAttrs, outAttributes, outAttributesBase);
		*outAttributesLength = outAttrsLength;

		// return data (temporary fix)
		if (getData) {
			*data = outData.data();
			*dataLength = outData.length();
		}
	}
	END_IPC(DL)
}

kern_return_t ucsp_server_decrypt(UCSP_ARGS, CONTEXT_ARGS, KeyHandle keyh,
	DATA_IN(cipher), DATA_OUT(clear))
{
	BEGIN_IPC
	SecuritydDataSave td("/var/tmp/securityd_Context_decrypt");	// XXX/gh  get sample Context for XDR testing
	relocate(context, contextBase, attributes, attrSize);
	// save attributes base addr for backwards compatibility
	intptr_t attraddr = reinterpret_cast<intptr_t>(&context->ContextAttributes);
	td.writeContext(&context, attraddr, attrSize);
	RefPointer<Key> key = Server::key(keyh);
	OutputData clearOut(clear, clearLength);
	key->database().decrypt(context, *key, DATA(cipher), clearOut);
	END_IPC(CSP)
}

// ...

kern_return_t ucsp_server_getAcl(UCSP_ARGS, AclKind kind, KeyHandle key,
	boolean_t haveTag, const char *tag,
	uint32 *countp, COPY_OUT(AclEntryInfo, acls))
{
	BEGIN_IPC
	uint32 count;
	AclEntryInfo *aclList;
	Server::aclBearer(kind, key).getAcl(haveTag ? tag : NULL, count, aclList);
	*countp = count;
	Copier<AclEntryInfo> aclsOut(aclList, count); // make flat copy

	{	// release the chunked memory originals
		ChunkFreeWalker free;
		for (uint32 n = 0; n < count; n++)
			walk(free, aclList[n]);
		
		// release the memory allocated for the list itself when we are done
		Allocator::standard().free (aclList);
	}
	
	// set result (note: this is *almost* flips(), but on an array)
	*aclsLength = aclsOut.length();
	*acls = *aclsBase = aclsOut;
	if (flipClient()) {
		FlipWalker w;
		for (uint32 n = 0; n < count; n++)
			walk(w, (*acls)[n]);
		w.doFlips();
		Flippers::flip(*aclsBase);
	}
	SecuritydDataSave sds("/var/tmp/AclEntryInfo_getAcl");
	sds.writeAclEntryInfo(*acls, *aclsLength);
	Server::releaseWhenDone(aclsOut.keep());
	END_IPC(CSP)
}

kern_return_t ucsp_server_changeAcl(UCSP_ARGS, AclKind kind, KeyHandle key,
	COPY_IN(AccessCredentials, cred), CSSM_ACL_EDIT_MODE mode, CSSM_ACL_HANDLE handle,
	COPY_IN(AclEntryInput, acl))
{
	BEGIN_IPC
    relocate(cred, credBase, credLength);
	relocate(acl, aclBase, aclLength);
	SecuritydDataSave sds("/var/tmp/AclEntryInput_changeAcl");
	sds.writeAclEntryInput(acl, aclLength);
	Server::aclBearer(kind, key).changeAcl(AclEdit(mode, handle, acl), cred);
	END_IPC(CSP)
}

#endif	/* 0 -- example code */
