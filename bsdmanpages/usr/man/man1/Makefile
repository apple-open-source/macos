SECTION=1
SECDIR=${DSTROOT}/usr/share/man/man${SECTION}

# INSTALLFLAGS=-d
INSTALLFLAGS=-c

install:	ckdstroot ${SECDIR}
		install ${INSTALLFLAGS} *.${SECTION}* ${SECDIR};

${SECDIR}:
	mkdir -p -m 755 $@

ckdstroot:
	@if [ -n "${DSTROOT}" ]; \
	then \
		exit 0; \
	else \
		echo Must define DSTROOT; \
		exit 1; \
	fi
