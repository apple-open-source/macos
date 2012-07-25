noinst_PROGRAMS = sjeng
sjeng_SOURCES = attacks.c crazy.c epd.c learn.c partner.c seval.c \
                ttable.c book.c ecache.c eval.c moves.c search.c \
                sjeng.c utils.c newbook.c proof.c neval.c rcfile.c\
		leval.c draw.c see.c segtb.c\
		protos.h extvars.h sjeng.h squares.h
EXTRA_DIST = TODO NEWS ChangeLog COPYING BUGS THANKS blob2.c sjeng.rc 
SUBDIRS = . books tests

#
# Strip before installing
#
install-exec-local: 
	strip sjeng

#
# Ignore whatever other target ProjectBuilder throws at us
#
.DEFAULT:
	@echo "Ignoring request for unknown target" $@
