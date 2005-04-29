--- ext/readline/extconf.rb.orig	Sat Dec  4 12:50:57 2004
+++ ext/readline/extconf.rb	Sat Dec  4 12:51:19 2004
@@ -1,3 +1,6 @@
+# Doesn't work with libedit.
+exit
+
 require "mkmf"
 
 dir_config("readline")
