#!/bin/bash

#=======================================================================================
# set-up-integration.sh
# created by Richard Gillam, 9/25/24
#
# This tool performs the initial setup for integrating new versions of ICU and CLDR into
# the Apple ICU/CLDR fork.  It creates a bunch of directories for different versions of
# the source tree and populates them with the appropriate contents.  It also sets a
# bunch of environment variables to point to all the new directories and creates a new
# script called set-up-environment-variables.sh that can be used in an existing integration
# directory just to set up the appropriate environment variables.
#
# Here's what you get (assuming the "old" version is 74 and the "new" one is 76):
# variable       path                     branch/tag               description
# OSICU_OLD      osicu-74                 maint/maint-74*          The ICU project's release branch from the last release we integrated
# OSICU_NEW      osicu-76                 maint/maint-76*          The ICU project's release branch for the release we're integrating now
# AICU_OLD       aicu-74/icu/icu4c/source <supplied by user>       The icu source directory for our branch from the last integration we did
# AICU_NEW       aicu-76/icu/icu4c/source <current release branch> The icu source directory for ToT on the current release branch
# OSCLDR_OLD     oscldr-44                maint/maint-44           The CLDR project's release branch from the last release we integrated
# OSCLDR_NEW     oscldr-46                maint/maint-46           The CLDR project's release branch for the release we're integration now
# ACLDR_OLD      aicu-74/cldr             <supplied by user>       The cldr source directory for our branch from the last integration we did
# ACLDR_NEW      aicu-76/cldr             <current release branch> The cldr source directory for ToT on the current release branch
# AICU_OSSIMPORT aicu-ossimport           ossImport                The contents of the ossImport branch in our tree (we copy the contents of OSICU_NEW and OSCLDR_NEW into here)
# AICU_WORK      aicu-integration         <supplied by user>       The working directory where the actual integration happens
#
# The last two directories are the only ones we push from; the others are just there to
# facilitate comparisons and merging.
#
# You generally want to run this script by creating an empty directory, copying this script
# into it, and running it from there.  I've tried to write this so that you can reuse the
# same directory for multiple integrations, but I'm not 100% sure it'll work.
#=======================================================================================

# function for gathering input from the user.  $1 is the prompt string to display,
# $2 is the name of the variable to populate with the user's response, and $3 is
# the default value to assign to the variable if the user just hits return (or "required"
# to indicate the user has to enter something)
function read-user-input {
	local PROMPT=$1
	local VARNAME=$2
	local DEFAULT=$3
	
	local REQUIRED=
	if [ "$DEFAULT" = "required" ]; then
		REQUIRED=yes
		DEFAULT=
	fi
	
	export $VARNAME=$DEFAULT
	if [ $DEFAULT ]; then
		echo "$PROMPT Hit return for $DEFAULT"
	else
		echo "$PROMPT"
	fi
	
	local TMP
	read TMP
	if [ $TMP ]; then
		export $VARNAME=$TMP
	else
		if [ $REQUIRED ]; then
			echo "$VARNAME is required!"
			exit 1
		fi
	fi
}

# if set-up-environment-variables.sh isn't there, get the values of the variables from
# the user and then create a new set-up-environment-variables.sh file from the user's input
if [ ! -f set-up-environment-variables.sh ]; then
	INTEGRATION_ROOT=`pwd`

	read-user-input "Which version of ICU are you upgrading FROM?" OLD_ICU_VER
	read-user-input "Which version of ICU are you upgrading TO?" NEW_ICU_VER
	read-user-input "Which version of CLDR are you upgrading FROM?" OLD_CLDR_VER
	read-user-input "Which version of CLDR are you upgrading TO?" NEW_CLDR_VER
	
	read-user-input "What branch/tag do you want to use for OSICU_OLD?" OSICU_OLD_BRANCH "maint/maint-$OLD_ICU_VER"
	read-user-input "What branch/tag do you want to use for OSICU_NEW?" OSICU_NEW_BRANCH "maint/maint-$NEW_ICU_VER"
	read-user-input "What branch/tag do you want to use for OSCLDR_OLD?" OSCLDR_OLD_BRANCH "maint/maint-$OLD_CLDR_VER"
	read-user-input "What branch/tag do you want to use for OSCLDR_NEW?" OSCLDR_NEW_BRANCH "maint/maint-$NEW_CLDR_VER"
	
	read-user-input "What branch/tag do you want to use for the previous integration (AICU_OLD)?" AICU_OLD_BRANCH required
	read-user-input "What branch/tag do you want to use for AICU_NEW?" AICU_NEW_BRANCH "release/22.E-CrystalglowE"
	read-user-input "What branch/tag do you want to use for the current integration (AICU_WORK)?" AICU_WORK_BRANCH required
	
	cat >>set-up-environment-variables.sh << EOF
	export INTEGRATION_ROOT=$INTEGRATION_ROOT
	export OLD_ICU_VER=$OLD_ICU_VER
	export NEW_ICU_VER=$NEW_ICU_VER
	export OLD_CLDR_VER=$OLD_CLDR_VER
	export NEW_CLDR_VER=$NEW_CLDR_VER
	export OSICU_OLD=$INTEGRATION_ROOT/osicu-$OLD_ICU_VER
	export OSICU_NEW=$INTEGRATION_ROOT/osicu-$NEW_ICU_VER
	export OSCLDR_OLD=$INTEGRATION_ROOT/oscldr-$OLD_CLDR_VER
	export OSCLDR_NEW=$INTEGRATION_ROOT/oscldr-$NEW_CLDR_VER
	export AICU_OLD_PARENT=$INTEGRATION_ROOT/aicu-$OLD_ICU_VER
	export AICU_NEW_PARENT=$INTEGRATION_ROOT/aicu-$NEW_ICU_VER
	export AICU_OLD=\$AICU_OLD_PARENT/icu/icu4c/source
	export AICU_NEW=\$AICU_NEW_PARENT/icu/icu4c/source
	export ACLDR_OLD=\$AICU_OLD_PARENT/cldr
	export ACLDR_NEW=\$AICU_NEW_PARENT/cldr
	export AICU_OSSIMPORT=$INTEGRATION_ROOT/aicu-ossimport
	export AICU_WORK=$INTEGRATION_ROOT/aicu-integration
	export SCRIPTS=\$AICU_WORK/apple/scripts/integration
	export LOGS=\$AICU_WORK/apple/docs/icu-$NEW_ICU_VER-integration
	export OSICU_OLD_BRANCH=$OSICU_OLD_BRANCH
	export OSICU_NEW_BRANCH=$OSICU_NEW_BRANCH
	export OSCLDR_OLD_BRANCH=$OSCLDR_OLD_BRANCH
	export OSCLDR_NEW_BRANCH=$OSCLDR_NEW_BRANCH
	export AICU_OLD_BRANCH=$AICU_OLD_BRANCH
	export AICU_NEW_BRANCH=$AICU_NEW_BRANCH
	export AICU_WORK_BRANCH=$AICU_WORK_BRANCH
	export EDIT=bbedit
	export DIFF=bcompare
EOF
	
	chmod a+x set-up-environment-variables.sh
fi

# use set-up-environment-variables.sh to initialize all the environment variables
source set-up-environment-variables.sh

OSICU_REPO_URL=git@github.com:unicode-org/icu.git
OSCLDR_REPO_URL=git@github.com:unicode-org/cldr.git
AICU_REPO_URL=ssh://git@stash.sd.apple.com/i18n/icu.git

# function for cloning repositories.  $1 is the URL of the repo, $2 is the directory
# name to clone to, and $3 is the branch or tag to switch the repo to after cloning
function clone-repo {
	local URL="$1"
	local DIR=$2
	local BRANCH=$3
	
	if [ ! -d $DIR ]; then
		git clone $URL $DIR
		pushd $DIR
		git checkout $BRANCH || exit 1
		popd
	fi
}

# clone all the relevant repos (see above)
clone-repo $OSICU_REPO_URL $OSICU_OLD $OSICU_OLD_BRANCH
clone-repo $OSICU_REPO_URL $OSICU_NEW $OSICU_NEW_BRANCH
clone-repo $OSCLDR_REPO_URL $OSCLDR_OLD $OSCLDR_OLD_BRANCH
clone-repo $OSCLDR_REPO_URL $OSCLDR_NEW $OSCLDR_NEW_BRANCH

clone-repo $AICU_REPO_URL $AICU_OLD_PARENT $AICU_OLD_BRANCH
clone-repo $AICU_REPO_URL $AICU_NEW_PARENT $AICU_NEW_BRANCH
clone-repo $AICU_REPO_URL $AICU_OSSIMPORT ossImport

# $AICU_WORK is the only repo we don't use clone-repo to clone-- see comment
# below for why
if [ ! -d $AICU_WORK ]; then
	git clone $AICU_REPO_URL $AICU_WORK || exit 1
	pushd $AICU_WORK
	# This directory is the only one for which this script creates a branch.
	# This will be the directory where the actual integration work happens.
	# The script will first look for an upstream branch with the right name.
	# If it doesn't find one, it creates a new branch on the client based on
	# AICU_NEW_BRANCH, which should be the current release branch.  
	git checkout -b $AICU_WORK_BRANCH origin/$AICU_WORK_BRANCH || git checkout -b $AICU_WORK_BRANCH origin/$AICU_NEW_BRANCH || exit 1
	popd
fi

