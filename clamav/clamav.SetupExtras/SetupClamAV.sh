#!/bin/sh
#
# Configure Clam AV 
#
echo "Configuring Anti-Virus scanner..."

# Install config files
_clamd_conf="/etc/clamd.conf"
_clamd_conf_default="/etc/clamd.conf.default"
if [ ! -e $_clamd_conf ]; then
	if [ -e $_clamd_conf_default ]; then
		cp $_clamd_conf_default $_clamd_conf
	fi
fi

chown root:wheel $_clamd_conf
chmod 644 $_clamd_conf

_freshclam_conf="/etc/freshclam.conf"
_freshclam_conf_default="/etc/freshclam.conf.default"
if [ ! -e $_freshclam_conf ]; then
	if [ -e $_freshclam_conf_default ]; then
		cp $_freshclam_conf_default $_freshclam_conf
	fi
fi

chown root:wheel $_freshclam_conf
chmod 644 $_freshclam_conf

# Default log files
_clamav_log="/var/log/clamav.log"
_freshclam_log="/var/log/freshclam.log"

# Create log files
if [ ! -e $_clamav_log ]; then
	touch $_clamav_log
fi

if [ ! -e $_freshclam_log ]; then
	touch $_freshclam_log
fi

# Set ownership and permissions
chown _amavisd:admin $_clamav_log
chown _clamav:admin $_freshclam_log

chmod 640 $_clamav_log
chmod 640 $_freshclam_log

# Make sure Clam AV data directory exists
#  and has correct permissions and ownership
_clamav_var_dir=/private/var/clamav
if [ ! -d $_clamav_var_dir ]; then
	mkdir $_clamav_var_dir
fi

chown _clamav:_clamav $_clamav_var_dir
chmod 755 $_clamav_var_dir

# Set _clamav home to /var/clamav
`/usr/bin/dscl . -create /Users/_clamav NFSHomeDirectory /var/clamav`
`/usr/bin/dscl . -append /Groups/_amavisd GroupMembership _clamav`

# Syslog settings
if ! grep '^local2\.' /etc/syslog.conf >/dev/null
then
  echo "local2.crit\t\t\t\t\t\t$_clamav_log" >> /etc/syslog.conf
  kill -1 `cat /var/run/syslog.pid`
fi

# Launch freshclam to initialize virus databases
`/bin/launchctl load -w /System/Library/LaunchDaemons/org.clamav.freshclam-init.plist`

echo "done."
