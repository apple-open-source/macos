/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_VACATION_COMMON_H
#define __EXT_VACATION_COMMON_H

#include "sieve-common.h"

/*
 * Extension configuration
 */

#define EXT_VACATION_DEFAULT_PERIOD (7*24*60*60)
#define EXT_VACATION_DEFAULT_MIN_PERIOD (24*60*60)
#define EXT_VACATION_DEFAULT_MAX_PERIOD 0
 
struct ext_vacation_config {
	unsigned int min_period;
	unsigned int max_period;
	unsigned int default_period;
	bool use_original_recipient;
	bool dont_check_recipient;
};

/* 
 * Commands 
 */

extern const struct sieve_command_def vacation_command;

/* 
 * Operations 
 */

extern const struct sieve_operation_def vacation_operation;

/* 
 * Extensions
 */

/* Vacation */

extern const struct sieve_extension_def vacation_extension;

bool ext_vacation_load
	(const struct sieve_extension *ext, void **context);
void ext_vacation_unload
	(const struct sieve_extension *ext);

/* Vacation-seconds */

extern const struct sieve_extension_def vacation_seconds_extension;

bool ext_vacation_register_seconds_tag
	(struct sieve_validator *valdtr, const struct sieve_extension *vacation_ext);

#endif /* __EXT_VACATION_COMMON_H */
