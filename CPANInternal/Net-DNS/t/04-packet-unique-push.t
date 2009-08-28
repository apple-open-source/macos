# $Id: 04-packet-unique-push.t 704 2008-02-06 21:30:59Z olaf $

use Test::More tests => 77;
use strict;

BEGIN { use_ok('Net::DNS'); }     #1


# Matching of RR name is not case sensitive 
my $packet=Net::DNS::Packet->new();

my $rr_1=Net::DNS::RR->new('bla.FOO  100 IN TXT "lower case"');
my $rr_2=Net::DNS::RR->new('bla.foo  100 IN TXT "lower case"');
my $rr_3=Net::DNS::RR->new('bla.foo 100 IN TXT "MIXED CASE"');
my $rr_4=Net::DNS::RR->new('bla.foo 100 IN TXT "mixed case"');

$packet->unique_push("answer",$rr_1);
$packet->unique_push("answer",$rr_2);
is($packet->header->ancount,1,"unique_push, case sensitivity test 1");

$packet->unique_push("answer",$rr_3);
$packet->unique_push("answer",$rr_4);
is($packet->header->ancount,3,"unique_push, case sensitivity test 2");



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
			1,
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

