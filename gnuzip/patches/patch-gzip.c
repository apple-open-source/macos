--- gzip.c.orig	Sat Sep 28 03:38:43 2002
+++ gzip.c	Tue Feb 22 18:49:52 2005
@@ -47,6 +47,12 @@
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
@@ -242,6 +248,7 @@
 char **args = NULL;   /* argv pointer if GZIP env variable defined */
 char *z_suffix;       /* default suffix (can be set with --suffix) */
 size_t z_len;         /* strlen(z_suffix) */
+int zcat;             /* Are we invoked as zcat */
 
 off_t bytes_in;             /* number of input bytes */
 off_t bytes_out;            /* number of output bytes */
@@ -492,6 +499,8 @@
 	    || strequ(progname, "gzcat")) {    /* gzcat */
 	decompress = to_stdout = 1;
     }
+
+    zcat = strequ(progname, "zcat");
 #endif
 
     z_suffix = Z_SUFFIX;
@@ -757,6 +766,8 @@
 local void treat_file(iname)
     char *iname;
 {
+    char newname[MAX_PATH_LEN];
+
     /* Accept "-" as synonym for stdin */
     if (strequ(iname, "-")) {
 	int cflag = to_stdout;
@@ -765,6 +776,21 @@
 	return;
     }
 
+    /* POSIX zcat must add .Z to all files if not present when decompressing */
+    if (zcat && COMPAT_MODE("bin/zcat", "Unix2003")) {
+	char *suffix;
+	if ((suffix = strrchr(iname, '.')) == NULL ||
+	    strcmp(suffix, ".Z")) {
+	    if (strlen(iname) > sizeof(newname) - 3) {
+		WARN((stderr, "%s: %s too long to append .Z\n", progname, iname));
+	    } else {
+		strcpy(newname, iname);
+		strcat(newname, ".Z");
+		iname = newname;
+	    }
+	}
+    }
+
     /* Check if the input file is present, set ifname and istat: */
     if (get_istat(iname, &istat) != OK) return;
 
@@ -878,6 +904,13 @@
     if (!to_stdout && close(ofd)) {
 	write_error();
     }
+
+#if __APPLE__
+    if (!to_stdout) {
+	copyfile(ifname, ofname, 0, COPYFILE_ACL | COPYFILE_XATTR);
+    }
+#endif
+
     if (method == -1) {
 	if (!to_stdout) xunlink (ofname);
 	return;
