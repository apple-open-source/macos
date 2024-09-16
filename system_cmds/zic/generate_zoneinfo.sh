#!/bin/sh

set -xe

if [ $# -ne 5 ]; then
    echo "Usage: $0 SRCROOT OBJROOT BUILT_PRODUCTS_DIR SDKROOT PLATFORM_NAME" 1>&2
    exit 1
fi

SRCROOT="$1"
OBJROOT="$2"
BUILT_PRODUCTS_DIR="$3"
SDKROOT="$4"
PLATFORM_NAME="$5"

ZICHOST_SYMROOT="${BUILT_PRODUCTS_DIR}/zic_host-sym"
ZICHOST_DSTROOT="${BUILT_PRODUCTS_DIR}/zic_host-dst"
ZICHOST="${ZICHOST_DSTROOT}/zic_host"

LOCALTIME="US/Pacific"
POSIXRULES="US/Pacific"

ZONEINFO="${BUILT_PRODUCTS_DIR}/zoneinfo"
DATFILES="${BUILT_PRODUCTS_DIR}/datfiles"
PRIVATEDIR="${BUILT_PRODUCTS_DIR}/private"

# ftp://elsie.nci.nih.gov/pub/tzdata*.tar.gz
# the tzdata*.tar.gz file is automatically unpacked and a version file created
# /usr/local/share/tz/tzdata*.tar.gz is installed by the TimeZoneData project
TARBALL="${SDKROOT}"/usr/local/share/tz/latest_tzdata.tar.gz

mkdir -p "${DATFILES}"
mkdir -p "${ZONEINFO}"
tar zxf "${TARBALL}" -C "${DATFILES}"
awk '/^Release [0-9]{4}[a-z] / { print $2; exit }' "${DATFILES}"/NEWS >"${DATFILES}"/version
make -C "${DATFILES}" VERSION_DEPS= tzdata.zi

ZONE_FILES="africa antarctica asia australasia europe northamerica southamerica"
ZONE_FILES="${ZONE_FILES} factory etcetera backward"
for tz in ${ZONE_FILES}; do
    if [ ${tz} = "northamerica" ]; then
        ARG="-p America/New_York"
    else
        ARG=""
    fi
    "${ZICHOST}" $ARG -d "${ZONEINFO}" -b fat "${DATFILES}/${tz}"
done

chmod -R og-w "${ZONEINFO}"
for f in zone.tab iso3166.tab leapseconds tzdata.zi ; do
    install -m 0444 "${DATFILES}/$f" "${ZONEINFO}/$f"
done

if [ -n "${RC_BRIDGE}" ]; then
    ACTUAL_PLATFORM_NAME="bridgeos"
else
    ACTUAL_PLATFORM_NAME="${PLATFORM_NAME}"
fi

case "${ACTUAL_PLATFORM_NAME}" in
bridge*)
    LOCALTIME="GMT"
    ;;
esac

ln_tzinfo_embedded() {
    mkdir -p "${PRIVATEDIR}/var/db"
    mkdir -p -m a+rx "${PRIVATEDIR}/var/db/timezone"

    # This link must precisely start with TZDIR followed by a slash. radar:13532660
    ln -hfs "/var/db/timezone/zoneinfo/${LOCALTIME}" "${PRIVATEDIR}/var/db/timezone/localtime"
}

case "$ACTUAL_PLATFORM_NAME" in
iphone*|appletv*|watch*|bridge*)
    ln_tzinfo_embedded
    ;;
macosx)
    mkdir -p "${PRIVATEDIR}/etc"
    ln -hfs "/var/db/timezone/zoneinfo/${LOCALTIME}" "${PRIVATEDIR}/etc/localtime"
    ;;
*)
    case "${FALLBACK_PLATFORM}" in
        iphone*|appletv*|watch*|bridge*)
            ln_tzinfo_embedded
            ;;
        *)
            echo "Unsupported platform: ${ACTUAL_PLATFORM_NAME}" 1>&2
            exit 1
            ;;
    esac
    ;;
esac

install -m 0444 "${DATFILES}/version" "${ZONEINFO}/+VERSION"
touch "${ZONEINFO}"
touch "${PRIVATEDIR}"

