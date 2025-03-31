#!/bin/bash

# HOW TO USE THIS SCRIPT
#
# This script takes no parameters.  You just run it from the command line after running
# set-up-environment.sh to set up the environment variables.
#
# This script requires that you have Bazel and Bazelisk installed on your machine and in
# the path.  Peter's original instructions had steps for making sure your Bazel version was
# up to date that I didn't replicate here and that may not be important, but there _is_
# a chance you'll have to update your Bazel version (I'm kind of hoping this is something
# Markus does each year and we can just piggyback on that.)

if [ -z $AICU_WORK ]; then
	echo "AICU_WORK isn't defined-- did you run set-up-environment.sh?"
	exit 1
fi

cd $AICU_WORK

# Copy the original versions of the Unicode-data source files to the backup folder and
# then apply the Apple patches to them.
FILES_TO_MODIFY="DerivedCoreProperties ppucd UnicodeData"
WORKING_UDATA_DIR="icu/icu4c/source/data/unidata"
BACKUP_UDATA_DIR="icu/icu4c/source/data/unidata/base_unidata"

for f in $FILES_TO_MODIFY; do
	cp $WORKING_UDATA_DIR/$f.txt $BACKUP_UDATA_DIR/$f.txt
	git add $BACKUP_UDATA_DIR/$f.txt
	sed -f $SCRIPTS/$f.patch.txt -i '' $WORKING_UDATA_DIR/$f.txt
	git add $WORKING_UDATA_DIR/$f.txt
done

# NOTES (rtg): Peter's process for updating these files was to do it manually, although he
# thought we could possibly use "patch" or "git apply" to automate it.  I tried both and
# had a lot of trouble.  I eventually re-did the patches as sed scripts.  This is more
# robust than applying patches, but not foolproof.  Furthermore, sed isn't powerful enough
# to update the "Total code points:" lines in DerivedCoreProperties.txt to include
# "+ApplePUA", as Peter was doing ("patch" and "git apply" won't do this either).
# Be careful to check the output before checking in to make sure the patches applied cleanly.
#
# What REALLY needs to happen here is for us to quit doing this altogether and go back to
# using the stock Unicode properties files from OSICU as they come to us.  Instead, we should
# add CODE to Apple ICU to do an extra lookup in separate tables that we control when somebody
# does a property query on an Apple PUA code point, something I suspect is happening very
# rarely these days.

# We can't run this tool in our actual working directory because of all the source changes
# we've made there, and we don't really want to run it in one of the other working directories.
# So we create a copy of $OSICU_NEW called osicu-uprops-work and run it there.
cd $OSICU_NEW/..
OSICU_NEW_PARENT=`pwd`
OSICU_UPROPS_WORK=$OSICU_NEW_PARENT/osicu-uprops-work
echo "Creating $OSICU_NEW $OSICU_UPROPS_WORK..."
cp -R $OSICU_NEW $OSICU_UPROPS_WORK

# Copy the files we modified above into this new temporary working directory we just created
for f in $FILES_TO_MODIFY; do
	echo "Copying $AICU_WORK/$WORKING_UDATA_DIR/$f.txt to $OSICU_UPROPS_WORK/icu4c/source/data/unidata..."
	cp $AICU_WORK/$WORKING_UDATA_DIR/$f.txt $OSICU_UPROPS_WORK/icu4c/source/data/unidata
done

# Set up environment variables that are needed by the generation tool
export ICU_SRC=$OSICU_UPROPS_WORK
export ICU4C_DATA_IN=$ICU_SRC/icu4c/source/data/in
export ICU4C_UNIDATA=$ICU_SRC/icu4c/source/data/unidata

# Now cd into the new working directory and run the existing tool for generating the props files
cd $ICU_SRC
bazelisk clean
icu4c/source/data/unidata/generate.sh

# Copy the generated files back into $AICU_WORK
GENERATED_SOURCE_FILES="\
	common/ubidi_props_data.h\
	common/ucase_props_data.h\
	common/uchar_props_data.h\
"
GENERATED_BINARY_FILES="\
	data/in/ubidi.icu\
	data/in/ucase.icu\
	data/in/ulayout.icu\
	data/in/uprops.icu\
"

# For source files, we can't just copy them-- we have clients who still want the original,
# undoctored property data.  So instead we concatenate the old and new versions of the file
# together, with an #ifdef construct around the two versions to select based on whether
# the platform is Darwin-based.
mkdir $SCRIPTS/tmp
echo "#if !U_PLATFORM_IS_DARWIN_BASED" >$SCRIPTS/tmp/p1.txt
echo "#else // U_PLATFORM_IS_DARWIN_BASED" >$SCRIPTS/tmp/p2.txt
echo "#endif // U_PLATFORM_IS_DARWIN_BASED" >$SCRIPTS/tmp/p3.txt
for f in $GENERATED_SOURCE_FILES; do
	echo "Combining $AICU_WORK/icu/icu4c/source/$f with $OSICU_UPROPS_WORK/icu4c/source/$f"
	cat $SCRIPTS/tmp/p1.txt $AICU_WORK/icu/icu4c/source/$f $SCRIPTS/tmp/p2.txt $OSICU_UPROPS_WORK/icu4c/source/$f $SCRIPTS/tmp/p3.txt >$SCRIPTS/tmp/result.txt
	cp $SCRIPTS/tmp/result.txt $AICU_WORK/icu/icu4c/source/$f
	git add $AICU_WORK/icu/icu4c/source/$f
done
rm -rf $SCRIPTS/tmp

# For generated binary files, on the other hand, we _do_ just copy them
for f in $GENERATED_BINARY_FILES; do
	echo "Copying $OSICU_UPROPS_WORK/icu4c/source/$f to $AICU_WORK/icu/icu4c/source/$f"
	cp $OSICU_UPROPS_WORK/icu4c/source/$f $AICU_WORK/icu/icu4c/source/$f
	git add $AICU_WORK/icu/icu4c/source/$f
done

# And delete the temporary working directory where we ran the generation too
rm -rf $OSICU_UPROPS_WORK

