/*
 * (c) Copyright 2001-2002 by Steve Grubb
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */

#ifndef SENSOR_H
#define SENSOR_H

#include "defs.h"
#include "service.h"

void init_sensor( void );
void process_sensor( const struct service *, const union xsockaddr *);
status_e check_sensor( const union xsockaddr * );
void destroy_global_access_list( void );

#endif
