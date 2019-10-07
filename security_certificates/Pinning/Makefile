ASSET_DIR = "BuiltPinningAssets"

# Set a fallback if SDKROOT or RC_PLATFORM_NAME are not defined
SDKROOT ?= $(shell xcrun --sdk iphoneos.internal --show-sdk-path)
RC_PLATFORM_NAME ?= iPhoneOS

# Use xcrun (which knows about SDKROOT) to find the correct tools
SIGNING_TOOL = $(shell xcrun -f sign_file)

asset:
	rm -rf ${ASSET_DIR}
	mkdir -p ${ASSET_DIR}
	ditto Info.plist ${ASSET_DIR}
	mkdir -p ${ASSET_DIR}/AssetData
	ditto CertificatePinning.plist ${ASSET_DIR}/AssetData

cleanall: clean
	rm -rf ${ASSET_DIR}

clean:
	rm -rf staged

stage: asset
	echo "CertificatePinning staging"
	rm -rf staged
	$(shell /usr/bin/xcrun --find assettool --sdk ioshostadditions) stage -p . -s staged -v 40
	$(shell /usr/bin/xcrun --find assettool --sdk ioshostadditions) sign -s staged
