#
#  Makefile for Apache2
#

builddir     = .

top_dir      = /usr/local/apache2

top_srcdir   = ${top_dir}
top_builddir = ${top_dir}

include ${top_builddir}/build/special.mk

APXS      = apxs
APACHECTL = apachectl

#DEFS=-Dmy_define=my_value
#INCLUDES=-Imy/include/dir
#LIBS=-Lmy/lib/dir -lmylib

all: local-shared-build

clean:
	-rm -f *.o *.lo *.slo *.la 

