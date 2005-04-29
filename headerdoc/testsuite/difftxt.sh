#!/bin/sh

# Uncomment if you're getting inexplicable hangs with "www".
# (Why didn't it send the error message to stderr!?!
# rm -f /tmp/w3c-cache/.lock

LYNX="/sw/bin/lynx"
LYNXB="/usr/local/bin/lynx"
# WWW="/sw/bin/www -list"

if [ ! -x "$LYNX" ] ; then
	if [ ! -x "$LYNXB" ] ; then
		echo "Sorry.  The HeadaerDoc regression test suite requires"
		echo "   lynx to be installed.  You can obtain lynx from:"
		echo "               http://lynx.isc.org"

		exit -1;
	fi
	LYNX="$LYNXB"
fi

HTMLTOTEXT="$LYNX -dump -nolist"
# HTMLTOTEXT="$WWW"

# LYNXPATTERN='^ *.*\. file:\/\/localhost'
WWWPATTERN='^\[.*\] file:'
# REFPATTERN="$LYNXPATTERN"

cd $1
FILES="$(find .)";
cd ..

for i in $FILES ; do
	NAME="$(echo $i | sed "s/^\.\///")"
	# echo "PROCESSING $NAME";
	if [ -d "$1/$NAME" ] ; then
		mkdir -p ./tmp/$NAME
	else
	    if [ -f "$1/$NAME" ] ; then
		# echo "$NAME:"
		# echo $NAME ../tmp/$NAME-test ../$2/$NAME ../tmp/$NAME-test
		# echo "$HTMLTOTEXT $1/$NAME > ./tmp/$NAME-verified"

		# Convert to text and delete links references.
		mkdir -p "./tmp/$(dirname $NAME)"
		cp $1/$NAME ./tmp/$NAME
		$HTMLTOTEXT ./tmp/$NAME | grep -v "\(Last Updated .*\)" > ./tmp/$NAME-verified
		cp $2/$NAME ./tmp/$NAME
		$HTMLTOTEXT ./tmp/$NAME | grep -v "\(Last Updated .*\)" > ./tmp/$NAME-test
		diff -bBdu --ignore-all-space ./tmp/$NAME-verified ./tmp/$NAME-test > ./tmp/$NAME-txtdiff
		rm ./tmp/$NAME
		STRING="$(cat ./tmp/$NAME-txtdiff)"
		if [ "X$STRING" != "X" ] ; then
			echo "Diffing $NAME"
			cat "./tmp/$NAME-txtdiff"
		fi
		cat "./tmp/$NAME-txtdiff" >> textchanges
		# rm ./tmp/$NAME-verified ./tmp/$NAME-test
	    else
		echo "Could not process file $1/NAME";
	    fi
	fi
done

