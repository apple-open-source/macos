/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// attachfactory - industrial grade production of Attachment objects
//
#ifdef __MWERKS__
#define _CPP_ATTACHFACTORY
#endif
#include "attachfactory.h"

#include "cspattachment.h"
#include <Security/cssmdli.h>
#include <Security/cssmcli.h>
#include <Security/cssmaci.h>
#include <Security/cssmtpi.h>
#include <derived_src/funcnames.gen>
#include <map>


//
// A template to generate AttachmentMakers for the standard plugin types.
//
template <CSSM_SERVICE_TYPE type, typename Table, const char *const nameTable[]>
class StandardAttachmentMaker : public AttachmentMaker {
public:
    StandardAttachmentMaker() : AttachmentMaker(type)
    {
        for (unsigned n = 0; n < sizeof(nameTable) / sizeof(nameTable[0]); n++)
            nameMap.insert(typename NameMap::value_type(nameTable[n], n));
    }

    Attachment *make(Module *module,
                     const CSSM_VERSION &version,
                     uint32 subserviceId,
                     CSSM_SERVICE_TYPE subserviceType,
                     const CSSM_API_MEMORY_FUNCS &memoryOps,
                     CSSM_ATTACH_FLAGS attachFlags,
                     CSSM_KEY_HIERARCHY keyHierarchy,
                     CSSM_FUNC_NAME_ADDR *FunctionTable,
                     uint32 NumFunctions)
    {
        StandardAttachment<type, Table> *attachment =
        new StandardAttachment<type, Table>(module,
                                             nameMap,
                                             version,
                                             subserviceId,
                                             subserviceType,
                                             memoryOps,
                                             attachFlags,
                                             keyHierarchy);
        attachment->resolveSymbols(FunctionTable, NumFunctions);
        return attachment;
    }

private:
    typedef typename StandardAttachment<type, Table>::NameMap NameMap;
    NameMap nameMap;
};


//
// Implementation of an attachment factory
//
AttachmentFactory::AttachmentFactory()
{
    // generate explicitly known attachment types
    factories[CSSM_SERVICE_AC] = new StandardAttachmentMaker<CSSM_SERVICE_AC, CSSM_SPI_AC_FUNCS, ACNameTable>;
    factories[CSSM_SERVICE_CL] = new StandardAttachmentMaker<CSSM_SERVICE_CL, CSSM_SPI_CL_FUNCS, CLNameTable>;
    factories[CSSM_SERVICE_CSP] = new StandardAttachmentMaker<CSSM_SERVICE_CSP, CSSM_SPI_CSP_FUNCS, CSPNameTable>;
    factories[CSSM_SERVICE_DL] = new StandardAttachmentMaker<CSSM_SERVICE_DL, CSSM_SPI_DL_FUNCS, DLNameTable>;
    factories[CSSM_SERVICE_TP] = new StandardAttachmentMaker<CSSM_SERVICE_TP, CSSM_SPI_TP_FUNCS, TPNameTable>;
}


AttachmentMaker *AttachmentFactory::attachmentMakerFor(CSSM_SERVICE_TYPE type) const
{
    AttachFactories::const_iterator it = factories.find(type);
    if (it == factories.end())
        CssmError::throwMe(CSSMERR_CSSM_INVALID_SERVICE_MASK);
    return it->second;
}


//
// Manage an AttachmentMaker
//
AttachmentMaker::~AttachmentMaker()
{
}
