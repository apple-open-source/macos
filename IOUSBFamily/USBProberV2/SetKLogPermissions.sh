cd /System/Library/Extensions/
/usr/sbin/chown -R root:wheel KLog.kext
find KLog.kext -type d -exec /bin/chmod 0755 {} \;
find KLog.kext -type f -exec /bin/chmod 0644 {} \;
