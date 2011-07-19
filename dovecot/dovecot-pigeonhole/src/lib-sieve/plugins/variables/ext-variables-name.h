/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_VARIABLES_NAME
#define __EXT_VARIABLES_NAME

/* Variable Substitution
 * ---------------------
 * 
 * The variable strings are preprocessed into an AST list consisting of variable 
 * substitutions and constant parts of the string. The variables to which
 * the substitutions link are looked up and their index in their scope storage
 * is what is added to the list and eventually emitted as byte code. So, in 
 * bytecode a variable string will look as a series of substrings interrupted by
 * integer operands that refer to variables. During execution, the strings and 
 * the looked-up variables are concatenated to obtain the desired result. The 
 * the variable references are simple indexes into an array of variables, so
 * looking these up during execution is a trivial process.
 * 
 * However (RFC 5229):
 *   Tests or actions in future extensions may need to access the
 *   unexpanded version of the string argument and, e.g., do the expansion
 *   after setting variables in its namespace.  The design of the
 *   implementation should allow this.
 *
 * Various options exist to provide this feature. If the extension is entirely
 * namespace-based there is actually not very much of a problem. The variable
 * list can easily be extended with new argument-types that refer to a variable
 * identifier in stead of an index in the variable's storage. 
 */

#include "lib.h"
#include "array.h"

#include "sieve-common.h"

#include "ext-variables-common.h"

/*
 * Variable name parsing
 */
 
int ext_variable_name_parse
	(ARRAY_TYPE(sieve_variable_name) *vname, const char **str, const char *strend);

#endif /* __EXT_VARIABLES_NAME */
