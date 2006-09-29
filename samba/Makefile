# Set these variables as needed, then include this file, then:
#

# Project info
Project         = samba/source
UserType        = Administration
ToolType        = Services

GnuNoChown      = YES
GnuAfterInstall = install-startup-items install-config install-logdir install-testtools install-strip plugins

Extra_CC_Flags  = -mdynamic-no-pic  -no-cpp-precomp -I$(SRCROOT)/libopendirectorycommon -F/System/Library/PrivateFrameworks\
		-DUSES_RECVFROM -DWITH_OPENDIRECTORY -DWITH_MEMBERD -DUSES_KEYCHAIN -DWITH_SACL

ifneq "$(RC_RELEASE)" "Darwin"
Extra_CC_Flags += -DWITH_BRLM
endif

Extra_Configure_Flags = --with-swatdir="$(SHAREDIR)/swat"			\
			--with-sambabook="$(SHAREDIR)/swat/using_samba"		\
			--with-configdir="/private/etc"				\
			--with-privatedir="$(VARDIR)/db/samba"			\
			--with-libdir="/usr/lib/samba"				\
			--with-lockdir="$(VARDIR)/samba"			\
			--with-logfilebase="$(LOGDIR)/samba"			\
			--with-piddir="$(RUNDIR)"				\
			--with-krb5						\
			--with-ads						\
			--with-cups						\
			--with-ldap						\
			--with-spinlocks					\
			--with-libiconv						\
			--without-readline					\
			--disable-shared					\
			--without-libsmbclient					\
			--with-winbind						\
			--with-pam						

Extra_Install_Flags   = SWATDIR="$(DSTROOT)$(SHAREDIR)/swat"			\
			SAMBABOOK="$(DSTROOT)$(SHAREDIR)/swat/using_samba"	\
			PRIVATEDIR="$(DSTROOT)$(VARDIR)/db/samba"		\
			VARDIR="$(DSTROOT)$(VARDIR)"				\
			LIBDIR="$(DSTROOT)/usr/lib/samba"			\
			PIDDIR="$(DSTROOT)$(RUNDIR)"				\
			MANDIR="$(DSTROOT)/usr/share/man"			\
			LOCKDIR="$(DSTROOT)$(VARDIR)/samba"			\
			CONFIGDIR="$(DSTROOT)/private/etc"
			
Environment += EXTRA_BIN_PROGS="bin/smbget@EXEEXT@" \
EXTRA_ALL_TARGETS="bin/smbtorture@EXEEXT@ bin/msgtest@EXEEXT@ bin/masktest@EXEEXT@ bin/locktest@EXEEXT@ bin/locktest2@EXEEXT@ bin/vfstest@EXEEXT@"

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

ifneq "$(RC_RELEASE)" "Darwin"
LDFLAGS += -framework ByteRangeLocking
endif

LDFLAGS += -framework Security -framework CoreFoundation -framework DirectoryService -L$(OBJROOT) -lopendirectorycommon

PATCHES = $(wildcard $(SRCROOT)/patches/*.diff)

Install_Target = install

lazy_install_source::
	gcc $(RC_CFLAGS) -framework Security -DUSES_KEYCHAIN=1  -fPIC -c $(SRCROOT)/libopendirectorycommon/libopendirectorycommon.c -o $(OBJROOT)/libopendirectorycommon.o
	libtool -static -o $(OBJROOT)/libopendirectorycommon.a $(OBJROOT)/libopendirectorycommon.o

patch: $(PATCHES)
	for PATCH in $(PATCHES); do	\
	    echo "patching: $$PATCH";	\
	    patch -p0 -i "$$PATCH";	\
	done
	
repatch:
	for PATCH in $(PATCHES); do					\
	    echo "patching: $${PATCH##*/}";				\
	    patch -p0 -b -i "$$PATCH";					\
	    find samba -type f -name '*.orig' |				\
		while read F; do					\
		    echo -e "\t* $$F";					\
		    diff -udbNp "$$F" "$${F%.orig}" >> "$$PATCH.new";	\
		    mv "$$F" "$${F%.orig}";				\
		done;							\
		mv "$$PATCH.new" "$$PATCH";				\
	done

install-startup-items:
	$(INSTALL) -d -m 755 $(DSTROOT)/System/Library/LaunchDaemons
	$(INSTALL) -c -m 444 $(SRCROOT)/smbd.plist $(DSTROOT)/System/Library/LaunchDaemons/smbd.plist
	$(INSTALL) -c -m 444 $(SRCROOT)/nmbd.plist $(DSTROOT)/System/Library/LaunchDaemons/nmbd.plist
	$(INSTALL) -c -m 444 $(SRCROOT)/swat.plist $(DSTROOT)/System/Library/LaunchDaemons/swat.plist

install-config:
	$(INSTALL) -d -m 755 $(DSTROOT)/usr/local/OpenSourceVersions
	$(INSTALL) -d -m 755 $(DSTROOT)/private/etc
	$(INSTALL) -c -m 444 $(SRCROOT)/smb.conf.template $(DSTROOT)/private/etc
	$(INSTALL) -c -m 444 $(SRCROOT)/samba.plist $(DSTROOT)/usr/local/OpenSourceVersions
	$(INSTALL) -c -m 444 $(SRCROOT)/tdbtool.8 $(DSTROOT)/usr/share/man/man8/tdbtool.8

install-logdir:
	$(INSTALL) -d -m 755 $(DSTROOT)/private/var/log/samba
	$(INSTALL) -d -m 777 $(DSTROOT)/private/var/spool/samba


install-strip:
	for F in $(DSTROOT)/usr/{s,}bin/*; do	\
		cp "$$F" $(SYMROOT); \
		[ -f "$$F" -a -x "$$F" ] && strip -x "$$F";	\
	done
	rmdir $(DSTROOT)/$(RUNDIR)
	rm -f $(DSTROOT)/usr/share/man/man8/smbmnt.8
	rm -f $(DSTROOT)/usr/share/man/man8/smbmount.8
	rm -f $(DSTROOT)/usr/share/man/man8/smbumount.8
	rm -f $(DSTROOT)/usr/share/swat/help/smbmnt.8.html
	rm -f $(DSTROOT)/usr/share/swat/help/smbmount.8.html
	rm -f $(DSTROOT)/usr/share/swat/help/smbumount.8.html

install-testtools:
	$(INSTALL) -d -m 755 $(DSTROOT)/usr/local/bin
	$(INSTALL) -c -m 555 $(OBJROOT)/bin/smbtorture $(DSTROOT)/usr/local/bin/smbtorture
	$(INSTALL) -c -m 555 $(OBJROOT)/bin/masktest $(DSTROOT)/usr/local/bin/masktest
	$(INSTALL) -c -m 555 $(OBJROOT)/bin/vfstest $(DSTROOT)/usr/local/bin/vfstest
	$(INSTALL) -c -m 555 $(OBJROOT)/bin/msgtest $(DSTROOT)/usr/local/bin/msgtest
	$(INSTALL) -c -m 555 $(OBJROOT)/bin/locktest $(DSTROOT)/usr/local/bin/locktest
	$(INSTALL) -c -m 555 $(OBJROOT)/bin/locktest2 $(DSTROOT)/usr/local/bin/locktest2
	
plugins:
	echo "building $@";
	make -C $(SRCROOT)/auth_ods -f auth_ods.make RC_CFLAGS="$(RC_CFLAGS)"
	install -c -m 755 $(OBJROOT)/auth_ods.so $(DSTROOT)/usr/lib/samba/auth/opendirectory.so
	strip -x $(DSTROOT)/usr/lib/samba/auth/opendirectory.so
	make -C $(SRCROOT)/pdb_ods -f pdb_ods.make RC_CFLAGS="$(RC_CFLAGS) -DUSES_ODGROUPMAPPING -DUSE_ALGORITHMIC_RID -DUSES_KEYCHAIN"
	install -c -m 755 $(OBJROOT)/pdb_ods.so $(DSTROOT)/usr/lib/samba/pdb/opendirectorysam.so
	strip -x $(DSTROOT)/usr/lib/samba/pdb/opendirectorysam.so
	make -C $(SRCROOT)/darwin_vfs -f darwin_acls.make RC_CFLAGS="$(RC_CFLAGS)"
	install -c -m 755 $(OBJROOT)/darwin_acls.so $(DSTROOT)/usr/lib/samba/vfs/darwin_acls.so
	strip -x $(DSTROOT)/usr/lib/samba/vfs/darwin_acls.so
