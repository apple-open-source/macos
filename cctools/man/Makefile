ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif

COMMON_MAN1 = as.1 gprof.1 dyld.1 ld.1 nm.1 otool.1 ranlib.1 segedit.1 size.1 \
	      strings.1 strip.1 lipo.1 libtool.1 cmpdylib.1 pagestuff.1 \
	      dylibprof.1 redo_prebinding.1 nmedit.1 install_name_tool.1
OTHER_MAN1 = ar.1 file.1 atom.1

MAN3 = kld.3 dyld.3 dyld_debug.3 arch.3 get_end.3 end.3 getsectbyname.3 \
       getsectbynamefromheader.3 getsectdata.3 getsectdatafromheader.3 \
       getsegbyname.3 NSModule.3 NSObjectFileImage.3 redo_prebinding.3
MAN5 = Mach-O.5 a.out.5 ranlib.5 stab.5
MANL = cmpshlib.1 indr.1 mkshlib.1 checksyms.1 seg_addr_table.1 \
       check_dylib.1
MANL3 = libsyminfo.3

INSTALL_FILES = Makefile $(COMMON_MAN1) $(OTHER_MAN1) $(MAN3) $(MAN5) $(MANL) $(MANL3)

MANDIR = /usr/share/man
LOCMANDIR = /usr/local/share/man
DSTDIRS = $(DSTROOT)$(MANDIR)/man1 $(DSTROOT)$(MANDIR)/man3 \
	  $(DSTROOT)$(MANDIR)/man5 $(DSTROOT)$(LOCMANDIR)/man1 \
	  $(DSTROOT)$(LOCMANDIR)/man3

install: $(DSTDIRS) $(COMMON_MAN1) $(OTHER_MAN1) $(MAN3) $(MAN5) $(MANL) $(RC_OS) $(MANL3)
	install -c -m 444 $(COMMON_MAN1) $(DSTROOT)$(MANDIR)/man1
	install -c -m 444 $(MAN3) $(DSTROOT)$(MANDIR)/man3
	install -c -m 444 $(MAN5) $(DSTROOT)$(MANDIR)/man5
	install -c -m 444 $(MANL) $(DSTROOT)$(LOCMANDIR)/man1
	pwd
	ls -altr
	install -c -m 444 $(MANL3) $(DSTROOT)$(LOCMANDIR)/man3

teflon macos:

nextstep:
	install -c -m 444 $(OTHER_MAN1) $(DSTROOT)$(MANDIR)/man1

installsrc:
	$(MKDIRS) $(SRCROOT)
	chmod 755 $(SRCROOT)
	gnutar cf - $(INSTALL_FILES) | (cd $(SRCROOT); gnutar xf -)
	chmod 444 $(SRCROOT)/*

$(DSTDIRS):
	$(MKDIRS) $@

clean shlib_clean:

all:
