/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-match-types.h"

#include "ext-regex-common.h"

/* 
 * Regex match type operand
 */

static const struct sieve_extension_objects ext_match_types =
    SIEVE_EXT_DEFINE_MATCH_TYPE(regex_match_type);

const struct sieve_operand_def regex_match_type_operand = {
    "regex match",
    &regex_extension,
    0,
    &sieve_match_type_operand_class,
    &ext_match_types
};

