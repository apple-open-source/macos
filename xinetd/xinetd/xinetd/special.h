#ifndef SPECIAL_H
#define SPECIAL_H

#include "defs.h"
#include "builtins.h"

const builtin_s *spec_find(const char *service_name,int type);
status_e spec_service_handler( struct service *sp, connection_s *cp );
void spec_include(void);
	
#endif

