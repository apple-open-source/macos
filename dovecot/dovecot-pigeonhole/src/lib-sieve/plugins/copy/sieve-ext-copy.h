/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_EXT_COPY_H
#define __SIEVE_EXT_COPY_H

/* sieve_ext_copy_get_extension():
 *   Get the extension struct for the copy extension.
 */
static inline const struct sieve_extension *sieve_ext_copy_get_extension
(struct sieve_instance *svinst)
{
	return sieve_extension_get_by_name(svinst, "copy");
}

/* sieve_ext_copy_register_tag():
 *   Register the :copy tagged argument for a command other than fileinto and
 *   redirect.
 */
void sieve_ext_copy_register_tag
	(struct sieve_validator *valdtr, const struct sieve_extension *copy_ext,
		const char *command);

#endif /* __SIEVE_EXT_COPY_H */
