/*
 *  timingdata.c
 *  jam
 *
 *  Created by Dave Payne on Sun Mar 23 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#ifdef APPLE_EXTENSIONS

#include "timingdata.h"
#include "string.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <mach/mach.h>		// for system vm statistics
#include <sys/resource.h>       // for last command's resource usage statistics

static TIMING_DATA timing_data = {
    NULL,       // build_target_name
    0.0,        // build_start_time
    0,          // build_start_pageins
    0,          // build_start_pageouts
    0.0,        // last_cmd_user_time
    0.0,        // last_cmd_sys_time
    0.0,        // total_user_time
    0.0,        // total_sys_time
    NULL,       // timing_list_head
    NULL,       // timing_list_tail
    0           // num_entries
};


static void get_vm_stats( struct vm_statistics * stat );
static TIMING_ENTRY ** sort_timing_data();
static int compare_duration( const void *first_entry, const void *second_entry );
static void print_timing_entry( TIMING_ENTRY *timing_entry );
static void print_build_phase_header();
static void print_build_phase_times();
static void print_cmd_times_header();
static void print_cmd_times();
static int get_num_lines( char *filename );
static int get_file_size( char *filename );


//
// Routines to collect and store the timing data.
//

// Initialize time stamps and vm stats.
void init_timing_data()
{
    struct timeval current_time;
    struct vm_statistics vm_stats;
    
    gettimeofday( &current_time, (struct timezone *) NULL );
    timing_data.build_start_time = SECONDS_FROM_TIMEVAL( current_time );
    
    get_vm_stats( &vm_stats );
    timing_data.build_start_pageins = vm_stats.pageins;
    timing_data.build_start_pageouts = vm_stats.pageouts;
}


void set_timing_target_name( char *target_name )
{
    if ( target_name ) {
        timing_data.build_target_name = malloc( strlen(target_name) + 1 );
        strcpy( timing_data.build_target_name, target_name );
        
        printf( "TIME: ==========  BUILD TIMING INFO FOR TARGET '%s'  ==========\n", target_name );
    }    
}


// create_timing_entry() should be called at the start of an event to be timed.
// The caller is responsible for holding the pointer to the TIMING_ENTRY
// that is returned.  This TIMING_ENTRY records various interesting
// statistics at the start of the event.  When the event is complete,
// append_timing_entry() should be called to add this event to the queue
// of completed events.
TIMING_ENTRY *create_timing_entry()
{
    TIMING_ENTRY *timing_entry = (TIMING_ENTRY *) malloc( sizeof( TIMING_ENTRY ) );
    struct timeval current_time;
    struct vm_statistics vm_stats;

    timing_entry->rule_name = NULL;
    timing_entry->source_file = NULL;
    timing_entry->obj_file = NULL;
    
    get_vm_stats( &vm_stats );
    timing_entry->pageins_at_start = vm_stats.pageins;
    timing_entry->pageouts_at_start = vm_stats.pageouts;
    
    // Start timing as late as possible.
    gettimeofday( &current_time, (struct timezone *) NULL );
    timing_entry->time_at_start = SECONDS_FROM_TIMEVAL( current_time );
    
    return timing_entry;
}


// This routine can be called to temporarily store resource usage statistics
// for the last command.  It is expected that append_timing_entry() will be
// called soon after this, but it may not be convenient to call it at the
// time that we have the resource usage stats.
void record_last_resource_usage( struct rusage *ru )
{
    timing_data.last_cmd_user_time = SECONDS_FROM_TIMEVAL( ru->ru_utime );
    timing_data.last_cmd_sys_time = SECONDS_FROM_TIMEVAL( ru->ru_stime );
}


// append_timing_entry() should be called when an event being timed is
// complete.  This routine collects more statistics, and adds the
// TIMING_ENTRY to the list of completed events.
void append_timing_entry( TIMING_ENTRY *timing_entry, int cmd_slot,
                          char *rule, char *source_file, char *obj_file  )
{
    struct timeval current_time;
    struct vm_statistics vm_stats;
    
    // Stop timing ASAP.
    gettimeofday( &current_time, (struct timezone *) NULL );
    timing_entry->time_at_end = SECONDS_FROM_TIMEVAL( current_time );
    
    get_vm_stats( &vm_stats );
    timing_entry->pageins_at_end = vm_stats.pageins;
    timing_entry->pageouts_at_end = vm_stats.pageouts;
    
    timing_entry->user_time = timing_data.last_cmd_user_time;
    timing_data.total_user_time += timing_entry->user_time;
    timing_entry->sys_time = timing_data.last_cmd_sys_time;
    timing_data.total_sys_time += timing_entry->sys_time;
    timing_data.last_cmd_user_time = 0.0;
    timing_data.last_cmd_sys_time = 0.0;
    
    if ( rule ) {
        timing_entry->rule_name = malloc( strlen(rule) + 1 );
        strcpy( timing_entry->rule_name, rule );
    }
    if ( source_file ) {
        timing_entry->source_file = malloc( strlen(source_file) + 1 );
        strcpy( timing_entry->source_file, source_file );
    }
    if ( obj_file ) {
        timing_entry->obj_file = malloc( strlen(obj_file) + 1 );
        strcpy( timing_entry->obj_file, obj_file );
    }
    timing_entry->cmd_slot = cmd_slot;
    
    if ( ! timing_data.timing_list_head ) {
        timing_data.timing_list_head = timing_data.timing_list_tail = timing_entry;
    } else {
        timing_data.timing_list_tail->next = timing_entry;
        timing_data.timing_list_tail = timing_entry;
    }
    timing_entry->next = NULL;
    timing_data.num_entries++;
    
    print_timing_entry( timing_entry );
}


// Print the timing entry.  Put a blank line between it and the previous command info,
// because on a dual processor this often will be a time stamp for an earlier command
// than the last one we started.
static void print_timing_entry( TIMING_ENTRY *timing_entry )
{
    char *argument;
    char phase_name[256];

    printf( "\n" );

    if ( strncmp( timing_entry->rule_name, "BuildPhase", strlen( "BuildPhase" ) ) == 0 ) {        
        if ( timing_entry->obj_file == NULL ) {
            argument = "";
        } else if ( sscanf( timing_entry->obj_file, "<%255[^>]>", phase_name ) == 1 ) {
            argument = phase_name;
        } else {
            argument = timing_entry->obj_file;
        }
        printf( "TIME: %.2f elapsed;  Completed %s\n",
                timing_entry->time_at_end - timing_data.build_start_time, argument );
    } else {
        if ( strncmp( timing_entry->rule_name, "Compile", strlen( "Compile" ) ) == 0 && timing_entry->source_file ) {
            argument = timing_entry->source_file;
        } else if ( timing_entry->obj_file ) {
            char *p = strrchr( timing_entry->obj_file, '/' );
            p = ( p != NULL ) ? ( p + 1 ) : timing_entry->obj_file;
            argument = p;
        } else {
            argument = "";
        }
        
        printf( "TIME: %.2f elapsed;  %.2fr %.2fu %.2fs  pagein/out: %d / %d  cmd_slot: %d  %s %s\n",
                timing_entry->time_at_end - timing_data.build_start_time,
                timing_entry->time_at_end - timing_entry->time_at_start,
                timing_entry->user_time, timing_entry->sys_time,
                timing_entry->pageins_at_end - timing_entry->pageins_at_start,
                timing_entry->pageouts_at_end - timing_entry->pageouts_at_start,
                timing_entry->cmd_slot,
                timing_entry->rule_name, argument );
    }
}
    


// This routine was gleaned from the vm_stats utility.  The
// vm_statistics struct is defined in <mach/vm_statistics.h>
static void get_vm_stats( struct vm_statistics * stat )
{
    static mach_port_t myHost;
    int count = HOST_VM_INFO_COUNT;
    
    if ( ! myHost )
        myHost = mach_host_self();
    host_statistics( myHost, HOST_VM_INFO, (host_info_t) stat, &count);
}



//
// Routines to print the timing data.
//

void print_timing_data()
{
    if ( timing_data.num_entries > 0 ) {
        if ( timing_data.build_target_name == NULL ) {
            timing_data.build_target_name = "unknown";
        }
        printf( "TIME: \n" );
        printf( "TIME: ==========  BUILD TIMING SUMMARY FOR TARGET '%s'  ==========\n", timing_data.build_target_name );
        printf( "TIME: \n" );
        print_build_phase_header();
        print_build_phase_times();
        printf( "TIME: \n" );
        print_cmd_times_header();
        print_cmd_times();
        printf( "TIME: \n" );
        printf( "TIME:    TOTAL BUILD TIME FOR TARGET '%s':  %8.2f real   %8.2f user   %8.2f sys\n",
                timing_data.build_target_name,
                timing_data.timing_list_tail->time_at_end - timing_data.build_start_time,
                timing_data.total_user_time, timing_data.total_sys_time );
        printf( "TIME: \n" );
        printf( "TIME: \n" );
        printf( "TIME: \n" );
        printf( "To extract TIME info for all targets, run \"grep TIME: <logfile> | grep -v 'extract TIME' | sed 's/^TIME: //'\"\n" );
    }
}

static void print_build_phase_header()
{
    printf( "TIME:   PCNT      REAL      USER       SYS   PAGEIN /    OUT   SUMMARY OF BUILD PHASES\n" );
//    printf( "TIME:   ====      ====      ====       ===   ====== /    ===   =======================\n" );
    printf( "TIME:   ----      ----      ----       ---   ------ /    ---   -----------------------\n" );
}

static void print_cmd_times_header()
{
    printf( "TIME:   PCNT      REAL      USER       SYS   PAGEIN /    OUT   LINES  OBJSIZE   INDIVIDUAL BUILD RULES\n" );
//    printf( "TIME:   ====      ====      ====       ===   ====== /    ===   =====  =======   ======================\n" );
    printf( "TIME:   ----      ----      ----       ---   ------ /    ---   -----  -------   ----------------------\n" );
}

static void print_table_entry( double pcnt, double time,
                               double user_time, double sys_time,
                               int pageins, int pageouts )
{
    printf( "TIME: %6.2f%% %8.2fr %8.2fu %8.2fs  %6d / %6d   ",
            pcnt, time, user_time, sys_time, pageins, pageouts );
}


static void print_build_phase_times()
{
    double build_start_time, build_total_time;
    int total_pageins, total_pageouts;
    TIMING_ENTRY *entry;
    double time_elapsed;
    double pcnt_elapsed;
    double phase_start_time;
    int phase_start_pageins;
    int phase_start_pageouts;
    double phase_time;
    double phase_pcnt;
    double phase_user_time;
    double phase_sys_time;
    int phase_pageins;
    int phase_pageouts;
    char *argument = "";
    
    if ( timing_data.num_entries <= 0 )
        return;
    
    build_start_time = timing_data.build_start_time;
    build_total_time = timing_data.timing_list_tail->time_at_end - build_start_time;
    total_pageins = timing_data.timing_list_tail->pageins_at_end - timing_data.build_start_pageins;
    total_pageouts = timing_data.timing_list_tail->pageouts_at_end - timing_data.build_start_pageouts;
    
    phase_start_time = build_start_time;
    phase_start_pageins = timing_data.build_start_pageins;
    phase_start_pageouts = timing_data.build_start_pageouts;
    phase_user_time = 0;
    phase_sys_time = 0;

    for ( entry = timing_data.timing_list_head; entry != NULL; entry = entry->next ) {
        phase_user_time += entry->user_time;
        phase_sys_time += entry->sys_time;
        
        if ( ( strncmp( entry->rule_name, "BuildPhase", strlen( "BuildPhase" ) ) != 0 ) &&
             ( entry != timing_data.timing_list_tail ) )
            continue;
        
        time_elapsed = entry->time_at_end - build_start_time;
        pcnt_elapsed = time_elapsed * 100.0 / build_total_time;
        phase_time = entry->time_at_end - phase_start_time;
        phase_pcnt = phase_time * 100.0 / build_total_time;
        phase_pageins = entry->pageins_at_end - phase_start_pageins;
        phase_pageouts = entry->pageouts_at_end - phase_start_pageouts;
        
        if ( strncmp( entry->rule_name, "BuildPhase", strlen( "BuildPhase" ) ) == 0 ) {
            char phase_name[256];
            
            if ( entry->obj_file == NULL ) {
                argument = "";
            } else if ( sscanf( entry->obj_file, "<%255[^>]>", phase_name ) == 1 ) {
                argument = phase_name;
            } else {
                argument = entry->obj_file;
            }
            if ( strncmp( argument, "NoOp", strlen( "NoOp" ) ) == 0 )
                continue;
        } else if ( entry == timing_data.timing_list_tail ) {
            argument = "build wrap-up";
        }

        print_table_entry( phase_pcnt, phase_time, phase_user_time, phase_sys_time,
                           phase_pageins, phase_pageouts );
        printf( "%s\n", argument );

        // set up statistics for next phase
        phase_start_time = entry->time_at_end;
        phase_start_pageins = entry->pageins_at_end;
        phase_start_pageouts = entry->pageouts_at_end;
        phase_user_time = 0;
        phase_sys_time = 0;
    }
    
    print_table_entry( 100.0, build_total_time,
                       timing_data.total_user_time, timing_data.total_sys_time,
                       total_pageins, total_pageouts );
    printf( "TOTAL BUILD TIME\n" );
}

static void print_cmd_times()
{
    TIMING_ENTRY **timing_entry_array;
    int i;
    double build_start_time, build_end_time, build_total_time;
    TIMING_ENTRY *entry;
    double time_elapsed;
    double pcnt_elapsed;
    double cmd_time;
    double cmd_pcnt;
    int pageins;
    int pageouts;
    int num_lines;
    int obj_size;
    char *argument;
    int suppressed_count = 0;
    double suppressed_time = 0.0;
    double suppressed_pcnt = 0.0;
    
    if ( timing_data.num_entries <= 0 )
        return;
        
    timing_entry_array = sort_timing_data();
    
    build_start_time = timing_data.build_start_time;
    build_end_time = timing_data.timing_list_tail->time_at_end;
    build_total_time = build_end_time - build_start_time;
    
    // time_elapsed is the sum of all entries we've seen so far in our loop;
    // because the array is sorted by command duration rather than by command
    // end time, we can't just use the raw end time, we have to add it up.
    time_elapsed = 0.0;
    
    for (i = timing_data.num_entries - 1; i >= 0; i--) {
        entry = timing_entry_array[i];
        
        cmd_time = entry->time_at_end - entry->time_at_start;
        cmd_pcnt = cmd_time * 100.0 / build_total_time;
        time_elapsed += cmd_time;
        pcnt_elapsed = time_elapsed * 100.0 / build_total_time;
        pageins = entry->pageins_at_end - entry->pageins_at_start;
        pageouts = entry->pageouts_at_end - entry->pageouts_at_start;

        // Timing entries to suppress (other than adding their time):
        //      BuildPhase bookkeeping entries.
        //      Commands that took less than 0.1 seconds.
        //      Commands that took less than 0.1% of the build time.
        if ( strncmp( entry->rule_name, "BuildPhase", strlen( "BuildPhase" ) ) == 0 ||
             cmd_time < 0.1 || cmd_pcnt < 0.1 ) {
            suppressed_count++;
            suppressed_time += cmd_time;
            suppressed_pcnt += cmd_pcnt;
            continue;
        }

        print_table_entry( cmd_pcnt, cmd_time, entry->user_time, entry->sys_time,
                           pageins, pageouts );
        
        num_lines = 0;
        obj_size = 0;
        if ( strncmp( entry->rule_name, "Compile", strlen( "Compile" ) ) == 0 && entry->source_file ) {
            num_lines = get_num_lines( entry->source_file );
            obj_size = get_file_size( entry->obj_file );
            argument = entry->source_file;
        } else if ( entry->obj_file ) {
            char *p = strrchr( entry->obj_file, '/' );
            p = ( p != NULL ) ? ( p + 1 ) : entry->obj_file;
            argument = p;
        } else {
            argument = "";
        }
        
        if ( num_lines > 0 ) {
            printf( "%5d  ", num_lines );
        } else {
            printf( "%5s  ", "" );
        }
        if ( obj_size > 0 ) {
            printf( "%7d   ", obj_size );
        } else {
            printf( "%7s   ", "" );
        }
        printf( "%s %s\n", entry->rule_name, argument );
    }
    
    if ( suppressed_time >= 1.0 ) {
        printf ("TIME:   suppressed printing %d timing entries that took %.2f seconds (%.2f%%)\n",
                suppressed_count, suppressed_time, suppressed_pcnt );
    }
}

static int get_num_lines( char *filename )
{
    int fd;
    char buf[MAXBSIZE];
    int len;
    char *p;
    int num_lines = 0;
    
    if ( !filename )
        return 0;
    
    if ( ( fd = open( filename, O_RDONLY, 0 ) ) < 0 )
        return 0;
    
    while ( ( len = read( fd, buf, MAXBSIZE ) ) > 0 ) {
        for ( p = buf; len--; p++ ) {
            if ( *p == '\n' ) {
                num_lines++;
            }
        }
    }
    
    close( fd );
    
    return num_lines;
}

static int get_file_size( char *filename )
{
    struct stat statbuf;
    
    if ( !filename )
        return 0;
    
    if ( stat( filename, &statbuf ) < 0 )
        return 0;
    
    if ( S_ISREG( statbuf.st_mode ) ||
         S_ISLNK( statbuf.st_mode ) ||
         S_ISDIR( statbuf.st_mode ) ) {
        return statbuf.st_size;
    } else {
        return 0;
    }
}



// Create a sorted array from the linked list of timing entries.  The array is sorted by
// entry duration, from shortest to longest.
static TIMING_ENTRY ** sort_timing_data()
{
    TIMING_ENTRY **timing_entry_array;
    TIMING_ENTRY *timing_entry = timing_data.timing_list_head;
    int i;
    
    if ( timing_data.num_entries <= 0 )
        return NULL;
        
    timing_entry_array = (TIMING_ENTRY **) malloc( timing_data.num_entries * sizeof( TIMING_ENTRY *) );
    
    for ( i = 0; i < timing_data.num_entries; i++ ) {
        timing_entry_array[i] = timing_entry;
        timing_entry = timing_entry->next;
    }
    
    qsort( timing_entry_array, timing_data.num_entries, sizeof( TIMING_ENTRY * ), compare_duration );
    
    return timing_entry_array;
}


static int compare_duration( const void *first_entry_ptr, const void *second_entry_ptr )
{
    TIMING_ENTRY *first_timing_entry = *((TIMING_ENTRY **) first_entry_ptr);
    TIMING_ENTRY *second_timing_entry = *((TIMING_ENTRY **) second_entry_ptr);
    double first_timing_entry_duration = first_timing_entry->time_at_end - first_timing_entry->time_at_start;
    double second_timing_entry_duration = second_timing_entry->time_at_end - second_timing_entry->time_at_start;
    
    return ( first_timing_entry_duration > second_timing_entry_duration ) ? 1 : -1;
}

#endif
