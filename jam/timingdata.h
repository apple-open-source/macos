/*
 *  timingdata.h
 *  jam
 *
 *  Created by Dave Payne on Sun Mar 23 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _TIMINGDATA_H_
#define _TIMINGDATA_H_

#ifdef APPLE_EXTENSIONS

#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>       // for last command's resource usage statistics

typedef struct _timing_data  TIMING_DATA;
typedef struct _timing_entry TIMING_ENTRY;

struct _timing_entry
{
    TIMING_ENTRY    *next;                  // pointer to next timing entry
    char            *rule_name;             // name of the jam rule for this command
    char            *source_file;           // name of the source file processing, if cmd has one
    char            *obj_file;              // name of the object file or 'target'
    int             cmd_slot;               // slot in which this command was executed
    double          time_at_start;          // time at which command was started
    double          time_at_end;            // time at which command finished
    double          user_time;              // CPU user time taken by this command
    double          sys_time;               // CPU system time taken by this command
    int             pageins_at_start;       // number of pageins that had happened when we started this command
    int             pageins_at_end;         // number of pageins that had happened when this command ended
    int             pageouts_at_start;      // number of pageouts that had happened when we started this command
    int             pageouts_at_end;        // number of pageouts that had happened when this command ended
} ;

struct _timing_data {
    char            *build_target_name;
    double          build_start_time;       // time at which entire build was started
    int             build_start_pageins;    // number of pageins that had happened in system when we started build
    int             build_start_pageouts;   // number of pageouts that had happened in system when we started build
    double          last_cmd_user_time;     // temporary recording of CPU user time from last command that finished
    double          last_cmd_sys_time;      // temporary recording of CPU system time from last command that finished
    double          total_user_time;        // user time of all commands so far, summed up
    double          total_sys_time;         // sys time of all commands so far, summed up
    TIMING_ENTRY    *timing_list_head;      // pointer to first timing entry
    TIMING_ENTRY    *timing_list_tail;      // pointer to last timing entry
    int             num_entries;            // number of entries in list
};

# define SECONDS_FROM_TIMEVAL(timeval)	((((double) (timeval).tv_sec) * 1000000.0 + (timeval).tv_usec) / 1000000.0)


void init_timing_data();
void set_timing_target_name( char *target_name );
TIMING_ENTRY *create_timing_entry();
void record_last_resource_usage( struct rusage *ru );
void append_timing_entry( TIMING_ENTRY *timing_entry, int cmd_slot, char * rule, char * source_file, char * obj_file );
void print_timing_data();

#endif

#endif

