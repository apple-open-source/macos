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
// target - target objects and their sub-components
//
#ifndef _H_TARGET
#define _H_TARGET

#include <Security/ip++.h>
#include <Security/hosts.h>
#include <set>


namespace Security {
namespace Network {

using namespace IPPlusPlus;


class Protocol;


//
// A HostTarget is the "host part" of a full access target.
// HostTargets are suitable for use as keys in STL containers.
//
class HostTarget {
public:
    HostTarget(const char *scheme, Host h, IPPort p, string user = "", string password = "")
        : mScheme(scheme), mHost(h), mPort(p), mUser(user), mPassword(password) { }
        
    const char *scheme() const	{ return mScheme.c_str(); }
    const Host &host() const	{ return mHost; }
    IPPort port(IPPort defaultPort = 0) const { return mPort ? mPort : defaultPort; }

    //@@@ this should probably be replaced with pluggable authentication schemes
    bool haveUserPass() const	{ return mUser != ""; }
    string username() const		{ return mUser; }
    string password() const		{ return mPassword; }
    
    bool operator == (const HostTarget &other) const;	// equality
    bool operator < (const HostTarget &other) const;	// less-than for sorting
    bool operator <= (const HostTarget &other) const;	// proper nonstrict subsumption
    
    HostTarget defaultPort(IPPort def) const;
    
    string urlForm() const;		// canonical URL prefix form (without /path postfix)

private:
    string mScheme;				// URL scheme
    Host mHost;					// host name or number; no default
    IPPort mPort;				// port number; zero to use protocol default
    string mUser;				// username; default empty
    string mPassword;			// password; default empty
};


//
// Targets
// Targets are suitable for use as keys in STL functions.
//
class Target {
public:
    Target(const HostTarget &theHost, const char *thePath) : host(theHost), path(thePath) { }
    Target(const HostTarget &theHost, string thePath) : host(theHost), path(thePath) { }
    Target(const char *scheme, Host h, IPPort p, const char *thePath) 
        : host(scheme, h, p), path(thePath) { }
    
    bool operator == (const Target &other) const;
    bool operator <= (const Target &other) const;
    
    operator const HostTarget &() const		{ return host; }
    
    string urlForm() const;		// construct canonical URL form
    
    const HostTarget host;
    const string path;
};


}	// end namespace Network
}	// end namespace Security


#endif _H_TARGET
