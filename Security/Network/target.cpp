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
#include "target.h"
#include "protocol.h"


namespace Security {
namespace Network {


//
// Produce a HostTarget with a default port inserted, if necessary.
//
HostTarget HostTarget::defaultPort(IPPort defPort) const
{
    return HostTarget(scheme(), host(), port(defPort), username(), password());
}


//
// Given a Target, construct a canonical proper URL string
//
string HostTarget::urlForm() const
{
    // form the :port optional part
    char portPart[10];
    if (mPort)
        sprintf(portPart, ":%d", mPort);
    else
        portPart[0] = '\0';
        
    // build the whole form
    char buffer[1024];
    if (haveUserPass()) {
        snprintf(buffer, sizeof(buffer), "%s://%s:%s@%s%s",
            scheme(), mUser.c_str(), mPassword.c_str(),
            mHost.name().c_str(), portPart);
    } else {
        snprintf(buffer, sizeof(buffer), "%s://%s%s",
            scheme(), mHost.name().c_str(), portPart);
    }
    return buffer;
}

string Target::urlForm() const
{
    return host.urlForm() + path;
}


bool HostTarget::operator == (const HostTarget &other) const
{
    return mScheme == other.mScheme
        && mHost == other.mHost
        && mPort == other.mPort
        && mUser == other.mUser
        && mPassword == other.mPassword;
}

bool HostTarget::operator < (const HostTarget &other) const
{
    // arbitrary lexicographic ordering
    if (mScheme != other.mScheme)
        return mScheme < other.mScheme;
    if (mHost != other.mHost)
        return mHost < other.mHost;
    if (mPort != other.mPort)
        return mPort < other.mPort;
    if (mUser != other.mUser)
        return mUser < other.mUser;
    return mPassword < other.mPassword;
}

bool HostTarget::operator <= (const HostTarget &other) const
{
    //@@@ be lenient on subsume-matching empty users/passwords? Distinguish spec/unspec?
    return mHost <= other.mHost
        && mScheme == other.mScheme
        && mPort == other.mPort
        && mUser == other.mUser
        && mPassword == other.mPassword;
}

bool Target::operator == (const Target &other) const
{
    return host == other.host && path == other.path;
}

bool Target::operator <= (const Target &other) const
{
    //@@@ be lenient on path matches? Usage?
    return host <= other.host && path == other.path;
}


}	// end namespace Network
}	// end namespace Security
