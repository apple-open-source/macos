#!/bin/sh

#
# This script is used to process frameworks for the developer.apple.com
# website.  Since the script is also somewhat handy for anybody else
# doing similar amounts of procesing, I'm including it here, though you
# will probably want to change some of the options to headerDoc2HTML.pl
# and gatherHeaderDoc.pl if you use it.
#
# The script is straightforward.  It reads a file called
# FrameworkList that contains lines in the form
#
#    /path/to/headers MyFrameworkName
#
# and processes the directory "headers", storing results in a folder
# called MyFrameworkName.  The Master TOC file is named
# MyFrameworkName.html instead of the usual masterTOC.html.
#
# Finally, the third argument is used to create an xml file that
# is used to store metadata about the newly-generated documentation
# directory.  If you aren't in our group at Apple, that part
# is probably not relevant to you, but it isn't worth maintaining
# a separate version for such a tiny option.
#

FRAMEWORKS="$(cat FrameworkList)";
oldifs="$IFS"
# NOTE: This is intentionally a string containing a newline.
IFS="
"

for frameworkline in $FRAMEWORKS ; do
	if [ "$frameworklineX" != "X" ] ; then
		framework="$(echo $frameworkline | cut -f1 -d' ')"
		frameworkName="$(echo $frameworkline | cut -f2 -d' ')"

		echo "FRAMEWORK: $framework"
		echo "FRAMEWORKNAME: $frameworkName"

		frameworkDir="$framework"
		# frameworkName=`basename $framework`
		outputDir="framework_output/$frameworkName"
		rm -rf $outputDir
		mkdir -p $outputDir
		echo "Processing $frameworkDir into $outputDir";
		delete=0

		frameworkHDOC="frameworkHDOC/$frameworkName.hdoc"

		echo "HDOC FILE WOULD BE $frameworkHDOC"
		if [ -f "$frameworkHDOC" ] ; then
			mkdir copyframework;
			cp $frameworkHDOC copyframework;
			cp -R $frameworkDir copyframework;
			frameworkDir="copyframework"
			cat $frameworkHDOC
			delete=1
		fi
		# ls $frameworkDir

		./headerDoc2HTML.pl -H -O -p -o $outputDir $frameworkDir
		./gatherHeaderDoc.pl $outputDir $frameworkName.html

		if [ $delete == 1 ] ; then
			echo "Cleaning up."
			# echo "Will delete $frameworkDir";
			# sleep 5;
			chmod -R u+w copyframework
			rm -rf copyframework
		fi


	fi
done

if [ -f "./breadcrumbtree.pl" ] ; then
	./breadcrumbtree.pl framework_output
fi

IFS="$oldifs"

