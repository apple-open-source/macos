#! /bin/sh
#
# $Id: find_in_path.sh,v 1.1.1.1 2000/06/10 00:40:47 wsanchez Exp $
#
EX_OK=0
EX_NOT_FOUND=1

ifs="$IFS"; IFS="${IFS}:"
for dir in $PATH /usr/5bin /usr/ccs/bin
do
	if [ -r $dir/$1 ]
	then
		echo $dir/$1
		exit $EX_OK
	fi
done
IFS=$ifs

exit $EX_NOT_FOUND
