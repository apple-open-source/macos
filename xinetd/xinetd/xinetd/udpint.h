#ifndef UDPINT_H
#define UDPINT_H

#include "defs.h"
#include "int.h"

#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
void di_exit(void);
struct intercept_s *di_init(struct server *serp);

#endif
