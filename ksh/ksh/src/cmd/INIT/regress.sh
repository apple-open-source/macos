########################################################################
#                                                                      #
#               This software is part of the ast package               #
#                     Copyright (c) 1994-2007 AT&T                     #
#                      and is licensed under the                       #
#                  Common Public License, Version 1.0                  #
#                               by AT&T                                #
#                                                                      #
#                A copy of the License is available at                 #
#            http://www.opensource.org/licenses/cpl1.0.txt             #
#         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         #
#                                                                      #
#              Information and Software Systems Research               #
#                            AT&T Research                             #
#                           Florham Park NJ                            #
#                                                                      #
#                 Glenn Fowler <gsf@research.att.com>                  #
#                                                                      #
########################################################################
: regress - run regression tests in command.tst

command=regress
case $(getopts '[-][123:xyz]' opt --xyz 2>/dev/null; echo 0$opt) in
0123)	USAGE=$'
[-?
@(#)$Id: regress (AT&T Research) 2007-05-08 $
]
'$USAGE_LICENSE$'
[+NAME?regress - run regression tests]
[+DESCRIPTION?\bregress\b runs the tests in \aunit\a, or
    \aunit\a\b.tst\b if \aunit\a does not exist. If \acommand\a is omitted
    then it is assumed to be the base name of \aunit\a. All testing is done
    in the temporary directory \aunit\a\b.tmp\b.]
[+?Default test output lists the \anumber\a and \adescription\a for
    each active \bTEST\b group and the \anumber\a:\aline\a for each
    individual \bEXEC\b test. Each test that fails results in a diagnostic
    that contains the word \bFAILED\b; no other diagnostics contain this
    word.]
[b:ignore-space?Ignore space differences when comparing expected
    output.]
[i:pipe-input?Repeat each test with the standard input redirected through a
    pipe.]
[k:keep?Enable \bcore\b dumps, exit after the first test that fails,
    and do not remove the temporary directory \aunit\a\b.tmp\b.]
[o:pipe-output?Repeat each test with the standard output redirected through
    a pipe.]
[p:pipe-io?Repeat each test with the standard input and standard output
    redirected through pipes.]
[q:quiet?Output information on \bFAILED\b tests only.]
[r!:regular?Run each test with the standard input and standard output
    redirected through regular files.]
[t:test?Run only tests matching \apattern\a. Tests are numbered and
    consist of at least two digits (0 filled if necessary.) Tests matching
    \b+(0)\b are always run.]:[pattern]
[v:verbose?List differences between actual (<) and expected (>) output,
    errors and exit codes. Also disable long output line truncation.]
[D:debug?Enable debug tracing.]

unit [ command [ arg ... ] ]

[+INPUT FILES?The regression test file \aunit\a\b.tst\b is a \bksh\b(1)
    script that is executed in an environment with the following functions
    defined:]
    {
        [+BODY { ... }?Defines the test body; used for complex tests.]
        [+CD \adirectory\a?Create and change to working directory for
            one test.]
        [+CLEANUP \astatus\a?Called at exit time to remove the
            temporary directory \aunit\a\b.tmp\b, list the tests totals via
            \bTALLY\b, and exit with status \astatus\a.]
        [+COMMAND \aarg\a ...?Runs the current command under test with
            \aarg\a ... appended to the default args.]
        [+COPY \afrom to\a?Copy file \afrom\a to \ato\a. \afrom\a may
            be a regular file or \bINPUT\b, \bOUTPUT\b or \bERROR\b. Post
            test comparisons are still done for \afrom\a.]
        [+DIAGNOSTICS [ \b1\b | \"\" ]]?No argument or an argument of
            \b1\b declares that diagnostics are to expected for the
            remainder of the current \bTEST\b; \"\" reverts to the default
            state that diagnostics are not expected.]
        [+DO \astatement\a?Defines additional statements to be executed
            for the current test. \astatement\a may be a { ... } group.]
        [+EMPTY \bINPUT|OUTPUT|ERROR|SAME?The corresponding file is
            expected to be empty.]
        [+ERROR [ \b-n\b ]] \afile\a | - \adata\a ...?The standard
            error is expected to match either the contents of \afile\a or
            the line \adata\a. \bERROR -n\b does not append a newline to
            \adata\a.]
        [+EXEC [ \aarg\a ... ]]?Runs the command under test with
            optional arguments. \bINPUT\b, \bOUTPUT\b, \bERROR\b, \bEXIT\b
            and \bSAME\b calls following this \bEXEC\b up until the next
            \bEXEC\b or the end of the script provide details for the
            expected results.  If no arguments are specified then the
	    arguments from the previious \bEXEC\b in the current
	    \bTEST\b group are used, or no arguments if this is the
	    first \bEXEC\b in the group.]
        [+EXIT \astatus\a?The command exit status is expected to match
            the pattern \astatus\a.]
        [+EXPORT [-]] \aname\a=\avalue\a ...?Export environment
            variables for one test.]
        [+FATAL \amessage\a ...?\amessage\a is printed on the standard
            error and \bregress\b exits with status \b1\b.]
        [+IGNORE \afile\a ...?\afile\a is ignored for subsequent result
            comparisons. \afile\a may be \bOUTPUT\b or \bERROR\b.]
        [+IGNORESPACE?Ignore space differences when comparing expected
            output.]
        [+INCLUDE \afile\a ...?One or more \afile\a operands are read
            via the \bksh\b(1) \b.\b(1) command. \bVIEW\b is used to locate
            the files.]
        [+INFO \adescription\a?\adescription\a is printed on the
            standard error.]
        [+INITIALIZE?Called by \bregress\b to initialize a each
            \bTEST\b group.]
        [+INPUT [ \b-n\b ]] \afile\a | - \adata\a ...?The standard
            input is set to either the contents of \afile\a or the line
            \adata\a. \bINPUT -n\b does not append a newline to \adata\a.]
        [+INTRO?Called by \bregress\b to introduce all \bTEST\b
            groups.]
        [+IO \bINPUT|OUTPUT|ERROR\b [ \b-n\b ]] \afile\a | - \adata\a ...?
            Internal support for the \bINPUT\b, \bOUTPUT\b and \bERROR\b
            functions.]
        [+KEEP \apattern\a ...?The temporary directory is cleared for
            each test. Files matching \apattern\a are retained between
            tests.]
        [+MOVE \afrom to\a?Rename file \afrom\a to \ato\a. \afrom\a may
            be a regular file or \bINPUT\b, \bOUTPUT\b or \bERROR\b. Post
            test comparisons are ignored for \afrom\a.]
        [+NOTE \acomment\a?\acomment\a is added to the current test
            trace output.]
        [+OUTPUT [ \b-n\b ]] \afile\a | - \adata\a ...?The standard
            output is expected to match either the contents of \afile\a or
            the line \adata\a. \bOUTPUT -n\b does not append a newline to
            \adata\a.]
        [+PROG \acommand\a [ \aarg\a ... ]]?\acommand\a is run with
            optional arguments.]
        [+REMOVE \afile\a ...?\afile\a ... are removed after the
            current test is done.]
        [+RUN?Called by \bregress\b to run the current test.]
        [+SAME \anew old\a?\anew\a is expected to be the same as
            \aold\a after the current test completes.]
	[+SET [\bno\b]]\aname\a[=\avalue\a]]?Set the command line
	    option --\aname\a.  The setting is in effect for all
	    tests until the next explicit \bSET\b.]
        [+TALLY?Called by \bregress\b display the \bTEST\b results.]
        [+TEST \anumber\a [ \adescription\a ... ]]?Define a new test
            group labelled \anumber\a with optional \adescripion\a.]
        [+TITLE [+]] \atext\a?Set the \bTEST\b output title to
            \atext\a. If \b+\b is specified then \atext\a is appended to
            the default title. The default title is the test file base
            name, and, if different from the test file base name, the test
            unit base name.]
        [+TWD [ \adir\a ... ]]?Set the temporary test dir to \adir\a.
            The default is \aunit\a\b.tmp\b, where \aunit\a is the test
            input file sans directory and suffix. If \adir\a matches \b/*\b
            then it is the directory name; if \adir\a is non-null then the
            prefix \b${TMPDIR:-/tmp}\b is added; otherwise if \adir\a is
            omitted then
            \b${TMPDIR:-/tmp}/tst-\b\aunit\a-$$-$RANDOM.\b\aunit\a is
            used.]
        [+UMASK [ \amask\a ]]?Run subsequent tests with \bumask\b(1)
            \amask\a. If \amask\a is omitted then the original \bumask\b is
            used.]
        [+UNIT \acommand\a [ \aarg\a ... ]]?Define the command and
            optional default arguments to be tested. \bUNIT\b explicitly
            overrides the default command name derived from the test script
            file name.]
        [+VIEW \avar\a [ \afile\a ]]?\avar\a is set to the full
            pathname of \avar\a [ \afile\a ]] in the current \b$VPATH\b
            view if defined.]
    }
[+SEE ALSO?\bnmake\b(1), \bksh\b(1)]
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
	cd "$TWD"
	case $KEEP in
	"")	RM *
		;;
	*)	for i in *
		do	case $i in
			!($KEEP))	j="$j $i" ;;
			esac
		done
		case $j in
		?*)	RM $j ;;
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
	EMPTY PIPE
	EMPTY SAME
}

function INTRO
{
	typeset base command

	if	[[ ! $TEST_quiet ]]
	then	base=${REGRESS##*/}
		base=${base%.tst}
		command=${COMMAND##*/}
		command=${command%' '*}
		set -- $TITLE
		TITLE=
		case $1 in
		''|+)	if	[[ $command == $base ]]
			then	TITLE=$COMMAND
			else	TITLE="$COMMAND, $base"
			fi
			if	(( $# ))
			then	shift
			fi
			;;
		esac
		while	(( $# ))
		do	if	[[ $TITLE ]]
			then	TITLE="$TITLE, $1"
			else	TITLE="$1"
			fi
			shift
		done
		print -u2 "TEST	$TITLE"
	fi
}

function TALLY
{
	typeset msg
	case $GROUP in
	INIT)	;;
	*)	msg="TEST	$TITLE, $TESTS test"
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

function TITLE # text
{
	TITLE=$@
}

function CLEANUP # status
{
	if	[[ ! $TEST_keep && $GROUP!=INIT ]]
	then	cd $SOURCE
		RM "$TWD"
	fi
	TALLY
	exit $1
}

function RUN # [ op ]
{
	typeset i
	[[ $UMASK != $UMASK_ORIG ]] && umask $UMASK_ORIG
	case $GROUP in
	INIT)	RM "$TWD"
		mkdir "$TWD" || FATAL "$TWD": cannot create directory
		cd "$TWD"
		TWD=$PWD
		: > rmu
		if	rm -u rmu >/dev/null 2>&1
		then	TEST_rmu=-u
		else	rm rmu
		fi
		if	[[ $UNIT ]]
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
	$TEST_select)
		if	[[ $ITEM == $FLUSHED ]]
		then	return
		fi
		FLUSHED=$ITEM
		((COUNT++))
		if	(( $ITEM <= $LASTITEM ))
		then	LABEL=$TEST#$COUNT
		else	LASTITEM=$ITEM
			LABEL=$TEST:$ITEM
		fi
		TEST_file=""
		exec >/dev/null
		for i in $INPUT
		do	case " $OUTPUT " in
			*" $i "*)
				if	[[ -f $i.sav ]]
				then	cp $i.sav $i
					COMPARE="$COMPARE $i"
				elif	[[ -f $i ]]
				then	cp $i $i.sav
					COMPARE="$COMPARE $i"
				fi
				;;
			esac
		done
		for i in $OUTPUT
		do	case " $COMPARE " in
			*" $i "*)
				;;
			*)	COMPARE="$COMPARE $i"
				;;
			esac
		done
		for i in $INIT
		do	$i $TEST INIT
		done
		if	[[ $BODY ]]
		then	SHOW=$NOTE
			if	[[ ! $TEST_quiet ]]
			then	print -r -u2 "	$SHOW"
			fi
			for i in $BODY
			do	$i $TEST BODY
			done
		else	SHOW=
			if	[[ ${PIPE[INPUT]} ]]
			then	if	[[ ${PIPE[OUTPUT]} ]]
				then	if	[[ ! $TEST_quiet ]]
					then	print -nu2 "$LABEL"
					fi
					cat <$TWD/INPUT | COMMAND "${ARGS[@]}" 2>$TWD/ERROR | cat >$TWD/OUTPUT
					RESULTS 'pipe input'
				else	if	[[ ! $TEST_quiet ]]
					then	print -nu2 "$LABEL"
					fi
					cat <$TWD/INPUT | COMMAND "${ARGS[@]}" >$TWD/OUTPUT 2>$TWD/ERROR
					RESULTS 'pipe io'
				fi
			elif	[[ ${PIPE[OUTPUT]} ]]
			then	if	[[ ! $TEST_quiet ]]
				then	print -nu2 "$LABEL"
				fi
				COMMAND "${ARGS[@]}" <$TWD/INPUT 2>$TWD/ERROR | cat >$TWD/OUTPUT
				RESULTS 'pipe output'
			else	if	[[ $TEST_regular ]]
				then	if	[[ ! $TEST_quiet ]]
					then	print -nu2 "$LABEL"
					fi
					COMMAND "${ARGS[@]}" <$TWD/INPUT >$TWD/OUTPUT 2>$TWD/ERROR
					RESULTS
				fi
				if	[[ $TEST_pipe_input ]]
				then	if	[[ ! $TEST_quiet ]]
					then	print -nu2 "$LABEL"
					fi
					cat <$TWD/INPUT | COMMAND "${ARGS[@]}" >$TWD/OUTPUT 2>$TWD/ERROR
					RESULTS 'pipe input'
				fi
				if	[[ $TEST_pipe_output ]]
				then	if	[[ ! $TEST_quiet ]]
					then	print -nu2 "$LABEL"
					fi
					COMMAND "${ARGS[@]}" <$TWD/INPUT 2>$TWD/ERROR | cat >$TWD/OUTPUT
					RESULTS 'pipe output'
				fi
				if	[[ $TEST_pipe_io ]]
				then	if	[[ ! $TEST_quiet ]]
					then	print -nu2 "$LABEL"
					fi
					cat <$TWD/INPUT | COMMAND "${ARGS[@]}" 2>$TWD/ERROR | cat >$TWD/OUTPUT
					RESULTS 'pipe io'
				fi
			fi
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
			while	(( $# > 1 ))
			do	mv $1 $2
				shift 2
			done
		fi
		for i in $DONE
		do	$i $TEST DONE $STATUS
		done
		COMPARE=""
		;;
	esac
	if	[[ $COMMAND_ORIG ]]
	then	COMMAND=$COMMAND_ORIG
		COMMAND_ORIG=
		ARGS=(${ARGS_ORIG[@]})
	fi
}

function DO # cmd ...
{
	[[ $GROUP == $TEST_select ]] || return 1
	[[ $UMASK != $UMASK_ORIG ]] && umask $UMASK
	return 0
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
	if	[[ $UNIT ]]
	then	set -- "${ARGV[@]}"
		case $1 in
		"")	set -- "$cmd" ;;
		[-+]*)	set -- "$cmd" "${ARGV[@]}" ;;
		esac
		UNIT=
	fi
	COMMAND=$1
	shift
	typeset cmd=$(whence $COMMAND)
	if	[[ ! $cmd ]]
	then	FATAL $COMMAND: not found
	elif	[[ ! $cmd ]]
	then	FATAL $cmd: not found
	fi
	case $# in
	0)	;;
	*)	COMMAND="$COMMAND $*" ;;
	esac
}

function TWD # [ dir ]
{
	case $1 in
	'')	TWD=${TMPDIR:-/tmp}/tst-${TWD%.*}-$$-$RANDOM ;;
	/*)	TWD=$1 ;;
	*)	TWD=${TMPDIR:-/tmp}/$1 ;;
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
	if	[[ ! $TEST_quiet && $TEST == $TEST_select ]]
	then	print -r -u2 "$TEST	$2"
	fi
	unset ARGS
	unset EXPORT
	EXPORTS=0
	TEST_file=""
	if	[[ $TEST != ${GROUP}* ]]
	then	GROUP=${TEST%%+([abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ])}
		if	[[ $GROUP == $TEST_select ]]
		then	INITIALIZE
		fi
	fi
	((SUBTESTS=0))
	[[ $TEST == $TEST_select ]]
}

function EXEC # arg ...
{
	if	[[ $GROUP != $TEST_select ]]
	then	return
	fi
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

function CD
{
	RUN
	if	[[ $GROUP == $TEST_select ]]
	then	mkdir -p "$@" && cd "$@" || FATAL cannot initialize working directory "$@"
	fi
}

function EXPORT
{
	typeset x
	RUN
	if	[[ $GROUP != $TEST_select ]]
	then	return
	fi
	for x
	do	EXPORT[EXPORTS++]=$x
	done
}

function FLUSH
{
	if	[[ $GROUP != $TEST_select ]]
	then	return
	fi
	if	((SUBTESTS++))
	then	RUN
	fi
}

function PROG # cmd arg ...
{
	typeset command args
	if	[[ $GROUP != $TEST_select ]]
	then	return
	fi
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

function IO # [ PIPE ] INPUT|OUTPUT|ERROR [-f*|-n] file|- data ...
{
	typeset op i v f file pipe x
	if	[[ $GROUP != $TEST_select ]]
	then	return
	fi
	[[ $UMASK != $UMASK_ORIG ]] && umask $UMASK_ORIG
	if	[[ $1 == PIPE ]]
	then	pipe=1
		shift
	fi
	op=$1
	shift
	PIPE[$op]=$pipe
	file=$TWD/$op
	case $1 in
	-x)	x=1; shift ;;
	esac
	case $1 in
	-f*|-n)	f=$1; shift ;;
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
	OUTPUT|ERROR)
		file=$file.ex
		if	[[ $file != /* ]]
		then	file=$TWD/$file
		fi
		;;
	esac
	#unset SAME[$op]
	SAME[$op]=
	RM $TWD/$file.sav
	if	[[ $file == */* ]]
	then	mkdir -p ${file%/*}
	fi
	if	[[ $file != */ ]]
	then	case $#:$f in
		0:)	: > $file ;;
		*:-f)	printf -- "$@" > $file ;;
		*:-f*)	printf -- "${f#-f}""$@" > $file ;;
		*)	print $f -r -- "$@" > $file ;;
		esac
		if	[[ $x ]]
		then	chmod +x $file
		fi
	fi
}

function INPUT # file|- data ...
{
	IO $0 "$@"
}

function COPY # from to
{
	if	[[ $GROUP != $TEST_select ]]
	then	return
	fi
	COPY="$COPY $@"
}

function MOVE # from to
{
	typeset f
	if	[[ $GROUP != $TEST_select ]]
	then	return
	fi
	for f
	do	case $f in
		INPUT|OUTPUT|ERROR)
			f=$TWD/$f
			;;
		/*)	;;
		*)	f=$PWD/$f
			;;
		esac
		MOVE="$MOVE $f"
	done
}

function SAME # new old
{
	typeset i file v
	if	[[ $GROUP != $TEST_select ]]
	then	return
	fi
	case $# in
	2)	case $1 in
		INPUT)	cat $2 > $1; return ;;
		esac
		SAME[$1]=$2
		file=$1
		COMPARE="$COMPARE $1"
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
		COMPARE="$COMPARE $2"
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

function RM # rm(1) args
{
	if	[[ ! $TEST_rmu ]]
	then	chmod -R u+rwx "$@" >/dev/null 2>&1
	fi
	rm $TEST_rmu $TEST_rmflags "$@"
}

function REMOVE # file ...
{
	typeset i
	for i
	do	RM $i $i.sav
	done
}

function IGNORE # file ...
{
	typeset i
	for i
	do	case $i in
		INPUT|OUTPUT|ERROR)
			i=$TWD/$i
			;;
		esac
		case " $IGNORE " in
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

function IGNORESPACE
{
	: ${IGNORESPACE=-b}
}

function EXIT # status
{
	EXIT=$1
}

function INFO # info description
{
	typeset -R15 info=$1
	if	[[ ! $1 ]]
	then	info=no
	fi
	shift
	if	[[ ! $TEST_quiet ]]
	then	print -r -u2 "$info " "$@"
	fi
}

function COMMAND # arg ...
{
	((TESTS++))
	case " ${EXPORT[*]}" in
	*' 'LC_ALL=*)
		;;
	*' 'LC_+([A-Z])=*)
		EXPORT[EXPORTS++]="LC_ALL="
		;;
	esac
	if	[[ $TEST_keep ]]
	then	(
		PS4=''
		set -x
		print -r -- "${EXPORT[@]}" "PATH=$PATH" $COMMAND "$@"
		) 2>&1 >/dev/null |
		sed 's,^print -r -- ,,' >$TWD/COMMAND
		chmod +x $TWD/COMMAND
	fi
	[[ $UMASK != $UMASK_ORIG ]] && umask $UMASK
	eval "${EXPORT[@]}" PATH='$PATH' '$'COMMAND '"$@"'
	STATUS=$?
	[[ $UMASK != $UMASK_ORIG ]] && umask $UMASK_ORIG
	return $STATUS
}

function RESULTS # pipe*
{
	typeset i j k s failed ignore io
	if	[[ $1 ]]
	then	io="$1 "
	fi
	for i in $COMPARE $TWD/OUTPUT $TWD/ERROR
	do	case " $IGNORE $ignore $MOVE " in
		*" $i "*)	continue ;;
		esac
		ignore="$ignore $i"
		j=${SAME[${i##*/}]}
		if	[[ ! $j ]]
		then	if	[[ $i == /* ]]
			then	k=$i
			else	k=$TWD/$i
			fi
			for s in ex sav err
			do	[[ -f $k.$s ]] && break
			done
			j=$k.$s
		fi
		if	[[ $DIAGNOSTICS && $i == */ERROR ]]
		then	if	[[ $STATUS == 0 && ! -s $TWD/ERROR ]]
			then	failed=$failed${failed:+,}DIAGNOSTICS
			fi
			continue
		fi
		diff $IGNORESPACE $i $j >$i.diff 2>&1
		if	[[ -s $i.diff ]]
		then	failed=$failed${failed:+,}${i#$TWD/}
			if	[[ $TEST_verbose ]]
			then	print -u2 "	===" diff $IGNORESPACE ${i#$TWD/} "<actual >expected ==="
				cat $i.diff >&2
			fi
		fi
	done
	if	[[ ! $failed && $STATUS != $EXIT ]]
	then	failed="exit code $EXIT expected -- got $STATUS"
	fi
	if	[[ $failed ]]
	then	((ERRORS++))
		if	[[ ! $TEST_quiet ]]
		then	SHOW="FAILED ${io}[ $failed ] $NOTE"
			print -r -u2 "	$SHOW"
		fi
		if	[[ $TEST_keep ]]
		then	GROUP=FINI
			exit
		fi
	elif	[[ ! $TEST_quiet ]]
	then	SHOW=$NOTE
		print -r -u2 "	$SHOW"
	fi
}

function SET # [no]name[=value]
{
	typeset i r
	if	[[ $TEST ]]
	then	RUN
	fi
	for i
	do	if	[[ $i == - ]]
		then	r=1
		elif	[[ $i == + ]]
		then	r=
		else	if	[[ $i == no?* ]]
			then	i=${i#no}
				v=
			elif	[[ $i == *=* ]]
			then	v=${i#*=}
				if	[[ $v == 0 ]]
				then	v=
				fi
				i=${i%%=*}
			else	v=1
			fi
			i=${i//-/_}
			if	[[ $r ]]
			then	READONLY[$i]=1
			elif	[[ ${READONLY[$i]} ]]
			then	continue
			fi
			eval TEST_$i=$v
		fi
	done
}

function VIEW # var [ file ]
{
	nameref var=$1
	typeset i bwd file pwd view root offset
	if	[[ $var ]]
	then	return 0
	fi
	case $# in
	1)	file=$1 ;;
	*)	file=$2 ;;
	esac
	pwd=${TWD%/*}
	bwd=${PMP%/*}
	if	[[ -r $file ]]
	then	if	[[ ! -d $file ]]
		then	var=$PWD/$file
			return 0
		fi
		for i in $file/*
		do	if	[[ -r $i ]]
			then	var=$PWD/$file
				return 0
			fi
			break
		done
	fi
	for view in ${VIEWS[@]}
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
		if	[[ -r $view$offset/$file ]]
		then	if	[[ ! -d $view$offset/$file ]]
			then	var=$view$offset/$file
				return 0
			fi
			for i in $view$offset/$file/*
			do	if	[[ -f $i ]]
				then	var=$view$offset/$file
					return 0
				fi
				break
			done
		fi
	done
	var=
	return 1
}

function INCLUDE # file ...
{
	typeset f v
	for f
	do	if	VIEW v $f || [[ $PREFIX && $f != /* ]] && VIEW v $PREFIX$f
		then	. $v
		else	FATAL $f: not found
		fi
	done
}

function UMASK # [ mask ]
{
	if	(( $# ))
	then	UMASK=$1
	else	UMASK=$UMASK_ORIG
	fi
}

function PIPE # INPUT|OUTPUT|ERROR file|- data ...
{
	IO $0 "$@"
}

# main

integer ERRORS=0 EXPORTS=0 TESTS=0 SUBTESTS=0 LINE=0 ITEM=0 LASTITEM=0 COUNT
typeset ARGS COMMAND COPY DIAGNOSTICS ERROR EXEC FLUSHED=0 GROUP=INIT
typeset IGNORE INPUT KEEP OUTPUT TEST SOURCE MOVE NOTE UMASK UMASK_ORIG
typeset ARGS_ORIG COMMAND_ORIG TITLE UNIT ARGV PREFIX OFFSET IGNORESPACE
typeset COMPARE
typeset TEST_file TEST_keep TEST_pipe_input TEST_pipe_io TEST_pipe_output
typeset TEST_quiet TEST_regular=1 TEST_rmflags='-rf --' TEST_rmu TEST_select

typeset -A EXPORT SAME VIEWS PIPE READONLY
typeset -Z LAST=00

unset FIGNORE

while	getopts -a $command "$USAGE" OPT
do	case $OPT in
	b)	(( $OPTARG )) && IGNORESPACE=-b
		;;
	i)	SET - pipe-input=$OPTARG
		;;
	k)	SET - keep=$OPTARG
		;;
	o)	SET - pipe-output=$OPTARG
		;;
	p)	SET - pipe-io=$OPTARG
		;;
	q)	SET - quiet=$OPTARG
		;;
	r)	SET - regular=$OPTARG
		;;
	t)	if	[[ $TEST_select ]]
		then	TEST_select="$TEST_select|${OPTARG//,/\|}"
		else	TEST_select="${OPTARG//,/\|}"
		fi
		;;
	v)	SET - verbose=$OPTARG
		;;
	D)	SET - trace=$OPTARG
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
PATH=$SOURCE:${PATH#?(.):}
PATH=${PATH%%:?(.)}:/usr/5bin:/bin:/usr/bin
UNIT=$1
shift
if	[[ -f $UNIT && ! -x $UNIT ]]
then	REGRESS=$UNIT
else	REGRESS=${UNIT%.tst}
	REGRESS=$REGRESS.tst
	[[ -f $REGRESS ]] || FATAL $REGRESS: regression tests not found
fi
UNIT=${UNIT##*/}
UNIT=${UNIT%.tst}
if	[[ $VPATH ]]
then	set -A VIEWS ${VPATH//:/' '}
	OFFSET=${SOURCE#${VIEWS[0]}}
	if	[[ $OFFSET ]]
	then	OFFSET=${OFFSET#/}/
	fi
fi
if	[[ $REGRESS == */* ]]
then	PREFIX=${REGRESS%/*}
	if	[[ ${#VIEWS[@]} ]]
	then	for i in ${VIEWS[@]}
		do	PREFIX=${PREFIX#$i/}
		done
	fi
	PREFIX=${PREFIX#$OFFSET}
	if	[[ $PREFIX ]]
	then	PREFIX=$PREFIX/
	fi
fi
TWD=$PWD/$UNIT.tmp
PMP=$(/bin/pwd)/$UNIT.tmp
UMASK_ORIG=$(umask)
UMASK=$UMASK_ORIG
ARGV=("$@")
trap 'RUN; CLEANUP 0' EXIT
trap 'CLEANUP $?' HUP INT PIPE TERM
if	[[ ! $TEST_select ]]
then	TEST_select="[0123456789]*"
fi
TEST_select="@($TEST_select|+(0))"
if	[[ $TEST_trace ]]
then	PS4='+$LINENO+ '
	set -x
fi
if	[[ $TEST_verbose ]]
then	typeset SHOW
else	typeset -L70 SHOW
fi
if	[[ $TEST_keep ]] && (ulimit -c 0) >/dev/null 2>&1
then	ulimit -c 0
fi

# some last minute shenanigans

alias BODY='BODY=BODY; function BODY'
alias DO='(( $ITEM != $FLUSHED )) && RUN DO; DO &&'
alias DONE='DONE=DONE; function DONE'
alias EXEC='LINE=$LINENO; EXEC'
alias INIT='INIT=INIT; function INIT'
alias PROG='LINE=$LINENO; FLUSH; PROG'

# do the tests

. $REGRESS
