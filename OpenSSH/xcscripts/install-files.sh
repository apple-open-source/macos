#! /bin/sh
. "${SRCROOT}/xcscripts/include.sh"
set -ex
# $installpath should be set within Xcode
if [ -z "${installpath}" ]; then
    >&2 echo "$installpath must be set"
    exit 2
fi
dir="$(dst ${installpath})"
xmkdir ${dir}
for n in $(jot "${SCRIPT_INPUT_FILE_COUNT}" 0); do
    eval file="\${SCRIPT_INPUT_FILE_$n}"
    xinstall ${file} ${dir}/
done
