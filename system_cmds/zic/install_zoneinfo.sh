#!/bin/sh
set -e
set -x

if [ -n "$RC_BRIDGE" ]; then
    ACTUAL_PLATFORM_NAME="bridgeos"
else
    ACTUAL_PLATFORM_NAME="${PLATFORM_NAME}"
fi

# This sets up the paths for the default set of zoneinfo files
# On iOS, watchOS, and tvOS, this is handled by tzd in coordination with 
# launchd on first boot.
# On macOS, the directory needs to be setup # during mastering for SIP 
# protection, and the symlink for the BaseSystem.
# On bridgeOS tzd doesn't exist, so needs this all setup statically.
default_zoneinfo_setup()
{
    mkdir -p "${DSTROOT}/private/var/db/timezone"
    ln -hfs "/usr/share/zoneinfo.default" "${DSTROOT}/private/var/db/timezone/zoneinfo"
    chmod 555 "${DSTROOT}/private/var/db/timezone"
}

install_zoneinfo_ios_tv_watch_macos() {
    ditto "${BUILT_PRODUCTS_DIR}/zoneinfo" "${DSTROOT}/usr/share/zoneinfo.default"
    ln -hfs "/var/db/timezone/zoneinfo" "${DSTROOT}/usr/share/zoneinfo"
}

case "$ACTUAL_PLATFORM_NAME" in
iphone*|appletv*|watch*|macosx)
    install_zoneinfo_ios_tv_watch_macos
    case "$ACTUAL_PLATFORM_NAME" in
    macosx)
        default_zoneinfo_setup
        ;;
    esac
    ;;
bridge*)
    # Since bridgeOS lacks any mechanism to change timezones (currently),
    # and in the interest of saving space, only GMT is installed.
    mkdir -p "${DSTROOT}/usr/share/zoneinfo.default"
    chmod 555 "${DSTROOT}/usr/share/zoneinfo.default"
    ditto "${BUILT_PRODUCTS_DIR}/zoneinfo/GMT" "${DSTROOT}/usr/share/zoneinfo.default/GMT"
    default_zoneinfo_setup
    ;;
*)
    case "$FALLBACK_PLATFORM" in
        iphone*|appletv*|watch*|bridge*)
            install_zoneinfo_ios_tv_watch_macos
            ;;
        *)
            echo "Unsupported platform: $ACTUAL_PLATFORM_NAME"
            exit 1
            ;;
    esac
    ;;
esac

