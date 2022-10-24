#! /bin/zsh

note() {
    echo "note: [Tribeca] $@"
}

die() {
    echo "error: [Tribeca] $@"
    exit 1
}

plist="$1"; shift

note "AUTOERASE_ON=\"$AUTOERASE_ON\""
(( AUTOERASE_ON > 0 )) || exit 0

note "Enabling in ${plist}"
/usr/bin/plutil \
    -insert 'MachServices.com\.apple\.security\.tribeca' \
    -bool true \
    $plist || die "plutil failed for ${plist}"
exit 0
