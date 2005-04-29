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

integer Errors=0
Command=$0
# RANDOM
if	(( RANDOM==RANDOM || $RANDOM==$RANDOM ))
then	err_exit RANDOM variable not working
fi
# SECONDS
sleep 3
if	(( SECONDS < 2 ))
then	err_exit SECONDS variable not working
fi
# _
set abc def
if	[[ $_ != def ]]
then	err_exit _ variable not working
fi
# ERRNO
#set abc def
#rm -f foobar#
#ERRNO=
#2> /dev/null < foobar#
#if	(( ERRNO == 0 ))
#then	err_exit ERRNO variable not working
#fi
# PWD
if	[[ !  $PWD -ef . ]]
then	err_exit PWD variable not working
fi
# PPID
if	[[ $($SHELL -c 'echo $PPID')  != $$ ]]
then	err_exit PPID variable not working
fi
# OLDPWD
old=$PWD
cd /
if	[[ $OLDPWD != $old ]]
then	err_exit OLDPWD variable not working
fi
cd $d || err_exit cd failed
# REPLY
read <<-!
	foobar
	!
if	[[ $REPLY != foobar ]]
then	err_exit REPLY variable not working
fi
integer save=$LINENO
# LINENO
LINENO=10
#
#  These lines intentionally left blank
#
if	(( LINENO != 13))
then	err_exit LINENO variable not working
fi
LINENO=save+10
ifs=$IFS
IFS=:
x=a::b::c
if	[[ $x != a::b::c ]]
then	err_exit "Word splitting on constants"
fi
set -- $x
if	[[ $# != 5 ]]
then	err_exit ":: doesn't separate null arguments "
fi
set x
if	x$1=0 2> /dev/null
then	err_exit "x\$1=value treated as an assignment"
fi
# check for attributes across subshells
typeset -i x=3
y=1/0
if	( typeset x=y) 2> /dev/null
then	print -u2 "attributes not passed to subshells"
fi
unset x
function x.set
{
	nameref foo=${.sh.name}.save
	foo=${.sh.value}
	.sh.value=$0
}
x=bar
if	[[ $x != x.set ]]
then	err_exit 'x.set does not override assignment'
fi
x.get()
{
	nameref foo=${.sh.name}.save
	.sh.value=$foo
}

if	[[ $x != bar ]]
then	err_exit 'x.get does not work correctly'
fi
typeset +n foo
unset foo
foo=bar
(
	unset foo
	if	[[ $foo != '' ]]
	then	err_exit '$foo not null after unset in subsehll'
	fi
)
if	[[ $foo != bar ]]
then	err_exit 'unset foo in subshell produces side effect '
fi
unset foo
if	[[ $( { : ${foo?hi there} ; } 2>&1) != *'hi there' ]]
then	err_exit '${foo?hi there} with foo unset does not print hi there on 2'
fi
x=$0
set foobar
if	[[ ${@:0} != "$x foobar" ]]
then	err_exit '${@:0} not expanding correctly'
fi
set --
if	[[ ${*:0:1} != "$0" ]]
then	err_exit '${@:0} not expanding correctly'
fi
function COUNT.set
{
        (( ACCESS++ ))
}
COUNT=0
(( COUNT++ ))
if	(( COUNT != 1 || ACCESS!=2 ))
then	err_exit " set discipline failure COUNT=$COUNT ACCESS=$ACCESS"
fi
LANG=C > /dev/null 2>&1
if	[[ $LANG != C ]]
then	err_exit "C locale not working"
fi
unset RANDOM
unset -n foo
foo=junk
function foo.get
{
	.sh.value=stuff 
	unset -f foo.get
}
if	[[ $foo != stuff ]]
then	err_exit "foo.get discipline not working"
fi
if	[[ $foo != junk ]]
then	err_exit "foo.get discipline not working after unset"
fi
# special variables
set -- 1 2 3 4 5 6 7 8 9 10
sleep 1000 &
if	[[ $(print -r -- ${#10}) != 2 ]]
then	err_exit '${#10}, where ${10}=10 not working'
fi
for i in @ '*' ! '#' - '?' '$'
do	false
	eval foo='$'$i bar='$'{$i}
	if	[[ ${foo} != "${bar}" ]]
	then	err_exit "\$$i not equal to \${$i}"
	fi
	command eval bar='$'{$i%?} 2> /dev/null || err_exit "\${$i%?} gives syntax error"
	if	[[ $i != [@*] && ${foo%?} != "$bar"  ]]
	then	err_exit "\${$i%?} not correct"
	fi
	command eval bar='$'{$i#?} 2> /dev/null || err_exit "\${$i#?} gives syntax error"
	if	[[ $i != [@*] && ${foo#?} != "$bar"  ]]
	then	err_exit "\${$i#?} not correct"
	fi
	command eval foo='$'{$i} bar='$'{#$i} || err_exit "\${#$i} gives synta
x error"
	if	[[ $i != @([@*]) && ${#foo} != "$bar" ]]
	then	err_exit "\${#$i} not correct"
	fi
done
unset x
CDPATH=/
x=$(cd tmp)
if	[[ $x != /tmp ]]
then	err_exit 'CDPATH does not display new directory'
fi
mkdir /tmp/ksh$$
CDPATH=/:
x=$(cd /tmp;cd ksh$$)
if	[[ $x ]]
then	err_exit 'CDPATH displays new directory when not used'
fi
x=$(cd tmp/ksh$$)
if	[[ $x != /tmp/ksh$$ ]]
then	err_exit "CDPATH tmp/ksh$$ does not display new directory"
fi
cd /
rm -rf /tmp/ksh$$
TMOUT=100
(TMOUT=20)
if	(( TMOUT !=100 ))
then	err_exit 'setting TMOUT in subshell affects parent'
fi
unset y
function setdisc # var
{
        eval function $1.get'
        {
                .sh.value=good
        }
        '
}
y=bad
setdisc y
if	[[ $y != good ]]
then	err_exit 'setdisc function not working'
fi
integer x=$LINENO
: $'\
'
if	(( LINENO != x+3  ))
then	err_exit '\<newline> gets linenumber count wrong'
fi
set --
set -- "${@-}"
if	(( $# !=1 ))
then	err_exit	'"${@-}" not expanding to null string'
fi
for i in : % + / 3b '**' '***' '@@' '{' '[' '}' !!  '*a' '@a' '$foo'
do      (eval : \${"$i"} 2> /dev/null) && err_exit "\${$i} not an syntax error"
done
unset IFS
( IFS='  ' ; read -r a b c <<-!
	x  y z
	!
	if	[[ $b ]]
	then	err_exit 'IFS="  " not causing adjacent space to be null string'
	fi
)
read -r a b c <<-!
x  y z
!
if	[[ $b != y ]]
then	err_exit 'IFS not restored after subshell'
fi
if	[[ $( (print ${12345:?}) 2>&1) != *12345* ]]
then	err_exit 'Incorrect error message with ${12345?}'
fi
unset foobar
if	[[ $( (print ${foobar:?}) 2>&1) != *foobar* ]]
then	err_exit 'Incorrect error message with ${foobar?}'
fi
unset bar
if	[[ $( (print ${bar:?bam}) 2>&1) != *bar*bam* ]]
then	err_exit 'Incorrect error message with ${foobar?}'
fi
{ $SHELL -c '
function foo
{
	typeset SECONDS=0
	sleep 1.5
	print $SECONDS
	
}
x=$(foo)
(( x >1 && x < 2 )) 
'
} 2> /dev/null   || err_exit 'SECONDS not working in function'
trap 'rm -f /tmp/script$$ /tmp/out$$' EXIT
cat > /tmp/script$$ <<-\!
	posixfun()
	{
		unset x
	 	nameref x=$1
	 	print  -r -- "$x"
	}
	function fun
	{
	 	nameref x=$1
	 	print  -r -- "$x"
	}
	if	[[ $1 ]]
	then	file=${.sh.file}
	else	print -r -- "${.sh.file}"
	fi
!
chmod +x /tmp/script$$
. /tmp/script$$  1
[[ $file == /tmp/script$$ ]] || err_exit ".sh.file not working for dot scripts"
[[ $($SHELL /tmp/script$$) == /tmp/script$$ ]] || err_exit ".sh.file not working for scripts"
[[ $(posixfun .sh.file) == /tmp/script$$ ]] || err_exit ".sh.file not working for posix functions"
[[ $(fun .sh.file) == /tmp/script$$ ]] || err_exit ".sh.file not working for functions"
[[ $(posixfun .sh.fun) == posixfun ]] || err_exit ".sh.fun not working for posix functions"
[[ $(fun .sh.fun) == fun ]] || err_exit ".sh.fun not working for functions"
[[ $(posixfun .sh.subshell) == 1 ]] || err_exit ".sh.subshell not working for posix functions"
[[ $(fun .sh.subshell) == 1 ]] || err_exit ".sh.subshell not working for functions"
(
    [[ $(posixfun .sh.subshell) == 2 ]]  || err_exit ".sh.subshell not working for posix functions in subshells"
    [[ $(fun .sh.subshell) == 2 ]]  || err_exit ".sh.subshell not working for functions in subshells"
    (( .sh.subshell == 1 )) || err_exit ".sh.subshell not working in a subshell"
)
TIMEFORMAT='this is a test'
[[ $({ { time :;} 2>&1;}) == "$TIMEFORMAT" ]] || err_exit 'TIMEFORMAT not working'
: ${.sh.version}
[[ $(alias integer) == *.sh.* ]] && err_exit '.sh. prefixed to alias name'
: ${.sh.version}
[[ $(whence rm) == *.sh.* ]] && err_exit '.sh. prefixed to tracked alias name'
: ${.sh.version}
[[ $(cd /bin;env | grep PWD) == *.sh.* ]] && err_exit '.sh. prefixed to PWD'
# unset discipline bug fix
dave=dave
function dave.unset
{
    unset dave
}
unset dave
[[ $(typeset +f) == *dave.* ]] && err_exit 'unset discipline not removed'
print 'print ${VAR}' >  /tmp/script$$
VAR=foo /tmp/script$$ > /tmp/out$$
[[ $(</tmp/out$$) == foo ]] || err_exit 'environment variables not passed to scripts'
(
	unset dave
	function  dave.append
	{
		.sh.value+=$dave
		dave=
	}
	dave=foo; dave+=bar
	[[ $dave == barfoo ]] || exit 2
) 2> /dev/null  
case $? in
0)	 ;;
1)	 err_exit 'append discipline not implemented';;
*)	 err_exit 'append discipline not working';;
esac
exit $((Errors))
