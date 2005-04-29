--- main.c.orig	Fri Oct 29 17:03:43 2004
+++ main.c	Fri Oct 29 20:00:02 2004
@@ -78,6 +78,27 @@
 static char *quote_for_env PARAMS ((char *out, char *in));
 static void initialize_global_hash_tables PARAMS ((void));
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+/*
+ * The various Apple, NeXT, and NeXT PDO hacks are no longer enabled by
+ * default, so that default behaves matches GNU make.
+ *
+ * The hacks can be enabled selectively enabling the following options.
+ * They can be enabled by setting the variable USE_APPLE_PB_SUPPORT to one
+ * or more of the options, or by specify "-N <option>" on the command line.
+ *
+ *	all		Turn on all NeXT features.
+ *
+ *	makefiles	DON'T remake Makefiles
+ *
+ *	quiet		Be quiet: warn about using vpath compatibility
+ *			mode or missing targets or overriding implicit rules.
+ *
+ *	vpath		Use the System V vpath compatibility mode.
+ *
+ *	errexit		Use "sh -ec" (instead of "sh -c") to execute rules.
+ */
+#endif
 
 /* The structure that describes an accepted command switch.  */
 
@@ -197,6 +218,13 @@
 
 static struct stringlist *makefiles = 0;
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+/* Apple's hacks are OFF by default, and are enabled by putting the
+   "use-apple-pbhacks-*" directives in the pb_makefiles. */
+unsigned int next_flag = 0;
+static struct stringlist *next_flag_list = 0;
+#endif
+
 /* Number of job slots (commands that can be run at once).  */
 
 unsigned int job_slots = 1;
@@ -325,6 +353,11 @@
                               Consider FILE to be infinitely new.\n"),
     N_("\
   --warn-undefined-variables  Warn when an undefined variable is referenced.\n"),
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+    N_("\
+  -N OPTION, --NeXT-option=OPTION\n\
+                              Turn on value of NeXT OPTION.\n"),
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
     NULL
   };
 
@@ -385,6 +418,9 @@
     { 'W', string, (char *) &new_files, 0, 0, 0, 0, 0, "what-if" },
     { CHAR_MAX+4, flag, (char *) &warn_undefined_variables_flag, 1, 1, 0, 0, 0,
 	"warn-undefined-variables" },
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+    { 'N', string, (char *) &next_flag_list, 0, 0, 0, 0, 0, "NeXT-option" },
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
     { '\0', }
   };
 
@@ -1106,6 +1142,27 @@
   decode_env_switches ("MFLAGS", 6);
 #endif
   decode_switches (argc, argv, 0);
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+  if (next_flag_list != 0) {
+        char **p;
+      next_flag = 0;
+      for (p = next_flag_list->list; *p != 0; p++) {
+	  if (strcmp(*p, "vpath") == 0) {
+	      next_flag |= NEXT_VPATH_FLAG;
+	  } else if (strcmp(*p, "quiet") == 0) {
+	      next_flag |= NEXT_QUIET_FLAG;
+	  } else if (strcmp(*p, "makefiles") == 0) {
+	      next_flag |= NEXT_MAKEFILES_FLAG;
+	  } else if (strcmp(*p, "errexit") == 0) {
+	      next_flag |= NEXT_ERREXIT_FLAG;
+	  } else if (strcmp(*p, "all") == 0) {
+	      next_flag = NEXT_ALL_FLAGS;
+	  } else {
+	      error ("Unrecognized flag `%s'.", *p);
+	  }
+      }
+  }
+#endif	/* __APPLE__ || NeXT || NeXT_PDO */
 #ifdef WINDOWS32
   if (suspend_flag) {
         fprintf(stderr, "%s (pid = %d)\n", argv[0], GetCurrentProcessId());
@@ -1262,9 +1319,11 @@
       makelevel = 0;
   }
 
+#if !(defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO))
   /* Except under -s, always do -w in sub-makes and under -C.  */
   if (!silent_flag && (directories != 0 || makelevel > 0))
     print_directory_flag = 1;
+#endif
 
   /* Let the user disable that with --no-print-directory.  */
   if (inhibit_print_directory_flag)
@@ -1639,6 +1698,9 @@
   remote_setup ();
 
   if (read_makefiles != 0)
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+      if (!(next_flag & NEXT_MAKEFILES_FLAG))
+#endif
     {
       /* Update any makefiles if necessary.  */
 
