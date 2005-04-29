#   File:       snack.make
#   Target:     snack
#   Created:    Tuesday, March 21, 2000 01:27:20 PM


MAKEFILE        = snack.make
ÄMondoBuildÄ    = {MAKEFILE}  # Make blank to avoid rebuilds when makefile is modified

TclLibDir    = Macintosh HD:Systemmapp:Till‰gg:Tool Command Language

SrcDir          = ::generic:
ObjDir          = :
Includes        = -i "{SrcDir}" -d HAS_STDARG -d MAC -d MAC_TCL -i "Macintosh HD:Desktop Folder:KÂre:inc:"

Sym-PPC         = 

PPCCOptions     = {Includes} {Sym-PPC} 


### Source Files ###

SrcFiles        =  è
				  ":jkAudIO_mac.c" è
				  "{SrcDir}ffa.c" è
				  "{SrcDir}g711.c" è
				  "{SrcDir}jkCanvSect.c" è
				  "{SrcDir}jkCanvSpeg.c" è
				  "{SrcDir}jkCanvWave.c" è
				  "{SrcDir}jkPitchCmd.c" è
				  "{SrcDir}jkSound.c" è
				  "{SrcDir}jkSoundEngine.c" è
				  "{SrcDir}jkSoundEdit.c" è
				  "{SrcDir}jkSoundFile.c" è
				  "{SrcDir}jkSoundProc.c" è
				  "{SrcDir}snack.c" è
				  "{SrcDir}jkFormatMP3.c" è
				  "{SrcDir}jkMixer.c" è
				  "{SrcDir}jkAudio.c" è
				  "{SrcDir}jkFilter.c" è
				  "{SrcDir}jkFilterIIR.c" è
				  "{SrcDir}jkSynthesis.c" è
				  "{SrcDir}shape.c" è
				  "{SrcDir}snackStubInit.c"
				  
#

### Object Files ###

ObjFiles-PPC    =  è
                  "{ObjDir}jkAudIO_mac.c.x" è
				  "{ObjDir}ffa.c.x" è
				  "{ObjDir}g711.c.x" è
				  "{ObjDir}jkCanvSect.c.x" è
				  "{ObjDir}jkCanvSpeg.c.x" è
				  "{ObjDir}jkCanvWave.c.x" è
				  "{ObjDir}jkPitchCmd.c.x" è
				  "{ObjDir}jkSound.c.x" è
				  "{ObjDir}jkSoundEngine.c.x" è
				  "{ObjDir}jkSoundEdit.c.x" è
				  "{ObjDir}jkSoundFile.c.x" è
				  "{ObjDir}jkSoundProc.c.x" è
				  "{ObjDir}snack.c.x" è
				  "{ObjDir}jkFormatMP3.c.x" è
				  "{ObjDir}jkMixer.c.x" è
				  "{ObjDir}jkAudio.c.x" è
				  "{ObjDir}jkFilter.c.x" è
				  "{ObjDir}jkFilterIIR.c.x" è
				  "{ObjDir}jkSynthesis.c.x" è
				  "{ObjDir}shape.c.x" è
				  "{ObjDir}snackStubInit.c.x"

### Libraries ###

LibFiles-PPC    =  è
				  "{SharedLibraries}InterfaceLib" è
				  "{SharedLibraries}StdCLib" è
				  "{SharedLibraries}MathLib" è
				  "{PPCLibraries}StdCRuntime.o" è
				  "{PPCLibraries}PPCCRuntime.o" è
				  "{PPCLibraries}PPCToolLibs.o" è
				   "{TclLibDir}:Tcl8.3.shlb" è
				   "{TclLibDir}:Tk8.3.shlb"


### Default Rules ###

.c.x  ü  .c  {ÄMondoBuildÄ}
	{PPCC} {depDir}{default}.c -o {targDir}{default}.c.x {PPCCOptions}


### Build Rules ###

snack.shlb  üü  {ObjFiles-PPC} {LibFiles-PPC} {ÄMondoBuildÄ}
	PPCLink è
		-o {Targ} è
		{ObjFiles-PPC} è
		{LibFiles-PPC} è
		{Sym-PPC} è
		-mf -d è
		-t 'shlb' è
		-c '????' è
		-xm s è
		-export Snack_Init
	Rez snack.r -o snack.shlb

snack  üü  snack.shlb


### Required Dependencies ###

"{ObjDir}ffa.c.x"  ü  "{SrcDir}ffa.c"
"{ObjDir}g711.c.x"  ü  "{SrcDir}g711.c"
"{ObjDir}jkAudIO_mac.c.x"  ü  ":jkAudIO_mac.c"
"{ObjDir}jkCanvSect.c.x"  ü  "{SrcDir}jkCanvSect.c"
"{ObjDir}jkCanvSpeg.c.x"  ü  "{SrcDir}jkCanvSpeg.c"
"{ObjDir}jkCanvWave.c.x"  ü  "{SrcDir}jkCanvWave.c"
"{ObjDir}jkPitchCmd.c.x"  ü  "{SrcDir}jkPitchCmd.c"
"{ObjDir}jkSound.c.x"  ü  "{SrcDir}jkSound.c"
"{ObjDir}jkSoundEngine.c.x"  ü  "{SrcDir}jkSoundEngine.c"
"{ObjDir}jkSoundEdit.c.x"  ü  "{SrcDir}jkSoundEdit.c"
"{ObjDir}jkSoundFile.c.x"  ü  "{SrcDir}jkSoundFile.c"
"{ObjDir}jkSoundProc.c.x"  ü  "{SrcDir}jkSoundProc.c"
"{ObjDir}snack.c.x"  ü  "{SrcDir}snack.c"
"{ObjDir}jkFormatMP3.c.x"  ü  "{SrcDir}jkFormatMP3.c"
"{ObjDir}jkAudio.c.x"  ü  "{SrcDir}jkAudio.c"
"{ObjDir}jkFilter.c.x"  ü  "{SrcDir}jkFilter.c"
"{ObjDir}jkFilterIIR.c.x"  ü  "{SrcDir}jkFilterIIR.c"
"{ObjDir}jkSynthesis.c.x"  ü  "{SrcDir}jkSynthesis.c"
"{ObjDir}jkMixer.c.x"  ü  "{SrcDir}jkMixer.c"
"{ObjDir}shape.c.x"  ü  "{SrcDir}shape.c"
"{ObjDir}snackStubInit.c.x"  ü  "{SrcDir}snackStubInit.c"


### Optional Dependencies ###
### Build this target to generate "include file" dependencies. ###

Dependencies  ü  $OutOfDate
	MakeDepend è
		-append {MAKEFILE} è
		-ignore "{CIncludes}" è
		-objdir "{ObjDir}" è
		-objext .x è
		{Includes} è
		{SrcFiles}


