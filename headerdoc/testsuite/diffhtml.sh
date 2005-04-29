#!/bin/sh

cd $2
for i in $(find .) ; do
	NAME="$(echo $i | sed "s/^\.\///")"
	if [ -d $NAME ] ; then
		mkdir -p ../tmp/$NAME
	fi
	if [ -f $i ] ; then
		grep -v "\(Last Updated.*\)"  ../$1/$i > ../tmp/$i-verified
		grep -v "\(Last Updated.*\)"  ../$2/$i > ../tmp/$i-test
		diff -bBdu ../tmp/$i-verified ../tmp/$i-test > ../tmp/$i-diffhtml
		cat ../tmp/$i-diffhtml
		cat ../tmp/$i-diffhtml >> ../htmlchanges
	fi
done

