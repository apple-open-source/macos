#!/bin/bash

AEP_Project="libusb"
AEP_Version="1.0.24"
AEP_ProjVers=$AEP_Project"-"$AEP_Version
AEP_Filename=$AEP_ProjVers.tar.bz2
AEP_ExtractDir=$AEP_ProjVers
AEP_Patches=("darwin_no_seize.patch")
SRCROOT="."

# Extract the source.
tar -C $SRCROOT -jxf $SRCROOT/$AEP_Filename
rm -R $SRCROOT"/"$AEP_Project
mv $SRCROOT"/"$AEP_ExtractDir $SRCROOT"/"$AEP_Project
for patchfile in ${AEP_Patches[@]}; do
	(cd $SRCROOT"/"$AEP_Project && patch -p0 < $SRCROOT"./"files"/"$patchfile) || exit 1;
done
