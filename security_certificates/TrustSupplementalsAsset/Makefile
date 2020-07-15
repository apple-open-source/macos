ASSET_DIR = BuiltPinningAssets
ASSET_DATA = ${ASSET_DIR}/AssetData
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
	ditto Info.plist ${ASSET_DIR}
	mkdir -p ${ASSET_DATA}
	plutil -convert binary1 ${PINNING_DIR}/CertificatePinning.plist -o  ${ASSET_DATA}/CertificatePinning.plist
	python ${ASSET_TOOLS_DIR}/BuildTrustedCTLogsPlist.py -infile ${CT_DIR}/log_list.json -outfile ${ASSET_DATA}/TrustedCTLogs.plist
	plutil -convert binary1 ${ASSET_DATA}/TrustedCTLogs.plist
	plutil -convert binary1 AnalyticsSamplingRates.plist -o  ${ASSET_DATA}/AnalyticsSamplingRates.plist
	plutil -convert binary1 AppleCertificateAuthorities.plist -o  ${ASSET_DATA}/AppleCertificateAuthorities.plist

cleanall: clean
	rm -rf ${ASSET_DIR}

clean:
	rm -rf staged

stage: asset
	echo "PKItrustSupplementals staging"
	rm -rf staged
	$(shell /usr/bin/xcrun --find assettool) stage -p . -s staged -b ${BASE_URL} -v 8
	$(shell /usr/bin/xcrun --find assettool) sign -s staged
