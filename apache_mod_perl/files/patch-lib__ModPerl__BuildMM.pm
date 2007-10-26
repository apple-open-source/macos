--- lib/ModPerl/BuildMM.pm.orig	2006-04-18 12:50:45.000000000 -0700
+++ lib/ModPerl/BuildMM.pm	2006-04-18 12:51:01.000000000 -0700
@@ -111,7 +111,7 @@
         INC       => $inc,
         CCFLAGS   => $ccflags,
         OPTIMIZE  => $build->perl_config('optimize'),
-        LDDLFLAGS => $build->perl_config('lddlflags'),
+        LDDLFLAGS => "$ENV{RC_CFLAGS} " . $build->perl_config('lddlflags'),
         LIBS      => $libs,
         dynamic_lib => { OTHERLDFLAGS => $build->otherldflags },
     );
