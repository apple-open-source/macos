--- sudo.c.orig	Tue Aug 24 14:01:13 2004
+++ sudo.c	Wed Dec  1 05:57:29 2004
@@ -88,6 +88,9 @@
 # endif
 #endif
 
+#include <bsm/libbsm.h>
+#include <bsm/audit_uevents.h>
+
 #include "sudo.h"
 #include "interfaces.h"
 #include "version.h"
@@ -115,6 +118,9 @@
 extern struct passwd *sudo_getpwuid	__P((uid_t));
 extern struct passwd *sudo_pwdup	__P((const struct passwd *));
 
+void audit_success(struct passwd *pwd);
+void audit_fail(struct passwd *pwd, char *errmsg);
+
 /*
  * Globals
  */
@@ -286,6 +292,7 @@
     if (!def_stay_setuid && set_perms == set_perms_posix) {
 	if (setuid(0)) {
 	    perror("setuid(0)");
+	    audit_fail(NULL, "setuid failure");
 	    exit(1);
 	}
 	set_perms = set_perms_nosuid;
@@ -311,18 +318,22 @@
     /* This goes after the sudoers parse since we honor sudoers options. */
     if (sudo_mode == MODE_KILL || sudo_mode == MODE_INVALIDATE) {
 	remove_timestamp((sudo_mode == MODE_KILL));
+	/* no need to audit this event */
 	exit(0);
     }
 
-    if (ISSET(validated, VALIDATE_ERROR))
+    if (ISSET(validated, VALIDATE_ERROR)) {
+	/* no need to audit this event */
 	log_error(0, "parse error in %s near line %d", _PATH_SUDOERS,
 	    errorlineno);
+    }
 
     /* Is root even allowed to run sudo? */
     if (user_uid == 0 && !def_root_sudo) {
 	(void) fprintf(stderr,
 	    "Sorry, %s has been configured to not allow root to run it.\n",
 	    getprogname());
+	/* no need to audit this event */
 	exit(1);
     }
 
@@ -341,8 +352,10 @@
 
     /* Bail if a tty is required and we don't have one.  */
     if (def_requiretty) {
-	if ((fd = open(_PATH_TTY, O_RDWR|O_NOCTTY)) == -1)
+	if ((fd = open(_PATH_TTY, O_RDWR|O_NOCTTY)) == -1) {
+	    audit_fail(runas_pw, "no tty");
 	    log_error(NO_MAIL, "sorry, you must have a tty to run sudo");
+	}
 	else
 	    (void) close(fd);
     }
@@ -374,31 +387,40 @@
 	/* Finally tell the user if the command did not exist. */
 	if (cmnd_status == NOT_FOUND_DOT) {
 	    warnx("ignoring `%s' found in '.'\nUse `sudo ./%s' if this is the `%s' you wish to run.", user_cmnd, user_cmnd, user_cmnd);
+	    audit_fail(runas_pw, "command in current dir");
 	    exit(1);
 	} else if (cmnd_status == NOT_FOUND) {
 	    warnx("%s: command not found", user_cmnd);
+	    audit_fail(runas_pw, "Command not found");
 	    exit(1);
 	}
 
 	log_auth(validated, 1);
-	if (sudo_mode == MODE_VALIDATE)
+	if (sudo_mode == MODE_VALIDATE) {
+	    audit_success(runas_pw);
 	    exit(0);
+	}
 	else if (sudo_mode == MODE_LIST) {
 	    list_matches();
 #ifdef HAVE_LDAP
 	    sudo_ldap_list_matches();
 #endif
+	    audit_success(runas_pw);
 	    exit(0);
 	}
 
 	/* This *must* have been set if we got a match but... */
 	if (safe_cmnd == NULL) {
+	    audit_fail(runas_pw, "internal error - safe_cmnd");
 	    log_error(MSG_ONLY,
 		"internal error, safe_cmnd never got set for %s; %s",
 		user_cmnd,
 		"please report this error at http://courtesan.com/sudo/bugs/");
 	}
 
+	/* sudo operation succeeded */
+	audit_success(runas_pw);
+
 	/* Override user's umask if configured to do so. */
 	if (def_umask != 0777)
 	    (void) umask(def_umask);
@@ -457,6 +479,7 @@
 	exit(127);
     } else if (ISSET(validated, FLAG_NO_USER) || (validated & FLAG_NO_HOST)) {
 	log_auth(validated, 1);
+	audit_fail(runas_pw, "No user or host");
 	exit(1);
     } else if (ISSET(validated, VALIDATE_NOT_OK)) {
 	if (def_path_info) {
@@ -477,10 +500,12 @@
 	    /* Just tell the user they are not allowed to run foo. */
 	    log_auth(validated, 1);
 	}
+	audit_fail(runas_pw, "Validation error");
 	exit(1);
     } else {
 	/* should never get here */
 	log_auth(validated, 1);
+	audit_fail(runas_pw, "Validation error");
 	exit(1);
     }
     exit(0);	/* not reached */
@@ -498,8 +523,10 @@
     int nohostname, rval;
 
     /* Sanity check command from user. */
-    if (user_cmnd == NULL && strlen(NewArgv[0]) >= PATH_MAX)
+    if (user_cmnd == NULL && strlen(NewArgv[0]) >= PATH_MAX) {
+	audit_fail(NULL, "pathname too long");
 	errx(1, "%s: File name too long", NewArgv[0]);
+    }
 
 #ifdef HAVE_TZSET
     (void) tzset();		/* set the timezone if applicable */
@@ -609,8 +636,10 @@
 	    NewArgv[0] = runas_pw->pw_shell;
 	else if (user_shell && *user_shell)
 	    NewArgv[0] = user_shell;
-	else
+	else {
+	    audit_fail(NULL, "unable to determine shell");
 	    errx(1, "unable to determine shell");
+	}
 
 	/* copy the args from NewArgv */
 	for (dst = NewArgv + 1; (*dst = *src) != NULL; ++src, ++dst)
@@ -1050,8 +1079,10 @@
 	}
     } else {
 	runas_pw = sudo_getpwnam(user);
-	if (runas_pw == NULL)
+	if (runas_pw == NULL) {
+	    audit_fail(NULL, "no passwd entry for user");
 	    log_error(NO_MAIL|MSG_ONLY, "no passwd entry for %s!", user);
+	}
     }
     return(TRUE);
 }
@@ -1069,18 +1100,24 @@
     if (def_rootpw) {
 	if (runas_pw->pw_uid == 0)
 	    pw = runas_pw;
-	else if ((pw = sudo_getpwuid(0)) == NULL)
+	else if ((pw = sudo_getpwuid(0)) == NULL) {
+	    audit_fail(NULL, "user does not exist in passwd file");
 	    log_error(0, "uid 0 does not exist in the passwd file!");
+	}
     } else if (def_runaspw) {
 	if (strcmp(def_runas_default, *user_runas) == 0)
 	    pw = runas_pw;
-	else if ((pw = sudo_getpwnam(def_runas_default)) == NULL)
+	else if ((pw = sudo_getpwnam(def_runas_default)) == NULL) {
+	    audit_fail(NULL, "user does not exist in passwd file");
 	    log_error(0, "user %s does not exist in the passwd file!",
 		def_runas_default);
+	}
     } else if (def_targetpw) {
-	if (runas_pw->pw_name == NULL)
+	if (runas_pw->pw_name == NULL) {
+	    audit_fail(NULL, "uid does not exist in passwd file");
 	    log_error(NO_MAIL|MSG_ONLY, "no passwd entry for %lu!",
 		runas_pw->pw_uid);
+	}
 	pw = runas_pw;
     } else
 	pw = sudo_user.pw;
@@ -1155,4 +1192,121 @@
     }
     putchar('\n');
     exit(exit_val);
+}
+
+/*
+ * Include the following tokens in the audit record for successful sudo operations
+ * header
+ * subject
+ * return
+ */
+void audit_success(struct passwd *pwd)
+{
+	int aufd;
+	token_t *tok;
+	auditinfo_t auinfo;
+	long au_cond;
+	uid_t uid = pwd->pw_uid;
+	gid_t gid = pwd->pw_gid;
+	pid_t pid = getpid();
+
+	/* If we are not auditing, don't cut an audit record; just return */
+	if (auditon(A_GETCOND, &au_cond, sizeof(long)) < 0) {
+		fprintf(stderr, "sudo: Could not determine audit condition\n");
+		exit(1);
+	}
+	if (au_cond == AUC_NOAUDIT)
+		return;
+
+	if(getaudit(&auinfo) != 0) {
+		fprintf(stderr, "sudo: getaudit failed:  %s\n", strerror(errno));
+		exit(1);
+	}
+
+	if((aufd = au_open()) == -1) {
+		fprintf(stderr, "sudo: Audit Error: au_open() failed\n");
+		exit(1);      
+	}
+
+	/* subject token represents the subject being created */
+	if((tok = au_to_subject32(auinfo.ai_auid, geteuid(), getegid(), 
+			uid, gid, pid, auinfo.ai_asid, &auinfo.ai_termid)) == NULL) {
+		fprintf(stderr, "sudo: Audit Error: au_to_subject32() failed\n");
+		exit(1);      
+	}
+	au_write(aufd, tok);
+
+	if((tok = au_to_return32(0, 0)) == NULL) {
+		fprintf(stderr, "sudo: Audit Error: au_to_return32() failed\n");
+		exit(1);
+	}
+	au_write(aufd, tok);
+
+	if(au_close(aufd, 1, AUE_sudo) == -1) {
+		fprintf(stderr, "sudo: Audit Error: au_close() failed\n");
+		exit(1);
+	}
+	return; 
+}
+
+/*
+ * Include the following tokens in the audit record for failed sudo operations
+ * header
+ * subject
+ * text
+ * return
+ */
+void audit_fail(struct passwd *pwd, char *errmsg)
+{
+	int aufd;
+	token_t *tok;
+	auditinfo_t auinfo;
+	long au_cond;
+	uid_t uid = pwd ? pwd->pw_uid : -1;
+	gid_t gid = pwd ? pwd->pw_gid : -1;
+	pid_t pid = getpid();
+
+	/* If we are not auditing, don't cut an audit record; just return */
+	if (auditon(A_GETCOND, &au_cond, sizeof(long)) < 0) {
+		fprintf(stderr, "sudo: Could not determine audit condition\n");
+		exit(1);
+	}
+	if (au_cond == AUC_NOAUDIT)
+		return;
+
+	if(getaudit(&auinfo) != 0) {
+		fprintf(stderr, "sudo: getaudit failed:  %s\n", strerror(errno));
+		exit(1);
+	}
+
+	if((aufd = au_open()) == -1) {
+		fprintf(stderr, "sudo: Audit Error: au_open() failed\n");
+		exit(1);      
+	}
+
+	/* subject token corresponds to the subject being created, or -1 if non attributable */
+	if((tok = au_to_subject32(auinfo.ai_auid, geteuid(), getegid(), 
+			uid, gid, pid, auinfo.ai_asid, &auinfo.ai_termid)) == NULL) {
+		fprintf(stderr, "sudo: Audit Error: au_to_subject32() failed\n");
+		exit(1);      
+	}
+	au_write(aufd, tok);
+
+	if((tok = au_to_text(errmsg)) == NULL) {
+		fprintf(stderr, "sudo: Audit Error: au_to_text() failed\n");
+		exit(1);
+	}
+	au_write(aufd, tok);
+
+	if((tok = au_to_return32(1, errno)) == NULL) {
+		fprintf(stderr, "sudo: Audit Error: au_to_return32() failed\n");
+		exit(1);
+	}
+	au_write(aufd, tok);
+
+	if(au_close(aufd, 1, AUE_sudo) == -1) {
+		fprintf(stderr, "sudo: Audit Error: au_close() failed\n");
+		exit(1);
+	}
+	return;
 }
