DSTROOT="You must set DSTROOT on the command line to a directory that you do not care about"
OBJROOT="You must set OBJROOT on the command line to a directory that you do not care about"
SYMROOT="You must set SYMROOT on the command line to a directory that you do not care about"

.DEFAULT:
	@echo Making $@ target.
	if [ "x$(RC_OS)" = xsolaris -o "x$(RC_OS)" = xhpux ]; then \
                $(MAKE) -f Makefile.pdo $@; \
        else \
                $(MAKE) -f Makefile $@; \
        fi

# Export all the variables to the sub make.

.EXPORT_ALL_VARIABLES:
