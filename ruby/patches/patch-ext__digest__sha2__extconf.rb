--- ext/digest/sha2/extconf.rb.orig	Wed Oct 27 09:04:43 2004
+++ ext/digest/sha2/extconf.rb	Wed Oct 27 09:04:53 2004
@@ -4,6 +4,7 @@
 require "mkmf"
 
 $CFLAGS << " -DHAVE_CONFIG_H -I#{File.dirname(__FILE__)}/.."
+$CPPFLAGS << " -DHAVE_CONFIG_H -I#{File.dirname(__FILE__)}/.."
 
 $objs = [
   "sha2.#{$OBJEXT}",
