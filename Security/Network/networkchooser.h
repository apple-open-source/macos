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
#ifndef _H_CHOOSER
#define _H_CHOOSER

#include <Security/netmanager.h>
#include <Security/protocol.h>
#include <set>
#include <map>


using namespace IPPlusPlus;

namespace Security {
namespace Network {


class Protocol;
class Transfer;
class Connection;


//
//
class Chooser {
    typedef Protocol::Operation Operation;
public:
    Chooser(Manager &mgr);
    virtual ~Chooser();
    
    Manager &manager;
    
public:
    // add and remove direct protocols
    void add(Protocol *protocol);
    void remove(Protocol *protocol);
    Protocol &protocolFor(const char *protoName) const; // find protocol by URL scheme
    
public:
    // override this method to implement protocol choosing
    virtual Protocol &protocolFor(const HostTarget &target) const;
    
public:
    Transfer *makeTransfer(const Target &target, Operation operation);
    
private:
    typedef map<string, Protocol *> ProtoMap;
    ProtoMap mCoreProtocols;					// map of registered protocols
};


}	// end namespace Network
}	// end namespace Security


#endif _H_CHOOSER
