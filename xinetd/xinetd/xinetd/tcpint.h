#ifndef TCPINT_H
#define TCPINT_H

#include "defs.h"
#include "int.h"

#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
void si_exit(void);
struct intercept_s *si_init(struct server *serp);

#endif
