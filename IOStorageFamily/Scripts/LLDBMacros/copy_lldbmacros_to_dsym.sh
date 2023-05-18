#! /bin/sh -e

# We must copy with a script because 'Copy Files' build phase is always relative to the DSTROOT directory. Our script must end up in the Symbols directory which is outside of DSTROOT.
python_script="${SRCROOT}/Scripts/LLDBMacros/iosf_lldb_macros.py"

dsym_python_dir="${DWARF_DSYM_FOLDER_PATH}/${DWARF_DSYM_FILE_NAME}/Contents/Resources/Python/"
mkdir -p "${dsym_python_dir}"

for variant in ${BUILD_VARIANTS}; do
	case ${variant} in
	normal)
		cp "${python_script}" ${dsym_python_dir}/${EXECUTABLE_NAME}.py
		;;
	*)
		SUFFIX="_${variant}"
		ln -sf ${EXECUTABLE_NAME}.py ${dsym_python_dir}/${EXECUTABLE_NAME}${SUFFIX}.py
		;;
	esac
done

if [ "${PLATFORM_NAME}" != "macosx" ]; then
    kdk_python_dir="${DSTROOT}/AppleInternal/KextObjects/Python/${MODULE_NAME}"
    mkdir -p "${kdk_python_dir}"
    cp "${python_script}" "${kdk_python_dir}/${EXECUTABLE_NAME}.py"
fi
