/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#ifndef __SIEVE_PLUGINS_H
#define __SIEVE_PLUGINS_H

#include "sieve-common.h"

void sieve_plugins_load(struct sieve_instance *svinst, const char *path, const char *plugins);
void sieve_plugins_unload(struct sieve_instance *svinst);

#endif /* __SIEVE_PLUGINS_H */
