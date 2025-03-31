#!/bin/sh
# -e: exit on error, -u: error on unset variables, -x: show commands
set -e -u -x

if [ -z ${DSTROOT} ] ; then
    echo "Error: DSTROOT is unset!"
    exit 1
elif [ ${DSTROOT} = "/" ] ; then
    echo "Error: DSTROOT cannot specify actual root directory \"/\"!"
    exit 1
fi
if [ $# -gt 0 ] && [ "$1" = "-cryptex" ] ; then
    echo "Building asset with cryptex directory layout"
    CRYPTEX_FORMAT=1
    ASSET_DIR=${DSTROOT}
    ASSET_DATA=${ASSET_DIR}/SecurePKITrustStoreAssets/SecurePKITrustStore/Source
    METADATA_DIR=${DSTROOT}/SecurePKITrustStoreAssets/SecurePKITrustStore/Metadata
else
    echo "Building asset with standard directory layout"
    CRYPTEX_FORMAT=0
    ASSET_DIR=${DSTROOT}/PKITrustStoreAssets/PKITrustStore
    ASSET_DATA=${ASSET_DIR}/AssetData
    METADATA_DIR=${ASSET_DIR}
fi
BUILT_ASSETS_DIR=${BUILT_PRODUCTS_DIR}/BuiltAssets
BUILT_KEYCHAINS_DIR=${BUILT_PRODUCTS_DIR}/BuiltKeychains
CONFIG_DIR=${SRCROOT}/config
CT_DIR=${SRCROOT}/certificate_transparency
PINNING_DIR=${SRCROOT}/Pinning
SUPPLEMENTALS_DIR=${SRCROOT}/TrustSupplementalsAsset

mkdir -p ${METADATA_DIR}
mkdir -p ${ASSET_DATA}
if [ ${CRYPTEX_FORMAT} -eq 1 ] ; then
    # currently, we must not create the AssetData directory ourselves
    #mkdir -p ${METADATA_DIR}/AssetData
    # currently, we must put CryptexInfo.plist into Source instead of Metadata
    echo "Moving CryptexInfo.plist"
    plutil -convert binary1 ${CONFIG_DIR}/CryptexInfo.plist -o ${ASSET_DATA}/CryptexInfo.plist
fi

echo "Moving Info.plist"
plutil -convert binary1 ${CONFIG_DIR}/Info-Asset.plist -o ${METADATA_DIR}/Info.plist
# update asset version from baseline content version in Info.plist
BASECONTENTVERSION=`plutil -extract MobileAssetProperties.ContentVersion raw ${METADATA_DIR}/Info.plist`
CVLENGTH=`printf "%s" ${BASECONTENTVERSION} | wc -c`
if [ ${CVLENGTH} != 10 ] ; then
    printf "Bad content version length (%s), expected 10" ${CVLENGTH}
    exit 1
fi
ASSETCONTENTVERSION=$(( ${BASECONTENTVERSION} + 1 ))
plutil -replace MobileAssetProperties.ContentVersion -integer ${ASSETCONTENTVERSION} ${METADATA_DIR}/Info.plist
if [ ${CRYPTEX_FORMAT} -eq 1 ] ; then
    echo "Updating Info.plist for cryptex asset"
    # ensure these two keys are present and set to true in the asset plist output
    plutil -replace MobileAssetProperties.__ContainsCryptexContents -bool YES ${METADATA_DIR}/Info.plist
    plutil -replace MobileAssetProperties.__RequiresPersonalization -bool YES ${METADATA_DIR}/Info.plist
else
    echo "Updating Info.plist for non-cryptex asset"
    # remove these keys from the asset plist output
    plutil -remove MobileAssetProperties.__ContainsCryptexContents ${METADATA_DIR}/Info.plist
    plutil -remove MobileAssetProperties.__RequiresPersonalization ${METADATA_DIR}/Info.plist
fi

echo "Moving AnalyticsSamplingRates.plist"
plutil -convert binary1 ${SUPPLEMENTALS_DIR}/AnalyticsSamplingRates.plist -o ${ASSET_DATA}/AnalyticsSamplingRates.plist

echo "Moving Anchors"
ditto ${BUILT_ASSETS_DIR}/Anchors ${ASSET_DATA}/Anchors

echo "Moving Anchors.plist"
ditto ${BUILT_ASSETS_DIR}/Anchors.plist ${ASSET_DATA}/Anchors.plist

echo "Moving AppleCertificateAuthorities.plist"
plutil -convert binary1 ${SUPPLEMENTALS_DIR}/AppleCertificateAuthorities.plist -o ${ASSET_DATA}/AppleCertificateAuthorities.plist

echo "Moving AssetVersion.plist"
plutil -convert binary1 ${CONFIG_DIR}/AssetVersion.plist -o ${ASSET_DATA}/AssetVersion.plist
plutil -replace VersionNumber -integer ${ASSETCONTENTVERSION} ${ASSET_DATA}/AssetVersion.plist

echo "Moving Blocked.plist"
plutil -convert binary1 ${BUILT_ASSETS_DIR}/Blocked.plist -o ${ASSET_DATA}/Blocked.plist

echo "Moving CertificatePinning.plist"
plutil -convert binary1 ${PINNING_DIR}/CertificatePinning.plist -o ${ASSET_DATA}/CertificatePinning.plist

echo "Moving certsIndex.data"
ditto ${BUILT_ASSETS_DIR}/certsIndex.data ${ASSET_DATA}/certsIndex.data

echo "Moving certsTable.data"
ditto ${BUILT_ASSETS_DIR}/certsTable.data ${ASSET_DATA}/certsTable.data

echo "Moving EVRoots.plist"
plutil -convert binary1 ${BUILT_ASSETS_DIR}/EVRoots.plist -o ${ASSET_DATA}/EVRoots.plist

echo "Moving manifest.data"
ditto ${BUILT_ASSETS_DIR}/manifest.data ${ASSET_DATA}/manifest.data

echo "Moving SystemRootCertificates.keychain"
ditto ${BUILT_KEYCHAINS_DIR}/SystemRootCertificates.keychain ${ASSET_DATA}/SystemRootCertificates.keychain

echo "Moving TrustedCTLogs.plist"
plutil -convert binary1 ${BUILT_ASSETS_DIR}/TrustedCTLogs.plist -o ${ASSET_DATA}/TrustedCTLogs.plist

echo "Moving TrustedCTLogs_nonTLS.plist"
plutil -convert binary1 ${BUILT_ASSETS_DIR}/TrustedCTLogs_nonTLS.plist -o ${ASSET_DATA}/TrustedCTLogs_nonTLS.plist

echo "Moving TrustStore.html"
ditto ${BUILT_ASSETS_DIR}/TrustStore.html ${ASSET_DATA}/TrustStore.html

if [ -d ${DSTROOT}/System ] ; then
    # clean up the temporary System directory in our DSTROOT
    if [ -e ${DSTROOT}/System/Library/Frameworks ] ; then
        # failsafe: don't expect Frameworks to exist in our build output
        echo "Error: unexpected content in DSTROOT!"
        exit 1
    fi
    echo "Removing unneeded DSTROOT output"
    rm -rf ${DSTROOT}/System
fi


