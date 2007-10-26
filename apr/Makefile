.PHONY: installsrc clean installhdrs install

SUBPROJECTS = apr apr-util

installsrc::
	@cp Makefile $(SRCROOT)

install::
	@for proj in $(SUBPROJECTS); do \
		mkdir -p $(SYMROOT)/$${proj}; \
	done

installsrc clean installhdrs install::
	@for proj in $(SUBPROJECTS); do \
		(cd $${proj} && make $@ \
			SRCROOT=$(SRCROOT)/$${proj} \
			OBJROOT=$(OBJROOT)/$${proj} \
			SYMROOT=$(SYMROOT)/$${proj} \
			DSTROOT=$(DSTROOT) \
		) || exit 1; \
	done
