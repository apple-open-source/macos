#!/bin/sh

if [ "$1" = "" ] ; then
	find tests -exec ./gitfixup.sh {} \;
	find sourcefiles -exec ./gitfixup.sh {} \;
else
	FILE="$1"

	echo "FILE: $FILE"

	# Skip the CVS directory and everything in it.
	## if [ "$(echo "$FILE" | grep '\/CVS$')" != "" ] ; then exit 0; fi
	## if [ "$(echo "$FILE" | grep '\/CVS\/')" != "" ] ; then exit 0; fi

	DIR="$(dirname "$FILE")"
	FILENAME="$(basename "$FILE")"

	# FILENAMEQUOT="$(echo "$FILENAME" | sed 's/\//\\\//g')"

	## if [ "$(grep "$FILENAMEQUOT" "$DIR/CVS/Entries")" = "" ] ; then

	if ! git ls-files "$FILE" --error-unmatch > /dev/null 2>&1 ; then
		echo "MISSING: $FILENAME"
		# cvs add "$FILE"
		git add "$FILE"

		# if [ -d "$TESTPATH".expected/ ] ; then
			# cd "$TESTPATH".expected/
			# find . -name CVS -exec mv {} /tmp/headerdoc_temptestdir/{} \; > /dev/null 2>&1
			# cd ../..
		# fi
	fi

fi

