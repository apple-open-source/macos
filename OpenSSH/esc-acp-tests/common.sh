tmpdir="$(mktemp -d -t esp-acp)"
if [[ "$?" -ne 0 ]]; then
  exit 1
fi
path=( ${SCRIPT_PATH:h}/build/Release ${SCRIPT_PATH:h}/build/Debug
       /usr/bin /usr/sbin /bin /sbin )
CLEANUPS=('rm -rf "$tmpdir"')
function do_cleanup {
  local cleanup
  for cleanup in $CLEANUPS; do
    eval "$cleanup"
  done
}
trap do_cleanup EXIT

function cleanup {
  CLEANUPS=("$@" $CLEANUPS)
}


sshca="$tmpdir/testca"
sshkey="$tmpdir/id_ecdsa"
sshpub="$tmpdir/id_ecdsa.pub"
sshcert="$tmpdir/id_ecdsa-cert.pub"
ssh-keygen -q -t ecdsa -N '' -f "$sshca"
ssh-keygen -q -t ecdsa -N '' -f "$sshkey"

function randomdigits {
  LC_ALL=C tr -dc '[:digit:]' </dev/urandom | head -c $1
}

function certgen {
  ssh-keygen -s "$sshca" \
    -I $(uuidgen) \
    -z $(randomdigits 7) \
    -V +1w \
    -q \
    "$@" \
    "$sshkey"
}

function esc_acp {
  cert="$(awk '{ print $2 }' "$sshcert")"
  echo esc-acp $ESC_ACP_DEBUG "$cert" "$@" >>"${TRACE:-/dev/null}"
  esc-acp $ESC_ACP_DEBUG "$cert" "$@" 2>&1
}
