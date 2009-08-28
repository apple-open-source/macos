/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */
/*
    AutoEnvironment.cpp
    Environment Variables
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#include <errno.h>
#include "AutoDefs.h"
#include "AutoEnvironment.h"
#include "auto_impl_utilities.h"

namespace Auto {

    //
    // read_long
    //
    // Read a long (integer) value from the environment variable given by var.
    // Return the value, or returns default_value if var is unset.
    // msg is an optional descriptive message indicating the effect of a non-default value for var
    //
    uint32_t Environment::read_long(const char *var, uint32_t default_value, const char *msg) {
        uint32_t result = default_value;
        const char *s = getenv(var);
        if (s) {
            uint32_t parsed = strtol(s, NULL, 10);
            if (parsed != 0 || errno != EINVAL) {
                result = parsed;
                if (result != default_value)
                    malloc_printf("%s: %s = \"%s\" in environment. %s\n", auto_prelude(), var, s, msg ?: "");
            }
        }
        return result;
    }
    
    //
    // read_bool
    //
    // Read a boolean value from the environment variable given by var.
    // Returns default_value if var is not set in the environment.
    // Returns true if var is set to an empty string
    // Returns true if var is set to "yes" or "true" (case insensitive).
    // Returns false if var is set to "no" or "false".
    // msg is an optional descriptive message indicating the effect of a non-default value for var
    //
    bool Environment::read_bool(const char *var, bool default_value, const char *msg) {
        bool result = default_value;
        const char *s = getenv(var);
        if (s) {
            if (strlen(s) == 0)
                result = true;
            else if (strcasecmp(s, "yes") == 0 || strcasecmp(s, "true") == 0 || strcmp(s, "1") == 0)
                result = true;
            else if (strcasecmp(s, "no") == 0 || strcasecmp(s, "false") == 0 || strcmp(s, "0") == 0)
                result = false;
            if (result != default_value)
                malloc_printf("%s: %s = \"%s\" in environment. %s\n", auto_prelude(), var, s, msg ?: "");
        }
        return result;
    }
    
#if defined(DEBUG)
    bool Environment::clear_all_new;                          // clear all new blocks
    bool Environment::dirty_all_new;                          // dirty all new blocks
    bool Environment::unsafe_scan;                            // perform final scan when threads are not suspended
    bool Environment::print_stats;                            // print statistics after collection
    bool Environment::print_scan_stats;                       // print scanning statistics
    bool Environment::print_allocs;                           // print vm and malloc allocations and deallocations
    uint32_t Environment::local_allocations_size_limit;       // maximum number of local blocks allowed (per thread)
#endif

    bool Environment::guard_pages;                           // create guard pages for all small/medium blocks.
    bool Environment::dirty_all_deleted;                     // dirty all deleted blocks
    bool Environment::enable_monitor;                        // enable the external debug monitor
    bool Environment::use_exact_scanning;                    // trust exact layouts when scanning
    bool Environment::thread_collections;                    // enable thread local collections
    bool Environment::log_reference_counting;                // log reference counting activity
    uint32_t Environment::exhaustive_collection_limit;       // max # of full collections in an exhaustive collection
    bool Environment::resurrection_is_fatal;                 // true if resurrection is a fatal error

    //
    // initialize
    //
    // Reads the environment variables values.
    //
    void Environment::initialize() {
#if defined(DEBUG)
        clear_all_new     = read_bool("AUTO_CLEAR_ALL_NEW", false);
        dirty_all_new     = read_bool("AUTO_DIRTY_ALL_NEW", false);
        unsafe_scan       = read_bool("AUTO_UNSAFE_SCAN", false);
        print_stats       = read_bool("AUTO_PRINT_STATS", false);
        print_scan_stats  = read_bool("AUTO_SCAN_PRINT_STATS", false);
        local_allocations_size_limit = read_long("AUTO_THREAD_LOCAL_BLOCK_LIMIT", 2000);
#endif
        guard_pages       = read_bool("AUTO_USE_GUARDS", false, "Guard pages are enabled.  Application will be slower and use more memory. Buffer overruns in the Auto zone will be caught.");
        dirty_all_deleted = read_bool("AUTO_DIRTY_ALL_DELETED", false, "Deleted objects will be dirtied by the collector (similar to MallocScribble).") || read_long("MallocScribble", false, "Deleted objects will be dirtied by the collector.");
        enable_monitor    = read_bool("AUTO_ENABLE_MONITOR", false);
        use_exact_scanning = read_bool("AUTO_USE_EXACT_SCANNING", true, "Exact scanning disabled. Additional references may be found.");
        thread_collections = read_bool("AUTO_USE_TLC", true, "Thread local collector [TLC] disabled.");
        log_reference_counting = read_bool("AUTO_REFERENCE_COUNT_LOGGING", false, "Reference count logging enabled.");
        exhaustive_collection_limit = read_long("AUTO_EXHAUSTIVE_COLLECTION_LIMIT", 8);
        resurrection_is_fatal = read_bool("AUTO_RESURRECTION_ABORT", true, "Resurrections errors will not be treated as fatal. This may lead to heap inconsistencies and crashes, possibly long after the resurrection occurs.");
    }
};
