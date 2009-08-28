
/* 
   Samba Unix/Linux Dynamic DNS Update
   net ads commands

   Copyright (C) Krishna Ganugapati (krishnag@centeris.com)         2006
   Copyright (C) Gerald Carter                                      2006
   Copyright (C) 2008 Apple Inc. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
*/

#include "includes.h"
#include "utils/net.h"
#include "dns.h"

#if defined(WITH_DNS_UPDATES)
extern const char *dns_errstr(DNS_ERROR err);

/*********************************************************************
*********************************************************************/

static DNS_ERROR
negotiate_security_context(TALLOC_CTX * mem_ctx,
			const char * pszDomainName,
			const char * pszServerName,
			char ** keyname,
			gss_ctx_id_t * gss_context)
{
	DNS_ERROR err;

	if (!(*keyname = dns_generate_keyname( mem_ctx ))) {
		return ERROR_DNS_NO_MEMORY;
	}

	err = dns_negotiate_sec_ctx( pszDomainName, pszServerName,
				     *keyname, gss_context, DNS_SRV_ANY );

	/* retry using the Windows 2000 DNS hack */
	if (!ERR_DNS_IS_OK(err)) {
		return dns_negotiate_sec_ctx( pszDomainName, pszServerName,
					     *keyname, gss_context,
					     DNS_SRV_WIN2000 );
	}

	return ERROR_DNS_SUCCESS;
}

static DNS_ERROR DoDNSUpdate_A(char *pszServerName,
		      const char *pszDomainName, const char *pszHostName,
		      const struct in_addr *iplist, size_t num_addrs)
{
	DNS_ERROR err;
	struct dns_connection *conn;
	TALLOC_CTX *mem_ctx;
	OM_uint32 minor;
	struct dns_update_request *req, *resp;

	if (!(mem_ctx = talloc_init(__func__))) {
		return ERROR_DNS_NO_MEMORY;
	}
		
	err = dns_open_connection( pszServerName, DNS_TCP, mem_ctx, &conn );
	if (!ERR_DNS_IS_OK(err)) {
		goto error;
	}

	/*
	 * Probe if everything's fine
	 */

	err = dns_create_probe(mem_ctx, pszDomainName, pszHostName,
			       num_addrs, iplist, &req);
	if (!ERR_DNS_IS_OK(err)) goto error;

	err = dns_update_transaction(mem_ctx, conn, req, &resp);
	if (!ERR_DNS_IS_OK(err)) goto error;

	if (dns_response_code(resp->flags) == DNS_NO_ERROR) {
		TALLOC_FREE(mem_ctx);
		return ERROR_DNS_SUCCESS;
	}

	/*
	 * First try without signing
	 */

	err = dns_create_update_request_a(mem_ctx, pszDomainName, pszHostName,
					iplist, num_addrs, &req);
	if (!ERR_DNS_IS_OK(err)) goto error;

	err = dns_update_transaction(mem_ctx, conn, req, &resp);
	if (!ERR_DNS_IS_OK(err)) goto error;

	if (dns_response_code(resp->flags) == DNS_NO_ERROR) {
		TALLOC_FREE(mem_ctx);
		return ERROR_DNS_SUCCESS;
	}

	/*
	 * Okay, we have to try with signing
	 */
	{
		gss_ctx_id_t gss_context;
		char *keyname;

		err = negotiate_security_context(mem_ctx,
			pszDomainName, pszServerName,
			&keyname, &gss_context);

		if (!ERR_DNS_IS_OK(err))
			goto error;

		err = dns_sign_update(req, gss_context, keyname,
				      "gss.microsoft.com", time(NULL), 3600);

		gss_delete_sec_context(&minor, &gss_context, GSS_C_NO_BUFFER);
		if (!ERR_DNS_IS_OK(err)) goto error;

		err = dns_update_transaction(mem_ctx, conn, req, &resp);
		if (!ERR_DNS_IS_OK(err)) goto error;

		err = (dns_response_code(resp->flags) == DNS_NO_ERROR) ?
			ERROR_DNS_SUCCESS : ERROR_DNS_UPDATE_FAILED;
		if (!ERR_DNS_IS_OK(err)) goto error;

		TALLOC_FREE(mem_ctx);
		return ERROR_DNS_SUCCESS;

	}


error:
	TALLOC_FREE(mem_ctx);
	return err;
}

static DNS_ERROR DoDNSUpdate_PTR_with_zone(TALLOC_CTX *mem_ctx,
			struct dns_connection *conn,
			char *pszServerName,
			const char *pszDomainName,
			const char *pszHostName,
			const char * zone_name,
			const struct in_addr ip)
{
	DNS_ERROR err;
	struct dns_update_request *ptr_req, *resp;

#define WRONG_ZONE_RESPONSE(code) \
	(((code) == DNS_NOTZONE) || ((code) == DNS_NOTAUTH))

	/*
	 * First try without signing
	 */

	err = dns_create_update_request_ptr(mem_ctx, pszHostName,
					zone_name, ip, &ptr_req);
	if (!ERR_DNS_IS_OK(err)) goto error;

	err = dns_update_transaction(mem_ctx, conn, ptr_req, &resp);
	if (!ERR_DNS_IS_OK(err)) goto error;

	if (dns_response_code(resp->flags) == DNS_NO_ERROR) {
		return ERROR_DNS_SUCCESS;
	} else if (WRONG_ZONE_RESPONSE(dns_response_code(resp->flags))) {
		return ERROR_DNS_WRONG_ZONE;
	}

	/*
	 * Okay, we have to try with signing
	 */
	{
		OM_uint32 minor;
		gss_ctx_id_t gss_context;
		char *keyname;

		err = negotiate_security_context(mem_ctx,
			pszDomainName, pszServerName,
			&keyname, &gss_context);

		if (!ERR_DNS_IS_OK(err)) goto error;

		err = dns_sign_update(ptr_req, gss_context, keyname,
				      "gss.microsoft.com", time(NULL), 3600);

		gss_delete_sec_context(&minor, &gss_context, GSS_C_NO_BUFFER);
		if (!ERR_DNS_IS_OK(err)) goto error;

		err = dns_update_transaction(mem_ctx, conn, ptr_req, &resp);
		if (!ERR_DNS_IS_OK(err)) goto error;

		if (dns_response_code(resp->flags) == DNS_NO_ERROR) {
			err = ERROR_DNS_SUCCESS;
		} else if (WRONG_ZONE_RESPONSE(dns_response_code(resp->flags))) {
			err = ERROR_DNS_WRONG_ZONE;
		} else {
			err = ERROR_DNS_UPDATE_FAILED;
		}
	}

#undef WRONG_ZONE_RESPONSE

error:
	return err;
}

static DNS_ERROR DoDNSUpdate_PTR(char *pszServerName,
		      const char *pszDomainName,
		      const char *pszHostName,
		      const struct in_addr ip)
{
	DNS_ERROR err;
	struct dns_connection *conn;
	char * zone_name;
	TALLOC_CTX *mem_ctx;

	if (!(mem_ctx = talloc_init(__func__))) {
		return ERROR_DNS_NO_MEMORY;
	}

	zone_name = talloc_asprintf(mem_ctx, "%d.%d.%d.%d.in-addr.arpa.",
		(ntohl(ip.s_addr) & 0x000000ff),
		(ntohl(ip.s_addr) & 0x0000ff00) >> 8,
		(ntohl(ip.s_addr) & 0x00ff0000) >> 16,
		(ntohl(ip.s_addr) & 0xff000000) >> 24);
	if (zone_name == NULL) {
		err = ERROR_DNS_NO_MEMORY;
		goto done;
	}

	err = dns_open_connection( pszServerName, DNS_TCP, mem_ctx, &conn );
	if (!ERR_DNS_IS_OK(err)) {
		goto done;
	}

	/* Keep knocking off domain components until we find a zone that
	 * we can update out PTR record in.
	 */
	while ((zone_name = strchr(zone_name, '.'))) {
		/* Skip the '.' we are pointing at. */
		zone_name++;

		/* Stop if we hit the end or the root DNS servers. */
		if (*zone_name == '\0' ||
		    strcmp(zone_name, "in-addr.arpa.") == 0) {
			err = ERROR_DNS_INVALID_NAME;
			goto done;
		}

		err = DoDNSUpdate_PTR_with_zone(mem_ctx, conn,
			    pszServerName, pszDomainName, pszHostName,
			    zone_name, ip);

		DEBUG(6, ("updating PTR for %s in %s zone: %s\n",
			    pszHostName, zone_name, dns_errstr(err)));

		if (ERR_DNS_IS_OK(err)) {
			goto done;
		}

		if (!ERR_DNS_EQUAL(err, ERROR_DNS_WRONG_ZONE)) {
			goto done;
		}
	}

done:
	TALLOC_FREE(mem_ctx);
	return err;
}

DNS_ERROR DoDNSUpdate(char *pszServerName,
		      const char *pszDomainName, const char *pszHostName,
		      const struct in_addr *iplist, size_t num_addrs )
{
	DNS_ERROR a_err;
	DNS_ERROR ptr_err;

	if ( (num_addrs <= 0) || !iplist ) {
		return ERROR_DNS_INVALID_PARAMETER;
	}

	a_err = DoDNSUpdate_A(pszServerName, pszDomainName, pszHostName,
				iplist, num_addrs);
	if (!ERR_DNS_IS_OK(a_err)) {
		d_printf("DNS A-record update for %s failed: %s\n",
				pszHostName, dns_errstr(a_err));
	}

	ptr_err = DoDNSUpdate_PTR(pszServerName, pszDomainName, pszHostName,
				iplist[0]);
	if (!ERR_DNS_IS_OK(ptr_err)) {
		d_printf("DNS PTR-record update for %s failed: %s\n",
				pszHostName, dns_errstr(ptr_err));
	}

	if (!ERR_DNS_IS_OK(a_err)) {
	     return a_err;
	}

	if (!ERR_DNS_IS_OK(ptr_err)) {
	     return ptr_err;
	}

	return ERROR_DNS_SUCCESS;
}

/*********************************************************************
*********************************************************************/

static bool ip4_mask_match(struct in_addr ip, struct in_addr mask)
{
    return (ip.s_addr & mask.s_addr) == mask.s_addr;
}

int get_my_ip_address( struct in_addr **ips )
{
	int i, n;
	struct in_addr loopback_ip = *interpret_addr2("127.0.0.0");
	struct in_addr *list;
	int count = 0;

	/* find the first non-loopback address from our list of interfaces */

	load_interfaces();
	n = iface_count();
	
	if ( (list = SMB_MALLOC_ARRAY( struct in_addr, n )) == NULL ) {
		return -1;
	}

	for (i = 0; i < n; i++) {
		struct interface *iface = get_interface(i);
		if (ip4_mask_match(iface->ip, loopback_ip)) {
			/* Skip addresses that are in the loopback network. */
			continue;
		}

		memcpy(&list[count++], &iface->ip, sizeof(struct in_addr));
	}

	*ips = list;
	return count;
}

DNS_ERROR do_gethostbyname(const char *server, const char *host)
{
	struct dns_connection *conn;
	struct dns_request *req, *resp;
	DNS_ERROR err;

	err = dns_open_connection(server, DNS_UDP, NULL, &conn);
	if (!ERR_DNS_IS_OK(err)) goto error;

	err = dns_create_query(conn, host, QTYPE_A, DNS_CLASS_IN, &req);
	if (!ERR_DNS_IS_OK(err)) goto error;

	err = dns_transaction(conn, conn, req, &resp);

 error:
	TALLOC_FREE(conn);
	return err;
}

#endif	/* defined(WITH_DNS_UPDATES) */
