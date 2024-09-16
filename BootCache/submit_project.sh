#!/bin/zsh

# submit_project.sh 2.0.12
# source lives at <ssh://git@stash.sd.apple.com/perf/submit_project.git>
# usage: submit_project.sh [-v <version>] train [train [...]]

# override these values per project
projname="BootCache" # default: first alphabetically found <projname>.xcodeproj in the current directory
pbxprojpath="BootCache.xcodeproj/project.pbxproj" # default: <projname>.xcodeproj/project.pbxproj
defaultbranch="main"
# # #

set -e

zparseopts -D -A opthash v:
vers=$opthash[-v]
trains=($*)

if [[ $pbxprojpath == "" && $projname == "" ]]
then
	pbxprojpath=$(find -s . -name project.pbxproj -depth 2 -print | head -n1 | sed 's/^\.\///')
fi

if [[ $projname == "" ]]
then
	projname=$(echo ${pbxprojpath} | cut -d . -f 1)
fi

if [[ $trains == "" ]]
then
	echo "You need to specify one or more B&I trains to submit to, like this:"
	echo
	echo "$0 [-v <version (not including project name, so 123, not ${projname}-123)>] Fuji Whitetail"
	exit 1
fi

echo "Submitting ${projname} with project file at ${pbxprojpath}..."

if ! git diff-index --quiet HEAD $pbxprojpath
then
	echo "$pbxprojpath has uncommitted changes (project file must be clean), bailing."
	exit 1
fi

echo 'Fetching the latest from the default remote...'
echo

git fetch -t

echo

if [[ $vers == "" ]]
then
	vers=$(git tag -l | fgrep ${projname}- | sort -n -t- -k2 | tail -n 1 | cut -d '-' -f 2-)

	if [[ $vers != "" && ! ( $vers =~ "[^0-9\.]" ) ]]
	then
		echo "Previous version is '${vers}'."
	else
		echo "Couldn't get previous version.  Bailing."
		exit 1
	fi

	lastsha=$(git rev-parse ${projname}-${vers}^{})
	cursha=$(git rev-parse HEAD)

	echo

	if [[ $lastsha != $cursha ]]
	then
		vers=$(($vers + 1))
		tag="yes"
	else
		tag="no"
		echo "There aren't any new commits since \033[1;32m${projname}-${vers}\033[0m, so there's no need to tag a new version."
		echo "\t${lastsha} == ${cursha}"
	fi
else
	if [[ $vers =~ "[^0-9\.]" ]]
	then
		echo "Specified version number \"'${vers}\" is not a B&I-compatible version number."
		echo "Version numbers should be of the form: \"123\", or \"123.4\", or \"123.4.5\", etc."
		echo "You might have accidentally included the project name, e.g. \"${projname}-123\"."
		echo "If so, don't include the project name; this script will add it for you automatically."
		exit 1
	fi

	echo "You specified project version number \"${vers}\"."

	if git rev-parse "$projname"-"$vers" >/dev/null 2>/dev/null
	then
		tag="no"
		echo "Tag \033[1;32m${projname}-${vers}\033[0m already exists.  We'll use the existing tag instead of retagging."
	else
		tag="yes"
	fi
fi

if [[ $tag == "yes" ]]
then
	branchon=$(git symbolic-ref --short HEAD)
	color_branchon="\033[1;35m${branchon}\033[0m"

	echo "Head of current branch ${color_branchon} will be tagged \033[1;32m${projname}-${vers}\033[0m and submitted."

	if [[ $branchon != $defaultbranch ]]
	then
		echo 'WARNING: A commit will be added to this branch that will not appear on "'$defaultbranch'", and that commit will be tagged.'
		echo '(Usually you only want this if "'$color_branchon'" is an alternate release branch, e.g., for a software update.)'
		echo
		echo -n 'Is this OK? (Y/n): '
		read ok

		if [[ $ok != 'Y' ]]
		then
			echo 'Bailing.'
			exit 1
		fi
	fi

	localbranchsha=$(git rev-parse "refs/heads/${branchon}")
	remotebranchsha=$(git rev-parse "refs/remotes/origin/${branchon}") # XXX what if not `origin` ?

	if [[ $localbranchsha == $remotebranchsha ]]
	then
		echo "Great news: branch ${color_branchon} is up to date with the server."
		echo "\t${localbranchsha} == ${remotebranchsha}"
	else
		echo 'WARNING: Your branch "'$color_branchon'" is not up to date with the server.'
		echo "\t${localbranchsha} == ${remotebranchsha}"
		echo 'Submitting without resetting will leave commits out of the submission and confuse everyone.'
		echo
		echo -n 'May I go ahead and reset your local branch to point at the latest on the server? (Y/n): '
		read ok

		if [[ $ok != 'Y' ]]
		then
			echo 'Bailing.'
			exit 1
		fi

		git reset --hard "$remotebranchsha"
	fi
fi

echo

first_and="First"
submit_to_trains=()

for train in $trains
do
	color_train="\033[1;36m${train}\033[0m"
	echo "${first_and} for ${color_train}..."
	echo

	first_and="And now"

	lastvers=$(xbsfind projectversions --update Built${train} ${projname} | tail -1 | xargs)

	if [[ $? != 0 ]]
	then
		echo "Couldn't get last submission to ${color_train}."
		exit 1
	else
		echo "Last submission to ${color_train} was ${lastvers}."
	fi

	if [[ "${projname}-${vers}" == $lastvers ]]
	then
		echo "${color_train} already has ${projname}-${vers}!  Skipping ${color_train}..."
		echo
		continue
	fi

	echo
	echo "You will be submitting the following changes to ${color_train}:"
	echo

	git --no-pager log ${lastvers}..HEAD

	echo
	echo -n "Are those changes OK to submit to ${color_train}? (Y/n): "
	read ok

	if [[ $ok != "Y" ]]
	then
		echo
		echo "We'll skip ${color_train} then."
	else
		submit_to_trains+=(${train})
	fi

	echo
done

if [[ $#submit_to_trains == 0 ]]
then
	echo "Skipped all trains."
	exit 1
fi

if [[ $tag == "yes" ]]
then
	
	# Only try to set the version if the project has versioning configured
	if [[ $(agvtool vers -terse) != "" ]]
	then
		agvtool new-version $vers
		git commit $pbxprojpath -m "Bump to version ${vers}."
		git push origin HEAD
	else
		echo 'WARNING: This project does not use Apple-generic versioning so the version will not be bumped.'

		echo
		echo -n "Is this OK? (Y/n): "
		read ok

		if [[ $ok != "Y" ]]
		then
			echo 'Bailing.'
			exit 1
		fi
	fi

	git tag -a $projname-$vers -m "Tagging version ${vers}."
	git push origin $projname-$vers
fi

echo "Running submitproject..."
echo

xbs submitproject -tag "${projname}-${vers}" ${submit_to_trains} --submissionPolicy 4

echo "Done, congrats!"
