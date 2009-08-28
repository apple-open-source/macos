#!/bin/ksh

#/*! @function formatmessage_ksh
#    @abstract formats a message in a dumb way
#    @param $1 The last word of the message
#    @param $2 The next-to-last word of the message
#    @param $3+ The rest of the message
#*/

function formatmessage_ksh()
{
	LW=$1
	NLW=$2
	shift
	shift
	STRING=$@
	echo "$STRING $NLW $LW"
}

# Here starteh the script.  And yea, though the script be
# short, it doth make thee laugh.

formatmessage test. a This is
formatmessage system. broadcast This is only a test of the emergency
formatmessage "this station." watching If this had been an actual emergency, you would not be

