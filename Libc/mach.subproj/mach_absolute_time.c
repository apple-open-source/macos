#include <stdint.h>
#include <mach/clock.h>

extern mach_port_t clock_port;

uint64_t mach_absolute_time(void) {
#if defined(__ppc__)
	__asm__ volatile("0: mftbu r3");
	__asm__ volatile("mftb r4");
	__asm__ volatile("mftbu r0");
	__asm__ volatile("cmpw r0,r3");
	__asm__ volatile("bne- 0b");
#else
	mach_timespec_t now;
	(void)clock_get_time(clock_port, &now);
	return (uint64_t)now.tv_sec * NSEC_PER_SEC + now.tv_nsec;
#endif
}

