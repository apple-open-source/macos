#! /bin/sh
. "${SRCROOT}/xcscripts/include.sh"

# $prefix should be set within Xcode for internal man pages
mandir="$(dst ${prefix:-/usr}/share/man)"
for n in $(jot "${SCRIPT_INPUT_FILE_COUNT}" 0); do
    eval file="\${SCRIPT_INPUT_FILE_$n}"
    sect=$(expr "$file" : '.*\([0-9]\)')
    xmkdir ${mandir}/man${sect}
    xinstall ${file} ${mandir}/man${sect}
done
