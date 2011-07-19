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
    Statistics.h
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_STATISTICS__
#define __AUTO_STATISTICS__


#include "Definitions.h"
#include <mach/thread_act.h>
#include <assert.h>


namespace Auto {
    
    //----- Timing -----//
    
    // The Timer template provides a generic timer model. The template parameter can wrap different system timers.
    // The TimeDataSource must provide:
    //     uint64_t current_time(void) - return the current time, using arbitrary time units
    //     uint64_t microseconds_duration(uint64_t start, uint64_t end) - compute the duration in microseconds between start and end

    template <class TimeDataSource> class Timer {
        TimeDataSource _timeDataSource; // The timer data source.
        volatile uint64_t _start;       // The absolute time when the timer was last started (0 if the timer is not running)
        volatile int64_t _accumulated;  // The total accumulated time, in microseconds
        char _description[8];           // A string buffer used to return a textual representation of the recorded time
        
    public:
        Timer() : _start(0), _accumulated(0) {}
        
        // Test whether the timer is currently running.
        inline boolean_t timer_running() const { return _start != 0; }

        // Start the timer.
        void start()                        { assert(!timer_running()); _start = _timeDataSource.current_time(); }

        // Stop the timer. The duration since the last call to start is added to the accumulated time.
        void stop()                         { assert(timer_running()); add_time(_timeDataSource.microseconds_duration(_start, _timeDataSource.current_time())); _start = 0; }

        // Clear the accumulated time.
        void reset()                        { assert(!timer_running()); _accumulated = 0; }
        
        // Add time to the timer. duration is in microseconds.
        void add_time(usword_t duration)    { OSAtomicAdd64(duration, &_accumulated); }

        // Add the accumulated time from another timer to this timer.
        void add_time(Timer &other)         { OSAtomicAdd64(other.microseconds(), &_accumulated); }

        // Return the accumulated time in microseconds.
        int64_t microseconds() const        { assert(!timer_running()); return _accumulated; }
        
        // Returns the current elapsed time on a running timer. Does not stop the timer.
        // Returns zero if the timer is running. 
        int64_t elapsed_microseconds() { 
            int64_t start = _start;
            return start != 0 ? _timeDataSource.microseconds_duration(start, _timeDataSource.current_time()) : 0;
        }
    
        // Return a formatted textual representation of the accumulated time.
        // The returned buffer lives in the object being queried.
        const char *time_string()                 {
            int64_t timeval = microseconds();
            if (timeval < 999) {
                snprintf(_description, sizeof(_description), "%4.3g us", (float)timeval);
            } else if (timeval < 999999) {
                snprintf(_description, sizeof(_description), "%4.3g ms", (float)timeval/1000);
            } else {
                snprintf(_description, sizeof(_description), "%4.3g s", (float)timeval/1000/1000);
            }
            return _description;
        }
    };
    
#if 0
    // A timer data source that measures time based on the user cpu time charged to the calling thread.
    // This data source is relatively expensive in that it requires a kernel call.
    class UserCPUTimeDataSource {
    public:
        inline uint64_t current_time(void) {
            thread_basic_info_data_t myinfo;
            unsigned int count = sizeof(myinfo);
            thread_info(pthread_mach_thread_np(pthread_self()), THREAD_BASIC_INFO, (thread_info_t)&myinfo, &count);
            return (int64_t)myinfo.user_time.seconds*1000000 + (int64_t)myinfo.user_time.microseconds;
        }
        inline uint64_t microseconds_duration(uint64_t start, uint64_t end) { return end - start; }
    };
    typedef Timer<UserCPUTimeDataSource> UserCPUTimer;
#endif
    
    // A timer data source that measures elapsed wall clock time. This timer has very little cost to use (no system call).
    class WallClockTimeDataSource {
        inline static mach_timebase_info_data_t &cached_timebase() {
            static mach_timebase_info_data_t _timebase;
            if (_timebase.denom == 0) {
                mach_timebase_info(&_timebase);
                _timebase.denom *= 1000; // we're using microseconds instead of nanoseconds
            }
            return _timebase;
        }
        
    public:
        uint64_t current_time(void) { return mach_absolute_time(); }
        inline uint64_t microseconds_duration(uint64_t start, uint64_t end) { 
            mach_timebase_info_data_t &timebase = cached_timebase();
            return (end - start) * timebase.numer / timebase.denom; 
        }
        
        WallClockTimeDataSource() { }
    };
    typedef Timer<WallClockTimeDataSource> WallClockTimer;

    
    // CollectionTimer wraps up the timing statistics that are collected during a heap collection.
    class CollectionTimer {
    public:
        // Typedefs to permit changing the underlying clock for the measurement
        typedef WallClockTimer TotalCollectionTimer;
        typedef WallClockTimer ScanTimer;
    
    private:
        TotalCollectionTimer _total_time;   // timer that runs from the beginning to end of the heap collection
        ScanTimer _scan_timer;              // timer that accumulates all the time threads spend scanning
        boolean_t _scan_timer_enabled;
        
    public:
        CollectionTimer() : _scan_timer_enabled(false) {}
        
        // Accessors for the particular timers.
        inline TotalCollectionTimer &total_time()      { return _total_time; }
        inline ScanTimer &scan_timer()                 { assert(_scan_timer_enabled); return _scan_timer; }
        
        inline void enable_scan_timer()                { _scan_timer_enabled = true; }
        inline boolean_t scan_timer_enabled()          { return _scan_timer_enabled; }
    };
    
    
    //----- Statistics -----//
    
    class Statistics {
        // Number of allocated Large/Subzone blocks. Includes blocks on the thread local allocation cache.
        // (ie "allocated" is based on subzone perspective).
        volatile uint64_t _count;
        
        // total # bytes represented by blocks included in _count
        volatile uint64_t _size;

        // per heap collection statistics
        volatile uint64_t _blocks_scanned;
        volatile uint64_t _bytes_scanned;

        WallClockTimer _idle_timer;              // measures the interval between heap collections
        volatile int64_t _should_collect_interval_start; // records the last time Zone::should_check() ran, to throttle requests
        
//#define MEASURE_TLC_STATS
#ifdef MEASURE_TLC_STATS
        // These are all block counts, and don't include larges.
        volatile uint64_t _local_allocations;   // blocks allocated thread local
        volatile uint64_t _global_allocations;  // blocks allocated not thread local
        volatile uint64_t _escaped;             // count of blocks that transitioned local->global
        volatile uint64_t _local_collected;     // count of local garbage blocks
        volatile uint64_t _global_collected;    // count of global garbage blocks (excluding Large blocks)
        volatile uint64_t _recycled;            // count of locally recovered blocks
        volatile uint64_t _global_freed;        // count of local garbage which was passed to global collector to finalize/free
#endif
        
    public:
        //
        // Constructor
        //
        Statistics() { bzero(this, sizeof(Statistics)); }
        
        //
        // Reset before starting a heap collection
        //
        inline void reset_for_heap_collection() {
            _blocks_scanned = 0;
            _bytes_scanned = 0;
        }
        
        //
        // Accessors
        //
        inline uint64_t count()                     const { return _count; }
        inline uint64_t size()                      const { return _size; }
        WallClockTimer &idle_timer()                { return _idle_timer; }
        inline uint64_t blocks_scanned()        const { return _blocks_scanned; }
        inline uint64_t bytes_scanned()         const { return _bytes_scanned; }
        inline volatile int64_t *last_should_collect_time() { return &_should_collect_interval_start;}
#ifdef MEASURE_TLC_STATS
        inline void print_tlc_stats() { malloc_printf("allocations - local: %ld, global: %ld. Escaped: %ld. collected - local: %ld, global: %ld. Recovered - local: %ld, global: %ld\n", _local_allocations, _global_allocations, _escaped, _local_collected, _global_collected, _recycled, _global_freed); }
#endif
        
        //
        // Accumulators
        //
        inline void add_count(int64_t n)                { OSAtomicAdd64(n, (volatile int64_t *)&_count); }
        inline void add_size(int64_t size)              { OSAtomicAdd64(size, (volatile int64_t *)&_size); }
        inline void add_blocks_scanned(int64_t count)   { OSAtomicAdd64(count, (volatile int64_t *)&_blocks_scanned); }
        inline void add_bytes_scanned(int64_t count)    { OSAtomicAdd64(count, (volatile int64_t *)&_bytes_scanned); }
#ifdef MEASURE_TLC_STATS
        inline void add_local_allocations(int64_t n)    { OSAtomicAdd64(n, (volatile int64_t *)&_local_allocations); }
        inline void add_global_allocations(int64_t n)   { OSAtomicAdd64(n, (volatile int64_t *)&_global_allocations); }
        inline void add_escaped(int64_t n)              { OSAtomicAdd64(n, (volatile int64_t *)&_escaped); }
        inline void add_local_collected(int64_t n)      { OSAtomicAdd64(n, (volatile int64_t *)&_local_collected); }
        inline void add_global_collected(int64_t n)     { OSAtomicAdd64(n, (volatile int64_t *)&_global_collected); }
        inline void add_recycled(int64_t n)             { OSAtomicAdd64(n, (volatile int64_t *)&_recycled); }
        inline void add_global_freed(int64_t n)         { OSAtomicAdd64(n, (volatile int64_t *)&_global_freed); }
#endif
    };
};

#endif // __AUTO_STATISTICS__

