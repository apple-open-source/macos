#!/bin/bash

ICU_ROOT=`pwd`
SRC_ROOT=$ICU_ROOT/icu/icu4c/source
CLDR_ROOT=$ICU_ROOT/cldr
CLDR_TMP_ROOT=$ICU_ROOT/cldr-staging
TOOLS_ROOT=$ICU_ROOT/icu/tools
ANT_OPTS="-Xmx8192m"

# check to make sure we're running from the root level of the "icu" repo
for i in $SRC_ROOT $CLDR_ROOT $TOOLS_ROOT; do
	if [ ! -e $i ]; then
		echo "$i not found -- are you in the right directory?"
		exit 1
	fi
done

# check to make sure JAVA_HOME is set
if [ -z "$JAVA_HOME" ]; then
	echo "JAVA_HOME is not set-- do you have a JDK installed?"
	exit 1
fi

# check all of the CLDR files for syntax errors	
for i in main segments supplemental transforms; do
	echo "### Checking syntax in $CLDR_ROOT/common/$i..."
	xmllint -noout -valid $CLDR_ROOT/common/$i/*.xml
	if [ "$?" -ne "0" ]; then
		echo "CLDR XML validation failed"
		exit 1
	fi
done

# create intermediate directories (clear out cldr-staging if it's already there)
# (TODO: rather than create cldr/exemplars/main here, it probably makes more sense to
# actually populate that directory like everything else and put it under source control--
# remember to take out this line if/when we do that!)
echo "### Creating intermediate directories..."
rm -rf $CLDR_TMP_ROOT
mkdir -p $CLDR_TMP_ROOT
mkdir -p $CLDR_ROOT/exemplars/main

# TODO: Peter's original instructions have a step in here where we run CLDR's unit tests.
# Those tests have never passed.  I'm pretty sure this is because they're full of Apple
# customizations.  At some point, we should go to the trouble of making the CLDR unit tests
# pass and start keeping them up to date when we change things.  But for now, we're not
# doing this.

# Note: Peter's original instructions had a step here where we copy a bunch of files from
# $CLDR_ROOT/seed into $CLDR_ROOT/common.  The CLDR project eliminated the "seed" directory
# a couple versions ago and moved everything into "common" for us, so this step is no
# longer necessary.

# Note: Peter's original instructions had a step here where we had to rename ur_IN.xml and
# ur_PK.xml to ur_Arab_IN.xml and ur_Arab_PK.xml.  Apple's copy of CLDR has this change
# checked into source control now, so this step is no longer necessary.  (The
# `update-ossimport.sh` script does that renaming when updating the ossImport branch so that
# any changes to those files will still get caught automatically during the integration process.)

# Copy CDLR dtds to ICU in case there are any updates
echo "### Copying DTDs into SRC_ROOT..."
cp -p $CLDR_ROOT/common/dtd/ldml.dtd  $SRC_ROOT/data/dtd/cldr/common/dtd/
cp -p $CLDR_ROOT/common/dtd/ldmlICU.dtd $SRC_ROOT/data/dtd/cldr/common/dtd/

# Build the CLDR API library
# (TODO: Peter's original instructions have us using "ant install-cldr-libs" in $TOOLS_ROOT/cldr,
# but I've never been able to get that to work.  Similarly, doing "mvn package" in $CLDR_ROOT/tools
# doesn't completely work either, because we get an error when we try to build the Survey Tool.
# What I'm doing here is just building $CLDR_ROOT/tools/cldr-code and manually exporting it to
# $TOOLS_ROOT/cldr/lib.)
echo "### Building CLDR API library..."
cd $CLDR_ROOT/tools/cldr-code
mvn package -DskipTests=true
if [ "$?" -ne "0" ]; then
	echo Failed to build CLDR API library
	exit 1
fi

echo "### Exporting CLDR API library to CLDR_ROOT..."
mkdir -p $TOOLS_ROOT/cldr/lib
cd $TOOLS_ROOT/cldr/lib
mvn install:install-file \
  -Dproject.parent.relativePath="" \
  -DgroupId=org.unicode.cldr \
  -DartifactId=cldr-api \
  -Dversion=0.1-SNAPSHOT \
  -Dpackaging=jar \
  -DgeneratePom=true \
  -DlocalRepositoryPath=. \
  -Dfile="$CLDR_ROOT/tools/cldr-code/target/cldr-code.jar"
if [ "$?" -ne "0" ]; then
	echo Failed to export CLDR API library to TOOLS_ROOT
	exit 1
fi

mvn dependency:purge-local-repository \
  -Dproject.parent.relativePath="" \
  -DmanualIncludes=org.unicode.cldr:cldr-api:jar

# Build the final version (with inheritance in place) of the CLDR stuff to cldr-staging
(
	echo "### Building cldr-staging directory..."
	export CLDR_DIR=$CLDR_ROOT
	export CLDR_TMP_DIR=$CLDR_TMP_ROOT
	cd $SRC_ROOT/data
	ant cleanprod || exit 1
	ant setup || exit 1
	ant proddata || exit 1
)
if [ "$?" -ne "0" ]; then
	echo Failed to build cldr-staging directory
	exit 1
fi

# Now build the actual ICU data source files from the .xml files in cldr-staging and
# copy the test files over to their proper locations
(
	echo "### Building ICU data source files..."
	export CLDR_DIR=$CLDR_ROOT
	cd $TOOLS_ROOT/cldr/cldr-to-icu
	ant -f build-icu-data.xml -DcldrDataDir="$CLDR_TMP_ROOT/production" || exit 1
	cd $TOOLS_ROOT/cldr
	ant copy-cldr-testdata || exit 1
)
if [ "$?" -ne "0" ]; then
	echo Failed to build ICU data source files
	exit 1
fi

# Copy localeCanonicalization.txt into $SRC_ROOT
# (TODO: Not sure why we have to do this.  Is anybody actually using this file?)
echo "### Copying localeCanonicalization.txt to SRC_ROOT..."
sed '1 i\
# File copied from cldr common/testData/localeIdentifiers/localeCanonicalization.txt\
' $CLDR_ROOT/common/testData/localeIdentifiers/localeCanonicalization.txt >$SRC_ROOT/test/testdata/localeCanonicalization.txt

# Add lstm entries to end of data/brkitr/root.txt
# (TODO: Is there a better way to do this? I couldn't get the -i option to work.)
echo "### Adding lstm entry to end of data/brkitr/root.txt"
sed -i '' '$ i\
    lstm{\
        Thai{"Thai_graphclust_model4_heavy.res"}\
        Mymr{"Burmese_graphclust_model5_heavy.res"}\
    }\
' $SRC_ROOT/data/brkitr/root.txt

# Fix the aliases in all the ur_Aran_IN.txt files
# (TODO: The fact that we have to do this suggests there's a bug in the generator tool that we should fix)
echo "### Fixing the %%ALIAS entry in all the ur_Arab_IN files..."
for i in `find $SRC_ROOT/data -name "ur_Aran_IN.txt" -print`; do
	sed -i '' 's/___{""}/"%%ALIAS"{"ur_Arab_IN"}/' $i
done
