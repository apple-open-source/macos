/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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
    Environment.cpp
    Environment Variables
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#include <errno.h>
#include <crt_externs.h>

#include "Definitions.h"
#include "Environment.h"
#ifndef AUTO_TESTER
#include "auto_impl_utilities.h"
#endif

namespace Auto {
#if defined(DEBUG)
    bool Environment::clear_all_new;                          // clear all new blocks
    bool Environment::dirty_all_new;                          // dirty all new blocks
    bool Environment::print_stats;                            // print statistics after collection
    bool Environment::print_scan_stats;                       // print scanning statistics
    bool Environment::print_allocs;                           // print vm and malloc allocations and deallocations
    bool Environment::unscanned_store_warning;                // warn when GC-managed pointers stored in unscanned memory.
#endif

    bool Environment::guard_pages;                            // create guard pages for all small/medium blocks.
    bool Environment::dirty_all_deleted;                      // dirty all deleted blocks
    bool Environment::thread_collections;                     // enable thread local collections
    bool Environment::log_reference_counting;                 // log reference counting activity
    bool Environment::log_compactions;                        // log compaction activity
    bool Environment::scramble_heap;                          // move all possible objects during compaction
    uint32_t Environment::exhaustive_collection_limit;        // max # of full collections in an exhaustive collection
    bool Environment::resurrection_is_fatal;                  // true if resurrection is a fatal error
    bool Environment::environ_has_auto_prefix;                // true if any strings in environ have the AUTO_ prefix.
    bool Environment::environ_has_malloc_prefix;              // true if any strings in environ have the Malloc prefix.
    double Environment::default_duty_cycle;                   // default collector duty cycle.
    

    const char *Environment::get(const char *name) {
        if ((environ_has_auto_prefix && strncmp(name, "AUTO_", 5) == 0) ||
            (environ_has_malloc_prefix && strncmp(name, "Malloc", 5) == 0)) {
                return ::getenv(name);
        }
        return NULL;
    }

    //
    // read_long
    //
    // Read a long (integer) value from the environment variable given by var.
    // Return the value, or returns default_value if var is unset.
    // msg is an optional descriptive message indicating the effect of a non-default value for var
    //
    long Environment::read_long(const char *var, long default_value, const char *msg) {
        long result = default_value;
        const char *s = Environment::get(var);
        if (s) {
            long parsed = strtol(s, NULL, 10);
            if (parsed != 0 || errno != EINVAL) {
                result = parsed;
#ifndef AUTO_TESTER
                if (result != default_value)
                    malloc_printf("%s: %s = \"%s\" in environment. %s\n", auto_prelude(), var, s, msg ?: "");
#endif
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
        const char *s = Environment::get(var);
        if (s) {
            if (strlen(s) == 0)
                result = true;
            else if (strcasecmp(s, "yes") == 0 || strcasecmp(s, "true") == 0 || strcmp(s, "1") == 0)
                result = true;
            else if (strcasecmp(s, "no") == 0 || strcasecmp(s, "false") == 0 || strcmp(s, "0") == 0)
                result = false;
#ifndef AUTO_TESTER
            if (result != default_value)
                malloc_printf("%s: %s = \"%s\" in environment. %s\n", auto_prelude(), var, s, msg ?: "");
#endif
        }
        return result;
    }
    
    static void prescan_environment() {
        for (char **env = *_NSGetEnviron(); *env != NULL; ++env) {
            if (strncmp(*env, "AUTO_", 5) == 0)
                Environment::environ_has_auto_prefix = true;
            else if (strncmp(*env, "Malloc", 6) == 0)
                Environment::environ_has_malloc_prefix = true;
        }
    }
    
    //
    // initialize
    //
    // Reads the environment variables values.
    //
    void Environment::initialize() {
        prescan_environment();
    
#if defined(DEBUG)
        clear_all_new     = read_bool("AUTO_CLEAR_ALL_NEW", false);
        dirty_all_new     = read_bool("AUTO_DIRTY_ALL_NEW", false);
        print_stats       = read_bool("AUTO_PRINT_STATS", false);
        print_scan_stats  = read_bool("AUTO_SCAN_PRINT_STATS", false);
        unscanned_store_warning = read_bool("AUTO_UNSCANNED_STORE_WARNING", false, "Unscanned store warnings enabled.");
#endif
        guard_pages       = read_bool("AUTO_USE_GUARDS", false, "Guard pages are enabled.  Application will be slower and use more memory. Buffer overruns in the Auto zone will be caught.");
        dirty_all_deleted = read_bool("AUTO_DIRTY_ALL_DELETED", false, "Deleted objects will be dirtied by the collector (similar to MallocScribble).") || read_long("MallocScribble", false, "Deleted objects will be dirtied by the collector.");
        thread_collections = read_bool("AUTO_USE_TLC", true, "Thread local collector [TLC] disabled.");
        log_reference_counting = read_bool("AUTO_REFERENCE_COUNT_LOGGING", false, "Reference count logging enabled.");
        log_compactions = read_bool("AUTO_COMPACTION_LOGGING", false, "Compaction logging enabled.");
        scramble_heap = read_bool("AUTO_COMPACTION_SCRAMBLE", false, "Heap scrambling enabled.");
        exhaustive_collection_limit = read_long("AUTO_EXHAUSTIVE_COLLECTION_LIMIT", 8);
        resurrection_is_fatal = read_bool("AUTO_RESURRECTION_ABORT", true, "Resurrections errors will not be treated as fatal. This may lead to heap inconsistencies and crashes, possibly long after the resurrection occurs.");
        default_duty_cycle = (double)read_long("AUTO_DUTY_CYCLE", 25) / 100.0;
        if (default_duty_cycle < 0.0001 || default_duty_cycle > 1.0) {
            default_duty_cycle = 0.25;
#ifndef AUTO_TESTER
            malloc_printf("%s: Invalid value for AUTO_DUTY_CYCLE. Using default.\n", auto_prelude());
#endif
        }
    }
}
