NAME = Mac-Carbon
VERSION = 0.82

NO64 = YES

include ../Makefile.inc

custompatching:
	ed - $(DIR)/common.pl < $(MYSRC)/common.pl.ed
	set -x && \
	for i in `find $(DIR) -name '*.[ch]' -o -name '*.xs'`; do \
	    $(MYSRC)/unFlatCarbon $$i > $$i.new && \
	    mv -f $$i.new $$i || exit 1; \
	done
	LANG=en_US.ISO8859-1 ed - $(DIR)/AppleEvents/PerlAEUtils.cp < $(MYSRC)/PerlAEUtils.cp.ed
