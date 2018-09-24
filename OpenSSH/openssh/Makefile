# uncomment if you run a non bourne compatable shell. Ie. csh
#SHELL = /bin/sh

AUTORECONF=autoreconf

prefix=/usr/local
exec_prefix=${prefix}
bindir=${exec_prefix}/bin
sbindir=${exec_prefix}/sbin
libexecdir=${exec_prefix}/libexec
datadir=${datarootdir}
datarootdir=${prefix}/share
mandir=${datarootdir}/man
mansubdir=man
sysconfdir=${prefix}/etc
piddir=/var/run
srcdir=.
top_srcdir=.

DESTDIR=

SSH_PROGRAM=${exec_prefix}/bin/ssh
ASKPASS_PROGRAM=$(libexecdir)/ssh-askpass
SFTP_SERVER=$(libexecdir)/sftp-server
SSH_KEYSIGN=$(libexecdir)/ssh-keysign
SSH_PKCS11_HELPER=$(libexecdir)/ssh-pkcs11-helper
PRIVSEP_PATH=/var/empty
SSH_PRIVSEP_USER=sshd
STRIP_OPT=-s
TEST_SHELL=sh

PATHS= -DSSHDIR=\"$(sysconfdir)\" \
	-D_PATH_SSH_PROGRAM=\"$(SSH_PROGRAM)\" \
	-D_PATH_SSH_ASKPASS_DEFAULT=\"$(ASKPASS_PROGRAM)\" \
	-D_PATH_SFTP_SERVER=\"$(SFTP_SERVER)\" \
	-D_PATH_SSH_KEY_SIGN=\"$(SSH_KEYSIGN)\" \
	-D_PATH_SSH_PKCS11_HELPER=\"$(SSH_PKCS11_HELPER)\" \
	-D_PATH_SSH_PIDDIR=\"$(piddir)\" \
	-D_PATH_PRIVSEP_CHROOT_DIR=\"$(PRIVSEP_PATH)\"

CC=gcc
LD=gcc
CFLAGS=-g -O2 -pipe -Qunused-arguments -Wunknown-warning-option -Wall -Wpointer-arith -Wuninitialized -Wsign-compare -Wformat-security -Wsizeof-pointer-memaccess -Wno-pointer-sign -Wno-unused-result -fno-strict-aliasing -D_FORTIFY_SOURCE=2 -ftrapv -fno-builtin-memset -fstack-protector-strong -fPIE  
CPPFLAGS=-I. -I$(srcdir)  -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.14.Internal.sdk/usr/local/include/sshcrypto   $(PATHS) -DHAVE_CONFIG_H
LIBS=-lcrypto -lbsm -lz  -lresolv
K5LIBS=-dynamic -Wl,-search_paths_first -lkrb5
GSSLIBS=-dynamic -Wl,-search_paths_first -lkrb5
SSHLIBS=
SSHDLIBS= -lsandbox -lpam -ldl
LIBEDIT=
AR=ar
AWK=awk
RANLIB=ranlib
INSTALL=/usr/bin/install -c
SED=/usr/bin/sed
ENT=
XAUTH_PATH=xauth
LDFLAGS=-L. -Lopenbsd-compat/  -fstack-protector-strong -L/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.14.Internal.sdk/usr/local/lib/sshcrypto -pie 
EXEEXT=
MANFMT=/usr/bin/nroff -mandoc
MKDIR_P=./install-sh -c -d

TARGETS=ssh$(EXEEXT) sshd$(EXEEXT) ssh-add$(EXEEXT) ssh-keygen$(EXEEXT) ssh-keyscan${EXEEXT} ssh-keysign${EXEEXT} ssh-pkcs11-helper$(EXEEXT) ssh-agent$(EXEEXT) scp$(EXEEXT) sftp-server$(EXEEXT) sftp$(EXEEXT)

XMSS_OBJS=\
	ssh-xmss.o \
	sshkey-xmss.o \
	xmss_commons.o \
	xmss_fast.o \
	xmss_hash.o \
	xmss_hash_address.o \
	xmss_wots.o

LIBOPENSSH_OBJS=\
	ssh_api.o \
	ssherr.o \
	sshbuf.o \
	sshkey.o \
	sshbuf-getput-basic.o \
	sshbuf-misc.o \
	sshbuf-getput-crypto.o \
	krl.o \
	bitmap.o \
	${XMSS_OBJS}

LIBSSH_OBJS=${LIBOPENSSH_OBJS} \
	authfd.o authfile.o bufaux.o bufbn.o bufec.o buffer.o \
	canohost.o channels.o cipher.o cipher-aes.o cipher-aesctr.o \
	cipher-ctr.o cleanup.o \
	compat.o crc32.o fatal.o hostfile.o \
	log.o match.o moduli.o nchan.o packet.o opacket.o \
	readpass.o ttymodes.o xmalloc.o addrmatch.o \
	atomicio.o key.o dispatch.o mac.o uidswap.o uuencode.o misc.o utf8.o \
	monitor_fdpass.o rijndael.o ssh-dss.o ssh-ecdsa.o ssh-rsa.o dh.o \
	msg.o progressmeter.o dns.o entropy.o gss-genr.o umac.o umac128.o \
	ssh-pkcs11.o smult_curve25519_ref.o \
	poly1305.o chacha.o cipher-chachapoly.o \
	ssh-ed25519.o digest-openssl.o digest-libc.o hmac.o \
	sc25519.o ge25519.o fe25519.o ed25519.o verify.o hash.o \
	kex.o kexdh.o kexgex.o kexecdh.o kexc25519.o \
	kexdhc.o kexgexc.o kexecdhc.o kexc25519c.o \
	kexdhs.o kexgexs.o kexecdhs.o kexc25519s.o \
	platform-pledge.o platform-tracing.o platform-misc.o

SSHOBJS= ssh.o readconf.o clientloop.o sshtty.o \
	sshconnect.o sshconnect2.o mux.o

SSHDOBJS=sshd.o auth-rhosts.o auth-passwd.o \
	audit.o audit-bsm.o audit-linux.o platform.o \
	sshpty.o sshlogin.o servconf.o serverloop.o \
	auth.o auth2.o auth-options.o session.o \
	auth2-chall.o groupaccess.o \
	auth-skey.o auth-bsdauth.o auth2-hostbased.o auth2-kbdint.o \
	auth2-none.o auth2-passwd.o auth2-pubkey.o \
	monitor.o monitor_wrap.o auth-krb5.o \
	auth2-gss.o gss-serv.o gss-serv-krb5.o \
	loginrec.o auth-pam.o auth-shadow.o auth-sia.o md5crypt.o \
	sftp-server.o sftp-common.o \
	sandbox-null.o sandbox-rlimit.o sandbox-systrace.o sandbox-darwin.o \
	sandbox-seccomp-filter.o sandbox-capsicum.o sandbox-pledge.o \
	sandbox-solaris.o

MANPAGES	= moduli.5.out scp.1.out ssh-add.1.out ssh-agent.1.out ssh-keygen.1.out ssh-keyscan.1.out ssh.1.out sshd.8.out sftp-server.8.out sftp.1.out ssh-keysign.8.out ssh-pkcs11-helper.8.out sshd_config.5.out ssh_config.5.out
MANPAGES_IN	= moduli.5 scp.1 ssh-add.1 ssh-agent.1 ssh-keygen.1 ssh-keyscan.1 ssh.1 sshd.8 sftp-server.8 sftp.1 ssh-keysign.8 ssh-pkcs11-helper.8 sshd_config.5 ssh_config.5
MANTYPE		= doc

CONFIGFILES=sshd_config.out ssh_config.out moduli.out
CONFIGFILES_IN=sshd_config ssh_config moduli

PATHSUBS	= \
	-e 's|/etc/ssh/ssh_config|$(sysconfdir)/ssh_config|g' \
	-e 's|/etc/ssh/ssh_known_hosts|$(sysconfdir)/ssh_known_hosts|g' \
	-e 's|/etc/ssh/sshd_config|$(sysconfdir)/sshd_config|g' \
	-e 's|/usr/libexec|$(libexecdir)|g' \
	-e 's|/etc/shosts.equiv|$(sysconfdir)/shosts.equiv|g' \
	-e 's|/etc/ssh/ssh_host_key|$(sysconfdir)/ssh_host_key|g' \
	-e 's|/etc/ssh/ssh_host_ecdsa_key|$(sysconfdir)/ssh_host_ecdsa_key|g' \
	-e 's|/etc/ssh/ssh_host_dsa_key|$(sysconfdir)/ssh_host_dsa_key|g' \
	-e 's|/etc/ssh/ssh_host_rsa_key|$(sysconfdir)/ssh_host_rsa_key|g' \
	-e 's|/etc/ssh/ssh_host_ed25519_key|$(sysconfdir)/ssh_host_ed25519_key|g' \
	-e 's|/var/run/sshd.pid|$(piddir)/sshd.pid|g' \
	-e 's|/etc/moduli|$(sysconfdir)/moduli|g' \
	-e 's|/etc/ssh/moduli|$(sysconfdir)/moduli|g' \
	-e 's|/etc/ssh/sshrc|$(sysconfdir)/sshrc|g' \
	-e 's|/usr/X11R6/bin/xauth|$(XAUTH_PATH)|g' \
	-e 's|/var/empty|$(PRIVSEP_PATH)|g' \
	-e 's|/usr/bin:/bin:/usr/sbin:/sbin|/usr/bin:/bin:/usr/sbin:/sbin|g'

FIXPATHSCMD	= $(SED) $(PATHSUBS)
FIXALGORITHMSCMD= $(SHELL) $(srcdir)/fixalgorithms $(SED) \
		     

all: $(CONFIGFILES) $(MANPAGES) $(TARGETS)

$(LIBSSH_OBJS): Makefile.in config.h
$(SSHOBJS): Makefile.in config.h
$(SSHDOBJS): Makefile.in config.h

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

LIBCOMPAT=openbsd-compat/libopenbsd-compat.a
$(LIBCOMPAT): always
	(cd openbsd-compat && $(MAKE))
always:

libssh.a: $(LIBSSH_OBJS)
	$(AR) rv $@ $(LIBSSH_OBJS)
	$(RANLIB) $@

ssh$(EXEEXT): $(LIBCOMPAT) libssh.a $(SSHOBJS)
	$(LD) -o $@ $(SSHOBJS) $(LDFLAGS) -lssh -lopenbsd-compat $(SSHLIBS) $(LIBS) $(GSSLIBS)

sshd$(EXEEXT): libssh.a	$(LIBCOMPAT) $(SSHDOBJS)
	$(LD) -o $@ $(SSHDOBJS) $(LDFLAGS) -lssh -lopenbsd-compat $(SSHDLIBS) $(LIBS) $(GSSLIBS) $(K5LIBS)

scp$(EXEEXT): $(LIBCOMPAT) libssh.a scp.o progressmeter.o
	$(LD) -o $@ scp.o progressmeter.o bufaux.o $(LDFLAGS) -lssh -lopenbsd-compat $(LIBS)

ssh-add$(EXEEXT): $(LIBCOMPAT) libssh.a ssh-add.o
	$(LD) -o $@ ssh-add.o $(LDFLAGS) -lssh -lopenbsd-compat $(LIBS)

ssh-agent$(EXEEXT): $(LIBCOMPAT) libssh.a ssh-agent.o ssh-pkcs11-client.o
	$(LD) -o $@ ssh-agent.o ssh-pkcs11-client.o $(LDFLAGS) -lssh -lopenbsd-compat $(LIBS)

ssh-keygen$(EXEEXT): $(LIBCOMPAT) libssh.a ssh-keygen.o
	$(LD) -o $@ ssh-keygen.o $(LDFLAGS) -lssh -lopenbsd-compat $(LIBS)

ssh-keysign$(EXEEXT): $(LIBCOMPAT) libssh.a ssh-keysign.o readconf.o
	$(LD) -o $@ ssh-keysign.o readconf.o $(LDFLAGS) -lssh -lopenbsd-compat $(LIBS)

ssh-pkcs11-helper$(EXEEXT): $(LIBCOMPAT) libssh.a ssh-pkcs11-helper.o ssh-pkcs11.o
	$(LD) -o $@ ssh-pkcs11-helper.o ssh-pkcs11.o $(LDFLAGS) -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

ssh-keyscan$(EXEEXT): $(LIBCOMPAT) libssh.a ssh-keyscan.o
	$(LD) -o $@ ssh-keyscan.o $(LDFLAGS) -lssh -lopenbsd-compat -lssh $(LIBS)

sftp-server$(EXEEXT): $(LIBCOMPAT) libssh.a sftp.o sftp-common.o sftp-server.o sftp-server-main.o
	$(LD) -o $@ sftp-server.o sftp-common.o sftp-server-main.o $(LDFLAGS) -lssh -lopenbsd-compat $(LIBS)

sftp$(EXEEXT): $(LIBCOMPAT) libssh.a sftp.o sftp-client.o sftp-common.o sftp-glob.o progressmeter.o
	$(LD) -o $@ progressmeter.o sftp.o sftp-client.o sftp-common.o sftp-glob.o $(LDFLAGS) -lssh -lopenbsd-compat $(LIBS) $(LIBEDIT)

# test driver for the loginrec code - not built by default
logintest: logintest.o $(LIBCOMPAT) libssh.a loginrec.o
	$(LD) -o $@ logintest.o $(LDFLAGS) loginrec.o -lopenbsd-compat -lssh $(LIBS)

$(MANPAGES): $(MANPAGES_IN)
	if test "$(MANTYPE)" = "cat"; then \
		manpage=$(srcdir)/`echo $@ | sed 's/\.[1-9]\.out$$/\.0/'`; \
	else \
		manpage=$(srcdir)/`echo $@ | sed 's/\.out$$//'`; \
	fi; \
	if test "$(MANTYPE)" = "man"; then \
		$(FIXPATHSCMD) $${manpage} | $(FIXALGORITHMSCMD) | \
		    $(AWK) -f $(srcdir)/mdoc2man.awk > $@; \
	else \
		$(FIXPATHSCMD) $${manpage} | $(FIXALGORITHMSCMD) > $@; \
	fi

$(CONFIGFILES): $(CONFIGFILES_IN)
	conffile=`echo $@ | sed 's/.out$$//'`; \
	$(FIXPATHSCMD) $(srcdir)/$${conffile} > $@

# fake rule to stop make trying to compile moduli.o into a binary "moduli.o"
moduli:
	echo

clean:	regressclean
	rm -f *.o *.a $(TARGETS) logintest config.cache config.log
	rm -f *.out core survey
	rm -f regress/check-perm$(EXEEXT)
	rm -f regress/unittests/test_helper/*.a
	rm -f regress/unittests/test_helper/*.o
	rm -f regress/unittests/sshbuf/*.o
	rm -f regress/unittests/sshbuf/test_sshbuf$(EXEEXT)
	rm -f regress/unittests/sshkey/*.o
	rm -f regress/unittests/sshkey/test_sshkey$(EXEEXT)
	rm -f regress/unittests/bitmap/*.o
	rm -f regress/unittests/bitmap/test_bitmap$(EXEEXT)
	rm -f regress/unittests/conversion/*.o
	rm -f regress/unittests/conversion/test_conversion$(EXEEXT)
	rm -f regress/unittests/hostkeys/*.o
	rm -f regress/unittests/hostkeys/test_hostkeys$(EXEEXT)
	rm -f regress/unittests/kex/*.o
	rm -f regress/unittests/kex/test_kex$(EXEEXT)
	rm -f regress/unittests/match/*.o
	rm -f regress/unittests/match/test_match$(EXEEXT)
	rm -f regress/unittests/utf8/*.o
	rm -f regress/unittests/utf8/test_utf8$(EXEEXT)
	rm -f regress/misc/kexfuzz/*.o
	rm -f regress/misc/kexfuzz/kexfuzz$(EXEEXT)
	(cd openbsd-compat && $(MAKE) clean)

distclean:	regressclean
	rm -f *.o *.a $(TARGETS) logintest config.cache config.log
	rm -f *.out core opensshd.init openssh.xml
	rm -f Makefile buildpkg.sh config.h config.status
	rm -f survey.sh openbsd-compat/regress/Makefile *~ 
	rm -rf autom4te.cache
	rm -f regress/unittests/test_helper/*.a
	rm -f regress/unittests/test_helper/*.o
	rm -f regress/unittests/sshbuf/*.o
	rm -f regress/unittests/sshbuf/test_sshbuf
	rm -f regress/unittests/sshkey/*.o
	rm -f regress/unittests/sshkey/test_sshkey
	rm -f regress/unittests/bitmap/*.o
	rm -f regress/unittests/bitmap/test_bitmap
	rm -f regress/unittests/conversion/*.o
	rm -f regress/unittests/conversion/test_conversion
	rm -f regress/unittests/hostkeys/*.o
	rm -f regress/unittests/hostkeys/test_hostkeys
	rm -f regress/unittests/kex/*.o
	rm -f regress/unittests/kex/test_kex
	rm -f regress/unittests/match/*.o
	rm -f regress/unittests/match/test_match
	rm -f regress/unittests/utf8/*.o
	rm -f regress/unittests/utf8/test_utf8
	rm -f regress/unittests/misc/kexfuzz
	(cd openbsd-compat && $(MAKE) distclean)
	if test -d pkg ; then \
		rm -fr pkg ; \
	fi

veryclean: distclean
	rm -f configure config.h.in *.0

cleandir: veryclean

mrproper: veryclean

realclean: veryclean

catman-do:
	@for f in $(MANPAGES_IN) ; do \
		base=`echo $$f | sed 's/\..*$$//'` ; \
		echo "$$f -> $$base.0" ; \
		$(MANFMT) $$f | cat -v | sed -e 's/.\^H//g' \
			>$$base.0 ; \
	done

depend: depend-rebuild
	rm -f .depend.bak

depend-rebuild:
	rm -f config.h
	touch config.h
	makedepend -w1000 -Y. -f .depend *.c 2>/dev/null
	rm -f config.h

depend-check: depend-rebuild
	cmp .depend .depend.bak || (echo .depend stale && exit 1)

distprep: catman-do depend-check
	$(AUTORECONF)
	-rm -rf autom4te.cache .depend.bak

install: $(CONFIGFILES) $(MANPAGES) $(TARGETS) install-files install-sysconf host-key check-config
install-nokeys: $(CONFIGFILES) $(MANPAGES) $(TARGETS) install-files install-sysconf
install-nosysconf: $(CONFIGFILES) $(MANPAGES) $(TARGETS) install-files

check-config:
	-$(DESTDIR)$(sbindir)/sshd -t -f $(DESTDIR)$(sysconfdir)/sshd_config

install-files:
	$(MKDIR_P) $(DESTDIR)$(bindir)
	$(MKDIR_P) $(DESTDIR)$(sbindir)
	$(MKDIR_P) $(DESTDIR)$(mandir)/$(mansubdir)1
	$(MKDIR_P) $(DESTDIR)$(mandir)/$(mansubdir)5
	$(MKDIR_P) $(DESTDIR)$(mandir)/$(mansubdir)8
	$(MKDIR_P) $(DESTDIR)$(libexecdir)
	$(MKDIR_P) -m 0755 $(DESTDIR)$(PRIVSEP_PATH)
	$(INSTALL) -m 0755 $(STRIP_OPT) ssh$(EXEEXT) $(DESTDIR)$(bindir)/ssh$(EXEEXT)
	$(INSTALL) -m 0755 $(STRIP_OPT) scp$(EXEEXT) $(DESTDIR)$(bindir)/scp$(EXEEXT)
	$(INSTALL) -m 0755 $(STRIP_OPT) ssh-add$(EXEEXT) $(DESTDIR)$(bindir)/ssh-add$(EXEEXT)
	$(INSTALL) -m 0755 $(STRIP_OPT) ssh-agent$(EXEEXT) $(DESTDIR)$(bindir)/ssh-agent$(EXEEXT)
	$(INSTALL) -m 0755 $(STRIP_OPT) ssh-keygen$(EXEEXT) $(DESTDIR)$(bindir)/ssh-keygen$(EXEEXT)
	$(INSTALL) -m 0755 $(STRIP_OPT) ssh-keyscan$(EXEEXT) $(DESTDIR)$(bindir)/ssh-keyscan$(EXEEXT)
	$(INSTALL) -m 0755 $(STRIP_OPT) sshd$(EXEEXT) $(DESTDIR)$(sbindir)/sshd$(EXEEXT)
	$(INSTALL) -m 4711 $(STRIP_OPT) ssh-keysign$(EXEEXT) $(DESTDIR)$(SSH_KEYSIGN)$(EXEEXT)
	$(INSTALL) -m 0755 $(STRIP_OPT) ssh-pkcs11-helper$(EXEEXT) $(DESTDIR)$(SSH_PKCS11_HELPER)$(EXEEXT)
	$(INSTALL) -m 0755 $(STRIP_OPT) sftp$(EXEEXT) $(DESTDIR)$(bindir)/sftp$(EXEEXT)
	$(INSTALL) -m 0755 $(STRIP_OPT) sftp-server$(EXEEXT) $(DESTDIR)$(SFTP_SERVER)$(EXEEXT)
	$(INSTALL) -m 644 ssh.1.out $(DESTDIR)$(mandir)/$(mansubdir)1/ssh.1
	$(INSTALL) -m 644 scp.1.out $(DESTDIR)$(mandir)/$(mansubdir)1/scp.1
	$(INSTALL) -m 644 ssh-add.1.out $(DESTDIR)$(mandir)/$(mansubdir)1/ssh-add.1
	$(INSTALL) -m 644 ssh-agent.1.out $(DESTDIR)$(mandir)/$(mansubdir)1/ssh-agent.1
	$(INSTALL) -m 644 ssh-keygen.1.out $(DESTDIR)$(mandir)/$(mansubdir)1/ssh-keygen.1
	$(INSTALL) -m 644 ssh-keyscan.1.out $(DESTDIR)$(mandir)/$(mansubdir)1/ssh-keyscan.1
	$(INSTALL) -m 644 moduli.5.out $(DESTDIR)$(mandir)/$(mansubdir)5/moduli.5
	$(INSTALL) -m 644 sshd_config.5.out $(DESTDIR)$(mandir)/$(mansubdir)5/sshd_config.5
	$(INSTALL) -m 644 ssh_config.5.out $(DESTDIR)$(mandir)/$(mansubdir)5/ssh_config.5
	$(INSTALL) -m 644 sshd.8.out $(DESTDIR)$(mandir)/$(mansubdir)8/sshd.8
	$(INSTALL) -m 644 sftp.1.out $(DESTDIR)$(mandir)/$(mansubdir)1/sftp.1
	$(INSTALL) -m 644 sftp-server.8.out $(DESTDIR)$(mandir)/$(mansubdir)8/sftp-server.8
	$(INSTALL) -m 644 ssh-keysign.8.out $(DESTDIR)$(mandir)/$(mansubdir)8/ssh-keysign.8
	$(INSTALL) -m 644 ssh-pkcs11-helper.8.out $(DESTDIR)$(mandir)/$(mansubdir)8/ssh-pkcs11-helper.8

install-sysconf:
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)
	@if [ ! -f $(DESTDIR)$(sysconfdir)/ssh_config ]; then \
		$(INSTALL) -m 644 ssh_config.out $(DESTDIR)$(sysconfdir)/ssh_config; \
	else \
		echo "$(DESTDIR)$(sysconfdir)/ssh_config already exists, install will not overwrite"; \
	fi
	@if [ ! -f $(DESTDIR)$(sysconfdir)/sshd_config ]; then \
		$(INSTALL) -m 644 sshd_config.out $(DESTDIR)$(sysconfdir)/sshd_config; \
	else \
		echo "$(DESTDIR)$(sysconfdir)/sshd_config already exists, install will not overwrite"; \
	fi
	@if [ ! -f $(DESTDIR)$(sysconfdir)/moduli ]; then \
		if [ -f $(DESTDIR)$(sysconfdir)/primes ]; then \
			echo "moving $(DESTDIR)$(sysconfdir)/primes to $(DESTDIR)$(sysconfdir)/moduli"; \
			mv "$(DESTDIR)$(sysconfdir)/primes" "$(DESTDIR)$(sysconfdir)/moduli"; \
		else \
			$(INSTALL) -m 644 moduli.out $(DESTDIR)$(sysconfdir)/moduli; \
		fi ; \
	else \
		echo "$(DESTDIR)$(sysconfdir)/moduli already exists, install will not overwrite"; \
	fi

host-key: ssh-keygen$(EXEEXT)
	@if [ -z "$(DESTDIR)" ] ; then \
		./ssh-keygen -A; \
	fi

host-key-force: ssh-keygen$(EXEEXT) ssh$(EXEEXT)
	./ssh-keygen -t dsa -f $(DESTDIR)$(sysconfdir)/ssh_host_dsa_key -N ""
	./ssh-keygen -t rsa -f $(DESTDIR)$(sysconfdir)/ssh_host_rsa_key -N ""
	./ssh-keygen -t ed25519 -f $(DESTDIR)$(sysconfdir)/ssh_host_ed25519_key -N ""
	if ./ssh -Q key | grep ecdsa >/dev/null ; then \
		./ssh-keygen -t ecdsa -f $(DESTDIR)$(sysconfdir)/ssh_host_ecdsa_key -N ""; \
	fi

uninstallall:	uninstall
	-rm -f $(DESTDIR)$(sysconfdir)/ssh_config
	-rm -f $(DESTDIR)$(sysconfdir)/sshd_config
	-rmdir $(DESTDIR)$(sysconfdir)
	-rmdir $(DESTDIR)$(bindir)
	-rmdir $(DESTDIR)$(sbindir)
	-rmdir $(DESTDIR)$(mandir)/$(mansubdir)1
	-rmdir $(DESTDIR)$(mandir)/$(mansubdir)8
	-rmdir $(DESTDIR)$(mandir)
	-rmdir $(DESTDIR)$(libexecdir)

uninstall:
	-rm -f $(DESTDIR)$(bindir)/ssh$(EXEEXT)
	-rm -f $(DESTDIR)$(bindir)/scp$(EXEEXT)
	-rm -f $(DESTDIR)$(bindir)/ssh-add$(EXEEXT)
	-rm -f $(DESTDIR)$(bindir)/ssh-agent$(EXEEXT)
	-rm -f $(DESTDIR)$(bindir)/ssh-keygen$(EXEEXT)
	-rm -f $(DESTDIR)$(bindir)/ssh-keyscan$(EXEEXT)
	-rm -f $(DESTDIR)$(bindir)/sftp$(EXEEXT)
	-rm -f $(DESTDIR)$(sbindir)/sshd$(EXEEXT)
	-rm -r $(DESTDIR)$(SFTP_SERVER)$(EXEEXT)
	-rm -f $(DESTDIR)$(SSH_KEYSIGN)$(EXEEXT)
	-rm -f $(DESTDIR)$(SSH_PKCS11_HELPER)$(EXEEXT)
	-rm -f $(DESTDIR)$(mandir)/$(mansubdir)1/ssh.1
	-rm -f $(DESTDIR)$(mandir)/$(mansubdir)1/scp.1
	-rm -f $(DESTDIR)$(mandir)/$(mansubdir)1/ssh-add.1
	-rm -f $(DESTDIR)$(mandir)/$(mansubdir)1/ssh-agent.1
	-rm -f $(DESTDIR)$(mandir)/$(mansubdir)1/ssh-keygen.1
	-rm -f $(DESTDIR)$(mandir)/$(mansubdir)1/sftp.1
	-rm -f $(DESTDIR)$(mandir)/$(mansubdir)1/ssh-keyscan.1
	-rm -f $(DESTDIR)$(mandir)/$(mansubdir)8/sshd.8
	-rm -f $(DESTDIR)$(mandir)/$(mansubdir)8/sftp-server.8
	-rm -f $(DESTDIR)$(mandir)/$(mansubdir)8/ssh-keysign.8
	-rm -f $(DESTDIR)$(mandir)/$(mansubdir)8/ssh-pkcs11-helper.8

regress-prep:
	$(MKDIR_P) `pwd`/regress/unittests/test_helper
	$(MKDIR_P) `pwd`/regress/unittests/sshbuf
	$(MKDIR_P) `pwd`/regress/unittests/sshkey
	$(MKDIR_P) `pwd`/regress/unittests/bitmap
	$(MKDIR_P) `pwd`/regress/unittests/conversion
	$(MKDIR_P) `pwd`/regress/unittests/hostkeys
	$(MKDIR_P) `pwd`/regress/unittests/kex
	$(MKDIR_P) `pwd`/regress/unittests/match
	$(MKDIR_P) `pwd`/regress/unittests/utf8
	$(MKDIR_P) `pwd`/regress/misc/kexfuzz
	[ -f `pwd`/regress/Makefile ] || \
	    ln -s `cd $(srcdir) && pwd`/regress/Makefile `pwd`/regress/Makefile

REGRESSLIBS=libssh.a $(LIBCOMPAT)

regress/modpipe$(EXEEXT): $(srcdir)/regress/modpipe.c $(REGRESSLIBS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(srcdir)/regress/modpipe.c \
	$(LDFLAGS) -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

regress/setuid-allowed$(EXEEXT): $(srcdir)/regress/setuid-allowed.c $(REGRESSLIBS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(srcdir)/regress/setuid-allowed.c \
	$(LDFLAGS) -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

regress/netcat$(EXEEXT): $(srcdir)/regress/netcat.c $(REGRESSLIBS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(srcdir)/regress/netcat.c \
	$(LDFLAGS) -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

regress/check-perm$(EXEEXT): $(srcdir)/regress/check-perm.c $(REGRESSLIBS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(srcdir)/regress/check-perm.c \
	$(LDFLAGS) -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

UNITTESTS_TEST_HELPER_OBJS=\
	regress/unittests/test_helper/test_helper.o \
	regress/unittests/test_helper/fuzz.o

regress/unittests/test_helper/libtest_helper.a: ${UNITTESTS_TEST_HELPER_OBJS}
	$(AR) rv $@ $(UNITTESTS_TEST_HELPER_OBJS)
	$(RANLIB) $@

UNITTESTS_TEST_SSHBUF_OBJS=\
	regress/unittests/sshbuf/tests.o \
	regress/unittests/sshbuf/test_sshbuf.o \
	regress/unittests/sshbuf/test_sshbuf_getput_basic.o \
	regress/unittests/sshbuf/test_sshbuf_getput_crypto.o \
	regress/unittests/sshbuf/test_sshbuf_misc.o \
	regress/unittests/sshbuf/test_sshbuf_fuzz.o \
	regress/unittests/sshbuf/test_sshbuf_getput_fuzz.o \
	regress/unittests/sshbuf/test_sshbuf_fixed.o

regress/unittests/sshbuf/test_sshbuf$(EXEEXT): ${UNITTESTS_TEST_SSHBUF_OBJS} \
    regress/unittests/test_helper/libtest_helper.a libssh.a
	$(LD) -o $@ $(LDFLAGS) $(UNITTESTS_TEST_SSHBUF_OBJS) \
	    regress/unittests/test_helper/libtest_helper.a \
	    -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

UNITTESTS_TEST_SSHKEY_OBJS=\
	regress/unittests/sshkey/test_fuzz.o \
	regress/unittests/sshkey/tests.o \
	regress/unittests/sshkey/common.o \
	regress/unittests/sshkey/test_file.o \
	regress/unittests/sshkey/test_sshkey.o

regress/unittests/sshkey/test_sshkey$(EXEEXT): ${UNITTESTS_TEST_SSHKEY_OBJS} \
    regress/unittests/test_helper/libtest_helper.a libssh.a
	$(LD) -o $@ $(LDFLAGS) $(UNITTESTS_TEST_SSHKEY_OBJS) \
	    regress/unittests/test_helper/libtest_helper.a \
	    -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

UNITTESTS_TEST_BITMAP_OBJS=\
	regress/unittests/bitmap/tests.o

regress/unittests/bitmap/test_bitmap$(EXEEXT): ${UNITTESTS_TEST_BITMAP_OBJS} \
    regress/unittests/test_helper/libtest_helper.a libssh.a
	$(LD) -o $@ $(LDFLAGS) $(UNITTESTS_TEST_BITMAP_OBJS) \
	    regress/unittests/test_helper/libtest_helper.a \
	    -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

UNITTESTS_TEST_CONVERSION_OBJS=\
	regress/unittests/conversion/tests.o

regress/unittests/conversion/test_conversion$(EXEEXT): \
    ${UNITTESTS_TEST_CONVERSION_OBJS} \
    regress/unittests/test_helper/libtest_helper.a libssh.a
	$(LD) -o $@ $(LDFLAGS) $(UNITTESTS_TEST_CONVERSION_OBJS) \
	    regress/unittests/test_helper/libtest_helper.a \
	    -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

UNITTESTS_TEST_KEX_OBJS=\
	regress/unittests/kex/tests.o \
	regress/unittests/kex/test_kex.o

regress/unittests/kex/test_kex$(EXEEXT): ${UNITTESTS_TEST_KEX_OBJS} \
    regress/unittests/test_helper/libtest_helper.a libssh.a
	$(LD) -o $@ $(LDFLAGS) $(UNITTESTS_TEST_KEX_OBJS) \
	    regress/unittests/test_helper/libtest_helper.a \
	    -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

UNITTESTS_TEST_HOSTKEYS_OBJS=\
	regress/unittests/hostkeys/tests.o \
	regress/unittests/hostkeys/test_iterate.o

regress/unittests/hostkeys/test_hostkeys$(EXEEXT): \
    ${UNITTESTS_TEST_HOSTKEYS_OBJS} \
    regress/unittests/test_helper/libtest_helper.a libssh.a
	$(LD) -o $@ $(LDFLAGS) $(UNITTESTS_TEST_HOSTKEYS_OBJS) \
	    regress/unittests/test_helper/libtest_helper.a \
	    -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

UNITTESTS_TEST_MATCH_OBJS=\
	regress/unittests/match/tests.o

regress/unittests/match/test_match$(EXEEXT): \
    ${UNITTESTS_TEST_MATCH_OBJS} \
    regress/unittests/test_helper/libtest_helper.a libssh.a
	$(LD) -o $@ $(LDFLAGS) $(UNITTESTS_TEST_MATCH_OBJS) \
	    regress/unittests/test_helper/libtest_helper.a \
	    -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

UNITTESTS_TEST_UTF8_OBJS=\
	regress/unittests/utf8/tests.o

regress/unittests/utf8/test_utf8$(EXEEXT): \
    ${UNITTESTS_TEST_UTF8_OBJS} \
    regress/unittests/test_helper/libtest_helper.a libssh.a
	$(LD) -o $@ $(LDFLAGS) $(UNITTESTS_TEST_UTF8_OBJS) \
	    regress/unittests/test_helper/libtest_helper.a \
	    -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

MISC_KEX_FUZZ_OBJS=\
	regress/misc/kexfuzz/kexfuzz.o

regress/misc/kexfuzz/kexfuzz$(EXEEXT): ${MISC_KEX_FUZZ_OBJS} libssh.a
	$(LD) -o $@ $(LDFLAGS) $(MISC_KEX_FUZZ_OBJS) \
	    -lssh -lopenbsd-compat -lssh -lopenbsd-compat $(LIBS)

regress-binaries: regress/modpipe$(EXEEXT) \
	regress/setuid-allowed$(EXEEXT) \
	regress/netcat$(EXEEXT) \
	regress/check-perm$(EXEEXT) \
	regress/unittests/sshbuf/test_sshbuf$(EXEEXT) \
	regress/unittests/sshkey/test_sshkey$(EXEEXT) \
	regress/unittests/bitmap/test_bitmap$(EXEEXT) \
	regress/unittests/conversion/test_conversion$(EXEEXT) \
	regress/unittests/hostkeys/test_hostkeys$(EXEEXT) \
	regress/unittests/kex/test_kex$(EXEEXT) \
	regress/unittests/match/test_match$(EXEEXT) \
	regress/unittests/utf8/test_utf8$(EXEEXT) \
	regress/misc/kexfuzz/kexfuzz$(EXEEXT)

REGRESSTMP = "$(PWD)/regress"

tests interop-tests t-exec unit: regress-prep regress-binaries $(TARGETS)
	BUILDDIR=`pwd`; \
	TEST_SSH_SCP="$${BUILDDIR}/scp"; \
	TEST_SSH_SSH="$${BUILDDIR}/ssh"; \
	TEST_SSH_SSHD="$${BUILDDIR}/sshd"; \
	TEST_SSH_SSHAGENT="$${BUILDDIR}/ssh-agent"; \
	TEST_SSH_SSHADD="$${BUILDDIR}/ssh-add"; \
	TEST_SSH_SSHKEYGEN="$${BUILDDIR}/ssh-keygen"; \
	TEST_SSH_SSHPKCS11HELPER="$${BUILDDIR}/ssh-pkcs11-helper"; \
	TEST_SSH_SSHKEYSCAN="$${BUILDDIR}/ssh-keyscan"; \
	TEST_SSH_SFTP="$${BUILDDIR}/sftp"; \
	TEST_SSH_SFTPSERVER="$${BUILDDIR}/sftp-server"; \
	TEST_SSH_PLINK="plink"; \
	TEST_SSH_PUTTYGEN="puttygen"; \
	TEST_SSH_CONCH="conch"; \
	TEST_SSH_IPV6="yes" ; \
	TEST_SSH_UTF8="yes" ; \
	TEST_SSH_ECC="yes" ; \
	cd $(srcdir)/regress || exit $$?; \
	$(MAKE) \
		.OBJDIR="$${BUILDDIR}/regress" \
		.CURDIR="`pwd`" \
		BUILDDIR="$${BUILDDIR}" \
		OBJ="$(REGRESSTMP)" \
		PATH="$${BUILDDIR}:$${PATH}" \
		TEST_ENV=MALLOC_OPTIONS="" \
		TEST_MALLOC_OPTIONS="" \
		TEST_SSH_SCP="$${TEST_SSH_SCP}" \
		TEST_SSH_SSH="$${TEST_SSH_SSH}" \
		TEST_SSH_SSHD="$${TEST_SSH_SSHD}" \
		TEST_SSH_SSHAGENT="$${TEST_SSH_SSHAGENT}" \
		TEST_SSH_SSHADD="$${TEST_SSH_SSHADD}" \
		TEST_SSH_SSHKEYGEN="$${TEST_SSH_SSHKEYGEN}" \
		TEST_SSH_SSHPKCS11HELPER="$${TEST_SSH_SSHPKCS11HELPER}" \
		TEST_SSH_SSHKEYSCAN="$${TEST_SSH_SSHKEYSCAN}" \
		TEST_SSH_SFTP="$${TEST_SSH_SFTP}" \
		TEST_SSH_SFTPSERVER="$${TEST_SSH_SFTPSERVER}" \
		TEST_SSH_PLINK="$${TEST_SSH_PLINK}" \
		TEST_SSH_PUTTYGEN="$${TEST_SSH_PUTTYGEN}" \
		TEST_SSH_CONCH="$${TEST_SSH_CONCH}" \
		TEST_SSH_IPV6="$${TEST_SSH_IPV6}" \
		TEST_SSH_UTF8="$${TEST_SSH_UTF8}" \
		TEST_SSH_ECC="$${TEST_SSH_ECC}" \
		TEST_SHELL="${TEST_SHELL}" \
		EXEEXT="$(EXEEXT)" \
		$@ && echo all tests passed

compat-tests: $(LIBCOMPAT)
	(cd openbsd-compat/regress && $(MAKE))

regressclean:
	if [ -f regress/Makefile ] && [ -r regress/Makefile ]; then \
		(cd regress && $(MAKE) clean) \
	fi

survey: survey.sh ssh
	@$(SHELL) ./survey.sh > survey
	@echo 'The survey results have been placed in the file "survey" in the'
	@echo 'current directory.  Please review the file then send with'
	@echo '"make send-survey".'

send-survey:	survey
	mail portable-survey@mindrot.org <survey

package: $(CONFIGFILES) $(MANPAGES) $(TARGETS)
	if [ "no" = yes ]; then \
		sh buildpkg.sh; \
	fi

# # DO NOT DELETE

addrmatch.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h match.h log.h
atomicio.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h atomicio.h
audit-bsm.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
audit-linux.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
audit.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
auth-bsdauth.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
auth-krb5.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h ssh.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h log.h misc.h servconf.h uidswap.h key.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h
auth-options.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h ssherr.h log.h misc.h sshkey.h match.h ssh2.h auth-options.h
auth-pam.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
auth-passwd.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h log.h misc.h servconf.h key.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h auth-options.h
auth-rhosts.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h uidswap.h pathnames.h log.h misc.h key.h sshkey.h servconf.h canohost.h hostfile.h auth.h auth-pam.h audit.h loginrec.h
auth-shadow.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
auth-sia.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
auth-skey.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
auth.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h match.h groupaccess.h log.h misc.h servconf.h key.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h auth-options.h canohost.h uidswap.h packet.h openbsd-compat/sys-queue.h
auth.o: dispatch.h opacket.h authfile.h monitor_wrap.h ssherr.h compat.h channels.h
auth2-chall.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h ssh2.h key.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h log.h misc.h servconf.h
auth2-gss.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
auth2-hostbased.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h ssh2.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h log.h misc.h servconf.h compat.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h canohost.h
auth2-hostbased.o: monitor_wrap.h pathnames.h ssherr.h match.h
auth2-kbdint.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h key.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h log.h misc.h servconf.h
auth2-none.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h atomicio.h xmalloc.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h log.h misc.h servconf.h compat.h ssh2.h ssherr.h
auth2-none.o: monitor_wrap.h
auth2-passwd.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h ssherr.h log.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h monitor_wrap.h misc.h servconf.h
auth2-pubkey.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h ssh.h ssh2.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h log.h misc.h servconf.h compat.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h pathnames.h uidswap.h
auth2-pubkey.o: auth-options.h canohost.h monitor_wrap.h authfile.h match.h ssherr.h channels.h session.h
auth2.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h atomicio.h xmalloc.h ssh2.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h log.h misc.h servconf.h compat.h key.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h pathnames.h
auth2.o: monitor_wrap.h ssherr.h
authfd.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h ssh.h sshkey.h authfd.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h compat.h log.h atomicio.h misc.h ssherr.h
authfile.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h ssh.h log.h authfile.h misc.h atomicio.h sshkey.h ssherr.h krl.h
bitmap.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h bitmap.h
bufaux.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h ssherr.h
bufbn.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
bufec.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h ssherr.h
buffer.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h ssherr.h
canohost.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h log.h canohost.h misc.h
chacha.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h chacha.h
channels.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h ssh.h ssh2.h ssherr.h packet.h dispatch.h opacket.h log.h misc.h channels.h compat.h canohost.h key.h sshkey.h authfd.h pathnames.h
cipher-aes.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/openssl-compat.h
cipher-aesctr.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h cipher-aesctr.h rijndael.h
cipher-chachapoly.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h ssherr.h cipher-chachapoly.h chacha.h poly1305.h
cipher-ctr.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
cipher.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h misc.h ssherr.h digest.h openbsd-compat/openssl-compat.h
cleanup.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h
clientloop.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h ssh.h ssh2.h packet.h dispatch.h opacket.h compat.h channels.h key.h sshkey.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h
clientloop.o: kex.h mac.h myproposal.h log.h misc.h readconf.h clientloop.h sshconnect.h authfd.h atomicio.h sshpty.h match.h msg.h ssherr.h hostfile.h
compat.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h compat.h log.h match.h kex.h mac.h key.h sshkey.h
crc32.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h crc32.h
dh.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
digest-libc.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ssherr.h digest.h
digest-openssl.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
dispatch.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ssh2.h log.h dispatch.h packet.h openbsd-compat/sys-queue.h opacket.h compat.h ssherr.h
dns.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h sshkey.h ssherr.h dns.h log.h digest.h
ed25519.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h crypto_api.h ge25519.h fe25519.h sc25519.h
entropy.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
fatal.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h
fe25519.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h fe25519.h crypto_api.h
ge25519.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h fe25519.h crypto_api.h sc25519.h ge25519.h ge25519_base.data
groupaccess.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h groupaccess.h match.h log.h
gss-genr.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
gss-serv-krb5.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
gss-serv.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
hash.o: crypto_api.h includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h digest.h log.h ssherr.h
hmac.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h digest.h hmac.h
hostfile.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h match.h sshkey.h hostfile.h log.h misc.h ssherr.h digest.h hmac.h
kex.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ssh2.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h compat.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h sshkey.h kex.h mac.h key.h log.h match.h misc.h
kex.o: monitor.h ssherr.h digest.h
kexc25519.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ssh2.h sshkey.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h kex.h mac.h key.h log.h digest.h ssherr.h
kexc25519c.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h sshkey.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h kex.h mac.h key.h log.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h ssh2.h digest.h ssherr.h
kexc25519s.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h sshkey.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h digest.h kex.h mac.h key.h log.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h ssh2.h ssherr.h
kexdh.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
kexdhc.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
kexdhs.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
kexecdh.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
kexecdhc.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
kexecdhs.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
kexgex.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
kexgexc.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
kexgexs.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
key.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h key.h sshkey.h compat.h ssherr.h log.h authfile.h
krl.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ./openbsd-compat/sys-tree.h openbsd-compat/sys-queue.h ssherr.h sshkey.h authfile.h misc.h log.h digest.h bitmap.h krl.h
log.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h
loginrec.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h key.h sshkey.h hostfile.h ssh.h loginrec.h log.h atomicio.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h canohost.h auth.h auth-pam.h audit.h
logintest.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h loginrec.h
mac.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h digest.h hmac.h umac.h mac.h misc.h ssherr.h openbsd-compat/openssl-compat.h
match.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h match.h misc.h
md5crypt.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
misc.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h misc.h log.h ssh.h ssherr.h uidswap.h
moduli.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
monitor.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ./openbsd-compat/sys-tree.h openbsd-compat/sys-queue.h atomicio.h xmalloc.h ssh.h key.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h cipher.h cipher-chachapoly.h chacha.h poly1305.h
monitor.o: cipher-aesctr.h rijndael.h kex.h mac.h dh.h packet.h dispatch.h opacket.h auth-options.h sshpty.h channels.h session.h sshlogin.h canohost.h log.h misc.h servconf.h monitor.h monitor_wrap.h monitor_fdpass.h compat.h ssh2.h authfd.h match.h ssherr.h
monitor_fdpass.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h monitor_fdpass.h
monitor_wrap.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h ssh.h key.h sshkey.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h kex.h mac.h hostfile.h auth.h auth-pam.h audit.h
monitor_wrap.o: loginrec.h auth-options.h packet.h dispatch.h opacket.h log.h monitor.h monitor_wrap.h atomicio.h monitor_fdpass.h misc.h channels.h session.h servconf.h ssherr.h
msg.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ssherr.h log.h atomicio.h msg.h misc.h
mux.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h log.h ssh.h ssh2.h pathnames.h misc.h match.h channels.h msg.h packet.h dispatch.h opacket.h monitor_fdpass.h sshpty.h key.h sshkey.h readconf.h clientloop.h
mux.o: ssherr.h
nchan.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h ssh2.h ssherr.h packet.h dispatch.h opacket.h channels.h compat.h log.h
opacket.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ssherr.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h log.h
packet.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h key.h sshkey.h xmalloc.h crc32.h compat.h ssh2.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h kex.h mac.h digest.h log.h canohost.h misc.h
packet.o: channels.h ssh.h packet.h dispatch.h opacket.h ssherr.h
platform-misc.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
platform-pledge.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
platform-tracing.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h
platform.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h misc.h servconf.h key.h sshkey.h hostfile.h auth.h auth-pam.h audit.h loginrec.h
poly1305.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h poly1305.h
progressmeter.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h progressmeter.h atomicio.h misc.h
readconf.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/glob.h xmalloc.h ssh.h compat.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h pathnames.h log.h sshkey.h misc.h readconf.h match.h kex.h mac.h key.h
readconf.o: uidswap.h myproposal.h digest.h
readpass.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h misc.h pathnames.h log.h ssh.h uidswap.h
rijndael.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h rijndael.h
sandbox-capsicum.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
sandbox-darwin.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
sandbox-null.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
sandbox-pledge.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
sandbox-rlimit.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
sandbox-seccomp-filter.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
sandbox-solaris.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
sandbox-systrace.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
sc25519.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h sc25519.h crypto_api.h
scp.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h ssh.h atomicio.h pathnames.h log.h misc.h progressmeter.h utf8.h
servconf.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h ssh.h log.h misc.h servconf.h compat.h pathnames.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h key.h sshkey.h kex.h mac.h
servconf.o: match.h channels.h groupaccess.h canohost.h packet.h dispatch.h opacket.h hostfile.h auth.h auth-pam.h audit.h loginrec.h myproposal.h digest.h
serverloop.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h packet.h dispatch.h opacket.h log.h misc.h servconf.h canohost.h sshpty.h channels.h compat.h ssh2.h key.h sshkey.h cipher.h cipher-chachapoly.h chacha.h
serverloop.o: poly1305.h cipher-aesctr.h rijndael.h kex.h mac.h hostfile.h auth.h auth-pam.h audit.h loginrec.h session.h auth-options.h serverloop.h ssherr.h
session.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h ssh.h ssh2.h sshpty.h packet.h dispatch.h opacket.h match.h uidswap.h compat.h channels.h key.h sshkey.h cipher.h cipher-chachapoly.h chacha.h poly1305.h
session.o: cipher-aesctr.h rijndael.h hostfile.h auth.h auth-pam.h audit.h loginrec.h auth-options.h authfd.h pathnames.h log.h misc.h servconf.h sshlogin.h serverloop.h canohost.h session.h kex.h mac.h monitor_wrap.h sftp.h atomicio.h
sftp-client.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h ssherr.h log.h atomicio.h progressmeter.h misc.h utf8.h sftp.h sftp-common.h sftp-client.h openbsd-compat/glob.h
sftp-common.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h ssherr.h log.h misc.h sftp.h sftp-common.h
sftp-glob.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h sftp.h sftp-common.h sftp-client.h openbsd-compat/glob.h
sftp-server-main.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h sftp.h misc.h xmalloc.h
sftp-server.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h ssherr.h log.h misc.h match.h uidswap.h sftp.h sftp-common.h
sftp.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h log.h pathnames.h misc.h utf8.h sftp.h ssherr.h sftp-common.h sftp-client.h openbsd-compat/glob.h
ssh-add.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/openssl-compat.h xmalloc.h ssh.h log.h sshkey.h authfd.h authfile.h pathnames.h misc.h ssherr.h digest.h
ssh-agent.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h ssh.h sshkey.h authfd.h compat.h log.h misc.h digest.h ssherr.h match.h
ssh-dss.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
ssh-ecdsa.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
ssh-ed25519.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h crypto_api.h log.h sshkey.h ssherr.h ssh.h
ssh-keygen.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h sshkey.h authfile.h uuencode.h pathnames.h log.h misc.h match.h hostfile.h dns.h ssh.h ssh2.h ssherr.h ssh-pkcs11.h atomicio.h krl.h digest.h utf8.h authfd.h
ssh-keyscan.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h ssh.h sshkey.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h kex.h mac.h key.h compat.h myproposal.h packet.h dispatch.h
ssh-keyscan.o: opacket.h log.h atomicio.h misc.h hostfile.h ssherr.h ssh_api.h ssh2.h dns.h
ssh-keysign.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h log.h sshkey.h ssh.h ssh2.h misc.h authfile.h msg.h canohost.h pathnames.h readconf.h uidswap.h ssherr.h
ssh-pkcs11-client.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
ssh-pkcs11-helper.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h log.h misc.h sshkey.h authfd.h ssh-pkcs11.h ssherr.h
ssh-pkcs11.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
ssh-rsa.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
ssh-xmss.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
ssh.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/openssl-compat.h openbsd-compat/sys-queue.h xmalloc.h ssh.h ssh2.h canohost.h compat.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h digest.h packet.h
ssh.o: dispatch.h opacket.h channels.h key.h sshkey.h authfd.h authfile.h pathnames.h clientloop.h log.h misc.h readconf.h sshconnect.h kex.h mac.h sshpty.h match.h msg.h uidswap.h version.h ssherr.h myproposal.h utf8.h
ssh_api.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ssh_api.h openbsd-compat/sys-queue.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h sshkey.h kex.h mac.h key.h ssh.h ssh2.h packet.h dispatch.h opacket.h compat.h
ssh_api.o: log.h authfile.h misc.h version.h myproposal.h ssherr.h
sshbuf-getput-basic.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ssherr.h
sshbuf-getput-crypto.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ssherr.h
sshbuf-misc.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ssherr.h
sshbuf.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ssherr.h misc.h
sshconnect.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h key.h sshkey.h hostfile.h ssh.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h uidswap.h compat.h sshconnect.h log.h misc.h readconf.h atomicio.h dns.h monitor_fdpass.h
sshconnect.o: ssh2.h version.h authfile.h ssherr.h authfd.h
sshconnect2.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h openbsd-compat/sys-queue.h xmalloc.h ssh.h ssh2.h packet.h dispatch.h opacket.h compat.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h key.h sshkey.h kex.h mac.h
sshconnect2.o: myproposal.h sshconnect.h authfile.h dh.h authfd.h log.h misc.h readconf.h match.h canohost.h msg.h pathnames.h uidswap.h hostfile.h ssherr.h utf8.h
sshd.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h ./openbsd-compat/sys-tree.h openbsd-compat/sys-queue.h xmalloc.h ssh.h ssh2.h sshpty.h packet.h dispatch.h opacket.h log.h misc.h match.h servconf.h uidswap.h compat.h cipher.h cipher-chachapoly.h
sshd.o: chacha.h poly1305.h cipher-aesctr.h rijndael.h digest.h key.h sshkey.h kex.h mac.h myproposal.h authfile.h pathnames.h atomicio.h canohost.h hostfile.h auth.h auth-pam.h audit.h loginrec.h authfd.h msg.h channels.h session.h monitor.h monitor_wrap.h ssh-sandbox.h auth-options.h version.h ssherr.h
ssherr.o: ssherr.h
sshkey-xmss.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
sshkey.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h crypto_api.h ssh2.h ssherr.h misc.h cipher.h cipher-chachapoly.h chacha.h poly1305.h cipher-aesctr.h rijndael.h digest.h sshkey.h sshkey-xmss.h match.h xmss_fast.h
sshlogin.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h loginrec.h log.h misc.h servconf.h
sshpty.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h sshpty.h log.h misc.h
sshtty.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h sshpty.h
ttymodes.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h packet.h openbsd-compat/sys-queue.h dispatch.h opacket.h log.h compat.h ttymodes.h
uidswap.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h log.h uidswap.h xmalloc.h
umac.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h umac.h misc.h rijndael.h
umac128.o: umac.c includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h umac.h misc.h rijndael.h
utf8.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h utf8.h
uuencode.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h uuencode.h
verify.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h crypto_api.h
xmalloc.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h xmalloc.h log.h
xmss_commons.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
xmss_fast.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
xmss_hash.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
xmss_hash_address.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
xmss_wots.o: includes.h config.h defines.h platform.h openbsd-compat/openbsd-compat.h openbsd-compat/base64.h openbsd-compat/sigact.h openbsd-compat/readpassphrase.h openbsd-compat/vis.h openbsd-compat/getrrsetbyname.h openbsd-compat/sha1.h openbsd-compat/sha2.h openbsd-compat/rmd160.h openbsd-compat/md5.h openbsd-compat/blf.h openbsd-compat/getopt.h openbsd-compat/bsd-misc.h openbsd-compat/bsd-setres_id.h openbsd-compat/bsd-signal.h openbsd-compat/bsd-statvfs.h openbsd-compat/bsd-waitpid.h openbsd-compat/bsd-poll.h openbsd-compat/fake-rfc2553.h openbsd-compat/bsd-cygwin_util.h openbsd-compat/port-aix.h openbsd-compat/port-irix.h openbsd-compat/port-linux.h openbsd-compat/port-solaris.h openbsd-compat/port-net.h openbsd-compat/port-uw.h openbsd-compat/bsd-nextstep.h entropy.h buffer.h sshbuf.h
