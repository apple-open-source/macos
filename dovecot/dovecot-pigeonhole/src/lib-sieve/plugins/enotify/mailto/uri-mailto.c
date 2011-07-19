/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */
  
/* FIXME: URI syntax conforms to something somewhere in between RFC 2368 and
 *   draft-duerst-mailto-bis-05.txt. Should fully migrate to new specification
 *   when it matures. This requires modifications to the address parser (no
 *   whitespace allowed within the address itself) and UTF-8 support will be
 *   required in the URL.
 */
 
#include "lib.h"
#include "array.h"
#include "str.h"
#include "str-sanitize.h"

#include "rfc2822.h"

#include "sieve-common.h"
#include "sieve-error.h"
#include "sieve-address.h"
#include "sieve-message.h"

#include "uri-mailto.h"

/* Util macros */

#define uri_mailto_error(PARSER, ...) \
	sieve_error((PARSER)->ehandler, NULL, "invalid mailto URI: " __VA_ARGS__ )
	
#define uri_mailto_warning(PARSER, ...) \
	sieve_warning((PARSER)->ehandler, NULL, "mailto URI: " __VA_ARGS__ )

/* Parser object */

struct uri_mailto_parser {
	pool_t pool;
	struct sieve_error_handler *ehandler;

	struct uri_mailto *uri;

	const char **reserved_headers;
	const char **unique_headers;

	int max_recipients;
	int max_headers;
};

/* 
 * Reserved and unique headers 
 */
 
static inline bool uri_mailto_header_is_reserved
(struct uri_mailto_parser *parser, const char *field_name)
{
	const char **hdr = parser->reserved_headers;

	if ( hdr == NULL ) return FALSE;

	/* Check whether it is reserved */
	while ( *hdr != NULL ) {
		if ( strcasecmp(field_name, *hdr) == 0 )
			return TRUE;
		hdr++;
	}

	return FALSE;
}

static inline bool uri_mailto_header_is_unique
(struct uri_mailto_parser *parser, const char *field_name)
{
	const char **hdr = parser->unique_headers;

	if ( hdr == NULL ) return FALSE;

	/* Check whether it is supposed to be unique */
	while ( *hdr != NULL ) {
		if ( strcasecmp(field_name, *hdr) == 0 )
			return TRUE;
		hdr++;
	}

	return FALSE;
} 

/* 
 * Low-level URI parsing.
 *
 * FIXME: much of this implementation will be common to other URI schemes. This
 *        should be merged into a common implementation.
 */

static const char _qchar_lookup[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 00
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 10
	0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0,  // 20
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,  // 30
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 40
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,  // 50
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 60
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0,  // 70

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 80
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 90
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // A0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // B0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // C0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // D0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // E0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // F0
};

static inline bool _is_qchar(unsigned char c)
{
	return _qchar_lookup[c];
}
  
static inline int _decode_hex_digit(unsigned char digit)
{
	switch ( digit ) {
	case '0': case '1': case '2': case '3': case '4': 
	case '5': case '6': case '7': case '8': case '9': 
		return (int) digit - '0';

	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		return (int) digit - 'a' + 0x0a;
		
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		return (int) digit - 'A' + 0x0A;
	}
	
	return -1;
}

static bool _parse_hex_value(const char **in, char *out)
{
	int value, digit;
		
	if ( (digit=_decode_hex_digit((unsigned char) **in)) < 0 )
		return FALSE;
	
	value = digit << 4;
	(*in)++;
	
	if ( (digit=_decode_hex_digit((unsigned char) **in)) < 0 )
		return FALSE;	

	value |= digit;
	(*in)++;

	if ( value == 0 )
		return FALSE;

	*out = (char) value;
	return TRUE;	
}

/* 
 * URI recipient parsing 
 */ 

static bool uri_mailto_add_valid_recipient
(struct uri_mailto_parser *parser, string_t *recipient, bool cc)
{
	struct uri_mailto *uri = parser->uri;
	struct uri_mailto_recipient *new_recipient;
	struct uri_mailto_recipient *rcpts;
	unsigned int count, i;
	const char *error;
	const char *normalized;
	 
	/* Verify recipient */
	if ( (normalized=sieve_address_normalize(recipient, &error)) == NULL ) {
		uri_mailto_error(parser, "invalid recipient '%s': %s",
			str_sanitize(str_c(recipient), 80), error);
		return FALSE;
	}
	
	/* Add recipient to the uri */
	if ( uri != NULL ) { 				
		/* Get current recipients */
		rcpts = array_get_modifiable(&uri->recipients, &count);
		
		/* Enforce limits */
		if ( parser->max_recipients > 0 && (int)count >= parser->max_recipients ) {
			if ( (int)count == parser->max_recipients) {
				uri_mailto_warning(parser, 
					"more than the maximum %u recipients specified; "
					"rest is discarded", parser->max_recipients);
			}
			return TRUE;	
		}
	
		/* Check for duplicate first */
		for ( i = 0; i < count; i++ ) {
			if ( sieve_address_compare(rcpts[i].normalized, normalized, TRUE) == 0 ) 
				{
				/* Upgrade existing Cc: recipient to a To: recipient if possible */
				rcpts[i].carbon_copy = ( rcpts[i].carbon_copy && cc );
			
				uri_mailto_warning(parser, "ignored duplicate recipient '%s'",
					str_sanitize(str_c(recipient), 80));
				return TRUE;
			} 
		}			

		/* Add */
		new_recipient = array_append_space(&uri->recipients);
		new_recipient->carbon_copy = cc;
		new_recipient->full = p_strdup(parser->pool, str_c(recipient));
		new_recipient->normalized = p_strdup(parser->pool, normalized);
	}

	return TRUE;
}

static bool uri_mailto_parse_recipients
(struct uri_mailto_parser *parser, const char **uri_p)
{
	string_t *to = t_str_new(128);
	const char *p = *uri_p;
	
	if ( *p == '\0' || *p == '?' )
		return TRUE;
		
	while ( *p != '\0' && *p != '?' ) {
		if ( *p == '%' ) {
			/* % encoded character */
			char ch;
			
			p++;
			
			/* Parse 2-digit hex value */
			if ( !_parse_hex_value(&p, &ch) ) {
				uri_mailto_error(parser, "invalid %% encoding");
				return FALSE;
			}

			/* Check for delimiter */
			if ( ch == ',' ) {
				/* Verify and add recipient */
				if ( !uri_mailto_add_valid_recipient(parser, to, FALSE) )
					return FALSE;
			
				/* Reset for next recipient */
				str_truncate(to, 0);
			}	else {
				/* Content character */
				str_append_c(to, ch);
			}
		} else {
			if ( *p == ':' || *p == ';' || *p == ',' || !_is_qchar(*p) ) {
				uri_mailto_error
					(parser, "invalid character '%c' in 'to' part", *p);
				return FALSE;
			}

			/* Content character */
			str_append_c(to, *p);
			p++;
		}
	}	
	
	/* Skip '?' */
	if ( *p != '\0' ) p++;
	
	/* Verify and add recipient */
	if ( !uri_mailto_add_valid_recipient(parser, to, FALSE) )
		return FALSE;

	*uri_p = p;
	return TRUE;
}

static bool uri_mailto_parse_header_recipients
(struct uri_mailto_parser *parser, string_t *rcpt_header, bool cc)
{
	string_t *to = t_str_new(128);
	const char *p = (const char *) str_data(rcpt_header);
	const char *pend = p + str_len(rcpt_header);
		
	while ( p < pend ) {
		if ( *p == ',' ) {
			/* Verify and add recipient */
			if ( !uri_mailto_add_valid_recipient(parser, to, cc) )
				return FALSE;
			
			/* Reset for next recipient */
			str_truncate(to, 0);
		} else {
			/* Content character */
			str_append_c(to, *p);
		}
		p++;
	}	
	
	/* Verify and add recipient */
	if ( !uri_mailto_add_valid_recipient(parser, to, cc) )
		return FALSE;

	return TRUE;	
}

/* URI header parsing */

static bool uri_mailto_header_is_duplicate
(struct uri_mailto_parser *parser, const char *field_name)
{	
	struct uri_mailto *uri = parser->uri;

	if ( uri == NULL ) return FALSE;

	if ( uri_mailto_header_is_unique(parser, field_name) ) {
		const struct uri_mailto_header_field *hdrs;
		unsigned int count, i;

		hdrs = array_get(&uri->headers, &count);	
		for ( i = 0; i < count; i++ ) {
			if ( strcasecmp(hdrs[i].name, field_name) == 0 ) 
				return TRUE;
		}
	}
	
	return FALSE;
}

static bool uri_mailto_parse_headers
(struct uri_mailto_parser *parser, const char **uri_p)
{
	struct uri_mailto *uri = parser->uri;
	unsigned int header_count = 0;
	string_t *field = t_str_new(128);
	const char *p = *uri_p;
					
	while ( *p != '\0' ) {
		enum {
			_HNAME_IGNORED, 
			_HNAME_GENERIC,
			_HNAME_TO,
			_HNAME_CC,
			_HNAME_SUBJECT, 
			_HNAME_BODY 
		} hname_type = _HNAME_GENERIC;
		struct uri_mailto_header_field *hdrf = NULL;
		const char *field_name;
		
		/* Parse field name */
		while ( *p != '\0' && *p != '=' ) {
			char ch = *p;
			p++;
			
			if ( ch == '%' ) {
				/* Encoded, parse 2-digit hex value */
				if ( !_parse_hex_value(&p, &ch) ) {
					uri_mailto_error(parser, "invalid %% encoding");
					return FALSE;
				}
			} else if ( ch != '=' && !_is_qchar(ch) ) {
				uri_mailto_error
					(parser, "invalid character '%c' in header field name part", ch);
				return FALSE;
			}

			str_append_c(field, ch);
		}
		if ( *p != '\0' ) p++;

		/* Verify field name */
		if ( !rfc2822_header_field_name_verify(str_c(field), str_len(field)) ) {
			uri_mailto_error(parser, "invalid header field name");
			return FALSE;
		}

		if ( parser->max_headers > -1 && 
			(int)header_count >= parser->max_headers ) {
			/* Refuse to accept more headers than allowed by policy */
			if ( (int)header_count == parser->max_headers ) {
				uri_mailto_warning(parser, "more than the maximum %u headers specified; "
					"rest is discarded", parser->max_headers);
			}
			
			hname_type = _HNAME_IGNORED;
		} else {
			/* Add new header field to array and assign its name */
			
			field_name = str_c(field);
			if ( strcasecmp(field_name, "to") == 0 )
				hname_type = _HNAME_TO;
			else if ( strcasecmp(field_name, "cc") == 0 )
				hname_type = _HNAME_CC;
			else if ( strcasecmp(field_name, "subject") == 0 )
				hname_type = _HNAME_SUBJECT;
			else if ( strcasecmp(field_name, "body") == 0 )
				hname_type = _HNAME_BODY;
			else if ( !uri_mailto_header_is_reserved(parser, field_name) ) {
				if ( uri != NULL ) {
					if ( !uri_mailto_header_is_duplicate(parser, field_name) ) {
						hdrf = array_append_space(&uri->headers);
						hdrf->name = p_strdup(parser->pool, field_name);
					} else {
						uri_mailto_warning(parser, 
							"ignored duplicate for unique header field '%s'",
							str_sanitize(field_name, 32));
						hname_type = _HNAME_IGNORED;
					}
				} else {
					hname_type = _HNAME_IGNORED;
				}
			} else {
				uri_mailto_warning(parser, "ignored reserved header field '%s'",
					str_sanitize(field_name, 32));
				hname_type = _HNAME_IGNORED;
			}
		}
		
		header_count++;
			
		/* Reset for field body */
		str_truncate(field, 0);
		
		/* Parse field body */		
		while ( *p != '\0' && *p != '&' ) {
			char ch = *p;
			p++;
			
			if ( ch == '%' ) {
				/* Encoded, parse 2-digit hex value */
				if ( !_parse_hex_value(&p, &ch) ) {
					uri_mailto_error(parser, "invalid %% encoding");
					return FALSE;
				}
			} else if ( ch != '=' && !_is_qchar(ch) ) {
				uri_mailto_error
					(parser, "invalid character '%c' in header field value part", ch);
				return FALSE;
			}
			str_append_c(field, ch);
		}
		if ( *p != '\0' ) p++;
		
		/* Verify field body */
		if ( hname_type == _HNAME_BODY ) {
			// FIXME: verify body ... 
		} else {
			if ( !rfc2822_header_field_body_verify(str_c(field), str_len(field)) ) {
				uri_mailto_error(parser, "invalid header field body");
				return FALSE;
			}
		}
		
		/* Assign field body */

		switch ( hname_type ) {
		case _HNAME_IGNORED:
			break;
		case _HNAME_TO:
			/* Gracefully allow duplicate To fields */
			if ( !uri_mailto_parse_header_recipients(parser, field, FALSE) )
				return FALSE;
			break;
		case _HNAME_CC:
			/* Gracefully allow duplicate Cc fields */
			if ( !uri_mailto_parse_header_recipients(parser, field, TRUE) )
				return FALSE;
			break;
		case _HNAME_SUBJECT:
			/* Igore duplicate subject field */
			if ( uri != NULL ) {
				if ( uri->subject == NULL )
					uri->subject = p_strdup(parser->pool, str_c(field));
				else
					uri_mailto_warning(parser, "ignored duplicate subject field");
			}
			break;
		case _HNAME_BODY:
			/* Igore duplicate body field */
			if ( uri != NULL ) {
				if ( uri->body == NULL )
					uri->body = p_strdup(parser->pool, str_c(field));
				else 
					uri_mailto_warning(parser, "ignored duplicate body field");
			}
			break;
		case _HNAME_GENERIC:
			if ( uri != NULL && hdrf != NULL ) 
				hdrf->body = p_strdup(parser->pool, str_c(field));
			break;
		}
			
		/* Reset for next name */
		str_truncate(field, 0);
	}	
	
	/* Skip '&' */
	if ( *p != '\0' ) p++;

	*uri_p = p;
	return TRUE;
}

static bool uri_mailto_parse_uri
(struct uri_mailto_parser *parser, const char *uri_body)
{
	const char *p = uri_body;
	
	/* 
	 * mailtoURI   = "mailto:" [ to ] [ hfields ]
	 * to          = [ addr-spec *("%2C" addr-spec ) ]
	 * hfields     = "?" hfield *( "&" hfield )
	 * hfield      = hfname "=" hfvalue
	 * hfname      = *qchar
	 * hfvalue     = *qchar
	 * addr-spec   = local-part "@" domain
	 * local-part  = dot-atom / quoted-string
	 * qchar       = unreserved / pct-encoded / some-delims
	 * some-delims = "!" / "$" / "'" / "(" / ")" / "*"
	 *               / "+" / "," / ";" / ":" / "@"
	 *
	 * to         ~= *tqchar
	 * tqchar     ~= <qchar> without ";" and ":" 
	 * 
	 * Scheme 'mailto:' already parsed, starting parse after colon
	 */

	/* First extract to-part by searching for '?' and decoding % items
	 */

	if ( !uri_mailto_parse_recipients(parser, &p) )
		return FALSE;	

	/* Extract hfield items */	
	
	while ( *p != '\0' ) {		
		/* Extract hfield item by searching for '&' and decoding '%' items */
		if ( !uri_mailto_parse_headers(parser, &p) )
			return FALSE;		
	}
	
	return TRUE;
}

/*
 * Validation
 */

bool uri_mailto_validate
(const char *uri_body, const char **reserved_headers, 
	const char **unique_headers, int max_recipients, int max_headers, 
	struct sieve_error_handler *ehandler)
{
	struct uri_mailto_parser parser;

	memset(&parser, 0, sizeof(parser));
	parser.ehandler = ehandler;
	parser.max_recipients = max_recipients;
	parser.max_headers = max_headers;
	parser.reserved_headers = reserved_headers;
	parser.unique_headers = unique_headers;
	
	/* If no errors are reported, we don't need to record any data */
	if ( ehandler != NULL ) { 
		parser.pool = pool_datastack_create();

		parser.uri = p_new(parser.pool, struct uri_mailto, 1);
		p_array_init(&parser.uri->recipients, parser.pool, max_recipients);
		p_array_init(&parser.uri->headers, parser.pool, max_headers);
	}
	
	if ( !uri_mailto_parse_uri(&parser, uri_body) )
		return FALSE;
		
	if ( ehandler != NULL ) {
		if ( array_count(&parser.uri->recipients) == 0 )
			uri_mailto_warning(&parser, "notification URI specifies no recipients");
	}
   
	return TRUE;
}

/*
 * Parsing
 */

struct uri_mailto *uri_mailto_parse
(const char *uri_body, pool_t pool, const char **reserved_headers, 
	const char **unique_headers, int max_recipients, int max_headers, 
	struct sieve_error_handler *ehandler)
{
	struct uri_mailto_parser parser;
	
	parser.pool = pool;
	parser.ehandler = ehandler;
	parser.max_recipients = max_recipients;
	parser.max_headers = max_headers;
	parser.reserved_headers = reserved_headers;
	parser.unique_headers = unique_headers;

	parser.uri = p_new(pool, struct uri_mailto, 1);
	p_array_init(&parser.uri->recipients, pool, max_recipients);
	p_array_init(&parser.uri->headers, pool, max_headers);
	
	if ( !uri_mailto_parse_uri(&parser, uri_body) )
		return FALSE;
		
	if ( ehandler != NULL ) {
		if ( array_count(&parser.uri->recipients) == 0 )
			uri_mailto_warning(&parser, "notification URI specifies no recipients");
	}

	return parser.uri;
}





