#!/bin/sh
# -e: exit on error, -u: error on unset variables, -x: show commands
set -e -u -x

ASSET_DIR=${DSTROOT}/BuiltSupplementalAssets
ASSET_DIR2=${DSTROOT}/BuiltSupplementalAssets2
ASSET_DATA=${ASSET_DIR}/AssetData
ASSET_DATA2=${ASSET_DIR2}/AssetData
ASSET_TOOLS_DIR=${SRCROOT}/CertificateTool/BuildiOSAsset
CT_DIR=${SRCROOT}/certificate_transparency
PINNING_DIR=${SRCROOT}/Pinning
SRC_ASSET_DIR=${SRCROOT}/TrustSupplementalsAsset

mkdir -p ${ASSET_DIR}
ditto ${SRC_ASSET_DIR}/v1/Info.plist ${ASSET_DIR}
mkdir -p ${ASSET_DATA}

mkdir -p ${ASSET_DIR2}
ditto ${SRC_ASSET_DIR}/v2/Info.plist ${ASSET_DIR2}
mkdir -p ${ASSET_DATA2}

echo "Starting TrustedCTLogs.plist file build"
python ${ASSET_TOOLS_DIR}/BuildTrustedCTLogsPlist.py -infile ${CT_DIR}/log_list.json -outfile ${ASSET_DATA}/TrustedCTLogs.plist
STATUS=$?
if [ $STATUS -ne 0 ] ; then
echo "BuildTrustedCTLogsPlist.rb failed with exit status $STATUS"
    exit 1
fi
ditto ${ASSET_DATA}/TrustedCTLogs.plist ${ASSET_DATA2}/TrustedCTLogs.plist

echo "Starting TrustedCTLogs_nonTLS.plist file build"
python ${ASSET_TOOLS_DIR}/BuildTrustedCTLogsPlist.py -infile ${CT_DIR}/log_list.json -outfile ${ASSET_DATA}/TrustedCTLogs_nonTLS.plist
STATUS=$?
if [ $STATUS -ne 0 ] ; then
echo "BuildTrustedCTLogsPlist.rb failed with exit status $STATUS"
    exit 1
fi
ditto ${ASSET_DATA}/TrustedCTLogs_nonTLS.plist ${ASSET_DATA2}/TrustedCTLogs_nonTLS.plist

echo "Converting TrustedCTLogs.plist"
plutil -convert binary1 ${ASSET_DATA}/TrustedCTLogs.plist
ditto ${ASSET_DATA}/TrustedCTLogs.plist ${ASSET_DATA2}/TrustedCTLogs.plist

echo "Converting TrustedCTLogs_nonTLS.plist"
plutil -convert binary1 ${ASSET_DATA}/TrustedCTLogs_nonTLS.plist
ditto ${ASSET_DATA}/TrustedCTLogs_nonTLS.plist ${ASSET_DATA2}/TrustedCTLogs_nonTLS.plist

echo "Moving CertificatePinning.plist"
plutil -convert binary1 ${PINNING_DIR}/CertificatePinning.plist -o ${ASSET_DATA}/CertificatePinning.plist
ditto ${ASSET_DATA}/CertificatePinning.plist ${ASSET_DATA2}/CertificatePinning.plist

echo "Moving AnalyticsSamplingRates.plist"
plutil -convert binary1 ${SRC_ASSET_DIR}/AnalyticsSamplingRates.plist -o ${ASSET_DATA}/AnalyticsSamplingRates.plist
ditto ${ASSET_DATA}/AnalyticsSamplingRates.plist ${ASSET_DATA2}/AnalyticsSamplingRates.plist

echo "Moving AppleCertificateAuthorities.plist"
plutil -convert binary1 ${SRC_ASSET_DIR}/AppleCertificateAuthorities.plist -o ${ASSET_DATA}/AppleCertificateAuthorities.plist
ditto ${ASSET_DATA}/AppleCertificateAuthorities.plist ${ASSET_DATA2}/AppleCertificateAuthorities.plist
