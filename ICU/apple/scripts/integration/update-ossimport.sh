#!/bin/bash

#=======================================================================================
# update-ossimport.sh
# created by Richard Gillam, 9/26/24
#
# This script creates a git commit that makes the directories we care about in
# $AICU_OSSIMPORT look exactly like the corresponding directories in $OSCLDR_NEW
# or $OSICU_NEW (except for a few spots where we explicitly change things)
# and spits out the commit hash for the new commit.
#
# This script takes one command-line parameter, which should be either 'icu' or 'cldr'.
# This is so we can update the ICU and CLDR directories as two different commits,
# allowing us to integrate them back to $AICU_WORK in two separate passes.
#=======================================================================================

# check to make sure the necessary environment variables have been set up
if [ ! -n $AICU_OSSIMPORT ]; then
	echo "AICU_OSSIMPORT is empty-- did you run set-up-environment.sh?"
	exit 1
fi

# figure out which directories we're synchronizing:
# - for CLDR, we're synchronizing $AICU_OSSIMPORT/cldr with $OSCLDR_NEW
#   and we're synchronizing every subdirectory under those directories, except for
#   docs, exemplars, and specs
# - for ICU, we're synchronizing $AICU_OSSIMPORT/icu with $OSICU_NEW,
#   but we're only synchronizing the icu4c/source and tools directories
if [ "$1" == "cldr" ]; then
	DIRECTORIES_TO_SKIP="docs exemplars specs"
	OSSIMPORT_SUBDIR=$AICU_OSSIMPORT/cldr
	SRC_DIR=$OSCLDR_NEW
	for f in $(ls $OSCLDR_NEW); do
		if [[ "$DIRECTORIES_TO_SKIP" != *"$f"* ]]; then
			DIRECTORIES_TO_COPY="$DIRECTORIES_TO_COPY $f"
		fi
	done
fi
if [ "$1" == "icu" ]; then
	DIRECTORIES_TO_COPY="icu4c/source tools"
	OSSIMPORT_SUBDIR=$AICU_OSSIMPORT/icu
	SRC_DIR=$OSICU_NEW
fi

# use the new DIRECTORIES_TO_COPY variable to tell whether the user specified which
# project to synchronize on the command line.  If they didn't, dump out with an error
if [ ! -n "$DIRECTORIES_TO_COPY" ]; then
	echo "You need to specify either cldr or icu on the command line"
	exit 1
fi

# for each subdirectory of the directories we're synchronizing, delete the version under
# $AICU_OSSIMPORT and copy over the one from $OSCLDR_NEW/$OSICU_NEW (we have to delete
# first so that we don't keep anything that's been deleted on the OS side)
for d in $DIRECTORIES_TO_COPY; do
	SRC="$SRC_DIR/$d"
	DEST="$OSSIMPORT_SUBDIR/$d"
	if [ -e $SRC ]; then
		rm -rf $DEST
		cp -R $SRC $DEST
	fi
done

# perform Apple-specific manual fixups.  There are the exceptions to the idea that we want
# the stuff under $AICU_OSSIMPORT to look exactly like the stuff in $OSICU_NEW or $OSCLDR_NEW.
# These are cases where we've moved stuff in our actual ICU project and making the corresponding
# moves in $AICU_OSSIMPORT makes syncing in the actual diffs easier.  The changes are:
# - In CLDR, rename ur_IN and ur_PK to ur_Arab_IN and ur_Arab_PK.  Edit each file to include
#   a <script> element right below the <language> element in the file (we're inserting it at
#   line 12, which will be the wrong place if the copyright header changes length, but we should
#   see that when reconciling merge conflicts).
# - In ICU, ulocdata.cpp and ulocdata.h move from the "i18n" tree to the "common" tree
if [ "$1" == "cldr" ]; then
	mv $OSSIMPORT_SUBDIR/common/main/ur_IN.xml $OSSIMPORT_SUBDIR/common/main/ur_Arab_IN.xml
	mv $OSSIMPORT_SUBDIR/common/main/ur_PK.xml $OSSIMPORT_SUBDIR/common/main/ur_Arab_PK.xml
	sed -i '' '12 i\
		<script type="Arab"/>\
' $OSSIMPORT_SUBDIR/common/main/ur_Arab_IN.xml
	sed -i '' '12 i\
		<script type="Arab"/>\
' $OSSIMPORT_SUBDIR/common/main/ur_Arab_PK.xml
fi
if [ "$1" == "icu" ]; then
	mv $OSSIMPORT_SUBDIR/icu4c/source/i18n/ulocdata.cpp $OSSIMPORT_SUBDIR/icu4c/source/common
	mv $OSSIMPORT_SUBDIR/icu4c/source/i18n/unicode/ulocdata.h $OSSIMPORT_SUBDIR/icu4c/source/common/unicode
fi

# for every file in $AICU_OSSIMPORT that "git status" shows as "deleted:",
# use "git rm" to add its deletion to the commit
pushd $AICU_OSSIMPORT
for f in $(git status | grep "deleted:" | sed "s/\t*deleted:\ *//"); do
	git rm $f
done

# then go back through all the directories we care about in $AICU_OSSIMPORT and
# do "git add" to add all the changed and new files to the commit ("git add" is recursive--
# when you apply it to a directory, it adds all the new and changed files in that directory)
for d in $DIRECTORIES_TO_COPY; do
	git add $OSSIMPORT_SUBDIR/$d
done

# actually commit the changes
if [ "$1" == "cldr" ]; then
	git commit -m "Bring cldr directory in ossImport branch up to match OS CLDR $OSCLDR_NEW_BRANCH"
fi
if [ "$1" == "icu" ]; then
	git commit -m "Bring icu directory in ossImport branch up to match OS ICU $OSICU_NEW_BRANCH"
fi

# and print out the commit message, with its hash
git log -1

popd
