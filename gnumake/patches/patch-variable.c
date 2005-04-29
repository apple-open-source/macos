--- variable.c.orig	Fri Oct 29 16:30:02 2004
+++ variable.c	Fri Oct 29 16:33:36 2004
@@ -78,6 +78,40 @@
 
 /* Implement variables.  */
 
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+static void check_apple_pb_support (name, length, value)
+     char *name;
+     unsigned int length;
+     char *value;
+{
+  char *p;
+
+  if (length == 20 && !strncmp (name, "USE_APPLE_PB_SUPPORT", length)) {
+    for (p = value; *p != '\0'; p++) {
+      if (isspace (*p)) {
+	continue;
+      }
+      if (!strncmp (p, "all", 3)) {
+	p += 3;
+	next_flag |= NEXT_ALL_FLAGS;
+      } else if (!strncmp (p, "vpath", 5)) {
+	p += 5;
+	next_flag |= NEXT_VPATH_FLAG;
+      } else if (!strncmp (p, "quiet", 5)) {
+	p += 5;
+	next_flag |= NEXT_QUIET_FLAG;
+      } else if (!strncmp (p, "makefiles", 9)) {
+	p += 9;
+	next_flag |= NEXT_MAKEFILES_FLAG;
+      } else if (!strncmp (p, "errexit", 7)) {
+	p += 7;
+	next_flag |= NEXT_ERREXIT_FLAG;
+      }
+    }
+  }
+}
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
+
 void
 init_hash_global_variable_set ()
 {
@@ -105,6 +139,10 @@
   struct variable *v;
   struct variable **var_slot;
   struct variable var_key;
+
+#if defined(__APPLE__) || defined(NeXT) || defined(NeXT_PDO)
+  check_apple_pb_support (name, length, value);
+#endif /* __APPLE__ || NeXT || NeXT_PDO */
 
   if (set == NULL)
     set = &global_variable_set;
