#include <mach/mach.h>
#include <mach-o/dyld_debug.h>
#import "stuff/openstep_mach.h"
#ifndef __MACH30__
#define task_port_t task_t
#define thread_port_t thread_t
#define thread_port_array_t thread_array_t
#define mach_port_name_t port_name_t
#define mach_port_type_array_t port_type_array_t
#endif /* !defined(__MACH30__) */

/*
 * This is the layout of the (__DATA,__dyld) section.  The start_debug_thread,
 * debug_port and debug_thread fields are used in here.  This is defined in
 * crt1.o in assembly code and this structure is declared to match.
 */
struct dyld_data_section {
    void     *stub_binding_helper_interface;
    void     *_dyld_func_lookup;
    void     *start_debug_thread;
    mach_port_t   debug_port;
    thread_port_t debug_thread;
#ifdef CORE_DEBUG
    /*
     * This first field got dropped, so its not in MacOS X DP4.  So to not
     * break things we can't add it here as this code wants to read
     * sizeof(struct dyld_data_section) from the (__DATA,__dyld) section.
     */
    void      *dyld_stub_binding_helper;
    unsigned long core_debug;
#endif
};

void set_dyld_debug_error_data(
    enum dyld_debug_return dyld_debug_return,
    kern_return_t mach_error,
    int dyld_debug_errno,
    unsigned long local_error,
    char *file_name,
    unsigned long line_number);

#define CORE_ERROR 100

#define SET_MACH_DYLD_DEBUG_ERROR(k, l) \
	set_dyld_debug_error_data(DYLD_FAILURE,(k),0,(l),__FILE__,__LINE__)
#define SET_ERRNO_DYLD_DEBUG_ERROR(e, l) \
	set_dyld_debug_error_data(DYLD_FAILURE,0,(e),(l),__FILE__,__LINE__)
#define SET_LOCAL_DYLD_DEBUG_ERROR(l) \
	set_dyld_debug_error_data(DYLD_FAILURE,0,0,(l),__FILE__,__LINE__)

enum dyld_debug_return get_dyld_data(
    task_port_t target_task,
    struct dyld_data_section *data);

enum dyld_debug_return set_dyld_data(
    task_port_t target_task,
    struct dyld_data_section *dyld_data);

/*
 * These are dummy values written into the dyld data's debug_port and
 * debug_thread so that the dyld_debug routines will know that the task it is
 * operating on is a core file and to force a new debug tread to be started.
 */
#define CORE_DEBUG_PORT   0xf355a360
#define CORE_DEBUG_THREAD 0xf40f50f1
