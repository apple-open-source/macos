#       $Id: Makefile,v 1.5 2000/07/25 02:25:38 wsanchez Exp $

SHELL= /bin/sh
SUBDIRS= libsmutil libsmdb sendmail mail.local mailstats makemap \
	 praliases rmail smrsh vacation
BUILD=   ./Build
OPTIONS= $(CONFIG) $(FLAGS)

all: FRC
	@set -e && for x in $(SUBDIRS); \
	do \
		(cd $$x; echo Making $@ in:; pwd; \
		$(SHELL) $(BUILD) $(OPTIONS)); \
	done

clean: FRC
	@set -e && for x in $(SUBDIRS); \
	do \
		(cd $$x; echo Making $@ in:; pwd; \
		$(SHELL) $(BUILD) $(OPTIONS) $@); \
	done

install install-strip: FRC
	@set -e && for x in $(SUBDIRS); \
	do \
		(cd $$x; echo Making $@ in:; pwd; \
		$(SHELL) $(BUILD) $(OPTIONS) $@); \
	done

fresh: FRC
	@set -e && for x in $(SUBDIRS); \
	do \
		(cd $$x; echo Making $@ in:; pwd; \
		$(SHELL) $(BUILD) $(OPTIONS) -c); \
	done

$(SUBDIRS): FRC
	@cd $@; pwd; \
	$(SHELL) $(BUILD) $(OPTIONS)

FRC:
