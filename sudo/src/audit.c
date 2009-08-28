
#include <config.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_FNMATCH
# include <fnmatch.h>
#endif /* HAVE_FNMATCH */
#ifdef HAVE_NETGROUP_H
# include <netgroup.h>
#endif /* HAVE_NETGROUP_H */
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


#include "audit.h"
#include <sys/errno.h>
#include <bsm/libbsm.h>
#include <bsm/audit_uevents.h>


/*
 * Include the following tokens in the audit record for successful sudo operations
 * header
 * subject
 * return
 */
void audit_success(struct passwd *pwd)
{
	int aufd;
	token_t *tok;
	auditinfo_addr_t auinfo;
	long au_cond;
	uid_t uid = pwd->pw_uid;
	gid_t gid = pwd->pw_gid;
	pid_t pid = getpid();

	/* If we are not auditing, don't cut an audit record; just return */
	if (auditon(A_GETCOND, &au_cond, sizeof(long)) < 0) {
		fprintf(stderr, "sudo: Could not determine audit condition\n");
		exit(1);
	}
	if (au_cond == AUC_NOAUDIT)
		return;

	if(getaudit_addr(&auinfo, sizeof(auinfo)) != 0) {
		fprintf(stderr, "sudo: getaudit_addr failed:  %s\n", strerror(errno));
		exit(1);
	}

	if((aufd = au_open()) == -1) {
		fprintf(stderr, "sudo: Audit Error: au_open() failed\n");
		exit(1);      
	}

	/* subject token represents the subject being created */
	if((tok = au_to_subject32_ex(auinfo.ai_auid, geteuid(), getegid(), 
			uid, gid, pid, auinfo.ai_asid, &auinfo.ai_termid)) == NULL) {
		fprintf(stderr, "sudo: Audit Error: au_to_subject32_ex() failed\n");
		exit(1);      
	}
	au_write(aufd, tok);

	if((tok = au_to_return32(0, 0)) == NULL) {
		fprintf(stderr, "sudo: Audit Error: au_to_return32() failed\n");
		exit(1);
	}
	au_write(aufd, tok);

	if(au_close(aufd, 1, AUE_sudo) == -1) {
		fprintf(stderr, "sudo: Audit Error: au_close() failed\n");
		exit(1);
	}
	return; 
}

/*
 * Include the following tokens in the audit record for failed sudo operations
 * header
 * subject
 * text
 * return
 */
void audit_fail(struct passwd *pwd, char *errmsg)
{
	int aufd;
	token_t *tok;
	auditinfo_addr_t auinfo;
	long au_cond;
	uid_t uid = pwd ? pwd->pw_uid : -1;
	gid_t gid = pwd ? pwd->pw_gid : -1;
	pid_t pid = getpid();

	/* If we are not auditing, don't cut an audit record; just return */
	if (auditon(A_GETCOND, &au_cond, sizeof(long)) < 0) {
		fprintf(stderr, "sudo: Could not determine audit condition\n");
		exit(1);
	}
	if (au_cond == AUC_NOAUDIT)
		return;

	if(getaudit_addr(&auinfo, sizeof(auinfo)) != 0) {
		fprintf(stderr, "sudo: getaudit failed:  %s\n", strerror(errno));
		exit(1);
	}

	if((aufd = au_open()) == -1) {
		fprintf(stderr, "sudo: Audit Error: au_open() failed\n");
		exit(1);      
	}

	/* subject token corresponds to the subject being created, or -1 if non attributable */
	if((tok = au_to_subject32(auinfo.ai_auid, geteuid(), getegid(), 
			uid, gid, pid, auinfo.ai_asid, &auinfo.ai_termid)) == NULL) {
		fprintf(stderr, "sudo: Audit Error: au_to_subject32() failed\n");
		exit(1);      
	}
	au_write(aufd, tok);

	if((tok = au_to_text(errmsg)) == NULL) {
		fprintf(stderr, "sudo: Audit Error: au_to_text() failed\n");
		exit(1);
	}
	au_write(aufd, tok);

	if((tok = au_to_return32(1, errno)) == NULL) {
		fprintf(stderr, "sudo: Audit Error: au_to_return32() failed\n");
		exit(1);
	}
	au_write(aufd, tok);

	if(au_close(aufd, 1, AUE_sudo) == -1) {
		fprintf(stderr, "sudo: Audit Error: au_close() failed\n");
		exit(1);
	}
	return;
}
