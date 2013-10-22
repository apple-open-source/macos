#!/bin/bash
set -e -x

# check if we're building for the simulator
if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
	DSTROOT="${DSTROOT}${SDKROOT}"
fi

DIR="$DSTROOT"/usr/include/arpa
install -d -m 0755 -o "$INSTALL_OWNER" -g "$INSTALL_GROUP" \
	"$DIR"
ln -s ../nameser.h "$DIR"
chmod -h 0644 "$DIR"/nameser.h
chown -h "$INSTALL_OWNER:$INSTALL_GROUP" "$DIR"/nameser.h
