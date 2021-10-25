#! /bin/zsh
SCRIPT_PATH="${0:A:h}"
. "$SCRIPT_PATH/common.sh"

echo 1..1

princ=jvidrine@APPLECONNECT.APPLE.COM
certgen -n $princ

esc_acp no_princs no_groups | tee "$tmpdir/results"
if grep -q "^$princ\$" $tmpdir/results; then
  echo ok
else
  echo not ok
fi
