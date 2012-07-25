PROJECT=$(shell basename `pwd -P` | sed -e 's/\(.*\)-[0-9][0-9.]*$$/\1/')
VERSION=$(shell basename `pwd -P` | sed -e 's/.*-\([0-9][0-9.]*$$\)/\1/')

#----------------------------------------------------------------------
#
# Build for [current] release
#
#----------------------------------------------------------------------

all :
	/usr/local/bin/buildit .				\
	  -noinstallsrc -noinstallhdrs -noverify -nosum		\
	  -arch i386 -arch x86_64 -arch ppc 			\
	  -target All						\
	  -project ${PROJECT}-${VERSION}			\
	  -configuration Debug					\
	  -release $(shell cat /usr/share/buildit/.releaseName)	\

#----------------------------------------------------------------------
#
# Darwin build
#
#----------------------------------------------------------------------

darwin :
	/usr/local/bin/buildit .				\
	  -noinstallsrc -noinstallhdrs -noverify -nosum		\
	  -arch i386 -arch x86_64 -arch ppc 			\
	  -target All						\
	  -project ${PROJECT}_darwin-${VERSION}			\
	  -configuration Debug					\
	  -release $(shell cat /usr/share/buildit/.releaseName)	\
	  -othercflags "\"-D_OPEN_SOURCE_\""			\

#----------------------------------------------------------------------
#
# Build for Lion, SULionXXX, ...
#
#----------------------------------------------------------------------

LION_CFLAGS=
LION_CFLAGS+=-D__MAC_10_8=1070
LION_CFLAGS+=-D__AVAILABILITY_INTERNAL__MAC_10_8=__attribute__((visibility(\\\"default\\\")))
LION_CFLAGS+=-DHAVE_REACHABILITY_SERVER=YES

lion :
	/usr/local/bin/buildit .				\
	  -noinstallsrc -noinstallhdrs -noverify -nosum		\
	  -arch i386 -arch x86_64				\
	  -target All						\
	  -project ${PROJECT}-${VERSION}			\
	  -configuration Debug					\
	  -release $(shell cat /usr/share/buildit/.releaseName)	\
	  -othercflags "$(LION_CFLAGS)"				\

