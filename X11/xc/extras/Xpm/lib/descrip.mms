cc_defs = /define=(NEED_STRCASECMP,NEED_STRDUP,NO_ZPIPE)
c_deb = 

.ifdef __DECC__
pref = /prefix=all
.endif

OBJS=\
data.obj,\
create.obj,\
misc.obj,\
rgb.obj,\
scan.obj,\
parse.obj,\
hashtab.obj,\
WrFFrP.obj,\
RdFToP.obj,\
CrPFrDat.obj,\
CrDatFrP.obj,\
WrFFrI.obj,\
RdFToI,\
CrIFrDat.obj,\
CrDatFrI.obj,\
CrIFrBuf.obj,\
CrPFrBuf.obj,\
CrBufFrI.obj,\
CrBufFrP.obj,\
RdFToDat.obj,\
WrFFrDat.obj,\
Attrib.obj,\
CrIFrP.obj,\
CrPFrI.obj,\
Image.obj,\
Info.obj,\
RdFToBuf.obj,\
WrFFrBuf.obj

CFLAGS= $(C_DEB) $(CC_DEFS) $(PREF)

all : libXpm.olb sxpm.exe
      @ library/compress libxpm.olb 

libXpm.olb : libXpm.olb($(OBJS))
	@ write sys$output " Libxpm available"

sxpm.exe : sxpm.obj libXpm.olb
                   @ link sxpm,sxpm/opt
clean : 
	delete *.obj;*,libXpm.olb;*


# Other dependencies.
CrBufFrI.obj :  XpmI.h xpm.h
CrBufFrP.obj :  XpmI.h xpm.h
CrDatFI.obj :  XpmI.h xpm.h
CrDatFP.obj :  XpmI.h xpm.h
CrIFrBuf.obj :  XpmI.h xpm.h
CrIFrDat.obj :  XpmI.h xpm.h
CrPFrBuf.obj :  XpmI.h xpm.h
CrPFrDat.obj :  XpmI.h xpm.h
RdFToDat.obj :  XpmI.h xpm.h
RdFToI.obj :  XpmI.h xpm.h
RdFToP.obj :  XpmI.h xpm.h
WrFFrDat.obj :  XpmI.h xpm.h
WrFFrI.obj :  XpmI.h xpm.h
WrFFrP.obj :  XpmI.h xpm.h
create.obj :  XpmI.h xpm.h
data.obj :  XpmI.h xpm.h
hashtab.obj :  XpmI.h xpm.h
misc.obj :  XpmI.h xpm.h
parse.obj :  XpmI.h xpm.h
rgb.obj :  XpmI.h xpm.h
scan.obj :  XpmI.h xpm.h
Attrib.obj :  XpmI.h xpm.h
CrIFrP.obj :  XpmI.h xpm.h
CrPFrI.obj :  XpmI.h xpm.h
Image.obj :  XpmI.h xpm.h
Info.obj :  XpmI.h xpm.h
RdFToBuf.obj :  XpmI.h xpm.h
WrFFrBuf.obj :  XpmI.h xpm.h

sxpm.obj : [-.sxpm]sxpm.c xpm.h
