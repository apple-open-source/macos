#!/bin/sh
headers="${DSTROOT}${SYSTEM_LIBRARY_DIR}/Frameworks/IOKit.framework/Versions/A/Headers/storage/*.h";
for header in ${headers}; do
    unifdef -UKERNEL "${header}" > "${header}.unifdef"
    mv "${header}.unifdef" "${header}"
done
