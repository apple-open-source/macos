# f2c
#
# Build f2c into OBJROOT and let scipy use it to build.  A copy of numpy
# is needed, modified to support f2c as one of its known fortran compilers.
# Installation directory is given by the variable TMPPREFIX.
# Set RC_CFLAGS to build multiple architectures.

F2CNAME = f2c
F2CVERS = 20100827
F2CNAMEVERS = $(F2CNAME)-$(F2CVERS)
LIBF2CNAME = libf2c

F2CSRC = $(SRCROOT)/tmpprefix/$(F2CNAME)
F2CTOP = $(OBJROOT)/$(F2CNAME)
F2CDIR = $(F2CTOP)/$(F2CNAME)
TARSUFFIX = .tar.gz

install: $(F2CDIR) $(F2CDIR)/$(LIBF2CNAME) buildsrc
	@set -x && \
	cd $(F2CDIR) && \
	sed 's,XXXXX,$(TMPPREFIX),' $(F2CSRC)/fc.ed | ed - fc && \
	install -d $(TMPPREFIX)/bin $(TMPPREFIX)/include $(TMPPREFIX)/lib && \
	cp -p $(LIBF2CNAME)/$(LIBF2CNAME).a $(TMPPREFIX)/lib && \
	cp -p $(LIBF2CNAME)/f2c.h $(TMPPREFIX)/include && \
	ranlib $(TMPPREFIX)/lib/$(LIBF2CNAME).a && \
	install src/f2c $(TMPPREFIX)/bin && \
	install fc $(TMPPREFIX)/bin && \
	ln -s fc $(TMPPREFIX)/bin/f77
	cp -f $(F2CDIR)/permission $(OSL)/$(F2CNAMEVERS).txt
	cp -f $(F2CSRC)/$(F2CNAME).partial $(OSV)/$(F2CNAMEVERS).partial

$(F2CDIR):
	mkdir -p $(F2CTOP)
	tar xzof $(F2CNAME)-mod$(TARSUFFIX) -C $(F2CTOP)

$(F2CDIR)/$(LIBF2CNAME):
	@set -x && \
	cd $(F2CDIR) && \
	unzip $(LIBF2CNAME).zip -d $(LIBF2CNAME) && \
	cd $(LIBF2CNAME) && \
	cp makefile.u Makefile && \
	ed - f2c.h0 < $(F2CSRC)/f2c.h0.ed && \
	ed - sysdep1.h0 < $(F2CSRC)/sysdep1.h0.ed && \
	ed - Makefile < $(F2CSRC)/libf2c_Makefile.ed && \
	ed - fio.h < $(F2CSRC)/fio.h.ed && \
	cp $(F2CSRC)/arith.h $(F2CSRC)/cpu_time__.c . && \
	make f2c.h && \
	make

buildsrc:
	@set -x && \
	cd $(F2CDIR)/src && \
	cp makefile.u Makefile && \
	ed - Makefile < $(F2CSRC)/src_Makefile.ed && \
	ed - format.c < $(F2CSRC)/src_format.c.ed && \
	ed - defs.h < $(F2CSRC)/src_defs.h.ed && \
	ed - main.c < $(F2CSRC)/src_main.c.ed && \
	rm -f f2c.h && \
	ln -s ../$(LIBF2CNAME)/f2c.h && \
	make

afterinstallsrc:
	perl $(TOPSRCROOT)/tar-remove.pl '^f2c/(?:msdos|mswin)/' $(F2CSRC)/$(F2CNAME)$(TARSUFFIX) $(F2CNAME)-mod$(TARSUFFIX)
	rm -f $(F2CSRC)/$(F2CNAME)$(TARSUFFIX)
