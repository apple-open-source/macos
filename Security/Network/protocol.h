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
#ifndef _H_PROTOCOL
#define _H_PROTOCOL

#include <Security/ip++.h>
#include "netmanager.h"

using namespace IPPlusPlus;


namespace Security {
namespace Network {


class Transfer;
class Target;


//
// A Protocol object represents a particular transfer/access protocol.
//
class Protocol {
public:
    Protocol(Manager &mgr, const char *prefix = NULL);
    virtual ~Protocol();

    virtual const char *name() const;
    const char *urlPrefix() const	{ return mPrefix ? mPrefix : "?"; }
    
    typedef unsigned int Operation;
    enum {
        download = 1,			// transfer data to sink
        upload = 2,				// transfer data from source
        transaction = 3,		// source-to-sink transaction mode
        
        protocolSpecific = 101	// starting here is protocol specific
    };
    
    virtual Transfer *makeTransfer(const Target &target, Operation operation);
    
    Manager &manager;
    
public:
    virtual bool isProxy() const;	// true if this is a proxy protocol
    virtual const HostTarget &proxyHost() const; // proxy host if isProxy()

private:
    const char *mPrefix;
};


}	// end namespace Network
}	// end namespace Security


#endif /* _H_PROTOCOL */
