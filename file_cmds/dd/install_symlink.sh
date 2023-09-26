#!/bin/sh
set -e
set -x

ln_dd() {
    ln -hfs /usr/local/bin/dd "$DSTROOT"/bin/dd
}

case "$PLATFORM_NAME" in
iphoneos|appletvos|watchos|bridgeos)
    ln_dd
    ;;
macosx|iphonesimulator)
    ;;
*)
    case "$FALLBACK_PLATFORM" in
    iphoneos|appletvos|watchos|bridgeos)
        ln_dd
        ;;
    *)
        echo "Unsupported platform: $ACTUAL_PLATFORM_NAME"
        exit 1
        ;;
    esac
    ;;
esac

