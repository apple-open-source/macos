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
#include "simplemanager.h"
#include "netparameters.h"
#include <Security/url.h>

#include "file-protocol.h"
#include "ftp-protocol.h"
#include "http-protocol.h"
#include "https-protocol.h"


namespace Security {
namespace Network {


SimpleManager::SimpleManager() : Chooser(static_cast<Manager &>(*this))
{
    add(new HTTPProtocol(*this));
    add(new SecureHTTPProtocol(*this));
    add(new FTPProtocol(*this));
    add(new FileProtocol(*this));
}


//
// Run all active transfers synchronously until complete.
//
void SimpleManager::allTransfersSynchronous()
{
    run();
}


}	// end namespace Network
}	// end namespace Security
