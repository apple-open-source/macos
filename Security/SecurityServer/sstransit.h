/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


//
// sstransit - SecurityServer client library transition code.
//
// These are the functions that implement CssmClient methods in terms of
// MIG IPC client calls, plus their supporting machinery.
//
// WARNING! HERE BE DRAGONS!
// This code involves moderately arcane magic including (but not limited to)
// dancing macros paired off with self-maintaining stack objects. Don't take
// anything for granted! Be very afraid of ALL-CAPS names. Your best bet is
// probably to stick with the existing patterns.
//
#ifndef _H_SSTRANSIT
#define _H_SSTRANSIT

#include "ssclient.h"
#include <Security/mach++.h>
#include <Security/cssmwalkers.h>
#include <Security/AuthorizationWalkers.h>
#include "ucsp.h"

namespace Security
{

// stock leading argument profile used by all calls
#define UCSP_ARGS	mGlobal().serverPort, mig_get_reply_port(), &rcode

// IPC/IPCN wrap the actual Mach IPC call. IPC also activates the connection first
#define IPCN(statement) \
	{ CSSM_RETURN rcode; check(statement); if (rcode != CSSM_OK) CssmError::throwMe(rcode); }
#define IPC(statement)	{ activate(); IPCN(statement); }

// pass mandatory or optional CssmData arguments into an IPC call
#define DATA(arg)			arg.data(), arg.length()
#define OPTIONALDATA(arg)	(arg ? arg->data() : NULL), (arg ? arg->length() : 0)

// pass structured arguments in/out of IPC calls. See "data walkers" for details
#define COPY(copy)			copy, copy.length(), copy
#define COPY_OUT(copy)		&copy, &copy##Length, &copy##Base
#define COPY_OUT_DECL(type,name) type *name, *name##Base; mach_msg_type_number_t name##Length


//
// DataOutput manages an output CssmData argument.
//
class DataOutput {
public:
	DataOutput(CssmData &arg, CssmAllocator &alloc)
		: argument(arg), allocator(alloc) { mData = NULL; }
	~DataOutput();
	
	void **data() { return &mData; }
	mach_msg_type_number_t *length() { return &mLength; }
	
	CssmData &argument;
	CssmAllocator &allocator;

private:
	void *mData;
	mach_msg_type_number_t mLength;
};


//
// Bundle up a Context for IPC transmission
//
class SendContext {
public:
	SendContext(const Context &ctx);
	~SendContext() { CssmAllocator::standard().free(attributes); }
	
	const Context &context;
	CSSM_CONTEXT_ATTRIBUTE *attributes;
	size_t attributeSize;
};

#define CONTEXT(ctx)	ctx.context, ctx.attributes, ctx.attributes, ctx.attributeSize

} // end namespace Security

#endif //_H_SSTRANSIT
