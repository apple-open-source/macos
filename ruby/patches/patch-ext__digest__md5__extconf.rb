--- ext/digest/md5/extconf.rb.orig	Wed Oct 27 08:55:31 2004
+++ ext/digest/md5/extconf.rb	Wed Oct 27 08:55:49 2004
@@ -4,6 +4,7 @@
 require "mkmf"
 
 $CFLAGS << " -DHAVE_CONFIG_H -I#{File.dirname(__FILE__)}/.."
+$CPPFLAGS << " -DHAVE_CONFIG_H -I#{File.dirname(__FILE__)}/.."
 
 $objs = [ "md5init.#{$OBJEXT}" ]
 
