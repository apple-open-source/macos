require ["enotify", "fileinto", "variables", "envelope"];

if header :matches "from" "*@*.example.org" {
	# :matches is used to get the MAIL FROM address
	if envelope :all :matches "from" "*" {
		set "env_from" " [really: ${1}]";
	}

	# :matches is used to get the value of the Subject header
	if header :matches "Subject" "*" {
		set "subject" "${1}";
	}

	# :matches is used to get the address from the From header
	if address :matches :all "from" "*" {
		set "from_addr" "${1}";
	}

	notify :message "${from_addr}${env_from}: ${subject}"
		"mailto:alm@example.com";
}

