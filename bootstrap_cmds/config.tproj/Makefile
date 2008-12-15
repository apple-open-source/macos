Project = config
ProductType = tool
Install_Dir = /usr/bin

HFILES = config.h

LFILES = lexer.l
YFILES = parser.y
CFILES = externs.c main.c mkglue.c mkheaders.c mkioconf.c\
         mkmakefile.c mkswapconf.c openp.c searchp.c


# turn on all the warnings...
Extra_CC_Flags = -Wall -mdynamic-no-pic \
        -Wno-format-y2k -W -Wstrict-prototypes -Wmissing-prototypes \
        -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch \
        -Wshadow -Wcast-align -Wbad-function-cast -Wchar-subscripts -Winline \
        -Wnested-externs -Wredundant-decls
Extra_CC_Flags += -fwritable-strings
Extra_CC_Flags += -I. -DCMU -DLOCALARCHITECTURE
Extra_LD_Flags = -dead_strip
Extra_Y_Flags = -d

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
