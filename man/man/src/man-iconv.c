/*
 * From andy@pylesos.asp-linux.com.ua  Tue Dec  3 14:17:38 2002
 * (polished, aeb)
 *
 * Manpages for a given language have a traditional character set.
 * E.g., for Russian this is koi8r.
 * If the user uses a different locale, throw in an invocation of iconv.
 *
 * Exports:
 *   const char *get_converter (const char *path);
 * Conversion is to the users locale. Conversion is from the
 * manpage charset, found in environment variables, or in
 * PATH/.charset, where PATH is the directory (below that) containing
 * the man page.
 *
 * TODO: adapt this to man.conf way
 */

/*
 * By default iconv is not used - this is the wrong interface.
 * But if you want it, define USE_ICONV.
 */
#undef USE_ICONV

#include <stdio.h>	/* NULL */

#if defined __GLIBC__ && __GLIBC__ >= 2 && defined USE_ICONV
#include <stdlib.h>	/* getenv */
#include <unistd.h>	/* access */
#include <string.h>	/* strcmp */
#include <locale.h>	/* setlocale */
#include <langinfo.h>	/* nl_langinfo */
#include <iconv.h>	/* iconv_open */
#include "man-iconv.h"	/* get_converter */
#include "util.h"	/* my_strdup */
#include "man.h"	/* debug */

static char *
find_iconv(void) {
	static char *iconv_path = NULL;
	static int inited = 0;

	if (!inited) {
		char *file = getenv("MAN_ICONV_PATH");
		if (!file)
			file = "/usr/bin/iconv";
		if (access(file, X_OK) == 0)
			iconv_path = my_strdup(file);
		inited = 1;
	}
	return iconv_path;
}

static char *
iconv_extra_flags(void) {
	static char *iconv_flags = "-cs";
	static int inited = 0;

	if (!inited) {
		char *opt = getenv("MAN_ICONV_OPT");
		if (opt)
			iconv_flags = my_strdup(opt);
		inited = 1;
	}
	return iconv_flags;
}

static char *
get_locale_charset (void) {
	char *old_lc_ctype, *charset;

	if ((charset = getenv("MAN_ICONV_OUTPUT_CHARSET")) == NULL) {
		old_lc_ctype = setlocale(LC_CTYPE, "");
		charset = nl_langinfo(CODESET);
		setlocale(LC_CTYPE, old_lc_ctype);
	}
	return charset;
}

static char * 
get_man_charset (const char *path) {
	char *charset_env, *file, *path2, *p;
	FILE *f = NULL;

	charset_env = getenv("MAN_ICONV_INPUT_CHARSET");
	if (charset_env)
		return charset_env;

	if (!path || !*path)
		return NULL;

	if (debug)
		fprintf(stderr, "get_man_charset: path=%s\n", path);

	/* strip trailing "/.." and try that directory first */
	path2 = my_strdup(path);
	p = strrchr(path2, '/');
	if (p && !strcmp(p, "/..")) {
		*p = 0;
		file = my_xsprintf("%s/.charset", path2);
		f = fopen(file, "r");
		free(file);
	}
	free(path2);

	/* if that fails, try path itself */
	if (f == NULL) {
		file = my_xsprintf("%s/.charset", path);
		f = fopen(file, "r");
		free(file);
	}

	if (f) {
		char charset[100], *p;

		fgets(charset, sizeof(charset), f);
		fclose(f);
		fprintf(stderr, "read %s\n", charset);
		p = strchr(charset, '\n');
		if (p) {
			*p = 0;
			return my_strdup(charset);
		}
	}
	return NULL;
}

static int 
is_conversion_supported (char *from, char *to) {
	iconv_t cd;

	if (!from || !*from || !to || !*to || !strcmp(from,to))
		return 0;
	if ((cd = iconv_open(to, from)) != (iconv_t) -1) {
		iconv_close(cd);
		return 1;
	}
	return 0;
}

const char *
get_converter (const char *path) {
	char *from, *to, *iconv_path;

	iconv_path = find_iconv();
	from = get_man_charset(path);
	to = get_locale_charset();
	if (debug)
		fprintf(stderr, "get_converter: iconv_path=%s from=%s to=%s\n",
			iconv_path, from, to);
	if (iconv_path && is_conversion_supported(from, to))
		return my_xsprintf("%s %s -f %s -t %s",
				   iconv_path, iconv_extra_flags(), from, to);
	return NULL;
}
#else
#include "man-iconv.h"

const char *
get_converter (const char *path) {
	return NULL;
}
#endif /* __GLIBC__ && __GLIBC__ >= 2 */
