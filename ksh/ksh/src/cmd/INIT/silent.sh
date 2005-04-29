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
# non-ksh stub for the nmake silent prefix
# @(#)silent (AT&T Research) 1992-08-11

case $-:$BASH_VERSION in
*x*:[01234567899]*)	: bash set -x is broken :; set +ex ;;
esac

while	:
do	case $# in
	0)	exit 0 ;;
	esac
	case $1 in
	*=*)	case $RANDOM in
		$RANDOM)`echo $1 | sed "s/\\([^=]*\\)=\\(.*\\)/eval \\1='\\2'; export \\1/"` ;;
		*)	export "$1" ;;
		esac
		shift
		;;
	*)	break
		;;
	esac
done
"$@"
