/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension encoded-character
 * ---------------------------
 *
 * Authors: Stephan Bosch
 * Specification: RFC 5228
 * Implementation: full
 * Status: testing
 *
 */

#include "lib.h"
#include "unichar.h"

#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-validator.h"

#include <ctype.h>

/* 
 * Extension
 */

static bool ext_encoded_character_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *valdtr);
	
struct sieve_extension_def encoded_character_extension = { 
	"encoded-character", 
	NULL, NULL,
	ext_encoded_character_validator_load, 
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_NO_OPERATIONS, 
	SIEVE_EXT_DEFINE_NO_OPERANDS
};

/*
 * Encoded string argument
 */

bool arg_encoded_string_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *context);

const struct sieve_argument_def encoded_string_argument = { 
	"@encoded-string", 
	NULL, 
	arg_encoded_string_validate, 
	NULL, NULL, NULL
};

/* Parsing */

static bool _skip_whitespace
	(const char **in, const char *inend)
{
	while ( *in < inend ) {
		if ( **in == '\r' ) {
			(*in)++;
			if ( **in != '\n' )
				return FALSE;
			continue;
		}
		
		/* (Loose LF is non-standard) */
		if ( **in != ' ' && **in != '\n' && **in != '\t' ) 
			break;
			
		(*in)++;
	}
	
	return TRUE;
}

static bool _parse_hexint
(const char **in, const char *inend, int max_digits, unsigned int *result)
{
	int digit = 0;
	*result = 0;
		
	while ( *in < inend && (max_digits == 0 || digit < max_digits) ) {
	
		if ( (**in) >= '0' && (**in) <= '9' ) 
			*result = ((*result) << 4) + (**in) - ((unsigned int) '0');
		else if ( (**in) >= 'a' && (**in) <= 'f' )
			*result = ((*result) << 4) + (**in) - ((unsigned int) 'a') + 0x0a;
		else if ( (**in) >= 'A' && (**in) <= 'F' )
			*result = ((*result) << 4) + (**in) - ((unsigned int) 'A') + 0x0a;
		else
			return ( digit > 0 );
	
		(*in)++;
		digit++;
	}
	
	if ( digit == max_digits ) {
		/* Hex digit _MUST_ end here */
		if ( (**in >= '0' && **in <= '9')	|| (**in >= 'a' && **in <= 'f') ||
			(**in >= 'A' && **in <= 'F') )
			return FALSE;
			
		return TRUE;
	}
	
	return ( digit > 0 );
}

static bool _decode_hex
(const char **in, const char *inend, string_t *result) 
{
	int values = 0;
	
	while ( *in < inend ) {
		unsigned int hexpair;
		
		if ( !_skip_whitespace(in, inend) ) return FALSE;
		
		if ( !_parse_hexint(in, inend, 2, &hexpair) ) break;
		
		str_append_c(result, (unsigned char) hexpair);
		values++;
	}
	
	return ( values > 0 );
}

static int _decode_unicode
(const char **in, const char *inend, string_t *result, unsigned int *error_hex) 
{
	int values = 0;
	bool valid = TRUE;
	
	while ( *in < inend ) {
		unsigned int unicode_hex;
		
		if ( !_skip_whitespace(in, inend) ) return FALSE;
		
		if ( !_parse_hexint(in, inend, 0, &unicode_hex) ) break;

		if ( (unicode_hex <= 0xD7FF) || 
			(unicode_hex >= 0xE000 && unicode_hex <= 0x10FFFF)	) 
			uni_ucs4_to_utf8_c((unichar_t) unicode_hex, result);
		else {
			if ( valid ) *error_hex = unicode_hex;
			valid = FALSE;
		}	
		values++;
	}
	
	return ( values > 0 );
}

bool arg_encoded_string_validate
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd)
{
	bool result = TRUE;
	enum { ST_NONE, ST_OPEN, ST_TYPE, ST_CLOSE } 
		state = ST_NONE;
	string_t *str = sieve_ast_argument_str(*arg);
	string_t *tmpstr, *newstr = NULL;
	const char *p, *mark, *strstart, *substart = NULL;
	const char *strval = (const char *) str_data(str);
	const char *strend = strval + str_len(str);
	unsigned int error_hex = 0;

	T_BEGIN {		
		tmpstr = t_str_new(32);	
			
		p = strval;
		strstart = p;
		while ( result && p < strend ) {
			switch ( state ) {
			/* Normal string */
			case ST_NONE:
				if ( *p == '$' ) {
					substart = p;
					state = ST_OPEN;
				}
				p++;
				break;
			/* Parsed '$' */
			case ST_OPEN:
				if ( *p == '{' ) {
					state = ST_TYPE;
					p++;
				} else 
					state = ST_NONE;
				break;
			/* Parsed '${' */
			case ST_TYPE:
				mark = p;
				/* Scan for 'hex' or 'unicode' */
				while ( p < strend && i_isalpha(*p) ) p++;
					
				if ( *p != ':' ) {
					state = ST_NONE;
					break;
				}
				
				state = ST_CLOSE;
				
				str_truncate(tmpstr, 0);
				if ( strncasecmp(mark, "hex", p - mark) == 0 ) {
					/* Hexadecimal */
					p++;
					if ( !_decode_hex(&p, strend, tmpstr) )
						state = ST_NONE;
				} else if ( strncasecmp(mark, "unicode", p - mark) == 0 ) {
					/* Unicode */
					p++;
					if ( !_decode_unicode(&p, strend, tmpstr, &error_hex) )
						state = ST_NONE;
				} else {	
					/* Invalid encoding */
					p++;
					state = ST_NONE;
				}
				break;
			case ST_CLOSE:
				if ( *p == '}' ) {				
					/* We now know that the substitution is valid */	

					if ( error_hex != 0 ) {
						sieve_argument_validate_error(valdtr, *arg, 
							"invalid unicode character 0x%08x in encoded character substitution",
							error_hex);
						result = FALSE;
						break;
					}
					
					if ( newstr == NULL ) {
						newstr = str_new(sieve_ast_pool((*arg)->ast), str_len(str)*2);
					}
					
					str_append_n(newstr, strstart, substart-strstart);
					str_append_str(newstr, tmpstr);
					
					strstart = p + 1;
					substart = strstart;
					
					p++;	
				} 
				state = ST_NONE;
			}
		}
	} T_END;

	if ( !result ) return FALSE;
	
	if ( newstr != NULL ) {
		if ( strstart != strend )
			str_append_n(newstr, strstart, strend-strstart);	
	
		sieve_ast_argument_string_set(*arg, newstr);
	}
	
	/* Pass the processed string to a (possible) next layer of processing */
	return sieve_validator_argument_activate_super
		(valdtr, cmd, *arg, TRUE);
}

/* 
 * Extension implementation
 */

static bool ext_encoded_character_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	/* Override the constant string argument with our own */
	sieve_validator_argument_override
		(valdtr, SAT_CONST_STRING, ext, &encoded_string_argument); 
	
	return TRUE;
}
