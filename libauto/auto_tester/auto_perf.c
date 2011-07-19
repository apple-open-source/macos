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
    auto_perf.c
    Performance utilities.
    Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 */

#include <stdio.h>
#include "auto_zone.h"
#include <mach/mach_time.h>
#include <mach/clock_types.h>
#include <time.h>

auto_zone_t *azone;
mach_timebase_info_data_t timebase;

uint64_t duration(uint64_t start, uint64_t end) {
    return (end - start) * timebase.numer / timebase.denom;
}

char *duration_description(uint64_t time, char *buf, int bufsz) {
    static char *suffixes[] = { "ns", "us", "ms", "s"};
    int divisor = 1;
    int suffix = 0;

    while (time/divisor > 1000 && suffix < sizeof(suffixes)/sizeof(void *)) {
        divisor *= 1000;
        suffix++;
    }
    
    snprintf(buf, bufsz, "%3.3g %s", (float)time/divisor, suffixes[suffix]);
    return buf;
}

void test_duration_description() {
    char buf[32];
    int d=1;
    uint64_t start, end;
    struct timespec sleep_time;
    
    for (int i=0; i<10; i++) {
        sleep_time.tv_sec = d/NSEC_PER_SEC;
        sleep_time.tv_nsec = d % NSEC_PER_SEC;
        start = mach_absolute_time();
        nanosleep(&sleep_time, NULL);
        end = mach_absolute_time();
        printf("sleep of %d ns ==> %s\n", d, duration_description(duration(start, end), buf, sizeof(buf)));
        d *= 10;
    }
}

void initialize() {
    azone = auto_zone_create("perf test zone");
    mach_timebase_info(&timebase);
}

// executes test many times and returns the speed in iterations per second
void measure(uint64_t *reps, uint64_t *time, void (^test)()) {
    const uint64_t MIN_MEASURE_TIME = ((uint64_t)NSEC_PER_SEC);
    uint64_t start, end;
    uint64_t repetitions = 1, d, new_reps;
    
    do {
        start = mach_absolute_time();
        for (uint64_t i=0; i<repetitions; i++) {
            test();
        }
        end = mach_absolute_time();
        d = duration(start, end);
        if (d < MIN_MEASURE_TIME) {
            if (d < MIN_MEASURE_TIME/1000) {
                new_reps = repetitions * 10;
            } else {
                new_reps = (MIN_MEASURE_TIME+MIN_MEASURE_TIME/100) * repetitions / d;
            }
            //printf("d = %llu, reps = %llu, new_reps = %llu\n", d, repetitions, new_reps);
            repetitions = new_reps;
        }
    } while (d < MIN_MEASURE_TIME);
    *reps = repetitions;
    *time = d;
}

void log_result(char *name, uint64_t repetitions, uint64_t time) {
    char buf[32];
    printf("%s: %ld repetitions in %s = %ld per second\n", name, (long)repetitions, duration_description(time, buf, sizeof(buf)), (long)(repetitions * NSEC_PER_SEC / time));
}

void measure_auto_zone_set_write_barrier() {
    void *object1 = auto_zone_allocate_object(azone, sizeof(void *), AUTO_MEMORY_SCANNED, 0, 0);
    void *object2 = auto_zone_allocate_object(azone, sizeof(void *), AUTO_MEMORY_SCANNED, 0, 0);
    uint64_t reps, time;
    measure(&reps, &time, ^{
        auto_zone_set_write_barrier(azone, object1, object2);
    });
    log_result("auto_zone_set_write_barrier", reps, time);
}

void measure_subzone_refcounting() {
    void *o = auto_zone_allocate_object(azone, sizeof(void *), AUTO_MEMORY_SCANNED, 0, 0);
    uint64_t reps, time;
    
    measure(&reps, &time, ^{
        auto_zone_retain(azone, o);
    });
    log_result("auto_zone_retain(subzone)", reps, time);

    // retain a lot so we can do the release test without underflow
    for (int i=0; i<reps; i++)
        auto_zone_retain(azone, 0);

    measure(&reps, &time, ^{
        auto_zone_retain_count(azone, o);
    });
    log_result("auto_zone_retain_count(subzone)", reps, time);
    
    measure(&reps, &time, ^{
        auto_zone_release(azone, o);
    });
    log_result("auto_zone_release(subzone)", reps, time);
}

int main(int argc, char *argv[])
{
    initialize();
    
    //test_duration_description();
    measure_auto_zone_set_write_barrier();
    measure_subzone_refcounting();
    
    return 0;
}