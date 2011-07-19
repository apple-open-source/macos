#!/bin/sh

SOURCEDIR="$PWD"/sourcefiles
if [ "$RESOLVELINKS" = "" ] ; then
	RESOLVELINKS="$PWD/../../xmlman/resolveLinks"
fi
export UPDATE_SCRIPT="$PWD/update.sh"

if [ ! -f "$RESOLVELINKS" ] ; then
	RESOLVELINKS="$(which resolveLinks)";
	if [ "$RESOLVELINKS" = "" ] ; then
		echo "Could not find resolvelinks.  Please specify the path via the"
		echo "RESOLVELINKS environment variable."
		exit 1
	fi
fi

# echo "USING $RESOLVELINKS"

PROMPT=0

OKCOUNT=0
FAILCOUNT=0

if [ "$1" = "--update" ] ; then
	PROMPT=1
	shift
fi

FROMPERL=0
if [ "$1" = "--fromperl" ] ; then
	FROMPERL=1
	shift
fi

if [ "$DEBUG" = "" ] ; then
	DEBUG=0
fi

if [ "$UPDATE" = "" ] ; then
	UPDATE=0;
fi

MADEDIR=0

if [ ! -d /private/tmp ] ; then
	mkdir -p /private/tmp

	if [ $? != 0 ] ; then
		echo "Directory /private/tmp does not exist and could not create."
		echo "The test suite requires this directory to work correctly."
		echo "Please create it and make it writable."
		exit 1
	fi

	MADEDIR=1
fi

runtest()
{
	local TESTFILE="$1"
	local NAME="$(echo "$TESTFILE" | sed 's/\.rltest$//')"
	local RETVAL=0

	if [ $DEBUG = 1 ] ; then
		echo "Loading test in debug mode.";
	fi

	OLDDIR="$PWD"
	cd /private/tmp

	. "$TESTFILE"

	if [ $DEBUG = 1 ] ; then
		echo "Copying source files from \"$SOURCEDIR\".";
	fi
	rm -rf /private/tmp/headerdoc_temptestdir
	cp -r "$SOURCEDIR" /private/tmp/headerdoc_temptestdir
	# find /private/tmp/headerdoc_temptestdir -name CVS -exec rm -rf {} \;

	if [ $DEBUG = 1 ] ; then
		echo "Running command \"$COMMAND\".";
	fi

	eval "$COMMAND" 2>&1 | grep -v 'For a detailed resolver report' > /private/tmp/headerdoc_temptestdir/testout

	cd "$OLDDIR"

	if [ -f /tmp/xref_out ] ; then
		mv /tmp/xref_out /private/tmp/headerdoc_temptestdir
	fi


	# End of processing.

	if [ $UPDATE = 1 ] ; then
		exit 0
	fi

	if [ $DEBUG = 1 -o $PROMPT = 1 ] ; then
		diff -ru -x CVS "$NAME.expected" /private/tmp/headerdoc_temptestdir
		RETVAL=$?
	else
		diff -rq -x CVS "$NAME.expected" /private/tmp/headerdoc_temptestdir
		RETVAL=$?
	fi

	if [ $DEBUG = 1 ] ; then
		echo "Waiting for user to press return."
		read BOGUS
	elif [ $PROMPT = 1 -a $RETVAL != 0 ] ; then
		echo "Confirm? (Y/N)"
		read CONFIRM
		if [ "$CONFIRM" = "y" -o "$CONFIRM" = "Y" ] ; then
			$UPDATE_SCRIPT "$NAME" > /dev/null 2>&1
			if [ $? != 0 ] ; then
				echo "Update failed"
				RETVAL=1
			else
				RETVAL=0
			fi
		fi
	fi
	rm -rf /private/tmp/headerdoc_temptestdir;

	return $RETVAL;
}

EXITSTATUS=0
if [ $# = 1 ] ; then
	cd tests
	if [ "$(echo "$1" | grep '^\/')" != "" ] ; then
		runtest "$1"
	else
		runtest "$PWD/$1"
	fi
	if [ $? = 0 ] ; then
		printf "\033[32mOK\033[39m\n"
	else
		printf "\033[31mFAIL\033[39m\n"
		EXITSTATUS=1
	fi
	cd ..

	exit $EXITSTATUS;
fi

cd tests
for i in *.rltest ; do
	NAME="$(echo "$i" | sed 's/\.rltest$//')"
	printf "%s: " "$NAME"

	runtest "$PWD/$i"
	if [ $? = 0 ] ; then
		printf "\033[32mOK\033[39m\n"
		OKCOUNT="$(expr "$OKCOUNT" '+' "1")"
	else
		printf "\033[31mFAIL\033[39m\n"
		FAILCOUNT="$(expr "$FAILCOUNT" '+' "1")"
		EXITSTATUS=1
	fi
done
cd ..

TOTAL="$(expr "$OKCOUNT" '+' "$FAILCOUNT")"

if [ $OKCOUNT -eq $TOTAL ] ; then
	printf "Passed: \033[32m$OKCOUNT/$TOTAL\033[39m\n";
else
	printf "Passed: \033[31m$OKCOUNT/$TOTAL\033[39m\n";
	printf "Failed: \033[31m$FAILCOUNT/$TOTAL\033[39m\n";
fi

if [ $FROMPERL = 1 ] ; then
	echo "PERLSTAT RESOLVELINKS: $OKCOUNT $FAILCOUNT";
fi

if [ $MADEDIR = 1 ] ; then
	rmdir /private/tmp
	rmdir /private
fi

exit $EXITSTATUS;
