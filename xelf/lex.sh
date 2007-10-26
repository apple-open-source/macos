#!/bin/sh
if [ "${COMMAND_MODE:=Unix2003}" != "legacy" ]; then
    exec /usr/local/bin/flex-2.5.4 -X ${1+"$@"}
else
    exec /usr/local/bin/flex-2.5.4 ${1+"$@"}
fi
