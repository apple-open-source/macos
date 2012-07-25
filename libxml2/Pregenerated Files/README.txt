The files in this directory are generated programatically as part of the regular automake-based libxml2
build process. The manner in which they are generated is sufficiently complicated that for now we'll
stick with checking in the generated files and updating them by hand when needed.

config.h and libxml/xmlversion.h: Taken directly from a regular automake-based build of libxml2.
xml2-config: Hand-written based on xml2-config.in since the values are constant for OS X.
