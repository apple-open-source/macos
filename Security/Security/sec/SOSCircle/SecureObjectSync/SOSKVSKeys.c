/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <SecureObjectSync/SOSKVSKeys.h>
#include <utilities/SecCFWrappers.h>
#include <SecureObjectSync/SOSAccountPriv.h>


void AppendCircleKeyName(CFMutableArrayRef array, CFStringRef name) {
    CFStringRef circle_key = SOSCircleKeyCreateWithName(name, NULL);
    CFArrayAppendValue(array, circle_key);
    CFReleaseNull(circle_key);
}

//
//
// MARK: KVS Keys
// TODO: Handle '|' and "¬" in other strings.
//
const CFStringRef kSOSKVSKeyParametersKey = CFSTR(">KeyParameters");
const CFStringRef kSOSKVSInitialSyncKey = CFSTR("^InitialSync");
const CFStringRef kSOSKVSAccountChangedKey = CFSTR("^AccountChanged");

const CFStringRef sWarningPrefix = CFSTR("!");
const CFStringRef sAncientCirclePrefix = CFSTR("@");
const CFStringRef sCirclePrefix = CFSTR("o");
const CFStringRef sRetirementPrefix = CFSTR("-");
const CFStringRef sCircleSeparator = CFSTR("|");
const CFStringRef sFromToSeparator = CFSTR(":");

static CFStringRef copyStringEndingIn(CFMutableStringRef in, CFStringRef token) {
    if(token == NULL) return CFStringCreateCopy(NULL, in);
    CFRange tokenAt = CFStringFind(in, token, 0);
    if(tokenAt.location == kCFNotFound) return NULL;
    CFStringRef retval = CFStringCreateWithSubstring(NULL, in, CFRangeMake(0, tokenAt.location));
    CFStringDelete(in, CFRangeMake(0, tokenAt.location+1));
    return retval;
}

SOSKVSKeyType SOSKVSKeyGetKeyTypeAndParse(CFStringRef key, CFStringRef *circle, CFStringRef *from, CFStringRef *to)
{
    SOSKVSKeyType retval = kUnknownKey;
    
    if(CFStringHasPrefix(key, sCirclePrefix)) retval = kCircleKey;
    else if(CFStringHasPrefix(key, sRetirementPrefix)) retval = kRetirementKey;
    else if(CFStringHasPrefix(key, kSOSKVSKeyParametersKey)) retval = kParametersKey;
    else if(CFStringHasPrefix(key, kSOSKVSInitialSyncKey)) retval = kInitialSyncKey;
    else if(CFStringHasPrefix(key, kSOSKVSAccountChangedKey)) retval = kAccountChangedKey;
    else retval = kMessageKey;
    
    switch(retval) {
        case kCircleKey:
            if (circle) {
                CFRange fromRange = CFRangeMake(1, CFStringGetLength(key)-1);
                *circle = CFStringCreateWithSubstring(NULL, key, fromRange);
            }
            break;
        case kMessageKey: {
            CFStringRef mCircle = NULL;
            CFStringRef mFrom = NULL;
            CFStringRef mTo = NULL;
            CFMutableStringRef keycopy = CFStringCreateMutableCopy(NULL, 128, key);
            
            if( ((mCircle = copyStringEndingIn(keycopy, sCircleSeparator)) != NULL) &&
               ((mFrom = copyStringEndingIn(keycopy, sFromToSeparator)) != NULL) &&
               (CFStringGetLength(mFrom) > 0)  ) {
                mTo = copyStringEndingIn(keycopy, NULL);
                if (circle && mCircle) *circle = CFStringCreateCopy(NULL, mCircle);
                if (from && mFrom) *from = CFStringCreateCopy(NULL, mFrom);
                if (to && mTo) *to = CFStringCreateCopy(NULL, mTo);
            } else {
                retval = kUnknownKey;
            }
            CFReleaseNull(mCircle);
            CFReleaseNull(mFrom);
            CFReleaseNull(mTo);
            CFReleaseNull(keycopy);
        }
            break;
        case kRetirementKey: {
            CFStringRef mCircle = NULL;
            CFStringRef mPeer = NULL;
            CFMutableStringRef keycopy = CFStringCreateMutableCopy(NULL, 128, key);
            CFStringDelete(keycopy, CFRangeMake(0, 1));
            if( ((mCircle = copyStringEndingIn(keycopy, sCircleSeparator)) != NULL) &&
               ((mPeer = copyStringEndingIn(keycopy, NULL)) != NULL)) {
                if (circle) *circle = CFStringCreateCopy(NULL, mCircle);
                if (from) *from = CFStringCreateCopy(NULL, mPeer);
            } else {
                retval = kUnknownKey;
            }
            // TODO - Update our circle
            CFReleaseNull(mCircle);
            CFReleaseNull(mPeer);
            CFReleaseNull(keycopy);
        }
            break;
        case kAccountChangedKey:
        case kParametersKey:
        case kInitialSyncKey:
        case kUnknownKey:
            break;
    }
    
    return retval;
}


SOSKVSKeyType SOSKVSKeyGetKeyType(CFStringRef key)
{
    return SOSKVSKeyGetKeyTypeAndParse(key, NULL, NULL, NULL);
}

CFStringRef SOSCircleKeyCreateWithCircle(SOSCircleRef circle, CFErrorRef *error)
{
    return SOSCircleKeyCreateWithName(SOSCircleGetName(circle), error);
}


CFStringRef SOSCircleKeyCreateWithName(CFStringRef circleName, CFErrorRef *error)
{
    if(!circleName) return NULL;
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("%@%@"), sCirclePrefix, circleName);
}

CFStringRef SOSCircleKeyCopyCircleName(CFStringRef key, CFErrorRef *error)
{
    CFStringRef circleName = NULL;
    
    if (kCircleKey != SOSKVSKeyGetKeyTypeAndParse(key, &circleName, NULL, NULL)) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircleName, NULL, error, NULL, CFSTR("Couldn't find circle name in key '%@'"), key);
        
        CFReleaseNull(circleName);
    }
    
    return circleName;
}

CFStringRef SOSMessageKeyCopyCircleName(CFStringRef key, CFErrorRef *error)
{
    CFStringRef circleName = NULL;
    
    if (SOSKVSKeyGetKeyTypeAndParse(key, &circleName, NULL, NULL) != kMessageKey) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircleName, NULL, error, NULL, CFSTR("Couldn't find circle name in key '%@'"), key);
        
        CFReleaseNull(circleName);
    }
    return circleName;
}

CFStringRef SOSMessageKeyCopyFromPeerName(CFStringRef messageKey, CFErrorRef *error)
{
    CFStringRef fromPeer = NULL;
    
    if (SOSKVSKeyGetKeyTypeAndParse(messageKey, NULL, &fromPeer, NULL) != kMessageKey) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircleName, NULL, error, NULL, CFSTR("Couldn't find from peer in key '%@'"), messageKey);
        
        CFReleaseNull(fromPeer);
    }
    return fromPeer;
}

CFStringRef SOSMessageKeyCreateWithCircleNameAndPeerNames(CFStringRef circleName, CFStringRef from_peer_name, CFStringRef to_peer_name)
{
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("%@%@%@%@%@"),
                                    circleName, sCircleSeparator, from_peer_name, sFromToSeparator, to_peer_name);
}

CFStringRef SOSMessageKeyCreateWithCircleAndPeerNames(SOSCircleRef circle, CFStringRef from_peer_name, CFStringRef to_peer_name)
{
    return SOSMessageKeyCreateWithCircleNameAndPeerNames(SOSCircleGetName(circle), from_peer_name, to_peer_name);
}

CFStringRef SOSMessageKeyCreateWithCircleAndPeerInfos(SOSCircleRef circle, SOSPeerInfoRef from_peer, SOSPeerInfoRef to_peer)
{
    return SOSMessageKeyCreateWithCircleAndPeerNames(circle, SOSPeerInfoGetPeerID(from_peer), SOSPeerInfoGetPeerID(to_peer));
}

CFStringRef SOSMessageKeyCreateFromPeerToTransport(SOSTransportMessageKVSRef transport, CFStringRef peer_name) {
    CFErrorRef error = NULL;
    SOSEngineRef engine = SOSTransportMessageGetEngine((SOSTransportMessageRef)transport);
    
    CFStringRef circleName = SOSTransportMessageKVSGetCircleName(transport);
    CFStringRef my_id = SOSEngineGetMyID(engine);
    
    CFStringRef result = SOSMessageKeyCreateWithCircleNameAndPeerNames(circleName, peer_name, my_id);
    CFReleaseSafe(error);
    return result;
}

CFStringRef SOSMessageKeyCreateFromTransportToPeer(SOSTransportMessageKVSRef transport, CFStringRef peer_name) {
    CFErrorRef error = NULL;
    SOSEngineRef engine = SOSTransportMessageGetEngine((SOSTransportMessageRef)transport);
    
    CFStringRef circleName = SOSTransportMessageKVSGetCircleName(transport);
    CFStringRef my_id = SOSEngineGetMyID(engine);
    
    CFStringRef result = SOSMessageKeyCreateWithCircleNameAndPeerNames(circleName, my_id, peer_name);
    CFReleaseSafe(error);
    return result;
}

CFStringRef SOSRetirementKeyCreateWithCircleNameAndPeer(CFStringRef circle_name, CFStringRef retirement_peer_name)
{
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("%@%@%@%@"),
                                    sRetirementPrefix, circle_name, sCircleSeparator, retirement_peer_name);
}

CFStringRef SOSRetirementKeyCreateWithCircleAndPeer(SOSCircleRef circle, CFStringRef retirement_peer_name)
{
    return SOSRetirementKeyCreateWithCircleNameAndPeer(SOSCircleGetName(circle), retirement_peer_name);
}

