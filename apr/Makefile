.PHONY: installsrc clean installhdrs install

SUBPROJECTS = apr apr-util

installsrc::
	@cp Makefile apr.plist $(SRCROOT)

OSV = $(DSTROOT)/usr/local/OpenSourceVersions

install::
	@for proj in $(SUBPROJECTS); do \
		mkdir -p $(SYMROOT)/$${proj}; \
	done
	/bin/mkdir -p -m 0755 $(OSV)
	/usr/bin/install -m 0444 $(SRCROOT)/apr.plist $(OSV)

installsrc clean installhdrs install::
	@for proj in $(SUBPROJECTS); do \
		(cd $${proj} && make $@ \
			SRCROOT=$(SRCROOT)/$${proj} \
			OBJROOT=$(OBJROOT)/$${proj} \
			SYMROOT=$(SYMROOT)/$${proj} \
			DSTROOT=$(DSTROOT) \
		) || exit 1; \
	done
