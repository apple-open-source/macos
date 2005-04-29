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

function home # id
{
	typeset IFS=: pwd=/etc/passwd
	set -o noglob
	if	[[ -f $pwd ]] && grep -c "^$1:" $pwd > /dev/null
	then	set -- $(grep "^$1:" $pwd)
		print -r -- "$6"
	else	print .
	fi
}

integer Errors=0
Command=$0
OLDPWD=/bin
if	[[ ~ != $HOME ]]
then	err_exit '~' not $HOME
fi
x=~
if	[[ $x != $HOME ]]
then	err_exit x=~ not $HOME
fi
x=x:~
if	[[ $x != x:$HOME ]]
then	err_exit x=x:~ not x:$HOME
fi
if	[[ ~+ != $PWD ]]
then	err_exit '~' not $PWD
fi
x=~+
if	[[ $x != $PWD ]]
then	err_exit x=~+ not $PWD
fi
if	[[ ~- != $OLDPWD ]]
then	err_exit '~' not $PWD
fi
x=~-
if	[[ $x != $OLDPWD ]]
then	err_exit x=~- not $OLDPWD
fi
for u in root Administrator
do	h=$(home $u)
	if	[[ $h != . ]]
	then	if	[[ ~$u != $h ]]
		then	err_exit "~$u not $h"
		fi
		x=~$u
		if	[[ $x != $h ]]
		then	err_exit "x=~$u not $h"
		fi
		break
	fi
done
x=~%%
if	[[ $x != '~%%' ]]
then	err_exit 'x='~%%' not '~%%
fi
x=~:~
if	[[ $x != "$HOME:$HOME" ]]
then	err_exit x=~:~ not $HOME:$HOME
fi
exit $((Errors))
