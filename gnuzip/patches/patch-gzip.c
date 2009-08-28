--- gzip.c.orig	2006-12-27 00:00:43.000000000 -0800
+++ gzip.c	2008-08-26 18:06:38.000000000 -0700
@@ -63,6 +63,13 @@
 #include <signal.h>
 #include <sys/stat.h>
 #include <errno.h>
+#ifdef __APPLE__
+#include <sys/attr.h>
+#include <copyfile.h>
+#include <get_compat.h>
+#else
+#define COMPAT_MODE(a,b) (1)
+#endif /* __APPLE__ */
 
 #include "tailor.h"
 #include "gzip.h"
@@ -207,6 +214,7 @@
 char **args = NULL;   /* argv pointer if GZIP env variable defined */
 char *z_suffix;       /* default suffix (can be set with --suffix) */
 size_t z_len;         /* strlen(z_suffix) */
+int zcat;             /* Are we invoked as zcat */
 
 /* The set of signals that are caught.  */
 static sigset_t caught_signals;
@@ -418,6 +426,8 @@
     else if (strequ (program_name + 1, "cat")     /* zcat, pcat, gcat */
 	     || strequ (program_name, "gzcat"))   /* gzcat */
 	decompress = to_stdout = 1;
+
+    zcat = strequ (program_name, "zcat");
 #endif
 
     z_suffix = Z_SUFFIX;
@@ -667,12 +677,35 @@
     }
 }
 
+#ifdef __APPLE__
+static void
+clear_type_and_creator(char *path)
+{
+	struct attrlist alist;
+	struct {
+		u_int32_t length;
+		char info[32];
+	} abuf;
+
+	memset(&alist, 0, sizeof(alist));
+	alist.bitmapcount = ATTR_BIT_MAP_COUNT;
+	alist.commonattr = ATTR_CMN_FNDRINFO;
+
+	if (!getattrlist(path, &alist, &abuf, sizeof(abuf), 0) && abuf.length == sizeof(abuf)) {
+		memset(abuf.info, 0, 8);
+		setattrlist(path, &alist, abuf.info, sizeof(abuf.info), 0);
+	}
+}
+#endif /* __APPLE__ */
+
 /* ========================================================================
  * Compress or decompress the given file
  */
 local void treat_file(iname)
     char *iname;
 {
+    char newname[MAX_PATH_LEN];
+
     /* Accept "-" as synonym for stdin */
     if (strequ(iname, "-")) {
 	int cflag = to_stdout;
@@ -681,6 +714,21 @@
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
@@ -827,6 +875,10 @@
 	sigset_t oldset;
 	int unlink_errno;
 
+#ifdef __APPLE__
+	copyfile(ifname, ofname, 0, COPYFILE_ACL | COPYFILE_XATTR);
+	clear_type_and_creator(ofname);
+#endif
 	copy_stat (&istat);
 	if (close (ofd) != 0)
 	  write_error ();
