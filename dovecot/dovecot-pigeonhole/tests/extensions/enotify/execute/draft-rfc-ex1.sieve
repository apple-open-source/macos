require ["enotify", "fileinto", "variables"];

if header :contains "from" "boss@example.org" {
	notify :importance "1"
		:message "This is probably very important"
		"mailto:alm@example.com";
	# Don't send any further notifications
	stop;
}

if header :contains "to" "sievemailinglist@example.org" {
	# :matches is used to get the value of the Subject header
	if header :matches "Subject" "*" {
		set "subject" "${1}";
	}

	# :matches is used to get the value of the From header
	if header :matches "From" "*" {
		set "from" "${1}";
	}

	notify :importance "3"
		:message "[SIEVE] ${from}: ${subject}"
		"mailto:alm@example.com";
	fileinto "INBOX.sieve";
}
