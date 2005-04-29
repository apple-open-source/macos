--- auth/sudo_auth.c.orig	Fri Oct 15 22:34:13 2004
+++ auth/sudo_auth.c	Fri Oct 15 22:37:43 2004
@@ -110,11 +110,13 @@
     (void) sigaction(SIGTSTP, &sa, &osa);
 
     /* Make sure we have at least one auth method. */
-    if (auth_switch[0].name == NULL)
+    if (auth_switch[0].name == NULL) {
+    	audit_fail(pw, "No authentication methods");
     	log_error(0, "%s  %s %s",
 	    "There are no authentication methods compiled into sudo!",
 	    "If you want to turn off authentication, use the",
 	    "--disable-authentication configure option.");
+    }
 
     /* Set FLAG_ONEANDONLY if there is only one auth method. */
     if (auth_switch[1].name == NULL)
@@ -129,8 +131,10 @@
 	    status = (auth->init)(pw, &prompt, auth);
 	    if (status == AUTH_FAILURE)
 		CLR(auth->flags, FLAG_CONFIGURED);
-	    else if (status == AUTH_FATAL)	/* XXX log */
+	    else if (status == AUTH_FATAL)	/* XXX log */ {
+		audit_fail(pw, "Auth Failure");
 		exit(1);		/* assume error msg already printed */
+	    }
 
 	    if (NEEDS_USER(auth))
 		set_perms(PERM_ROOT);
@@ -147,8 +151,10 @@
 		status = (auth->setup)(pw, &prompt, auth);
 		if (status == AUTH_FAILURE)
 		    CLR(auth->flags, FLAG_CONFIGURED);
-		else if (status == AUTH_FATAL)	/* XXX log */
+		else if (status == AUTH_FATAL)	/* XXX log */ {
+		    audit_fail(pw, "Auth Failure");
 		    exit(1);		/* assume error msg already printed */
+		}
 
 		if (NEEDS_USER(auth))
 		    set_perms(PERM_ROOT);
@@ -189,8 +195,10 @@
 
 	/* Exit loop on nil password, but give it a chance to match first. */
 	if (nil_pw) {
-	    if (counter == def_passwd_tries)
+	    if (counter == def_passwd_tries) {
+		audit_fail(pw, "password attempt limit reached");
 		exit(1);
+	    }
 	    else
 		break;
 	}
@@ -206,8 +214,10 @@
 		set_perms(PERM_USER);
 
 	    status = (auth->cleanup)(pw, auth);
-	    if (status == AUTH_FATAL)	/* XXX log */
+	    if (status == AUTH_FATAL)	/* XXX log */ {
+		audit_fail(pw, "Auth Failure");
 		exit(1);		/* assume error msg already printed */
+	    }
 
 	    if (NEEDS_USER(auth))
 		set_perms(PERM_ROOT);
@@ -223,10 +233,12 @@
 		flags = 0;
 	    else
 		flags = NO_MAIL;
+	    audit_fail(pw, "Incorrect password");
 	    log_error(flags, "%d incorrect password attempt%s",
 		def_passwd_tries - counter,
 		(def_passwd_tries - counter == 1) ? "" : "s");
 	case AUTH_FATAL:
+	    audit_fail(pw, "Auth failure");
 	    exit(1);
     }
     /* NOTREACHED */
