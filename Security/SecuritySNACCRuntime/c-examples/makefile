# c-examples/makefile
#
# $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-examples/makefile,v 1.1.1.1 2001/05/18 23:14:07 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:07  mb
# Move from private repository to open source repository
#
# Revision 1.2  2000/06/08 20:06:37  dmitch
# Mods for X port.
#
# Revision 1.1.1.1  1999/03/16 18:06:08  aram
# Originals from SMIME Free Library.
#
# Revision 1.2  1995/07/24 20:38:56  rj
# `cd && make' instead of `cd; make'.
#
# changed `_' to `-' in file names.
#

# Appple change: snmp test does not work.
#SUBDIRS		= test-lib simple any snmp
SUBDIRS		= test-lib simple any

#-------------------------------------------------------------------------------

.PHONY:	implicit_default
implicit_default::
		$(MAKE) subdirs

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

# dummy:
install::
