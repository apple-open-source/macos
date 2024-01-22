#include <langinfo.h>
#include <string.h>

#ifndef nitems
#define nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

/*
 * This is not the whole story; once we gain charset.alias support, we actually
 * only return UTF-8 as part of the default mapping of * -> UTF-8.  This will
 * eventually change to ASCII, which is the default if we don't have a mapping
 * set at all.
 */
#define LOCALE_CHARSET_DEFAULT	"UTF-8"

const char *
locale_charset(void)
{

	return (LOCALE_CHARSET_DEFAULT);
}

void
libcharset_set_relocation_prefix(const char *a __unused, const char *b __unused)
{
	/*
	 * Binary compatible stub to match what libiconv does with its
	 * equivalent libiconv_set_relocation_prefix.
	 */
}
