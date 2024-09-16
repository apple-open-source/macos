xinstall() {
    install -g "${INSTALL_GROUP}" -o "${INSTALL_OWNER}" \
      -m "${INSTALL_MODE:-0644}" "$@"
}

xmkdir() {
    for dir in "$@"; do
        test -d "${dir}" || \
        install -g "${INSTALL_GROUP}" -o "${INSTALL_OWNER}" \
          -m "${INSTALL_MODE_DIR:-0755}" -d "${dir}" || return $?
    done
}

addprefix() {
    while [ "${ADDPREFIX}" != "${ADDPREFIX%/}" ]; do
        ADDPREFIX="${ADDPREFIX%/}"
    done
    if [ "$#" -eq 1 ]; then
        printf '%s/%s' "${ADDPREFIX}" "${1#/}"
    else
        for path in "$@"; do
            printf '%s/%s ' "${ADDPREFIX}" "${path#/}"
        done
    fi
}

dst() {
    ADDPREFIX="${DSTROOT}" addprefix "$@"
}

src() {
    ADDPREFIX="${SRCROOT}" addprefix "$@"
}

obj() {
    ADDPREFIX="${OBJROOT}" addprefix "$@"
}

set -e
