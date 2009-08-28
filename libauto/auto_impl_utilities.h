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
    auto_impl_utilities.h
    Implementation Utilities
    Copyright (c) 2002-2008 Apple Inc. All rights reserved.
*/

#ifndef __AUTO_IMPL_UTILITIES__
#define __AUTO_IMPL_UTILITIES__

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <mach/mach_time.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>

#include <AvailabilityMacros.h>

#include "auto_zone.h"

__BEGIN_DECLS

/*********  Debug options   ************/

#define DEBUG_IMPL  1
#if DEBUG_IMPL
#define INLINE  
#else
#define INLINE inline
#endif

/*********  Various types   ************/

typedef long spin_lock_t;

/*********  ptr set utilities ************/
//
// Pointer sets are used to track the use of allocated objects.
//

typedef struct ptr_set ptr_set;

extern ptr_set *ptr_set_new();
extern void ptr_set_dispose(ptr_set *set);
extern void ptr_set_add(ptr_set *set, void *ptr);
extern int ptr_set_is_member_no_lock(ptr_set *set, void *ptr);
extern int ptr_set_is_member(ptr_set *set, void *ptr);
extern void ptr_set_remove(ptr_set *set, void *ptr);

typedef struct ptr_map ptr_map;

extern ptr_map *ptr_map_new();
extern void ptr_map_set(ptr_map *map, void *key, void *value);
extern void *ptr_map_get(ptr_map *map, void *key);
extern void *ptr_map_remove(ptr_map *map, void *key);


/*********  zone definition ************/

typedef spin_lock_t auto_lock_t;             // must be a spin lock, to be compatible with fork()

#define AUTO_ZONE_VERSION           5                                       // stored in malloc_zone_t version field, so that
                                                                            // zone enumeration can validate the data structures.

/*********  Malloc Logging (see Libc/gen/malloc.c) ************/

typedef void (malloc_logger_t)(uint32_t type_flags, uintptr_t zone_ptr, uintptr_t size, uintptr_t ptr_arg, uintptr_t return_val, uint32_t num_hot_to_skip);
extern malloc_logger_t *malloc_logger;

#define MALLOC_LOG_TYPE_ALLOCATE        2
#define MALLOC_LOG_TYPE_DEALLOCATE      4
#define MALLOC_LOG_TYPE_HAS_ZONE        8
#define MALLOC_LOG_TYPE_CLEARED         64

/*********  Locking ************/

extern int __is_threaded;
extern void _spin_lock(spin_lock_t *lockp);
extern int  _spin_lock_try(spin_lock_t *lockp);
extern void _spin_unlock(spin_lock_t *lockp);

inline void spin_lock(spin_lock_t *lockp) { if (__is_threaded) _spin_lock(lockp); }
inline int spin_lock_try(spin_lock_t *lockp) { return (__is_threaded ? _spin_lock_try(lockp) : 1); }
inline void spin_unlock(spin_lock_t *lockp) { if (__is_threaded) _spin_unlock(lockp); }

/*********  Implementation utilities    ************/

extern vm_address_t auto_get_sp();

size_t auto_round_page(size_t size);
    // rounds up to an integer page size

extern const char *auto_prelude(void);
    // returns the prelude string, that contains the pid, to be logged in every log

extern void auto_error(void *azone, const char *msg, const void *ptr);
extern void auto_fatal(const char *format, ...);

/*********  Dealing with time   ************/

static inline auto_date_t auto_date_now(void) {
#if 0
    return mach_absolute_time();
#elif 0
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return ((auto_date_t)usage.ru_utime.tv_sec * (auto_date_t)1000000) + (auto_date_t)usage.ru_utime.tv_usec +
           ((auto_date_t)usage.ru_stime.tv_sec * (auto_date_t)1000000) + (auto_date_t)usage.ru_stime.tv_usec;
#else
    thread_basic_info_data_t myinfo;
    unsigned int count = sizeof(myinfo);
    thread_info(pthread_mach_thread_np(pthread_self()), THREAD_BASIC_INFO, (thread_info_t)&myinfo, &count);
/*
struct thread_basic_info
{
       time_value_t     user_time;
       time_value_t   system_time;
       integer_t        cpu_usage;
       policy_t            policy;
       integer_t        run_state;
       integer_t            flags;
       integer_t    suspend_count;
       integer_t       sleep_time;
};
struct time_value {
        integer_t seconds;
        integer_t microseconds;
};

*/

   return ((auto_date_t)myinfo.user_time.seconds*(auto_date_t)1000000) + (auto_date_t)myinfo.user_time.microseconds +
          ((auto_date_t)myinfo.system_time.seconds*(auto_date_t)1000000) + (auto_date_t)myinfo.system_time.microseconds;
#endif
}

extern double auto_time_interval(auto_date_t after, auto_date_t before);
    // returns duration in seconds
    // use auto_time_interval(duration, 0) if already a duration

/*********  Collection definition   ************/

/*
static inline auto_date_t auto_collection_time() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * (auto_date_t)1000000) + (auto_date_t)tv.tv_usec;
}
*/

enum {
    AUTO_COLLECTION_STATUS_INTERRUPT = -1,
    AUTO_COLLECTION_STATUS_ERROR = 0,
    AUTO_COLLECTION_STATUS_OK = 1
};

/*********  Internal allocation ************/

extern malloc_zone_t *aux_zone;

extern void aux_init(void);

static inline void *aux_malloc(size_t size) {
    return malloc_zone_malloc(aux_zone, size);
}

static inline void *aux_calloc(size_t count, size_t size) {
    return malloc_zone_calloc(aux_zone, count, size);
}

static inline void *aux_valloc(size_t size) {
    return malloc_zone_valloc(aux_zone, size);
}

static inline void *aux_realloc(void *ptr, size_t size) {
    return malloc_zone_realloc(aux_zone, ptr, size);
}

static inline void aux_free(void *ptr) {
    return malloc_zone_free(aux_zone, ptr);
}

/*********  debug utilities ************/

extern void auto_collect_print_trace_stats(void);

extern void auto_record_refcount_stack(auto_zone_t *azone, void *ptr, int delta);
extern void auto_print_refcount_stacks(void *ptr);

__private_extern__ void auto_refcount_underflow_error(void *);
__private_extern__ void auto_zone_resurrection_error();
__private_extern__ void auto_zone_thread_local_error(void);
__private_extern__ void auto_zone_thread_registration_error(void);
__private_extern__ void auto_zone_global_data_memmove_error(void);
__private_extern__ void auto_zone_association_error(void *address);

__END_DECLS

#endif /* __AUTO_IMPL_UTILITIES__ */
