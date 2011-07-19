/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#ifndef __URI_MAILTO_H
#define __URI_MAILTO_H

/* 
 * Types 
 */

struct uri_mailto_header_field {
	const char *name;
	const char *body;
};

struct uri_mailto_recipient {
	const char *full;
	const char *normalized;
	bool carbon_copy;
};

ARRAY_DEFINE_TYPE(recipients, struct uri_mailto_recipient);
ARRAY_DEFINE_TYPE(headers, struct uri_mailto_header_field);

struct uri_mailto {
	ARRAY_TYPE(recipients) recipients;
	ARRAY_TYPE(headers) headers;
	const char *subject;
	const char *body;
	const char *from_normalized;
};

bool uri_mailto_validate
	(const char *uri_body, const char **reserved_headers, 
		const char **unique_headers, int max_recipients, int max_headers, 
		struct sieve_error_handler *ehandler);

struct uri_mailto *uri_mailto_parse
(const char *uri_body, pool_t pool, const char **reserved_headers, 
	const char **unique_headers, int max_recipients, int max_headers, 
	struct sieve_error_handler *ehandler);

#endif /* __URI_MAILTO_H */


