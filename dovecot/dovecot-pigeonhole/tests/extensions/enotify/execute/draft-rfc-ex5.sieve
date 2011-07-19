require ["enotify"];

if notify_method_capability
	"xmpp:tim@example.com?message;subject=SIEVE"
	"Online"
	"yes" {
	notify :importance "1" :message "You got mail"
		"xmpp:tim@example.com?message;subject=SIEVE";
} else {
	notify :message "You got mail" "tel:+14085551212";
}
