$!---------------make.com for xpmlib----------------------------------------
$! make xpmlib under VMS
$!
$ Make = ""
$!
$! Where are we?
$!
$ here = f$directory()
$ disk = f$trnlnm("SYS$DISK")
$ path = "''disk'"+ "''here'" 
$ xpath = "''path'" - "SXPM]" + "lib]"
$ if f$trnlnm("X11").eqs."" then define x11 decw$include,'xpath
$!
$! Check for MMK/MMS
$!
$ If F$Search ("Sys$System:MMS.EXE") .nes. "" Then Make = "MMS"
$ If F$Type (MMK) .eqs. "STRING" Then Make = "MMK"
$!
$! Look for the compiler used
$!
$ ccopt = "/define=(NEED_STRCASECMP,NEED_STRDUP,NO_ZPIPE)"
$ if f$getsyi("HW_MODEL").ge.1024
$ then
$  ccopt = "/prefix=all"+ccopt
$  comp  = "__decc__=1"
$  if f$trnlnm("SYS").eqs."" then define sys sys$library:
$ else
$  if f$search("SYS$SYSTEM:DECC$COMPILER.EXE").eqs.""
$   then
$    comp  = "__vaxc__=1"
$    if f$trnlnm("SYS").eqs."" then define sys sys$library:
$   else
$    if f$trnlnm("SYS").eqs."" then define sys decc$library_include:
$    ccopt = "/decc/prefix=all"+ccopt
$    comp  = "__decc__=1"
$  endif
$ endif
$!
$! Produce linker-options file according to X-Release and compiler used
$!
$ open/write optf sxpm.opt
$ write optf "libxpm.olb/lib"
$ write optf "sys$share:decw$xextlibshr.exe/share"
$ write optf "sys$share:decw$xlibshr.exe/share"
$ @sys$update:decw$get_image_version sys$share:decw$xlibshr.exe decw$version
$ if f$extract(4,3,decw$version).eqs."1.1"
$ then
$   write optf "sys$share:decw$xtshr.exe/share"
$ endif
$ if f$extract(4,3,decw$version).eqs."1.2"
$ then
$   write optf "sys$share:decw$xtlibshrr5.exe/share"
$ endif
$ close optf
$!
$! Build the thing plain or with 'Make'
$!
$ write sys$output "Compiling XPMlib sources ..."
$  if (Make .eqs. "")
$   then
$    'Make'/Macro = ('comp')
$  else
$   CALL MAKE CrBufFrI.OBJ "CC ''CCOPT' CrBufFrI" -
                CrBufFrI.c XpmI.h xpm.h
$   CALL MAKE CrBufFrP.OBJ "CC ''CCOPT' CrBufFrP" -
                CrBufFrP.c XpmI.h xpm.h
$   CALL MAKE CrDatFI.OBJ "CC ''CCOPT' CrDatFrI" -
                CrDatFrI.c XpmI.h xpm.h
$   CALL MAKE CrDatFP.OBJ "CC ''CCOPT' CrDatFrP" -
                CrDatFrP.c XpmI.h xpm.h
$   CALL MAKE CrIFrBuf.OBJ "CC ''CCOPT' CrIFrBuf" -
                CrIFrBuf.c XpmI.h xpm.h
$   CALL MAKE CrIFrDat.OBJ "CC ''CCOPT' CrIFrDat" -
                CrIFrDat.c XpmI.h xpm.h
$   CALL MAKE CrPFrBuf.OBJ "CC ''CCOPT' CrPFrBuf" -
                CrPFrBuf.c XpmI.h xpm.h
$   CALL MAKE CrPFrDat.OBJ "CC ''CCOPT' CrPFrDat" -
                CrPFrDat.c XpmI.h xpm.h
$   CALL MAKE RdFToDat.OBJ "CC ''CCOPT' RdFToDat" -
                RdFToDat.c XpmI.h xpm.h
$   CALL MAKE RdFToI.OBJ "CC ''CCOPT' RdFToI" -
                RdFToI.c XpmI.h xpm.h
$   CALL MAKE RdFToP.OBJ "CC ''CCOPT' RdFToP" -
                RdFToP.c XpmI.h xpm.h
$   CALL MAKE WrFFrDat.OBJ "CC ''CCOPT' WrFFrDat" -
                WrFFrDat.c XpmI.h xpm.h
$   CALL MAKE WrFFrI.OBJ "CC ''CCOPT' WrFFrI" -
                WrFFrI.c XpmI.h xpm.h
$   CALL MAKE WrFFrP.OBJ "CC ''CCOPT' WrFFrP" -
                WrFFrP.c XpmI.h xpm.h
$   CALL MAKE create.OBJ "CC ''CCOPT' create" -
                create.c XpmI.h xpm.h
$   CALL MAKE data.OBJ "CC ''CCOPT' data" -
                data.c XpmI.h xpm.h
$   CALL MAKE hashtab.OBJ "CC ''CCOPT' hashtab" -
                hashtab.c XpmI.h xpm.h
$   CALL MAKE misc.OBJ "CC ''CCOPT' misc" -
                misc.c XpmI.h xpm.h
$   CALL MAKE parse.OBJ "CC ''CCOPT' parse" -
                parse.c XpmI.h xpm.h
$   CALL MAKE rgb.OBJ "CC ''CCOPT' rgb" -
                rgb.c XpmI.h xpm.h
$   CALL MAKE scan.OBJ "CC ''CCOPT' scan" -
                scan.c XpmI.h xpm.h
$   CALL MAKE Attrib.OBJ "CC ''CCOPT' Attrib" -
                Attrib.c XpmI.h xpm.h
$   CALL MAKE CrIFrP.OBJ "CC ''CCOPT' CrIFrP" -
                CrIFrP.c XpmI.h xpm.h
$   CALL MAKE CrPFrI.OBJ "CC ''CCOPT' CrPFrI" -
                CrPFrI.c XpmI.h xpm.h
$   CALL MAKE Image.OBJ "CC ''CCOPT' Image" -
                Image.c XpmI.h xpm.h
$   CALL MAKE Info.OBJ "CC ''CCOPT' Info" -
                Info.c XpmI.h xpm.h
$   CALL MAKE RdFToBuf.OBJ "CC ''CCOPT' RdFToBuf" -
                RdFToBuf.c XpmI.h xpm.h
$   CALL MAKE WrFFrBuf.OBJ "CC ''CCOPT' WrFFrBuf" -
                WrFFrBuf.c XpmI.h xpm.h
$   write sys$output "Building XPMlib ..."
$   CALL MAKE LIBXPM.OLB "lib/crea libxpm.olb *.obj" *.OBJ
$   CALL MAKE SXPM.OBJ "CC  ''CCOPT' [-.sxpm]sxpm" -
                [-.sxpm]sxpm.c xpm.h
$   write sys$output "Linking SXPM ..."
$   CALL MAKE SXPM.EXE "LINK  sxpm,sxpm.opt/OPT" sxpm.OBJ
$  endif
$ write sys$output "XPMlib build completed"
$ sxpm :=="$''path'sxpm.exe"
$ exit
$!
$!
$MAKE: SUBROUTINE   !SUBROUTINE TO CHECK DEPENDENCIES
$ V = 'F$Verify(0)
$! P1 = What we are trying to make
$! P2 = Command to make it
$! P3 - P8  What it depends on
$
$ If F$Search(P1) .Eqs. "" Then Goto Makeit
$ Time = F$CvTime(F$File(P1,"RDT"))
$arg=3
$Loop:
$       Argument = P'arg
$       If Argument .Eqs. "" Then Goto Exit
$       El=0
$Loop2:
$       File = F$Element(El," ",Argument)
$       If File .Eqs. " " Then Goto Endl
$       AFile = ""
$Loop3:
$       OFile = AFile
$       AFile = F$Search(File)
$       If AFile .Eqs. "" .Or. AFile .Eqs. OFile Then Goto NextEl
$       If F$CvTime(F$File(AFile,"RDT")) .Ges. Time Then Goto Makeit
$       Goto Loop3
$NextEL:
$       El = El + 1
$       Goto Loop2
$EndL:
$ arg=arg+1
$ If arg .Le. 8 Then Goto Loop
$ Goto Exit
$
$Makeit:
$ VV=F$VERIFY(0)
$ write sys$output P2
$ 'P2
$ VV='F$Verify(VV)
$Exit:
$ If V Then Set Verify
$ENDSUBROUTINE
