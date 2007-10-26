--- lib/mkmf.rb	2006-08-17 07:47:50.000000000 +0200
+++ lib/mkmf.rb.new	2006-09-24 19:40:58.000000000 +0200
@@ -285,7 +285,7 @@
 end
 
 def cpp_command(outfile, opt="")
-  Config::expand("$(CPP) #$INCFLAGS #$CPPFLAGS #$CFLAGS #{opt} #{CONFTEST_C} #{outfile}",
+  Config::expand("$(CPP) #$INCFLAGS #$CPPFLAGS #$CFLAGS #{opt} #{CONFTEST_C} #{outfile}".gsub(/-arch \w+/, ""),
 		 CONFIG.merge('hdrdir' => $hdrdir.quote, 'srcdir' => $srcdir.quote))
 end
 
