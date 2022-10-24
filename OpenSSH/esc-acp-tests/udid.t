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

echo 1..8

certgen -n $princ "$@"
{ do_acp && echo ok } || echo not ok
certgen -n $princ "$@" -O extension:device-udid@corp.apple.com=$udid
{ do_acp && echo ok } || echo not ok
ESC_ACP_MOCK_UDID=$(print $udid | tr '[A-F]' '[a-f]' | tr -d '-')
{ do_acp && echo ok } || echo not ok
ESC_ACP_MOCK_UDID=""
{ do_acp && echo not ok } || echo ok
ESC_ACP_MOCK_UDID="00008030"
{ do_acp && echo not ok } || echo ok
ESC_ACP_MOCK_UDID="00008030-000E508E0240012F"
{ do_acp && echo not ok } || echo ok
ESC_ACP_MOCK_UDID=$udid
certgen -n $princ "$@" -O extension:device-udid@corp.apple.com=
{ do_acp && echo not ok } || echo ok
certgen -n $princ "$@" -O extension:device-udid@corp.apple.com="00008030"
{ do_acp && echo not ok } || echo ok
