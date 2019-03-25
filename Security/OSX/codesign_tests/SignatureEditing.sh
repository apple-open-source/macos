#!/bin/sh

v=${v:-:}

fails=0
t=$(mktemp -d /tmp/cs-edit-XXXXXX)

runTest () {
    test=$1
    shift

    echo "[BEGIN] ${test}"

    ${v} echo "> $@"
    "$@" > $t/outfile.txt 2>&1
    res=$?
    [ $res != 0 ] && res=1 #normalize

    if expr "$test" : "fail"  > /dev/null; then
        exp=1
    else
        exp=0
    fi

    ${v} cat $t/outfile.txt
    if [ $res -eq $exp ]; then
        echo "[PASS] ${test}"
        echo
        rm -f $t/outfile.txt
    else
        echo
        cat $t/outfile.txt
        echo
        echo "[FAIL] ${test}"
        echo
        fails=$(($fails+1))
    fi
}

codesign=${codesign:-codesign}

editTest () {
    name="$1"
    shift
    target="$1"
    shift

    rm -f $t/cms

    runTest validate-$name $codesign -v -R="anchor apple" -v "$target"
    runTest dump-cms-$name $codesign -d --dump-cms=$t/cms "$target"
    runTest edit-nonsense-into-cms-$name $codesign -e "$target" --edit-cms /etc/hosts
    runTest fail-nonsense-validation-$name $codesign -v -R="anchor apple" -v "$target"
    runTest edit-original-into-cms-$name $codesign -e "$target" --edit-cms $t/cms
    runTest success-cms-validation-$name $codesign -v -R="anchor apple" -v "$target"
    runTest edit-cat-cms-into-cms-$name $codesign -e "$target" --edit-cms $t/cat.cms
    runTest fail-cat-cms-validation-$name $codesign -v -R="anchor apple" -v "$target"
    runTest edit-original-again-into-cms-$name $codesign -e "$target" --edit-cms $t/cms
    runTest success-cms-validation-again-$name $codesign -v -R="anchor apple" -v "$target"
}

runTest dump-cat-cms $codesign -d --dump-cms=$t/cat.cms /bin/cat

runTest prepare-ls cp -R /bin/ls $t/ls
editTest ls $t/ls
runTest prepare-TextEdit cp -R /Applications/TextEdit.app $t/TextEdit.app
editTest TextEdit $t/TextEdit.app

runTest prepare-codeless cp -R /var/db/gke.bundle $t/gke.bundle
editTest codeless $t/gke.bundle

runTest codesign-remove-signature $codesign --remove $t/ls
runTest codesign-omit-adhoc $codesign -s - -f --omit-adhoc-flag $t/ls
runTest adhoc-omitted sh -c "$codesign -d -v $t/ls 2>&1| grep -F 'flags=0x0(none)'"

# cleanup

if [ $fails != 0 ] ; then
    echo "$fails signature edit tests failed"
    exit 1
else
    echo "all signature edit tests passed"
    rm -rf $t
fi

exit 0
