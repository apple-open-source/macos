#
# Makes all of the type table tools
# (currently mkchdr, pval and ptbl)
#
# The library in snacc/tbl-lib must be made prior to making these tools
#
# MS 93
#
# $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/tbl-tools/makefile,v 1.1.1.1 2001/05/18 23:14:10 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:10  mb
# Move from private repository to open source repository
#
# Revision 1.2  2000/05/10 21:40:50  rmurphy
# removing makefile ref to unavailable subdirectories
#
# Revision 1.1.1.1  1999/03/16 18:06:54  aram
# Originals from SMIME Free Library.
#
# Revision 1.2  1997/08/27 15:55:44  wan
# Added generic table decoding, debug routines, berdecode, and asnwish.
#
# Revision 1.1  1997/01/02 09:25:24  rj
# first check-in
#

SUBDIRS		= # mkchdr pval ptbl berdecode

#-------------------------------------------------------------------------------

.PHONY:	implicit_default subdirs

implicit_default::	subdirs

subdirs::	$(SUBDIRS)
$(SUBDIRS)::
		cd $@ && $(MAKE) $(subtarget)

# the following hack is needed for older make versions (gmake doesn't need it):
init-depend::
		@for dir in $(SUBDIRS); do\
		  test -f $$dir/dependencies || touch $$dir/dependencies;\
		done

.DEFAULT::
		$(MAKE) subdirs subtarget=$@

distfiles::
		echo makefile
		for dir in $(SUBDIRS); do\
		  subfiles=`cd $$dir && $(MAKE) -s $@`;\
		  for file in $$subfiles; do\
		    echo "$$dir/$$file";\
		  done;\
		done
