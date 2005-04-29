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
mkdir /tmp/ksh$$
cd /tmp/ksh$$
trap "PATH=$PATH; cd /; rm -rf /tmp/ksh$$" EXIT
cat > bug1 <<- \EOF
	print print ok > /tmp/ok$$
	/bin/chmod 755 /tmp/ok$$
	trap 'cd /; rm -f /tmp/ok$$' EXIT
	function a
	{
	        typeset -x PATH=/tmp
	        ok$$
	}
	path=$PATH
	unset PATH
	a
	PATH=$path
}
EOF
[[ $($SHELL ./bug1  2> /dev/null) == ok ]]  || err_exit "PATH in function not working"
cat > bug1 <<- \EOF
	function lock_unlock
	{
	typeset PATH=/usr/bin
	typeset -x PATH=''
	}
	
	PATH=/usr/bin
	: $(PATH=/usr/bin getconf PATH)
	typeset -ft lock_unlock
	lock_unlock
EOF
($SHELL ./bug1)  2> /dev/null || err_exit "path_delete bug"
mkdir tdir$$
if	$SHELL tdir$$ > /dev/null 2>&1
then	err_exit 'not an error to run ksh on a directory'
fi

print 'print hi' > ls
if	[[ $($SHELL ls 2> /dev/null) != hi ]]
then	err_exit "$SHELL name not executing version in current directory"
fi
pwd=$PWD
# get rid of leading and trailing : and trailing :.
PATH=${PATH%.}
PATH=${PATH%:}
PATH=${PATH#.}
PATH=${PATH#:}
path=$PATH
var=$(whence date)
dir=$(basename "$var")
for i in 1 2 3 4 5 6 7 8 9 0
do	if	! whence notfound$i 2> /dev/null
	then	cmd=notfound$i		
		break
	fi
done
print 'print hello' > date
chmod +x date
print 'print notfound' >  $cmd
chmod +x "$cmd"
> foo
chmod 755 foo
for PATH in $path :$path $path: .:$path $path: $path:. $PWD::$path $PWD:.:$path $path:$PWD $path:.:$PWD
do	
#	print path=$PATH $(whence date)
#	print path=$PATH $(whence "$cmd")
		date
		"$cmd"
done > /dev/null 2>&1
builtin -d date 2> /dev/null
if	[[ $(PATH=:/usr/bin; date) != 'hello' ]]
then	err_exit "leading : in path not working"
fi
(
	PATH=$PWD:
	builtin chmod
	print 'print cannot execute' > noexec
	chmod 644 noexec
	if	[[ ! -x noexec ]]
	then	noexec > /dev/null 2>&1
	else	exit 126
	fi
)
[[ $? == 126 ]] || err_exit 'exit status of non-executable is not 126' 
builtin -d rm 2> /dev/null
rm=$(whence rm)
d=$(dirname "$rm")
unset FPATH
PATH=/dev/null
if	date > /dev/null 2>&1
then	err_exit 'programs in . should not be found'
fi
[[ $(whence ./foo) != "$PWD/"./foo ]] && err_exit 'whence ./foo not working'
[[ $(whence "$PWD/foo") != "$PWD/foo" ]] && err_exit 'whence $PWD/foo not working'
[[ $(whence ./xxxxx) ]] && err_exit 'whence ./xxxx not working'
PATH=$d:
cp "$rm" kshrm$$
if	[[ $(whence kshrm$$) != kshrm$$  ]]
then	err_exit 'trailing : in pathname not working'
fi
cp "$rm" rm
PATH=:$d
if	[[ $(whence rm) != rm ]]
then	err_exit 'leading : in pathname not working'
fi
PATH=$d: whence rm > /dev/null
if	[[ $(whence rm) != rm ]]
then	err_exit 'pathname not restored after scoping'
fi
cd /
if	whence ls > /dev/null
then	PATH=
	if	[[ $(whence rm) ]]
	then	err_exit 'setting PATH to Null not working'
	fi
	unset PATH
	if	[[ $(whence rm) != /*rm ]]
	then	err_exit 'unsetting path  not working'
	fi
fi
PATH=/dev:/tmp/ksh$$
x=$(whence rm)
typeset foo=$(PATH=/xyz:/abc :)
y=$(whence rm)
[[ $x != "$y" ]] && err_exit 'PATH not restored after command substitution'
PATH=$(getconf PATH)
x=$(whence ls)
PATH=.:$PWD:${x%/ls}
[[ $(whence ls) == "$x" ]] || err_exit 'PATH search bug when .:$PWD in path'
PATH=$PWD:.:${x%/ls}
[[ $(whence ls) == "$x" ]] || err_exit 'PATH search bug when :$PWD:. in path'
exit $((Errors))
