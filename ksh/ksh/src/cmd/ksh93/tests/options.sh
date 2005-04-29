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
if	[[ $( ${SHELL-ksh} -s hello<<-\!
		print $1
		!
	    ) != hello ]]
then	err_exit "${SHELL-ksh} -s not working"
fi
x=$(
	set -e
	false && print bad
	print good
)
if	[[ $x != good ]]
then	err_exit 'sh -e not workuing'
fi
[[ $($SHELL -D -c 'print hi; print $"hello"') == '"hello"' ]] || err_exit 'ksh -D not working'
if	command set -G 2> /dev/null
then	mkdir /tmp/ksh$$
	cd /tmp/ksh$$
	mkdir bar foo
	> bar.c  > bam.c
	> bar/foo.c > bar/bam.c
	> foo/bam.c
	set -- **.c
	[[ $* == 'bam.c bar.c' ]] || err_exit '**.c not working with -G option'
	set -- **
	[[ $* == 'bam.c bar bar.c bar/bam.c bar/foo.c foo foo/bam.c' ]] || err_exit '** not working with -G option'
	set -- **/*.c
	[[ $* == 'bam.c bar.c bar/bam.c bar/foo.c foo/bam.c' ]] || err_exit '**/*.c not working with -G option'
	set -- **/bam.c
	[[ $* == 'bam.c bar/bam.c foo/bam.c' ]] || err_exit '**/bam.c not working with -G option'
	cd ~-
	rm -rf /tmp/ksh$$
fi
exit $((Errors))
