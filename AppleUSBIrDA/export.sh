#!/bin/tcsh
echo Make the export files for AppleUSBIrDA and AppleSCCIrDA so
echo that the kext include fewer global symbols
echo
echo First delete the current export files and rebuild
rm -f AppleSCCIrDA/AppleSCCIrDA.exp 
rm -f AppleUSBIrDA/AppleUSBIrDA.exp

echo Now do a build with full symbols

pbxbuild -target AppleSCCIrDA
pbxbuild -target AppleUSBIrDA

echo Now generating export files based on export.keys

nm -g build/AppleSCCIrDA.kext/Contents/MacOS/AppleSCCIrDA | grep -v "U " | \
    fgrep -F -f export.keys | \
    awk ' { print $3 } '  > AppleSCCIrDA/AppleSCCIrDA.exp

nm -g build/AppleUSBIrDA.kext/Contents/MacOS/AppleUSBIrDA | grep -v "U " | \
    fgrep -F -f export.keys | \
    awk ' { print $3 } '  > AppleUSBIrDA/AppleUSBIrDA.exp

ls -l AppleSCCIrDA/AppleSCCIrDA.exp 
ls -l AppleUSBIrDA/AppleUSBIrDA.exp
