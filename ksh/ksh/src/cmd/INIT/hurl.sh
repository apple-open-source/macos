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
: copy http url data

command=hurl
agent="$command/2003-05-11 (AT&T Labs Research)"
verbose=0

case `(getopts '[-][123:xyz]' opt --xyz; echo 0$opt) 2>/dev/null` in
0123)	ARGV0="-a $command"
	USAGE=$'
[-?
@(#)$Id: hurl (AT&T Labs Research) 2003-05-11 $
]
'$USAGE_LICENSE$'
[+NAME?hurl - copy http url data]
[+DESCRIPTION?\bhurl\b copies the data for the \bhttp\b \aurl\a operand
	to the standard output. The \aurl\a must be of the form
	\bhttp://\b\ahost\a[\b:\b\aport\a]]\b/\b\apath\a. The default
	\aport\a is \b80\b.]
[+?\bhurl\b is a shell script that attempts to access the \aurl\a by
	these methods:]{
	[+/dev/tcp/\ahost\a\b/80\b?Supported by \bksh\b(1) and recent
		\bbash\b(1).]
	[+wget -q -O - \aurl\a?]
	[+curl -s -L -o - \aurl\a?]
}
[v:verbose?Verbose trace.]

url

[+SEE ALSO?\bcurl\b(1), \bwget\b(1)]
'
	while	getopts $ARGV0 "$USAGE" OPT
	do	case $OPT in
		v)	verbose=1 ;;
		esac
	done
	shift `expr $OPTIND - 1`
	case $# in
	1)	;;
	*)	OPTIND=0
		getopts $ARGV0 "$USAGE" OPT '-?'
		exit 2
		;;
	esac
	;;
*)	while	:
	do	case $# in
		0)	break ;;
		esac
		case $1 in
		--)	shift
			break
			;;
		-v|--v|--ve|--ver|--verb|--verbo|--verbos|--verbose)
			verbose=1
			;;
		-*)	echo "$command: $1: unknown option" >&2
			set x x
			break
			;;
		*)	break
			;;
		esac
		shift
	done
	case $# in
	1)	;;
	*)	echo "Usage: $command [ -v ] url" >&2
		exit 2
		;;
	esac
	;;
esac

url=$1

while	:
do	test 0 != $verbose && echo "$command: url=$url" >&2
	case $url in
	*://*/*)prot=${url%%:*}
		host=${url#*://}
		path=/${host#*/}
		host=${host%%/*}
		case $host in
		*:+([0-9]))
			port=${host##*:}
			host=${host%:*}
			;;
		*)	port=80
			;;
		esac
		;;
	*)	echo "$command: protocol://host/path expected" >&2
		exit 1
		;;
	esac
	test 0 != $verbose && echo "$command: prot=$prot host=$host port=$port path=$path" >&2
	case $prot in
	http)	if	(eval "exec >" || exit 0) 2>/dev/null &&
			eval "exec 8<> /dev/tcp/\$host/$port" 2>/dev/null
		then	test 0 != $verbose && echo "$command: using /dev/tcp/$host/$port" >&2
			if	! echo "GET $path HTTP/1.0
Host: $host
User-Agent: $agent
" >&8
			then	echo "$command: $host: write error"
				exit 1
			fi
			{
				if	! read prot code text
				then	echo "$command: $host: read error" >&2
					exit 1
				fi
				code=${code%:*}
				test 0 != $verbose && echo "$command: prot=$prot code=$code $text" >&2
				while	:
				do	if	! read head data
					then	echo "$command: $host: read error" >&2
						exit 1
					fi
					test 0 != $verbose && echo "$command: head=$head $data" >&2
					case $head in
					Location:)
						case $code in
						301|302)url=$data
							continue 2
							;;
						esac
						;;
					''|?)	break
						;;
					esac
				done
				case $code in
				200)	cat
					exit
					;;
				*)	echo "$0: $url: $code: $text" >&2
					exit 1
					;;
				esac
			} <&8 
		elif	wget -q -O - $url 2>/dev/null
		then	test 0 != $verbose && echo "$command: using wget" >&2
			exit
		elif	curl -s -L -o - $url 2>/dev/null
		then	test 0 != $verbose && echo "$command: using curl" >&2
			exit
		else	echo "$command: $url: { /dev/tcp/$host/$port wget curl } failed" >&2
			exit 1
		fi
		;;
	*)	echo "$command: $prot: protocol not supported" >&2
		exit 1
		;;
	esac
done
