DSTROOT="You must set DSTROOT on the command line to a directory that you do not care about"
OBJROOT="You must set OBJROOT on the command line to a directory that you do not care about"
SYMROOT="You must set SYMROOT on the command line to a directory that you do not care about"

.DEFAULT build install: force
	@echo Making $@ target.
	if [ "$(RC_OS)" = solaris -o "$(RC_OS)" = hpux ]; then \
                $(MAKE) -f Makefile.pdo $@; \
        else \
                $(MAKE) -f Makefile $@; \
        fi

force:

# Export all the variables to the sub make.

.EXPORT_ALL_VARIABLES:
