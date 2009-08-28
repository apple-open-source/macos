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
    AutoEnvironment.h
    Environment Variables
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma
#ifndef __AUTO_ENVIRONMENT__
#define __AUTO_ENVIRONMENT__

namespace Auto {


    class Environment {
    
      public:
#if defined(DEBUG)
        static bool clear_all_new;                          // clear all new blocks
        static bool dirty_all_new;                          // dirty all new blocks
        static bool unsafe_scan;                            // perform final scan when threads are not suspended
        static bool print_stats;                            // print statistics after collection
        static bool print_scan_stats;                       // print scanning statistics
        static bool print_allocs;                           // print vm and malloc allocations and deallocations
        static uint32_t local_allocations_size_limit;       // maximum number of local blocks allowed (per thread)
#else
        enum {
            clear_all_new = 0,                             // clear all new blocks
            dirty_all_new = 0,                             // dirty all new blocks
            unsafe_scan = 0,                               // perform final scan when threads are not suspended
            print_stats = 0,                               // print statistics after collection
            print_scan_stats = 0,                          // print scanning statistics
            print_allocs = 0,                              // print vm and malloc allocations and deallocation
            local_allocations_size_limit = 2000,           // maximum number of local blocks allowed (per thread)
        };
#endif
        static bool guard_pages;                           // create guard pages for blocks >= page_size
        static bool dirty_all_deleted;                     // dirty all deleted blocks
        static bool enable_monitor;                        // enable the external debug monitor
        static bool use_exact_scanning;                    // trust exact layouts when scanning
        static bool thread_collections;                    // enable thread local collections
        static bool log_reference_counting;                // log reference counting activity
        static uint32_t exhaustive_collection_limit;       // max # of full collections in an exhaustive collection
        static bool resurrection_is_fatal;                 // true if resurrection is a fatal error
        
        //
        // initialize
        //
        // Reads the environment variables values.
        //
        static void initialize();
      
        //
        // read_long
        //
        // Read a long (integer) value from the environment variable given by var.
        // Return the value, or returns default_value if var is unset.
        // msg is an optional descriptive message indicating the effect of a non-default value for var
        //
        static uint32_t read_long(const char *var, uint32_t default_value, const char *msg = NULL);
        
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
        static bool read_bool(const char *var, bool default_value, const char *msg = NULL);
            
    };
};

#endif // __AUTO_ENVIRONMENT__
