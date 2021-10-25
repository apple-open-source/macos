#! /bin/zsh
SCRIPT_PATH="${0:A:h}"
. "$SCRIPT_PATH/common.sh"

princ="jvidrine@APPLECONNECT.APPLE.COM"
udid="00008030-000E508E0240012E"
export ESC_ACP_MOCK_UDID=$udid

function do_acp {
  esc_acp no_princs no_groups | tee "$tmpdir/results"
  grep -q "^$princ\$" $tmpdir/results
}


echo 1..2

certgen -n $princ "$@" -O extension:device-udid@corp.apple.com=$udid
if do_acp; then
  echo ok
else
  echo not ok
fi
ESC_ACP_MOCK_UDID="00008030-000E508E0240012F"
if do_acp; then
  echo not ok
else
  echo ok
fi
