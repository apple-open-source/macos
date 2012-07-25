########################################################################
#                                                                      #
#               This software is part of the ast package               #
#                     Copyright (c) 1994-2011 AT&T                     #
#                      and is licensed under the                       #
#                  Common Public License, Version 1.0                  #
#                               by AT&T                                #
#                                                                      #
#                A copy of the License is available at                 #
#            http://www.opensource.org/licenses/cpl1.0.txt             #
#         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         #
#                                                                      #
#              Information and Software Systems Research               #
#                            AT&T Research                             #
#                           Florham Park NJ                            #
#                                                                      #
#                 Glenn Fowler <gsf@research.att.com>                  #
#                                                                      #
########################################################################
# non-ksh script for the nmake ignore prefix
# @(#)ignore (AT&T Research) 1992-08-11

case $-:$BASH_VERSION in
*x*:[0123456789]*)	: bash set -x is broken :; set +ex ;;
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
exit 0
