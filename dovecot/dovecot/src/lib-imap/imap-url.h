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

/* APPLE - urlauth, catenate */

#ifndef IMAP_URL_H
#define IMAP_URL_H

struct imap_url_parts {
	const char *user;
	const char *auth_type;
	const char *hostport;
	const char *mailbox;
	const char *uidvalidity;
	const char *uid;
	const char *section;
	const char *expiration;
	time_t expiration_time;
	const char *access;
	const char *mechanism;
	const char *urlauth;

	const char *rump;
};

// URL-decode enc into dec (%XX decoding)
bool url_decode(const char *enc, string_t *dec);

// parse an RFC 2192+4467 URL into its parts
void imap_url_parse(const char *url, struct imap_url_parts *parts);

// decode the parts of an IMAP URL
bool imap_url_decode(const struct imap_url_parts *enc_parts,
		     struct imap_url_parts *dec_parts,
		     const char **error);

// validate conformance to RFC 3501 "atom"
bool imap_url_atom_validate(const char *s);
// validate conformance to RFC 3501 "ASTRING-CHAR"
bool imap_url_astring_chars_validate(const char *s);
// validate conformance to RFC 3501 "quoted"
bool imap_url_quoted_validate(const char *s);
// validate conformance to RFC 3501 "literal"
bool imap_url_literal_validate(const char *s);
// validate conformance to RFC 3501 "astring"
bool imap_url_astring_validate(const char *s);
// validate conformance to RFC 1738 "hostport"
bool imap_url_hostport_validate(const char *s);
// validate conformance to RFC 3501 "mailbox"
bool imap_url_mailbox_validate(const char *s);
// validate conformance to RFC 3501 "nz_number"
bool imap_url_nz_number_validate(const char *s);
// validate conformance to RFC 3501 "section-text"
bool imap_url_section_text_validate(const char *s);
// validate conformance to RFC 2192 "section"
bool imap_url_section_validate(const char *s);
// validate conformance to RFC 3339 "date-time"
bool imap_url_datetime_validate(const char *s);
// validate conformance to RFC 4467 "access"
bool imap_url_access_validate(const char *s);
// validate conformance to RFC 4467 "mechanism"
bool imap_url_mechanism_validate(const char *s);
// validate conformance to RFC 4467 "urlauth" (really enc-urlauth)
bool imap_url_urlauth_validate(const char *s);

// build a URL from parts
void imap_url_construct(const struct imap_url_parts *parts, string_t *url);

// filter a URL according to RFC 4469 url-resp-text
const char *imap_url_sanitize(const char *url);

#endif
