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
// hosts - value-semantics host identifier class
//
#include "hosts.h"
#include <arpa/inet.h>
#include <netdb.h>


namespace Security {
namespace IPPlusPlus {


class NamedHost : public Host::Spec {
public:
    NamedHost(const char *name);
    
    string name() const;
    set<IPAddress> addresses() const;
    
    bool operator == (const NamedHost &other) const
    { return mName == other.mName; }
    
private:
    string mName;
    set<IPAddress> mAddrs;
};


class IPv4NumberHost : public Host::Spec {
public:
    IPv4NumberHost(IPAddress addr) : mAddr(addr) { }
    
    string name() const;
    set<IPAddress> addresses() const;
    
    bool operator == (const IPv4NumberHost &other) const
    { return mAddr == other.mAddr; }
    
private:
    IPAddress mAddr;
};


//
// Host basics
//
Host::Host(const char *form)
{
    //@@@ IPv4 only at this time
    IPAddress addr;
    if (inet_aton(form, &addr))
        mSpec = new IPv4NumberHost(addr);
    else
        mSpec = new NamedHost(form);
}


//
// Compare for equality
//
bool Host::operator == (const Host &other) const
{
    // really silly hack alert: just compare lexicographically by name
    return mSpec ? (name() == other.name()) : !other.mSpec;
}

bool Host::operator < (const Host &other) const
{
    // really silly hack alert: just compare lexicographically by name
    return !mSpec || (other.mSpec && name() < other.name());
}


//
// Compare for subsumption
//
bool Host::operator <= (const Host &other) const
{
    return false;
}


//
// IPv4 address host specs (a single IPv4 address)
//
string IPv4NumberHost::name() const
{
    return mAddr;
}

set<IPAddress> IPv4NumberHost::addresses() const
{
    set<IPAddress> result;
    result.insert(mAddr);
    return result;
}


//
// IPv4 hostname host specs (a set of addresses derived from a name lookup)
// @@@ If we want to support IPv6, this should ALSO contain IPv6 lookup results.
//
NamedHost::NamedHost(const char *name) : mName(name)
{
    //@@@ NOT THREAD SAFE - find another way to do name resolution
    if (hostent *he = gethostbyname(name)) {
        for (char **p = he->h_addr_list; *p; p++)
            mAddrs.insert(*reinterpret_cast<in_addr *>(*p));
        debug("ipname", "host %s resolves to %ld address(es)", mName.c_str(), mAddrs.size());
        return;
    }
    UnixError::throwMe(ENOENT);	//@@@ h_errno translation or other source
}

string NamedHost::name() const
{
    return mName;
}

set<IPAddress> NamedHost::addresses() const
{
    return mAddrs;
}


}	// end namespace IPPlusPlus
}	// end namespace Security
