/*
 * Copyright 1988-2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
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
#pragma ident	"@(#)bsmaudit.c	1.1	01/09/17 SMI"

#include "includes.h"
#if defined(HAVE_BSM_AUDIT_H) && defined(HAVE_LIBBSM)
#ifndef __APPLE__
#include <sys/systeminfo.h>
#endif
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifndef __APPLE__
#include <sys/systeminfo.h>
#endif
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>

#include <pwd.h>
#ifndef __APPLE__
#include <shadow.h>
#include <utmpx.h>
#endif
#include <unistd.h>
#include <string.h>

#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <bsm/audit_uevents.h>
#include <bsm/audit_record.h>
#include "openbsd-compat/bsd-solaris.h"

#include <locale.h>

#include "ssh.h"
#include "log.h"

#ifdef __APPLE__
#define gettext(x) (x)
#endif

#if defined(HAVE_GETAUDIT_ADDR)
#define	AuditInfoStruct		auditinfo_addr
#define AuditInfoTermID		au_tid_addr_t
#define GetAuditFunc(a,b)	getaudit_addr((a),(b))
#define GetAuditFuncText	"getaudit_addr"
#define SetAuditFunc(a,b)	setaudit_addr((a),(b))
#define SetAuditFuncText	"setaudit_addr"
#define AUToSubjectFunc		au_to_subject_ex
#define AUToReturnFunc(a,b)	au_to_return32((a), (int32_t)(b))
#else
#define	AuditInfoStruct		auditinfo
#define AuditInfoTermID		au_tid_t
#define GetAuditFunc(a,b)	getaudit(a)
#define GetAuditFuncText	"getaudit"
#define SetAuditFunc(a,b)	setaudit(a)
#define SetAuditFuncText	"setaudit"
#define AUToSubjectFunc		au_to_subject
#define AUToReturnFunc(a,b)	au_to_return((a), (u_int)(b))
#endif

static void solaris_audit_record(int typ, char *string, au_event_t event_no);
static void solaris_audit_session_setup(void);
static int selected(char *nam, uid_t uid, au_event_t event, int sf);

static void get_terminal_id(AuditInfoTermID *tid);

#ifndef __APPLE__
extern int	cannot_audit(int);
#endif
extern void	aug_init(void);
extern dev_t	aug_get_port(void);
extern int 	aug_get_machine(char *, uint32_t *, uint32_t *);
extern void	aug_save_auid(au_id_t);
extern void	aug_save_uid(uid_t);
extern void	aug_save_euid(uid_t);
extern void	aug_save_gid(gid_t);
extern void	aug_save_egid(gid_t);
extern void	aug_save_pid(pid_t);
extern void	aug_save_asid(au_asid_t);
extern void	aug_save_tid(dev_t, unsigned int);
extern void	aug_save_tid_ex(dev_t, uint32_t *, uint32_t);
extern int	aug_save_me(void);
extern int	aug_save_namask(void);
extern void	aug_save_event(au_event_t);
extern void	aug_save_sorf(int);
extern void	aug_save_text(char *);
extern void	aug_save_text1(char *);
extern void	aug_save_text2(char *);
extern void	aug_save_na(int);
extern void	aug_save_user(char *);
extern void	aug_save_path(char *);
extern int	aug_save_policy(void);
extern void	aug_save_afunc(int (*)(int));
extern int	aug_audit(void);
extern int	aug_na_selected(void);
extern int	aug_selected(void);
extern int	aug_daemon_session(void);

static char	sav_ttyn[512];
static char	sav_name[512];
static uid_t	sav_uid;
static gid_t	sav_gid;
static dev_t	sav_port;
static uint32_t	sav_machine[4];
static uint32_t	sav_iptype;
static char	sav_host[MAXHOSTNAMELEN];
static char	*sav_cmd = NULL;

void
solaris_audit_save_port(int port)
{
	if (cannot_audit(0)) {
		return;
	}
	sav_port = port;
	debug3("BSM audit: sav_port=%ld", (long)sav_port);
}

void
solaris_audit_save_host(const char *host)
{
	int		i;
#if !defined(HAVE_GETAUDIT_ADDR)
	in_addr_t	ia;
#endif

	if (cannot_audit(0)) {
		return;
	}
	(void) strlcpy(sav_host, host, sizeof (sav_host));
	debug3("BSM audit: sav_host=%s", sav_host);
	memset(sav_machine, 0, sizeof(sav_machine));
#if defined(HAVE_GETAUDIT_ADDR)
	(void) aug_get_machine(sav_host, &sav_machine[0], &sav_iptype);
	debug3("BSM audit: sav_iptype=%ld", (long)sav_iptype);
#else
	ia = inet_addr(host);
	memcpy(&sav_machine[0], &ia, sizeof(sav_machine[0]));
	sav_iptype = 0;			/* not used, but just in case */
#endif
	for (i = 0; i < sizeof(sav_machine) / sizeof(sav_machine[0]); i++) {
		debug3("BSM audit: sav_machine[%d]=%08lx",
		    i, (long)sav_machine[i]);
	}
}

void
solaris_audit_save_command(const char *command)
{
	if (cannot_audit(0)) {
		return;
	}
	if (sav_cmd != NULL) {
		free(sav_cmd);
		sav_cmd = NULL;
	}
	sav_cmd = strdup(command);
	debug3("BSM audit: sav_cmd=%s", sav_cmd);
}

void
solaris_audit_save_ttyn(const char *ttyn)
{
	if (cannot_audit(0)) {
		return;
	}
	(void) strlcpy(sav_ttyn, ttyn, sizeof (sav_ttyn));
	debug3("BSM audit: sav_ttyn=%s", sav_ttyn);
}

void
solaris_audit_save_name(const char *name)
{
	if (cannot_audit(0)) {
		return;
	}
	(void) strlcpy(sav_name, name, sizeof (sav_name));
	debug3("BSM audit: sav_name=%s", sav_name);
}

void
solaris_audit_save_pw(struct passwd *pwd)
{
	if (cannot_audit(0)) {
		return;
	}
	if (pwd == NULL) {
		sav_uid = -1;
		sav_gid = -1;
	} else {
		(void) strlcpy(sav_name, pwd->pw_name, sizeof (sav_name));
		sav_uid = pwd->pw_uid;
		sav_gid = pwd->pw_gid;
	}
	debug3("BSM audit: sav_name=%s", sav_name);
	debug3("BSM audit: sav_uid=%ld", (long)sav_uid);
	debug3("BSM audit: sav_gid=%ld", (long)sav_gid);
}

void
solaris_audit_nologin(void)
{
	if (cannot_audit(0)) {
		return;
	}
	solaris_audit_record(1, gettext("logins disabled by /etc/nologin"),
	    AUE_openssh);
}

void
solaris_audit_maxtrys(void)
{
	char    textbuf[BSM_TEXTBUFSZ];

	if (cannot_audit(0)) {
		return;
	}
	(void) snprintf(textbuf, sizeof (textbuf),
		gettext("too many tries for user %s"), sav_name);
	solaris_audit_record(1, textbuf, AUE_openssh);
}

void
solaris_audit_not_console(void)
{
	if (cannot_audit(0)) {
		return;
	}
	solaris_audit_record(2, gettext("not_console"), AUE_openssh);
}

void
solaris_audit_bad_pw(const char *what)
{
	char    textbuf[BSM_TEXTBUFSZ];

	if (cannot_audit(0)) {
		return;
	}
	if (sav_uid == -1) {
		(void) snprintf(textbuf, sizeof (textbuf),
			gettext("invalid user name \"%s\""), sav_name);
		solaris_audit_record(3, textbuf, AUE_openssh);
	} else {
		(void) snprintf(textbuf, sizeof (textbuf),
			gettext("invalid %s for user %s"), what, sav_name);
		solaris_audit_record(4, textbuf, AUE_openssh);
	}
}

void
solaris_audit_success(void)
{
	char    textbuf[BSM_TEXTBUFSZ];

	if (cannot_audit(0)) {
		return;
	}

	solaris_audit_session_setup();
	(void) snprintf(textbuf, sizeof (textbuf),
		gettext("successful login %s"), sav_name);
	solaris_audit_record(0, textbuf, AUE_openssh);
}

static void
solaris_audit_record(int typ, char *string, au_event_t event_no)
{
	int		ad, rc, sel;
	uid_t		uid;
	gid_t		gid;
	pid_t		pid;
	AuditInfoTermID	tid;

	uid = sav_uid;
	gid = sav_gid;
	pid = getpid();

	get_terminal_id(&tid);

	if (typ == 0) {
		rc = 0;
	} else {
		rc = -1;
	}

	sel = selected(sav_name, uid, event_no, rc);
	debug3("BSM audit: typ %d rc %d \"%s\"", typ, rc, string);
	if (!sel)
		return;

	ad = au_open();

	(void) au_write(ad, AUToSubjectFunc(uid, uid, gid, uid, gid,
	    pid, pid, &tid));
	(void) au_write(ad, au_to_text(string));
	if (sav_cmd != NULL) {
		(void) au_write(ad, au_to_text(sav_cmd));
	}
	(void) au_write(ad, AUToReturnFunc(typ, rc));

	rc = au_close(ad, AU_TO_WRITE, event_no);
	if (rc < 0) {
		error("BSM audit: solaris_audit_record failed to write \"%s\" record: %s",
		    string, strerror(errno));
	}
}

static void
solaris_audit_session_setup(void)
{
	int	rc;
	struct AuditInfoStruct info;
	au_mask_t mask;
	struct AuditInfoStruct now;

	info.ai_auid = sav_uid;
	info.ai_asid = getpid();
	mask.am_success = 0;
	mask.am_failure = 0;

	(void) au_user_mask(sav_name, &mask);

	info.ai_mask.am_success  = mask.am_success;
	info.ai_mask.am_failure  = mask.am_failure;

	/* see if terminal id already set */
	if (GetAuditFunc(&now, sizeof (now)) < 0) {
		error("BSM audit: solaris_audit_session_setup: %s failed: %s",
		    GetAuditFuncText, strerror(errno));
	}

	debug("BSM solaris_audit_setup_session: calling get_terminal_id");
	get_terminal_id(&(info.ai_termid));

	rc = SetAuditFunc(&info, sizeof (info));
	if (rc < 0) {
		error("BSM audit: solaris_audit_session_setup: %s failed: %s",
		    SetAuditFuncText, strerror(errno));
	}
}


static void
get_terminal_id(AuditInfoTermID *tid)
{
#if defined(__APPLE__)
	if(kAUBadParamErr == audit_set_terminal_id(tid))
	    debug("BSM get_terminal_id: error");
#elif defined(HAVE_GETAUDIT_ADDR)
	tid->at_port = sav_port;
	tid->at_type = sav_iptype;
	tid->at_addr[0] = sav_machine[0];
	tid->at_addr[1] = sav_machine[1];
	tid->at_addr[2] = sav_machine[2];
	tid->at_addr[3] = sav_machine[3];
#else
	tid->port = sav_port;
	tid->machine = sav_machine[0];
#endif
}

void
solaris_audit_logout(void)
{
	char    textbuf[BSM_TEXTBUFSZ];

	(void) snprintf(textbuf, sizeof (textbuf),
		gettext("sshd logout %s"), sav_name);

	solaris_audit_record(0, textbuf, AUE_logout);
}

static int
selected(char *nam, uid_t uid, au_event_t event, int sf)
{
	int	rc, sorf;
	char	naflags[512];
	struct au_mask mask;

	mask.am_success = mask.am_failure = 0;
	if (uid < 0) {
		rc = getacna(naflags, 256); /* get non-attrib flags */
		if (rc == 0)
			(void) getauditflagsbin(naflags, &mask);
	} else {
		rc = au_user_mask(nam, &mask);
	}

	if (sf == 0) {
		sorf = AU_PRS_SUCCESS;
	} else {
		sorf = AU_PRS_FAILURE;
	}
	rc = au_preselect(event, &mask, sorf, AU_PRS_REREAD);

	return (rc);
}
#endif /* BSM */
