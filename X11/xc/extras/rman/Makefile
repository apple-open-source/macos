#
# Makefile for PolyglotMan
#   It's helpful to read the README-rman file first.
#   You should read over all parts of this file,
#      down to the "you shouldn't modify" line
#
# Tom Phelps (phelps@ACM.org)
#


### you need to localize the paths on these lines

# The executable `rman' is placed in BINDIR.
# If you're also installing TkMan (available separately--see README-rman),
#    this must be a directory that's in your bin PATH.
# MANDIR holds the man page.

BINDIR = /private/share/bin
#BINDIR = /usr/local/bin
MANDIR = /home/auspex/h/bair/phelps/man/man1
#MANDIR = /usr/local/man/man1


### if you have GNU gcc v2.x, use these definitions

CC = gcc
# if you don't have gcc v2.x, try -O for CFLAGS
CFLAGS = -O2 -finline-functions

### if you just have a standard UNIX, use these instead of GNU.
### CC must be an ANSI C compiler

#CC = cc
#CFLAGS = -O

# Solaris and SysV people may need this
#CFLAGS = -O2 -finline-functions

# For HP-UX
#CC = cc
#CFLAGS = -Aa -O
# HP-UX 10.20
#CFLAGS = -Ae -O

# DEC Alpha and Ultrix, -std1 needed to conform to ANSI C
#CC = cc
#CFLAGS = -std1 -O3 -Olimit 1000


# list of valid volume numbers and letters
# you can also set these at runtime with -V
VOLLIST = "1:2:3:4:5:6:7:8:9:o:l:n:p"
# SCO Unix has expanded set of volume letters
#VOLLIST = "1:2:3:4:5:6:7:8:9:o:l:n:p:C:X:S:L:M:F:G:A:H"
# SGI and UnixWare 2.0
#VOLLIST = "1:2:3:4:5:6:7:8:9:o:l:n:p:D"


# the printf strings used to set the <TITLE> and
# to set URL hyperlinks to referenced manual pages
# can be defined at runtime.  The defaults are defined below.
# The first %s parameter is the manual page name,
# the second the volume/section number.
# you can set these at runtime with -l and -r, respectively

MANTITLEPRINTF = "%s(%s) manual page"
# relative link to pregenerated file in same directory
MANREFPRINTF = "%s.%s"
# on-the-fly through a cgi-bin script
#MANREFPRINTF = "/cgi-bin/man2html?%s&%s"


# # # these lines are probably fine

CP = cp
# or you can use GNU's cp and backup files that are about to be overwritten
#CP = cp -b
RM = rm


#--------------------------------------------------
#
# you shouldn't modify anything below here
#
#--------------------------------------------------

version = 3.0.8
rman = rman-$(version)
srcs = rman.c getopt.c macish.c
objs = rman
defs = -DVOLLIST='$(VOLLIST)' -DMANTITLEPRINTF='$(MANTITLEPRINTF)' -DMANREFPRINTF='$(MANREFPRINTF)'
libs = 
aux = ANNOUNCE-rman README-rman Makefile Makefile.mac rman.1 rman.html CHANGES
distrib = $(srcs) $(libs) $(aux) contrib


all:  rman
	@echo 'Files made in current directory.'
	@echo 'You should "make install".'

# everyone but me zaps assertions with the -DNDEBUG flag
rman: rman.c Makefile
	$(CC) -DNDEBUG $(defs) -DPOLYGLOTMANVERSION=\"$(version)\" $(CFLAGS) -o rman rman.c

dox: rman.1 ANNOUNCE-rman

ANNOUNCE-rman: Announce-rman
	sed -e /=========/,/bogusline/p -e d Announce-rman > ANNOUNCE-rman

rman.1: rman.html rman
	echo '.TH PolyglotMan 1' > rman.1
	cat ../html/html.dtd rman.html | sgmls | ../html/html2man.pl >> rman.1

debug:
	$(CC) $(defs) -DDEBUG -DPOLYGLOTMANVERSION=\"debug\" -g -Wall -o rman rman.c

prof:
	quantify -cache-dir=/home/orodruin/h/bair/phelps/spine/rman/cache $(CC) -DNDEBUG $(defs) -DPOLYGLOTMANVERSION=\"QUANTIFY\" -g -o rman rman.c

install: rman
#	$(INSTALL) -s rman $(BINDIR)
	$(RM) -f $(BINDIR)/rman
	$(CP) rman $(BINDIR)
	$(RM) -f $(MANDIR)/rman.1
	$(CP) rman.1 $(MANDIR)

# test version includes assertions
#	ginstall rman $(BINDIR)/`arch`
test: rman.c Makefile
	$(CC) $(defs) -DPOLYGLOTMANVERSION=\"$(version)\" $(CFLAGS) -Wall -ansi -pedantic -o rman rman.c
	ls -l rman
	ginstall rman $(BINDIR)
	rman -v
	rman --help
	@echo 'Assertion checks:'
	rman -f html weirdman/hp-tbl.1 > /dev/null
	rman -f html weirdman/Pnews.1 > /dev/null
	nroff -man rman.1 | rman -f html > /dev/null

sww:
	rm -f rman $(wildcard ~/bin/{sun4,snake,alpha}/rman)
	rman

clean:
	rm -f $(objs)

dist: dox
	rm -rf $(rman)*
	mkdir $(rman)
	$(CP) -R $(distrib) $(rman)
#	expand -4 rman.c > $(rman)/rman.c
	rm -f $(rman)/contrib/*~
	@echo 'gcksum crc   length name' > MANIFEST
	@echo '----------   ------ ----' >> MANIFEST
	@gcksum $(filter-out contrib, $(filter-out %~, $(distrib) $(wildcard contrib/*))) | gtee -a MANIFEST
	mv MANIFEST $(rman)
	gtar chvf $(rman).tar $(rman)
	compress $(rman).tar
#	gzip -9v $(rman).tar
	rm -rf $(rman) ANNOUNCE-rman rman.1
	@echo "*** Did you remember to ci -l first?"

uu: tar
	gznew $(rman).tar.Z
	echo 'uudecode, gunzip (from GNU), untar' > $(rman).tar.gz.uu
	uuencode $(rman).tar.gz $(rman).tar.gz >> $(rman).tar.gz.uu
