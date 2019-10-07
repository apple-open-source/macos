ASSET_DIR = BuiltTLSConfigAssets
ASSET_DATA = ${ASSET_DIR}/AssetData

asset:
	rm -rf ${ASSET_DIR}
	mkdir -p ${ASSET_DIR}
	ditto Info.plist ${ASSET_DIR}
	mkdir -p ${ASSET_DATA}
	ditto TLSConfig.plist ${ASSET_DIR}/AssetData

cleanall: clean
	rm -rf ${ASSET_DIR}

clean:
	rm -rf staged

stage: asset
	echo "TLSConfigs staging"
	rm -rf staged
	$(shell /usr/bin/xcrun --find assettool --sdk ioshostadditions) stage -p . -s staged -v 1
	$(shell /usr/bin/xcrun --find assettool --sdk ioshostadditions) sign -s staged

installsrc:
	$(DITTO) . $(SRCROOT)

install:
	$(PLUTIL) -convert binary1 $(SRCROOT)/Asset/Info.plist
	$(DITTO) $(SRCROOT)/Asset $(DSTROOT)/MySampleProject/
	chown -R root:wheel $(DSTROOT)
	find $(DSTROOT) -type d -exec chmod 755 {} \;
	find $(DSTROOT) -type f -exec chmod 644 {} \;
	$(DOTCLEAN) -m $(DSTROOT)

installhdrs:
	$(ECHO) "doing nothing"
