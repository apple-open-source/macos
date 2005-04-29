--- ext/dbm/extconf.rb.orig	Wed Oct 27 14:19:38 2004
+++ ext/dbm/extconf.rb	Wed Oct 27 14:22:28 2004
@@ -34,6 +34,7 @@
     for hdr in $dbm_conf_headers.fetch(db, ["ndbm.h"])
       if have_header(hdr.dup) and have_type("DBM", hdr.dup, hsearch)
 	$CFLAGS += " " + hsearch + '-DDBM_HDR="<'+hdr+'>"'
+	$CPPFLAGS += " " + hsearch + '-DDBM_HDR="<'+hdr+'>"'
 	return true
       end
     end
@@ -55,7 +56,7 @@
 
 have_header("cdefs.h") 
 have_header("sys/cdefs.h") 
-if /DBM_HDR/ =~ $CFLAGS and have_func(db_prefix("dbm_open"))
+if (/DBM_HDR/ =~ $CFLAGS || /DBM_HDR/ =~ $CPPFLAGS) and have_func(db_prefix("dbm_open"))
   have_func(db_prefix("dbm_clearerr")) unless $dbm_conf_have_gdbm
   create_makefile("dbm")
 end
