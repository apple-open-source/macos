/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <sys/param.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "webdav_cookie.h"
#include "webdav_utils.h"

// **************
// Manage cookies
// **************
int lock_cookies(void);
int unlock_cookies(void);
void add_cookie(WEBDAV_COOKIE *newCookie);
boolean_t removeMatchingCookie(WEBDAV_COOKIE *aCookie);
void list_insert_cookie(WEBDAV_COOKIE *newCookie);
void list_remove_cookie(WEBDAV_COOKIE *aCookie);
WEBDAV_COOKIE *dequeueCookie(void);
WEBDAV_COOKIE *find_cookie(WEBDAV_COOKIE *aCookie);
boolean_t cookies_match(WEBDAV_COOKIE *cookie1, WEBDAV_COOKIE *cookie2);
boolean_t checkCookieExpired(WEBDAV_COOKIE *aCookie);

// *************
// Parse cookies
// *************
typedef enum token_result{
    COOKIE_EOF = 0,         // reached EOF
    COOKIE_PARSE_ERR = 1,   // unexpected parse error
    COOKIE_NAME = 2,        // name as in "name = value" (delimited by '=')
    COOKIE_VALUE = 3,       // value, as in "name = value" (delimited by ';' or '"')
    COOKIE_DELIMITER = 4    // end of current cookie, found ',' as in "cookie=name; attr=value, cookie2=name2"
} TOKEN_RESULT;

typedef enum cookie_parse_state {
    ST_COOKIE_NAME = 0,
    ST_COOKIE_VAL = 1,
    ST_ATTR_NAME = 2,
    ST_PATH_VAL = 3,
    ST_DOMAIN_VAL = 4,
    ST_EXPIRES_VAL = 5,
    ST_MAXAGE_VAL = 6
} PARSE_COOKIE_STATE;

#define KEEP_QUOTED 0

WEBDAV_COOKIE *nextCookieFromHeader(CFStringRef cookieHeader, CFIndex headerLen, CFIndex startPosition, CFIndex *nextPosition);
CFStringRef nextToken(CFStringRef str, CFIndex strLen, CFIndex startPos, TOKEN_RESULT *result, CFIndex *nextPosition);
boolean_t isWeekday(CFStringRef str, CFIndex strLen, CFIndex position);
void skipWhiteSpace(CFStringRef str, CFIndex strLen, CFIndex *position);
void skipWhiteSpaceReverse(CFStringRef str, CFIndex startPosition, CFIndex *position);
boolean_t findNextSeparator(CFStringRef str, CFIndex strLen, CFIndex startPos, CFIndex *foundAt, UniChar *ch);
boolean_t findCharacter(CFStringRef str, CFIndex strLen, CFIndex startPos, CFIndex *foundAt, UniChar ch);
CFStringRef cleanDomainName(CFStringRef inStr);
CFStringRef nextDomainComponent(CFStringRef inStr, CFIndex strLen, CFIndex startPos, CFIndex *nextStartPos);
boolean_t isLetterDigitHyphen(UniChar ch);

// helpers
boolean_t path2InPath1(const char *path1, const char *path2);
void free_cookie_fields(WEBDAV_COOKIE *cookie);
void printCookie(WEBDAV_COOKIE *aCookie);
CFStringRef cookiePathFromURL(CFURLRef url);
boolean_t doesDomainMatch(CFStringRef domainStr, CFStringRef aStr);
CFStringRef cleanDomainName(CFStringRef inStr);
boolean_t is_ip_address_str(CFStringRef hostStr);
boolean_t is_ip_address(const char *host);

// global array of cookies
#define WEBDAV_MAX_COOKIES 10

WEBDAV_COOKIE *cookie_head, *cookie_tail;
uint32_t cookie_count;
pthread_mutex_t cookie_lock;

extern int gSecureConnection;
extern CFURLRef gBaseURL;				/* the base URL for this mount */
extern CFStringRef gBasePath;			/* the base path (from gBaseURL) for this mount */
extern char gBasePathStr[MAXPATHLEN];	/* gBasePath as a c-string */

// main entry point for incoming cookie
void handle_cookies(CFStringRef str, CFHTTPMessageRef message)
{
	WEBDAV_COOKIE *aCookie;
	CFURLRef url;
	CFStringRef urlPathStr, domainStr, tmpStr;
	CFIndex pos1, pos2, len;
	boolean_t done;

	urlPathStr = NULL;
	url = NULL;

	if ( str == NULL ) {
		goto err_out;
	}

	len = CFStringGetLength(str);
	if (!len) {
		goto err_out;
	}

	done= false;
	pos1 = 0;
	while (done == false) {
		aCookie = nextCookieFromHeader(str, len, pos1, &pos2);
		if (aCookie == NULL) {
			// all done
			goto err_out;
		}

		// ******************
		// 1. Cookie Expired?
		// ******************
		if (checkCookieExpired(aCookie) == true) {
			syslog(LOG_DEBUG, "%s: COOKIE NOT ACCEPTED %s=%s, expired\n",
				   __FUNCTION__, aCookie->cookie_name_str, aCookie->cookie_val_str);

			// Delete existing cookie if we have it
			lock_cookies();
			removeMatchingCookie(aCookie);
			unlock_cookies();

			// Free this expired cookie
			free_cookie_fields(aCookie);
			free (aCookie);

			// on to next cookie
			goto skip_cookie;
		}

		// *************************
		// 2. Check Secure attribute
		// *************************
		if ((aCookie->cookie_secure == true) && (gSecureConnection == false)) {
			syslog(LOG_DEBUG, "%s: COOKIE NOT ACCEPTED %s=%s, requires secure connection\n", __FUNCTION__,
				   aCookie->cookie_name_str, aCookie->cookie_val_str);

			// Free this expired cookie
			free_cookie_fields(aCookie);
			free (aCookie);

			// on to next cookie
			goto skip_cookie;
		}

		// *************************
		// 3. Check domain attribute
		// *************************
		url = CFHTTPMessageCopyRequestURL(message);
		if (url == NULL) {
			// nothing we can do, forget this cookie
			free_cookie_fields(aCookie);
			free(aCookie);

			// on to the next cookie
			goto skip_cookie;
		}

		tmpStr = CFURLCopyHostName(url);
		CFRelease(url);

		if (tmpStr == NULL) {
			// nothing we can do, forget this cookie
			free_cookie_fields(aCookie);
			free(aCookie);

			// on to the next cookie
			goto skip_cookie;
		}

		// clean domain str
		domainStr = cleanDomainName(tmpStr);

		if (domainStr == NULL) {
			syslog(LOG_DEBUG, "%s: COOKIE NOT ACCEPTED %s=%s, cleanDomainName error\n", __FUNCTION__,
				   aCookie->cookie_name_str, aCookie->cookie_val_str);

			// nothing we can do, forget this cookie
			free_cookie_fields(aCookie);
			free(aCookie);

			// on to the next cookie
			goto skip_cookie;
		}

		if ( (is_ip_address_str(domainStr) == true) || (aCookie->cookie_domain == NULL)) {
			// URL has an ip address instead of hostname,
			// have to set cookie domain to URL ip address
			if (aCookie->cookie_domain != NULL) {
				CFRelease(aCookie->cookie_domain);
				aCookie->cookie_domain = NULL;
			}
			if (aCookie->cookie_domain_str != NULL) {
				free (aCookie->cookie_domain_str);
				aCookie->cookie_domain_str = NULL;
			}

			aCookie->cookie_domain = domainStr;
			aCookie->cookie_domain_str = createUTF8CStringFromCFString(domainStr);
			if (aCookie->cookie_domain_str == NULL) {
				// have to punt
				free_cookie_fields(aCookie);
				free(aCookie);

				// on to the next cookie
				goto skip_cookie;
			}
		}
		else if (doesDomainMatch(aCookie->cookie_domain, domainStr) == false) {
				// This cookie will never be sent out, because the cookie domain
				// does not "domain match" domain of hostname
				syslog(LOG_DEBUG, "%s: cookie domain mismatch %s=%s, cookie domain: %s, host: %s\n", __FUNCTION__,
					   aCookie->cookie_name_str, aCookie->cookie_val_str, aCookie->cookie_domain_str, gBasePathStr);

				// have to punt
				free_cookie_fields(aCookie);
				free(aCookie);

				// on to the next cookie
				goto skip_cookie;
		}

		// ***********************
		// 4. Check path attribute
		// ***********************
		if (aCookie->cookie_path == NULL) {
			// Assign path from request url as path attribute for this cookie

			if (message == NULL) {
				// nothing we can do, forget this cookie
				free_cookie_fields(aCookie);
				free(aCookie);

				// on to the next cookie
				goto skip_cookie;
			}

			url = CFHTTPMessageCopyRequestURL(message);

			if (url == NULL) {
				// nothing we can do, forget this cookie
				free_cookie_fields(aCookie);
				free(aCookie);

				// on to the next cookie
				goto skip_cookie;
			}

			urlPathStr = cookiePathFromURL(url);
			CFRelease(url);

			if (urlPathStr == NULL) {
				// nothing we can do, forget this cookie
				free_cookie_fields(aCookie);
				free(aCookie);

				// on to the next cookie
				goto skip_cookie;
			}

			aCookie->cookie_path = urlPathStr;
			aCookie->cookie_path_str = createUTF8CStringFromCFString(urlPathStr);
		}
		else {
			if ((path2InPath1(gBasePathStr, aCookie->cookie_path_str) != true) &&
				(path2InPath1(aCookie->cookie_path_str, gBasePathStr) != true)) {
				syslog(LOG_DEBUG, "%s: COOKIE NOT ACCEPTED %s=%s, path mismatch: gBasePath: %s, cookie_path: %s\n",
					   __FUNCTION__, aCookie->cookie_name_str, aCookie->cookie_val_str, gBasePathStr, aCookie->cookie_path_str);
				// path attribute is not a subpath of gBaseURL path, we will never send
				// it out, so forget this cookie
				free_cookie_fields(aCookie);
				free(aCookie);

				// on to the next cookie
				goto skip_cookie;
			}
		}
		// add the new cookie
		add_cookie(aCookie);
skip_cookie:
		pos1 = pos2;
	}
err_out:
	return;
}

void add_cookie_headers(CFHTTPMessageRef message, CFURLRef url)
{
	CFStringRef urlPathStr;
	CFMutableStringRef cookieStr;
	WEBDAV_COOKIE *aCookie;
	char *cpath;

	cpath = NULL;
	cookieStr = NULL;
	urlPathStr = NULL;

	if (!cookie_count) {
		goto err_out;
	}

	urlPathStr = cookiePathFromURL(url);
	if (urlPathStr == NULL) {
		syslog(LOG_DEBUG, "%s: no path from urlPathStr\n", __FUNCTION__);
		goto err_out;
	}

	cpath = createUTF8CStringFromCFString(urlPathStr);
	if (cpath == NULL) {
		goto err_out;
	}

	lock_cookies();
	aCookie = cookie_head;
	while (aCookie != NULL) {
		if  ((aCookie->cookie_secure == true) && (gSecureConnection != true)) {
			// Don't have a secure connection, and this cookie requires one
			syslog(LOG_DEBUG, "%s: NO MATCH cookie: %s=%s requires secure connection\n", __FUNCTION__,
				   aCookie->cookie_name_str, aCookie->cookie_val_str);
			goto skip_cookie;
		}

		if (path2InPath1(aCookie->cookie_path_str, cpath) == true) {
			// add this cookie to outgoing message
			if (cookieStr == NULL) {
				cookieStr = CFStringCreateMutable(kCFAllocatorDefault, 0);
				if (cookieStr == NULL) {
					unlock_cookies();
					goto err_out;
				}

				CFStringAppend(cookieStr, aCookie->cookie_header);
			} else {
				CFStringAppend(cookieStr, CFSTR("; "));
				CFStringAppend(cookieStr, aCookie->cookie_header);
			}
		}
		else {
			syslog(LOG_DEBUG, "%s: cookie: %s=%s, Failed path-match, cookie_path: %s url path: %s\n", __FUNCTION__,
				   aCookie->cookie_name_str, aCookie->cookie_val_str, aCookie->cookie_path_str, cpath);
		}
skip_cookie:
		aCookie = aCookie->next;
	}
	unlock_cookies();

	if (cookieStr != NULL) {
		CFHTTPMessageSetHeaderFieldValue(message, CFSTR("Cookie"), cookieStr);
	}

err_out:
	if (urlPathStr != NULL)
		CFRelease(urlPathStr);
	if (cookieStr != NULL)
		CFRelease(cookieStr);
	if (cpath != NULL)
		free (cpath);
	return;
}

void purge_expired_cookies(void)
{
	WEBDAV_COOKIE *aCookie, *nextCookie;
	time_t now;

	lock_cookies();
	if (cookie_head == 0) {
		// nothing to purge
		unlock_cookies();
		return;
	}

	now = time(NULL);
	aCookie = cookie_head;
	while (aCookie != NULL) {
		nextCookie = aCookie->next;
		if (aCookie->has_expire_time == true) {
			if (now >= aCookie->cookie_expire_time) {
				// this cookie is expired
				list_remove_cookie(aCookie);
				free_cookie_fields(aCookie);
				free(aCookie);
			}
		}
		aCookie = nextCookie;
	}

	unlock_cookies();
}

void add_cookie(WEBDAV_COOKIE *newCookie)
{
	// Remove any matching cookie
	lock_cookies();
	removeMatchingCookie(newCookie);
	list_insert_cookie(newCookie);
	unlock_cookies();
}

boolean_t removeMatchingCookie(WEBDAV_COOKIE *aCookie)
{
	WEBDAV_COOKIE *anotherCookie;
	boolean_t didRemove;

	didRemove = false;

	if (cookie_head == 0)
		goto out;

	anotherCookie = find_cookie(aCookie);

	if (anotherCookie != NULL) {
		list_remove_cookie(anotherCookie);
		free_cookie_fields(anotherCookie);
		free(anotherCookie);
		didRemove = true;
	}
out:
	return (didRemove);
}

void list_remove_cookie(WEBDAV_COOKIE *aCookie)
{
	if (aCookie->prev == NULL) {
		// head position
		cookie_head = aCookie->next;

		if (cookie_head == NULL) {
			cookie_tail = NULL;
			cookie_count = 0;
		}
		else {
			cookie_head->prev = NULL;
			cookie_count--;
		}
	}
	else if (aCookie->next == NULL) {
		// tail position
		cookie_tail = aCookie->prev;
		cookie_tail->next = NULL;
		cookie_count--;
	}
	else {
		// somewhere in the middle
		aCookie->prev->next = aCookie->next;
		aCookie->next->prev = aCookie->prev;
		cookie_count--;
	}
}
void list_insert_cookie(WEBDAV_COOKIE *newCookie)
{
	WEBDAV_COOKIE *aCookie;

	if (cookie_count >= WEBDAV_MAX_COOKIES) {
		// Remove the oldest cookie
		aCookie = dequeueCookie();
		if (aCookie != NULL) {
			free_cookie_fields(aCookie);
			free(aCookie);
		}
	}

	if (cookie_head == NULL) {
		// empty list
		cookie_head = newCookie;
		cookie_tail = newCookie;
		newCookie->prev = NULL;
		newCookie->next = NULL;
	} else if (cookie_head == cookie_tail) {
		// only one in list
		cookie_head->next = newCookie;
		cookie_tail = newCookie;
		newCookie->prev = cookie_head;
	} else {
		// more than one in list
		cookie_tail->next = newCookie;
		newCookie->prev = cookie_tail;
		cookie_tail = newCookie;
	}

	cookie_count++;

	return;
}

WEBDAV_COOKIE *dequeueCookie(void)
{
	WEBDAV_COOKIE *aCookie = NULL;

	if (cookie_head == NULL) {
		// empty
		cookie_count = 0;
		goto out;
	}

	if (cookie_head == cookie_tail) {
		// only one in list
		aCookie = cookie_head;
		cookie_head = NULL;
		cookie_tail = NULL;
		cookie_count = 0;
		goto out;
	}

	// more than one in list
	aCookie = cookie_tail;
	cookie_tail = aCookie->prev;
	cookie_tail->next = NULL;
	if (cookie_count)
		cookie_count--;
out:
	return (aCookie);
}

WEBDAV_COOKIE *find_cookie(WEBDAV_COOKIE *aCookie)
{
	WEBDAV_COOKIE *someCookie, *matchCookie;

	matchCookie = NULL;

	someCookie = cookie_head;

	while (someCookie != NULL) {
		if (cookies_match(aCookie, someCookie) == true) {
			matchCookie = someCookie;
			break;
		}

		someCookie = someCookie->next;
	}

	return (matchCookie);
}

boolean_t cookies_match(WEBDAV_COOKIE *cookie1, WEBDAV_COOKIE *cookie2)
{
	size_t len1, len2;
	boolean_t matched = false;

	if (cookie1 == NULL || cookie2 == NULL) {
		goto out;
	}

	// (1) Check cookie names
	if (cookie1->cookie_name_str == NULL || cookie2->cookie_name_str == NULL) {
		goto out;
	}

	len1 = strlen(cookie1->cookie_name_str);
	len2 = strlen(cookie2->cookie_name_str);

	if (len1 != len2) {
		goto out;
	}

	if(strncmp(cookie1->cookie_name_str, cookie1->cookie_name_str, len1) != 0) {
		goto out;
	}

	// (2) Check cookie paths
	if (cookie1->cookie_path_str != NULL) {
		if (cookie2->cookie_path_str == NULL){
			goto out;
		}

		len1 = strlen(cookie1->cookie_path_str);
		len2 = strlen(cookie2->cookie_path_str);

		if (len1 != len2) {
			goto out;
		}

		if (strncmp(cookie1->cookie_path_str, cookie2->cookie_path_str, len1) != 0) {
			goto out;
		}
	}
	else {
		if (cookie2->cookie_path_str != NULL) {
			goto out;
		}
	}

	// (3) Check cookie domains
	if (cookie1->cookie_domain_str != NULL) {
		if (cookie2->cookie_domain_str == NULL){
			goto out;
		}

		len1 = strlen(cookie1->cookie_domain_str);
		len2 = strlen(cookie2->cookie_domain_str);

		if (len1 != len2) {
			goto out;
		}

		if (strncmp(cookie1->cookie_domain_str, cookie2->cookie_domain_str, len1) != 0) {
			goto out;
		}
	}
	else {
		if (cookie2->cookie_domain_str != NULL) {
			goto out;
		}
	}

	// If we made it this far, they match
	matched = true;

out:
	return (matched);
}

boolean_t checkCookieExpired(WEBDAV_COOKIE *aCookie)
{
	boolean_t hasExpired;
	time_t now;

	hasExpired = false;
	now = time(NULL);

	if (aCookie->has_expire_time != true) {
		goto out;
	}

	if ( (aCookie->cookie_expire_time == 0) || (aCookie->cookie_expire_time < now)) {
		hasExpired = true;
	}

out:
	return (hasExpired);
}


// ***********************
// *** PARSING COOKIES ***
// ***********************

WEBDAV_COOKIE *nextCookieFromHeader(CFStringRef cookieHeader, CFIndex headerLen, CFIndex startPosition, CFIndex *nextPosition)
{
    WEBDAV_COOKIE *nextCookie;
    CFStringRef tokenStr, tmpStr;
    CFIndex pos1, pos2, len;
	CFRange range;
    TOKEN_RESULT res;
    PARSE_COOKIE_STATE st;
    char *str;
    boolean_t done;

    nextCookie = NULL;
    st = ST_COOKIE_NAME;

    if (startPosition >= headerLen) {
        goto err_out;
    }

    nextCookie = malloc(sizeof(WEBDAV_COOKIE));
    if (nextCookie == NULL) {
        goto err_out;
    }
    bzero(nextCookie, sizeof(WEBDAV_COOKIE));

    pos1 = startPosition;
    done = false;
    while (done == false) {
		tokenStr = nextToken(cookieHeader, headerLen, pos1, &res, &pos2);
        switch (res) {
            case COOKIE_NAME:
                switch (st) {
                    case ST_COOKIE_NAME:
                        nextCookie->cookie_name = tokenStr;
                        nextCookie->cookie_name_str = createUTF8CStringFromCFString(tokenStr);
                        st = ST_COOKIE_VAL;
                        break;
                    case ST_COOKIE_VAL:
                        // Shouldn't happen
                        CFRelease(tokenStr);
                        goto err_out;
                        break;

                    case ST_ATTR_NAME:
                        if (CFStringCompare(tokenStr, CFSTR("Path"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                            // Path?
                            nextCookie->cookie_path = tokenStr;
                            nextCookie->cookie_path_str = createUTF8CStringFromCFString(tokenStr);
                            st = ST_PATH_VAL;
                        }
                        else if (CFStringCompare(tokenStr, CFSTR("Domain"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                            // Domain
                            nextCookie->cookie_domain = tokenStr;
                            nextCookie->cookie_domain_str = createUTF8CStringFromCFString(tokenStr);
                            st = ST_DOMAIN_VAL;
                        }
                        else if (CFStringCompare(tokenStr, CFSTR("Expires"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                            // Expires
                            st = ST_EXPIRES_VAL;
                        }
                        else if (CFStringCompare(tokenStr, CFSTR("Max-Age"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                            // MaxAge
                            st = ST_MAXAGE_VAL;
                        }
                        else if (CFStringCompare(tokenStr, CFSTR("Secure"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                            // Secure
                            nextCookie->cookie_secure = true;
                            CFRelease(tokenStr);
                        }
                        else if (CFStringCompare(tokenStr, CFSTR("HttpOnly"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                            // HttpOnly
                            nextCookie->cookie_httponly = true;
                            CFRelease(tokenStr);
                        }
                        else {
                            CFRelease(tokenStr);
                        }
                        break;
                    case ST_PATH_VAL:
                        CFRelease(tokenStr);
                        goto err_out;
                        break;
                    case ST_DOMAIN_VAL:

                        break;
                    case ST_EXPIRES_VAL:
                        CFRelease(tokenStr);
                        goto err_out;
                        break;
                    case ST_MAXAGE_VAL:
                        CFRelease(tokenStr);
                        goto err_out;
                        break;
                }
                break;
            case COOKIE_VALUE:
                switch (st) {
                    case ST_COOKIE_NAME:
                        // Shouldn't happen
                        CFRelease(tokenStr);
                        goto err_out;
                        break;
                    case ST_COOKIE_VAL:
                        nextCookie->cookie_val = tokenStr;
                        nextCookie->cookie_val_str = createUTF8CStringFromCFString(tokenStr);
                        st = ST_ATTR_NAME;
                        break;
                    case ST_ATTR_NAME:
                        if (CFStringCompare(tokenStr, CFSTR("Secure"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                            // Secure
                            nextCookie->cookie_secure = true;
                            CFRelease(tokenStr);
                        }
                        else if (CFStringCompare(tokenStr, CFSTR("HttpOnly"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
                            // HttpOnly
                            nextCookie->cookie_httponly = true;
                            CFRelease(tokenStr);
                        }
                        else {
                            CFRelease(tokenStr);
                        }
                        break;
                    case ST_PATH_VAL:
						len = CFStringGetLength(tokenStr);
                        if ( (CFStringHasSuffix(tokenStr, CFSTR("/")) == true) && len > 1) {
                            // remove trailing slash
                            range.length = len - 1;
                            range.location = 0;
                            tmpStr = CFStringCreateWithSubstring(kCFAllocatorDefault, tokenStr, range);
                            CFRelease(tokenStr);
                            tokenStr = tmpStr;
                        }
                        nextCookie->cookie_path = tokenStr;
                        nextCookie->cookie_path_str = createUTF8CStringFromCFString(tokenStr);
                        st = ST_ATTR_NAME;
                        break;
                    case ST_DOMAIN_VAL:
						// clean it up (convert to lower case, etc...)
						nextCookie->cookie_domain = cleanDomainName(tokenStr);
						if (nextCookie->cookie_domain != NULL) {
							nextCookie->cookie_domain_str = createUTF8CStringFromCFString(nextCookie->cookie_domain);
						}
                        CFRelease(tokenStr);
                        st = ST_ATTR_NAME;
                        break;
                    case ST_EXPIRES_VAL:
                        nextCookie->has_expire_time = true;
                        nextCookie->cookie_expire_time = DateStringToTime(tokenStr);
                        st = ST_ATTR_NAME;
                        break;
                    case ST_MAXAGE_VAL:
                        nextCookie->has_expire_time = true;
                        str = createUTF8CStringFromCFString(tokenStr);
                        nextCookie->cookie_expire_time = time(NULL) + strtol(str, NULL, 10);
                        st = ST_ATTR_NAME;
                        break;
                }
                break;

            case COOKIE_DELIMITER:
                done = true;
                break;

            case COOKIE_PARSE_ERR:
                goto err_out;
                break;

            case COOKIE_EOF:
                done = true;
                break;
        }
        pos1 = pos2;
    }

    // Make sure we received a cookie name
    if (nextCookie->cookie_name == NULL) {
        goto err_out;
    }

	// Fix the path if needed
	if (nextCookie->cookie_path_str != NULL) {
		if (nextCookie->cookie_path_str[0] != '/') {
			// no beginning slash, use default path
			free(nextCookie->cookie_path_str);
			if (nextCookie->cookie_path != NULL)
				CFRelease(nextCookie->cookie_path);
			nextCookie->cookie_path_str = NULL;
			nextCookie->cookie_path = NULL;
		}
	}

	// Fix domain if needed
	if (nextCookie->cookie_domain != NULL) {
		if (is_ip_address_str(nextCookie->cookie_domain) == true) {
			// Cannot accept ip address as a domain
			CFRelease(nextCookie->cookie_domain);
			nextCookie->cookie_domain = NULL;
			if (nextCookie->cookie_domain_str != NULL) {
				free (nextCookie->cookie_domain_str);
				nextCookie->cookie_domain_str = NULL;
			}
		}
	}

	// Compose the cookie header (for outgoing messages)
	nextCookie->cookie_header = CFStringCreateMutable(kCFAllocatorDefault, 0);

	if (nextCookie->cookie_header == NULL) {
		goto err_out;
	}

	CFStringAppend(nextCookie->cookie_header, nextCookie->cookie_name);

	if (nextCookie->cookie_val != NULL) {
		CFStringAppend(nextCookie->cookie_header, CFSTR("="));
		CFStringAppend(nextCookie->cookie_header, nextCookie->cookie_val);
	}

    *nextPosition = pos2;
    return (nextCookie);

err_out:
    if (nextCookie) {
        free_cookie_fields(nextCookie);
        free (nextCookie);
    }
    return (NULL);
}

CFStringRef nextToken(CFStringRef str, CFIndex strLen, CFIndex startPos, TOKEN_RESULT *result, CFIndex *nextPosition)
{
    CFStringRef token;
    CFIndex pos1, pos2, pos3;
    CFRange range, range2;
    UniChar ch, ch2;
    boolean_t found;

    token = NULL;
    *result = COOKIE_PARSE_ERR;

    if (startPos >= strLen) {
        *result = COOKIE_EOF;
        goto out;
    }

    // skip any white space
    pos1 = startPos;
    skipWhiteSpace(str, strLen, &pos1);
    if (pos1 >= strLen) {
        *result = COOKIE_EOF;
        goto out;
    }

    // Find the next separator
    found = findNextSeparator(str, strLen, pos1, &pos2, &ch);

    if (found == false) {
        // Check for ending name or value
        range.location = pos1;

        // trim any trailing whitespace
        skipWhiteSpaceReverse(str, pos2-1, &pos3);
        pos3++;

        if (pos3 > pos1) {
            range.length = pos3 - pos1;

            token = CFStringCreateWithSubstring(kCFAllocatorDefault, str, range);
            *result = COOKIE_VALUE;
        }
        else {
            *result = COOKIE_EOF;
        }
        *nextPosition = pos2;
        goto out;
    }

    //  token = value
    //  ^     ^
    //  |     |
    // pos1  pos2
    //
    if (ch == '=') {
        // Handle quoted string
        ch2 = CFStringGetCharacterAtIndex(str, pos1);
        if (ch2 == '"') {
            if (KEEP_QUOTED) {
                found = findCharacter(str, strLen, pos1 + 1, &pos3, '"');
            } else {
                pos1++;
                found = findCharacter(str, strLen, pos1, &pos3, '"');
            }

            // Did we get the ending quote?
            if (found == false) {
                // parser is confused
                *result = COOKIE_PARSE_ERR;
                goto out;
            }

            range.location = pos1;
            range.length = pos3 - pos1;

            if (KEEP_QUOTED)
                range.length++;

            token = CFStringCreateWithSubstring(kCFAllocatorDefault, str, range);
            if (token == NULL) {
                *result = COOKIE_PARSE_ERR;
                goto out;
            }

            // skip over '='
            *nextPosition = pos2 + 1;
            *result = COOKIE_NAME;
            goto out;
        }

        // Name is not quoted
        range.location = pos1;

        // trim any trailing white
        skipWhiteSpaceReverse(str, pos2-1, &pos3);
        pos3++;
        range.length = pos3 - pos1;

        token = CFStringCreateWithSubstring(kCFAllocatorDefault, str, range);
        if (token == NULL) {
            *result = COOKIE_PARSE_ERR;
            goto out;
        }

        *nextPosition = pos2 + 1;
        *result = COOKIE_NAME;
        goto out;
    }

    // token = value;
    //         ^    ^
    //         |    |
    //        pos1 pos2
    //
    if (ch == ';') {
        // handle quoted value
        ch2 = CFStringGetCharacterAtIndex(str, pos1);
        if (ch2 == '"') {
            if (KEEP_QUOTED) {
                found = findCharacter(str, strLen, pos1 + 1, &pos3, '"');
            } else {
                pos1++;
                found = findCharacter(str, strLen, pos1, &pos3, '"');
            }

            // Did we get the ending quote?
            if (found == false) {
                // parser is confused
                *result = COOKIE_PARSE_ERR;
                goto out;
            }

            range.location = pos1;
            range.length = pos3 - pos1;

            if (KEEP_QUOTED)
                range.length++;

            token = CFStringCreateWithSubstring(kCFAllocatorDefault, str, range);
            if (token == NULL) {
                *result = COOKIE_PARSE_ERR;
                goto out;
            }

            // skip over '='
            *nextPosition = pos2 + 1;
            *result = COOKIE_VALUE;
            goto out;
        }

        // Value is not quoted
        range.location = pos1;

        // trim any trailing white
        skipWhiteSpaceReverse(str, pos2-1, &pos3);
        pos3++;
        range.length = pos3 - pos1;

        token = CFStringCreateWithSubstring(kCFAllocatorDefault, str, range);

        if (token == NULL) {
            *result = COOKIE_PARSE_ERR;
            goto out;
        }

        *nextPosition = pos2 + 1;
        *result = COOKIE_VALUE;
        goto out;
    }

    if (ch == ',') {
        // Make sure we haven't found a comma in a quoted string
        ch2 = CFStringGetCharacterAtIndex(str, pos1);
        if (ch2 == '"') {
            if (KEEP_QUOTED) {
                found = findCharacter(str, strLen, pos1 + 1, &pos3, '"');
            } else {
                pos1++;
                found = findCharacter(str, strLen, pos1, &pos3, '"');
            }

            // Did we get the ending quote?
            if (found == false) {
                // parser is confused
                *result = COOKIE_PARSE_ERR;
                goto out;
            }

            range.location = pos1;
            range.length = pos3 - pos1;

            if (KEEP_QUOTED)
                range.length++;

            token = CFStringCreateWithSubstring(kCFAllocatorDefault, str, range);
            if (token == NULL) {
                *result = COOKIE_PARSE_ERR;
                goto out;
            }

            // skip over '"'
            *nextPosition = pos3 + 1;
            *result = COOKIE_VALUE;
            goto out;
        }

        // Comma was not in a quoted string, check for date string
        if (isWeekday(str, strLen, pos1) == true) {
            // expires=Thu, 23 Jun 2011 01:24:00 GMT
            // expires=Sunday, 06-Nov-94 08:49:37 GMT
            //         ^     ^
            //         |     |
            //        pos1  pos2

            // We've got a date string, so find the "next" separator
            range2.location = pos1;
            range2.length = strLen - pos1;

            if (CFStringFindWithOptions(str, CFSTR("GMT"), range2, 0, &range) != true) {
                // weekday but no "GMT", parser is confused
                *result = COOKIE_PARSE_ERR;
                goto out;
            }


            // We have a date string
            pos2 = range.location + 3;
            range2.location = pos1;
            range2.length = pos2 - pos1;

            token = CFStringCreateWithSubstring(kCFAllocatorDefault, str, range2);

            if (token == NULL) {
                *result = COOKIE_PARSE_ERR;
                goto out;
            }

            *nextPosition = pos2;
            *result = COOKIE_VALUE;
            goto out;
        }

        // If it's not a weekday in an Expires attribute, then it is a
        // delimiter for the next cookie in the header
        // expires=Sunday, 06-Nov-94 08:49:37 GMT, next_cookie-value;
        //                                       ^
        //                                       |
        //                                      pos2
        *result = COOKIE_DELIMITER;
        // skip over the comma
        *nextPosition = pos1 + 1;
    }

out:
    return (token);
}

CFStringRef cleanDomainName(CFStringRef inStr)
{
	CFStringRef workStr;
	CFMutableStringRef domainStr;
	CFIndex len, pos, nextPos;
	boolean_t done, gotFirstComponent;

	workStr = NULL;
	domainStr = NULL;
	gotFirstComponent = false;

	if (inStr == NULL) {
		goto out;
	}

	domainStr = CFStringCreateMutable(kCFAllocatorDefault, 0);
	if (domainStr == NULL){
		goto out;
	}

	len = CFStringGetLength(inStr);
	pos = 0;

	done = false;
	while (done == false) {
		workStr = nextDomainComponent(inStr, len, pos, &nextPos);
		if (workStr == NULL)
			break;
		if (gotFirstComponent == true) {
			CFStringAppend(domainStr, CFSTR("."));
		}
		else
			gotFirstComponent = true;
			CFStringAppend(domainStr, workStr);
			CFRelease(workStr);
			pos = nextPos;
		}

		if (CFStringGetLength(domainStr) == 0) {
			// Didn't get anything
			CFRelease(domainStr);
			domainStr = NULL;
		}

		if (domainStr != NULL)
			CFStringLowercase(domainStr, CFLocaleGetSystem());
out:
		return (domainStr);
	}

CFStringRef nextDomainComponent(CFStringRef inStr, CFIndex strLen, CFIndex startPos, CFIndex *nextStartPos)
{
	CFIndex p1, p2, pos;
	CFStringRef componentStr;
	CFRange range;
	UniChar ch;
	boolean_t found, result;

	componentStr = NULL;
	found = false;
	pos = startPos;

	// Find first name character
	while (pos < strLen) {
		ch = CFStringGetCharacterAtIndex(inStr, pos);
		result = isLetterDigitHyphen(ch);
		if (result == true) {
			found = true;
			break;
		}
		pos++;
	}

	if ( found == false) {
		goto out;
	}

	p1 = pos;

	// Now find end position
	pos++;
	found = false;

	while (pos < strLen) {
		ch = CFStringGetCharacterAtIndex(inStr, pos);
		result = isLetterDigitHyphen(ch);
		if (result == false) {
			found = true;
			break;
		}
		pos++;
	}

	if (pos <= p1) {
		// Nothing to return
		goto out;
	}

	p2 = pos;

	// Now make the comoponent string
	range.location = p1;
	range.length = p2 - p1;
	componentStr = CFStringCreateWithSubstring(kCFAllocatorDefault, inStr, range);
	*nextStartPos = p2;
out:
	return (componentStr);
}

boolean_t isLetterDigitHyphen(UniChar ch)
{
	if ( ((ch >= 'A') && ( ch <= 'Z')) ||
		((ch >= 'a') && ( ch <= 'z')) ||
		((ch >= '0') && ( ch <= '9')) ||
		(ch == '-') ) {
		return (true);
	} else {
		return (false);
	}
}

//
// Returns true if aStr domain-matches domainStr
// per RFC-6265
//
boolean_t doesDomainMatch(CFStringRef domainStr, CFStringRef aStr)
{
	boolean_t result;

	result = false;

	if (domainStr == NULL || aStr == NULL)
		goto out;

	// First, check if they are identical
	if (CFStringCompare(domainStr, aStr, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
		result = true;
		goto out;
	}

	// Check if domainStr is a suffix of aStr
	if (CFStringHasSuffix(aStr, domainStr) == true) {
		result = true;
		goto out;
	}

out:
	return (result);
}

boolean_t is_ip_address_str(CFStringRef hostStr)
{
	boolean_t result;
	char *str;

	result = false;

	if (hostStr == NULL) {
		goto out;
	}

	str = createUTF8CStringFromCFString(hostStr);

	if (str == NULL) {
		goto out;
	}

	result = is_ip_address(str);
out:
	return (result);
}

boolean_t is_ip_address(const char *host)
{
    struct in_addr inaddr4;
    struct in6_addr inaddr6;
    boolean_t result;

    result = false;

    if (host == NULL) {
		goto out;
    }

    // Try IPv4
    if (inet_pton(AF_INET, host, &inaddr4) == 1) {
        result = true;
        goto out;
    }

    // Try IPv6
    if (inet_pton(AF_INET6, host, &inaddr6) == 1) {
        result = true;
    }

out:
    return (result);
}

boolean_t isWeekday(CFStringRef str, CFIndex strLen, CFIndex position)
{
    CFIndex len;
    CFStringRef dayStr;
    boolean_t weekday;
    CFRange range;

    weekday = false;
    dayStr = NULL;

    if (position >= strLen) {
        goto out;
    }

    len = strLen - position;

    if (len < 3) {
        // Cannot be a weekday "Mon", "Tues", etc...
        goto out;
    }

    range.location = position;
    range.length = len;

    dayStr = CFStringCreateWithSubstring(kCFAllocatorDefault, str, range);

    if (dayStr == NULL) {
        goto out;
    }

    if (CFStringHasPrefix(dayStr, CFSTR("Mon")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Tue")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Wed")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Thu")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Fri")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Sat")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Sun")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Monday")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Tuesday")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Wednesday")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Thursday")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Friday")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Saturday")) == true)
        weekday = true;
    else if (CFStringHasPrefix(dayStr, CFSTR("Sunday")) == true)
        weekday = true;
out:
    if (dayStr != NULL)
        CFRelease(dayStr);
    return (weekday);
}

CFStringRef cookiePathFromURL(CFURLRef url)
{
	CFStringRef pathStr, tmpStr;
	CFRange rangeIn, rangeOut;
	CFIndex len;
	Boolean result;

	pathStr = NULL;
	tmpStr = NULL;

	if (url == NULL) {
		goto out;
	}

	tmpStr = CFURLCopyPath(url);
	if (tmpStr == NULL) {
		goto out;
	}

	len = CFStringGetLength(tmpStr);
	if (!len)
		goto out;

	if (CFStringHasPrefix(tmpStr, CFSTR("/")) == false) {
		goto out;
	}

	if (len == 1)
		goto out;

	// Copy the path up to (but not including) the right-most '/'
	rangeIn.location = 0;
	rangeIn.length = len;
	result = CFStringFindWithOptions(tmpStr, CFSTR("/"), rangeIn, kCFCompareBackwards, &rangeOut);

	if (result == true) {
        if (rangeOut.location > 0) {
            rangeIn.location = 0;
            rangeIn.length = rangeOut.location;
            pathStr = CFStringCreateWithSubstring(kCFAllocatorDefault, tmpStr, rangeIn);
        }
        else {
            pathStr = tmpStr;
            tmpStr = NULL;
        }
		goto out;
	} else {
		pathStr = tmpStr;
		tmpStr = NULL;
		goto out;
	}

out:
	if (tmpStr != NULL)
		CFRelease(tmpStr);
	if (pathStr == NULL) {
		pathStr = CFStringCreateCopy(kCFAllocatorDefault, CFSTR("/"));
	}
	return (pathStr);
}

void skipWhiteSpace(CFStringRef str, CFIndex strLen, CFIndex *position)
{
    CFIndex pos;
    UniChar c;

    pos = *position;

    while (pos < strLen) {
        c = CFStringGetCharacterAtIndex(str, pos);
        if ((c > 32) && (c < 127)) {
            break;
        }
        pos++;
    }

    *position = pos;
}

void skipWhiteSpaceReverse(CFStringRef str, CFIndex startPosition, CFIndex *position)
{
    CFIndex pos;
    UniChar c;

    pos = startPosition;

    while (pos > 0) {
        c = CFStringGetCharacterAtIndex(str, pos);
        if ((c > 32) && (c < 127)) {
            break;
        }
        pos--;
    }

    *position = pos;
}

boolean_t findNextSeparator(CFStringRef str, CFIndex strLen, CFIndex startPos, CFIndex *foundAt, UniChar *ch)
{
    CFIndex pos;
    UniChar c;
    boolean_t foundit;

    foundit = false;
    pos = startPos;

    // Scan for the next separator
    while (pos < strLen) {
        c = CFStringGetCharacterAtIndex(str, pos);
        switch (c) {
            case ',':
            case '=':
            case ';':
                foundit = true;
                *ch = c;
                goto out;
                break;

            default:
                break;
        }

        pos++;
    }

out:
    *foundAt = pos;
    return (foundit);
}

boolean_t findCharacter(CFStringRef str, CFIndex strLen, CFIndex startPos, CFIndex *foundAt, UniChar ch)
{
    CFIndex pos;
    UniChar c;
    boolean_t foundit;

    foundit = false;
    pos = startPos;

    // Scanning
    while (pos < strLen) {
        c = CFStringGetCharacterAtIndex(str, pos);
        if (c == ch) {
            foundit = true;
            *foundAt = pos;
            break;
        }

        pos++;
    }

    return (foundit);
}

void free_cookie_fields(WEBDAV_COOKIE *cookie)
{
    if (cookie != NULL) {
		if (cookie->cookie_header != NULL)
			CFRelease(cookie->cookie_header);
		if (cookie->cookie_name != NULL)
			CFRelease(cookie->cookie_name);
        if (cookie->cookie_name_str != NULL)
            free(cookie->cookie_name_str);

		if (cookie->cookie_val != NULL)
            CFRelease(cookie->cookie_val);
        if (cookie->cookie_val_str != NULL)
            free(cookie->cookie_val_str);

		if (cookie->cookie_path != NULL)
            CFRelease(cookie->cookie_path);
        if (cookie->cookie_path_str != NULL)
            free(cookie->cookie_path_str);

		if (cookie->cookie_domain != NULL)
            CFRelease(cookie->cookie_domain);
        if (cookie->cookie_domain_str != NULL)
            free(cookie->cookie_domain_str);
    }
}

void cookies_init(void)
{
	pthread_mutexattr_t mutexattr;

	pthread_mutexattr_init(&mutexattr);
	pthread_mutex_init(&cookie_lock, &mutexattr);

	cookie_head = NULL;
	cookie_tail = NULL;
	cookie_count = 0;
}

// Returns TRUE if path2 is enclosed in path1.
//
// Example: path1 = /a/b/c
//
//     path2    Return Value
//     -------  ------------
//     /a          FALSE
//     /a/b        FALSE
//     /a/b/c      TRUE
//     /a/b/c/d    TRUE
//
boolean_t path2InPath1(const char *path1, const char *path2)
{
    size_t path1Len, path2Len, i;
    Boolean  ret;

    ret = false;

    if (path1 == NULL || path2 == NULL)
        goto out;

    path1Len = strlen(path1);
    path2Len = strlen(path2);

    // check for zero string len
    if (path1Len == 0 || path2Len == 0) {
        goto out;
    }

    // make sure we have absolute paths here
    if (path1[0] != '/' || path2[0] != '/') {
        goto out;
    }

    // If path1 is '/', then it includes all subpaths
    if (path1Len == 1) {
        ret = true;
        goto out;
    }

    // strip trailing slashes
    if (path1[path1Len - 1] == '/')
        path1Len--;
    if (path2[path2Len - 1] == '/')
        path2Len--;

    if (path1Len > path2Len) {
        goto out;
    }

    for (i = 0; i < path1Len; i++) {
        if (path1[i] != path2[i]) {
            goto out;
        }
    }

    // if we got here then path2 is contained in path1
    ret = true;
out:
    return (ret);
}

/*****************************************************************************/

int lock_cookies(void)
{
	int error;

	error = pthread_mutex_lock(&cookie_lock);

	return (error);
}

/*****************************************************************************/

int unlock_cookies(void)
{
	int error;

	error = pthread_mutex_unlock(&cookie_lock);

	return (error);
}

/*****************************************************************************/

void dump_cookies(struct webdav_request_cookies *req)
{
	WEBDAV_COOKIE *aCookie;

	if (req == NULL) {
		syslog(LOG_DEBUG, "%s: req is null\n", __FUNCTION__);
	}

	lock_cookies();
	syslog(LOG_ERR, "%s: Cookie count: %u\n", __FUNCTION__, cookie_count);

	aCookie = cookie_head;
	while(aCookie != NULL) {
		printCookie(aCookie);
		aCookie = aCookie->next;
	}

	unlock_cookies();
}

void reset_cookies(struct webdav_request_cookies *req)
{
	WEBDAV_COOKIE *aCookie, *nextCookie;
	uint32_t num;

	if (req == NULL) {
		syslog(LOG_DEBUG, "%s: req is null\n", __FUNCTION__);
	}

	lock_cookies();
	num = 0;
	aCookie = cookie_head;
	while(aCookie != NULL) {
		nextCookie = aCookie->next;
		list_remove_cookie(aCookie);
		syslog(LOG_ERR, "%s: Removing cookie: %s\n", __FUNCTION__, aCookie->cookie_name_str);
		free_cookie_fields(aCookie);
		free (aCookie);
		num++;
		aCookie = nextCookie;
	}
	unlock_cookies();

	syslog(LOG_ERR, "%s: Removed %u cookies\n", __FUNCTION__, num);
}

/*****************************************************************************/

void printCookie(WEBDAV_COOKIE *aCookie)
{
    time_t now;

    now = time(NULL);
    if (aCookie->cookie_val_str == NULL) {
        syslog(LOG_ERR, "Cookie: '%s'\n", aCookie->cookie_name_str);
    }
    else {
		syslog(LOG_ERR, "Cookie: %s='%s'\n", aCookie->cookie_name_str, aCookie->cookie_val_str);
    }

    if (aCookie->cookie_path_str != NULL) {
		syslog(LOG_ERR, "Path: %s\n", aCookie->cookie_path_str);
    }

    if (aCookie->cookie_domain_str != NULL) {
		syslog(LOG_ERR, "Domain: %s\n", aCookie->cookie_domain_str);
    }

    if (aCookie->has_expire_time == TRUE) {
        now = time(NULL);
		syslog(LOG_ERR, "Expires @: %ld (current %ld)\n", aCookie->cookie_expire_time, now);
    }

    if (aCookie->cookie_secure == TRUE)
		syslog(LOG_ERR, "Secure\n");

    if (aCookie->cookie_httponly == TRUE)
		syslog(LOG_ERR, "HttpOnly\n");
}

/*****************************************************************************/
