test.exe:       $*.obj
   wlink file $*.obj lib freetype.lib import _fdhdr freetype.FONT_DRIVER_DISPATCH_TABLE
test.obj:       $*.c
   wcc386 -d2 -zp1 $*.c
