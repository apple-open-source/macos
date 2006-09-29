--- gzip.c.orig	2005-10-11 17:02:42.000000000 -0700
+++ gzip.c	2005-10-11 17:05:28.000000000 -0700
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
 
@@ -875,8 +901,14 @@
     }
 
     close(ifd);
-    if (!to_stdout && close(ofd)) {
-	write_error();
+    if (!to_stdout) {
+#if __APPLE__
+	copyfile(ifname, ofname, 0, COPYFILE_ACL | COPYFILE_XATTR);
+#endif
+	/* Copy modes, times, ownership, and remove the input file */
+	copy_stat(&istat);
+	if (close(ofd))
+	    write_error();
     }
     if (method == -1) {
 	if (!to_stdout) xunlink (ofname);
@@ -896,10 +928,6 @@
 	}
 	fprintf(stderr, "\n");
     }
-    /* Copy modes, times, ownership, and remove the input file */
-    if (!to_stdout) {
-	copy_stat(&istat);
-    }
 }
 
 /* ========================================================================
@@ -1317,6 +1345,7 @@
 		/* Copy the base name. Keep a directory prefix intact. */
                 char *p = base_name (ofname);
                 char *base = p;
+		char *base2;
 		for (;;) {
 		    *p = (char)get_char();
 		    if (*p++ == '\0') break;
@@ -1324,6 +1353,8 @@
 			error("corrupted input -- file name too large");
 		    }
 		}
+		base2 = basename (base);
+		strcpy(base, base2);
                 /* If necessary, adapt the name to local OS conventions: */
                 if (!list) {
                    MAKE_LEGAL_NAME(base);
@@ -1725,7 +1756,7 @@
     reset_times(ofname, ifstat);
 #endif
     /* Copy the protection modes */
-    if (chmod(ofname, ifstat->st_mode & 07777)) {
+    if (fchmod(ofd, ifstat->st_mode & 07777)) {
 	int e = errno;
 	WARN((stderr, "%s: ", progname));
 	if (!quiet) {
@@ -1734,7 +1765,7 @@
 	}
     }
 #ifndef NO_CHOWN
-    chown(ofname, ifstat->st_uid, ifstat->st_gid);  /* Copy ownership */
+    (void) fchown(ofd, ifstat->st_uid, ifstat->st_gid);  /* Copy ownership */
 #endif
     remove_ofname = 0;
     /* It's now safe to remove the input file: */
