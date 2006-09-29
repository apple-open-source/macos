# Makefile for netnews tools useful with cyrus
#
# netnews/Makefile.  Generated from Makefile.in by configure.
# Copyright (c) 2000 Carnegie Mellon University.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer. 
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
#
# 3. The name "Carnegie Mellon University" must not be used to
#    endorse or promote products derived from this software without
#    prior written permission. For permission or any other legal
#    details, please contact  
#      Office of Technology Transfer
#      Carnegie Mellon University
#      5000 Forbes Avenue
#      Pittsburgh, PA  15213-3890
#      (412) 268-4387, fax: (412) 268-7395
#      tech-transfer@andrew.cmu.edu
#
# 4. Redistributions of any form whatsoever must retain the following
#    acknowledgment:
#    "This product includes software developed by Computing Services
#     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
#
# CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
# THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
# FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
# AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
# OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# $Id: Makefile.in,v 1.6 2005/03/05 00:37:29 dasenbro Exp $
# 
srcdir = .


CC = gcc
INSTALL = /usr/bin/install -c
RANLIB = ranlib

DEFS = -DHAVE_CONFIG_H -DMAC_OS_X -DDHAVE_SSL -DUSE_SETPROCTITLE -DHAVE_STDARG_H -DHAVE_STRLCPY -DHAVE_STRLCAT
CPPFLAGS = -I.. -I$(srcdir) -I$(srcdir)/../lib -I$(srcdir)/../et -I/usr/local/BerkeleyDB/include  -I/usr/include
LIBS =  -L/usr/lib  -lsasl2 -lresolv -lgssapi_krb5 -lresolv -lfl   -L/usr/local/BerkeleyDB/lib  -ldb-4.2 -lssl -lcrypto 
DEPLIBS = ../lib/libcyrus.a ../lib/libcyrus_min.a  

CFLAGS = -g -O2 
LDFLAGS = -L/usr/lib -L/usr/local/BerkeleyDB/lib 

SHELL = /bin/sh
MAKEDEPEND = /Volumes/Source/Projects/iservers/cvs-servers/CyrusIMAP-156.11/cyrus_imap/tools/not-mkdep

prefix = /usr/local
exec_prefix = ${prefix}
cyrus_prefix = /usr/cyrus
service_path = /usr/bin/cyrus/bin

OBJS = remotepurge.o readconfig.o

all: remotepurge

install:
	$(INSTALL) -s -m 755 remotepurge $(DESTDIR)$(service_path)

.c.o:
	$(CC) -c $(CPPFLAGS) $(DEFS) $(CFLAGS) $<

remotepurge: $(OBJS) $(DEPLIBS)
	$(CC) $(LDFLAGS) -o remotepurge $(OBJS) \
		$(DEPLIBS) $(LIBS)


clean:
	rm -f *.o Makefile.bak remotepurge makedepend.log

distclean: clean
	rm -f Makefile

depend:
	${MAKEDEPEND} $(CPPFLAGS) $(DEFS) $(CFLAGS) *.c $(srcdir)/*.c 1>makedepend.log 2>&1

# DO NOT DELETE THIS LINE -- make depend depends on it.

