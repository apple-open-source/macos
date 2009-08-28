#if !defined(_MIG_MACHINE_H)
#define _MIG_MACHINE_H
#if defined(__i386__) || defined(__x86_64__)
#include "i386/mig_machine.h"
#elif defined(__ppc__) || defined(__ppc64__)
#include "ppc/mig_machine.h"
#else
#error Architecture not supported
#endif
#endif
