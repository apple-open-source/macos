#ifndef @FILEGUARD@
#define @FILEGUARD@

#include <mach/mach.h>

#if defined (__MACH30__) && defined (__ppc__)
#include "machine/powerpc-apple-darwin--powerpc-apple-darwin/@file@"
#elif defined (__MACH30__) && defined (__i386__)
#include "machine/i386-apple-darwin--i386-apple-darwin/@file@"
#elif defined (__MACH30__) && defined (__x86_64__)
#include "machine/x86_64-apple-darwin--x86_64-apple-darwin/@file@"
#elif defined (__MACH30__) && defined (__arm__)
#include "machine/arm-apple-darwin--arm-apple-darwin/@file@"
#else
#error unknown architecture
#endif

#endif /* @FILEGUARD@ */
