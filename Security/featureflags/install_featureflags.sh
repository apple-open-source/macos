set -x

INPUT_FILE_PATH="${SCRIPT_INPUT_FILE_0}"
OUTPUT_FILE_PATH="${SCRIPT_OUTPUT_FILE_0}"

mkdir -vp `dirname "${OUTPUT_FILE_PATH}"`

cp -v "${INPUT_FILE_PATH}" "${OUTPUT_FILE_PATH}"

if  [ "${PLATFORM_NAME}" == "appletvos" ]; then
    plutil -replace OctagonSupportsPersonaMultiuser -json '{ "Enabled" : true }' "${OUTPUT_FILE_PATH}"
else
    echo "No feature flags to modify"
fi
