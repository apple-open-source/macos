#ifndef __INITNG_H__
#define __INITNG_H__

#ifdef __APPLE_API_PRIVATE
/* Returns zero on success, not zero on failure. If needed, we can return more
 * explicit error codes */
int __register_per_user_mach_bootstrap_servers(void);
#endif

#endif
