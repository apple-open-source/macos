#ifndef @FILEGUARD@
#define @FILEGUARD@

#include <mach/mach.h>

#if defined (__MACH30__) && defined (__ppc__)
#include "powerpc-apple-macos10--powerpc-apple-macos10/@file@"
#elif defined (__MACH30__) && defined (__i386__)
#include "i386-apple-macos10--i386-apple-macos10/@file@"
#elif ! defined (__MACH30__) && defined (__ppc__)
#include "powerpc-apple-rhapsody--powerpc-apple-rhapsody/@file@"
#elif defined (__MACH30__) && defined (__i386__)
#include "i386-apple-rhapsody--i386-apple-rhapsody/@file@"
#else
#error unknown architecture
#endif

#endif /* @FILEGUARD@ */
