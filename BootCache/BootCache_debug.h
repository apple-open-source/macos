//
//  BootCache_debug.h
//  BootCache.kext
//
//  Created by Brian Tearse-Doyle on 10/13/17.
//

#ifndef BootCache_debug_h
#define BootCache_debug_h

#include <IOKit/IOLib.h> // For IOMalloc

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Build options.
 */
/*#define BC_DEBUG defined automatically in project debug build */
/*#define BC_EXTRA_DEBUG*/

#ifdef BC_DEBUG
extern struct timeval debug_starttime;
extern struct timeval debug_currenttime;
extern lck_mtx_t debug_printlock;
extern char* debug_print_buffer;
extern size_t debug_print_buffer_length;
extern size_t debug_print_buffer_capacity;
extern bool debug_print_buffer_failed;
extern size_t debug_print_size;
extern char* debug_temp_buffer;
#endif

#define RANGE_FMT "%#-12llx-%#-12llx (%#-8llx)"
#define RANGE_ARGS(offset, length) ((uint64_t)offset), (uint64_t)((offset) + (length)), ((uint64_t)length)

#ifdef BC_DEBUG
# define MACH_DEBUG
# define MACH_ASSERT 1
# define DEBUG_BUFFER_GROW_AMOUNT (1024*1024)
# define message(fmt, args...)	\
do { \
lck_mtx_lock(&debug_printlock); \
microtime(&debug_currenttime); \
timersub(&debug_currenttime, &debug_starttime, &debug_currenttime); \
if (!debug_print_buffer_failed) { \
	while ((debug_print_size = snprintf(debug_print_buffer + debug_print_buffer_length, debug_print_buffer_capacity - debug_print_buffer_length, "%5d.%03d %24s[%4d]: " fmt "\n", (u_int)(debug_currenttime.tv_sec), (u_int)(debug_currenttime.tv_usec / 1000), __FUNCTION__, __LINE__, ##args)) + 1 + debug_print_buffer_length > debug_print_buffer_capacity) { \
		debug_temp_buffer = IONewZeroData(typeof(*debug_temp_buffer), debug_print_buffer_capacity + DEBUG_BUFFER_GROW_AMOUNT); \
		if (debug_temp_buffer) { \
			if (debug_print_buffer) { \
				memmove(debug_temp_buffer, debug_print_buffer, debug_print_buffer_length); \
				IODeleteData(debug_print_buffer, typeof(*debug_print_buffer), debug_print_buffer_capacity); \
			} \
			debug_print_buffer = debug_temp_buffer; \
			debug_temp_buffer = NULL; \
			debug_print_buffer_capacity += DEBUG_BUFFER_GROW_AMOUNT; \
		} else { \
			debug_print_buffer_length = MIN(debug_print_buffer_length + 32, debug_print_buffer_capacity); \
			snprintf(debug_print_buffer + debug_print_buffer_length - 32, 32, "\noverflowed buffer %#lx\n", debug_print_buffer_capacity); \
			debug_print_buffer_failed = true; \
			break; \
		} \
	} \
	if (!debug_print_buffer_failed) { \
		debug_print_buffer_length += debug_print_size; \
	} \
} \
printf("BootCache: %5d.%03d %24s[%4d]: " fmt "\n", (u_int)(debug_currenttime.tv_sec), (u_int)(debug_currenttime.tv_usec / 1000), __FUNCTION__, __LINE__, ##args); \
lck_mtx_unlock(&debug_printlock); \
} while (0)

# define debug(fmt, args...)	message("*** " fmt, ##args)
extern void Debugger(const char *);


#else // !BC_DEBUG
# define debug(fmt, args...)	do {} while (0)
# define message(fmt, args...)	printf("BootCache: " fmt "\n" , ##args)
#endif

#ifdef BC_EXTRA_DEBUG
# define xdebug(fmt, args...)	message("+++ " fmt, ##args)
#else
# define xdebug(fmt, args...)	do {} while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* BootCache_debug_h */
