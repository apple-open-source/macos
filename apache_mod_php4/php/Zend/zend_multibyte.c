/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2003 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        | 
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Masaki Fujimoto <masaki-f@fides.dti.ne.jp>                  |
   |          Rui Hirokawa <hirokawa@php.net>                             |
   +----------------------------------------------------------------------+
*/

/*	$Id: zend_multibyte.c,v 1.4.2.1.8.3 2008/08/06 08:09:07 derick Exp $ */

#include "zend.h"
#include "zend_compile.h"
#include "zend_operators.h"
#include "zend_multibyte.h"

#ifdef ZEND_MULTIBYTE

const char *utf8_aliases[] = {"UTF8", NULL};
zend_encoding encoding_utf8 = {
	NULL,
	NULL,
	"UTF-8",
	(const char*(*)[])&utf8_aliases,
	1
};

const char *euc_jp_aliases[] = {"EUC_JP", "eucJP", "x-euc-jp", NULL};
zend_encoding encoding_euc_jp = {
	NULL,
	NULL,
	"EUC-JP",
	(const char*(*)[])&euc_jp_aliases,
	1
};

const char *sjis_aliases[] = {"sjis", "x-sjis", "SHIFT-JIS", NULL};
zend_encoding encoding_sjis = {
	sjis_input_filter,
	sjis_output_filter,
	"Shift_JIS",
	(const char*(*)[])&sjis_aliases,
	0
};

const char *eucjp_win_aliases[] = {"eucJP-open", NULL};
zend_encoding encoding_eucjp_win = {
	NULL,
	NULL,
	"eucJP-win",
	(const char *(*)[])&eucjp_win_aliases,
	1
};

const char *sjis_win_aliases[] = {"SJIS-open", "MS_Kanji", "Windows-31J", "CP932", NULL};
zend_encoding encoding_sjis_win = {
	/* sjis-filters does not care about diffs of Shift_JIS and CP932 */
	sjis_input_filter,
	sjis_output_filter,
	"SJIS-win",
	(const char *(*)[])&sjis_win_aliases,
	0
};

const char *ascii_aliases[] = {"us-ascii", NULL};
zend_encoding encoding_ascii = {
	NULL,
	NULL,
	"ASCII",
	(const char*(*)[])&ascii_aliases,
	1
};

zend_encoding *zend_encoding_table[] = {
	&encoding_utf8,
	&encoding_ascii,
	&encoding_euc_jp,
	&encoding_sjis,
	&encoding_eucjp_win,
	&encoding_sjis_win,
	NULL
};


static char* zend_multibyte_assemble_list(zend_encoding **encoding_list, int
		encoding_list_size);
static int zend_multibyte_parse_encoding_list(const char *encoding_list, 
		int encoding_list_size, zend_encoding ***result, int *result_size);
static inline char* zend_memnstr(char *haystack, char *needle, int needle_len,
		char *end);


ZEND_API int zend_multibyte_set_script_encoding(char *encoding_list, 
		int encoding_list_size TSRMLS_DC)
{
	if (CG(script_encoding_list)) {
		efree(CG(script_encoding_list));
		CG(script_encoding_list) = NULL;
	}
	CG(script_encoding_list_size) = 0;

	if (!encoding_list) {
		return 0;
	}

	zend_multibyte_parse_encoding_list(encoding_list, encoding_list_size,
			&(CG(script_encoding_list)), &(CG(script_encoding_list_size)));

	return 0;
}


ZEND_API int zend_multibyte_set_internal_encoding(char *encoding_name, 
		int encoding_name_size TSRMLS_DC)
{
	CG(internal_encoding) = zend_multibyte_fetch_encoding(encoding_name);
	return 0;
}


ZEND_API int zend_multibyte_set_functions(zend_encoding_detector
		encoding_detector, zend_encoding_converter encoding_converter,
		zend_multibyte_oddlen multibyte_oddlen  TSRMLS_DC)
{
	CG(encoding_detector) = encoding_detector;
	CG(encoding_converter) = encoding_converter;
	CG(multibyte_oddlen) = multibyte_oddlen;
	return 0;
}


ZEND_API int zend_multibyte_set_filter(zend_encoding *onetime_encoding
		TSRMLS_DC)
{
	zend_encoding *script_encoding = NULL, *internal_encoding = NULL;
	char *name, *list;

	LANG_SCNG(input_filter) = NULL;
	LANG_SCNG(output_filter) = NULL;
	LANG_SCNG(script_encoding) = NULL;
	LANG_SCNG(internal_encoding) = NULL;

	/* decision making... */
	if (!onetime_encoding) {
		if (!CG(script_encoding_list) || !CG(script_encoding_list_size)) {
			script_encoding = NULL;
		} else if (CG(script_encoding_list_size) > 1 && CG(encoding_detector)) {
			/* we need to detect automatically */
			list = zend_multibyte_assemble_list(CG(script_encoding_list),
					CG(script_encoding_list_size));
			name = CG(encoding_detector)(LANG_SCNG(code), 
					LANG_SCNG(code_size), list TSRMLS_CC);
			if (list) {
				efree(list);
			}
			script_encoding = zend_multibyte_fetch_encoding(name);
			efree(name);
		} else {
			script_encoding = *(CG(script_encoding_list));
		}
	} else {
		script_encoding = onetime_encoding;
	}

	if (CG(encoding_converter)) {
		internal_encoding = CG(internal_encoding);
	} else {
		internal_encoding = NULL;
	}

	/* judge input/output filter */
	LANG_SCNG(script_encoding) = script_encoding;
	LANG_SCNG(internal_encoding) = internal_encoding;
	if (!script_encoding) {
		return 0;
	}

	if (!internal_encoding) {
		if (script_encoding->input_filter) {
			LANG_SCNG(input_filter) = script_encoding->input_filter;
		}
		if (script_encoding->output_filter) {
			LANG_SCNG(output_filter) = script_encoding->output_filter;
		}
		return 0;
	}

	if (script_encoding == internal_encoding) {
		if (script_encoding->compatible) {
			return 0;
		}

		if (script_encoding->input_filter) {
			LANG_SCNG(input_filter) = script_encoding->input_filter;
		}
		if (script_encoding->output_filter) {
			LANG_SCNG(output_filter) = script_encoding->output_filter;
		}
		return 0;
	}

	if (internal_encoding->compatible) {
		LANG_SCNG(input_filter) = zend_multibyte_encoding_filter;
		return 0;
	}

	if (script_encoding->compatible) {
		LANG_SCNG(output_filter) = zend_multibyte_encoding_filter;
		return 0;
	}

	/* no way! it's too hard... :( */
	if (script_encoding->input_filter && internal_encoding->output_filter) {
		LANG_SCNG(input_filter) = script_encoding->input_filter;
		LANG_SCNG(output_filter) = internal_encoding->output_filter;
	}

	return 0;
}


ZEND_API zend_encoding* zend_multibyte_fetch_encoding(char *encoding_name)
{
	int i, j;
	zend_encoding *encoding;

	if (!encoding_name) {
		return NULL;
	}

	for (i = 0; (encoding = zend_encoding_table[i]) != NULL; i++) {
		if (zend_binary_strcasecmp((char*)encoding->name, 
					strlen(encoding->name), encoding_name, 
					strlen(encoding_name)) == 0) {
			return encoding;
		}
	}

	for (i = 0; (encoding = zend_encoding_table[i]) != NULL; i++) {
		if (encoding->aliases != NULL) {
			for (j = 0; (*encoding->aliases)[j] != NULL; j++) {
				if (zend_binary_strcasecmp((char*)(*encoding->aliases)[j], 
						strlen((*encoding->aliases)[j]), encoding_name, 
						strlen(encoding_name)) == 0) {
					return encoding;
				}
			}
		}
	}

	return NULL;
}


ZEND_API int zend_multibyte_encoding_filter(char **to, int *to_length,
		const char *from, int from_length TSRMLS_DC)
{
	int oddlen;

	if (!CG(encoding_converter)) {
		return 0;
	}

	if (CG(multibyte_oddlen)) {
		oddlen = CG(multibyte_oddlen)(from, from_length,
				LANG_SCNG(script_encoding)->name TSRMLS_CC);
		if (oddlen > 0) {
			from_length -= oddlen;
		}
	}

	if (CG(encoding_converter)(to, to_length, from, from_length,
			LANG_SCNG(internal_encoding)->name,
			LANG_SCNG(script_encoding)->name
			TSRMLS_CC) != 0) {
		return 0;
	}

	return from_length;
}


/*
 *	Shift_JIS Input/Output Filter
 */
static const unsigned char table_sjis[] = { /* 0x80-0x9f,0xE0-0xEF */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 0, 0, 0
};

int sjis_input_filter(char **buf, int *length, const char *sjis, int sjis_length
		TSRMLS_DC)
{
	unsigned char *p, *q;
	unsigned char  c1, c2;

	*buf = (char*)emalloc(sjis_length*3/2+1);
	if (!*buf)
		return 0;
	*length = 0;

	p = (unsigned char*)sjis;
	q = (unsigned char*)*buf;

	/* convert [SJIS -> EUC-JP] (for lex scan) -- some other better ways? */
	while (*p && (p-(unsigned char*)sjis) < sjis_length) {
		if (!(*p & 0x80)) {
			*q++ = *p++;
			continue;
		}

		/* handling 8 bit code */
		if (table_sjis[*p] == 1) {
			/* 1 byte kana */
			*q++ = 0x8e;
			*q++ = *p++;
			continue;
		}

		if (!*(p+1)) {
			*q++ = *p++;
			break;
		}

		if (table_sjis[*p] == 2) {
			/* 2 byte kanji code */
			c1 = *p++;
			if (!*p || (p-(unsigned char*)sjis) >= sjis_length) {
				break;
			}
			c2 = *p++;
			c1 -= (c1 <= 0x9f) ? 0x71 : 0xb1;
			c1 = (c1 << 1) + 1;
			if (c2 >= 0x9e) {
				c2 -= 0x7e;
				c1++;
			} else if (c2 > 0x7f) {
				c2 -= 0x20;
			} else {
				c2 -= 0x1f;
			}

			c1 |= 0x80;
			c2 |= 0x80;

			*q++ = c1;
			*q++ = c2;
		} else {
			/*
			 * for user defined chars (ATTENTION)
			 *
			 * THESE ARE NOT CODE FOR CONVERSION! :-P
			 * (using *ILLEGALLY* 3byte EUC-JP space)
			 *
			 * we cannot perfectly (== 1 to 1)  convert these chars to EUC-JP.
			 * so, these code are for perfect RESTORING in sjis_output_filter()
			 */
			c1 = *p++;
			if (!*p || (p-(unsigned char*)sjis) >= sjis_length) {
				break;
			}
			c2 = *p++;
			*q++ = (char)0x8f;
			/*
			 * MAP TO (EUC-JP):
			 * type A: 0xeba1 - 0xf4fe
			 * type B: 0xf5a1 - 0xfefe
			 * type C: 0xa1a1 - 0xa6fe
			 */
			c1 -= (c1 > 0xf9) ? (0x79+0x71) : (0x0a+0xb1);
			c1 = (c1 << 1) + 1;
			if (c2 >= 0x9e) {
				c2 -= 0x7e;
				c1++;
			} else if (c2 > 0x7f) {
				c2 -= 0x20;
			} else {
				c2 -= 0x1f;
			}
			
			c1 |= 0x80;
			c2 |= 0x80;

			*q++ = c1;
			*q++ = c2;
		}
	}
	*q = (char)NULL;
	*length = (char*)q - *buf;

	return *length;
}

static const unsigned char table_eucjp[] = { /* 0xA1-0xFE */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1
};

int sjis_output_filter(char **sjis, int *sjis_length, const char *buf, int length
		TSRMLS_DC)
{
	unsigned char c1, c2;
	char *p;
	const char *q;

	if (!sjis || !sjis_length) {
		return 0;
	}

	/* always Shift_JIS <= EUC-JP */
	*sjis = (char*)emalloc(length+1);
	if (!sjis) {
		return 0;
	}
	p = *sjis;
	q = buf;

	/* restore converted strings [EUC-JP -> Shift_JIS] */
	while (*q) {
		if (!(*q & 0x80)) {
			*p++ = *q++;
			continue;
		}

		/* hankaku kana */
		if (*q == (char)0x8e) {
			q++;
			if (*q) {
				*p++ = *q++;
			}
			continue;
		}

		/* 2 byte kanji code */
		if (table_eucjp[(unsigned char)*q] == 2) {
			c1 = (*q++ & ~0x80) & 0xff;
			if (*q) {
				c2 = (*q++ & ~0x80) & 0xff;
			} else {
				q--;
				break;
			}

			c2 += (c1 & 0x01) ? 0x1f : 0x7d;
			if (c2 >= 0x7f) {
				c2++;
			}
			c1 = ((c1 - 0x21) >> 1) + 0x81;
			if (c1 > 0x9f) {
				c1 += 0x40;
			}
			
			*p++ = c1;
			*p++ = c2;
			continue;
		}

		if (*q == (char)0x8f) {
			q++;
			if (*q) {
				c1 = (*q++ & ~0x80) & 0xff;
			} else {
				q--;
				break;
			}
			if (*q) {
				c2 = (*q++ & ~0x80) & 0xff;
			} else {
				q -= 2;
				break;
			}
			
			c2 += (c1 & 0x01) ? 0x1f : 0x7d;
			if (c2 >= 0x7f) {
				c2++;
			}
			c1 = ((c1 - 0x21) >> 1) + 0x81;
			if (c1 > 0x9f) {
				c1 += 0x40;
			}
			
			if (c1 >= 0x81 && c1 <= 0x9f) {
				c1 += 0x79;
			} else {
				c1 += 0x0a;
			}
			
			*p++ = c1;
			*p++ = c2;
			continue;
		}

		/* some other chars (may not happen) */
		*p++ = *q++;
	}
	*p = '\0';
	*sjis_length = p - *sjis;

	return q-buf;	/* return length we actually read */
}


static char* zend_multibyte_assemble_list(zend_encoding **encoding_list, int
		encoding_list_size)
{
	int i, list_size = 0;
	const char *name;
	char *list = NULL;

	if (!encoding_list || !encoding_list_size) {
		return NULL;
	}

	for (i = 0; i < encoding_list_size; i++) {
		name = (*(encoding_list+i))->name;
		if (name) {
			list_size += strlen(name) + 1;
			if (!list) {
				list = (char*)emalloc(list_size);
				if (!list) {
					return NULL;
				}
				*list = (char)NULL;
			} else {
				list = (char*)erealloc(list, list_size);
				if (!list) {
					return NULL;
				}
				strcat(list, ",");
			}
			strcat(list, name);
		}
	}
	return list;
}


/*
 *	code stolen from ext/mbstring/mbstring.c
 */
static int zend_multibyte_parse_encoding_list(const char *encoding_list, 
		int encoding_list_size, zend_encoding ***result, int *result_size)
{
	int n, size;
	char *p, *p1, *p2, *endp, *tmpstr;
	zend_encoding **list, **entry, *encoding;

	list = NULL;
	if (encoding_list == NULL || encoding_list_size <= 0) {
		return -1;
	} else {
		/* copy the encoding_list string for work */
		tmpstr = (char *)estrndup(encoding_list, encoding_list_size);
		if (tmpstr == NULL) {
			return -1;
		}
		/* count the number of listed encoding names */
		endp = tmpstr + encoding_list_size;
		n = 1;
		p1 = tmpstr;
		while ((p2 = zend_memnstr(p1, ",", 1, endp)) != NULL) {
			p1 = p2 + 1;
			n++;
		}
		size = n;
		/* make list */
		list = (zend_encoding**)ecalloc(size, sizeof(zend_encoding*));
		if (list != NULL) {
			entry = list;
			n = 0;
			p1 = tmpstr;
			do {
				p2 = p = zend_memnstr(p1, ",", 1, endp);
				if (p == NULL) {
					p = endp;
				}
				*p = '\0';
				/* trim spaces */
				while (p1 < p && (*p1 == ' ' || *p1 == '\t')) {
					p1++;
				}
				p--;
				while (p > p1 && (*p == ' ' || *p == '\t')) {
					*p = '\0';
					p--;
				}
				/* convert to the encoding number and check encoding */
				encoding = zend_multibyte_fetch_encoding(p1);
				if (encoding)
				{
					*entry++ = encoding;
					n++;
				}
				p1 = p2 + 1;
			} while (n < size && p2 != NULL);
			*result = list;
			*result_size = n;
		}
		efree(tmpstr);
	}

	if (list == NULL) {
		return -1;
	}

	return 0;
}

/*
 *	code stolen from ext/standard/php_string.h
 */
static inline char* zend_memnstr(char *haystack, char *needle, int needle_len, 
		char *end)
{
	char *p = haystack;
	char first = *needle;

	/* let end point to the last character where needle may start */
	if (needle_len > end - haystack) {
		return NULL;
	}
	end -= needle_len;
	
	while (p <= end) {
		while (*p != first)
			if (++p > end)
				return NULL;
		if (memcmp(p, needle, needle_len) == 0)
			return p;
		p++;
	}
	return NULL;
}

#endif /* ZEND_MULTIBYTE */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 tw=78
 * vim<600: sw=4 ts=4 tw=78
 */
