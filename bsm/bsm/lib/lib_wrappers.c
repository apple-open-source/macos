/*
 * XXX  lib_wrappers.c is a provisional name 
 */
#include <sys/types.h>	/* [f]stat(), sysctl(), gete[ug]id(), getpid() */
#include <sys/stat.h>	/* [f]stat() */
#include <sys/sysctl.h>	/* sysctl() */
#include <unistd.h>		/* STDIN_FILENO, gete[ug]id(), getpid() */
#include <syslog.h>		/* syslog() */
#include <stdarg.h>		/* syslog() */
#include <errno.h>
#include "libbsm.h"

/*
 * XXX  Write up in a separate white paper.  
 *
Event code		Token type; contents

AUE_create_user		text; "New user [<uid>, <gid>, <shortname>, <longname>]"
(In the following, if the short name is changing, use the old shortname
following "Modify user.")
AUE_modify_user		text; "Modify user <shortname> <UID|GID|SHORTNAME|LONGNAME>: old = <oldval>, new = <newval>"
AUE_modify_password	text; "Modify password for user <shortname>"
AUE_delete_user		text; "Delete user [<uid>, <gid>, <shortname>, <longname>]"
AUE_enable_user		text; ???
AUE_disable_user	text; ???
AUE_create_group	text; "Add group [<gid>, <groupname>]"
AUE_delete_group	text; "Delete group [<gid>, <groupname>]"
(In the following, if the name is changing, use the old name following 
"Modify group.")
AUE_modify_group	text; "Modify group <groupname> <GID|NAME>: old = <oldval>, new = <newval>"
AUE_add_to_group	text; "Add user <shortname> to group <groupname>"
AUE_remove_from_group	text; "Removed user <shortname> from group <groupname>"
AUE_revoke_obj		text; ???


DirectoryServices and netinfod "subatomic" events:
AUE_auth_user		text: "Authenticated user <shortname|UID>"
 */

/* These are not advertised in libbsm.h */
int audit_set_terminal_port(dev_t *p);
int audit_set_terminal_host(u_int32_t *m);

int 
audit_set_terminal_port(dev_t *p)
{
	struct stat st;

	if (p == NULL)
		return kAUBadParamErr;

	*p = NODEV;

	/* for /usr/bin/login, try fstat() first */
	if (fstat(STDIN_FILENO, &st) != 0)
	{
		if (errno != EBADF)
		{
			syslog(LOG_ERR, "fstat() failed (%s)", strerror(errno));
			return kAUStatErr;
		}
		if (stat("/dev/console", &st) != 0)
		{
			syslog(LOG_ERR, "stat() failed (%s)", strerror(errno));
			return kAUStatErr;
		}
	}
	*p = st.st_rdev;
	return kAUNoErr;
}

int 
audit_set_terminal_host(u_int32_t *m)
{
	int name[2] = { CTL_KERN, KERN_HOSTID };
	size_t len;

	if (m == NULL)
		return kAUBadParamErr;
	*m = 0;
	len = sizeof(*m);
	if (sysctl(name, 2, m, &len, NULL, 0) != 0)
	{
		syslog(LOG_ERR, "sysctl() failed (%s)", strerror(errno));
		return kAUSysctlErr;
	}
	return kAUNoErr;
}

int 
audit_set_terminal_id(au_tid_t *tid)
{
	int ret;

	if (tid == NULL)
		return kAUBadParamErr;
	if ((ret = audit_set_terminal_port(&tid->port)) != kAUNoErr)
		return ret;
	return audit_set_terminal_host(&tid->machine);
}


/*
 * This is OK for those callers who have only one token to write.  If you 
 * have multiple tokens that logically form part of the same audit record, 
 * you need to use the existing au_open()/au_write()/au_close() API: 
 *
 * aufd = au_open();
 * tok = au_to_random_token_1(...);
 * au_write(aufd, tok);
 * tok = au_to_random_token_2(...);
 * au_write(aufd, tok);
 * ...
 * au_close(aufd, 1, AUE_your_event_type);
 *
 * Assumes, like all wrapper calls, that the caller has previously checked
 * that auditing is enabled via the audit_get_state() call.  
 *
 * XXX  Should be more robust against bad arguments
 */
int 
audit_write(short event_code, token_t *subject, token_t *misctok, char
	    retval, int errcode)
{
    int aufd;
    char *func = "audit_write()";
    token_t *rettok;

    if ((aufd = au_open()) == -1)
    {
		au_free_token(subject);
		au_free_token(misctok);
		syslog(LOG_ERR, "%s: au_open() failed", func);
		return kAUOpenErr;
    }
    /* save subject */
    if (subject && au_write(aufd, subject) == -1)
    {
		au_free_token(subject);
		au_free_token(misctok);
		(void)au_close(aufd, 0, event_code);
		syslog(LOG_ERR, "%s: write of subject failed", func);
		return kAUWriteSubjectTokErr;
    }
    /* save the event-specific token */
    if (misctok && au_write(aufd, misctok) == -1)
    {
		au_free_token(misctok);
		(void)au_close(aufd, 0, event_code);
		syslog(LOG_ERR, "%s: write of caller token failed", func);
		return kAUWriteCallerTokErr;
    }
    /* tokenize and save the return value */
    if ((rettok = au_to_return32(retval, errcode)) == NULL)
    {
		(void)au_close(aufd, 0, event_code);
		syslog(LOG_ERR, "%s: au_to_return32() failed", func);
		return kAUMakeReturnTokErr;
    }
    if (au_write(aufd, rettok) == -1)
    {
		au_free_token(rettok);
		(void)au_close(aufd, 0, event_code);
		syslog(LOG_ERR, "%s: write of return code failed", func);
		return kAUWriteReturnTokErr;
    }
    /* 
     * au_close()'s second argument is "keep": if keep == 0, the record is
     * discarded.  We assume the caller wouldn't have bothered with this
     * function if it hadn't already decided to keep the record.  
     */
    if (au_close(aufd, 1, event_code) < 0)
    {
		syslog(LOG_ERR, "%s: au_close() failed", func);
		return kAUCloseErr;
    }
    return kAUNoErr;
}

/*
 * Same caveats as audit_write().  In addition, this function explicitly 
 * assumes success; use audit_write_failure() on error.  
 */
int 
audit_write_success(short event_code, token_t *tok, au_id_t auid, 
		    uid_t euid, gid_t egid, uid_t ruid, gid_t rgid, 
		    pid_t pid, au_asid_t sid, au_tid_t *tid)
{
    char *func = "audit_write_success()";
    token_t *subject = NULL;

    /* tokenize and save subject */
    subject = au_to_subject32(auid, euid, egid, ruid, rgid, pid, sid, tid);
    if (subject == NULL)
    {
	syslog(LOG_ERR, "%s: au_to_subject32() failed", func);
	return kAUMakeSubjectTokErr;
    }
    return audit_write(event_code, subject, tok, 0, 0);
}

/*
 * Same caveats as audit_write().  In addition, this function explicitly 
 * assumes success; use audit_write_failure_self() on error.  
 */
int 
audit_write_success_self(short event_code, token_t *tok)
{
    token_t *subject;
    char *func = "audit_write_success_self()";

    if ((subject = au_to_me()) == NULL)
    {
	syslog(LOG_ERR, "%s: au_to_me() failed", func);
	return kAUMakeSubjectTokErr;
    }
    return audit_write(event_code, subject, tok, 0, 0);
}

/*
 * Same caveats as audit_write().  In addition, this function explicitly 
 * assumes failure; use audit_write_success() otherwise.  
 *
 * XXX  This should let the caller pass an error return value rather than
 * hard-coding -1.  
 */
int
audit_write_failure(short event_code, char *errmsg, int errcode, 
		    au_id_t auid, uid_t euid, gid_t egid, uid_t ruid, 
		    gid_t rgid, pid_t pid, au_asid_t sid, au_tid_t *tid)
{
    char *func = "audit_write_failure()";
    token_t *subject, *errtok;

    subject = au_to_subject32(auid, euid, egid, ruid, rgid, pid, sid, tid);
    if (subject == NULL)
    {
	syslog(LOG_ERR, "%s: au_to_subject32() failed", func);
	return kAUMakeSubjectTokErr;
    }
    /* tokenize and save the error message */
    if ((errtok = au_to_text(errmsg)) == NULL)
    {
	syslog(LOG_ERR, "%s: au_to_text() failed", func);
	return kAUMakeTextTokErr;
    }
    return audit_write(event_code, subject, errtok, -1, errcode);
}

/*
 * Same caveats as audit_write().  In addition, this function explicitly 
 * assumes failure; use audit_write_success_self() otherwise.  
 *
 * XXX  This should let the caller pass an error return value rather than
 * hard-coding -1.  
 */
int 
audit_write_failure_self(short event_code, char *errmsg, int errret)
{
    char *func = "audit_write_failure_self()";
    token_t *subject, *errtok;

    if ((subject = au_to_me()) == NULL)
    {
	syslog(LOG_ERR, "%s: au_to_me() failed", func);
	return kAUMakeSubjectTokErr;
    }
    /* tokenize and save the error message */
    if ((errtok = au_to_text(errmsg)) == NULL)
    {
	syslog(LOG_ERR, "%s: au_to_text() failed", func);
	return kAUMakeTextTokErr;
    }
    return audit_write(event_code, subject, errtok, -1, errret);
}

/*
 * For auditing errors during login.  Such errors are implicitly 
 * non-attributable (i.e., not ascribable to any user).  
 *
 * Assumes, like all wrapper calls, that the caller has previously checked
 * that auditing is enabled via the audit_get_state() call.  
 */
int 
audit_write_failure_na(short event_code, char *errmsg, int errret,
		       uid_t euid, uid_t egid, pid_t pid, au_tid_t *tid)
{
    return audit_write_failure(event_code, errmsg, errret, -1, euid, 
			       egid, -1, -1, pid, -1, tid);
}


/* END OF au_write() WRAPPERS */

void 
audit_token_to_au32(
	audit_token_t	atoken,
	uid_t			*auidp,
	uid_t			*euidp,
	gid_t			*egidp,
	uid_t			*ruidp,
	gid_t			*rgidp,
	pid_t			*pidp,
	au_asid_t		*asidp,
	au_tid_t		*tidp)
{
	if (auidp != NULL)
		*auidp = (uid_t)atoken.val[0];
	if (euidp != NULL)
		*euidp = (uid_t)atoken.val[1];
	if (egidp != NULL)
		*egidp = (gid_t)atoken.val[2];
	if (ruidp != NULL)
		*ruidp = (uid_t)atoken.val[3];
	if (rgidp != NULL)
		*rgidp = (gid_t)atoken.val[4];
	if (pidp != NULL)
		*pidp = (pid_t)atoken.val[5];
	if (asidp != NULL)
		*asidp = (au_asid_t)atoken.val[6];
	if (tidp != NULL) {
		audit_set_terminal_host(&tidp->machine);
		tidp->port = (dev_t)atoken.val[7];
	}
}

