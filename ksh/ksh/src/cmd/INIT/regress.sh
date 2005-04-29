####################################################################
#                                                                  #
#             This software is part of the ast package             #
#                Copyright (c) 1994-2004 AT&T Corp.                #
#        and it may only be used by you under license from         #
#                       AT&T Corp. ("AT&T")                        #
#         A copy of the Source Code Agreement is available         #
#                at the AT&T Internet web site URL                 #
#                                                                  #
#       http://www.research.att.com/sw/license/ast-open.html       #
#                                                                  #
#    If you have copied or used this software without agreeing     #
#        to the terms of the license you are infringing on         #
#           the license and copyright and are violating            #
#               AT&T's intellectual property rights.               #
#                                                                  #
#            Information and Software Systems Research             #
#                        AT&T Labs Research                        #
#                         Florham Park NJ                          #
#                                                                  #
#               Glenn Fowler <gsf@research.att.com>                #
#                                                                  #
####################################################################
: regress - run regression tests in command.tst

command=regress
case $(getopts '[-][123:xyz]' opt --xyz 2>/dev/null; echo 0$opt) in
0123)	USAGE=$'
[-?
@(#)$Id: regress (AT&T Labs Research) 2004-01-11 $
]
'$USAGE_LICENSE$'
[+NAME?regress - run regression tests]
[+DESCRIPTION?\bregress\b runs the tests in \aunit\a, or \aunit\a\b.tst\b
	if \aunit\a does not exist. If \acommand\a is omitted then it is
	assumed to be the base name of \aunit\a. All testing is done in the
	temporary directory \aunit\a\b.tmp\b.]
[+?Default test output lists the \anumber\a and \adescription\a for each active
	\bTEST\b group and the \anumber\a:\aline\a for each individual
	\bEXEC\b test. Each test that fails results in a diagnostic that
	contains the word \bFAILED\b; no other diagnostics contain this word.]
[k:keep?Do not remove the temporary directory \aunit\a\b.tmp\b on exit.]
[q:quiet?Output information on \bFAILED\b tests only.]
[t:test?Run only tests matching \apattern\a. Tests are numbered and consist of
	at least two digits (0 filled if necessary).]:[pattern]
[v:verbose?List differences between actual (<) and expected (>) output, errors
	and exit codes. Also disable long output line truncation.]
[D:debug?Enable debug tracing.]

unit [ command [ arg ... ] ]

[+INPUT FILES?The regression test file \aunit\a\b.tst\b is a \bsh\b(1) script
	that is executed in an environment with the following functions
	defined:]{
	[+BODY { ... }?Defines the test body; used for complex tests.]
	[+CLEANUP \astatus\a?Called at exit time to remove the temporary
		directory \aunit\a\b.tmp\b, list the tests totals via
		\bTALLY\b, and exit with status \astatus\a.]
	[+COMMAND \aarg\a ...?Runs the current command under test with
		\aarg\a ... appended to the default args.]
	[+COPY \afrom to\a?Copy file \afrom\a to \ato\a. \afrom\a may be
		a regular file or \bINPUT\b, \bOUTPUT\b or \bERROR\b.
		Post test comparisons are still done for \afrom\a.]
	[+DIAGNOSTICS [ \b1\b | \"\" ]]?No argument or an argument of \b1\b
		declares that diagnostics are to expected for the remainder
		of the current \bTEST\b; \"\" reverts to the default state
		that diagnostics are not expected.]
	[+DO \astatement\a?Defines additional statements to be executed
		for the current test. \astatement\a may be a { ... } group.]
	[+EMPTY \bINPUT|OUTPUT|ERROR|SAME?The corresponding file is expected
		to be empty.]
	[+ERROR [ \b-n\b ]] \afile\a | - \adata\a ...?The standard error is
		expected to match either the contents of \afile\a or the line
		\adata\a. \bERROR -n\b does not append a newline to \adata\a.]
	[+EXEC [ \aarg\a ... ]]?Runs the command under test with optional
		arguments. \bINPUT\b, \bOUTPUT\b, \bERROR\b, \bEXIT\b and
		\bSAME\b calls following this \bEXEC\b up until the next
		\bEXEC\b or the end of the script provide details for the
		expected results.]
	[+EXIT \astatus\a?The command exit status is expected to match the
		pattern \astatus\a.]
	[+EXPORT \aname\a=\avalue\a ...?Export environment variables for one
		test.]
	[+FATAL \amessage\a ...?\amessage\a is printed on the standard error
		and \bregress\b exits with status \b1\b.]
	[+IGNORE \afile\a ...?\afile\a is ignored for subsequent result
		comparisons. \afile\a may be \bOUTPUT\b or \bERROR\b.]
	[+INFO \adescription\a?\adescription\a is printed on the standard
		error.]
	[+INITIALIZE?Called by \bregress\b to initialize a each \bTEST\b
		group.]
	[+INPUT [ \b-n\b ]] \afile\a | - \adata\a ...?The standard input is
		set to either the contents of \afile\a or the line
		\adata\a. \bINPUT -n\b does not append a newline to \adata\a.]
	[+INTRO?Called by \bregress\b to introduce all \bTEST\b groups.]
	[+IO \bINPUT|OUTPUT|ERROR\b [ \b-n\b ]] \afile\a | - \adata\a ...?
		Internal support for the \bINPUT\b, \bOUTPUT\b and \bERROR\b
		functions.]
	[+KEEP \apattern\a ...?The temporary directory is cleared for each
		test. Files matching \apattern\a are retained between tests.]
	[+MOVE \afrom to\a?Rename file \afrom\a to \ato\a. \afrom\a may be
		a regular file or \bINPUT\b, \bOUTPUT\b or \bERROR\b.
		Post test comparisons are ignored for \afrom\a.]
	[+NOTE \acomment\a?\acomment\a is added to the current test trace
		output.]
	[+OUTPUT [ \b-n\b ]] \afile\a | - \adata\a ...?The standard output is
		expected to match either the contents of \afile\a or the line
		\adata\a. \bOUTPUT -n\b does not append a newline to \adata\a.]
	[+PROG \acommand\a [ \aarg\a ... ]]?\acommand\a is run with optional
		arguments.]
	[+REMOVE \afile\a ...?\afile\a ... are removed after the current test
		is done.]
	[+RUN?Called by \bregress\b to run the current test.]
	[+SAME \anew old\a?\anew\a is expected to be the same as \aold\a after
		the current test completes.]
	[+TALLY?Called by \bregress\b display the \bTEST\b results.]
	[+TEST \anumber\a [ \adescription\a ... ]]?Define a new test group
		labelled \anumber\a with option \adescripion\a.]
	[+UNIT \acommand\a [ \aarg\a ... ]]?Define the command and optional
		default arguments to be tested. \bUNIT\b explicitly overrides
		the default command name derived from the test script
		file name.]
	[+VIEW \avar\a [ \afile\a ]]?\avar\a is set to the full pathname of
		\avar\a [ \afile\a ]] in the current \b$VPATH\b view if
		defined.]
}
[+SEE ALSO?\bnmake\b(1), \bsh\b(1)]
'
	;;
*)	USAGE='ko:[[no]name[=value]]t:[test]v unit [path [arg ...]]'
	;;
esac

function FATAL # message
{
	print -r -u2 "$command: $*"
	GROUP=FINI
	exit 1
}

function EMPTY
{
	typeset i
	typeset -n ARRAY=$1
	for i in ${!ARRAY[@]}
	do	unset ARRAY[$i]
	done
}

function INITIALIZE # void
{
	typeset i j
	cd "$TMP"
	case $KEEP in
	"")	rm $rmflags *
		;;
	*)	for i in *
		do	case $i in
			!($KEEP))	j="$j $i" ;;
			esac
		done
		case $j in
		?*)	rm $rmflags $j ;;
		esac
		;;
	esac
	: >INPUT >OUTPUT.ex >ERROR.ex
	BODY=""
	COPY=""
	DIAGNOSTICS=""
	DONE=""
	ERROR=""
	EXIT=0
	IGNORE=""
	INIT=""
	INPUT=""
	MOVE=""
	OUTPUT=""
	EMPTY SAME
}

function INTRO
{
	case $quiet in
	"")	print -u2 "TEST	$COMMAND" ;;
	esac
}

function TALLY
{
	typeset msg
	case $GROUP in
	INIT)	;;
	*)	msg="TEST	$COMMAND, $TESTS test"
		case $TESTS in
		1)	;;
		*)	msg=${msg}s ;;
		esac
		msg="$msg, $ERRORS error"
		case $ERRORS in
		1)	;;
		*)	msg=${msg}s ;;
		esac
		print -u2 "$msg"
		GROUP=INIT
		TESTS=0
		ERRORS=0
		;;
	esac
}

function CLEANUP # status
{
	case $dump in
	"")	cd $SOURCE
		rm $rmflags "$TMP"
		;;
	esac
	TALLY
	exit $1
}

function RUN # void
{
	typeset failed i j s
	typeset $truncate SHOW
	case $GROUP in
	INIT)	if	test "" != "$UNIT"
		then	set -- "${ARGV[@]}"
			case $1 in
			""|[-+]*)
				UNIT $UNIT "${ARGV[@]}"
				;;
			*)	UNIT "${ARGV[@]}"
				;;
			esac
		fi
		INTRO
		;;
	FINI)	;;
	$select)((COUNT++))
		if	(( $ITEM <= $LASTITEM ))
		then	LABEL=$TEST#$COUNT
		else	LASTITEM=$ITEM
			LABEL=$TEST:$ITEM
		fi
		case $quiet in
		"")	print -nu2 "$LABEL" ;;
		esac
		file=""
		exec >/dev/null
		#DEBUG#PS4='+$LINENO+ '; set -x
		for i in $INPUT
		do	case " $OUTPUT " in
			*" $i "*)
				if	test -f $i.sav
				then	cp $i.sav $i
					compare="$compare $i"
				elif	test -f $i
				then	cp $i $i.sav
					compare="$compare $i"
				fi
				;;
			esac
		done
		for i in $OUTPUT
		do	case " $compare " in
			*" $i "*)
				;;
			*)	compare="$compare $i"
				;;
			esac
		done
		for i in $INIT
		do	$i $TEST INIT
		done
		case $BODY in
		"")	COMMAND "${ARGS[@]}" <INPUT >OUTPUT 2>ERROR
			failed=""
			ignore=""
			set -- $COPY
			COPY=""
			while	:
			do	case $# in
				0|1)	break ;;
				*)	cp $1 $2 ;;
				esac
				shift 2
			done
			set -- $MOVE
			MOVE=""
			while	:
			do	case $# in
				0|1)	break ;;
				*)	mv $1 $2; ignore="$ignore $1" ;;
				esac
				shift 2
			done
			for i in $compare OUTPUT ERROR
			do	case " $IGNORE $ignore " in
				*" $i "*)	continue ;;
				esac
				ignore="$ignore $i"
				case ${SAME[$i]} in
				"")	for s in ex sav err
					do	test -f $i.$s && break
					done
					j=$i.$s
					;;
				*)	j=${SAME[$i]}
					;;
				esac
				case $DIAGNOSTICS:$i in
				?*:ERROR)
					case $STATUS in
					0)	test ! -s ERROR && failed=$failed${failed:+,}DIAGNOSTICS ;;
					esac
					continue
					;;
				*)	cmp -s $i $j && continue
					;;
				esac
				failed=$failed${failed:+,}$i
				case $verbose in
				?*)	print -u2 "	=== diff $i <actual >expected ==="
					diff $i $j >&2
					;;
				esac
			done
			case $failed in
			"")	case $STATUS in
				$EXIT)	;;
				*)	failed="exit code $EXIT expected -- got $STATUS" ;;
				esac
				;;
			esac
			case $failed in
			"")	SHOW=$NOTE
				case $quiet in
				"")	print -r -u2 "	$SHOW" ;;
				esac
				;;
			?*)	((ERRORS++))
				case $quiet in
				?*)	print -nu2 "$LABEL" ;;
				esac
				SHOW="FAILED [ $failed ] $NOTE"
				print -r -u2 "	$SHOW"
				case $dump in
				?*)	GROUP=FINI; exit ;;
				esac
				;;
			esac
			;;
		*)	SHOW=$NOTE
			case $quiet in
			"")	print -r -u2 "	$SHOW" ;;
			esac
			for i in $BODY
			do	$i $TEST BODY
			done
			;;
		esac
		for i in $DONE
		do	$i $TEST DONE $STATUS
		done
		compare=""
		#DEBUG#set +x
		;;
	esac
	if	test "" != "$COMMAND_ORIG"
	then	COMMAND=$COMMAND_ORIG
		COMMAND_ORIG=
		ARGS=(${ARGS_ORIG[@]})
	fi
}

function UNIT # cmd arg ...
{
	typeset cmd=$1
	case $cmd in
	-)	shift
		#BUG# ARGV=("${ARGV[@]}" "$@")
		set -- "${ARGV[@]}" "$@"
		ARGV=("$@")
		return
		;;
	esac
	if	test "" != "$UNIT"
	then	set -- "${ARGV[@]}"
		case $1 in
		"")	set -- "$cmd" ;;
		[-+]*)	set -- "$cmd" "${ARGV[@]}" ;;
		*)	set -- "${ARGV[@]}" ;;
		esac
		UNIT=
	fi
	COMMAND=$1
	shift
	typeset cmd=$(PATH=$SOURCE:$PATH:/usr/5bin:/bin:/usr/bin whence $COMMAND)
	if	test "" = "$cmd"
	then	FATAL $COMMAND: not found
	elif	test ! -x "$cmd"
	then	FATAL $cmd: not found
	fi
	COMMAND=$cmd
	case $# in
	0)	;;
	*)	COMMAND="$COMMAND $*" ;;
	esac
}

function TEST # number description arg ...
{
	RUN
	COUNT=0
	LASTITEM=0
	case $1 in
	-)		((LAST++)); TEST=$LAST ;;
	+([0123456789]))	LAST=$1 TEST=$1 ;;
	*)		LAST=0${1/[!0123456789]/} TEST=$1 ;;
	esac
	NOTE=
	case $TEST in
	$select)
		case $quiet in
		"")	print -r -u2 "$TEST	$2" ;;
		esac
		;;
	esac
	unset ARGS
	unset EXPORT
	EXPORTS=0
	file=""
	case $TEST in
	${GROUP}*)
		;;
	*)	GROUP=${TEST%%+([abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ])}
		case $GROUP in
		$select)INITIALIZE ;;
		esac
		;;
	esac
	((SUBTESTS=0))
}

function EXEC # arg ...
{
	case $GROUP in
	!($select))	return ;;
	esac
	if	((SUBTESTS++))
	then	RUN
	fi
	case $# in
	0)	set -- "${ARGS[@]}" ;;
	esac
	ITEM=$LINE
	NOTE="$(print -r -f '%q ' -- $COMMAND_ORIG "$@")"
	ARGS=("$@")
}

function EXPORT
{
	typeset x
	case $GROUP in
	!($select))	return ;;
	esac
	for x
	do	EXPORT[EXPORTS++]=$x
	done
}

function FLUSH
{
	case $GROUP in
	!($select))	return ;;
	esac
	if	((SUBTESTS++))
	then	RUN
	fi
}

function PROG # cmd arg ...
{
	typeset command args
	case $GROUP in
	!($select))	return ;;
	esac
	ITEM=$LINE
	NOTE="$(print -r -f '%q ' -- "$@")"
	COMMAND_ORIG=$COMMAND
	COMMAND=$1
	shift
	ARGS_ORIG=(${ARGS[@]})
	ARGS=("$@")
}

function NOTE # description
{
	NOTE=$*
}

function IO # INPUT|OUTPUT|ERROR [-n] file|- data ...
{
	typeset op i v f file
	case $GROUP in
	!($select))	return ;;
	esac
	op=$1
	shift
	file=$op
	case $1 in
	-n)	f=$1; shift ;;
	esac
	case $# in
	0)	;;
	*)	case $1 in
		-)	;;
		*)	file=$1
			eval i='$'$op
			case " $i " in
			*" $file "*)
				;;
			*)	eval $op='"$'$op' $file"'
				;;
			esac
			;;
		esac
		shift
		;;
	esac
	case " $IGNORE " in
	*" $file "*)
		for i in $IGNORE
		do	case $i in
			$file)	;;
			*)	v="$v $i" ;;
			esac
		done
		IGNORE=$v
		;;
	esac
	case $op in
	OUTPUT|ERROR)	file=$file.ex ;;
	esac
	#unset SAME[$op]
	SAME[$op]=
	rm $rmflags $file.sav
	case $#:$f in
	0:)	: > $file ;;
	*)	print $f -r -- "$@" > $file ;;
	esac
}

function INPUT # file|- data ...
{
	IO $0 "$@"
}

function COPY # from to
{
	case $GROUP in
	!($select))	return ;;
	esac
	COPY="$COPY $@"
}

function MOVE # from to
{
	case $GROUP in
	!($select))	return ;;
	esac
	MOVE="$MOVE $@"
}

function SAME # new old
{
	typeset i file v
	case $GROUP in
	!($select))	return ;;
	esac
	case $# in
	2)	case $1 in
		INPUT)	cat $2 > $1; return ;;
		esac
		SAME[$1]=$2
		file=$1
		compare="$compare $1"
		;;
	3)	SAME[$2]=$3
		file=$2
		eval i='$'$1
		case " $i " in
		*" $2 "*)
			;;
		*)	eval $1='"$'$1' $2"'
			;;
		esac
		compare="$compare $2"
		;;
	esac
	case " $IGNORE " in
	*" $file "*)
		for i in $IGNORE
		do	case $i in
			$file)	;;
			*)	v="$v $i" ;;
			esac
		done
		IGNORE=$v
		;;
	esac
}

function OUTPUT # file|- data ...
{
	IO $0 "$@"
}

function ERROR # file|- data ...
{
	IO $0 "$@"
}

function REMOVE # file ...
{
	typeset i
	for i
	do	rm $rmflags $i $i.sav
	done
}

function IGNORE # file ...
{
	typeset i
	for i
	do	case " $IGNORE " in
		*" $i "*)
			;;
		*)	IGNORE="$IGNORE $i"
			;;
		esac
	done
}

function KEEP # pattern ...
{
	typeset i
	for i
	do	case $KEEP in
		"")	KEEP="$i" ;;
		*)	KEEP="$KEEP|$i" ;;
		esac
	done
}

function DIAGNOSTICS # [ 1 | "" ]
{
	DIAGNOSTICS=${1:-1}
	EXIT='*'
}

function EXIT # status
{
	EXIT=$1
}

function INFO # info description
{
	typeset -R15 info=$1
	case $1 in
	"")	info=no ;;
	esac
	shift
	case $quiet in
	"")	print -r -u2 "$info " "$@" ;;
	esac
}

function COMMAND # arg ...
{
	((TESTS++))
	case $dump in
	?*)	(
		PS4=''
		set -x
		print -r -- "${EXPORT[@]}" $COMMAND "$@"
		) 2>&1 >/dev/null |
		sed 's,^print -r -- ,,' >COMMAND
		chmod +x COMMAND
		;; 
	esac
	eval "${EXPORT[@]}" '$'COMMAND '"$@"'
	STATUS=$?
	return $STATUS
}

function SET # [no]name[=value]
{
	typeset i
	for i
	do	case $i in
		no?*)	eval ${i#no}='""' ;;
		*=0)	eval ${i%0}='""' ;;
		*=*)	eval $i ;;
		*)	eval $i=1 ;;
		esac
	done
}

function VIEW # var [ file ]
{
	nameref var=$1
	typeset i bwd file pwd view root offset
	case $# in
	1)	file=$1 ;;
	*)	file=$2 ;;
	esac
	pwd=${PWD%/*}
	bwd=$(/bin/pwd)
	bwd=${bwd%/*}
	case $var in
	'')	var=$pwd/$file
		if	test -r $file
		then	if	test ! -d $file
			then	return
			fi
			for i in $file/*
			do	if	test -r $i
				then	return
				fi
				break
			done
		fi
		ifs=$IFS
		IFS=:
		set -- $VPATH
		IFS=$ifs
		for view
		do	case $view in
			/*)	;;
			*)	view=$pwd/$view ;;
			esac
			case $offset in
			'')	case $pwd in
				$view/*)	offset=${pwd#$view} ;;
				*)		offset=${bwd#$view} ;;
				esac
				;;
			esac
			if	test -r $view$offset/$file
			then	if	test ! -d $view$offset/$file
				then	var=$view$offset/$file
					break
				fi
				for i in $view$offset/$file/*
				do	if	test -f $i
					then	var=$view$offset/$file
						break
					fi
					break
				done
			fi
		done
		;;
	esac
}

# main

integer ERRORS=0 EXPORTS=0 TESTS=0 SUBTESTS=0 LINE=0 ITEM=0 LASTITEM=0 COUNT
typeset ARGS COMMAND COPY DIAGNOSTICS ERROR EXEC GROUP=INIT
typeset IGNORE INPUT KEEP OUTPUT TEST SOURCE MOVE NOTE
typeset ARGS_ORIG COMMAND_ORIG UNIT ARGV
typeset dump file quiet rmflags='-rfu --' select trace verbose truncate=-L70
typeset -A EXPORT SAME
typeset -Z LAST=00

unset FIGNORE

while	getopts -a $command "$USAGE" OPT
do	case $OPT in
	k)	SET dump=1
		;;
	q)	SET quiet=1
		;;
	t)	case $select in
		"")	select="${OPTARG//,/\|}" ;;
		*)	select="$select|${OPTARG//,/\|}" ;;
		esac
		;;
	v)	SET verbose=1
		truncate=
		;;
	D)	SET trace=1
		;;
	*)	GROUP=FINI
		exit 2
		;;
	esac
done
shift $OPTIND-1
case $# in
0)	FATAL test unit name omitted ;;
esac
export COLUMNS=80
SOURCE=$PWD
PATH=$SOURCE:${PATH#:}
UNIT=$1
shift
if	test -f $UNIT -a ! -x $UNIT
then	REGRESS=$UNIT
else	REGRESS=${UNIT%.tst}
	REGRESS=$REGRESS.tst
	test -f $REGRESS || FATAL $REGRESS: regression tests not found
fi
UNIT=${UNIT##*/}
UNIT=${UNIT%.tst}
TMP=$UNIT.tmp
ARGV=("$@")
case $select in
"")	select="[0123456789]*" ;;
*'|'*)	select="@($select)" ;;
esac

# all work done in local temp dir

trap "RUN; CLEANUP 0" 0
trap "CLEANUP $?" 1 2 13 15
rm $rmflags "$TMP"
mkdir "$TMP" || FATAL "$TMP": cannot create directory
cd "$TMP"
TMP=$PWD
case $trace in
?*)	PS4='+$LINENO+ '
	set -x
	;;
esac

# some last minute shenanigans

alias BODY='BODY=BODY; function BODY'
alias DO='[[ $GROUP == $select ]] &&'
alias DONE='DONE=DONE; function DONE'
alias EXEC='LINE=$LINENO; EXEC'
alias INIT='INIT=INIT; function INIT'
alias PROG='LINE=$LINENO; FLUSH; PROG'

# do the tests

. $REGRESS
