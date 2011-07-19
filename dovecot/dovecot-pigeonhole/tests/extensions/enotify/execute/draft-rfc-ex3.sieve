require ["enotify", "variables"];

set "notif_method"
	"xmpp:tim@example.com?message;subject=SIEVE;body=You%20got%20mail";

if header :contains "subject" "Your dog" {
	set "notif_method" "tel:+14085551212";
}

if header :contains "to" "sievemailinglist@example.org" {
	set "notif_method" "";
}

if not string :is "${notif_method}" "" {
	notify "${notif_method}";
}

if header :contains "from" "boss@example.org" {
	# :matches is used to get the value of the Subject header
	if header :matches "Subject" "*" {
		set "subject" "${1}";
	}

	# don't need high importance notification for
	# a 'for your information'
	if not header :contains "subject" "FYI:" {
		notify :importance "1" :message "BOSS: ${subject}"
			"tel:+14085551212";
	}
}

