
/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/csent \
   scripts/em-ent.plist \
   scripts/em-ent.ent

base=/tmp/Heimdal-em.dst

for a in kinit klist kdestroy kswitch kgetcred ; do
    codesign -f --entitlements=scripts/em-ent.ent -s 'iPhone Developer:' \
	$base/usr/local/bin/$a
done

for a in gsstool ; do
    codesign -f --entitlements=scripts/em-ent.ent -s 'iPhone Developer:' \
	$base/System/Library/PrivateFrameworks/Heimdal.framework/Helpers/$a
done


