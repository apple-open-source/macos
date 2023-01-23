/*
 * Copyright (c) 2008,2011-2012 Apple Inc. All Rights Reserved.
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


#include <security_cdsa_utilities/handletemplates_defs.h>
#include <Security/cssm.h>
#include <stdint.h>

namespace Security
{

// 
// Instantiate the explicit MappingHandle subclasses.  If there start to be
// a lot of these, break this into multiple .cpp files so useless classes
// aren't linked in everywhere.  
//

template struct TypedHandle<CSSM_HANDLE>;        // HandledObject

template class MappingHandle<CSSM_HANDLE>;      // HandleObject

template class MappingHandle<uint32_t>;         // U32HandleObject

}
