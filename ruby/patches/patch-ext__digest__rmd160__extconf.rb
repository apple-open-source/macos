--- ext/digest/rmd160/extconf.rb.orig	Wed Oct 27 09:03:52 2004
+++ ext/digest/rmd160/extconf.rb	Wed Oct 27 09:04:03 2004
@@ -4,6 +4,7 @@
 require "mkmf"
 
 $CFLAGS << " -DHAVE_CONFIG_H -I#{File.dirname(__FILE__)}/.."
+$CPPFLAGS << " -DHAVE_CONFIG_H -I#{File.dirname(__FILE__)}/.."
 
 $objs = [ "rmd160init.#{$OBJEXT}" ]
 
