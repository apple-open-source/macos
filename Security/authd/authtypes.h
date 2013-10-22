/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#ifndef _SECURITY_AUTH_TYPES_H_
#define _SECURITY_AUTH_TYPES_H_

#include <bsm/audit.h>
#include <mach/message.h>

typedef struct {
    uid_t auid;
	uid_t euid;
	gid_t egid;
	uid_t ruid;
	gid_t rgid;
	pid_t pid;
	au_asid_t asid;
	int32_t tid;
    audit_token_t opaqueToken;
} audit_info_s;

typedef au_asid_t session_id_t;
typedef struct _session_s * session_t;
typedef struct _process_s * process_t;
typedef struct _connection_s * connection_t;

typedef struct _auth_token_s * auth_token_t;
typedef struct _credential_s * credential_t;

typedef struct _ccaudit_s * ccaudit_t;
typedef struct _mechanism_s * mechanism_t;
typedef struct _rule_s * rule_t;
typedef struct _agent_s * agent_t;
typedef struct _engine_s * engine_t;

typedef struct _authdb_s * authdb_t;
typedef struct _authdb_connection_s * authdb_connection_t;

typedef struct _auth_items_s * auth_items_t;
typedef struct _auth_rights_s * auth_rights_t;

#endif /* !_SECURITY_AUTH_TYPES_H_ */
