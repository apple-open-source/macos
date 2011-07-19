/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"

#include "sieve-common.h"
#include "sieve-code.h"

#include "sieve-ext-variables.h"

#include "ext-enotify-common.h"

/*
 * Encodeurl modifier
 */
 
bool mod_encodeurl_modify(string_t *in, string_t **result);
 
const struct sieve_variables_modifier_def encodeurl_modifier = {
	SIEVE_OBJECT("encodeurl", &encodeurl_operand, 0),
	15,
	mod_encodeurl_modify
};
 
/*
 * Modifier operand
 */

static const struct sieve_extension_objects ext_enotify_modifiers =
	SIEVE_VARIABLES_DEFINE_MODIFIER(encodeurl_modifier);

const struct sieve_operand_def encodeurl_operand = { 
	"modifier", 
	&enotify_extension,
	0, 
	&sieve_variables_modifier_operand_class,
	&ext_enotify_modifiers
};

/*
 * Modifier implementation
 */

static const char _uri_reserved_lookup[256] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 00
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 10
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1,  // 20
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,  // 30
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 40
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,  // 50
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 60
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1,  // 70

	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 80
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 90
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // A0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // B0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // C0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // D0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // E0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // F0
};

bool mod_encodeurl_modify(string_t *in, string_t **result)
{	
	unsigned int i;
	const unsigned char *c;

	*result = t_str_new(2*str_len(in));
	c = str_data(in);
	
	for ( i = 0; i < str_len(in); i++, c++ ) {
		if ( _uri_reserved_lookup[*c] ) {
			str_printfa(*result, "%%%02X", *c);
		} else {
			str_append_c(*result, *c); 
		}	
	}

	return TRUE;
}
 

