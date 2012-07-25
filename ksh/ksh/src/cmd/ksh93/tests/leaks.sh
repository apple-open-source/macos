########################################################################
#                                                                      #
#               This software is part of the ast package               #
#          Copyright (c) 1982-2011 AT&T Intellectual Property          #
#                      and is licensed under the                       #
#                  Common Public License, Version 1.0                  #
#                    by AT&T Intellectual Property                     #
#                                                                      #
#                A copy of the License is available at                 #
#            http://www.opensource.org/licenses/cpl1.0.txt             #
#         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         #
#                                                                      #
#              Information and Software Systems Research               #
#                            AT&T Research                             #
#                           Florham Park NJ                            #
#                                                                      #
#                  David Korn <dgk@research.att.com>                   #
#                                                                      #
########################################################################
builtin vmstate 2>/dev/null || exit 0

function err_exit
{
	print -u2 -n "\t"
	print -u2 -r ${Command}[$1]: "${@:2}"
	let Errors+=1
}
alias err_exit='err_exit $LINENO'

Command=${0##*/}
integer Errors=0

# test for variable reset leak #

function test_reset
{
	integer i n=$1

	for ((i = 0; i < n; i++))
	do	u=$i
	done
}

n=1000

# one round to get to steady state -- sensitive to -x

test_reset $n
a=0$(vmstate --format='+%(size)u')

test_reset $n
a=0$(vmstate --format='+%(size)u')
test_reset $n
b=0$(vmstate --format='+%(size)u')

if	(( b > a ))
then	err_exit "variable value reset memory leak -- $((b-a)) bytes after $n iterations"
fi

# buffer boundary tests

for exp in 65535 65536
do	got=$($SHELL -c 'x=$(printf "%.*c" '$exp' x); print ${#x}' 2>&1)
	[[ $got == $exp ]] || err_exit "large command substitution failed -- expected $exp, got $got"
done

exit $((Errors<125?Errors:125))
