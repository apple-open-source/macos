
DIR = strings
include ../MakePaths.dir

INSTALLDIR = $(DSTROOT)/usr/standalone/i386
VPATH = $(OBJROOT)

CHOWN = chown

FILES = Language.table \
	BootHelp.txt \
	English.lproj/Localizable.strings \
	English.lproj/NetInstall.strings \
	French.lproj/Localizable.strings \
	French.lproj/NetInstall.strings \
	German.lproj/Localizable.strings \
	German.lproj/NetInstall.strings

# Remove Swedish, Italian and Spanish for Rhap 1.0
#	Italian.lproj/Localizable.strings \
#	Italian.lproj/NetInstall.strings \
#	Spanish.lproj/Localizable.strings \
#	Spanish.lproj/NetInstall.strings \
#	Swedish.lproj/Localizable.strings \
#	Swedish.lproj/NetInstall.strings

DIRS =  ${INSTALLDIR}/English.lproj \
	${INSTALLDIR}/French.lproj \
	${INSTALLDIR}/German.lproj

# Remove Swedish, Italian and Spanish for Rhap 1.0
#	${INSTALLDIR}/Italian.lproj
#	${INSTALLDIR}/Spanish.lproj \
#	${INSTALLDIR}/Swedish.lproj

DIRS_NEEDED = ${DIRS}

all:

install_i386:: $(INSTALLDIR) $(DIRS_NEEDED)
	tar cf - ${FILES} | (cd ${INSTALLDIR}; tar xfBp - )
	chown -fR root.wheel $(INSTALLDIR)
	chmod -R ugo-w  $(INSTALLDIR)

include ../MakeInc.dir
