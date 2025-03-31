#!/bin/bash

# HOW TO USE THIS SCRIPT
#
# This script depends on set-up-environment.sh having being run first, as it depends on
# environment variables that are defined there.
#
# Syntax: $SCRIPTS/start-resolving.sh <commit-hash> [<"cldr" or "icu">]
#
# <commit-hash> is the hash for a commit in the ossImport branch that should either be
# updating ossImport's CLDR files to a new version of updating ossImport's ICU files to
# a new version.  This parameter is required.
#
# <"cldr" or "icu"> is either the literal string "cldr" or the literal string "icu",
# and specifies whether the commit hash being integrated is the CLDR or ICU files.
# This parameter is optional; if not specified (or some other value), it defaults to "cldr".
#
# This script cherry-picks the specified commit from the ossImport branch into the current
# working branch and stages that cherry-pick as a pending commit.  If the cherry-pick has
# merge conflicts (which it inevitably will), a list of all of the conflicting files is saved
# in $LOGS/conflicting-files.txt and all of those files are added to the pending commit
# (with the conflict markers sill in place).  Since the resolution process usually takes days
# or weeks and multiple engineers may be involved, we need to be able to check in
# work-in-progress during the integration process.
#
# The script also sets up $LOGS/resolve-mode.sh, which keeps track of whether the CLDR
# of ICU tree is being resolved, and $LOGS/resolution-log.txt, which will keep track of
# what you did to each file during the resolution process.  (These files  and
# $LOGS/conflicting-files.txt are used by $SCRIPTS/resolve-next.sh.)  These files are also
# added to the pending commit.
#
# After running this script, you generally should commit and push the changes to your working
# branch, manually resolve any half-deleted files (files that one branch deleted and the other
# branch changed), and then run $SCRIPTS/resolve-next.sh to begin resolving merge conflicts.

if [ -z $1 ]; then
	echo "You need to specify a commit hash"
	exit 1
fi

if [ -z $AICU_WORK ]; then
	echo "AICU_WORK isn't defined-- did you run set-up-environment.sh?"
	exit 1
fi

COMMIT_HASH=$1
CHERRY_PICK_LOG=$LOGS/cherry-pick-$COMMIT_HASH-log.txt
MODIFIED_FILES=$LOGS/modified-files-$COMMIT_HASH.txt
CONFLICTING_FILES_LIST=$LOGS/conflicting_files.txt
HALF_DELETED_FILES_LIST=$LOGS/half_deleted.txt

MODE=$2
if [ -z $MODE ]; then
	MODE=cldr
fi

cd $AICU_WORK
git cherry-pick -n $COMMIT_HASH >$CHERRY_PICK_LOG
git status >$MODIFIED_FILES

# if we're resolving the ICU tree, after doing the cherry-pick, we just do
# "git checkout --theirs" on a bunch of files and directories that will have
# to be re-generated in subsequent steps (such as the ICU data directory, which
# will be re-generated from CLDR, and a number of Unicode properties files,
# which get re-generated after we manually sync their source files)
if [ $MODE = "icu" ]; then
	FILES_TO_SKIP="\
		icu/icu4c/source/common/localefallback_data.h \
		icu/icu4c/source/common/ubidi_props_data.h \
		icu/icu4c/source/common/ucase_props_data.h \
		icu/icu4c/source/common/uchar_props_data.h \
		icu/icu4c/source/data/in/ubidi.icu \
		icu/icu4c/source/data/in/ucase.icu \
		icu/icu4c/source/data/in/ulayout.icu \
		icu/icu4c/source/data/in/uprops.icu \
		icu/icu4c/source/data/brkitr \
		icu/icu4c/source/data/coll \
		icu/icu4c/source/data/curr \
		icu/icu4c/source/data/lang \
		icu/icu4c/source/data/locales \
		icu/icu4c/source/data/misc \
		icu/icu4c/source/data/rbnf \
		icu/icu4c/source/data/region \
		icu/icu4c/source/data/translit \
		icu/icu4c/source/data/unidata \
		icu/icu4c/source/data/unit \
		icu/icu4c/source/data/zone \
	"
	
	for f in $FILES_TO_SKIP; do
		echo "Auto-resolving $f"
		git checkout --theirs $f
		git add $f
	done
	
	git status >$MODIFIED_FILES
fi

echo `grep "\tmodified" $MODIFIED_FILES | wc -l` files modified on OS side without conflicts
echo `grep "\tnew file" $MODIFIED_FILES | wc -l` new files added on OS side
echo `grep "\tdeleted" $MODIFIED_FILES | wc -l` files removed on OS side without conflicts
echo `grep "\trenamed" $MODIFIED_FILES | wc -l` files renamed on OS side without conflicts

echo `grep "\tboth \(modified\|added\)" $MODIFIED_FILES | wc -l` files with conflicts that need to be resolved
echo `grep "\tdeleted by" $MODIFIED_FILES | wc -l` files that were changed by one side and deleted by the other

echo "(Dummy first file)" >$CONFLICTING_FILES_LIST
grep "\tboth \(modified\|added\)" $MODIFIED_FILES | sed "s/\t*both.*:\ *//" >>$CONFLICTING_FILES_LIST
grep "\tdeleted by \(them\|us\)" $MODIFIED_FILES | sed "s/\t*deleted by.*:\ *//" >$HALF_DELETED_FILES_LIST

if [ ! -s $HALF_DELETED_FILES_LIST ]; then
	rm $HALF_DELETED_FILES_LIST
fi

for f in `tail -n +2 $CONFLICTING_FILES_LIST`; do
	git add $f
done

git add $CHERRY_PICK_LOG
git add $MODIFIED_FILES
git add $CONFLICTING_FILES_LIST
if [ -s $HALF_DELETED_FILES_LIST ]; then
	git add $HALF_DELETED_FILES_LIST
fi

# write out a short file that allows resolve-next.sh to tell whether we're resolving
# CLDR or ICU files
echo "export RESOLVE_MODE=$MODE" >$LOGS/resolve-mode.sh
git add $LOGS/resolve-mode.sh
