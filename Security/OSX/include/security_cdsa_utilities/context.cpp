/*
 * Copyright (c) 2000-2001,2003-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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


//
// context - manage CSSM (cryptographic) contexts every which way.
//
// A note on memory management:
// Context attributes are allocated from application memory in big chunks comprising
// many attributes as well as the attribute array itself. The CSSM_CONTEXT fields
// NumberOfAttributes and ContextAttributes are handled as a group. Context::Builder
// and Context::copyFrom assume these fields are undefined and fill them. Context::clear
// assumes they are valid and invalides them, freeing memory.
//
// You may also want to look at cssmcontext.h in CSSM proper, where CSSM's internal Context
// objects are built on top of our Context class.
//
#include <security_cdsa_utilities/context.h>


//
// Construct Context objects
//
Context::Context(CSSM_CONTEXT_TYPE type, CSSM_ALGORITHMS algorithmId)
{
	clearPod();
	ContextType = type;
	AlgorithmType = algorithmId;
}


//
// Delete a single attribute from a Context by type.
// We implement this by simply nulling out the slot - the memory is not released,
// and will not be reclaimed until the Context is deleted or reconstructed for some reason.
//
void Context::deleteAttribute(CSSM_ATTRIBUTE_TYPE type)
{
    for (uint32 n = 0; n < attributesInUse(); n++)
        if (ContextAttributes[n].AttributeType == type) {
            ContextAttributes[n].AttributeType = CSSM_ATTRIBUTE_NONE; 
            ContextAttributes[n].AttributeLength = 0;
            return;
        }
    // not found
    CssmError::throwMe(CSSMERR_CSSM_ATTRIBUTE_NOT_IN_CONTEXT);
}


//
// This swiss-army-knife function performs a deep copy of all of a Context's attributes,
// bundling them up into a single memory node and storing them into a pointer/count pair.
// It also returns the size of the memory block allocated, in case you care (IPC does).
//
size_t Context::copyAttributes(CSSM_CONTEXT_ATTRIBUTE * &attrs, uint32 &count,
							   Allocator &alloc) const
{
    Context::Builder builder(alloc);
    for (unsigned n = 0; n < attributesInUse(); n++)
        builder.setup(ContextAttributes[n]);
    size_t size = builder.make();
    for (unsigned n = 0; n < attributesInUse(); n++)
        builder.put(ContextAttributes[n]);
    builder.done(attrs, count);
	return size;
}


//
// Locate attribute values by type.
// This function deals in attribute vectors, not contexts; hence the explicit count argument.
// Returns NULL for attribute not found.
//
Context::Attr *Context::find(CSSM_ATTRIBUTE_TYPE theType,
                             const CSSM_CONTEXT_ATTRIBUTE *attrs, unsigned int count)
{
    for (unsigned n = 0; n < count; n++)
        if (attrs[n].AttributeType == theType)
            return (Attr *)&attrs[n];
    return NULL;
}


//
// Post-IPC context fixup.
// A Context is transmitted via IPC as a two-element blob. The first is the Context
// structure itself, which is taken as flat. The second is the flattened attribute
// vector blob as produced by the Context::Builder class. Since IPC will relocate
// each blob, we need to offset all internal pointers to compensate.
//
void Context::postIPC(void *base, CSSM_CONTEXT_ATTRIBUTE *ipcAttributes)
{
	ReconstituteWalker relocator(LowLevelMemoryUtilities::difference(ipcAttributes, base));
	ContextAttributes = ipcAttributes;	// fix context->attr vector link
	for (uint32 n = 0; n < attributesInUse(); n++)
		walk(relocator, (*this)[n]);
}


//
// Context Builders
//
size_t Context::Builder::make()
{
    size_t vectorSize =
		LowLevelMemoryUtilities::alignUp(slotCount * sizeof(CSSM_CONTEXT_ATTRIBUTE));
    size_t totalSize = vectorSize + sizer;
    attributes = reinterpret_cast<Attr *>(allocator.malloc(totalSize));
    copier = LowLevelMemoryUtilities::increment(attributes, vectorSize);
    slot = 0;
	return totalSize;
}

void Context::Builder::done(CSSM_CONTEXT_ATTRIBUTE * &attributes, uint32 &count)
{
    assert(slot == slotCount);	// match pass profiles
    attributes = this->attributes;
    count = slotCount;
    this->attributes = NULL;	// delivered the goods, no longer our responsibility
}


//
// Debugging support
//
#if defined(DEBUGDUMP)

static void dumpData(CSSM_DATA *data)
{
	if (data == NULL)
		Debug::dump("[NULL]");
	else
		Debug::dump("[%p,%ld]@%p", data->Data, data->Length, data);
}

void Context::Attr::dump() const
{
    Debug::dump(" Attr{type=%x, size=%d, value=", int(AttributeType), int(AttributeLength));
    switch (AttributeType & CSSM_ATTRIBUTE_TYPE_MASK) {
        case CSSM_ATTRIBUTE_DATA_UINT32:
            Debug::dump("%ld", long(Attribute.Uint32)); break;
        case CSSM_ATTRIBUTE_DATA_STRING:
            Debug::dump("%s@%p", Attribute.String, Attribute.String); break;
		case CSSM_ATTRIBUTE_DATA_CSSM_DATA:
			dumpData(Attribute.Data);
			break;
		case CSSM_ATTRIBUTE_DATA_CRYPTO_DATA:
			dumpData(&Attribute.CryptoData->Param);
			break;
        default:
            Debug::dump("%p", Attribute.String); break;	// (slightly unclean)
    };
    Debug::dump("}\n");
}

void Context::dump(const char *title, const CSSM_CONTEXT_ATTRIBUTE *attrs) const
{
	if (attrs == NULL)
		attrs = ContextAttributes;
    Debug::dump("Context %s{type=%d, alg=%d, CSP=%u, %d attributes@%p:\n",
		   title ? title : "",
           int(ContextType), int(AlgorithmType), (unsigned int)CSPHandle,
           int(NumberOfAttributes), attrs);
    for (unsigned int n = 0; n < NumberOfAttributes; n++)
        Attr::overlay(attrs[n]).dump();
    Debug::dump("} // end Context\n");
}

#endif //DEBUGDUMP
