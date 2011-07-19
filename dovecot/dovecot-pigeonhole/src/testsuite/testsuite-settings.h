/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __TESTSUITE_SETTINGS_H
#define __TESTSUITE_SETTINGS_H

#include "sieve-common.h"

void testsuite_settings_init(void);
void testsuite_settings_deinit(void);

void testsuite_setting_set(const char *identifier, const char *value);
void testsuite_setting_unset(const char *identifier);

#endif /* __TESTSUITE_SETTINGS_H */
