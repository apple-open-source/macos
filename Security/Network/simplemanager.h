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
// simplemanager - "direct mode" network core manager
//
#ifndef _H_SIMPLEMANAGER
#define _H_SIMPLEMANAGER

#include "netmanager.h"
#include "networkchooser.h"
#include <Security/streams.h>


namespace Security {
namespace Network {


//
// The Test manager class
//
class SimpleManager : public Manager, public Chooser {
public:
    SimpleManager();
    
#if BUG_GCC
    void add(Transfer *xfer)		{ Manager::add(xfer); }
    void add(Protocol *proto)		{ Chooser::add(proto); }
#else
    using Manager::add;
    using Chooser::add;
#endif
    
public:
    void allTransfersSynchronous();
};


}	// end namespace Network
}	// end namespace Security


#endif //_H_SIMPLEMANAGER
