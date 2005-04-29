#
# "$Id: Makefile,v 1.11 2005/01/06 19:39:07 jlovell Exp $"
#
#   Man page makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2005 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Easy Software Products and are protected by Federal
#   copyright law.  Distribution and use rights are outlined in the file
#   "LICENSE.txt" which should have been included with this file.  If this
#   file is missing or damaged please contact Easy Software Products
#   at:
#
#       Attn: CUPS Licensing Information
#       Easy Software Products
#       44141 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636 USA
#
#       Voice: (301) 373-9600
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs
include Makefile.common


#
# Other languages...
#

LANGDIRS =	


#
# Make everything...
#

all:	$(CAT1) $(CAT5) $(CAT8)
	for dir in $(LANGDIRS); do \
		echo "Making all in man/$$dir..."; \
		(cd $$dir; $(MAKE) $(MFLAGS) all); \
	done


#
# Clean all config and object files...
#

clean:
	$(RM) $(CAT1) $(CAT5) $(CAT8)
	for dir in $(LANGDIRS); do \
		echo "Cleaning all in man/$$dir..."; \
		(cd $$dir; $(MAKE) $(MFLAGS) clean); \
	done


#
# Dummy depend target...
#

depend:


#
# Install files...
#

install:	all
	$(INSTALL_DIR) $(MANDIR)/man1
	for file in $(MAN1); do \
		$(INSTALL_MAN) $$file $(MANDIR)/man1/`basename $$file man`1; \
	done
	$(RM) $(MANDIR)/man1/cancel.1
	$(LN) lp.1 $(MANDIR)/man1/cancel.1
	$(INSTALL_DIR) $(MANDIR)/man5
	for file in $(MAN5); do \
		$(INSTALL_MAN) $$file $(MANDIR)/man5/`basename $$file man`5; \
	done
	$(INSTALL_DIR) $(AMANDIR)/man$(MAN8EXT)
	for file in $(MAN8); do \
		$(INSTALL_MAN) $$file $(AMANDIR)/man$(MAN8EXT)/`basename $$file man`$(MAN8EXT); \
	done
	$(RM) $(AMANDIR)/man$(MAN8EXT)/reject.$(MAN8EXT)
	$(LN) accept.$(MAN8EXT) $(AMANDIR)/man$(MAN8EXT)/reject.$(MAN8EXT)
	$(RM) $(AMANDIR)/man$(MAN8EXT)/disable.$(MAN8EXT)
	$(LN) enable.$(MAN8EXT) $(AMANDIR)/man$(MAN8EXT)/disable.$(MAN8EXT)
	for dir in $(LANGDIRS); do \
		echo "Installing all in man/$$dir..."; \
		(cd $$dir; $(MAKE) $(MFLAGS) install); \
	done


#
# End of "$Id: Makefile,v 1.11 2005/01/06 19:39:07 jlovell Exp $".
#
