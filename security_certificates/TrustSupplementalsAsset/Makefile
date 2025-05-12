ASSET_DIR = BuiltPinningAssets
ASSET_DIR2 = BuiltPinningAssets2
ASSET_DATA = ${ASSET_DIR}/AssetData
ASSET_DATA2 = ${ASSET_DIR2}/AssetData
ASSET_TOOLS_DIR = ../CertificateTool/BuildiOSAsset/
CT_DIR = ../certificate_transparency/
PINNING_DIR = ../Pinning
BASE_URL = "https://basejumper.apple.com/assets/"

# Set a fallback if SDKROOT or RC_PLATFORM_NAME are not defined
SDKROOT ?= $(shell xcrun --sdk iphoneos.internal --show-sdk-path)
RC_PLATFORM_NAME ?= iPhoneOS

# Use xcrun (which knows about SDKROOT) to find the correct tools
SIGNING_TOOL = $(shell xcrun -f sign_file)

asset:
	rm -rf ${ASSET_DIR}
	mkdir -p ${ASSET_DIR}
	ditto v1/Info.plist ${ASSET_DIR}
	mkdir -p ${ASSET_DATA}
	plutil -convert binary1 ${PINNING_DIR}/CertificatePinning.plist -o  ${ASSET_DATA}/CertificatePinning.plist
	python ${ASSET_TOOLS_DIR}/BuildTrustedCTLogsPlist.py -infile ${CT_DIR}/log_list.json -outfile ${ASSET_DATA}/TrustedCTLogs.plist
	plutil -convert binary1 ${ASSET_DATA}/TrustedCTLogs.plist
	python ${ASSET_TOOLS_DIR}/BuildTrustedCTLogsPlist.py -infile ${CT_DIR}/log_list.json -outfile ${ASSET_DATA}/TrustedCTLogs_nonTLS.plist
	plutil -convert binary1 ${ASSET_DATA}/TrustedCTLogs_nonTLS.plist
	plutil -convert binary1 AnalyticsSamplingRates.plist -o  ${ASSET_DATA}/AnalyticsSamplingRates.plist
	plutil -convert binary1 AppleCertificateAuthorities.plist -o  ${ASSET_DATA}/AppleCertificateAuthorities.plist

asset2:
	rm -rf ${ASSET_DIR2}
	mkdir -p ${ASSET_DIR2}
	ditto v2/Info.plist ${ASSET_DIR2}
	mkdir -p ${ASSET_DATA2}
	ditto ${ASSET_DATA}/CertificatePinning.plist ${ASSET_DATA2}/CertificatePinning.plist
	ditto ${ASSET_DATA}/TrustedCTLogs.plist ${ASSET_DATA2}/TrustedCTLogs.plist
	ditto ${ASSET_DATA}/TrustedCTLogs_nonTLS.plist ${ASSET_DATA2}/TrustedCTLogs_nonTLS.plist
	ditto ${ASSET_DATA}/AnalyticsSamplingRates.plist ${ASSET_DATA2}/AnalyticsSamplingRates.plist
	ditto ${ASSET_DATA}/AppleCertificateAuthorities.plist ${ASSET_DATA2}/AppleCertificateAuthorities.plist

cleanall: clean
	rm -rf ${ASSET_DIR}
	rm -rf ${ASSET_DIR2}

clean:
	rm -rf staged

stage: asset asset2
	echo "PKITrustSupplementals staging"
	rm -rf staged
	$(shell /usr/bin/xcrun --find assettool) stage -p . -s staged -b ${BASE_URL} -v 1006
	$(shell /usr/bin/xcrun --find assettool) sign -s staged
