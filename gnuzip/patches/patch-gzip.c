--- gzip.c.orig	2005-10-11 17:02:42.000000000 -0700
+++ gzip.c	2005-10-11 17:05:28.000000000 -0700
@@ -63,6 +63,12 @@
 #include <signal.h>
 #include <sys/stat.h>
 #include <errno.h>
+#ifdef __APPLE__
+#include <copyfile.h>
+#include <get_compat.h>
+#else
+#define COMPAT_MODE(a,b) (1)
+#endif /* __APPLE__ */
 
 #include "tailor.h"
 #include "gzip.h"
@@ -214,6 +220,7 @@
 char **args = NULL;   /* argv pointer if GZIP env variable defined */
 char *z_suffix;       /* default suffix (can be set with --suffix) */
 size_t z_len;         /* strlen(z_suffix) */
+int zcat;             /* Are we invoked as zcat */
 
 /* The set of signals that are caught.  */
 static sigset_t caught_signals;
@@ -422,6 +429,8 @@
     else if (strequ (program_name + 1, "cat")     /* zcat, pcat, gcat */
 	     || strequ (program_name, "gzcat"))   /* gzcat */
 	decompress = to_stdout = 1;
+
+    zcat = strequ (program_name, "zcat");
 #endif
 
     z_suffix = Z_SUFFIX;
@@ -677,6 +686,8 @@
 local void treat_file(iname)
     char *iname;
 {
+    char newname[MAX_PATH_LEN];
+
     /* Accept "-" as synonym for stdin */
     if (strequ(iname, "-")) {
 	int cflag = to_stdout;
@@ -685,6 +696,21 @@
 	return;
     }
 
+    /* POSIX zcat must add .Z to all files if not present when decompressing */
+    if (zcat && COMPAT_MODE("bin/zcat", "Unix2003")) {
+	char *suffix;
+	if ((suffix = strrchr(iname, '.')) == NULL ||
+	    strcmp(suffix, ".Z")) {
+	    if (strlen(iname) > sizeof(newname) - 3) {
+		WARN((stderr, "%s: %s too long to append .Z\n", program_name, iname));
+	    } else {
+		strcpy(newname, iname);
+		strcat(newname, ".Z");
+		iname = newname;
+	    }
+	}
+    }
+
     /* Check if the input file is present, set ifname and istat: */
     ifd = open_input_file (iname, &istat);
     if (ifd < 0)
@@ -831,6 +857,9 @@
 	sigset_t oldset;
 	int unlink_errno;
 
+#ifdef __APPLE__
+	copyfile(ifname, ofname, 0, COPYFILE_ACL | COPYFILE_XATTR);
+#endif
 	copy_stat (&istat);
 	if (close (ofd) != 0)
 	  write_error ();

