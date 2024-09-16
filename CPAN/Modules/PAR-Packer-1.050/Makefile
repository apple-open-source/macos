NAME = PAR-Packer
VERSION = 1.050

include ../Makefile.inc

custompatching:
	ed - $(DIR)/Makefile.PL < $(MYSRC)/Makefile.PL.ed

postinstall:
	@set -x && \
	cc -fno-typed-memory-operations -fno-typed-cxx-new-delete -o /tmp/pp_osx_codesign_fix $(DIR)/contrib/pp_osx_codesign_fix/pp_osx_codesign_fix.c && \
	/tmp/pp_osx_codesign_fix $(DSTROOT)/usr/bin/parl && \
	/tmp/pp_osx_codesign_fix $(DSTROOT)/usr/bin/parldyn || exit 1
