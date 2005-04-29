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
	print -u2 -r ${Command}[$1]: "${@:2}"
	let Errors+=1
}
alias err_exit='err_exit $LINENO'

Command=$0
integer Errors=0 j=4
base=/home/dgk/foo//bar
string1=$base/abcabcabc
if	[[ ${string1:0} != "$string1" ]]
then	err_exit "string1:0"
fi
if	[[ ${string1: -1} != "c" ]]
then	err_exit "string1: -1"
fi
if	[[ ${string1:0:1000} != "$string1" ]]
then	err_exit "string1:0"
fi
if	[[ ${string1:1} != "${string1#?}" ]]
then	err_exit "string1:1"
fi
if	[[ ${string1:1:4} != home ]]
then	err_exit "string1:1:4"
fi
if	[[ ${string1: -5:4} != bcab ]]
then	err_exit "string1: -5:4"
fi
if	[[ ${string1:1:j} != home ]]
then	err_exit "string1:1:j"
fi
if	[[ ${string1:(j?1:0):j} != home ]]
then	err_exit "string1:(j?1:0):j"
fi
if	[[ ${string1%*zzz*} != "$string1" ]]
then	err_exit "string1%*zzz*"
fi
if	[[ ${string1%%*zzz*} != "$string1" ]]
then	err_exit "string1%%*zzz*"
fi
if	[[ ${string1#*zzz*} != "$string1" ]]
then	err_exit "string1#*zzz*"
fi
if	[[ ${string1##*zzz*} != "$string1" ]]
then	err_exit "string1##*zzz*"
fi
if	[[ ${string1%+(abc)} != "$base/abcabc" ]]
then	err_exit "string1%+(abc)"
fi
if	[[ ${string1%%+(abc)} != "$base/" ]]
then	err_exit "string1%%+(abc)"
fi
if	[[ ${string1%/*} != "$base" ]]
then	err_exit "string1%/*"
fi
if	[[ "${string1%/*}" != "$base" ]]
then	err_exit '"string1%/*"'
fi
if	[[ ${string1%"/*"} != "$string1" ]]
then	err_exit 'string1%"/*"'
fi
if	[[ ${string1%%/*} != "" ]]
then	err_exit "string1%%/*"
fi
if	[[ ${string1#*/bar} != /abcabcabc ]]
then	err_exit "string1#*bar"
fi
if	[[ ${string1##*/bar} != /abcabcabc ]]
then	err_exit "string1#*bar"
fi
if	[[ "${string1#@(*/bar|*/foo)}" != //bar/abcabcabc ]]
then	err_exit "string1#@(*/bar|*/foo)"
fi
if	[[ ${string1##@(*/bar|*/foo)} != /abcabcabc ]]
then	err_exit "string1##@(*/bar|*/foo)"
fi
if	[[ ${string1##*/@(bar|foo)} != /abcabcabc ]]
then	err_exit "string1##*/@(bar|foo)"
fi
foo=abc
if	[[ ${foo#a[b*} != abc ]]
then	err_exit "abc#a[b*} != abc"
fi
if	[[ ${foo//[0-9]/bar} != abc ]]
then	err_exit '${foo//[0-9]/bar} not expanding correctly'
fi
foo='(abc)'
if	[[ ${foo#'('} != 'abc)' ]]
then	err_exit "(abc)#( != abc)"
fi
if	[[ ${foo%')'} != '(abc' ]]
then	err_exit "(abc)%) != (abc"
fi
foo=a123b456c
if	[[ ${foo/[0-9]?/""} != a3b456c ]]
then	err_exit '${foo/[0-9]?/""} not expanding correctly'
fi
if	[[ ${foo//[0-9]/""} != abc ]]
then	err_exit '${foo//[0-9]/""} not expanding correctly'
fi
if	[[ ${foo/#a/b} != b123b456c ]]
then	err_exit '${foo/#a/b} not expanding correctly'
fi
if	[[ ${foo/#?/b} != b123b456c ]]
then	err_exit '${foo/#?/b} not expanding correctly'
fi
if	[[ ${foo/%c/b} != a123b456b ]]
then	err_exit '${foo/%c/b} not expanding correctly'
fi
if	[[ ${foo/%?/b} != a123b456b ]]
then	err_exit '${foo/%?/b} not expanding correctly'
fi
while read -r pattern string expected
do	if	(( expected ))
	then	if	[[ $string != $pattern ]]
		then	err_exit "$pattern does not match $string"
		fi
		if	[[ ${string##$pattern} != "" ]]
		then	err_exit "\${$string##$pattern} not null"
		fi
		if	[ "${string##$pattern}" != '' ]
		then	err_exit "\"\${$string##$pattern}\" not null"
		fi
		if	[[ ${string/$pattern} != "" ]]
		then	err_exit "\${$string/$pattern} not null"
		fi
	else	if	[[ $string == $pattern ]]
		then	err_exit "$pattern matches $string"
		fi
	fi
done <<- \EOF
	+(a)*+(a)	aabca	1
	!(*.o)		foo.o	0
	!(*.o)		foo.c	1
EOF
xx=a/b/c/d/e
yy=${xx#*/}
if	[[ $yy != b/c/d/e ]]
then	err_exit '${xx#*/} != a/b/c/d/e when xx=a/b/c/d/e'
fi
if	[[ ${xx//\//\\} != 'a\b\c\d\e' ]]
then	err_exit '${xx//\//\\} not working'
fi
x=[123]def
if	[[ "${x//\[(*)\]/\{\1\}}" != {123}def ]]
then	err_exit 'closing brace escape not working'
fi
unset foo
foo=one/two/three
if	[[ ${foo//'/'/_} != one_two_three ]]
then	err_exit 'single quoting / in replacements failed'
fi
if	[[ ${foo//"/"/_} != one_two_three ]]
then	err_exit 'double quoting / in replacements failed'
fi
if	[[ ${foo//\//_} != one_two_three ]]
then	err_exit 'escaping / in replacements failed'
fi
function myexport 
{
	nameref var=$1
	if	(( $# > 1 ))
	then	export	$1=$2
	fi
	if	(( $# > 2 ))
	then	print $(myexport "$1" "$3" )
		return
	fi
	typeset val
	val=$(export | grep "^$1=")
	print ${val#"$1="}
	
}
export dgk=base
if	[[ $(myexport dgk fun) != fun ]]
then	err_exit 'export inside function not working'
fi
val=$(export | grep "^dgk=")
if	[[ ${val#dgk=} != base ]]
then	err_exit 'export not restored after function call'
fi
if	[[ $(myexport dgk fun fun2) != fun2 ]]
then	err_exit 'export inside function not working with recursive function'
fi
val=$(export | grep "^dgk=")
if	[[ ${val#dgk=} != base ]]
then	err_exit 'export not restored after recursive function call'
fi
if	[[ $(dgk=try3 myexport dgk) != try3 ]]
then	err_exit 'name=value not added to export list with function call'
fi
val=$(export | grep "^dgk=")
if	[[ ${val#dgk=} != base ]]
then	err_exit 'export not restored name=value function call'
fi
unset zzz
if	[[ $(myexport zzz fun) != fun ]]
then	err_exit 'export inside function not working for zzz'
fi
if	[[ $(export | grep "zzz=") ]]
then	err_exit 'zzz exported after function call'
fi
set -- foo/bar bam/yes last/file/done
if	[[ ${@/*\/@(*)/${.sh.match[1]}} != 'bar yes done' ]]
then	err_exit '.sh.match not working with $@'
fi
if	[[ ${@/*\/@(*)/\1} != 'bar yes done' ]]
then	err_exit '\1 not working with $@'
fi
var=(foo/bar bam/yes last/file/done)
if	[[ ${var[@]/*\/@(*)/${.sh.match[1]}} != 'bar yes done' ]]
then	err_exit '.sh.match not working with ${var[@]}'
fi
if	[[ ${var[@]/*\/@(*)/\1} != 'bar yes done' ]]
then	err_exit '\1 not working with ${var[@]}'
fi
var='abc_d2ef.462abc %%'
if	[[ ${var/+(\w)/Q} != 'Q.462abc %%' ]]
then	err_exit '${var/+(\w)/Q} not workding'
fi
if	[[ ${var//+(\w)/Q} != 'Q.Q %%' ]]
then	err_exit '${var//+(\w)/Q} not workding'
fi
if	[[ ${var//+(\S)/Q} != 'Q Q' ]]
then	err_exit '${var//+(\S)/Q} not workding'
fi
if	[[ "$(LC_ALL=debug $SHELL <<- \+EOF+
		x=a<2bc><3xyz>g
		print ${#x}
		+EOF+)" != 4
	]]
then	err_exit '${#x} not working with multibyte locales'
fi
foo='foo+bar+'
[[ $(print -r -- ${foo//+/'|'}) != 'foo|bar|' ]] && err_exit "\${foobar//+/'|'}"
[[ $(print -r -- ${foo//+/"|"}) != 'foo|bar|' ]] && err_exit '${foobar//+/"|"}'
[[ $(print -r -- "${foo//+/'|'}") != 'foo|bar|' ]] && err_exit '"${foobar//+/'"'|'"'}"'
[[ $(print -r -- "${foo//+/"|"}") != 'foo|bar|' ]] && err_exit '"${foobar//+/"|"}"'
unset x
x=abcedfg
: ${x%@(d)f@(g)}
[[ ${.sh.match[0]} == dfg ]] || err_exit '.sh.match[0] not dfg'
[[ ${.sh.match[1]} == d ]] || err_exit '.sh.match[1] not d'
[[ ${.sh.match[2]} == g ]] || err_exit '.sh.match[2] not g'
x=abcedddfg
: ${x%%+(d)f@(g)}
[[ ${.sh.match[1]} == ddd ]] || err_exit '.sh.match[1] not ddd'
unset a b
a='\[abc @(*) def\]'
b='[abc 123 def]'
[[ ${b//$a/\1} == 123 ]] || err_exit "\${var/pattern} not working with \[ in pattern"
unset X
$SHELL -c '[[ ! ${X[@]:0:300} ]]' 2> /dev/null || err_exit '${X[@]:0:300} with X undefined fails'
$SHELL -c '[[ ${@:0:300} == "$0" ]]' 2> /dev/null || err_exit '${@:0:300} with no arguments fails'
i=20030704
[[ ${i#{6}(?)} == 04 ]] ||  err_exit '${i#{6}(?)} not working'
[[ ${i#{6,6}(?)} == 04 ]] ||  err_exit '${i#{6,6}(?)} not working'
LC_ALL=posix
i="   ."
[[ $(printf "<%s>\n" ${i#' '}) == '<.>' ]] || err_exit 'printf "<%s>\n" ${i#' '} failed'
unset x
x=foo
[[ "${x%o}(1)" == "fo(1)" ]] ||  err_exit 'print ${}() treated as pattern'
exit $((Errors))
