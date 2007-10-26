#! /bin/bash
# Copyright (c) 2007 Apple Inc. All rights reserved.
# Download the closest version of FilesBuster to the current build.

curl()
{
    $CURL --progress-bar --fail --remote-name "$@"
}

train()
{
    case $(sw_vers -productVersion) in 
	10.3) echo Panther ;;
	10.4) echo Tiger ;;
	10.5) echo Leopard ;;
    esac
}

buildvers()
{
    sw_vers -buildVersion
}

make_url_from_buildvers()
{
    local url
    local tname=$(train)
    local buildvers=$1

    echo http://spectrum-asr.apple.com/tools/$tname/$buildvers/FilesBuster.tgz
}

subtract_one_buildvers ()
{
    local buildvers="$1"
    local buildnum=$(echo $buildvers | sed '-es:^[0-9][A-Z]\([0-9]*\):\1:')
    local buildnext=$[ $buildnum - 1 ]

    echo $buildvers | sed -es:$buildnum:$buildnext:
}

CURL=${CURL:-/usr/bin/curl}
BUILDVERS=${BUILDVERS:-$(buildvers)}

while : ; do
    if curl $(make_url_from_buildvers $BUILDVERS); then
	exit 0
    fi

    BUILDVERS=$(subtract_one_buildvers $BUILDVERS)
done
