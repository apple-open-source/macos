/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __IMAP_QUOTE_H
#define __IMAP_QUOTE_H

/* Return value suitable for sending to client, either as quoted-string or
   literal. Note that this also converts TABs into spaces, multiple spaces
   into single space and NULs to #128. */
char *managesieve_quote(pool_t pool, const unsigned char *value, size_t value_len);

/* Append to existing string. */
void managesieve_quote_append(string_t *str, const unsigned char *value,
		       size_t value_len, bool compress_lwsp);

#define managesieve_quote_append_string(str, value, compress_lwsp) \
	managesieve_quote_append(str, (const unsigned char *)(value), \
			  (size_t)-1, compress_lwsp)

#endif
