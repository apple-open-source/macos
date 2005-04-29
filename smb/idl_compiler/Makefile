# 
# (c) Copyright 1994 OPEN SOFTWARE FOUNDATION, INC.
# (c) Copyright 1994 HEWLETT-PACKARD COMPANY
# (c) Copyright 1994 DIGITAL EQUIPMENT CORPORATION
# To anyone who acknowledges that this file is provided "AS IS"
# without any express or implied warranty:
#                 permission to use, copy, modify, and distribute this
# file for any purpose is hereby granted without fee, provided that
# the above copyright notices and this notice appears in all source
# code copies, and that none of the names of Open Software
# Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
# Corporation be used in advertising or publicity pertaining to
# distribution of the software without specific, written prior
# permission.  Neither Open Software Foundation, Inc., Hewlett-
# Packard Company, nor Digital Equipment Corporation makes any
# representations about the suitability of this software for any
# purpose.
# 

CC	      = gcc
MV	      = mv
SED	      = sed
GENCAT	      = gencat
LEX	      = lex
YACC	      = bison -y

CFLAGS	      = -I. -DMIA -D_BSD -DYYPURE -DDUMPERS -g
LDFLAGS	      = -g
YFLAGS	      = -d

DEST	      = .

EXTHDRS	      =

HDRS	      = acf.h \
		ast.h \
		astp.h \
		astp_dmp.h \
		backend.h \
		be_pvt.h \
		bedeck.h \
		checker.h \
		chkichar.h \
		clihamts.h \
		clihandl.h \
		command.h \
		commstat.h \
		cspeldcl.h \
		cspell.h \
		cstubgen.h \
		cstubmts.h \
		ddbe.h \
		driver.h \
		dutils.h \
		errors.h \
		fe_pvt.h \
		files.h \
		frontend.h \
		genpipes.h \
		getflags.h \
		hdgen.h \
		icharsup.h \
		ifspec.h \
		ir_pvt.h \
		irep.h \
		marshall.h \
		message.h \
		mtsbacke.h \
		mtspipes.h \
		nametbl.h \
		namtbpvt.h \
		nidl.h \
		nidlmsg.h \
		propagat.h \
		sstubmts.h \
		sysdep.h \
		user_exc.h

INSTALL	      = /usr/bin/install

LD	      = $(CC)

LDFLAGS	      =

LIBS	      =

MAKEFILE      = Makefile

OBJS	      = acf_lex_yy.o \
		acf_y_tab.o \
		astp_com.o \
		astp_cpx.o \
		astp_dmp.o \
		astp_gbl.o \
		astp_sim.o \
		checker.o \
		chkichar.o \
		clihamts.o \
		command.o \
		comstmts.o \
		cspeldcl.o \
		cspell.o \
		cstubmts.o \
		ddbe.o \
		ddspell.o \
		driver.o \
		errors.o \
		files.o \
		frontend.o \
		getflags.o \
		hdgen.o \
		icharsup.o \
		idlparse.o \
		ifspemts.o \
		irepgen.o \
		irepscp.o \
		keywds.o \
		lex_yy.o \
		main.o \
		message.o \
		mtsbacke.o \
		mtspipes.o \
		namdump.o \
		nametbl.o \
		propagat.o \
		sstubmts.o \
		sysdep.o \
		user_exc.o

PRINT	      = pr

PROGRAM       = idl

SRCS	      = acf.l \
		acf.y \
		acf_lex_yy.c \
		astp_com.c \
		astp_cpx.c \
		astp_dmp.c \
		astp_gbl.c \
		astp_sim.c \
		checker.c \
		chkichar.c \
		clihamts.c \
		command.c \
		comstmts.c \
		cspeldcl.c \
		cspell.c \
		cstubmts.c \
		ddbe.c \
		ddspell.c \
		driver.c \
		errors.c \
		files.c \
		frontend.c \
		getflags.c \
		hdgen.c \
		icharsup.c \
		idlparse.c \
		ifspemts.c \
		irepgen.c \
		irepscp.c \
		keywds.c \
		main.c \
		message.c \
		mtsbacke.c \
		mtspipes.c \
		namdump.c \
		nametbl.c \
		nidl.l \
		nidl.y \
		propagat.c \
		sstubmts.c \
		sysdep.c \
		user_exc.c

SYSHDRS	      =

all:		$(PROGRAM) idl.cat

$(PROGRAM):     $(OBJS) $(LIBS)
		@echo "Linking $(PROGRAM) ..."
		$(CC) $(LDFLAGS) $(OBJS) -o $(PROGRAM)
		@echo "done"

clean:;		@rm -f $(OBJS) core \
		acf_lex_yy_i.h acf_y_tab.c acf_y_tab.h \
		lex_yy.c y_tab.c.h y_tab.h default_msg.h

clobber:;	@rm -f $(OBJS) $(PROGRAM) core tags idl.cat \
		acf_lex_yy_i.h acf_y_tab.c acf_y_tab.h \
		lex_yy.c y_tab.c.h y_tab.h default_msg.h

depend:;	@mkmf -f $(MAKEFILE) ROOT=$(ROOT)

echo:;		@echo $(HDRS) $(SRCS)

index:;		@ctags -wx $(HDRS) $(SRCS)

print:;		@$(PRINT) $(HDRS) $(SRCS)

tags:           $(HDRS) $(SRCS); @ctags $(HDRS) $(SRCS)

update:		$(DEST)/$(PROGRAM)

#
# dependencies and rules
#
$(OBJS): y_tab.h

acf_lex_yy.o: acf_lex_yy.c acf_lex_yy_i.h acf_y_tab.h

acf_lex_yy_i.h:  acf.l
	$(LEX) $(LFLAGS) -t $? >$@

acf_y_tab.c acf_y_tab.h: acf.y
	$(YACC) $(YFLAGS) $?
	$(MV) y.tab.c acf_y_tab.c
	$(MV) y.tab.h acf_y_tab.h

lex_yy.c: nidl.l
	$(LEX) $(LFLAGS) -t $? >$@

y_tab.c.h y_tab.h: nidl.y
	$(YACC) $(YFLAGS) $?
	$(MV) y.tab.c y_tab.c.h
	$(MV) y.tab.h y_tab.h

idlparse.o:	y_tab.c.h y_tab.h

lex_yy.o: y_tab.h

# This rule creates an include file containing the text of messages in
# the message file, so that IDL can print default messages.
# The following sed command (note that $ must be doubled in the Makefile):
#	1. Deletes lines beginning with $.
#	2. Deletes blank lines.
#	3. Changes lines of the form:
#		<number> <message test>
#	    to
#		"message text",		
default_msg.h: nidlmsg.msgs
	$(SED)  -e '/^\$$/d;/^$$/d;s/^[^ ]* /"/;s/$$/",/;' \
		$? >$@

message.o:	default_msg.h

# Generate the message catalog
idl.cat: nidlmsg.msgs
	$(GENCAT) $@ $?

#
# following dependencies are incomplete
#
###
acf.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h astp.h command.h
acf.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h acf.h \
	ast.h be_pvt.h ir_pvt.h fe_pvt.h astp.h command.h message.h files.h \
	propagat.h \
	checker.h
acf_lex_yy.o: acf.h
astp_com.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h astp.h \
	ast.h be_pvt.h ir_pvt.h fe_pvt.h checker.h message.h backend.h \
	command.h
astp_cpx.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h astp.h \
	ast.h be_pvt.h ir_pvt.h fe_pvt.h
astp_dmp.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h astp.h \
	ast.h be_pvt.h ir_pvt.h fe_pvt.h astp_dmp.h
astp_gbl.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h astp.h \
	ast.h be_pvt.h ir_pvt.h fe_pvt.h
astp_sim.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h astp.h \
	ast.h be_pvt.h ir_pvt.h fe_pvt.h
checker.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h checker.h \
	ast.h be_pvt.h ir_pvt.h fe_pvt.h chkichar.h astp.h command.h \
	message.h
chkichar.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h checker.h \
	ast.h be_pvt.h ir_pvt.h fe_pvt.h chkichar.h astp.h command.h \
	message.h
clihamts.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h bedeck.h commstat.h ifspec.h clihamts.h \
	clihandl.h dutils.h cspell.h cstubgen.h marshall.h
command.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h command.h \
	files.h \
	getflags.h message.h
comstmts.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h bedeck.h commstat.h cspell.h
cspeldcl.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h bedeck.h cspell.h cspeldcl.h hdgen.h
cspell.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h cspell.h cspeldcl.h genpipes.h bedeck.h
cstubmts.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h command.h cspell.h cspeldcl.h ddbe.h \
	checker.h irep.h ifspec.h commstat.h clihamts.h clihandl.h cstubgen.h \
	mtsbacke.h bedeck.h dutils.h genpipes.h marshall.h backend.h \
	cstubmts.h user_exc.h icharsup.h
ddbe.o: ddbe.h ast.h nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h be_pvt.h \
	ir_pvt.h fe_pvt.h checker.h irep.h astp.h command.h message.h \
	mtsbacke.h bedeck.h dutils.h genpipes.h marshall.h backend.h
ddspell.o: ddbe.h ast.h nidl.h \
	sysdep.h \
	errors.h nametbl.h nidlmsg.h be_pvt.h ir_pvt.h fe_pvt.h checker.h \
	irep.h astp.h command.h message.h mtsbacke.h bedeck.h dutils.h \
	genpipes.h marshall.h backend.h
driver.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h \
	driver.h ast.h be_pvt.h \
	ir_pvt.h fe_pvt.h command.h files.h \
	backend.h message.h frontend.h
errors.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h \
	frontend.h ast.h be_pvt.h ir_pvt.h fe_pvt.h message.h driver.h
files.o: \
	nidl.h \
	sysdep.h \
	errors.h nametbl.h nidlmsg.h files.h
frontend.o: \
	frontend.h \
	nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h astp.h astp_dmp.h checker.h command.h \
	files.h getflags.h propagat.h message.h
getflags.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h \
	getflags.h command.h driver.h message.h
hdgen.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h cspell.h cspeldcl.h files.h \
	genpipes.h \
	hdgen.h
icharsup.o: ast.h nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h be_pvt.h \
	ir_pvt.h fe_pvt.h bedeck.h clihandl.h commstat.h dutils.h icharsup.h
ifspemts.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h cspell.h command.h ifspec.h bedeck.h \
	dutils.h mtsbacke.h genpipes.h marshall.h backend.h
irepgen.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h astp.h irep.h
irepscp.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h irep.h
keywds.o: astp.h nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h
main.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h driver.h
message.o: \
	nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h \
	message.h
mtsbacke.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h irep.h command.h ddbe.h checker.h \
	mtsbacke.h bedeck.h dutils.h genpipes.h marshall.h backend.h
mtspipes.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h bedeck.h cspell.h ddbe.h checker.h irep.h \
	dutils.h mtspipes.h
namdump.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h \
	namtbpvt.h
nametbl.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h \
	namtbpvt.h
nidl.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h \
	ast.h be_pvt.h ir_pvt.h fe_pvt.h astp.h driver.h \
	command.h
nidl.o: \
	nidl.h \
	sysdep.h \
	errors.h nametbl.h nidlmsg.h ast.h be_pvt.h ir_pvt.h fe_pvt.h astp.h \
	frontend.h
propagat.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h \
	propagat.h ast.h be_pvt.h ir_pvt.h fe_pvt.h astp.h checker.h \
	command.h ddbe.h irep.h
sstubmts.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h ast.h \
	be_pvt.h ir_pvt.h fe_pvt.h bedeck.h command.h ddbe.h checker.h irep.h \
	ifspec.h cspeldcl.h cspell.h mtsbacke.h dutils.h genpipes.h \
	marshall.h backend.h mtspipes.h sstubmts.h user_exc.h clihandl.h \
	commstat.h icharsup.h cstubmts.h
sysdep.o: nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h
user_exc.o: ast.h nidl.h \
	sysdep.h errors.h nametbl.h nidlmsg.h be_pvt.h \
	ir_pvt.h fe_pvt.h cspell.h user_exc.h
