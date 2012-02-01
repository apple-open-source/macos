/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "array.h"

#include "sieve-common.h"

#include "ext-variables-common.h"
#include "ext-variables-limits.h"
#include "ext-variables-name.h"

#include <ctype.h>

bool sieve_variable_identifier_is_valid(const char *identifier)
{
	const char *p = identifier;
	size_t plen = strlen(identifier);
	const char *pend;

	if ( plen == 0 || plen >= EXT_VARIABLES_MAX_VARIABLE_NAME_LEN )
		return FALSE;

	pend = PTR_OFFSET(identifier, plen);

	if ( *p == '_' || i_isalpha(*p) ) {
		p++;

		while ( p < pend && (*p == '_' || i_isalnum(*p)) ) {
			p++;
		}
	}

	return ( p == pend );
}

int ext_variable_name_parse
(ARRAY_TYPE(sieve_variable_name) *vname, const char **str, const char *strend)
{
	const char *p = *str;

	array_clear(vname);

	while ( p < strend ) {
		struct sieve_variable_name *cur_element;
		string_t *cur_ident;

		/* Acquire current position in the array */

		if ( array_count(vname) >= EXT_VARIABLES_MAX_NAMESPACE_ELEMENTS )
			return -1;

		cur_element = array_append_space(vname);
		cur_ident = cur_element->identifier = t_str_new(32);

		/* Parse element */

		/* Identifier */
		if ( *p == '_' || i_isalpha(*p) ) {
			cur_element->num_variable = -1;
			str_truncate(cur_ident, 0);
			str_append_c(cur_ident, *p);
			p++;

			while ( p < strend && (*p == '_' || i_isalnum(*p)) ) {
				if ( str_len(cur_ident) >= EXT_VARIABLES_MAX_VARIABLE_NAME_LEN )
					return -1;
				str_append_c(cur_ident, *p);
				p++;
			}

		/* Num-variable */
		} else if ( i_isdigit(*p) ) {
			cur_element->num_variable = *p - '0';
			p++;

			while ( p < strend && i_isdigit(*p) ) {
				cur_element->num_variable = cur_element->num_variable*10 + (*p - '0');
				p++;
			}

			/* If a num-variable is first, no more elements can follow because no
			 * namespace is specified.
			 */
			if ( array_count(vname) == 1 ) {
				*str = p;
				return 1;
			}
		} else {
			*str = p;
			return -1;
		}

		/* Check whether next name element is present */
		if ( p < strend && *p == '.' ) {
			p++;

			/* It may not be empty */
			if ( p >= strend )
				return -1;
		} else
			break;
	}

	*str = p;
	return array_count(vname);
}


