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
// chooser - Protocol repository and Transfer maker for network protocols
//
#include "networkchooser.h"


namespace Security {
namespace Network {


Chooser::Chooser(Manager &mgr) : manager(mgr)
{
}

Chooser::~Chooser()
{
}


//
// Add, remove, and locate primary Protocols by name.
//
void Chooser::add(Protocol *protocol)
{
    //@@@ locking
    Protocol * &proto = mCoreProtocols[protocol->urlPrefix()];
    assert(proto == NULL);
    proto = protocol;
}

void Chooser::remove(Protocol *protocol)
{
    ProtoMap::iterator it = mCoreProtocols.find(protocol->urlPrefix());
    assert(it != mCoreProtocols.end());
    mCoreProtocols.erase(it);
}

Protocol &Chooser::protocolFor(const char *protoName) const
{
    ProtoMap::const_iterator it = mCoreProtocols.find(protoName);
    if (it == mCoreProtocols.end())
        UnixError::throwMe(ENOENT);
    return *it->second;
}


//
// The default implementation of protocolFor just finds a direct-connection Protocol
// for the target's scheme.
//
Protocol &Chooser::protocolFor(const HostTarget &target) const
{
    return protocolFor(target.scheme());
}


//
// Here is a short-cut makeTransfer method.
// It simply determines the proper Protocol, creates a Transfer from it, and adds it
// to the Manager.
// 
Transfer *Chooser::makeTransfer(const Target &target, Operation operation)
{
    Protocol &protocol = protocolFor(target);
    //@@@ use auto_ptr here?
    Transfer *transfer = protocol.makeTransfer(target, operation);
    manager.add(transfer);
    return transfer;
}


}	// end namespace Network
}	// end namespace Security
