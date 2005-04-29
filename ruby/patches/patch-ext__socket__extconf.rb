--- ext/socket/extconf.rb.orig	Wed Oct 27 14:11:03 2004
+++ ext/socket/extconf.rb	Wed Oct 27 14:14:21 2004
@@ -36,6 +36,7 @@
 }
 EOF
     $CFLAGS+=" -DENABLE_IPV6"
+    $CPPFLAGS+=" -DENABLE_IPV6"
     $ipv6 = true
   end
 end
@@ -50,6 +51,7 @@
 EOF
     $ipv6type = "inria"
     $CFLAGS="-DINET6 "+$CFLAGS
+    $CPPFLAGS="-DINET6 "+$CPPFLAGS
   elsif macro_defined?("__KAME__", <<EOF)
 #include <netinet/in.h>
 EOF
@@ -58,11 +60,13 @@
     $ipv6libdir="/usr/local/v6/lib"
     $ipv6trylibc=true
     $CFLAGS="-DINET6 "+$CFLAGS
+    $CPPFLAGS="-DINET6 "+$CPPFLAGS
   elsif File.directory? "/usr/inet6"
     $ipv6type = "linux"
     $ipv6lib="inet6"
     $ipv6libdir="/usr/inet6/lib"
     $CFLAGS="-DINET6 -I/usr/inet6/include "+$CFLAGS
+    $CPPFLAGS="-DINET6 -I/usr/inet6/include "+$CPPFLAGS
   elsif macro_defined?("_TOSHIBA_INET6", <<EOF)
 #include <sys/param.h>
 EOF
@@ -70,6 +74,7 @@
     $ipv6lib="inet6"
     $ipv6libdir="/usr/local/v6/lib"
     $CFLAGS="-DINET6 "+$CFLAGS
+    $CPPFLAGS="-DINET6 "+$CPPFLAGS
   elsif macro_defined?("__V6D__", <<EOF)
 #include </usr/local/v6/include/sys/v6config.h>
 EOF
@@ -77,6 +82,7 @@
     $ipv6lib="v6"
     $ipv6libdir="/usr/local/v6/lib"
     $CFLAGS="-DINET6 -I/usr/local/v6/include "+$CFLAGS
+    $CPPFLAGS="-DINET6 -I/usr/local/v6/include "+$CPPFLAGS
   elsif macro_defined?("_ZETA_MINAMI_INET6", <<EOF)
 #include <sys/param.h>
 EOF
@@ -84,10 +90,12 @@
     $ipv6lib="inet6"
     $ipv6libdir="/usr/local/v6/lib"
     $CFLAGS="-DINET6 "+$CFLAGS
+    $CPPFLAGS="-DINET6 "+$CPPFLAGS
   else
     $ipv6lib=with_config("ipv6-lib", nil)
     $ipv6libdir=with_config("ipv6-libdir", nil)
     $CFLAGS="-DINET6 "+$CFLAGS
+    $CPPFLAGS="-DINET6 "+$CPPFLAGS
   end
   
   if $ipv6lib
@@ -126,6 +134,7 @@
 }
 EOF
     $CFLAGS="-DHAVE_SIN_LEN "+$CFLAGS
+    $CPPFLAGS="-DHAVE_SIN_LEN "+$CPPFLAGS
 end
 
   if try_link(<<EOF)
@@ -148,6 +157,7 @@
 }
 EOF
     $CFLAGS="-DHAVE_SOCKADDR_STORAGE "+$CFLAGS
+    $CPPFLAGS="-DHAVE_SOCKADDR_STORAGE "+$CPPFLAGS
 else      #   doug's fix, NOW add -Dss_family... only if required!
 $CPPFLAGS += " -Dss_family=__ss_family -Dss_len=__ss_len"
   if try_link(<<EOF)
@@ -170,6 +180,7 @@
 }
 EOF
     $CFLAGS="-DHAVE_SOCKADDR_STORAGE "+$CFLAGS
+    $CPPFLAGS="-DHAVE_SOCKADDR_STORAGE "+$CPPFLAGS
 end
 end
 
@@ -189,6 +200,7 @@
 }
 EOF
     $CFLAGS="-DHAVE_SA_LEN "+$CFLAGS
+    $CPPFLAGS="-DHAVE_SA_LEN "+$CPPFLAGS
 end
 
 have_header("netinet/tcp.h") if not /cygwin/ =~ RUBY_PLATFORM # for cygwin 1.1.5
@@ -305,8 +317,10 @@
 case with_config("lookup-order-hack", "UNSPEC")
 when "INET"
   $CFLAGS="-DLOOKUP_ORDER_HACK_INET "+$CFLAGS
+  $CPPFLAGS="-DLOOKUP_ORDER_HACK_INET "+$CPPFLAGS
 when "INET6"
   $CFLAGS="-DLOOKUP_ORDER_HACK_INET6 "+$CFLAGS
+  $CPPFLAGS="-DLOOKUP_ORDER_HACK_INET6 "+$CPPFLAGS
 when "UNSPEC"
   # nothing special
 else
@@ -347,13 +361,16 @@
 }
 EOF
     $CFLAGS="-DHAVE_ADDR8 "+$CFLAGS
+    $CPPFLAGS="-DHAVE_ADDR8 "+$CPPFLAGS
   end
 end
 
 if have_getaddrinfo
   $CFLAGS="-DHAVE_GETADDRINFO "+$CFLAGS
+  $CPPFLAGS="-DHAVE_GETADDRINFO "+$CPPFLAGS
 else
   $CFLAGS="-I. "+$CFLAGS
+  $CPPFLAGS="-I. "+$CPPFLAGS
   $objs += ["getaddrinfo.#{$OBJEXT}"]
   $objs += ["getnameinfo.#{$OBJEXT}"]
   have_func("inet_ntop") or have_func("inet_ntoa")
@@ -378,6 +395,7 @@
 }
 EOF
   $CFLAGS="-Dsocklen_t=int "+$CFLAGS
+  $CPPFLAGS="-Dsocklen_t=int "+$CPPFLAGS
 end
 
 have_header("sys/un.h")
@@ -392,8 +410,10 @@
   if enable_config("socks", ENV["SOCKS_SERVER"])
     if have_library("socks5", "SOCKSinit")
       $CFLAGS+=" -DSOCKS5 -DSOCKS"
+      $CPPFLAGS+=" -DSOCKS5 -DSOCKS"
     elsif have_library("socks", "Rconnect")
       $CFLAGS+=" -DSOCKS"
+      $CPPFLAGS+=" -DSOCKS"
     end
   end
   create_makefile("socket")
