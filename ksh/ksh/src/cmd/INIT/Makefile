/*
 * initialization for all packages
 */

LICENSE = since=1994,author=gsf

PACKAGEROOT = $(INSTALLROOT:T=F:P=L*:O=n)

hurl :: hurl.sh LICENSE=since=2003,author=gsf

iffe :: iffe.sh LICENSE=since=1994,author=gsf+kpv

package :: package.sh

regress :: regress.sh LICENSE=since=1995,author=gsf

$(PACKAGEROOT)/bin :INSTALLDIR: execrate ignore mamprobe package silent

crossexec :: crossexec.sh

ditto :: ditto.sh LICENSE=since=2001,author=gsf+ek

execrate :: execrate.sh LICENSE=since=2002,author=gsf

filter :: filter.sh

ignore :: ignore.sh

silent :: silent.sh

$(PACKAGEROOT)/lib/package :INSTALLDIR: package.mk CONVERT.mk

$(LIBDIR)/make :INSTALLDIR: package.mk PROBE.mk TEST.mk WWW.mk \
	MSGFUN.mk MSGKEY.mk

cc ld ldd :PACKAGE_INIT: mamake.c proto.c ratz.c release.c

:: RELEASE hosttype.tst p.c hello.c \
	cc.darwin.ppc cc.hp.pa cc.hp.pa64 cc.ibm.risc cc.mvs.390 \
	cc.next.i386 cc.next.m68k cc.osf.alpha cc.sco.i386 cc.sgi.mips2 \
	cc.sgi.mips3 cc.sgi.mips3-o32 cc.sgi.mips4 cc.sgi.mips4-n32 \
	cc.unix.mc68k \
	ld.hp.pa \
	ldd.cygwin.i386 ldd.darwin.ppc ldd.hp.pa ldd.ibm.risc \
	ldd.mvs.390 ldd.sgi.mips2 ldd.sgi.mips3 ldd.sgi.mips4

LICENSE : .DONTCARE

if ! CC.HOSTTYPE
	CC.HOSTTYPE := $(_hosttype_|HOSTTYPE)
end

$(BINDIR)/mkdir :INSTALL: mkdir.sh
	if	test ! -x $(<) -a -x /bin/mkdir
	then	mkdir -p 2>/dev/null || :
		if	test -d ./-p
		then	rmdir ./-p
			cp $(*) $(<)
			chmod +x $(<)
		fi
	fi

/*
 * ksh93 function search on PATH
 * ksh93 ld library path search on PATH
 */

$(BINDIR)/.paths :INSTALL:
	{
	echo FPATH=../fun
	set x x : $(-mam:??$(.GETCONF LIBPATH:/:/ /G:/,/ : /G:Q)?)
	p=
	while	:
	do	while	:
		do	case $# in
			0)	break ;;
			esac
			x=$1
			shift
			case $x in
			:)	break ;;
			esac
		done
		case $# in
		0|1)	break
			;;
		2)	p=$2
			;;
		*)	case "$(CC.HOSTTYPE)" in
			$3)	p=$2; break ;;
			esac
			;;
		esac
	done
	case $p in
	'')	if	test -x /lib/dld.sl
		then	p=SHLIB_PATH
		elif	test -x /usr/lib/dyld
		then	p=DYLD_LIBRARY_PATH
		else	case "$(CC.HOSTTYPE)" in
			ibm.*|mvs.*)
				p=LIBPATH
				;;
			sgi.mips3)	
				p=LD_LIBRARYN32_PATH
				;;
			sgi.mips4)	
				p=LD_LIBRARYN64_PATH
				;;
			*)	p=LD_LIBRARY_PATH
				;;
			esac
		fi
		;;
	esac
	echo $p=../lib
	} > $(<)

/*
 * probe initialization
 */

for T C+probe C+make+probe.lcl C+pp+probe.lcl
	if T == "*.lcl"
		$(T) : .DONTCARE
	end
	$(LIBDIR)/probe/$(T:C,+,/,G) :INSTALL: $(T)
end

$(LIBDIR)/probe/C/mam/probe :INSTALL: mprobe

mprobe :: mprobe.sh

$(LIBDIR)/probe/C/make :INSTALLDIR: probe probe.ini

probe.sh : C+probe make.probe
	cat $(*) > $(<)

probe :: probe.sh

probe.ini : (CC.HOSTTYPE) probe.win32
	t=$(CC.HOSTTYPE)
	ifs=$IFS
	IFS=.
	set x $t
	IFS=$ifs
	t=$2
	set x $(*)
	while	:
	do	shift
		case $# in
		0)	break ;;
		esac
		case $1 in
		*probe.$t)
			break
		esac
	done
	case $1 in
	?*)	cmp -s $1 $(<) || cp $1 $(<) ;;
	*)	: > $(<)
	esac

/*
 * proto initialization
 */

$(INCLUDEDIR) :INSTALLDIR: prototyped.h

prototyped.h : $(BINDIR)/proto
	proto -f /dev/null > h.$(tmp).h
	if	$(CMP) -s h.$(tmp).h $(<)
	then	$(RM) -f h.$(tmp).h
	else	$(MV) h.$(tmp).h $(<)
	fi

/*
 * -l* library local conventions
 */

":MAPLIB:" : .MAKE .OPERATOR
	$(LIBDIR)/lib/$(<) :INSTALL: $(<).req
	$(<).req : (CC) $(>)
		r='-'
		for i in $(*)
		do	if	$(CC) -o $(<:B).exe $i -l$(<:B) > /dev/null
			then	$(CC) -o $(<:B).exe $i > /dev/null || {
					r='-l$(<:B)'
					break
				}
			fi
		done 2>/dev/null
		echo " $r" > $(<)
		rm -f $(<:B).exe $(<:B)$(CC.SUFFIX.OBJECT)

/*
 * check if -ldl is required
 * this allows makefiles to use -ldl on all systems
 *
 * NOTE: this works around the sgi botch:
 *	 (1) irix 5.* made -ldl optional but warned
 *	 (2) irix 6.* has no -ldl
 *	 (3) dynamic progs built on irix 5.* and using -ldl fail
 *           at runtime on irix 6.* because -ldl is not there
 */

dl :MAPLIB: dl.c

/*
 * miscellaneous -l* checks
 */

m :MAPLIB: m.c

nsl :MAPLIB: nsl.c

/*
 * what was sco smoking
 * almost all of gethost* are in -lnsl except gethostbyname which
 * is in -lsocket which isn't needed to resolve socket() but seems
 * to do the -lnsl job
 */

socket :MAPLIB: socket.c nsl.c
