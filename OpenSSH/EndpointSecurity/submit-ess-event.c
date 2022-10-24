//
//  submit-ess-event.c
//  sshd
//
//  Copyright © 2022 Apple Inc. All rights reserved.
//

#ifdef __APPLE_ENDPOINTSECURITY__
#include <EndpointSecuritySystem/ESSubmitSPI.h>
#include <SoftLinking/WeakLinking.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>

#include "submit-ess-event.h"
#include "log.h"

extern Authctxt *the_authctxt;

WEAK_LINK_FORCE_IMPORT(ess_notify_openssh_login);
WEAK_LINK_FORCE_IMPORT(ess_notify_openssh_logout);

ess_address_type_t
get_socket_family(const char *ip_addr)
{
	struct addrinfo *ai;
	int r;
	ess_address_type_t address_type;

	if ((r = getaddrinfo(ip_addr, NULL, NULL, &ai)) != 0) {
		error("getaddrinfo failed for %.100s: %.100s", ip_addr,
		    r == EAI_SYSTEM ? strerror(errno) : gai_strerror(r));
		return ESS_ADDRESS_TYPE_NONE;
	}

	switch (ai->ai_family) {
		case AF_INET:
			address_type = ESS_ADDRESS_TYPE_IPV4;
			break;
		case AF_INET6:
			address_type = ESS_ADDRESS_TYPE_IPV6;
			break;
		default:
			address_type = ESS_ADDRESS_TYPE_NONE;
			break;
	}

	freeaddrinfo(ai);
	return address_type;
}

void
submit_ess_event(const char *source_address, const char *username, ssh_audit_event_t audit_event)
{
	if (ess_notify_openssh_login == NULL || ess_notify_openssh_logout == NULL ) {
		return;
	}

	ess_address_type_t address_type = get_socket_family(source_address);
	if (address_type == ESS_ADDRESS_TYPE_NONE) {
		return;
	}

	bool is_disconnect = false;
	ess_openssh_login_result_type_t login_result_type;
	switch(audit_event) {
		case SSH_LOGIN_EXCEED_MAXTRIES:
			login_result_type = ESS_OPENSSH_LOGIN_EXCEED_MAXTRIES;
			break;
		case SSH_LOGIN_ROOT_DENIED:
			login_result_type = ESS_OPENSSH_LOGIN_ROOT_DENIED;
			break;
		case SSH_AUTH_SUCCESS:
			login_result_type = ESS_OPENSSH_AUTH_SUCCESS;
			break;
		case SSH_AUTH_FAIL_NONE:
			login_result_type = ESS_OPENSSH_AUTH_FAIL_NONE;
			break;
		case SSH_AUTH_FAIL_PASSWD:
			login_result_type = ESS_OPENSSH_AUTH_FAIL_PASSWD;
			break;
		case SSH_AUTH_FAIL_KBDINT:
			login_result_type = ESS_OPENSSH_AUTH_FAIL_KBDINT;
			break;
		case SSH_AUTH_FAIL_PUBKEY:
			login_result_type = ESS_OPENSSH_AUTH_FAIL_PUBKEY;
			break;
		case SSH_AUTH_FAIL_HOSTBASED:
			login_result_type = ESS_OPENSSH_AUTH_FAIL_HOSTBASED;
			break;
		case SSH_AUTH_FAIL_GSSAPI:
			login_result_type = ESS_OPENSSH_AUTH_FAIL_GSSAPI;
			break;
		case SSH_INVALID_USER:
			login_result_type = ESS_OPENSSH_INVALID_USER;
			break;
		case SSH_NOLOGIN:
			// This isn't emitted by ssh.
			return;
		case SSH_CONNECTION_CLOSE:
		case SSH_CONNECTION_ABANDON:
			// Treat CLOSE and ABANDON the same, as the most common case of
			// regular connection close emits an ABANDON, not a CLOSE, and
			// we don't make that distinction in EndpointSecurity anyway.
			// The use of CLOSE and ABANDON should probably be revisited
			// upstream.
			is_disconnect = true;
			break;
		case SSH_AUDIT_UNKNOWN:
			error("Unknown audit event type : %d", audit_event);
			return;
	}

	if (the_authctxt == NULL) {
		error("auth context not available");
		return;
	}

	uid_t uid = -1;
	if (the_authctxt->valid) {
		uid = the_authctxt->pw->pw_uid;
	}

	if (!is_disconnect) {
		ess_notify_openssh_login(login_result_type, address_type, source_address, username, (the_authctxt->valid) ? &uid : NULL);
	} else if (the_authctxt->authenticated) {
		// Only emit a logout event if the session was authenticated.
		ess_notify_openssh_logout(address_type, source_address, username, uid);
	}
}
#endif
