/*
 * ksh makefile
 * This makefile should work with nmake 2.3 and higher 
 */

if CC.HOSTTYPE == "*win*" && CC.SUFFIX.DYNAMIC == ".dll" || "$(PACKAGE_OPTIMIZE:N=space)"
	CCFLAGS += $$(CC.DLL)
	LIBS_req = -lshell
else
	CCLDFLAGS += $$(CC.EXPORT.DYNAMIC)
	LIBS_req = +lshell
	if ! VARIANT
		libtype = :static
	end
end

:PACKAGE:	ast$(libtype)

LICENSE = since=1982,author=dgk

CATALOG = libshell

.SOURCE : edit sh bltins data tests fun
.SOURCE.h : include 

SH = ksh		/* name of the shell program */
VERSION = 1.1

clobber = !($(SH))	/* move ksh to ksh.old on clobber */

/* documentation files */
DOCFILES = sh.memo builtins.mm README RELEASE PROMO.mm COMPATIBILITY \
		DESIGN OBSOLETE RELEASE93 RELEASE88

KSHELL == 1			/* this is ksh */

SH_DICT ==			/* message catalog */
SH_CMDLIB_DIR ==		/* libcmd.a builtin dir */
DEBUG ==			/* debugging purposes only */

/* set these variables to 1 to enable, 0 to disable, null to probe */

SHOPT_ACCT ==  			/* accounting */
SHOPT_ACCTFILE ==  		/* per user accounting info */
SHOPT_APPEND == 1		/* var+=val string append */
SHOPT_BASH ==			/* bash compatibility code */
SHOPT_BRACEPAT == 1		/* C-shell {...,...} expansions (, required) */
SHOPT_CMDLIB_BLTIN ==		/* make all cmds in libcmd.a builtins */
SHOPT_CMDLIB_DIR ==		/* SHOPT_CMDLIB_BLTIN=1 + "/opt/ast/bin/" */
SHOPT_COMPOUND_ARRAY == 1	/* allow compound arrays */
SHOPT_CRNL ==			/* accept <cr><nl> for <nl> */
SHOPT_DYNAMIC == 1		/* dynamic loading for builtins */
SHOPT_ECHOPRINT ==		/* make echo equivalent to print */
SHOPT_ESH == 1			/* emacs/gmacs edit mode */
SHOPT_FILESCAN == 1		/* fast file scan */
SHOPT_FS_3D == 			/* 3d file system */
SHOPT_HISTEXPAND == 		/* csh-style history file expansions */
SHOPT_KIA == 1			/* shell information database generation */
SHOPT_MULTIBYTE == 1		/* multibyte character handling */
SHOPT_NAMESPACE == 1		/* allow namespaces */
SHOPT_OLDTERMIO ==		/* support both TCGETA and TCGETS */
SHOPT_OO == 1			/* object oriented extension, not complete */
SHOPT_OPTIMIZE == 1		/* optimize loop invariants */
SHOPT_PFSH == 1			/* solaris exec_attr(4) profile execution */
SHOPT_P_SUID ==			/* real uid's that require -p for set[ug]id */
SHOPT_RAWONLY == 1		/* make viraw the only vi mode */
SHOPT_SEVENBIT ==		/* strip the eigth bit from characters */
SHOPT_SPAWN ==			/* use spawnveg for fork/exec */
SHOPT_SUID_EXEC == 1		/* allow (safe) suid/sgid shell scripts */
SHOPT_TIMEOUT ==		/* number of seconds for shell timeout */
SHOPT_VSH == 1			/* vi edit mode */

if  CC.HOSTTYPE == "*win*"
	SHOPT_CRNL == 1		/* <cr><nl> is eqivalent to <nl> */
end

if SHOPT_BASH
	BASH_HOSTTYPE == "$(CC.HOSTTYPE:S:/\.//)"
	BASH_MACHTYPE == "$(BASH_HOSTTYPE)-unknown-$(CC.HOSTTYPE:B)"
end

/* use the following libraries only if they exist */
LIBS_opt := +ljobs +li

if  CC.HOSTTYPE == "sol[89].*"
	LDFLAGS  += $(CC.LD.LAZY) $(CC.LD.NORECORD)
end

if ! "$(-mam:N=static*)" && CC.HOSTTYPE != "sgi.mips*"
	if CC.HOSTTYPE == "sol[789].*"
		LIBS_opt += +lsocket -lnsl
	else
		LIBS_opt += +lsocket +lnsl
	end
end
if CC.HOSTTYPE == "linux.sparc"
	/* the linux.sparc linker still has some problems */
	LIBS_opt += -ldll
end
LIBS_opt += -lsecdb +lintl -ldl
$(LIBS_opt)	: .DONTCARE

LDLIBRARIES = $(LIBS_opt)

FILES_opt =		/* optional file list enabled by vars above */
FILES_opt += $(SHOPT_ESH:T=V?emacs.c)
FILES_opt += $(SHOPT_VSH:T=V?vi.c)
FILES_opt += $(SHOPT_BASH:T=V?bash.c bash_pre_rc.c)
FILES_opt += $(SHOPT_HISTEXPAND:T=V?hexpand.c)
if SHOPT_X
LDFLAGS += -u  _XtAppInitialize -L/usr/add-on/X11/lib
end

:ALL: $(SH) shcomp $(SHOPT_SUID_EXEC:T=V?suid_exec.c) $(SHOPT_BASH:T=V?bash)

$(SH) :: sh.1 pmain.c $(LIBS_req)

DATAFILES = limits.c msg.c strdata.c testops.c keywords.c options.c \
                 signals.c aliases.c builtins.c variables.c lexstates.c

shell $(VERSION) :LIBRARY: shell.3 nval.3 alarm.c cd_pwd.c cflow.c deparse.c \
	getopts.c hist.c misc.c print.c read.c sleep.c trap.c test.c \
	typeset.c ulimit.c umask.c whence.c main.c nvdisc.c \
	arith.c args.c array.c completion.c defs.c edit.c expand.c \
	fault.c fcin.c history.c init.c io.c jobs.c lex.c macro.c name.c \
	nvtree.c parse.c path.c string.c streval.c subshell.c tdump.c timers.c \
	trestore.c waitevent.c xec.c env.c $(DATAFILES) $(FILES_opt) \
	-lcmd -ldll -last -lm

"sol*.i386" :NOOPTIMIZE: main.c
"win32*" :NOOPTIMIZE: trestore.c

if CC.HOSTTYPE == "win32*"
:DLL: $(CC.DEBUG)
else
:DLL: $(CC.OPTIMIZE)
end

:READONLY: $(DATAFILES)

$(INCLUDEDIR) :INSTALLPROTO: nval.h shell.h history.h

suid_exec ::	CC.DLL= suid_exec.c +last

shcomp	::	CC.DLL= SH_DICT='"libshell"' shcomp.c -lshell

$(FUNDIR) :INSTALLDIR: mode=+x dirs popd pushd

shcomp.o : _BLD_DLL= $(CC.HOSTTYPE:N=cygwin.*:??_BLD_shell=?)

if SHOPT_BASH
bash :LINK: $(SH)

bash_pre_rc.c : bash_pre_rc.sh
	echo "const char bash_pre_rc[] = " > $(<)
	sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/'"'"'/\\'"'"'/g' -e 's/^[[:space:]]*\(.*\)$/\"\1\\n\"/' $(*) >> $(<)
	echo ";" >> $(<)
end

:: syscall.s mamexec mamstate.c deparse.c shopen.c $(DOCFILES) \
	bash.c bash_pre_rc.sh hexpand.c

:: alias.sh append.sh arith.sh arrays.sh attributes.sh basic.sh bracket.sh \
	builtins.sh case.sh comvar.sh coprocess.sh exit.sh functions.sh \
	grep.sh heredoc.sh io.sh nameref.sh options.sh path.sh quoting.sh \
	quoting2.sh return.sh select.sh substring.sh tilde.sh variables.sh \
	shtests

if "$(PWD:B:N=cc-*)"

test : .NULL

else

test : $(BINDIR)/$(SH) $(SH) shtests
	$(SILENT) $(CMP) $(CMPFLAGS) $(*:O=1) $(*:O=2) 2>/dev/null ||
	echo "make install to run the tests on the latest $(SH)" >&2
	cd $(*:O=3:P=L!:D)
	SHELL=$(*:O=1:P=A) $(*:O=1:P=A) $(*:O=3:B)

end
