--- job.c.orig	Fri Oct 29 17:19:01 2004
+++ job.c	Fri Oct 29 17:23:35 2004
@@ -1011,8 +1011,16 @@
 #else
       (argv[0] && !strcmp (argv[0], "/bin/sh"))
 #endif
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+      /* allow either -ec or -c */
+      && ((argv[1]
+	   && argv[1][0] == '-' && argv[1][1] == 'c' && argv[1][2] == '\0') ||
+          (argv[1]
+	   && argv[1][0] == '-' && argv[1][1] == 'e' && argv[1][2] == 'c' && argv[1][3] == '\0'))
+#else
       && (argv[1]
           && argv[1][0] == '-' && argv[1][1] == 'c' && argv[1][2] == '\0')
+#endif __APPLE__ || NeXT || NeXT_PDO
       && (argv[2] && argv[2][0] == ':' && argv[2][1] == '\0')
       && argv[3] == NULL)
     {
@@ -1464,6 +1472,19 @@
 						     file);
     }
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO) /* for NEXT_VPATH_FLAG support */
+  if (next_flag & NEXT_VPATH_FLAG) {
+      for (i = 0; i < cmds->ncommand_lines; ++i) {
+	  char *line;
+	  if (lines[i] != 0) {
+	      line = allocated_vpath_expand_for_file (lines[i], file);
+	      free (lines[i]);
+	      lines[i] = line;
+	  }
+      }
+  }
+#endif	/* __APPLE__ || NeXT || NeXT_PDO */
+
   /* Start the command sequence, record it in a new
      `struct child', and add that to the chain.  */
 
@@ -2848,22 +2869,39 @@
        argument list.  */
 
     unsigned int shell_len = strlen (shell);
+    unsigned int line_len = strlen (line);
+    char *new_line;
+    char *command_ptr = NULL; /* used for batch_mode_shell mode */
+
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+    char *minus_c;
+    int minus_c_len;
+
+    if (next_flag & NEXT_ERREXIT_FLAG) {
+      minus_c = " -ec ";
+      minus_c_len = 5;
+    } else {
+      minus_c = " -c ";
+      minus_c_len = 4;
+    }
+#else
 #ifndef VMS
     static char minus_c[] = " -c ";
+    int minus_c_len = 4;
 #else
     static char minus_c[] = "";
+    int minus_c_len = 0;
 #endif
-    unsigned int line_len = strlen (line);
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
 
-    char *new_line = (char *) alloca (shell_len + (sizeof (minus_c) - 1)
-				      + (line_len * 2) + 1);
-    char *command_ptr = NULL; /* used for batch_mode_shell mode */
+    new_line = (char *) alloca (shell_len + minus_c_len
+				+ (line_len * 2) + 1);
 
     ap = new_line;
     bcopy (shell, ap, shell_len);
     ap += shell_len;
-    bcopy (minus_c, ap, sizeof (minus_c) - 1);
-    ap += sizeof (minus_c) - 1;
+    bcopy (minus_c, ap, minus_c_len);
+    ap += minus_c_len;
     command_ptr = ap;
     for (p = line; *p != '\0'; ++p)
       {
@@ -2911,7 +2949,7 @@
 #endif
 	*ap++ = *p;
       }
-    if (ap == new_line + shell_len + sizeof (minus_c) - 1)
+    if (ap == new_line + shell_len + minus_c_len)
       /* Line was empty.  */
       return 0;
     *ap = '\0';
@@ -2979,10 +3017,10 @@
          instead of recursively calling ourselves, because we
          cannot backslash-escape the special characters (see above).  */
       new_argv = (char **) xmalloc (sizeof (char *));
-      line_len = strlen (new_line) - shell_len - sizeof (minus_c) + 1;
+      line_len = strlen (new_line) - shell_len - minus_c_len;
       new_argv[0] = xmalloc (line_len + 1);
       strncpy (new_argv[0],
-               new_line + shell_len + sizeof (minus_c) - 1, line_len);
+               new_line + shell_len + minus_c_len, line_len);
       new_argv[0][line_len] = '\0';
       }
 #else
