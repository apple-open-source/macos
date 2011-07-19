/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "strfuncs.h"

#include "sieve-storage.h"
#include "sieve-storage-quota.h"

#include "managesieve-client.h"
#include "managesieve-quota.h"

bool managesieve_quota_check_validsize(struct client *client, size_t size)
{	
	uint64_t limit;

	if ( !sieve_storage_quota_validsize
		(client->storage, size, &limit) ) {
		client_send_noresp(client, "QUOTA/MAXSIZE", 
			t_strdup_printf("Script is too large (max %llu bytes).", 
			(unsigned long long int) limit));
	
		return FALSE;
	}

	return TRUE;
}
 
bool managesieve_quota_check_all
(struct client *client, const char *scriptname, size_t size)
{
	enum sieve_storage_quota quota;
	uint64_t limit;
	int ret;

	if ( (ret=sieve_storage_quota_havespace
		(client->storage, scriptname, size, &quota, &limit)) <= 0 ) {
		if ( ret == 0 ) {
			switch ( quota ) {
			case SIEVE_STORAGE_QUOTA_MAXSIZE:
				client_send_noresp(client, "QUOTA/MAXSIZE", t_strdup_printf(
					"Script is too large (max %llu bytes).", 
					(unsigned long long int) limit));
				break;

			case SIEVE_STORAGE_QUOTA_MAXSCRIPTS:
				client_send_noresp(client, "QUOTA/MAXSCRIPTS", t_strdup_printf(
					"Script count quota exceeded (max %llu scripts).", 
					(unsigned long long int) limit));
				break;

			case SIEVE_STORAGE_QUOTA_MAXSTORAGE:
				client_send_noresp(client, "QUOTA/MAXSTORAGE", t_strdup_printf(
					"Script storage quota exceeded (max %llu bytes).", 
					(unsigned long long int) limit));
				break;

			default:
				client_send_noresp(client, "QUOTA", "Quota exceeded.");
				break;
			}
		} else {
			client_send_storage_error(client, client->storage);
		}

		return FALSE;
	}

	return TRUE;
}

