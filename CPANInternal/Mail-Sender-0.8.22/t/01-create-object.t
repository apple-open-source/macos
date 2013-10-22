#!perl -T
use strict;
use Test::More tests => 5;

BEGIN {
	use_ok( 'Mail::Sender' );
}

my $sender = Mail::Sender->new({tls_allowed => 0});

ok( ($sender > 0), "created the object with default settings")
 or do { diag( "  Error: $Mail::Sender::Error"); exit};

SKIP: {
	skip "No SMTP server set in the default config", 3 unless $sender->{smtp};

	ok( $sender->{smtpaddr}, "smtpaddr defined");

	my $res = $sender->Connect();
	ok( (ref($res) or $res >=0), "->Connect()")
		or do { diag("Error: $Mail::Sender::Error"); exit};

	ok( ($sender->{'supports'} and ref($sender->{'supports'}) eq 'HASH'), "found out what extensions the server supports");
};
