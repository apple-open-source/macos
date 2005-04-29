--- ext/dl/extconf.rb.orig	Wed Oct 27 14:19:43 2004
+++ ext/dl/extconf.rb	Wed Oct 27 14:22:53 2004
@@ -20,10 +20,12 @@
   exit(0)
 end
 
-($CPPFLAGS || $CFLAGS) << " -I."
+$CFLAGS << " -I."
+$CPPFLAGS << " -I."
 
 if (Config::CONFIG['CC'] =~ /gcc/)  # from Win32API
   $CFLAGS << " -fno-defer-pop -fno-omit-frame-pointer"
+  $CPPFLAGS << " -fno-defer-pop -fno-omit-frame-pointer"
 end
 
 $with_dlstack ||= true
