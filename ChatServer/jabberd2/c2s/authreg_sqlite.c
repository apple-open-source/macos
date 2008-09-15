/*
 * jabberd - Jabber Open Source Server
 * Copyright (c) 2002 Jeremie Miller, Thomas Muldowney,
 *                    Ryan Eatmon, Robert Norris
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
 */

/* this is a simple anonymous plugin. It uses the check_password method to
 * force authentication to succeed regardless of what credentials the client
 * provides
 */

#include "c2s.h"

#ifdef STORAGE_SQLITE

#ifdef APPLE_ENABLE_OD_AUTH
#include <apple_authenticate.h>
#include <apple_authorize.h>
#endif /* APPLE_ENABLE_OD_AUTH */

#ifdef APPLE_ENABLE_OD_AUTH
/* Apple overrides for Open Directory authentication */

/* -----------------------------------------------------------------
	int _ar_od_user_exists()
	
	RETURNS:
		0 = user not found
		1 = user exists
   ----------------------------------------------------------------- */
static int _ar_od_user_exists(authreg_t ar, char *username, char *realm)
{
	log_debug( ZONE, "_ar_od_user_exists()." );
	if (NULL != username) log_debug( ZONE, "_ar_od_user_exists(): username = %s.", username);
	if (NULL != realm) log_debug( ZONE, "_ar_od_user_exists(): realm = %s.", realm);

	int iResult = od_auth_check_user_exists((const char *) username);
	log_debug( ZONE, "_ar_od_user_exists(): od_auth_check_user_exists returned %d", iResult );
	if (0 > iResult) /* error? */
		iResult = 0; /* return "not found" */

    return iResult;
}

/* -----------------------------------------------------------------
	int _ar_od_check_password()
	
	RETURNS:
		0 = password is authenticated
		1 = authentication failed
   ----------------------------------------------------------------- */
static int _ar_od_check_password(authreg_t ar, char *username, char *realm, char password[257])
{
	log_debug( ZONE, "_ar_od_check_password()." );
	if (NULL != username) log_debug( ZONE, "_ar_od_check_password(): username = %s.", username);
	if (NULL != realm) log_debug( ZONE, "_ar_od_check_password(): realm = %s.", realm);
	if ((NULL != password) && (0 < strlen(password)))
		log_debug( ZONE, "_ar_od_check_password(): password = %s.", password);
	
	/* Verify the password */
	int iResult = od_auth_check_plain_password(username, password);
	log_debug( ZONE, "_ar_od_check_password(): od_auth_check_plain_password returned %d", iResult );
	if (0 != iResult) /* error? */
		iResult = 1; /* return "auth failed" */
	else {
		/* Now that we know the user is legit, verify service access */
		int iErr = od_auth_check_service_membership(username, APPLE_CHAT_SACL_NAME);
		log_debug( ZONE, "_ar_od_check_password(): od_auth_check_service_membership returned %d", iErr );
		iResult = (1 == iErr) ? 0 : 1; /* return success/fail */
	}

    return iResult;
}

/* -----------------------------------------------------------------
	int _ar_od_create_challenge()
	
	RETURNS:
	   -1 = CRAM-MD5 unsupported for this user
		0 = operation failed
		1 = operation succeeded
   ----------------------------------------------------------------- */
static int _ar_od_create_challenge(authreg_t ar, char *username, char *challenge, int maxlen)
{
	log_debug( ZONE, "_ar_od_create_challenge()." );
	
	/* check whether the user account supports CRAM-MD5 password authentication */
	int iResult = od_auth_supports_cram_md5(username);
	log_debug( ZONE, "_ar_od_create_challenge(): od_auth_supports_cram_md5 returned %d", iResult );
	if (0 == iResult) /* auth method not available for this user */
		iResult = -1; /* return "failed" */
	
	/* create a unique challenge for this request */
	iResult = od_auth_create_crammd5_challenge(challenge, maxlen);
	log_debug( ZONE, "_ar_od_create_challenge(): od_auth_create_crammd5_challenge returned %d", iResult );
	if (0 < iResult) /* ok? */
		iResult = 1; /* return "success" */

    return iResult;
}

/* -----------------------------------------------------------------
	int _ar_od_check_response()
	
	RETURNS:
		0 = response is authenticated
		1 = authentication failed
   ----------------------------------------------------------------- */
static int _ar_od_check_response(authreg_t ar, char *username, char *realm, char *challenge, char *response)
{
	log_debug( ZONE, "_ar_od_check_response()." );
	if (NULL != username) log_debug( ZONE, "_ar_od_check_response(): username = %s.", username);
	if (NULL != realm) log_debug( ZONE, "_ar_od_check_response(): realm = %s.", realm);
	if ((NULL != challenge) && (0 < strlen(challenge)))
		log_debug( ZONE, "_ar_od_check_response(): challenge = %s.", challenge);
	if ((NULL != response) && (0 < strlen(response)))
		log_debug( ZONE, "_ar_od_check_response(): response = %s.", response);
	
	/* Verify the response */
	int iResult = od_auth_check_crammd5_response(username, challenge, response);
	log_debug( ZONE, "_ar_od_check_response(): od_auth_check_crammd5_response returned %d", iResult );
	if (0 != iResult) /* error? */
		iResult = 1; /* return "auth failed" */
	else {
		/* Now that we know the user is legit, verify service access */
		int iErr = od_auth_check_service_membership(username, APPLE_CHAT_SACL_NAME);
		log_debug( ZONE, "_ar_od_check_response(): od_auth_check_service_membership returned %d", iErr );
		iResult = (1 == iErr) ? 0 : 1; /* return success/fail */
	}

    return iResult;
}
#endif /* APPLE_ENABLE_OD_AUTH */

static int _ar_anon_user_exists(authreg_t ar, char *username, char *realm)
{
    /* always exists */
    return 1;
}

static int _ar_anon_check_password(authreg_t ar, char *username, char *realm, char password[257])
{
    /* always correct */
    return 0;
}

/** start me up */
int ar_sqlite_init(authreg_t ar)
{
#ifdef APPLE_ENABLE_OD_AUTH
    log_debug( ZONE, "APPLE: initializing OD auth functions." );
    ar->user_exists = _ar_od_user_exists;
	ar->check_password = _ar_od_check_password;
	ar->create_challenge = _ar_od_create_challenge;
	ar->check_response = _ar_od_check_response;
#else
    ar->user_exists = _ar_anon_user_exists;
    ar->check_password = _ar_anon_check_password;
#endif /* APPLE_ENABLE_OD_AUTH */

    return 0;
}

#endif
