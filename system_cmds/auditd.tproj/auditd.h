#ifndef _AUDITD_H_
#define _AUDITD_H_

#include <sys/types.h>
#include <sys/queue.h>
#include <syslog.h>

#define MAX_DIR_SIZE 255
#define AUDITD_NAME    "auditd"

#define POSTFIX_LEN		16
#define NOT_TERMINATED	".not_terminated" 

struct dir_ent {
	char *dirname;
	char softlim;
	TAILQ_ENTRY(dir_ent) dirs;
};

/* audit utility flags */
#define OPEN_NEW		0x1
#define READ_FILE		0x2
#define CLOSE_AND_DIE 	0x4

#define HARDLIM_ALL_WARN        "allhard"
#define SOFTLIM_ALL_WARN        "allsoft"
#define AUDITOFF_WARN           "aditoff"
#define EBUSY_WARN              "ebusy"
#define GETACDIR_WARN           "getacdir"
#define HARDLIM_WARN            "hard"
#define NOSTART_WARN            "nostart"
#define POSTSIGTERM_WARN        "postsigterm"
#define SOFTLIM_WARN            "soft"
#define TMPFILE_WARN            "tmpfile"

#define AUDITWARN_SCRIPT        "/etc/security/audit_warn"
#define AUDITD_PIDFILE		"/var/run/auditd.pid"

int audit_warn_allhard(int count);
int audit_warn_allsoft();
int audit_warn_auditoff();
int audit_warn_ebusy();
int audit_warn_getacdir(char *filename);
int audit_warn_hard(char *filename);
int audit_warn_nostart();
int audit_warn_postsigterm();
int audit_warn_soft(char *filename);
int audit_warn_tmpfile();

#endif /* !_AUDITD_H_ */

