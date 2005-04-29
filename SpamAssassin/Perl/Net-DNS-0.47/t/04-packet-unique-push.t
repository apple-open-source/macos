# $Id: 04-packet-unique-push.t,v 1.1 2004/04/09 17:04:48 dasenbro Exp $

use Test::More tests => 75;
use strict;

BEGIN { use_ok('Net::DNS'); }     #1


my $tests = sub {
	my ($method) = @_;
	my $domain = 'example.com';

	my @tests = (
		[ 
			1,
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.1'),
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.1'),
		],
		[
			2,
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.1'),
			Net::DNS::RR->new_from_string('bar.example.com 60 IN A 10.0.0.1'),
		],
		[ 
			2,
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.1'),
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.1'),
			Net::DNS::RR->new_from_string('foo.example.com 90 IN A 10.0.0.1'),
		],
		[ 
			3,
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.1'),
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.2'),
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.3'),
		],
		[ 
			3,
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.1'),
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.2'),
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.3'),
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.1'),
		],
		[ 
			3,
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.1'),
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.2'),
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.1'),
			Net::DNS::RR->new_from_string('foo.example.com 60 IN A 10.0.0.4'),
		],
	);
	
	my %sections = (
		answer     => 'ancount',
		authority  => 'nscount',
		additional => 'arcount',
	);
	
	foreach my $try (@tests) {
		my ($count, @rrs) = @$try;
	
		while (my ($section, $count_meth) = each %sections) {
		
			my $packet = Net::DNS::Packet->new($domain);
			
			$packet->$method($section, @rrs);
		
			is($packet->header->$count_meth(), $count, "$section right");
	
		}
	
		#
		# Now do it again calling safe_push() for each RR.
		# 
		while (my ($section, $count_meth) = each %sections) {
		
			my $packet = Net::DNS::Packet->new($domain);
			
			foreach (@rrs) {
				$packet->$method($section, $_);
			}
		
			is($packet->header->$count_meth(), $count, "$section right");
		}
	}
};

$tests->('unique_push');

{
	my @warnings;
	local $SIG{__WARN__} = sub { push(@warnings, "@_"); };
	
	$tests->('safe_push');
	
	is(scalar @warnings, 72);
	
	ok(!grep { $_ !~ m/deprecated/ } @warnings);
}
	
