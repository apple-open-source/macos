# Set these variables as needed, then include this file, then:
#

# Project info
Project         = samba/source
UserType        = Administration
ToolType        = Services

GnuNoChown      = YES
GnuAfterInstall = install-startup-xinetd install-config install-logdir  install-strip plugins

Extra_CC_Flags  = -no-cpp-precomp -I$(SRCROOT)/libopendirectorycommon -F/System/Library/PrivateFrameworks\
		-DWITH_OPENDIRECTORY -DUSES_RECVFROM -DDARWINOS=1

Extra_Configure_Flags = --with-swatdir="$(SHAREDIR)/swat"			\
			--with-sambabook="$(SHAREDIR)/swat/using_samba"		\
			--with-privatedir="$(VARDIR)/db/samba"			\
			--with-libdir="/etc"						\
			--with-lockdir="$(VARDIR)/samba"			\
			--with-logfilebase="$(LOGDIR)/samba"			\
			--with-piddir="$(RUNDIR)"				\
			--with-krb5						\
			--with-cups						\
			--with-ldap						\
			--with-spinlocks					\
			--with-libiconv						\
			--disable-shared					\
			--with-static-modules=vfs				\
			--without-libsmbclient					\
			--with-winbind

Extra_Install_Flags   = SWATDIR="$(DSTROOT)$(SHAREDIR)/swat"			\
			SAMBABOOK="$(DSTROOT)$(SHAREDIR)/swat/using_samba"	\
			PRIVATEDIR="$(DSTROOT)$(VARDIR)/db/samba"		\
			VARDIR="$(DSTROOT)$(VARDIR)"				\
			LIBDIR="$(DSTROOT)/private/etc"				\
			PIDDIR="$(DSTROOT)$(RUNDIR)"				\
			MANDIR="$(DSTROOT)/usr/share/man"			\
			LOCKDIR="$(DSTROOT)$(VARDIR)/samba"

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

LDFLAGS += -framework DirectoryService -L$(OBJROOT) -lopendirectorycommon

PATCHES = $(wildcard $(SRCROOT)/patches/*.diff)

Install_Target = install

lazy_install_source::
	gcc $(CFLAGS) -c $(SRCROOT)/libopendirectorycommon/libopendirectorycommon.c -o $(OBJROOT)/libopendirectorycommon.o
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

install-startup-xinetd:
	$(INSTALL) -d -m 755 $(DSTROOT)/private/etc/xinetd.d
	$(INSTALL) -c -m 444 $(SRCROOT)/smbd.xinetd $(DSTROOT)/private/etc/xinetd.d/smbd
	$(INSTALL) -c -m 444 $(SRCROOT)/nmbd.xinetd $(DSTROOT)/private/etc/xinetd.d/nmbd
	$(INSTALL) -c -m 444 $(SRCROOT)/smb-direct.xinetd $(DSTROOT)/private/etc/xinetd.d/smb-direct
	$(INSTALL) -c -m 444 $(SRCROOT)/swat.xinetd $(DSTROOT)/private/etc/xinetd.d/swat

install-config:
	$(INSTALL) -c -m 444 $(SRCROOT)/smb.conf.template $(DSTROOT)/private/etc

install-logdir:
	$(INSTALL) -d -m 755 $(DSTROOT)/private/var/log/samba
	$(INSTALL) -d -m 777 $(DSTROOT)/private/var/spool/samba
	$(INSTALL) -d -m 755 $(DSTROOT)/private/var/spool/lock

install-strip:
	for F in $(DSTROOT)/usr/{s,}bin/*; do	\
		cp "$$F" $(SYMROOT); \
		[ -f "$$F" -a -x "$$F" ] && strip -x "$$F";	\
	done
	rmdir $(DSTROOT)/$(RUNDIR)
	rmdir $(DSTROOT)/private/etc/rpc
	rm -f $(DSTROOT)/usr/share/man/man8/smbmnt.8
	rm -f $(DSTROOT)/usr/share/man/man8/smbmount.8
	rm -f $(DSTROOT)/usr/share/man/man8/smbumount.8
	rm -f $(DSTROOT)/usr/share/swat/help/smbmnt.8.html
	rm -f $(DSTROOT)/usr/share/swat/help/smbmount.8.html
	rm -f $(DSTROOT)/usr/share/swat/help/smbumount.8.html

plugins:
	echo "building $@";
	make -C $(SRCROOT)/auth_ods -f auth_ods.make RC_CFLAGS="$(RC_CFLAGS)"
	install -c -m 755 $(OBJROOT)/auth_ods.so $(DSTROOT)/private/etc/auth/opendirectory.so
	strip -x $(DSTROOT)/private/etc/auth/opendirectory.so
	make -C $(SRCROOT)/pdb_ods -f pdb_ods.make RC_CFLAGS="$(RC_CFLAGS)"
	install -c -m 755 $(OBJROOT)/pdb_ods.so $(DSTROOT)/private/etc/pdb/opendirectorysam.so
	strip -x $(DSTROOT)/private/etc/pdb/opendirectorysam.so
