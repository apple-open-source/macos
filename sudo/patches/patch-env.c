--- env.c.orig	Wed Dec  1 05:57:02 2004
+++ env.c	Wed Dec  1 05:57:56 2004
@@ -499,7 +499,7 @@
      * http://www.fortran-2000.com/ArnaudRecipes/sharedlib.html
      * XXX - should prepend to original value, if any
      */
-    if (noexec && def_noexec_file != NULL)
+    if (noexec && def_noexec_file != NULL) {
 #if defined(__darwin__) || defined(__APPLE__)
 	insert_env(format_env("DYLD_INSERT_LIBRARIES", def_noexec_file, VNULL), 1);
 	insert_env(format_env("DYLD_FORCE_FLAT_NAMESPACE", VNULL), 1);
@@ -510,6 +510,7 @@
 	insert_env(format_env("LD_PRELOAD", def_noexec_file, VNULL), 1);
 # endif
 #endif
+    }
 
     /* Set PS1 if SUDO_PS1 is set. */
     if (ps1)
