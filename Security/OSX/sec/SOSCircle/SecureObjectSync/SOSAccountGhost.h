//
//  SOSAccountGhost.h
//  sec
//
//  Created by Richard Murphy on 4/12/16.
//
//

#ifndef SOSAccountGhost_h
#define SOSAccountGhost_h

#include "SOSAccount.h"

bool SOSAccountTrustedCircleHasNoGhostOfMe(SOSAccount* account);
bool SOSAccountGhostResultsInReset(SOSAccount* account);
CF_RETURNS_RETAINED SOSCircleRef SOSAccountCloneCircleWithoutMyGhosts(SOSAccount* account, SOSCircleRef startCircle);

#endif /* SOSAccountGhost_h */
