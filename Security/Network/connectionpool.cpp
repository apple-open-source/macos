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
#include "connectionpool.h"
#include "netconnection.h"


namespace Security {
namespace Network {


//
// Try to locate a Connection with a suitable HostTarget from the pool.
// If found, remove it from the pool and return it. Otherwise, return NULL (no error).
//
Connection *ConnectionPool::get(const HostTarget &host)
{
    //@@@ locking, of course :-)
    ConnectionMap::iterator it = mConnections.find(host);
    if (it != mConnections.end()) {
        // take it and use it
        Connection *connection = it->second;
        mConnections.erase(it);
        secdebug("connpool", "Connection %p retrieved from pool", connection);
        return connection;
    }
    // none available
    return NULL;
}


//
// Retain a Connection in the pool
//
void ConnectionPool::retain(Connection *connection)
{
    //@@@ threading, of course :-)
    secdebug("connpool", "Connection %p retained in connection pool", connection);
    mConnections.insert(ConnectionMap::value_type(connection->hostTarget, connection));
    //mConnections[connection->hostTarget] = connection;
}


//
// Remove a retained Connection from the pool.
// Returns true if found (and removed); false otherwise.
//
bool ConnectionPool::remove(Connection *connection)
{
    // this search is two-stage to deal with potentially large multimaps
    typedef ConnectionMap::iterator Iter;
    pair<Iter, Iter> range = mConnections.equal_range(connection->hostTarget);
    for (Iter it = range.first; it != range.second; it++)
        if (it->second == connection) {
            mConnections.erase(it);
            secdebug("connpool", "Connection %p removed from connection pool", connection);
            return true;
        }
    return false;
}


//
// Clear the connection pool
//
void ConnectionPool::purge()
{
    secdebug("connpool", "Connection pool purging %ld connections", mConnections.size());
    for (ConnectionMap::iterator it = mConnections.begin(); it != mConnections.end(); it++)
        delete it->second;
    mConnections.erase(mConnections.begin(), mConnections.end());
}


}	// end namespace Network
}	// end namespace Security
