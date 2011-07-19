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
    Environment.h
    Environment Variables
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
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
        static bool print_stats;                            // print statistics after collection
        static bool print_scan_stats;                       // print scanning statistics
        static bool print_allocs;                           // print vm and malloc allocations and deallocations
        static bool unscanned_store_warning;                // warn when GC-managed pointers stored in unscanned memory.
#else
        enum {
            clear_all_new = 0,                              // clear all new blocks
            dirty_all_new = 0,                              // dirty all new blocks
            print_stats = 0,                                // print statistics after collection
            print_scan_stats = 0,                           // print scanning statistics
            print_allocs = 0,                               // print vm and malloc allocations and deallocation
            unscanned_store_warning = 0,                    // warn when GC-managed pointers stored in unscanned memory.
        };
#endif
        static bool guard_pages;                            // create guard pages for blocks >= page_size
        static bool dirty_all_deleted;                      // dirty all deleted blocks
        static bool thread_collections;                     // enable thread local collections
        static bool log_reference_counting;                 // log reference counting activity
        static bool log_compactions;                        // log compaction activity
        static bool scramble_heap;                          // move all possible objects when compacting.
        static uint32_t exhaustive_collection_limit;        // max # of full collections in an exhaustive collection
        static bool resurrection_is_fatal;                  // true if resurrection is a fatal error
        static bool environ_has_auto_prefix;                // true if any strings in environ have the AUTO_ prefix.
        static bool environ_has_malloc_prefix;              // true if any strings in environ have the Malloc prefix.
        static double default_duty_cycle;                   // default collector duty cycle
        
        //
        // initialize
        //
        // Reads the environment variables values.
        //
        static void initialize();
      
        //
        // get
        //
        // Bottleneck for all calls to getenv().
        //
        static const char *get(const char *name);
        
        //
        // read_long
        //
        // Read a long (integer) value from the environment variable given by var.
        // Return the value, or returns default_value if var is unset.
        // msg is an optional descriptive message indicating the effect of a non-default value for var
        //
        static long read_long(const char *var, long default_value, const char *msg = NULL);
        
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
        static bool read_bool(const char *var, bool default_value = false, const char *msg = NULL);
        
    };
};

#endif // __AUTO_ENVIRONMENT__
