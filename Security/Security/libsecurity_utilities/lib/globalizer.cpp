/*
 * Copyright (c) 2000-2004,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// globalizer - multiscope globalization services.
//
// This is a tentative, partial implementation.
// Status:
//	module scope: constructs, optional cleanup
//	thread scope: constructs, optional cleanup
//	process scope: not implemented (obsolete implementation, unused)
//	system scope: not implemented (probably never will)
//
// @@@ Assumption: {bool,T*} atomic unless PTHREAD_STRICT
//
#include <security_utilities/globalizer.h>
#include <security_utilities/debugging.h>
#include <cstdlib>
#include <stdexcept>

//
// The Error class thrown if Nexus operations fail
//
GlobalNexus::Error::~Error() throw()
{
}

void ModuleNexusCommon::do_create(void *(*make)())
{
    try
    {
        pointer = make();
    }
    catch (...)
    {
        pointer = NULL;
    }
}



void *ModuleNexusCommon::create(void *(*make)())
{
    dispatch_once(&once, ^{do_create(make);});
    
    if (pointer == NULL)
    {
        ModuleNexusError::throwMe();
    }
    
    return pointer;
}


//
// Process nexus operation
//
ProcessNexusBase::ProcessNexusBase(const char *identifier)
{
	const char *env = getenv(identifier);
	if (env == NULL) {	// perhaps we're first...
		auto_ptr<Store> store(new Store);
		char form[2*sizeof(Store *) + 2];
		sprintf(form, "*%p", &store);
		setenv(identifier, form, 0);	// do NOT overwrite...
		env = getenv(identifier);		// ... and refetch to resolve races
		if (sscanf(env, "*%p", &mStore) != 1)
			throw std::runtime_error("environment communication failed");
		if (mStore == store.get())		// we won the race...
			store.release();			// ... so keep the store
	} else
		if (sscanf(env, "*%p", &mStore) != 1)
			throw std::runtime_error("environment communication failed");
}
