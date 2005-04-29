--- bc/util.c.orig	Tue Oct 26 09:16:25 2004
+++ bc/util.c	Tue Oct 26 09:17:55 2004
@@ -28,6 +28,11 @@
        
 *************************************************************************/
 
+#ifdef __APPLE__
+#include <get_compat.h>
+#else  /* !__APPLE__ */
+#define COMPAT_MODE(a,b) (1)
+#endif /* __APPLE__ */
 
 #include "bcdefs.h"
 #ifndef VARARGS
@@ -346,7 +351,7 @@
     }
   else
     {
-      if (!std_only)
+      if (!std_only && !COMPAT_MODE("bin/bc", "Unix2003"))
 	{
 	  out_col++;
 	  if (out_col == line_size-1)
