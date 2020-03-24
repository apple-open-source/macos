sysconfdir=${prefix}/etc
piddir=/var/run
srcdir=.
top_srcdir=..


CC=cc
LD=cc
CFLAGS=-g -O2 -pipe -Wunknown-warning-option -Qunused-arguments -Wall -Wpointer-arith -Wuninitialized -Wsign-compare -Wformat-security -Wsizeof-pointer-memaccess -Wno-pointer-sign -Wno-unused-result -fno-strict-aliasing -mretpoline -D_FORTIFY_SOURCE=2 -ftrapv -fno-builtin-memset -fstack-protector-strong -fPIE  
CPPFLAGS=-I. -I.. -I$(srcdir) -I$(srcdir)/..  -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.15.Internal.sdk/usr/local/include/sshcrypto   -DHAVE_CONFIG_H
LIBS=-lcrypto -lbsm -lz  -lresolv
AR=ar
RANLIB=ranlib
INSTALL=/usr/bin/install -c
LDFLAGS=-L.  -fstack-protector-strong -L/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.15.Internal.sdk/usr/local/lib/sshcrypto -pie 

OPENBSD=base64.o \
	basename.o \
	bcrypt_pbkdf.o \
	bcrypt_pbkdf.o \
	bindresvport.o \
	blowfish.o \
	daemon.o \
	dirname.o \
	explicit_bzero.o \
	fmt_scaled.o \
	freezero.o \
	getcwd.o \
	getgrouplist.o \
	getopt_long.o \
	getrrsetbyname.o \
	glob.o \
	inet_aton.o \
	inet_ntoa.o \
	inet_ntop.o \
	md5.o \
	memmem.o \
	mktemp.o \
	pwcache.o \
	readpassphrase.o \
	reallocarray.o \
	recallocarray.o \
	rmd160.o \
	rresvport.o \
	setenv.o \
	setproctitle.o \
	sha1.o \
	sha2.o \
	sigact.o \
	strcasestr.o \
	strlcat.o \
	strlcpy.o \
	strmode.o \
	strndup.o \
	strnlen.o \
	strptime.o \
	strsep.o \
	strtoll.o \
	strtonum.o \
	strtoull.o \
	strtoul.o \
	timingsafe_bcmp.o \
	vis.o

COMPAT=	arc4random.o \
	bsd-asprintf.o \
	bsd-closefrom.o \
	bsd-cygwin_util.o \
	bsd-err.o \
	bsd-flock.o \
	bsd-getline.o \
	bsd-getpagesize.o \
	bsd-getpeereid.o \
	bsd-malloc.o \
	bsd-misc.o \
	bsd-nextstep.o \
	bsd-openpty.o \
	bsd-poll.o \
	bsd-setres_id.o \
	bsd-signal.o \
	bsd-snprintf.o \
	bsd-statvfs.o \
	bsd-waitpid.o \
	fake-rfc2553.o \
	getrrsetbyname-ldns.o \
	kludge-fd_set.o \
	openssl-compat.o \
	libressl-api-compat.o \
	xcrypt.o

PORTS=	port-aix.o \
	port-irix.o \
	port-linux.o \
	port-solaris.o \
	port-net.o \
	port-uw.o

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

all: libopenbsd-compat.a

$(COMPAT): ../config.h
$(OPENBSD): ../config.h
$(PORTS): ../config.h

libopenbsd-compat.a:  $(COMPAT) $(OPENBSD) $(PORTS)
	$(AR) rv $@ $(COMPAT) $(OPENBSD) $(PORTS)
	$(RANLIB) $@

clean:
	rm -f *.o *.a core

distclean: clean
	rm -f Makefile *~
