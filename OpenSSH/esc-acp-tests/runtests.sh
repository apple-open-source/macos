#! /bin/zsh
echo "${0:A:h}"
cd "${0:A:h}" || exit 2
export TRACE="$(mktemp -t esc-acp.trace)"
[[ -n "$TRACE" ]] || exit 2
trap 'rm -f "$result" "$TRACE"' EXIT
path=( ../build/Debug ../build/Release $path )

prove "$@" .

leaks=0
while read line; do
    leaks -quiet -atExit -- ${=line}
    if [[ "$?" -ne 0 ]]; then
        print -u2 "*** Command was: $line"
        : $[leaks++]
    fi
done < "${TRACE}"
if [[ "$leaks" -gt 0 ]]; then
    print -u2 "FAILED: ${leaks} run(s) produced memory leaks."
    exit 10
fi
print "No memory leaks detected."
