#!/bin/bash

# This script depends on set-up-environment.sh having being run first, as it depends on
# environment variables that are defined there.  It also depends on $SCRIPTS/start-resolving.sh
# having been run first, as it sets up the source tree (and a number of housekeeping files)
# to begin the conflict-resolution process.
#
# Syntax: $SCRIPTS/resolve-next.sh ["skip"] ["repeat"]
#
# If you don't supply any parameters, this script does the following:
# - For the first file listed in $LOGS/conflicting-files.txt (if it's a real pathname):
#   - Check to make sure it doesn't contain any conflict markers (and dump out with an error
#     if it does)
#   - Add it to the pending commit
#   - Prompt you for a explanation of what changes you made to the file to resolve the conflicts
#     and add that explanation to the end of $LOGS/resolution-log.txt.  [It's helpful to keep
#     an open editor window where you type notes as you resolve and then copy-paste into the
#     terminal when prompted.]  This comment can be multiple lines long-- you end it with
#     Ctrl-D.  $LOGS/resolution-log.txt is updated in the pending commit.
#   - Remove it from $LOGS/conflicting-files.txt and update conflicting-files.txt
#     in the pending commit
# - For the NEW first file listed in $LOGS/conflicting-files.txt (if there is one):
#   - Open it in your preferred editor (as specified by $EDIT)
#   - Diff the $OSCLDR_OLD and $OSCLDR_NEW (or $OSICU_OLD and $OSICU_NEW, as appropriate)
#     versions of the file in your preferred diff tool (as specified by $DIFF)
#   - Diff the $OSCLDR_OLD and $ACLDR_NEW (or $OSICU_OLD and $AICU_NEW, as appropriate)
#     versions of the file in your preferred diff tool (as specified by $DIFF)
#
# If $LOGS/conflicting-files.txt is empty, you're done resolving conflicts-- the script
# will automatically git rm that file and $LOGS/resolve-mode.sh.
#
# If you specify "repeat" on the command line, only the second half of the tasks listed above
# happens (that is, it just opens the file listed first in $LOGS/conflicting-files.txt in your
# editor and diff tool).  This is useful for picking up where you left off after checking in
# work in progress.
#
# If you specify "skip" on the command line, the file listed first in $LOGS/conflicting-files.txt
# is moved to the end and the next file down is opened in your editor and diff tool.
# This is useful if the diff is complicated and you want to deal with easier files (or ones that'll
# shed light on what to do with the hard one) first.
#
# The intended workflow is something like this:
# 1. Start the conflict-resolution process by running $SCRIPTS/start-resolving.sh
# 2. Manually resolve any half-deleted files and commit the results.
# 3. Run $SCRIPTS/resolve-next.sh to resolve conflicts in the first file.
# 4. Manually resolve those conflicts in your editor, using the two open diff windows to help
#    decide what to do.  Keep track of what you did in a scratch file in a separate editor.
# 5. When you're done, save the file in your editor and run $SCRIPTS/resolve-next.sh again.
# 6. When prompted, copy the resolution notes from your scratch file into the terminal window
#    and hit Ctrl-D.
# 7. The next file will open in your editor and your diff tool.  If you want to keep going,
#    go back to step 4.
# 8. Every so often (e.g., at the end of the day), commit the accumulated changes.  To start
#    up the next morning (if you didn't leave the editor and diff tools open), do
#    "$SCRIPTS/resolve-next repeat" to reopen the editor and diff tool and pick up
#    where you left off.

if [ -z $AICU_WORK ]; then
	echo "AICU_WORK isn't defined-- did you run set-up-environment.sh?"
	exit 1
fi

CONFLICTING_FILES_LIST=$LOGS/conflicting_files.txt
RESOLUTION_LOG=$LOGS/resolution_log.txt
TMP=$LOGS/tmp

if [ ! -s $CONFLICTING_FILES_LIST ]; then
	echo "conflicting_files.txt is empty-- resolution is done!"
	git rm -f $CONFLICTING_FILES_LIST
	git rm -f $LOGS/resolve-mode.sh
	exit 0
fi

cd $AICU_WORK

OLD_WORKING_FILE=`head -n +1 $CONFLICTING_FILES_LIST`

if [ "$1" = "skip" ]; then
	tail -n +2 $CONFLICTING_FILES_LIST >$TMP
	echo $OLD_WORKING_FILE >>$TMP
	cp $TMP $CONFLICTING_FILES_LIST
	rm $TMP
elif [ "$OLD_WORKING_FILE" != "(Dummy first file)" -a "$1" != "repeat" ]; then
	GREP_RESULT=`grep "<<<\|>>>>" $OLD_WORKING_FILE`
	if [ "$GREP_RESULT" ]; then
		echo "$OLD_WORKING_FILE still has conflict markers in it!  Did you remember to save?"
		exit 1
	fi
	
	echo "Type a resolution message for $OLD_WORKING_FILE followed by Ctrl-D:"
	echo "$OLD_WORKING_FILE:" >>$RESOLUTION_LOG
	cat >>$RESOLUTION_LOG
	
	git add $OLD_WORKING_FILE
	git add $RESOLUTION_LOG
fi
	
if [ "$1" != "repeat" ]; then
	tail -n +2 $CONFLICTING_FILES_LIST >$TMP
	cp $TMP $CONFLICTING_FILES_LIST
	rm $TMP
	git add $CONFLICTING_FILES_LIST
fi

if [ -f $LOGS/resolve-mode.sh ]; then
	source $LOGS/resolve-mode.sh
fi

if [ ! -s $CONFLICTING_FILES_LIST ]; then
	echo "conflicting_files.txt is empty-- resolution is done!"
	git rm -f $CONFLICTING_FILES_LIST
	git rm -f $LOGS/resolve-mode.sh
	exit 0
fi

NEW_WORKING_FILE=`head -n +1 $CONFLICTING_FILES_LIST`

if [ $RESOLVE_MODE = "icu" ]; then
	NEW_WORKING_FILE_FOR_DIFF=${NEW_WORKING_FILE#icu/}
	NEW_WORKING_FILE_FOR_DIFF2=${NEW_WORKING_FILE#icu/icu4c/source/}
	$DIFF $OSICU_OLD/$NEW_WORKING_FILE_FOR_DIFF $OSICU_NEW/$NEW_WORKING_FILE_FOR_DIFF
	$DIFF $OSICU_OLD/$NEW_WORKING_FILE_FOR_DIFF $AICU_NEW/$NEW_WORKING_FILE_FOR_DIFF2
	$EDIT $AICU_WORK/$NEW_WORKING_FILE
else
	NEW_WORKING_FILE_FOR_DIFF=${NEW_WORKING_FILE#cldr/}
	$DIFF $OSCLDR_OLD/$NEW_WORKING_FILE_FOR_DIFF $OSCLDR_NEW/$NEW_WORKING_FILE_FOR_DIFF
	$DIFF $OSCLDR_OLD/$NEW_WORKING_FILE_FOR_DIFF $ACLDR_NEW/$NEW_WORKING_FILE_FOR_DIFF
	$EDIT $AICU_WORK/$NEW_WORKING_FILE
fi
