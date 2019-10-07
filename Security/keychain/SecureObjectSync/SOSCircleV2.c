//
//  SOSCircleV2.c
//  sec
//
//  Created by Richard Murphy on 2/12/15.
//
//

#include "SOSCircleV2.h"

SOSPeerInfoRef SOSCircleV2PeerInfoGet(SOSCircleV2Ref circle, CFStringRef peerid) {
    return (SOSPeerInfoRef) CFSetGetValue(circle->peers, peerid);
}
