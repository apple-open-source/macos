--- lib/mkmf.rb.orig	2004-12-04 09:11:53.000000000 -0800
+++ lib/mkmf.rb	2004-12-04 09:12:03.000000000 -0800
@@ -230,7 +230,7 @@
 
 def cpp_command(outfile, opt="")
   "$(CPP) #$INCFLAGS -I#{$hdrdir} " \
-  "#$CPPFLAGS #$CFLAGS #{opt} #{CONFTEST_C} #{outfile}"
+  "#$CPPFLAGS #$CFLAGS #{opt} #{CONFTEST_C} #{outfile}".gsub(/-arch \w+/, "")
 end
 
 def libpathflag(libpath=$LIBPATH)
