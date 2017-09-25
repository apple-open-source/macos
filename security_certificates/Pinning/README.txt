Much of this description is courtesy of Tyler Hawkins in the BridgeAssets project

### Common Terms related to MobileAsset ###

* Mesu - the production / public-facing host for MobileAssets.
* Basejumper - the internal server for hosting / testing MobileAssets. Users must be within the internal network and have a default set in order to reach Basejumper.
	To get internal devices to point to Basejumper do:
		asutil --set-asset-server-url https://basejumper.apple.com/assets/ --asset-type com.apple.MobileAsset.CertificatePinning
* Styx - the train which (probably among other things) builds projects into MobileAssets and moves them to Basejumper.
* Beatbox - notification-based interface for kicking off builds in (our case) the Styx train.

### General notes about CertificatePinning and MobileAsset ###

* B&I does the important assettool work for us.
* This means that our primary responsibility is just to land our AssetProject dir in the correct place (which is the DSTROOT) and they will do 'assettool -install' for us.
* As such, if you are having issues getting things to show up in Basejumper I find that generally the best thing to check is the DSTROOT for your latest tag/submission,
* Never check-in or submit assettool output which you've staged yourself, it will just confuse things.
* Update the version number in the CertificatePinning.plist as well as the _ContentVersion in the Info.plist.
* We use the xcodeproject target CertificatePinningAsset in B&I. Locally, you can use the Makefile.

### Testing assettool Ingestion of CertificatePinningProject ###
1. While in the repo dir simply do:
    $ make stage
2. If you are satisfied with the output you probably want to cleanup:
    $ make cleanall

### Steps to Deploy CertificatePinning Changes to Basejumper: ###

1. Make changes to repo; get them reviewed, committed, etc.
2. Submit CertificatePinning to Styx...
    $ ~rc/bin/submitproject -project CertificatePinningAsset -version <+1 on version> -git -url . -tag <current security_certificates tag> Styx
3. Visit Beatbox and kick off a build for Styx/CertificatePinning (https://beatbox.apple.com/BuildDispatch/index.php?train=Styx).
4. Deploying to Basejumper may take a few moments.
5. Visit Basejumper to make sure XML and asset changes are reflected (https://basejumper.apple.com/assets/com_apple_MobileAsset_CertificatePinning/com_apple_MobileAsset_CertificatePinning.xml).

### Steps to Deploy CertificatePinning Changes to Mesu: ###

1. Before B&I (garfinkle@apple.com as of May 2016) can ditto our data from BaseJumper to Mesu QA will need to verify the BaseJumper assets against the app.
2. Once QA is satisfied with the asset B&I will ditto your assets to a Mesu-Staging area of basejumper; probably somewhere like https://basejumper.apple.com/mesu_staging/com_apple_MobileAsset_CertificatePinning/com_apple_MobileAsset_CertificatePinning.xml.
3. QA will now need to test the change using the profile attached to rdar://problem/21495876 in order to configure basejumper staging as the asset URL for production devices.
4. Once QA is satisfied with your asset in this environment B&I will ditto your assets to Mesu proper; probably somewhere like mesu.apple.com/assets/com_apple_MobileAsset_CertificatePinning/com_apple_MobileAsset_CertificatePinning.xml.
