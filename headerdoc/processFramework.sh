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
#    /path/to/headers	MyFrameworkName
#
# (with a *tab* between words) and processes the directory "headers",
# storing results in a folder called MyFrameworkName.  The Master TOC
# file is named MyFrameworkName.html instead of the usual masterTOC.html.
#
# Finally, the third argument is used to create an xml file that
# is used to store metadata about the newly-generated documentation
# directory.  If you aren't in our group at Apple, that part
# is probably not relevant to you, but it isn't worth maintaining
# a separate version for such a tiny option.
#

FRAMEWORK_OUTPUT="framework_output"
COPYFRAMEWORK="copyframework"

XML=""
if [ "x$1" = "x-X" ] ; then
	XML="-X"
	shift
	FRAMEWORK_OUTPUT="framework_output_xml"
	COPYFRAMEWORK="copyframework_xml"
fi

EVERYTHING=""
if [ "x$1" = "x-E" ] ; then
	EVERYTHING="-E"
	shift
fi

ASK=0
if [ "x$1" = "x-q" ] ; then
	ASK=1
	shift
fi


FRAMEWORKS="$(cat FrameworkList | grep -v '^\#')";
oldifs="$IFS"
# NOTE: This is intentionally a string containing a newline.
IFS="
"

for frameworkline in $FRAMEWORKS ; do
	if [ "$frameworklineX" != "X" ] ; then
	    framework="$(echo $frameworkline | cut -f1)"
	    frameworkName="$(echo $frameworkline | cut -f2)"

	    echo "FRAMEWORK: $framework"
	    echo "FRAMEWORKNAME: $frameworkName"

	    DO="y"
	    if [ "$ASK" = "1" ] ; then
		echo "Process this framework? (y/n) "
		read DO
	    fi
	    if [ "$DO" = "y" -o "$DO" = "Y" ] ; then

		frameworkDir="$(echo "$framework" | sed 's/\/$//g')"
		# frameworkName=`basename $framework`
		outputDir="$FRAMEWORK_OUTPUT/$frameworkName"
		rm -rf $outputDir
		mkdir -p $outputDir
		echo "Processing $frameworkDir into $outputDir";
		delete=0

		frameworkHDOC="frameworkHDOC/$frameworkName.hdoc"

		EXCLUDE=0
		if [ -f "frameworkHDOC/$frameworkName.skiplist" ] ; then
			EXCLUDE=1
			EXCLUDELISTFILE="frameworkHDOC/$frameworkName.skiplist"
		fi

		echo "HDOC FILE WOULD BE $frameworkHDOC"
		if [ -f "$frameworkHDOC" ] ; then
			echo "COPYING FRAMEWORK..."
			if [ -d $COPYFRAMEWORK ] ; then
				rm -rf $COPYFRAMEWORK;
			fi
			mkdir $COPYFRAMEWORK;
			cp -L $frameworkHDOC $COPYFRAMEWORK;
			cp -L -R $frameworkDir $COPYFRAMEWORK;
			frameworkDir="$COPYFRAMEWORK"
			cat $frameworkHDOC
			delete=1
			echo "DONE COPYING FRAMEWORK."
		fi
		# ls $frameworkDir

		if [ $EXCLUDE -eq 1 ] ; then
			./headerDoc2HTML.pl $XML $EVERYTHING --apple --auto-availability -H -O -j -Q -n -p -e $EXCLUDELISTFILE -o $outputDir $frameworkDir
		else
			./headerDoc2HTML.pl $XML $EVERYTHING --apple --auto-availability -H -O -j -Q -n -p -o $outputDir $frameworkDir
		fi
		if [ $? != 0 ] ; then
			echo "HeaderDoc crashed.  Exiting."
			exit -1;
		fi
		if [ "x$XML" = "x" ] ; then
			./gatherHeaderDoc.pl $outputDir index.html
			if [ $? != 0 ] ; then
				echo "GatherHeaderDoc crashed.  Exiting."
				exit -1;
			fi
		fi

		if [ $delete == 1 ] ; then
			echo "Cleaning up."
			# echo "Will delete $frameworkDir";
			# sleep 5;
			chmod -R u+w $COPYFRAMEWORK
			rm -rf $COPYFRAMEWORK
		fi
	    fi

	fi
done

if [ "x$XML" = "x" ] ; then
	if [ -f "./breadcrumbtree.pl" ] ; then
		if which perl5.8.9 > /dev/null ; then
			perl5.8.9 ./breadcrumbtree.pl $FRAMEWORK_OUTPUT
		else 
			./breadcrumbtree.pl $FRAMEWORK_OUTPUT
		fi
	fi
fi

IFS="$oldifs"

