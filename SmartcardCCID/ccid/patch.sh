#!/bin/bash
# set -x		#echo on

AEP_Project="ccid"
AEP_Version="1.5.1"
AEP_ProjVers=$AEP_Project"-"$AEP_Version
AEP_Filename=$AEP_ProjVers.tar.bz2
AEP_ExtractDir=$AEP_ProjVers
AEP_Patches=("destDirFix.patch" "ForceWithoutPcsc.patch" "ccid-info-plist.patch" "osxConfigure.patch" "headerpadLDFlags.patch")
SRCROOT="."

# Extract the source.
tar -C $SRCROOT -jxf $SRCROOT/$AEP_Filename
rm -R $SRCROOT"/"$AEP_Project
mv $SRCROOT"/"$AEP_ExtractDir $SRCROOT"/"$AEP_Project
echo "Patching sources"
for patchfile in ${AEP_Patches[@]}; do
	(cd $SRCROOT"/"$AEP_Project && patch -p0 < $SRCROOT"./"files"/"$patchfile) || exit 1;
done
echo "Calling configure"
cd $SRCROOT/$AEP_Project && MacOSX/configure --no-configure --disable-opensc
