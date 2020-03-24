The files in this directory are generated programatically as part of the regular automake-based libxml2
build process. The manner in which they are generated is sufficiently complicated that for now we'll
stick with checking in the generated files and updating them by hand when needed.

include/config.h and include/libxml/xmlversion.h: Taken directly from a regular automake-based build of libxml2.
xml2-config: Hand-modified based on generated xml2-config to include the SDKROOT in paths.

Steps to rebuild files:
1. Install Homebrew.  <http://brew.sh/>
2. Install autoconf, automake, libtool, pkg-config from Homebrew.
3. In the libxml2.git source directory (assuming /usr/local/bin is in your path):
   cd libxml2
   glibtoolize --force
   cp /usr/local/Cellar/pkg-config/0.28/share/aclocal/pkg.m4 ./m4/
   aclocal -I m4
   autoheader
   automake --add-missing --force-missing
   autoconf
   ./configure --prefix=/usr --without-iconv --with-icu --without-lzma --without-python --with-zlib
4. Edit xml2-config.
   a. Fix prefix value:
-prefix=/usr
+prefix=$(xcrun -show-sdk-path)/usr
   b. Remove unnecessary "-L/usr/lib" switches from --libs output:
            if [ "`uname`" = "Darwin" -a "-L${libdir}" = "-L/usr/lib" ]
            then
-               echo -lxml2 -L/usr/lib -lz   -lpthread   -licucore -lm   
+               echo -lxml2 -lz   -lpthread   -licucore -lm   
            else
-               echo -L${libdir} -lxml2 -L/usr/lib -lz   -lpthread   -licucore -lm   
+               echo -L${libdir} -lxml2 -lz   -lpthread   -licucore -lm   
            fi
        fi
   c. Fix --cflags path (make sure to include the space at the end):
     --cflags)
-       	echo -I${includedir}/libxml2 
+       	echo -I${includedir} 
        	;;
5. [Optional] Run tests (compare output prior to patch as there is some spew):
   make -j $(sysctl -n hw.ncpu)
   make check
   To run tests with AddressSanitizer enabled, re-run configure with this environment variable:
     CC="xcrun -sdk macosx.internal cc -fsanitize=address"
6. Copy replacement files into place:
   cp -p config.h "../Pregenerated Files/include/"
   cp -p include/libxml/xmlversion.h "../Pregenerated Files/include/libxml/"
   cp -p xml2-config "../Pregenerated Files/"
7. Apply patch to "./Pregenerated Files/include/libxml/"
diff --git a/Pregenerated Files/include/libxml/xmlversion.h b/Pregenerated Files/include/libxml/xmlversion.h
index c4bf45a0..c7992c7a 100644
--- a/Pregenerated Files/include/libxml/xmlversion.h	
+++ b/Pregenerated Files/include/libxml/xmlversion.h	
@@ -91,11 +91,7 @@ XMLPUBFUN void XMLCALL xmlCheckVersion(int version);
  * Whether the thread support is configured in
  */
 #if 1
-#if defined(_REENTRANT) || defined(__MT__) || \
-    (defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE - 0 >= 199506L))
 #define LIBXML_THREAD_ENABLED
-#endif
 #endif
 
 /**
8. Run git-add on changed files (including those in libxml2), and check them in.
   cd ..
   git add "Pregenerated Files/include/config.h" "Pregenerated Files/include/libxml/xmlversion.h" "Pregenerated Files/xml2-config"
9. Update libxml2.plist with libxml2 version, md5 hash, radars to upstream as needed.
10. Run git-commit to commit the updated files.
11. Clean up the files generated from Step 3.
   git status --ignored
   git clean --force -d -x
