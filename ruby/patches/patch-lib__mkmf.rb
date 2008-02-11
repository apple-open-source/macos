--- lib/mkmf.rb.old	2007-11-16 14:17:15.000000000 +0100
+++ lib/mkmf.rb	2007-11-16 14:17:34.000000000 +0100
@@ -287,7 +287,7 @@
 
 def cpp_command(outfile, opt="")
   conf = Config::CONFIG.merge('hdrdir' => $hdrdir.quote, 'srcdir' => $srcdir.quote)
-  Config::expand("$(CPP) #$INCFLAGS #$CPPFLAGS #$CFLAGS #{opt} #{CONFTEST_C} #{outfile}",
+  Config::expand("$(CPP) #$INCFLAGS #$CPPFLAGS #$CFLAGS #{opt} #{CONFTEST_C} #{outfile}".gsub(/-arch \w+/, ""),
 		 conf)
 end
 
