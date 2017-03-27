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

bool SOSAccountTrustedCircleHasNoGhostOfMe(SOSAccountRef account);
bool SOSAccountGhostResultsInReset(SOSAccountRef account);
SOSCircleRef SOSAccountCloneCircleWithoutMyGhosts(SOSAccountRef account, SOSCircleRef startCircle);

#endif /* SOSAccountGhost_h */
