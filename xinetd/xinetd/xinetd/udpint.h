#ifndef UDPINT_H
#define UDPINT_H

#include "defs.h"
#include "int.h"

void di_exit(void);
struct intercept_s *di_init(struct server *serp);

#endif
