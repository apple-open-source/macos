/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2024-2025 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __SRP_STRICT_H__
#define __SRP_STRICT_H__

//======================================================================================================================

#ifndef _SRP_STRICT_DISPOSE_TEMPLATE
#   define _SRP_STRICT_DISPOSE_TEMPLATE(PTR, FUNCTION)  \
        do {                                            \
            if (*(PTR) != NULL) {                       \
                FUNCTION(*PTR);                         \
                *(PTR) = NULL;                          \
            }                                           \
        } while(0)
#endif // _SRP_STRICT_DISPOSE_TEMPLATE

#ifndef CFForget
#   define CFForget(PTR)                    _SRP_STRICT_DISPOSE_TEMPLATE(PTR, CFRelease)
#endif

#ifndef DNSServiceRefSourceForget
#   define DNSServiceRefSourceForget(PTR)   _SRP_STRICT_DISPOSE_TEMPLATE(PTR, DNSServiceRefDeallocate)
#endif

#define srp_block_forget(PTR)               _SRP_STRICT_DISPOSE_TEMPLATE(PTR, _Block_release)
#define srp_cf_forget(PTR)                  _SRP_STRICT_DISPOSE_TEMPLATE(PTR, CFRelease)
#define srp_dispatch_forget(PTR)            _SRP_STRICT_DISPOSE_TEMPLATE(PTR, dispatch_release)
#define srp_nrdm_forget(PTR)                _SRP_STRICT_DISPOSE_TEMPLATE(PTR, nr_device_monitor_release)
#define srp_nw_forget(PTR)                  _SRP_STRICT_DISPOSE_TEMPLATE(PTR, nw_release)
#define srp_sec_forget(PTR)                 _SRP_STRICT_DISPOSE_TEMPLATE(PTR, sec_release)
#define srp_strict_free(PTR)                _SRP_STRICT_DISPOSE_TEMPLATE(PTR, free)
#define srp_xpc_forget(PTR)                 _SRP_STRICT_DISPOSE_TEMPLATE(PTR, xpc_release)

//======================================================================================================================

#include "srp-log.h"
#ifndef STRICT_ABORT
#define STRICT_ABORT(format, ...)       \
    do {                                \
        ERROR(format, ##__VA_ARGS__);   \
        __builtin_trap();               \
    } while (0)
#endif

#include "mdns_strict.h"
#define srp_strict_malloc                     mdns_malloc
#define srp_strict_calloc                     mdns_calloc
#define srp_strict_strdup                     mdns_strdup
#define srp_strict_strlcpy                    mdns_strlcpy

#endif // __SRP_STRICT_H__

// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 120
// indent-tabs-mode: nil
// End:
