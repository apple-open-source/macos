####################################################################
#                                                                  #
#             This software is part of the ast package             #
#                Copyright (c) 1982-2004 AT&T Corp.                #
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
#                David Korn <dgk@research.att.com>                 #
#                                                                  #
####################################################################
function err_exit
{
	print -u2 -n "\t"
	print -u2 -r $Command[$1]: "${@:2}"
	let Errors+=1
}
alias err_exit='err_exit $LINENO'

# test shell builtin commands
Command=$0
integer Errors=0
: ${foo=bar} || err_exit ": failed"
[[ $foo = bar ]] || err_exit ": side effects failed"
set -- - foobar
[[ $# = 2 && $1 = - && $2 = foobar ]] || err_exit "set -- - foobar failed"
set -- -x foobar
[[ $# = 2 && $1 = -x && $2 = foobar ]] || err_exit "set -- -x foobar failed"
getopts :x: foo || err_exit "getopts :x: returns false"
[[ $foo = x && $OPTARG = foobar ]] || err_exit "getopts :x: failed"
OPTIND=1
getopts :r:s var -r
if	[[ $var != : || $OPTARG != r ]]
then	err_exit "'getopts :r:s var -r' not working"
fi
false ${foo=bar} &&  err_exit "false failed"
read <<!
hello world
!
[[ $REPLY = 'hello world' ]] || err_exit "read builtin failed"
print x:y | IFS=: read a b
if	[[ $a != x ]]
then	err_exit "IFS=: read ... not working"
fi
read <<!
hello \
world
!
[[ $REPLY = 'hello world' ]] || err_exit "read continuation failed"
read -d x <<!
hello worldxfoobar
!
[[ $REPLY = 'hello world' ]] || err_exit "read builtin failed"
read <<\!
hello \
	world \

!
[[ $REPLY == 'hello 	world' ]] || err_exit "read continuation2 failed"
print "one\ntwo" | { read line
	print $line | /bin/cat > /dev/null
	read line
}
read <<\!
\
a\
\
\
b
!
if	[[ $REPLY != ab ]]
then	err_exit "read multiple continuation failed"
fi
if	[[ $line != two ]]
then	err_exit "read from pipeline failed"
fi
line=two
read line < /dev/null
if	[[ $line != "" ]]
then	err_exit "read from /dev/null failed"
fi
if	[[ $(print -R -) != - ]]
then	err_exit "print -R not working correctly"
fi
if	[[ $(print -- -) != - ]]
then	err_exit "print -- not working correctly"
fi
print -f "hello%nbar\n" size > /dev/null
if	((	size != 5 ))
then	err_exit "%n format of printf not working"
fi
print -n -u2 2>&1-
[[ -w /dev/fd/1 ]] || err_exit "2<&1- with built-ins has side effects"
x=$0
if	[[ $(eval 'print $0') != $x ]]
then	err_exit '$0 not correct for eval'
fi
unset x
readonly x
set -- $(readonly)
if      [[ " $@ " != *" x "* ]]
then    err_exit 'unset readonly variables are not displayed'
fi
if	[[ $(	for i in foo bar
		do	print $i
			continue 10
		done
	    ) != $'foo\nbar' ]]
then	err_exit 'continue breaks out of loop'
fi
(continue bad 2>/dev/null && err_exit 'continue bad should return an error')
(break bad 2>/dev/null && err_exit 'break bad should return an error')
(continue 0 2>/dev/null && err_exit 'continue 0 should return an error')
(break 0 2>/dev/null && err_exit 'break 0 should return an error')
breakfun() { break;}
continuefun() { continue;}
for fun in break continue
do	if	[[ $(	for i in foo
			do	${fun}fun
				print $i
			done
		) != foo ]]
	then	err_exit "$fun call in ${fun}fun breaks out of for loop"
	fi
done
if	[[ $(print -f "%b" "\a\n\v\b\r\f\E\03\\oo") != $'\a\n\v\b\r\f\E\03\\oo' ]]
then	err_exit 'print -f "%b" not working'
fi
if	[[ $(print -f "%P" "[^x].*b$") != '*[!x]*b' ]]
then	err_exit 'print -f "%P" not working'
fi
if	[[ $(abc: for i in foo bar;do print $i;break abc;done) != foo ]]
then	err_exit 'break labels not working'
fi
if	[[ $(command -v if)	!= if ]]
then	err_exit	'command -v not working'
fi
read -r var <<\!
	
!
if	[[ $var != "" ]]
then	err_exit "read -r of blank line not working"
fi
mkdir -p /tmp/ksh$$/a/b/c 2>/dev/null || err_exit  "mkdir -p failed"
rm -r /tmp/ksh$$ || err_exit "rm -r /tmp/ksh$$ failed"
trap 'print HUP' HUP
if	[[ $(trap) != "trap -- 'print HUP' HUP" ]]
then	err_exit '$(trap) not working'
fi
if	[[ $(trap -p HUP) != 'print HUP' ]]
then	err_exit '$(trap -p HUP) not working'
fi
[[ $($SHELL -c 'trap "print ok" SIGTERM; kill -s SIGTERM $$' 2> /dev/null) == ok
 ]] || err_exit 'SIGTERM not recognized'
[[ $($SHELL -c 'trap "print ok" sigterm; kill -s sigterm $$' 2> /dev/null) == ok
 ]] || err_exit 'SIGTERM not recognized'
${SHELL} -c 'kill -1 -$$' 2> /dev/null
[[ $(kill -l $?) == HUP ]] || err_exit 'kill -1 -pid not working' 
${SHELL} -c 'kill -1 -$$' 2> /dev/null
[[ $(kill -l $?) == HUP ]] || err_exit 'kill -n1 -pid not working' 
${SHELL} -c 'kill -s HUP -$$' 2> /dev/null
[[ $(kill -l $?) == HUP ]] || err_exit 'kill -HUP -pid not working' 
n=123
typeset -A base
base[o]=8#
base[x]=16#
base[X]=16#
for i in d i o u x X
do	if	(( $(( ${base[$i]}$(printf "%$i" $n) )) != n  ))
	then	err_exit "printf %$i not working"
	fi
done
if	[[ $( trap 'print done' EXIT) != done ]]
then	err_exit 'trap on EXIT not working'
fi
if	[[ $( trap 'print done' EXIT; trap - EXIT) == done ]]
then	err_exit 'trap on EXIT not being cleared'
fi
if	[[ $(type test) != 'test is a shell builtin' ]]
then	err_exit 'whence -v test not a builtin'
fi
builtin -d test
if	[[ $(type test) == *builtin* ]]
then	err_exit 'whence -v test after builtin -d incorrect'
fi
typeset -Z3 percent=$(printf '%o\n' "'%'")
forrmat=\\${percent}s
if      [[ $(printf "$forrmat") != %s ]]
then    err_exit "printf $forrmat not working"
fi
if	(( $(printf 'x\0y' | wc -c) != 3 ))
then	err_exit 'printf \0 not working'
fi
if	[[ $(printf "%bx%s\n" 'f\to\cbar') != $'f\to' ]]
then	err_exit 'printf %bx%s\n  not working'
fi
alpha=abcdefghijklmnop
if	[[ $(printf "%10.*s\n" 5 $alpha) != '     abcde' ]]
then	err_exit 'printf %10.%s\n  not working'
fi
float x2=.0000625
if	[[ $(printf "%10.5E\n" x2) != 6.25000E-05 ]]
then	err_exit 'printf "%10.5E" not normalizing correctly'
fi
x2=.000000001
if	[[ $(printf "%g\n" x2 2>/dev/null) != 1e-09 ]]
then	err_exit 'printf "%g" not working correctly'
fi
($SHELL read -s foobar <<\!
testing
!
) 2> /dev/null || err_exit ksh read -s var fails
if	[[ $(printf +3 2>/dev/null) !=   +3 ]]
then	err_exit 'printf is not processing formats beginning with + correctly'
fi
if	printf "%d %d\n" 123bad 78 >/dev/null 2>/dev/null
then	err_exit "printf not exiting non-zero with conversion errors"
fi
if	[[ $(trap --version 2> /dev/null;print done) != done ]]
then	err_exit 'trap builtin terminating after --version'
fi
if	[[ $(set --version 2> /dev/null;print done) != done ]]
then	err_exit 'set builtin terminating after --veresion'
fi
unset -f foobar
function foobar
{
	print 'hello world'
}
OPTIND=1
if	[[ $(getopts  $'[+?X\ffoobar\fX]' v --man 2>&1) != *'Xhello world'X* ]]
then	err_exit '\f...\f not working in getopts usage strings'
fi
if	[[	$(printf '%H\n' $'<>"& \'\tabc') != '&lt;&gt;&quot;&amp;&nbsp;&apos;&#9;abc' ]]
then	err_exit 'printf %H not working'
fi
if	[[	$(printf '%R %R %R %R\n' 'a.b' '*.c' '^'  '!(*.*)') != '^a\.b$ \.c$ ^\^$ ^(.*\..*)!$' ]]
then	err_exit 'printf %R not working'
fi
if	[[ $(printf '%..:c\n' abc) != a:b:c ]]
then	err_exit	"printf '%..:c' not working"
fi
if	[[ $(printf '%..*c\n' : abc) != a:b:c ]]
then	err_exit	"printf '%..*c' not working"
fi
if	[[ $(printf '%..:s\n' abc def ) != abc:def ]]
then	err_exit	"printf '%..:s' not working"
fi
if	[[ $(printf '%..*s\n' : abc def) != abc:def ]]
then	err_exit	"printf '%..*s' not working"
fi
behead()
{
	read line
	left=$(cat)
}
print $'line1\nline2' | behead
if	[[ $left != line2 ]]
then	err_exit  "read reading ahead on a pipe"
fi
read -n1 y <<!
abc
!
if      [[ $y != a ]]
then    err_exit  'read -n1 not working'
fi
print -n $'{ read -r line;print $line;}\nhello' > /tmp/ksh$$
chmod 755 /tmp/ksh$$
trap 'rm -rf /tmp/ksh$$' EXIT
if	[[ $($SHELL < /tmp/ksh$$) != hello ]]
then	err_exit 'read of incomplete line not working correctly'
fi
set -f
set -- *
if      [[ $1 != '*' ]]
then    err_exit 'set -f not working'
fi
unset pid1 pid2
false &
pid1=$!
pid2=$(
	wait $pid1
	(( $? == 127 )) || err_exit "job known to subshell"
	print $!
)
wait $pid1
(( $? == 1 )) || err_exit "wait not saving exit value"
wait $pid2
(( $? == 127 )) || err_exit "subshell job known to parent"
if	[[ $(foo=bar;foo=$foo exec -c $SHELL -c 'print $foo') != bar ]]
then	err_exit '"name=value exec -c ..." not working'
fi
$SHELL -c 'OPTIND=-1000000; getopts a opt -a' 2> /dev/null
[[ $? == 1 ]] || err_exit 'getopts with negative OPTIND not working'
getopts 'n#num' opt  -n 3
[[ $OPTARG == 3 ]] || err_exit 'getopts with numerical arguments failed'
if	[[ $($SHELL -c $'printf \'%2$s %1$s\n\' world hello') != 'hello world' ]]
then	err_exit 'printf %2$s %1$s not working'
fi
unset a
{ read -N3 a; read -N1 b;}  <<!
abcdefg
!
[[ $a == abc ]] || err_exit 'read -N3 here-document not working'
[[ $b == d ]] || err_exit 'read -N1 here-document not working'
read -n3 a <<!
abcdefg
!
[[ $a == abc ]] || err_exit 'read -n3 here-document not working'
(print -n a;sleep 1; print -n bcde) | { read -N3 a; read -N1 b;}
[[ $a == abc ]] || err_exit 'read -N3 from pipe not working'
[[ $b == d ]] || err_exit 'read -N1 from pipe not working'
(print -n a;sleep 1; print -n bcde) |read -n3 a
[[ $a == a ]] || err_exit 'read -n3 from pipe not working'
rm -f /tmp/fifo$$
if	mkfifo /tmp/fifo$$ 2> /dev/null
then	(print -n a; sleep 1;print -n bcde)  > /tmp/fifo$$ &
	{
	read -u5 -n3  -t2 a  || err_exit 'read -n3 from fifo timedout'
	read -u5 -n1 -t2 b || err_exit 'read -n1 from fifo timedout'
	} 5< /tmp/fifo$$
	[[ $a == a ]] || err_exit 'read -n3 from fifo not working'
	rm -f /tmp/fifo$$
	mkfifo /tmp/fifo$$ 2> /dev/null
	(print -n a; sleep 1;print -n bcde)  > /tmp/fifo$$ &
	{
	read -u5 -N3 -t2 a || err_exit 'read -N3 from fifo timed out'
	read -u5 -N1 -t2 b || err_exit 'read -N1 from fifo timedout'
	} 5< /tmp/fifo$$
	[[ $a == abc ]] || err_exit 'read -N3 from fifo not working'
	[[ $b == d ]] || err_exit 'read -N1 from fifo not working'
fi
rm -f /tmp/fifo$$
function longline
{
	integer i
	for((i=0; i < $1; i++))
	do	print argument$i
	done
}
# test command -x option
integer sum=0 n=10000
if	! ${SHELL:-ksh} -c 'print $#' count $(longline $n) > /dev/null  2>&1
then	for i in $(command command -x ${SHELL:-ksh} -c 'print $#;[[ $1 != argument0 ]]' count $(longline $n) 2> /dev/null)
	do	((sum += $i))
	done
	(( sum == n )) || err_exit "command -x processed only $sum arguments"
	command -p command -x ${SHELL:-ksh} -c 'print $#;[[ $1 != argument0 ]]' count $(longline $n) > /dev/null  2>&1
	[[ $? != 1 ]] && err_exit 'incorrect exit status for command -x'
fi
# test command -x option with extra arguments
integer sum=0 n=10000
if      ! ${SHELL:-ksh} -c 'print $#' count $(longline $n) > /dev/null  2>&1
then    for i in $(command command -x ${SHELL:-ksh} -c 'print $#;[[ $1 != argument0 ]]' count $(longline $n) one two three) #2> /dev/null)
	do      ((sum += $i))
	done
	(( sum  > n )) || err_exit "command -x processed only $sum arguments"
	(( (sum-n)%3==0 )) || err_exit "command -x processed only $sum arguments"
	(( sum == n+3)) && err_exit "command -x processed only $sum arguments"
	command -p command -x ${SHELL:-ksh} -c 'print $#;[[ $1 != argument0 ]]' count $(longline $n) > /dev/null  2>&1
	[[ $? != 1 ]] && err_exit 'incorrect exit status for command -x'
fi
# test for debug trap
[[ $(typeset -i i=0
	trap 'print $i' DEBUG
	while (( i <2))
	do	(( i++))
	done) == $'0\n0\n1\n1\n2' ]]  || print -r "DEBUG trap not working"
exit $((Errors))
