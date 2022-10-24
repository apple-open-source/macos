#!/bin/sh
set -e
set -x

enable_debug_telnet_embedded() {
    plutil -replace ProgramArguments -json '["/var/personalized_debug/usr/libexec/telnetd","-p","/var/personalized_debug/usr/bin/login"]' "${SCRIPT_OUTPUT_FILE_0}"
    plutil -replace Label -string com.apple.telnetd.debug "${SCRIPT_OUTPUT_FILE_0}"
}

enable_telnet_embedded() {
    /usr/libexec/PlistBuddy -x \
        -c "Delete :Disabled" \
        -c "Add :PosixSpawnType string Interactive" \
        -c "Delete :SessionCreate" \
        -c "Set :Sockets:Listeners:Bonjour false" \
        -c "Add :Sockets:Listeners:SockFamily string IPv4" \
        -c "Add :Sockets:Listeners:SockNodeName string localhost" \
        "${SCRIPT_OUTPUT_FILE_0}"

    if [ -n "$dodebug" ]; then
	enable_debug_telnet_embedded
    fi
}

enable_telnet_bridgeos() {
    /usr/libexec/PlistBuddy -x \
        -c "Delete :Sockets:Listeners:SockNodeName" \
        -c "Delete :Sockets:Listeners:SockFamily" \
        "${SCRIPT_OUTPUT_FILE_0}"
}

case "$1" in
--debug)
    dodebug=1
    ;;
esac

cp "${SCRIPT_INPUT_FILE_0}" "${SCRIPT_OUTPUT_FILE_0}"
case "$PLATFORM_NAME" in
iphone*|appletv*|watch*)
    enable_telnet_embedded
    ;;
bridge*)
    enable_telnet_embedded
    enable_telnet_bridgeos
    ;;
macosx)
    ;;
*)
    case "$FALLBACK_PLATFORM_NAME" in
        iphone*|appletv*|watch*)
            enable_telnet_embedded
            ;;
        bridge*)
            enable_telnet_embedded
            enable_telnet_bridgeos
            ;;
        *)
            echo "Unsupported platform: $PLATFORM_NAME"
            exit 1
            ;;
    esac
    ;;
esac
