The files in this directory are generated programatically as part of the regular automake-based libxml2
build process. The manner in which they are generated is sufficiently complicated that for now we'll
stick with checking in the generated files and updating them by hand when needed.

config.h and libxml/xmlversion.h: Taken directly from a regular automake-based build of libxml2.
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
   ./configure --prefix=/usr --without-iconv --with-icu --without-lzma --with-zlib
4. Edit xml2-config to change:
-prefix=/usr
+prefix=$(xcrun -show-sdk-path)/usr
5. Copy replacement files into place:
   cp -p config.h "../Pregenerated Files/"
   cp -p include/libxml/xmlversion.h "../Pregenerated Files/libxml/"
   cp -p xml2-config "../Pregenerated Files/"
6. Run git-add on changed files (including those in libxml2), and check them in.
   cd ..
   git add "Pregenerated Files/config.h" "Pregenerated Files/libxml/xmlversion.h" "Pregenerated Files/xml2-config"
   git add config.h.in ...
7. Run git-commit to commit the updated files.
8. Clean up the files generated from Step 3.
   git status --ignored
   git clean --force -d -x
