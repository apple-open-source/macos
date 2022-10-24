//
//  submit-ess-event.h
//  sshd
//
//  Copyright © 2022 Apple Inc. All rights reserved.
//

#ifndef submit_ess_event_h
#define submit_ess_event_h

#include "audit.h"
#include "hostfile.h"
#include "auth.h"

void submit_ess_event(const char *source_address, const char *username, ssh_audit_event_t event);

#endif /* submit_ess_event_h */
