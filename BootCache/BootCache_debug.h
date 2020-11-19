//
//  BootCache_debug.h
//  BootCache.kext
//
//  Created by Brian Tearse-Doyle on 10/13/17.
//

#ifndef BootCache_debug_h
#define BootCache_debug_h

#include <kern/kalloc.h>

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
#endif


#ifdef BC_DEBUG
# define MACH_DEBUG
# define MACH_ASSERT 1
# define DEBUG_BUFFER_GROW_AMOUNT (1024*1024)
# define message(fmt, args...)	\
do { \
microtime(&debug_currenttime); \
timersub(&debug_currenttime, &debug_starttime, &debug_currenttime); \
lck_mtx_lock(&debug_printlock); \
if (!debug_print_buffer_failed) { \
	size_t print_size; \
	while ((print_size = snprintf(debug_print_buffer + debug_print_buffer_length, debug_print_buffer_capacity - debug_print_buffer_length, "%5d.%03d %24s[%4d]: " fmt "\n", (u_int)(debug_currenttime.tv_sec), (u_int)(debug_currenttime.tv_usec / 1000), __FUNCTION__, __LINE__, ##args)) + 1 + debug_print_buffer_length > debug_print_buffer_capacity) { \
		char* newbuf = (char*)kalloc(debug_print_buffer_capacity + DEBUG_BUFFER_GROW_AMOUNT); \
		if (newbuf) { \
			if (debug_print_buffer) { \
				memmove(newbuf, debug_print_buffer, debug_print_buffer_length); \
				kfree(debug_print_buffer, debug_print_buffer_capacity); \
			} \
			debug_print_buffer = newbuf; \
			debug_print_buffer_capacity += DEBUG_BUFFER_GROW_AMOUNT; \
		} else { \
			debug_print_buffer_failed = true; \
			break; \
		} \
	} \
	if (!debug_print_buffer_failed) { \
		debug_print_buffer_length += print_size; \
	} \
} \
printf("BootCache: %5d.%03d %24s[%4d]: " fmt "\n", (u_int)(debug_currenttime.tv_sec), (u_int)(debug_currenttime.tv_usec / 1000), __FUNCTION__, __LINE__, ##args); \
lck_mtx_unlock(&debug_printlock); \
} while (0)

# define debug(fmt, args...)	message("*** " fmt, ##args)
extern void Debugger(const char *);
#else
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
