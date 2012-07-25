/*
 * Copyright (c) 2000-2002,2004 Apple Computer, Inc. All Rights Reserved.
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
#ifdef __MWERKS__
#define _CPP_CSSMCONTEXT
#endif
#include "cssmcontext.h"


//
// Destroy a HandleContext.
//
HandleContext::~HandleContext()
{
	attachment.free(extent);
	attachment.free(ContextAttributes);
}


//
// Locking protocol for HandleContexts
//
void HandleContext::lock()
{ attachment.enter(); }

bool HandleContext::tryLock()
{ return attachment.tryEnter(); }


//
// Merge a new set of attributes into an existing HandleContext, copying
// the new values deeply while releasing corresponding old values.
//
// NOTE: This is a HandleContext method; it does not work on bare Contexts.
//
void HandleContext::mergeAttributes(const CSSM_CONTEXT_ATTRIBUTE *attributes, uint32 count)
{
	// attempt to fast-path some simple or frequent cases
	if (count == 1) {
		if (Attr *attr = find(attributes[0].AttributeType)) {
			if (attr->baseType() == CSSM_ATTRIBUTE_DATA_UINT32) {
				// try quick replacement
				Attr oldAttr = *attr;
				*attr = attributes[0];
				if (CSSM_RETURN err = validateChange(CSSM_CONTEXT_EVENT_UPDATE)) {
					// roll back and fail
					*attr = oldAttr;
					CssmError::throwMe(err);
				}
				return;	// all done
			} else {
				// pointer value - does it fit into the space of the current value?
				size_t oldSize = size(*attr);
				size_t newSize = size(attributes[0]);
                Attr oldAttr = *attr;
				if (newSize <= oldSize) {	// give it a try...
					*attr = attributes[0];
					// NOTE that the CSP is getting a "temporary" pointer set to validate;
					// If we commit, the final copy will be elsewhere. CSP writer beware!
					if (CSSM_RETURN err = validateChange(CSSM_CONTEXT_EVENT_UPDATE)) {
						// roll back and fail
						*attr = oldAttr;
						CssmError::throwMe(err);
					}
					// commit new value
					CopyWalker copier(oldAttr.Attribute.String);
					walk(copier, *attr);
					return;
				}
			}
		} else {	// single change, new attribute
			if (Attr *slot = find(CSSM_ATTRIBUTE_NONE)) {
				const Attr *attr = static_cast<const Attr *>(&attributes[0]);
				if (attr->baseType() == CSSM_ATTRIBUTE_DATA_UINT32) {	// trivial
					Attr oldSlot = *slot;
					*slot = *attr;
					if (CSSM_RETURN err = validateChange(CSSM_CONTEXT_EVENT_UPDATE)) {
						*slot = oldSlot;
						CssmError::throwMe(err);
					}
					// already ok
					return;
				} else if (extent == NULL) {	// pointer value, allocate into extent
					void *data = attachment.malloc(size(*attr));
					try {
						Attr oldSlot = *slot;
						*slot = attributes[0];
						CopyWalker copier(data);
						walk(copier, *slot);
						if (CSSM_RETURN err = validateChange(CSSM_CONTEXT_EVENT_UPDATE)) {
							*slot = oldSlot;
							CssmError::throwMe(err);
						}
					} catch (...) {
						attachment.free(data);
						throw;
					}
					extent = data;
					return;
				}
			}
		}
	}
	
	// slow form: build a new value table and get rid of the old one
	Context::Builder builder(attachment);
	for (unsigned n = 0; n < count; n++)
		builder.setup(attributes[n]);
	for (unsigned n = 0; n < attributesInUse(); n++)
		if (!find(ContextAttributes[n].AttributeType, attributes, count))
			builder.setup(ContextAttributes[n]);
	builder.make();
	for (unsigned n = 0; n < count; n++)
		builder.put(attributes[n]);
	for (unsigned n = 0; n < attributesInUse(); n++)
		if (!find(ContextAttributes[n].AttributeType, attributes, count))
			builder.put(ContextAttributes[n]);
			
	// Carefully, now! The CSP may yet tell us to back out.
	// First, save the old values...
	CSSM_CONTEXT_ATTRIBUTE *oldAttributes = ContextAttributes;
	uint32 oldCount = NumberOfAttributes;
	
	// ...install new blob into the context...
	builder.done(ContextAttributes, NumberOfAttributes);
	
	// ...and ask the CSP whether this is okay
	if (CSSM_RETURN err = validateChange(CSSM_CONTEXT_EVENT_UPDATE)) {
		// CSP refused; put everything back where it belongs
		attachment.free(ContextAttributes);
		ContextAttributes = oldAttributes;
		NumberOfAttributes = oldCount;
		CssmError::throwMe(err);
	}
	
	// we succeeded, so NOW delete the old attributes blob
	attachment.free(oldAttributes);
}


//
// Ask the CSP to validate a proposed (and already implemented) change
//
CSSM_RETURN HandleContext::validateChange(CSSM_CONTEXT_EVENT event)
{
	// lock down the module if it is not thread-safe
	StLock<Module, &Module::safeLock, &Module::safeUnlock> _(attachment.module);
	return attachment.downcalls.EventNotify(attachment.handle(),
		event, handle(), this);
}


//
// Wrap up a deluxe context creation operation and return the new CC handle.
//
CSSM_CC_HANDLE HandleContext::Maker::operator () (CSSM_CONTEXT_TYPE type,
												CSSM_ALGORITHMS algorithm)
{
	// construct the HandleContext object
	HandleContext &context = *new(attachment) HandleContext(attachment, type, algorithm);
	context.CSPHandle = attachment.handle();
	done(context.ContextAttributes, context.NumberOfAttributes);
	
	// ask the CSP for consent
	if (CSSM_RETURN err = context.validateChange(CSSM_CONTEXT_EVENT_CREATE)) {
		// CSP refused; clean up and fail
		context.destroy(&context, context.attachment);
		CssmError::throwMe(err);
	}
	
	// return the new handle (we have succeeded)
	return context.handle();
}
