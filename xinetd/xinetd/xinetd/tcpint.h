#ifndef TCPINT_H
#define TCPINT_H

#include "defs.h"
#include "int.h"

void si_exit(void);
struct intercept_s *si_init(struct server *serp);

#endif
