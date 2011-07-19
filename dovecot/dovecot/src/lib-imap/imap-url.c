/*
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without  
 * modification, are permitted provided that the following conditions  
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright  
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above  
 * copyright notice, this list of conditions and the following  
 * disclaimer in the documentation and/or other materials provided  
 * with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
 * contributors may be used to endorse or promote products derived  
 * from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF 
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.
 */

#include "lib.h"
#include "str.h"
#include "utc-mktime.h"
#include "imap-utf7.h"
#include "imap-url.h"

#include <time.h>
#include <ctype.h>

#define	MECH_INTERNAL_ONLY	1

#define	LOWALPHA	"abcdefghijklmnopqrstuvwxyz"	/* RFC 1738 */
#define	HIALPHA		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"	/* RFC 1738 */
#define	ALPHA		LOWALPHA""HIALPHA		/* RFC 1738 */
#define	DIGIT		"0123456789"			/* RFC 1738 */
#define	SAFE		"$-_.+"				/* RFC 1738 */
#define	EXTRA		"!*'(),"			/* RFC 1738 */
#define	UNRESERVED	ALPHA""DIGIT""SAFE""EXTRA	/* RFC 1738 */
#define	ESCAPE		"%"				/* RFC 1738 */
#define	UCHAR		UNRESERVED""ESCAPE		/* RFC 1738 */
#define	ACHAR		UCHAR"&=~"			/* RFC 2192 */
#define	BCHAR		ACHAR":@/"			/* RFC 2192 */
#define	HEXDIG		DIGIT"ABCDEFabcdef"		/* RFC 2234 */
#define	HOST_CHARS	ALPHA".:-"DIGIT			/* RFC 1738 */
#define	DATETIME_CHARS	DIGIT".+-:TZ"			/* RFC 3339 */

// any CHAR except "(" / ")" / "{" / SP / CTL / "%" / "*" / '"' / "\" / "]"
#define	ATOM_CHARS	"\
!#$&'+,-./0123456789:;<=>?@\
ABCDEFGHIJKLMNOPQRSTUVWXYZ[^_`\
abcdefghijklmnopqrstuvwxyz|}~"

// any CHAR except "(" / ")" / "{" / SP / CTL / "%" / "*" / '"' / "\"
#define	ASTRING_CHARS	ATOM_CHARS"]"

static char atom_chars_allowed[256];
static char astring_chars_allowed[256];
static char quoted_allowed[256];	// any CHAR except '"' / "\" / CR / LF
static char quoted_specials_allowed[256];	// any CHAR except CR / LF
static char host_chars_allowed[256];
static char url_resp_allowed[256];
static bool initialized = FALSE;

// initialize the *_allowed tables once
static void init_allowed(void)
{
	if (!initialized) {
		const unsigned char *cp;
		int i;

		for (cp = (const unsigned char *) ATOM_CHARS; *cp; cp++)
			atom_chars_allowed[*cp & 0xff] = 1;

		for (cp = (const unsigned char *) ASTRING_CHARS; *cp; cp++)
			astring_chars_allowed[*cp & 0xff] = 1;

		for (cp = (const unsigned char *) HOST_CHARS; *cp; cp++)
			host_chars_allowed[*cp & 0xff] = 1;

		for (i = 0x01; i <= 0x7f; i++)
			quoted_specials_allowed[i] = 1;
		quoted_specials_allowed['\r'] = 0;
		quoted_specials_allowed['\n'] = 0;
		memcpy(quoted_allowed, quoted_specials_allowed,
		       sizeof quoted_allowed);
		quoted_allowed['"'] = 0;
		quoted_allowed['\\'] = 0;

		for (i = 0x01; i <= 0x09; i++)
			url_resp_allowed[i] = 1;
		for (i = 0x0b; i <= 0x0c; i++)
			url_resp_allowed[i] = 1;
		// RFC 4469 erroneously skips 0x5c but that's "\" not "]"
		for (i = 0x0e; i <= 0x5c; i++)
			url_resp_allowed[i] = 1;
		for (i = 0x5e; i <= 0xfe; i++)
			url_resp_allowed[i] = 1;

		initialized = TRUE;
	}
}

// URL-decode enc into dec (%XX decoding)
bool url_decode(const char *enc, string_t *dec)
{
	const char *cp;

	for (cp = enc; *cp; cp++) {
		if (*cp == '%') {
			unsigned int val;
			if (cp[1] >= '0' && cp[1] <= '9')
				val = cp[1] - '0';
			else if (cp[1] >= 'A' && cp[1] <= 'F')
				val = 10 + cp[1] - 'A';
			else if (cp[1] >= 'a' && cp[1] <= 'f')
				val = 10 + cp[1] - 'a';
			else
				return FALSE;
			val *= 16;
			if (cp[2] >= '0' && cp[2] <= '9')
				val += cp[2] - '0';
			else if (cp[2] >= 'A' && cp[2] <= 'F')
				val += 10 + cp[2] - 'A';
			else if (cp[2] >= 'a' && cp[2] <= 'f')
				val += 10 + cp[2] - 'a';
			else
				return FALSE;
			if (val == 0 || val > 0xff)
				return FALSE;
			str_append_c(dec, val);
			cp += 2;
		} else
			str_append_c(dec, *cp);
	}

	return TRUE;
}

// parse an RFC 2192+4467 URL into its parts
void imap_url_parse(const char *url, struct imap_url_parts *parts)
{
	const char *rump, *p;
	size_t s;

	rump = url;

	// "imap://" ...
	if (strncasecmp(url, "imap://", 7) == 0) {
		url += 7;
				      
		// ... enc-user ...
		s = strcspn(url, ";@");
		if (s <= 0)
			return;
		parts->user = t_strdup_until(url, url + s);
		url += s;

		// ... [";AUTH=" ( "*" / enc_auth_type )] ...
		if (strncasecmp(url, ";AUTH=", 6) == 0) {
			url += 6;
			p = strchr(url, '@');
			if (p == NULL)
				return;
			parts->auth_type = t_strdup_until(url, p);
			url = p;
		}

		// ... "@" hostport ...
		if (*url != '@')
			return;
		++url;
		p = strchr(url, '/');
		if (p == NULL)
			return;
		parts->hostport = t_strdup_until(url, p);
		url = p;
	}

	// ... ["/" enc_mailbox] ...
	if (*url == '/' && strncasecmp(url, "/;UID=", 6) != 0) {
		++url;
		p = strcasestr(url, ";UIDVALIDITY=");
		if (p == NULL)
			p = strcasestr(url, "/;UID=");
		if (p == NULL)
			return;
		parts->mailbox = t_strdup_until(url, p);
		url = p;
	}

	// ... [";UIDVALIDITY=" nz_number] ...
	if (strncasecmp(url, ";UIDVALIDITY=", 13) == 0) {
		url += 13;
		p = strchr(url, '/');
		if (p == NULL)
			return;
		parts->uidvalidity = t_strdup_until(url, p);
		url = p;
	}

	// ... ["/;UID=" nz_number] ...
	if (strncasecmp(url, "/;UID=", 6) == 0) {
		url += 6;
		s = strcspn(url, ";/");
		if (s <= 0)
			return;
		parts->uid = t_strdup_until(url, url + s);
		url += s;
	}

	// ... ["/;SECTION=" enc_section] ...
	if (strncasecmp(url, "/;SECTION=", 10) == 0) {
		url += 10;
		p = strchr(url, ';');
		if (p == NULL) {
			parts->section = t_strdup(url);
			return;
		}
		parts->section = t_strdup_until(url, p);
		url = p;
	}

	// ... [";EXPIRE=" date-time] ...
	if (strncasecmp(url, ";EXPIRE=", 8) == 0) {
		url += 8;
		p = strchr(url, ';');
		if (p == NULL)
			return;
		parts->expiration = t_strdup_until(url, p);
		// set expiration_time after decoding
		url = p;
	}

	// ... [";URLAUTH=" access] ...
	if (strncasecmp(url, ";URLAUTH=", 9) != 0)
		return;
	url += 9;
	p = strchr(url, ':');
	if (p == NULL) {
		parts->access = url;
		return;
	}
	parts->access = t_strdup_until(url, p);
	url = p;

	// save rump
	parts->rump = t_strdup_until(rump, url);

	// ... [ ":INTERNAL:" 32*HEXDIG ]
	if (strncasecmp(url, ":INTERNAL:", 10) != 0)
		return;
	parts->mechanism = "INTERNAL";
	url += 10;
	parts->urlauth = url;
}

// parse an RFC 3339 date-time into a time_t, or return 0
static time_t imap_url_date_time_parse(const char *s)
{
	struct tm tm;
	time_t t;

	memset(&tm, 0, sizeof tm);

	/* YYYY- */
	if (!i_isdigit(s[0]) || !i_isdigit(s[1]) ||
	    !i_isdigit(s[2]) || !i_isdigit(s[3]) || s[4] != '-')
		return 0;
	tm.tm_year = (((s[0] - '0') * 10 + s[1] - '0') * 10 +
		      s[2] - '0') * 10 + s[3] - '0' - 1900;
	s += 5;

	/* MM- */
	if (!i_isdigit(s[0]) || !i_isdigit(s[1]) || s[2] != '-')
		return 0;
	tm.tm_mon = (s[0] - '0') * 10 + s[1] - '0';
	if (tm.tm_mon < 1 || tm.tm_mon > 12)
		return 0;
	--tm.tm_mon;	/* 0-11 not 1-12 */
	s += 3;

	/* DDT */
	if (!i_isdigit(s[0]) || !i_isdigit(s[1]) || i_toupper(s[2]) != 'T')
		return 0;
	tm.tm_mday = (s[0] - '0') * 10 + s[1] - '0';
	s += 3;

	/* HH: */
	if (!i_isdigit(s[0]) || !i_isdigit(s[1]) || s[2] != ':')
		return 0;
	tm.tm_hour = (s[0] - '0') * 10 + s[1] - '0';
	s += 3;

	/* MM: */
	if (!i_isdigit(s[0]) || !i_isdigit(s[1]) || s[2] != ':')
		return 0;
	tm.tm_min = (s[0] - '0') * 10 + s[1] - '0';
	s += 3;

	/* SS */
	if (!i_isdigit(s[0]) || !i_isdigit(s[1]))
		return 0;
	tm.tm_sec = (s[0] - '0') * 10 + s[1] - '0';
	s += 2;

	/* skip optional fractional seconds */
	if (*s == '.') {
		do
			++s;
		while (i_isdigit(*s));
	}

	if (i_toupper(s[0]) == 'Z' && s[1] == '\0')
		tm.tm_gmtoff = 0;
	else if (s[0] == '+' || s[0] == '-') {
		char tzsign = *s++;

		/* HH: */
		if (!i_isdigit(s[0]) || !i_isdigit(s[1]) || s[2] != ':')
			return 0;
		tm.tm_gmtoff = ((s[0] - '0') * 10 + s[1] - '0') * 3600;
		s += 3;

		/* MM */
		if (!i_isdigit(s[0]) || !i_isdigit(s[1]) || s[2] != '\0')
			return 0;
		tm.tm_gmtoff += ((s[0] - '0') * 10 + s[1] - '0') * 60;

		if (tzsign == '-')
			tm.tm_gmtoff = -tm.tm_gmtoff;
	} else
		return 0;

	t = utc_mktime(&tm);
	if (t < 0)
		return 0;
	return t - tm.tm_gmtoff;
}

// decode the parts of a URL
bool imap_url_decode(const struct imap_url_parts *enc_parts,
		     struct imap_url_parts *dec_parts,
		     const char **error)
{
	string_t *str = t_str_new(256);

#define	URL_DECODE(field, name)						\
	if (enc_parts->field != NULL) {					\
		if (url_decode(enc_parts->field, str))			\
			dec_parts->field = t_strdup(str_c(str));	\
		else {							\
			*error = "invalid " name;			\
			str_free(&str);					\
			return FALSE;					\
		}							\
	}								\
	str_truncate(str, 0);

	URL_DECODE(user, "user ID");
	URL_DECODE(auth_type, "auth type");
	URL_DECODE(hostport, "server");
	URL_DECODE(mailbox, "mailbox");
	URL_DECODE(uidvalidity, "uidvalidity");
	URL_DECODE(uid, "uid");
	URL_DECODE(section, "section");
	URL_DECODE(expiration, "expiration");
	URL_DECODE(access, "access ID");
	URL_DECODE(mechanism, "mechanism");
	URL_DECODE(urlauth, "urlauth token");
	// do not set dec_parts->rump

	if (dec_parts->mailbox != NULL) {
		if (imap_utf8_to_utf7(dec_parts->mailbox, str) == 0)
			dec_parts->mailbox = t_strdup(str_c(str));
		else {
			*error = "invalid UTF-8 mailbox";
			str_free(&str);
			return FALSE;
		}
	}
	str_truncate(str, 0);

	if (dec_parts->expiration != NULL)
		dec_parts->expiration_time =
			imap_url_date_time_parse(dec_parts->expiration);

	str_free(&str);

	return TRUE;
}

// validate conformance to RFC 3501 "atom"
bool imap_url_atom_validate(const char *s)
{
	const unsigned char *cp;

	if (*s == '\0')
		return FALSE;

	init_allowed();

	for (cp = (const unsigned char *) s; *cp; cp++)
		if (!atom_chars_allowed[*cp & 0xff])
			return FALSE;
	return TRUE;
}

// validate conformance to RFC 3501 "ASTRING-CHAR"
bool imap_url_astring_chars_validate(const char *s)
{
	const unsigned char *cp;

	init_allowed();

	for (cp = (const unsigned char *) s; *cp; cp++)
		if (!astring_chars_allowed[*cp & 0xff])
			return FALSE;
	return TRUE;
}

// validate conformance to RFC 3501 "quoted"
bool imap_url_quoted_validate(const char *s)
{
	const unsigned char *cp;

	if (*s != '"')
		return FALSE;

	init_allowed();

	for (cp = (unsigned char *) s + 1; *cp; cp++) {
		if (*cp == '\\') {
			if (!quoted_specials_allowed[cp[1] & 0xff])
				return FALSE;
			++cp;
		} else if (*cp == '"')
			return cp[1] == '\0';
		else {
			if (!quoted_allowed[*cp & 0xff])
				return FALSE;
		}
	}

	return FALSE;
}

// validate conformance to RFC 3501 "literal"
bool imap_url_literal_validate(const char *s)
{
	const unsigned char *cp;
	unsigned int length = 0;

	if (*s != '{')
		return FALSE;
	for (cp = (const unsigned char *) s + 1; *cp && *cp != '}'; cp++) {
		if (*cp < '0' || *cp > '9')
			return FALSE;
		length = length * 10 + *cp - '0';
	}
	if (cp == (const unsigned char *) s + 1 || *cp != '}')
		return FALSE;
	++cp;
	if (*cp == '\r')
		++cp;
	if (*cp != '\n')
		return FALSE;
	if (strlen((const char *) cp + 1) != length)
		return FALSE;
	return TRUE;
}

// validate conformance to RFC 3501 "astring"
bool imap_url_astring_validate(const char *s)
{
	return *s != '\0' &&
		(imap_url_astring_chars_validate(s) ||
		 imap_url_quoted_validate(s) ||
		 imap_url_literal_validate(s));
}

// validate conformance to RFC 1738 "hostport"
bool imap_url_hostport_validate(const char *s)
{
	const unsigned char *cp;

	if (*s == '\0')
		return FALSE;

	init_allowed();

	for (cp = (const unsigned char *) s; *cp; cp++)
		if (!host_chars_allowed[*cp & 0xff])
			return FALSE;
	return TRUE;
}

// validate conformance to RFC 3501 "mailbox"
bool imap_url_mailbox_validate(const char *s)
{
	const char *qs;

	if (strcasecmp(s, "INBOX") == 0 || imap_url_astring_validate(s))
		return TRUE;

	// RFC 2192 implies quotes around mailboxes containing, e.g., SP
	qs = t_strconcat("\"", s, "\"", NULL);
	return imap_url_quoted_validate(qs);
}

// validate conformance to RFC 3501 "nz_number"
bool imap_url_nz_number_validate(const char *s)
{
	if (*s < '1' || *s > '9')
		return FALSE;
	while (*++s)
		if (*s < '0' || *s > '9')
			return FALSE;
	return TRUE;
}

// validate conformance to RFC 3501 "section-text"
bool imap_url_section_text_validate(const char *s)
{
	size_t spn;
	const char *field;

	if (strcasecmp(s, "HEADER") == 0 ||
	    strcasecmp(s, "TEXT") == 0 ||
	    strcasecmp(s, "MIME") == 0)
		return TRUE;

	if (strncasecmp(s, "HEADER.FIELDS", 13) != 0)
		return FALSE;
	s += 13;

	if (strncasecmp(s, ".NOT", 4) == 0)
		s += 4;

	if (*s != ' ')
		return FALSE;
	++s;

	if (*s != '(')
		return FALSE;
	++s;

	spn = strcspn(s, " )");
	if (spn <= 0)
		return FALSE;
	field = t_strdup_until(s, s + spn);
	if (!imap_url_astring_validate(field))
		return FALSE;
	s += spn;

	while (*s == ' ') {
		++s;
		spn = strcspn(s, " )");
		if (spn <= 0)
			return FALSE;
		field = t_strdup_until(s, s + spn);
		if (!imap_url_astring_validate(field))
			return FALSE;
		s += spn;
	}

	if (*s != ')')
		return FALSE;
	++s;

	return *s == '\0';
}

// validate conformance to RFC 2192 "section"
bool imap_url_section_validate(const char *s)
{
	size_t spn;
	const char *part;

	if (imap_url_section_text_validate(s))
		return TRUE;

	spn = strspn(s, DIGIT);
	if (spn <= 0)
		return FALSE;
	part = t_strdup_until(s, s + spn);
	if (!imap_url_nz_number_validate(part))
		return FALSE;
	s += spn;

	while (*s == '.') {
		++s;
		spn = strspn(s, DIGIT);
		if (spn <= 0) {
			--s;
			break;
		}
		part = t_strdup_until(s, s + spn);
		if (!imap_url_nz_number_validate(part))
			return FALSE;
		s += spn;
	}

	if (*s == '.') {
		++s;
		return imap_url_section_text_validate(s);
	}

	return *s == '\0';
}

// validate conformance to RFC 3339 "date-time"
bool imap_url_datetime_validate(const char *s)
{
	return *s != '\0' && strspn(s, DATETIME_CHARS) == strlen(s);
}

// validate conformance to RFC 4467 "access"
bool imap_url_access_validate(const char *s)
{
	if (strncasecmp(s, "submit+", 7) == 0)
		return imap_url_astring_validate(s + 7);
	if (strncasecmp(s, "user+", 5) == 0)
		return imap_url_astring_validate(s + 5);
	return strcasecmp(s, "authuser") == 0 ||
	       strcasecmp(s, "anonymous") == 0;
}

// validate conformance to RFC 4467 "mechanism"
bool imap_url_mechanism_validate(const char *s)
{
#if MECH_INTERNAL_ONLY
	return strcasecmp(s, "INTERNAL") == 0;
#else
	if (strcasecmp(s, "INTERNAL") == 0)
		return TRUE;
	if (*s == '\0')
		return FALSE;
	do {
		if ((*s < 'A' || *s > 'Z') &&
		    (*s < 'a' || *s > 'z') &&
		    (*s < '0' || *s > '9') &&
		    *s != '-' && *s != '.')
			return FALSE;
	} while (*++s);
	return TRUE;
#endif
}

// validate conformance to RFC 4467 "urlauth" (really enc-urlauth)
bool imap_url_urlauth_validate(const char *s)
{
	const char *cp;

	for (cp = s; *cp; cp++) {
		if ((*cp < '0' || *cp > '9') &&
		    (*cp < 'A' || *cp > 'F') &&
		    (*cp < 'a' || *cp > 'f'))
			return FALSE;
	}

	return cp - s >= 32;
}

// convert URL parts into a real URL
void imap_url_construct(const struct imap_url_parts *parts, string_t *url)
{
	str_append(url, "imap://");
	if (parts->user)
		str_append(url, parts->user);
	if (parts->auth_type) {
		str_append(url, ";AUTH=");
		str_append(url, parts->auth_type);
	}
	if (parts->hostport) {
		str_append_c(url, '@');
		str_append(url, parts->hostport);
	}
	str_append_c(url, '/');
	if (parts->mailbox)
		str_append(url, parts->mailbox);
	if (parts->uidvalidity) {
		str_append(url, ";UIDVALIDITY=");
		str_append(url, parts->uidvalidity);
	}
	if (parts->uid) {
		str_append(url, "/;UID=");
		str_append(url, parts->uid);
	}
	if (parts->section) {
		str_append(url, "/;SECTION=");
		str_append(url, parts->section);
	}
	if (parts->expiration) {
		str_append(url, ";EXPIRE=");
		str_append(url, parts->expiration);
	}
	if (parts->access) {
		str_append(url, ";URLAUTH=");
		str_append(url, parts->access);
	}
	if (parts->mechanism) {
		str_append_c(url, ':');
		str_append(url, parts->mechanism);
	}
	if (parts->urlauth) {
		str_append_c(url, ':');
		str_append(url, parts->urlauth);
	}
}

// filter a URL according to RFC 4469 url-resp-text
const char *imap_url_sanitize(const char *url)
{
	const char *cp;
	string_t *str;

	init_allowed();

	for (cp = url; *cp && url_resp_allowed[(unsigned char) *cp]; cp++)
		;
	if (*cp == '\0')
		return url;

	str = t_str_new(strlen(cp) + 1);
	str_append_n(str, url, cp - url);
	for ( ; *cp; cp++)
		str_append_c(str,
			url_resp_allowed[(unsigned char) *cp] ? *cp : '?');
	return str_c(str);
}
