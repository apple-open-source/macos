#!/bin/bash

for platform in ${SUPPORTED_PLATFORMS} ; do
    [[ "${platform}" == "${PLATFORM_NAME}" ]] && exit 0
done

for platform in ${SUPPORTED_PLATFORMS} ; do
    if [[ "${platform}" == "${FALLBACK_PLATFORM_NAME}" ]] ; then
        echo "Warning: New platform $PLATFORM_NAME detected! Fallback platform" \
                "($FALLBACK_PLATFORM_NAME) matches known platform, so not" \
                "failing. But please contact the Darwin Runtime Team to discuss" \
                "your new platform and ensure this project is configured" \
                "correctly."
        exit 0
    fi
done

echo "Unsupported platform encountered:" >&2
echo "    PLATFORM_NAME: ${PLATFORM_NAME}" >&2
echo "    SUPPORTED_PLATFORMS: ${SUPPORTED_PLATFORMS}" >&2
echo "Please contact the Darwin Runtime Team for help with your new platform bringup needs" >&2
exit 1
