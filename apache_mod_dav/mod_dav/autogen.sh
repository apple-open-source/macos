#!/bin/sh

echo "Creating config.h.in..."
autoheader

echo "Creating configure..."
autoconf

# always remove the config.cache. our configure is so small, there is no need
# to optimize whether we keep it or not
rm -f config.cache

# autoconf 2.5x sometimes generates a cache directory
rm -rf autom4te.cache

echo "Attempting to create INSTALL..."
(make -f Makefile.in doc && echo "  -> created.") || echo "oh well"
