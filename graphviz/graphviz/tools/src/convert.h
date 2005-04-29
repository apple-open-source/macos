#ifndef CONVERT_H
#define CONVERT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <agraph.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>

extern void dot_to_gxl(Agraph_t*, FILE*);
#ifdef HAVE_LIBEXPAT
extern Agraph_t* gxl_to_dot(FILE*);
#endif

#endif
