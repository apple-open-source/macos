--- ext/zlib/extconf.rb.orig	Wed Oct 27 14:17:33 2004
+++ ext/zlib/extconf.rb	Wed Oct 27 14:23:12 2004
@@ -55,11 +55,8 @@
   defines << "OS_CODE=#{os_code}"
 
   defines = defines.collect{|d|' -D'+d}.join
-  if $CPPFLAGS then
-    $CPPFLAGS += defines
-  else
-    $CFLAGS += defines
-  end
+  $CFLAGS += defines
+  $CPPFLAGS += defines
 
   create_makefile('zlib')
 
