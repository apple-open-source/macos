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
#                               basicrules.make
#

.SUFFIXES:
.SUFFIXES: .o .p .C .cc .cxx .cpp .c .m .M .s .h .ym .y .lm .l .pswm .psw \
           .mig .def .msg .x _svc.c _clnt.c _xdr.c .bproj .subproj .tproj \
	    .store .copy .strip .installsrc .installhdrs $(BUNDLE_EXTENSION) \
	   .depend .lproj


LOCAL_DIR_INCLUDE_DIRECTIVE = -I.

# Compilation rules:
.c.o:
	$(CC) $(ALL_CFLAGS) $(LOCAL_DIR_INCLUDE_DIRECTIVE) -c $< -o $(OFILE_DIR)/$*.o
.m.o:
	$(CC) $(ALL_CFLAGS) $(LOCAL_DIR_INCLUDE_DIRECTIVE) -c $< -o $(OFILE_DIR)/$*.o
.M.o:
	@(if [ "$(DISABLE_OBJCPLUSPLUS)" != "YES" ] ; then \
	   cmd="$(CC) $(OBJCPLUS_FLAG) $(ALL_CFLAGS) $(C++CFLAGS) $(LOCAL_DIR_INCLUDE_DIRECTIVE) -c $< -o $(OFILE_DIR)/$*.o" ;\
	   $(ECHO) $$cmd ; $$cmd ; \
	else \
	   $(ECHO) Sorry, Objective-C++ compilation of $*.M not supported. ;\
	   $(TOUCH) $(SYM_DIR)/$*.m ; \
	   $(CC) -c $(SYM_DIR)/$*.m -o $(OFILE_DIR)/$*.o ; \
	   $(RM) -f $(SYM_DIR)/$*.m ; \
	fi)

.C.o:
	@(if [ "$(DISABLE_OBJCPLUSPLUS)" != "YES" ] ; then \
	   cmd="$(CC) $(OBJCPLUS_FLAG) $(ALL_CFLAGS) $(C++CFLAGS) $(LOCAL_DIR_INCLUDE_DIRECTIVE) -c $< -o $(OFILE_DIR)/$*.o" ;\
	   $(ECHO) $$cmd ; $$cmd ; \
	else \
	   $(ECHO) Sorry, Objective-C++ compilation of $*.C not supported. ;\
	   $(TOUCH) $(SYM_DIR)/$*.m ; \
	   $(CC) -c $(SYM_DIR)/$*.m -o $(OFILE_DIR)/$*.o ; \
	   $(RM) -f $(SYM_DIR)/$*.m ; \
	fi)

.cc.o:
	$(CC) $(OBJCPLUS_FLAG) $(ALL_CFLAGS) $(C++CFLAGS) $(LOCAL_DIR_INCLUDE_DIRECTIVE) -c $< -o $(OFILE_DIR)/$*.o
.cxx.o:
	$(CC) $(OBJCPLUS_FLAG) $(ALL_CFLAGS) $(C++CFLAGS) $(LOCAL_DIR_INCLUDE_DIRECTIVE) -c $< -o $(OFILE_DIR)/$*.o
.cpp.o:
	$(CC) $(OBJCPLUS_FLAG) $(ALL_CFLAGS) $(C++CFLAGS) $(LOCAL_DIR_INCLUDE_DIRECTIVE) -c $< -o $(OFILE_DIR)/$*.o
.s.o:
	$(CC) $(ALL_CFLAGS) $(LOCAL_DIR_INCLUDE_DIRECTIVE) -c $< -o $(OFILE_DIR)/$*.o
.h.p:
	-$(CC) -precomp $(ALL_PRECOMP_CFLAGS) $*.h -o $*.p
        # Note that because precomps must go in the same directory as the .h
        # we may not be able to write the precomp, so use a '-'

refresh_precomps:
	@(if [ -z "$(DISABLE_PRECOMPS)" -a -n "$(ALL_PRECOMPS)" ] ; then \
	    cmd="$(FIXPRECOMPS) -precomps $(ALL_PRECOMPS) -update $(ALL_PRECOMP_CFLAGS)"; \
	    $$cmd ; \
	fi)

$(VERS_OFILE): $(VERS_FILE)
	$(CC) $(ALL_CFLAGS) -c $(VERS_FILE) -o $@
	
# pswrap-related rules:
ALL_PSWFLAGS = $(PSWFLAGS) -H AppKit
.psw.h: 
	$(PSWRAP) $(ALL_PSWFLAGS) -a -h $(SYM_DIR)/$*.h -o $(SYM_DIR)/$*.c $<
.psw.c:
	$(PSWRAP) $(ALL_PSWFLAGS) -a -h $(SYM_DIR)/$*.h -o $(SYM_DIR)/$*.c $<
.psw.o:
	$(PSWRAP) $(ALL_PSWFLAGS) -a -h $(SYM_DIR)/$*.h -o $(SYM_DIR)/$*.c $<
	$(CC) $(ALL_CFLAGS) -c $(SYM_DIR)/$*.c -o $(OFILE_DIR)/$*.o
.pswm.h:
	$(PSWRAP) $(ALL_PSWFLAGS) -a -h $(SYM_DIR)/$*.h -o $(SYM_DIR)/$*.m $<
.pswm.m:
	$(PSWRAP) $(ALL_PSWFLAGS) -a -h $(SYM_DIR)/$*.h -o $(SYM_DIR)/$*.m $<
.pswm.o:
	$(PSWRAP) $(ALL_PSWFLAGS) -a -h $(SYM_DIR)/$*.h -o $(SYM_DIR)/$*.m $<
	$(CC) $(ALL_CFLAGS) -c $(SYM_DIR)/$*.m -o $(OFILE_DIR)/$*.o


# yacc and lex-related rules
.y.o:
	@(initdir=`pwd`;                                                \
	cd $(SYM_DIR);                                                  \
	  cmd="$(YACC) $(YFLAGS) $$initdir/$*.y" ; echo $$cmd; $$cmd ;	\
	cd $$initdir ; 							\
	cmd="$(MV) $(MVFLAGS) $(SYM_DIR)/y.tab.c $(SYM_DIR)/$*.c" ; echo $$cmd; $$cmd ;	\
	cmd="$(CP) $(MVFLAGS) $(SYM_DIR)/y.tab.h $(SYM_DIR)/$*.h" ; echo $$cmd; $$cmd ;	\
	cmd="$(CC) $(ALL_CFLAGS) -I$$initdir -c $(SYM_DIR)/$*.c -o $(OFILE_DIR)/$*.o" ; \
	echo $$cmd ; $$cmd )

.y.c .y.h:
	@(initdir=`pwd`;                                                \
	cd $(SYM_DIR);                                                  \
	  cmd="$(YACC) $(YFLAGS) $$initdir/$*.y" ; echo $$cmd; $$cmd ;	\
	cd $$initdir ; 							\
	cmd="$(MV) $(MVFLAGS) $(SYM_DIR)/y.tab.c $(SYM_DIR)/$*.c" ; echo $$cmd; $$cmd ;	\
	cmd="$(CP) $(CPFLAGS) $(SYM_DIR)/y.tab.h $(SYM_DIR)/$*.h" ; echo $$cmd; $$cmd )

.ym.o:
	@(initdir=`pwd`;                                                 \
	cd $(SYM_DIR);                                                  \
	  cmd="$(YACC) $(YFLAGS) $$initdir/$*.ym" ; echo $$cmd; $$cmd ;	\
	cd $$initdir ; 							\
	cmd="$(MV) $(MVFLAGS) $(SYM_DIR)/y.tab.c $(SYM_DIR)/$*.m" ; echo $$cmd; $$cmd ;	\
	cmd="$(CP) $(CPFLAGS) $(SYM_DIR)/y.tab.h $(SYM_DIR)/$*.h" ; echo $$cmd; $$cmd ;	\
	cmd="$(CC) $(ALL_CFLAGS) -I$$initdir -c $(SYM_DIR)/$*.m -o $(OFILE_DIR)/$*.o" ; \
	echo $$cmd ; $$cmd )

.ym.m .ym.h:
	@(initdir=`pwd`;                                                 \
	cd $(SYM_DIR);                                                  \
	  cmd="$(YACC) $(YFLAGS) $$initdir/$*.ym" ; echo $$cmd; $$cmd ;	\
	cd $$initdir ; 							\
	cmd="$(MV) $(MVFLAGS) $(SYM_DIR)/y.tab.c $(SYM_DIR)/$*.m" ; echo $$cmd; $$cmd ;	\
	cmd="$(CP) $(CPFLAGS) $(SYM_DIR)/y.tab.h $(SYM_DIR)/$*.h" ; echo $$cmd; $$cmd )

.l.o:
	@(initdir=`pwd`;                                      		\
	cd $(SYM_DIR);                         				\
	  cmd="$(LEX) $(LFLAGS) $$initdir/$*.l" ; echo $$cmd; $$cmd ;	\
	cd $$initdir ; 							\
	cmd="$(MV) $(MVFLAGS) $(SYM_DIR)/lex.yy.c $(SYM_DIR)/$*.c" ; echo $$cmd; $$cmd ;	\
	cmd="$(CC) $(ALL_CFLAGS) -I$$initdir -c $(SYM_DIR)/$*.c -o $(OFILE_DIR)/$*.o" ; \
	echo $$cmd ; $$cmd )

.l.c:
	@(initdir=`pwd`;                                      		\
	cd $(SYM_DIR);                         				\
	  cmd="$(LEX) $(LFLAGS) $$initdir/$*.l" ; echo $$cmd; $$cmd ;	\
	cd $$initdir ; 							\
	cmd="$(MV) $(MVFLAGS) $(SYM_DIR)/lex.yy.c $(SYM_DIR)/$*.c" ; echo $$cmd; $$cmd )

.lm.o:
	@(initdir=`pwd`;                                      		\
	cd $(SYM_DIR);                         				\
	  cmd="$(LEX) $(LFLAGS) $$initdir/$*.lm" ; echo $$cmd; $$cmd ;	\
	cd $$initdir ; 							\
	cmd="$(MV) $(MVFLAGS) $(SYM_DIR)/lex.yy.c $(SYM_DIR)/$*.m" ; echo $$cmd; $$cmd ;	\
	cmd="$(CC) $(ALL_CFLAGS) -I$$initdir -c $(SYM_DIR)/$*.m -o $(OFILE_DIR)/$*.o" ; \
	echo $$cmd ; $$cmd )

.lm.m:
	@(initdir=`pwd`;                                      		\
	cd $(SYM_DIR);                         				\
	  cmd="$(LEX) $(LFLAGS) $$initdir/$*.lm" ; echo $$cmd; $$cmd ;	\
	cd $$initdir ; 							\
	cmd="$(MV) $(MVFLAGS) $(SYM_DIR)/lex.yy.c $(SYM_DIR)/$*.m" ; echo $$cmd; $$cmd )


# msgwrap-related rules - note that suffix rules will not work because the basename changes and there is no one-to-one mapping from .msg's to .o's.

$(MSGDERIVEDMFILES): $(MSGFILES) $(OTHER_MSG_DEPENDS)
	@(if [ "$(MSGFILES)" != "" ] ; then \
	        $(MKDIRS) $(SYM_DIR) ; \
		$(CP) $(MSGFILES) $(SYM_DIR) ; \
		cd $(SYM_DIR) ; \
		for msgfile in $(MSGFILES) ; do \
		    cmd="$(MSGWRAP) $$msgfile" ; \
		    echo $$cmd ; $$cmd ; \
		done ; \
		$(RM) -f $(MSGFILES) ; \
	fi)

# mig-related rules

$(ALLMIGDERIVEDSRCFILES): $(ALLMIGFILES) $(OTHER_MIG_DEPENDS)
	@(if [ -n "$(ALLMIGFILES)" ] ; then \
	        $(MKDIRS) $(SYM_DIR) ; \
		$(CP) $(ALLMIGFILES) $(SYM_DIR) ; \
		cd $(SYM_DIR) ; \
		for migfile in $(ALLMIGFILES) ; do \
		    cmd="$(MIG) $(MIGFLAGS) $$migfile" ; \
		    echo $$cmd ; $$cmd ; \
		done ; \
		$(RM) -f $(ALLMIGFILES) ; \
	fi)

# rpcgen-related rules

.x.h:
	$(RPCGEN) $(RPCFLAGS) -h -o $(SYM_DIR)/$*.h $*.x
.x_svc.c:
	$(RPCGEN) $(RPCFLAGS) -s udp -s tcp -o $(SYM_DIR)/$*_svc.c $*.x
.x_clnt.c:
	$(RPCGEN) $(RPCFLAGS) -l -o $(SYM_DIR)/$*_clnt.c $*.x
.x_xdr.c:
	$(RPCGEN) $(RPCFLAGS) -c -o $(SYM_DIR)/$*_xdr.c $*.x
