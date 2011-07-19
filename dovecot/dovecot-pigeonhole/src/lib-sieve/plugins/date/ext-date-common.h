/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
#ifndef __EXT_DATE_COMMON_H
#define __EXT_DATE_COMMON_H

#include "sieve-common.h"

#include <time.h>

/*
 * Extension
 */
 
extern const struct sieve_extension_def date_extension;

bool ext_date_interpreter_load
	(const struct sieve_extension *ext, const struct sieve_runtime_env *renv,
		sieve_size_t *address ATTR_UNUSED);

/* 
 * Tests
 */

extern const struct sieve_command_def date_test;
extern const struct sieve_command_def currentdate_test;
 
/*
 * Operations
 */

enum ext_date_opcode {
	EXT_DATE_OPERATION_DATE,
	EXT_DATE_OPERATION_CURRENTDATE
};

extern const struct sieve_operation_def date_operation;
extern const struct sieve_operation_def currentdate_operation;

/*
 * Zone string
 */

bool ext_date_parse_timezone(const char *zone, int *zone_offset_r);

/*
 * Current date
 */

time_t ext_date_get_current_date
	(const struct sieve_runtime_env *renv, int *zone_offset_r);

/*
 * Date part
 */

struct ext_date_part {
	const char *identifier;

	const char *(*get_string)(struct tm *tm, int zone_offset);
};

const char *ext_date_part_extract
	(const char *part, struct tm *tm, int zone_offset);

/*
 * Date stringlist
 */

enum ext_date_timezone_special {
	EXT_DATE_TIMEZONE_LOCAL    = 100,
	EXT_DATE_TIMEZONE_ORIGINAL = 101
};

struct sieve_stringlist *ext_date_stringlist_create
(const struct sieve_runtime_env *renv, struct sieve_stringlist *field_values,
	int time_zone, const char *date_part);



#endif /* __EXT_DATE_COMMON_H */
