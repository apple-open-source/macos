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
// connectionpool - manage pool of active, unused Connection objects
//
#ifndef _H_CONNECTIONPOOL
#define _H_CONNECTIONPOOL

#include "target.h"
#include <map>

namespace Security {
namespace Network {


class Connection;


//
// There is exactly one InternetAccessManager object per process.
//
class ConnectionPool {
public:
    ConnectionPool() { }
    ~ConnectionPool() { purge(); }
    
    Connection *get(const HostTarget &host);
    void retain(Connection *connection);
    bool remove(Connection *connection);
    
    void purge();
    
private:
    typedef multimap<HostTarget, Connection *> ConnectionMap;
    ConnectionMap mConnections;				// set of active connections
};


}	// end namespace Network
}	// end namespace Security


#endif _H_CONNECTIONPOOL
