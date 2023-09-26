#include <langinfo.h>
#include <string.h>

/*
 * How this was arrived at:
 * - Grab existing LC_CTYPEs:
 *   `ls -1 /usr/share/locale | grep '\.' | sed -Ee 's/^[^.]+\.//' | sort -u`
 * - Have a co-worker grab current GNU canonical names (just the names).
 * - Map it.  Unmapped entries fallback to the default, which is whatever
 *    the first charset_mapping entry is.
 */
static const struct charset_map_entry {
	const char *sysname;
	const char *stdname;
} charset_mappings[] = {
	/* Most common */
	{ "UTF-8",		"UTF-8" },	/* Default */
	{ "US-ASCII",		"ASCII" },
	/* Sorted */
	{ "Big5",		"BIG5" },
	{ "Big5HKSCS",		"BIG5-HKSCS" },
	{ "CP1251",		"CP1251" },
	{ "CP866",		"CP866" },
	{ "CP949",		"CP949" },
	{ "GB18030",		"GB18030" },
	{ "GB2312",		"GB2312" },
	{ "GBK",		"GBK" },
	{ "ISO8859-1",		"ISO-8559-1" },
	{ "ISO8859-13",		"ISO-8559-13" },
	{ "ISO8859-15",		"ISO-8559-15" },
	{ "ISO8859-2",		"ISO-8559-2" },
	{ "ISO8859-4",		"ISO-8559-4" },
	{ "ISO8859-5",		"ISO-8559-5" },
	{ "ISO8859-7",		"ISO-8559-7" },
	{ "ISO8859-9",		"ISO-8559-9" },
	{ "KOI8-R",		"KOI8-R" },
	{ "KOI8-U",		"KOI8-U" },
	{ "SJIS",		"SHIFT_JIS" },
};

#ifndef nitems
#define nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

const char *
locale_charset(void)
{
	const struct charset_map_entry *cmap;
	const char *sysname;
	size_t i;

	sysname = nl_langinfo(CODESET);
	if (strcmp(sysname, "") == 0)
		return (charset_mappings[0].stdname);

	for (i = 0; i < nitems(charset_mappings); i++) {
		cmap = &charset_mappings[i];
		/* Check it */
		if (strcmp(cmap->sysname, sysname) == 0)
			return (cmap->stdname);
	}

	return (charset_mappings[0].stdname);
}

void
libcharset_set_relocation_prefix(const char *a __unused, const char *b __unused)
{
	/*
	 * Binary compatible stub to match what libiconv does with its
	 * equivalent libiconv_set_relocation_prefix.
	 */
}
