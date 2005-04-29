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
integer x=1 y=2 z=3
if	(( 2+2 != 4 ))
then	err_exit 2+2!=4
fi
if	((x+y!=z))
then	err_exit x+y!=z
fi
if	(($x+$y!=$z))
then	err_exit $x+$y!=$z
fi
if	(((x|y)!=z))
then	err_exit "(x|y)!=z"
fi
if	((y >= z))
then	err_exit "y>=z"
fi
if	((y+3 != z+2))
then	err_exit "y+3!=z+2"
fi
if	((y<<2 != 1<<3))
then	err_exit "y<<2!=1<<3"
fi
if	((133%10 != 3))
then	err_exit "133%10!=3"
	if	(( 2.5 != 2.5 ))
	then	err_exit 2.5!=2.5
	fi
fi
d=0
((d || 1)) || err_exit 'd=0; ((d||1))'
if	(( d++!=0))
then	err_exit "d++!=0"
fi
if	(( --d!=0))
then	err_exit "--d!=0"
fi
if	(( (d++,6)!=6 && d!=1))
then	err_exit '(d++,6)!=6 && d!=1'
fi
d=0
if	(( (1?2+1:3*4+d++)!=3 || d!=0))
then	err_exit '(1?2+1:3*4+d++) !=3'
fi
for	((i=0; i < 20; i++))
do	:
done
if	(( i != 20))
then	err_exit 'for (( expr)) failed'
fi
for	((i=0; i < 20; i++)); do	: ; done
if	(( i != 20))
then	err_exit 'for (( expr));... failed'
fi
for	((i=0; i < 20; i++)) do	: ; done
if	(( i != 20))
then	err_exit 'for (( expr))... failed'
fi
if	(( (i?0:1) ))
then	err_exit '(( (i?0:1) )) failed'
fi
if	(( (1 || 1 && 0) != 1 ))
then	err_exit '( (1 || 1 && 0) != 1) failed'
fi
if	(( (_=1)+(_x=0)-_ ))
then	err_exit '(_=1)+(_x=0)-_ failed'
fi
if	((  (3^6) != 5))
then	err_exit '((3^6) != 5) failed'
fi
integer x=1
if	(( (x=-x) != -1 ))
then	err_exit '(x=-x) != -1 failed'
fi
i=2
if	(( 1$(($i))3 != 123 ))
then	err_exit ' 1$(($i))3 failed'
fi
((pi=4*atan(1.)))
point=(
	float x
	float y
)
(( point.x = cos(pi/6), point.y = sin(pi/6) ))
if	(( point.x*point.x + point.y*point.y > 1.01 ))
then	err_exit 'cos*cos +sin*sin > 1.01'
fi
if	(( point.x*point.x + point.y*point.y < .99 ))
then	err_exit 'cos*cos +sin*sin < .99'
fi
if [[ $((y=x=1.5)) != 1 ]]
then	err_exit 'typecast not working in arithmetic evaluation'
fi
typeset -E x=1.5
( ((x++))  ) 2>/dev/null
if [[ $? == 0 ]]
then	err_exit 'postincrement of floating point allowed'
fi
( ((++x))  ) 2>/dev/null
if [[ $? == 0 ]]
then	err_exit 'preincrement of floating point allowed'
fi
x=1.5
( ((x%1.1))  ) 2>/dev/null
if [[ $? == 0 ]]
then	err_exit 'floating point allowed with % operator'
fi
x=.125
if	[[ $(( 4 * x/2 )) != 0.25 ]] 
then	err_exit '(( 4 * x/2 )) is not 0.25, with x=.125'
fi
if	[[ $(( pow(2,3) )) != 8 ]]
then	err_exit '$(( pow(2,3) )) != 8'
fi
( [[ $(( pow(2,(3)) )) == 8 ]] ) 2> /dev/null
if	(( $? ))
then	err_exit '$(( pow(2,(3)) )) != 8'
fi
unset x
integer x=1; integer x=1
if	[[ $x != 1 ]]
then	err_exit 'two consecutive integer x=1 not working'
fi
unset z
{ z=$(typeset -RZ2 z2; (( z2 = 8 )); print $z2) ;} 2>/dev/null
if [[ $z != "08" ]]
then	err_exit "typeset -RZ2 leading 0 decimal not working [z=$z]"
fi
{ z=$(typeset -RZ3 z3; (( z3 = 8 )); print $z3) ;} 2>/dev/null
if [[ $z != "008" ]]
then	err_exit "typeset -RZ3 leading 0 decimal not working [z=$z]"
fi
unset z
typeset -Z3 z=010
(( z=z+1))
if	[[ $z != 011 ]]
then	err_exit "leading 0's in -Z not treated as decimal"
fi
unset x
integer x=0
if	[[ $((x+=1)) != 1  ]] || ((x!=1))
then	err_exit "+= not working"
	x=1
fi
x=1
if	[[ $((x*=5)) != 5  ]] || ((x!=5))
then	err_exit "*= not working"
	x=5
fi
if	[[ $((x%=4)) != 1  ]] || ((x!=1))
then	err_exit "%= not working"
	x=1
fi
if	[[ $((x|=6)) != 7  ]] || ((x!=7))
then	err_exit "|= not working"
	x=7
fi
if	[[ $((x&=5)) != 5  ]] || ((x!=5))
then	err_exit "&= not working"
	x=5
fi
function newscope
{
	float x=1.5 
	(( x += 1 ))
	print -r -- $x
}
if	[[ $(newscope) != 2.5 ]]
then	err_exit "arithmetic using wrong scope"
fi
unset x
integer y[3]=9 y[4]=2 i=3
(( x = y[3] + y[4] ))
if	[[ $x != 11 ]]
then	err_exit "constant index array arithmetic failure"
fi
(( x = $empty y[3] + y[4] ))
if	[[ $x != 11 ]]
then	err_exit "empty constant index array arithmetic failure"
fi
(( x = y[i] + y[i+1] ))
if	[[ $x != 11 ]]
then	err_exit "variable subscript index array arithmetic failure"
fi
integer a[5]=3 a[2]=4
(( x = y[a[5]] + y[a[2]] ))
if	[[ $x != 11 ]]
then	err_exit "nested subscript index array arithmetic failure"
fi
unset y
typeset -Ai y
y[three]=9 y[four]=2
three=four
four=three
(( x = y[three] + y[four] ))
if	[[ $x != 11 ]]
then	err_exit "constant associative array arithmetic failure"
fi
(( x = y[$three] + y[$four] ))
if	[[ $x != 11 ]]
then	err_exit "variable subscript associative array arithmetic failure"
fi
$SHELL -nc '((a = 1))' 2> /dev/null || err_exit "sh -n fails with arithmetic"
$SHELL -nc '((a.b++))' 2> /dev/null || err_exit "sh -n fails with arithmetic2"
unset z
float z=7.5
if	{ (( z%2 != 1));} 2> /dev/null
then	err_exit '% not working on floating point'
fi
chr=(a ' ' '=' '\r' '\n' '\\' '\"' '$' "\\'" '[' ']' '(' ')' '<' '\xab' '\040' '`' '{' '}' '*' '\E')
if	(('a' == 97))
then	val=(97 32  61 13 10 92 34 36 39 91 93 40 41 60 171 32 96 123 125 42 27)
else	val=(129 64 126 13 21 224 127 91 125 173 189 77 93 76 171 32 121 192 208 92 39 21)
fi
q=0
for ((i=0; i < ${#chr[@]}; i++))
do	if	(( '${chr[i]}' != ${val[i]} ))
	then	err_exit "(( '${chr[i]}'  !=  ${val[i]} ))"
	fi
	if	[[ $(( '${chr[i]}' )) != ${val[i]} ]]
	then	err_exit "(( '${chr[i]}' )) !=  ${val[i]}"
	fi
	if	[[ $(( L'${chr[i]}' )) != ${val[i]} ]]
	then	err_exit "(( '${chr[i]}' )) !=  ${val[i]}"
	fi
	if	eval '((' "'${chr[i]}'" != ${val[i]} '))'
	then	err_exit "eval (( '${chr[i]}'  !=  ${val[i]} ))"
	fi
	if	eval '((' "'${chr[i]}'" != ${val[i]} ' + $q ))'
	then	err_exit "eval (( '${chr[i]}'  !=  ${val[i]} ))"
	fi
done
unset x
typeset -ui x=4294967293
[[ $x != 4294967293 ]]  && err_exit "unsigned integers not working"
x=32767
x=x+1
[[ $x != 32768 ]]  && err_exit "unsigned integer addition not working"
unset x
float x=99999999999999999999999999
if	(( x < 1e20 ))
then	err_exit 'large integer constants not working'
fi
unset x  y
function foobar
{
	nameref x=$1
	(( x +=1 ))
	print $x
}
x=0 y=4
if	[[ $(foobar y) != 5 ]]
then	err_exit 'name references in arithmetic statements in functions broken'
fi
if	(( 2**3 != pow(2,3) ))
then	err_exit '2**3 not working'
fi
if	(( 2**3*2 != pow(2,3)*2 ))
then	err_exit '2**3*2 not working'
fi
if	(( 4**3**2 != pow(4,pow(3,2)) ))
then	err_exit '4**3**2 not working'
fi
if	(( (4**3)**2 != pow(pow(4,3),2) ))
then	err_exit '(4**3)**2 not working'
fi
typeset -Z3 x=11
typeset -i x
if	(( x != 11 ))
then	err_exit '-Z3 not treated as decimal'
fi
unset x
typeset -ui x=-1
(( x >= 0 )) || err_exit 'unsigned integer not working'
(( $x >= 0 )) || err_exit 'unsigned integer not working as $x'
unset x
typeset -ui42 x=50
if	[[ $x != 42#18 ]]
then	err_exit 'display of unsigned integers in non-decimal bases wrong'
fi
$SHELL -c 'i=0;(( ofiles[i] != -1 && (ofiles[i] < mins || mins == -1) ));exit 0' 2> /dev/null || err_exit 'lexical error with arithemtic expression'
rm -f core
$SHELL -c '(( +1 == 1))' 2> /dev/null || err_exit 'unary + not working'
typeset -E20 val=123.01234567890
[[ $val == 123.0123456789 ]] || err_exit "rounding error val=$val"
if	[[ $(print x$((10))=foo) != x10=foo ]]
then	err_exit 'parsing error with x$((10))=foo'
fi
$SHELL -c 'typeset x$((10))=foo' 2> /dev/null || err_exit 'typeset x$((10)) parse error'
x=$(( exp(log(2.0)) ))
(( x > 1.999 && x < 2.001 )) || err_exit 'composit functions not working'
unset x y n
typeset -Z8 x=0 y=0
integer n
for	(( n=0; n < 20; n++ ))
do	let "x = $x+1"
	(( y = $y+1 ))
done
(( x == n ))  || err_exit 'let with zero filled fields not working'
(( y == n ))  || err_exit '((...)) with zero filled fields not working'
typeset -LZ3 x=10
[[ $(($x)) == 10 && $((1$x)) == 1010 ]] || err_exit 'zero filled fields not preserving leading zeros'
unset y
[[ $(let y=$x;print $y) == 10 && $(let y=1$x;print $y) == 1010 ]] || err_exit 'zero filled fields not preserving leading zeros with let'
unset i ip ipx
typeset -i hex=( 172 30 18 1)
typeset -iu ip=0 ipx=0
integer i
for	((i=0; i < 4; i++))
do	(( ip =  (ip<<8) | hex[i]))
done
for ((i=0; i < 4; i++))
do	(( ipx = ip % 256 ))
	(( ip /= 256 ))
	(( ipx != hex[3-i] )) && err_exit "hex digit $((3-i)) not correct"
done	
unset x
x=010
(( x == 10 )) || err_exit 'leading zeros not ignored for arithmetic'
(( $x == 10 )) || err_exit 'leading zeros not ignored for arithmetic with $x'
typeset -i i=x
(( i == 10 )) || err_exit 'leading zeros not ignored for arithmetic assignment'
(( ${x:0:1} == 0 )) || err_exit 'leading zero should not be stripped for x:a:b'
c010=3
(( c$x  == 3 )) || err_exit 'leading zero with variable should not be stripped'
[[ $( ($SHELL -c '((++1))' 2>&1)2>/dev/null ) == *lvalue* ]] || err_exit "((--1)) not generating error message"
i=2
(( "22" == 22 )) || print err_exit "double quoted constants fail"
(( "2$i" == 22 )) || print err_exit "double quoted variables fail"
(( "18+$i+2" == 22 )) || print err_exit "double quoted expressions fail"
exit $((Errors))
