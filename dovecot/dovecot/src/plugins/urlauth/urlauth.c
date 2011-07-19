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
#include "buffer.h"
#include "str.h"
#include "hex-binary.h"
#include "urlauth-plugin.h"

#include <openssl/hmac.h>

#define	URLAUTH_URLAUTH_INTERNAL_VERSION	"d1"	/* must be hex */

// compute the urlauth token using the INTERNAL mechanism
void urlauth_urlauth_generate_internal(const char *rump,
				       const buffer_t *key,
				       string_t *urlauth)
{
	const void *key_data;
	size_t key_len = 0;
	unsigned char mac[EVP_MAX_MD_SIZE];
	unsigned int mac_len = 0;

	// compute HMAC-SHA1 of rump with key
	key_data = buffer_get_data(key, &key_len);
	i_assert(key_len > 0);
	HMAC(EVP_sha1(), key_data, key_len, (const unsigned char *) rump,
	     strlen(rump), mac, &mac_len);

	str_append(urlauth, URLAUTH_URLAUTH_INTERNAL_VERSION);
	str_append(urlauth, binary_to_hex(mac, mac_len));
}
	
// validate all the parts of the URL
bool urlauth_url_validate(const struct imap_url_parts *parts, bool full,
			  const char **error)
{
	// user: mandatory; RFC 3501 "userid"
	if (parts->user == NULL ||
	    !imap_url_astring_validate(parts->user)) {
		*error = "missing or invalid user ID";
		return FALSE;
	}

	// auth_type: optional; RFC 3501 "auth-type"
	if (parts->auth_type != NULL &&
	    !imap_url_atom_validate(parts->auth_type)) {
		*error = "invalid auth type";
		return FALSE;
	}
		    
	// hostport: mandatory; RFC 1738 "hostport"
	if (parts->hostport == NULL ||
	    !imap_url_hostport_validate(parts->hostport)) {
		*error = "missing or invalid server";
		return FALSE;
	}

	// mailbox: mandatory; RFC 3501 "mailbox"
	if (parts->mailbox == NULL ||
	    !imap_url_mailbox_validate(parts->mailbox)) {
		*error = "missing or invalid mailbox";
		return FALSE;
	}

	// uidvalidity: optional; RFC 3501 "nz-number"
	if (parts->uidvalidity != NULL &&
	    !imap_url_nz_number_validate(parts->uidvalidity)) {
		*error = "invalid uidvalidity";
		return FALSE;
	}

	// uid: mandatory; RFC 3501 "nz-number"
	if (parts->uid == NULL ||
	    !imap_url_nz_number_validate(parts->uid)) {
		*error = "missing or invalid uid";
		return FALSE;
	}

	// section: optional; RFC 2192 "section"
	if (parts->section != NULL &&
	    !imap_url_section_validate(parts->section)) {
		*error = "invalid section";
		return FALSE;
	}

	// expiration: optional; RFC 3339 "date-time"
	if (parts->expiration != NULL &&
	    (!imap_url_datetime_validate(parts->expiration) ||
	     parts->expiration_time == 0)) {
		*error = "invalid expiration";
		return FALSE;
	}

	// access: mandatory; RFC 4467 "access"
	if (parts->access == NULL ||
	    !imap_url_access_validate(parts->access)) {
		*error = "missing or invalid access ID";
		return FALSE;
	}

	if (full) {
		// mechanism: mandatory; RFC 4467 "mechanism"
		if (parts->mechanism == NULL ||
		    !imap_url_mechanism_validate(parts->mechanism)) {
			*error = "missing or invalid mechanism";
			return FALSE;
		}

		// urlauth: mandatory; RFC 4467 "urlauth"
		if (parts->urlauth == NULL ||
		    !imap_url_urlauth_validate(parts->urlauth)) {
			*error = "missing or invalid access token";
			return FALSE;
		}
	} else {
		// mechanism: absent
		if (parts->mechanism != NULL) {
			*error = "mechanism present";
			return FALSE;
		}

		// urlauth: absent
		if (parts->urlauth != NULL) {
			*error = "access token present";
			return FALSE;
		}
	}

	return TRUE;
}
