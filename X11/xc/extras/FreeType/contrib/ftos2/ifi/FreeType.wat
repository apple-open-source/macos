# WARNING!! this doesn't quite work yet!!!!
#
#       Makefile for FTIFI using Watcom C/C++
#
# Explanation of compiler switches used:
#  -4r      register calling convention, generate 486 code. Better than
#           Pentium opt. for Cyrix/IBM and AMD
#  -otexan  maximum optimization for speed
#  -zp1     pack structures on byte boundaries (quite important!)
#  -bd      build a DLL
#  -zc
#  -d2      include debug info
#
# Important linker options used:
#  initglobal   call DLL initialization function only once (not for each
#               process)
#  termglobal   call DLL termination function only once
#  oneautodata  use only one shared data segment

WCCR=-4r -otexan
#WCCD=-d2 -DDEBUG
#LNKD=debug all


FreeType.dll:       $*.obj $*.def
   wlink system os2v2 dll initglobal termglobal op oneautodata export GetOutline_ export FONT_DRIVER_DISPATCH_TABLE=_fdhdr file $* lib ..\lib\libttf.lib $(LNKD)

FreeType.obj:       ftifi.c ftifi.h
   wcc386 $(WCCD) $(WCCR) -zp1 -bd -zc -I..\lib -I..\lib\extend ftifi.c /Fo=freetype.obj
