The files in this directory are generated programatically as part of the regular automake-based libxslt
build process. The manner in which they are generated is sufficiently complicated that for now we'll
stick with checking in the generated files and updating them by hand when needed.

include/config.h, include/libxslt/xsltconfig.h and include/libexslt/exsltconfig.h: Taken
directly from a regular automake-based build of libxslt.
xslt-config: Hand-modified based on generated xslt-config to include the SDKROOT in paths.

Steps to rebuild files:
1. Install Homebrew.  <http://brew.sh/>
2. Install autoconf, automake, libtool, pkg-config from Homebrew.
3. In the libxslt.git source directory (assuming /usr/local/bin is in your path):
   cd libxslt.git/libxslt
   glibtoolize --force
   mkdir m4
   cp /usr/local/Cellar/pkg-config/0.28/share/aclocal/pkg.m4 ./m4/
   aclocal -I m4
   autoheader
   automake --add-missing --force-missing
   autoconf
   ./configure --prefix=/usr --without-python --disable-static
4. Edit config.h to make these changes:

--- config.h.orig	2015-12-05 13:52:59.000000000 -0800
+++ config.h	2015-12-05 13:59:57.000000000 -0800
@@ -177,7 +177,7 @@
 
 /* Enable extensions on AIX 3, Interix.  */
 #ifndef _ALL_SOURCE
-# define _ALL_SOURCE 1
+/* # undef _ALL_SOURCE */
 #endif
 /* Enable GNU extensions on systems that have them.  */
 #ifndef _GNU_SOURCE
@@ -185,15 +185,15 @@
 #endif
 /* Enable threading extensions on Solaris.  */
 #ifndef _POSIX_PTHREAD_SEMANTICS
-# define _POSIX_PTHREAD_SEMANTICS 1
+/* # undef _POSIX_PTHREAD_SEMANTICS */
 #endif
 /* Enable extensions on HP NonStop.  */
 #ifndef _TANDEM_SOURCE
-# define _TANDEM_SOURCE 1
+/* # undef _TANDEM_SOURCE */
 #endif
 /* Enable general extensions on Solaris.  */
 #ifndef __EXTENSIONS__
-# define __EXTENSIONS__ 1
+/* # undef __EXTENSIONS__ */
 #endif
 
 

5. Edit xslt-config to make these changes:

--- xslt-config.orig	2015-12-05 13:52:59.000000000 -0800
+++ xslt-config	2015-12-05 13:55:16.000000000 -0800
@@ -1,6 +1,6 @@
 #! /bin/sh
 
-prefix=/usr
+prefix=$(xcrun -show-sdk-path)/usr
 exec_prefix=${prefix}
 exec_prefix_set=no
 includedir=${prefix}/include
@@ -65,7 +65,7 @@
 	;;
 
     --plugins)
-	echo /usr/lib/libxslt-plugins
+	echo $libdir/libxslt-plugins
 	exit 0
 	;;
 
@@ -91,9 +91,9 @@
 
 the_libs="-L${libdir} -lxslt  -lxml2 -lz -lpthread -licucore -lm  "
 if test "$includedir" != "/usr/include"; then
-    the_flags="$the_flags -I$includedir `/usr/bin/xml2-config --cflags`"
+    the_flags="$the_flags -I$includedir `$(xcrun -show-sdk-path)/usr/bin/xml2-config --cflags`"
 else
-    the_flags="$the_flags `/usr/bin/xml2-config --cflags`"
+    the_flags="$the_flags `$(xcrun -show-sdk-path)/usr/bin/xml2-config --cflags`"
 fi
 
 if $cflags; then

7. Fix permissions on xslt-config.
   chmod 755 xslt-config
8. Revert unwanted changes:
   git checkout HEAD doc/xsltproc.1
9. [Optional] Run tests (compare output prior to patch as there is some spew):
   make -j $(sysctl -n hw.ncpu)
   make tests
   To run tests with AddressSanitizer enabled, re-run configure with this environment variable:
     CC="xcrun -sdk macosx.internal cc -fsanitize=address"
   NOTE: This currently doesn't work; I haven't figured out why yet! Compile with Xcode to get ASan builds for now.
10. Copy replacement files into place:
   cp -p config.h "../Pregenerated Files/include/"
   cp -p libexslt/exsltconfig.h "../Pregenerated Files/include/libexslt/"
   cp -p libxslt/xsltconfig.h "../Pregenerated Files/include/libxslt/"
   cp -p xslt-config "../Pregenerated Files/"
11. Run git-add on changed files (including those in libxslt), and check them in.
   cd ..
   git add "Pregenerated Files/include/config.h" "Pregenerated Files/include/libexslt/exsltconfig.h" "Pregenerated Files/include/libxslt/xsltconfig.h" "Pregenerated Files/xslt-config"
   git add libxslt/config.h.in
12. Update libxslt.plist with libxslt version, md5 hash, radars to upstream as needed.
13. Run git-commit to commit the updated files.
14. Clean up the files generated from Step 3.
   git status --ignored
   git clean --force -d -x
