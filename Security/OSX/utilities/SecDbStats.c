//
//Copyright (c) 2025 Apple Inc. All Rights Reserved.
//
// @APPLE_LICENSE_HEADER_START@
//
// This file contains Original Code and/or Modifications of Original Code
// as defined in and that are subject to the Apple Public Source License
// Version 2.0 (the 'License'). You may not use this file except in
// compliance with the License. Please obtain a copy of the License at
// http://www.opensource.apple.com/apsl/ and read it before using this
// file.
//
// The Original Code and all software distributed under the License are
// distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
// EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
// INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
// Please see the License for the specific language governing rights and
// limitations under the License.
//
// @APPLE_LICENSE_HEADER_END@
//

#include <stdlib.h>
#include <errno.h>
#include <os/signpost.h>
#include <os/signpost_private.h>

#include "debugging.h"
#include "SecDbStats.h"

typedef struct _StatAtom {
    qos_class_t arrivalQos;
    os_signpost_id_t identifier;
    bool readOnly;
} StatAtom;

static bool g_signposts = false;
void _SecDbStatsEnableWaitSignposts(bool yorn) {
    g_signposts = yorn;
}

bool _SecDbStatsWaitSignpostsEnabled() {
    return g_signposts;
}

static os_log_t GetSubsystem() {
    static os_log_t subsystem = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        subsystem = os_log_create("com.apple.security.keychain_db.signposts", "signpost");
    });
    return subsystem;
}

StatCtx SecDbStatStart(bool readOnly) {
    if (!g_signposts) {
        return NULL;
    }

    StatAtom* atom = malloc(sizeof(StatAtom));

    if (atom == NULL) {
        secerror("Unable to allocate StatCtx: %{darwin.errno}d", errno);
        return atom;
    }

    atom->arrivalQos = qos_class_self();
    os_log_t subsystem = GetSubsystem();
    atom->identifier = os_signpost_id_generate(subsystem);
    atom->readOnly = readOnly;

    if (atom->readOnly) {
        os_signpost_interval_begin(subsystem, atom->identifier, "read_connection_wait", OS_SIGNPOST_ENABLE_TELEMETRY);
    } else {
        os_signpost_interval_begin(subsystem, atom->identifier, "write_connection_wait", OS_SIGNPOST_ENABLE_TELEMETRY);
    }

    return atom;
}

void SecDbStatEnd(StatCtx ctx) {
    if (!g_signposts) {
        return;
    }

    StatAtom* atom = (StatAtom*)ctx;

    if (atom == NULL) {
        secerror("Passed NULL StatCtx");
        return;
    }

    os_log_t subsystem = GetSubsystem();

    if (atom->readOnly) {
        os_signpost_interval_end(subsystem, atom->identifier, "read_connection_wait", "priority=%{public,signpost.telemetry:number1,name=priority}d " OS_SIGNPOST_ENABLE_TELEMETRY, atom->arrivalQos);
    } else {
        os_signpost_interval_end(subsystem, atom->identifier, "write_connection_wait", "priority=%{public,signpost.telemetry:number1,name=priority}d " OS_SIGNPOST_ENABLE_TELEMETRY, atom->arrivalQos);
    }

    free(atom);
}

void SecDbStatImpulse(bool readOnly) {
    if (!g_signposts) {
        return;
    }

    os_log_t subsystem = GetSubsystem();
    os_signpost_id_t identifier = os_signpost_id_generate(subsystem);
    qos_class_t arrivalQos = qos_class_self();

    if (readOnly) {
        os_signpost_event_emit(subsystem, identifier, "read_connection_nowait", "priority=%{public,signpost.telemetry:number1,name=priority}d " OS_SIGNPOST_ENABLE_TELEMETRY, arrivalQos);
    } else {
        os_signpost_event_emit(subsystem, identifier, "write_connection_nowait", "priority=%{public,signpost.telemetry:number1,name=priority}d " OS_SIGNPOST_ENABLE_TELEMETRY, arrivalQos);
    }
}
