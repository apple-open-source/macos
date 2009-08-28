/*
 * $FreeBSD: src/usr.bin/colldef/common.h,v 1.2 2001/11/28 09:50:24 ache Exp $
 */

#include <sys/types.h>
#include <db.h>
#include <fcntl.h>

#define CHARMAP_SYMBOL_LEN 64
#define BUFSIZE 80

#define NOTEXISTS	0
#define EXISTS		1

#define	SYMBOL_CHAR	0
#define	SYMBOL_CHAIN	1
#define	SYMBOL_SYMBOL	2
#define	SYMBOL_STRING	3
#define	SYMBOL_IGNORE	4
#define	SYMBOL_ELLIPSIS	5
struct symbol {
	int type;
	int val;
	wchar_t name[CHARMAP_SYMBOL_LEN];
	union {
		wchar_t wc;
		wchar_t str[STR_LEN];
	} u;
};

extern int line_no;

struct symbol *getsymbol(const wchar_t *, int);
extern char *showwcs(const wchar_t *, int);

extern char map_name[FILENAME_MAX];
