##
# Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# The contents of this file constitute Original Code as defined in and
# are subject to the Apple Public Source License Version 1.1 (the
# "License").  You may not use this file except in compliance with the
# License.  Please obtain a copy of the License at
# http://www.apple.com/publicsource and read it before using this file.
# 
# This Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
#
# implicitrules.make
#
# Implicit rules for generating and compiling source code.
#
# IMPORTED VARIABLES
#   All of the commands exported by commands-*.make
#   All of the flags exported by flags.make
#

#
# eliminate all the default suffixes
#

.SUFFIXES:

#
# compiling
#

.SUFFIXES: .h .c .m .cc .cxx .cpp .cp .C .M .s .i386.o .m68k.o .sparc.o .ppc.o .ppc64.o .x86_64.o .o


ifneq "$(LIPO)" ""

NUM_ARCHS = $(words $(ADJUSTED_TARGET_ARCHS))

%.o : $(foreach A, $(ADJUSTED_TARGET_ARCHS), %.$(A).o)
ifeq "$(NUM_ARCHS)" "1"
	$(SILENT) $(RM) -f $(OFILE_DIR)/$(notdir $*).o ; $(SYMLINK) $(OFILE_DIR)/$(notdir $*).$(ADJUSTED_TARGET_ARCHS).o $(OFILE_DIR)/$(notdir $*).o
else
	$(SILENT) $(LIPO) -create -o $(OFILE_DIR)/$(notdir $*).o $(foreach ARCH, $(ADJUSTED_TARGET_ARCHS), $(OFILE_DIR)/$(notdir $*).$(ARCH).o)
endif

# shut off any built-in rules we are overriding

%.o : %.c

%.o : %.m

%.o : %.s

%.o : %.M


# compilation rules for the thin parts of fat object files

CURRENT_ARCH = $(subst .,,$(suffix $(basename $@)))

$(OFILE_DIR)/%.ppc.o %.ppc.o: %.c
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.ppc64.o %.ppc64.o: %.c
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.i386.o %.i386.o: %.c
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.x86_64.o %.x86_64.o: %.c
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.sparc.o %.sparc.o: %.c
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.hppa.o %.hppa.o: %.c
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.m68k.o %.m68k.o: %.c
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

#$(foreach A, $(ADJUSTED_TARGET_ARCHS), $(OFILE_DIR)/%.$(A).o %.$(A).o): %.m
$(OFILE_DIR)/%.ppc.o %.ppc.o: %.m
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.ppc64.o %.ppc64.o: %.m
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.i386.o %.i386.o: %.m
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.x86_64.o %.x86_64.o: %.m
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.sparc.o %.sparc.o: %.m
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.hppa.o %.hppa.o: %.m
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.m68k.o %.m68k.o: %.m
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

#$(foreach A, $(ADJUSTED_TARGET_ARCHS), $(OFILE_DIR)/%.$(A).o %.$(A).o): %.C

$(OFILE_DIR)/%.ppc.o %.ppc.o: %.C
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.ppc64.o %.ppc64.o: %.C
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.i386.o %.i386.o: %.C
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.x86_64.o %.x86_64.o: %.C
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.sparc.o %.sparc.o: %.C
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.hppa.o %.hppa.o: %.C
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.m68k.o %.m68k.o: %.C
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

#$(foreach A, $(ADJUSTED_TARGET_ARCHS), $(OFILE_DIR)/%.$(A).o %.$(A).o): %.cc
$(OFILE_DIR)/%.ppc.o %.ppc.o: %.cc
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.ppc64.o %.ppc64.o: %.cc
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.i386.o %.i386.o: %.cc
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.x86_64.o %.x86_64.o: %.cc
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.sparc.o %.sparc.o: %.cc
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.hppa.o %.hppa.o: %.cc
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.m68k.o %.m68k.o: %.cc
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

#$(foreach A, $(ADJUSTED_TARGET_ARCHS), $(OFILE_DIR)/%.$(A).o %.$(A).o): %.M
$(OFILE_DIR)/%.ppc.o %.ppc.o: %.M
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MMFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.ppc64.o %.ppc64.o: %.M
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MMFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.i386.o %.i386.o: %.M
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MMFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.x86_64.o %.x86_64.o: %.M
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MMFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.sparc.o %.sparc.o: %.M
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MMFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.hppa.o %.hppa.o: %.M
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MMFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.m68k.o %.m68k.o: %.M
	$(CC) -arch $(CURRENT_ARCH) $(ALL_MMFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<


#$(foreach A, $(ADJUSTED_TARGET_ARCHS), $(OFILE_DIR)/%.$(A).o %.$(A).o): %.cpp
$(OFILE_DIR)/%.ppc.o %.ppc.o: %.cpp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.ppc64.o %.ppc64.o: %.cpp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.i386.o %.i386.o: %.cpp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.x86_64.o %.x86_64.o: %.cpp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.sparc.o %.sparc.o: %.cpp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.hppa.o %.hppa.o: %.cpp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.m68k.o %.m68k.o: %.cpp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

#$(foreach A, $(ADJUSTED_TARGET_ARCHS), $(OFILE_DIR)/%.$(A).o %.$(A).o): %.cp
$(OFILE_DIR)/%.ppc.o %.ppc.o: %.cp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -x c++ -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.ppc64.o %.ppc64.o: %.cp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -x c++ -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.i386.o %.i386.o: %.cp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.x86_64.o %.x86_64.o: %.cp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.sparc.o %.sparc.o: %.cp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.hppa.o %.hppa.o: %.cp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.m68k.o %.m68k.o: %.cp
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

#$(foreach A, $(ADJUSTED_TARGET_ARCHS), $(OFILE_DIR)/%.$(A).o %.$(A).o): %.cxx

$(OFILE_DIR)/%.ppc.o %.ppc.o: %.cxx
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.ppc64.o %.ppc64.o: %.cxx
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.i386.o %.i386.o: %.cxx
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.x86_64.o %.x86_64.o: %.cxx
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.sparc.o %.sparc.o: %.cxx
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.hppa.o %.hppa.o: %.cxx
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.m68k.o %.m68k.o: %.cxx
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<


#$(foreach A, $(ADJUSTED_TARGET_ARCHS), $(OFILE_DIR)/%.$(A).o %.$(A).o): %.s

$(OFILE_DIR)/%.ppc.o %.ppc.o: %.s	
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.ppc64.o %.ppc64.o: %.s	
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.i386.o %.i386.o: %.s	
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.x86_64.o %.x86_64.o: %.s	
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.sparc.o %.sparc.o: %.s	
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.hppa.o %.hppa.o: %.s	
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<

$(OFILE_DIR)/%.m68k.o %.m68k.o: %.s	
	$(CC) -arch $(CURRENT_ARCH) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$(notdir $@) $<


else

# compilation on platforms not supporting fat .o's and hard links

.c.o:
	$(CC) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<

.m.o:
	$(CC) $(ALL_MFLAGS) -c -o $(OFILE_DIR)/$*.o $<

.cc.o .C.o .cxx.o .cpp.o .cp.o:
	$(CC) $(ALL_CCFLAGS) -c -o $(OFILE_DIR)/$*.o $<

.M.o:
ifneq "YES" "$(DISABLE_OBJCPLUSPLUS)"
	$(CC) $(ALL_MMFLAGS) -c -o $(OFILE_DIR)/$*.o $<
else
	$(SILENT) $(ECHO) Sorry, Objective-C++ compilation of $*.M not supported.
	$(TOUCH) $(SFILE_DIR)/$*.m
	$(CC) -c $(SFILE_DIR)/$*.m -o $(OFILE_DIR)/$*.o
	$(RM) -f $(SFILE_DIR)/$*.m
endif

.s.o:
	$(CC) $(ALL_CFLAGS) -c -o $(OFILE_DIR)/$*.o $<
endif

#
# pswraps
#

.SUFFIXES: .psw .pswm .h .c .m

.psw.h: 
	$(PSWRAP) $(ALL_PSWFLAGS) -a -h $(SFILE_DIR)/$*.h -o $(SFILE_DIR)/$*.c $<
.psw.c:
	$(PSWRAP) $(ALL_PSWFLAGS) -a -h $(SFILE_DIR)/$*.h -o $(SFILE_DIR)/$*.c $<
.pswm.h:
	$(PSWRAP) $(ALL_PSWFLAGS) -a -h $(SFILE_DIR)/$*.h -o $(SFILE_DIR)/$*.m $<
.pswm.m:
	$(PSWRAP) $(ALL_PSWFLAGS) -a -h $(SFILE_DIR)/$*.h -o $(SFILE_DIR)/$*.m $<

#
# Yacc and Lex
#

.SUFFIXES: .y .l .ym .lm .h .c 

.y.c .y.h:
	$(CD) $(SFILE_DIR) && $(YACC) $(ALL_YFLAGS) $(shell pwd)/$<
	$(CP) $(SFILE_DIR)/y.tab.h $(SFILE_DIR)/$*.h
	$(MV) $(SFILE_DIR)/y.tab.c $(SFILE_DIR)/$*.c

.ym.m .ym.h:
	$(CD) $(SFILE_DIR) && $(YACC) $(ALL_YFLAGS) $(shell pwd)/$<
	$(CP) $(SFILE_DIR)/y.tab.h $(SFILE_DIR)/$*.h
	$(MV) $(SFILE_DIR)/y.tab.c $(SFILE_DIR)/$*.m

.l.c:
	$(CD) $(SFILE_DIR) && $(LEX) $(ALL_LFLAGS) $(shell pwd)/$<
	$(MV) $(SFILE_DIR)/lex.yy.c $(SFILE_DIR)/$*.c

.lm.m:
	$(CD) $(SFILE_DIR) && $(LEX) $(ALL_LFLAGS) $(shell pwd)/$<
	$(MV) $(SFILE_DIR)/lex.yy.c $(SFILE_DIR)/$*.m

#
# mig, msgwrap, and rpcgen
#

ifneq "" "$(MIG)"
%.h %Server.c %User.c: %.defs
	$(CD) $(SFILE_DIR) && $(MIG) $(ALL_MIGFLAGS) $(shell pwd)/$<
%.h %Server.c %User.c: %.mig
	$(CD) $(SFILE_DIR) && $(MIG) $(ALL_MIGFLAGS) $(shell pwd)/$<
else
%.h: %.h.mig
	$(CP) $< $(SFILE_DIR)/$@
%User.c: %User.c.mig
	$(CP) $< $(SFILE_DIR)/$@
%Server.c: %Server.c.mig
	$(CP) $< $(SFILE_DIR)/$@
endif

%Speaker.h %Speaker.m %Listener.h %Listener.m: %.msg
	$(CD) $(SFILE_DIR) && $(MSGWRAP) $(shell pwd)/$<

%.h: %.x
	$(RPCGEN) $(ALL_RPCFLAGS) -h -o $(SYM_DIR)/$*.h $*.x
%.c: %.x_svc
	$(RPCGEN) $(ALL_RPCFLAGS) -s udp -s tcp -o $(SYM_DIR)/$*_svc.c $*.x
%.c: %.x_clnt
	$(RPCGEN) $(ALL_RPCFLAGS) -l -o $(SYM_DIR)/$*_clnt.c $*.x
%.c: %.x_xdr
	$(RPCGEN) $(ALL_RPCFLAGS) -c -o $(SYM_DIR)/$*_xdr.c $*.x
