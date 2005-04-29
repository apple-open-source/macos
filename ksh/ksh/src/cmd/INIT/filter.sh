####################################################################
#                                                                  #
#             This software is part of the ast package             #
#                Copyright (c) 1994-2004 AT&T Corp.                #
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
#               Glenn Fowler <gsf@research.att.com>                #
#                                                                  #
####################################################################
: convert command that operates on file args to pipeline filter

command=filter

tmp=/tmp/$command$$

case `(getopts '[-][123:xyz]' opt --xyz; echo 0$opt) 2>/dev/null` in
0123)	ARGV0="-a $command"
	USAGE=$'
[-?
@(#)$Id: filter (AT&T Labs Research) 2001-05-31 $
]
'$USAGE_LICENSE$'
[+NAME?filter - run a command in stdin/stdout mode]
[+DESCRIPTION?\bfilter\b runs \acommand\a in a mode that takes input from
	the \afile\a operands, or from the standard input if no \afile\a
	operands are specified, and writes the results to the standard output.
	It can be used to run commands like \bsplit\b(1), that normally modify
	\afile\a operands in-place, in pipelines. The \afile\a operands are
	not modified; \acommand\a is run on copies in \b/tmp\b.]

command [ option ... ] [ file ... ]

[+SEE ALSO?\bstrip\b(1)]
'
	;;
*)	ARGV0=""
	USAGE="command [ option ... ] [ file ... ]"
	;;
esac

usage()
{
	OPTIND=0
	getopts $ARGV0 "$USAGE" OPT '-?'
	exit 2
}

while	getopts $ARGV0 "$USAGE" OPT
do	case $OPT in
	*)	usage ;;
	esac
done
shift `expr $OPTIND - 1`
case $# in
0)	usage ;;
esac

cmd=$1
while	:
do	shift
	case $# in
	0)	break ;;
	esac
	case $1 in
	-*)	cmd="$cmd $1" ;;
	*)	break ;;
	esac
done
trap "rm -f $tmp" 0 1 2 3 15
case $# in
0)	cat > $tmp
	$cmd $tmp
	;;
*)	for file
	do	cp $file $tmp || exit 1
		chmod u+rwx $tmp || exit 1
		$cmd $tmp || exit 1
		cat $tmp
	done
	;;
esac
