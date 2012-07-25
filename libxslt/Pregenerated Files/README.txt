The files in this directory are generated programatically as part of the regular automake-based libxslt
build process. The manner in which they are generated is sufficiently complicated that for now we'll
stick with checking in the generated files and updating them by hand when needed.

config.h, xsltconfig.h and exsltconfig.h: Taken directly from a regular automake-based build of libxslt.
xslt-config: Hand-written based on xslt-config.in since the values are constant for OS X.
