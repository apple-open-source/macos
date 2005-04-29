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
	print -u2 -r $Command: "$@"
	let Errors+=1
}


Command=$0
integer Errors=0
{
x=abc
x+=def ;} 2> /dev/null
if	[[ $x != abcdef ]]
then	err_exit 'abc+def != abcdef'
fi
integer i=3
{ i+=4;} 2> /dev/null
if	(( i != 7 ))
then	err_exit '3+4!=7'
fi
iarray=( one two three )
{ iarray+= (four five six) ;} 2> /dev/null
if	[[ ${iarray[@]} != 'one two three four five six' ]]
then	err_exit 'index array append fails'
fi
unset iarray
iarray=one
{ iarray+= (four five six) ;} 2> /dev/null
if	[[ ${iarray[@]} != 'one four five six' ]]
then	err_exit 'index array append to scalar fails'
fi
typeset -A aarray
aarray=( [1]=1 [3]=4 [xyz]=xyz )
aarray+=( [2]=2 [3]=3 [foo]=bar )
if	[[ ${aarray[3]} != 3 ]]
then	err_exit 'associative array append fails'
fi
if	[[ ${#aarray[@]} != 5 ]]
then	err_exit 'number of elements of associative array append fails'
fi
point=(x=1 y=2)
point+=( y=3 z=4)
if	[[ ${point.y} != 3 ]]
then	err_exit 'compound append fails'
fi
unset foo
foo=one
foo+=(two)
if	[[ ${foo[@]} != 'one two' ]]
then	err_exit 'array append to non array variable fails'
fi
exit $((Errors))
