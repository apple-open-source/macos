--- ext/socket/socket.c.orig	Wed Oct 27 09:14:26 2004
+++ ext/socket/socket.c	Wed Oct 27 09:14:49 2004
@@ -93,6 +93,9 @@
 #define INET_SERVER 1
 #define INET_SOCKS  2
 
+/* short-circuit detection brain damage */
+#define HAVE_SOCKADDR_STORAGE
+
 #ifndef HAVE_SOCKADDR_STORAGE
 /*
  * RFC 2553: protocol-independent placeholder for socket addresses
