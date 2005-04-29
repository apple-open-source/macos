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

Command=$0
integer Errors=0
r=readonly u=Uppercase l=Lowercase i=22 i8=10 L=abc L5=def uL5=abcdef xi=20
x=export t=tagged H=hostname LZ5=026 RZ5=026 Z5=123 lR5=ABcdef R5=def n=l
for option in u l i i8 L L5 LZ5 RZ5 Z5 r x H t R5 uL5 lR5 xi n
do	typeset -$option $option
done
(r=newval) 2> /dev/null && err_exit readonly attribute fails
i=i+5
if	((i != 27))
then	err_exit integer attributes fails
fi
if	[[ $i8 != 8#12 ]]
then	err_exit integer base 8 fails
fi
if	[[ $u != UPPERCASE ]]
then	err_exit uppercase fails
fi
if	[[ $l != lowercase ]]
then	err_exit lowercase fails
fi
if	[[ $n != lowercase ]]
then	err_exit reference variables fail
fi
if	[[ t=tagged != $(typeset -t) ]]
then	err_exit tagged fails
fi
if	[[ t != $(typeset +t) ]]
then	err_exit tagged fails
fi
if	[[ $Z5 != 00123 ]]
then	err_exit zerofill fails
fi
if	[[ $RZ5 != 00026 ]]
then	err_exit right zerofill fails
fi
L=12345
if	[[ $L != 123 ]]
then	err_exit leftjust fails
fi
if	[[ $L5 != "def  " ]]
then	err_exit leftjust fails
fi
if	[[ $uL5 != ABCDE ]]
then	err_exit leftjust uppercase fails
fi
if	[[ $lR5 != bcdef ]]
then	err_exit rightjust fails
fi
if	[[ $R5 != "  def" ]]
then	err_exit rightjust fails
fi
if	[[ $($SHELL -c 'echo $x') != export ]]
then	err_exit export fails
fi
if	[[ $($SHELL -c 'xi=xi+4;echo $xi') != 24 ]]
then	err_exit export attributes fails
fi
x=$(foo=abc $SHELL <<!
	foo=bar
	$SHELL -c  'print \$foo'
!
)
if	[[ $x != bar ]]
then	err_exit 'environment variables require re-export'
fi
(typeset + ) > /dev/null 2>&1 || err_exit 'typeset + not working'
(typeset -L-5 buf="A" 2>/dev/null)
if [[ $? == 0 ]]
then	err_exit 'typeset allows negative field for left/right adjust'
fi
a=b
readonly $a=foo
if	[[ $b != foo ]]
then	err_exit 'readonly $a=b not working'
fi
if	[[ $(export | grep '^PATH=') != PATH=* ]]
then	err_exit 'export not working'
fi
picture=(
	bitmap=/fruit
	size=(typeset -E x=2.5)
)
string="$(print $picture)"
if [[ "${string}" != *'size=( typeset -E'* ]]
then	err_exit 'print of compound exponential variable not working'
fi
sz=(typeset -E y=2.2)
string="$(print $sz)"
if [[ "${sz}" == *'typeset -E -F'* ]]
then 	err_exit 'print of exponential shows both -E and -F attributes'  
fi
print 'typeset -i m=48/4+1;print -- $m' > /tmp/ksh$$
chmod +x /tmp/ksh$$
typeset -Z2 m
if	[[ $(/tmp/ksh$$) != 13 ]]
then	err_exit 'attributes not cleared for script execution'
fi
typeset -Z  LAST=00
unset -f foo
function foo
{
        if [[ $1 ]]
        then    LAST=$1
        else    ((LAST++))
        fi
}
foo 1
if	(( ${#LAST} != 2 ))
then	err_exit 'LAST!=2'
fi
foo
if	(( ${#LAST} != 2 ))
then	err_exit 'LAST!=2'
fi
rm -rf /tmp/ksh$$
set -a
unset foo
foo=bar
if	[[ $(export | grep ^foo=) != 'foo=bar' ]]
then	err_exit 'all export not working'
fi
unset foo
read foo <<!
bar
!
if	[[ $(export | grep ^foo=) != 'foo=bar' ]]
then	err_exit 'all export not working with read'
fi
if	[[ $(typeset | grep PS2) == PS2 ]]
then	err_exit 'typeset without arguments outputs names without attributes'
fi
unset a z q x
w1=hello
w2=world
t1="$w1 $w2"
if	(( 'a' == 97 ))
then	b1=aGVsbG8gd29ybGQ=
	b2=aGVsbG8gd29ybGRoZWxsbyB3b3JsZA==
else	b1=iIWTk5ZAppaZk4Q=
	b2=iIWTk5ZAppaZk4SIhZOTlkCmlpmThA==
fi
z=$b1
typeset -b x=$b1
[[ $x == "$z" ]] || print -u2 'binary variable not expanding correctly'
[[  $(printf "%B" x) == $t1 ]] || err_exit 'typeset -b not working'
typeset -b -Z5 a=$b1
[[  $(printf "%B" a) == $w1 ]] || err_exit 'typeset -b -Z5 not working'
typeset -b q=$x$x
[[ $q == $b2 ]] || err_exit 'typeset -b not working with concatination'
[[  $(printf "%B" q) == $t1$t1 ]] || err_exit 'typeset -b concatination not working'
x+=$b1
[[ $x == $b2 ]] || err_exit 'typeset -b not working with append'
[[  $(printf "%B" x) == $t1$t1 ]] || err_exit 'typeset -b append not working'
typeset -b -Z20 z=$b1
(( $(printf "%B" z | wc -c) == 20 )) || err_exit 'typeset -b -Z20 not storing 20 bytes'
exit	$((Errors))
