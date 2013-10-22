DIRS = f2c libpng freetype

install:
	@set -x && \
	for i in $(DIRS); do \
	    make -C $$i install OSL='$(OSL)' OSV='$(OSV)' \
		TMPPREFIX='$(TMPPREFIX)' || exit 1; \
	done

afterinstallsrc:
	@set -x && \
	for i in $(DIRS); do \
	    make -C $$i afterinstallsrc || exit 1; \
	done
