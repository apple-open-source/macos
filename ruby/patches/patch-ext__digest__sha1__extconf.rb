--- ext/digest/sha1/extconf.rb.orig	Wed Oct 27 09:04:39 2004
+++ ext/digest/sha1/extconf.rb	Wed Oct 27 09:04:57 2004
@@ -4,6 +4,7 @@
 require "mkmf"
 
 $CFLAGS << " -DHAVE_CONFIG_H -I#{File.dirname(__FILE__)}/.."
+$CPPFLAGS << " -DHAVE_CONFIG_H -I#{File.dirname(__FILE__)}/.."
 
 $objs = [ "sha1init.#{$OBJEXT}" ]
 
