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
// protocol - generic interface to an access protocol
//
#include "protocol.h"
#include "netmanager.h"
#include "neterror.h"


namespace Security {
namespace Network {


//
// Construct and manage a Protocol object
//
Protocol::Protocol(Manager &mgr, const char *prefix) : manager(mgr), mPrefix(prefix)
{
}

Protocol::~Protocol()
{
}


//
// By default, name() just returns the same as urlPrefix()
//
const char *Protocol::name() const
{
    return urlPrefix();
}


//
// Default Transfer factory (fails)
//
Transfer *Protocol::makeTransfer(const Target &, Operation)
{ Error::throwMe(); }


//
// Default to *not* a proxy protocol
//
bool Protocol::isProxy() const
{ return false; }

const HostTarget &Protocol::proxyHost() const
{
    assert(false);
}


}	// end namespace Network
}	// end namespace Security
