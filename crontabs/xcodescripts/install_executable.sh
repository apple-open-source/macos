#!/bin/sh
set -ex

install -m "${INSTALL_MODE_FLAG}" "${SCRIPT_INPUT_FILE_0}" "${SCRIPT_OUTPUT_FILE_0}"
chmod +x "${SCRIPT_OUTPUT_FILE_0}"
