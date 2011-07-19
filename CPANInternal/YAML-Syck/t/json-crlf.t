use strict;
use t::TestYAML ();
use Test::More tests => 6;

use JSON::Syck;

{
	$JSON::Syck::SingleQuote = 0;

	my $cr = JSON::Syck::Dump({ foo => "\r" });
	like $cr, qr/"\\r"/;

	my $lf = JSON::Syck::Dump({ foo => "\n" });
	like $lf, qr/"\\n"/;

	my $crlf = JSON::Syck::Dump({ foo => "\r\n" });
	like $crlf, qr/"\\r\\n"/;
}

{
	$JSON::Syck::SingleQuote = 1;

	my $cr = JSON::Syck::Dump({ foo => "\r" });
	like $cr, qr/'\\r'/;

	my $lf = JSON::Syck::Dump({ foo => "\n" });
	like $lf, qr/'\\n'/;

	my $crlf = JSON::Syck::Dump({ foo => "\r\n" });
	like $crlf, qr/'\\r\\n'/;
}

