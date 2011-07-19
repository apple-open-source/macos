/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "unichar.h"
#include "managesieve-parser.h"
#include "managesieve-quote.h"

/* Turn the value string into a valid MANAGESIEVE string or literal, no matter 
 * what. QUOTED-SPECIALS are escaped, but any invalid (UTF-8) character
 * is simply removed. Linebreak characters are not considered invalid, but
 * they do force the generation of a string literal.
 */
void managesieve_quote_append(string_t *str, const unsigned char *value,
		       size_t value_len, bool compress_lwsp)
{
	size_t i, extra = 0, escape = 0;
	string_t *tmp;
	bool 
		last_lwsp = TRUE, 
		literal = FALSE, 
		modify = FALSE;

 	if (value == NULL) {
		str_append(str, "\"\"");
		return;
	}

	if (value_len == (size_t)-1)
		value_len = strlen((const char *) value);

	for (i = 0; i < value_len; i++) {
		switch (value[i]) {
		case ' ':
		case '\t':
			if (last_lwsp && compress_lwsp) {
				modify = TRUE;
				extra++;
			}
			last_lwsp = TRUE;
			break;
		case '"':
		case '\\':
			escape++;
			last_lwsp = FALSE;
			break;
		case 13:
		case 10:
			literal = TRUE;
			last_lwsp = TRUE;
			break;
		default:
			last_lwsp = FALSE;
		}
	}

	if (!literal) {
		/* no linebreak chars, return as (escaped) "string" */
		str_append_c(str, '"');
	} else {
		/* return as literal */
		str_printfa(str, "{%"PRIuSIZE_T"}\r\n", value_len - extra);
	}

	tmp = t_str_new(value_len+escape+4);
	if (!modify && (literal || escape == 0))
		str_append_n(tmp, value, value_len);
	else {
		last_lwsp = TRUE;
		for (i = 0; i < value_len; i++) {
			switch (value[i]) {
			case '"':
			case '\\':
				last_lwsp = FALSE;
				if (!literal) 
					str_append_c(tmp, '\\');
				str_append_c(tmp, value[i]);
				break;
			case ' ':
			case '\t':
				if (!last_lwsp || !compress_lwsp)
					str_append_c(tmp, ' ');
				last_lwsp = TRUE;
				break;
			case 13:
			case 10:
				last_lwsp = TRUE;
				str_append_c(tmp, value[i]);
				break;
			default:
				last_lwsp = FALSE;
				str_append_c(tmp, value[i]);
				break;
			}
		}
	}

	if ( uni_utf8_get_valid_data(str_data(tmp), str_len(tmp), str) )
		str_append_str(str, tmp);

	if (!literal)
		str_append_c(str, '"');
}

char *managesieve_quote(pool_t pool, const unsigned char *value, size_t value_len)
{
	string_t *str;
	char *ret;

	if (value == NULL)
		return "\"\"";

	T_BEGIN {
		str = t_str_new(value_len + MAX_INT_STRLEN + 5);
		managesieve_quote_append(str, value, value_len, TRUE);
		ret = p_strndup(pool, str_data(str), str_len(str));
	} T_END;

	return ret;
}
