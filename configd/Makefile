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
	  -project configd					\
	  -configuration Debug					\
	  -release $(shell cat /usr/share/buildit/.releaseName)	\

#----------------------------------------------------------------------
#
# Build for SnowLeopard, SUSnowXXX, ...
#
# Note: assumes that the "pppcontroller_sendmsg" routine has been defined
#       in pppcontroller.defs.
#
#----------------------------------------------------------------------

PROJECT=$(shell basename `pwd -P`)

SNOW_CFLAGS += -D__MAC_10_7=1060
SNOW_CFLAGS += -D__AVAILABILITY_INTERNAL__MAC_10_7=__AVAILABILITY_INTERNAL__MAC_10_6
SNOW_CFLAGS += -D__AVAILABILITY_INTERNAL__MAC_10_5_DEP__MAC_10_7=__AVAILABILITY_INTERNAL__MAC_10_5
#SNOW_CFLAGS += -DHAVE_PPPCONTROLLER_SENDMSG=YES

snow :
	/usr/local/bin/buildit .				\
	  -noinstallsrc -noinstallhdrs -noverify -nosum		\
	  -arch i386 -arch x86_64 -arch ppc			\
	  -target All						\
	  -project ${PROJECT}					\
	  -configuration Debug					\
	  -release $(shell cat /usr/share/buildit/.releaseName)	\
	  -othercflags "\"$(SNOW_CFLAGS)\""			\

#----------------------------------------------------------------------
#
# Build for Lion, SULionXXX, ...
#
#----------------------------------------------------------------------

LION_CFLAGS=

lion :
	/usr/local/bin/buildit .				\
	  -noinstallsrc -noinstallhdrs -noverify -nosum		\
	  -arch i386 -arch x86_64				\
	  -target All						\
	  -project ${PROJECT}-${VERSION}			\
	  -configuration Debug					\
	  -release $(shell cat /usr/share/buildit/.releaseName)	\
	  -othercflags "$(LION_CFLAGS)"				\

