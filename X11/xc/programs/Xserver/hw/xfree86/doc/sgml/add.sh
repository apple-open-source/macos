#!/bin/sh
#
# $XFree86: xc/programs/Xserver/hw/xfree86/doc/sgml/add.sh,v 1.1 1999/08/23 09:06:04 dawes Exp $
#
name=`basename $1 .sgml`
sgmlfmt -f index $name.sgml | \
	sed -e 's,<title>,<item><htmlurl name=",' \
	    -e 's,</title>," url="'$name.html'">,' \
	    -e 's,<author>,<!-- ,' \
	    -e 's,</author>, -->,' \
	    -e 's,<date>,<!-- ,' \
	    -e 's,</date>, -->,' >> index.sgml
exit 0
