/*
 * Copyright 1993-2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef	_BSD_SOLARIS_H
#define	_BSD_SOLARIS_H

#if defined(HAVE_BSM_AUDIT_H) && defined(HAVE_LIBBSM)

#pragma ident	"@(#)bsmaudit.h	1.1	01/09/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <bsm/audit.h>
#define	AUE_openssh	32800

void solaris_audit_maxtrys(void);
void solaris_audit_nologin(void);
void solaris_audit_save_name(const char *name);
void solaris_audit_save_pw(struct passwd *pwd);
void solaris_audit_not_console(void);
void solaris_audit_bad_pw(const char *what);
void solaris_audit_save_host(const char *host);
void solaris_audit_save_ttyn(const char *ttyn);
void solaris_audit_save_port(int port);
void solaris_audit_save_command(const char *command);
void solaris_audit_success(void);
void solaris_audit_logout(void);

#ifdef	__cplusplus
}
#endif

#endif /* BSM */

#endif	/* _BSD_SOLARIS_H */
