//
//  BootCache_debug.h
//  BootCache.kext
//
//  Created by Brian Tearse-Doyle on 10/13/17.
//

#ifndef BootCache_debug_h
#define BootCache_debug_h

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
#endif


#ifdef BC_DEBUG
# define MACH_DEBUG
# define MACH_ASSERT 1
# define message(fmt, args...)	\
do { \
microtime(&debug_currenttime); \
timersub(&debug_currenttime, &debug_starttime, &debug_currenttime); \
lck_mtx_lock(&debug_printlock); \
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
