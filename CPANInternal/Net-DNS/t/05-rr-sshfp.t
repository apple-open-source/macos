# $Id: 05-rr-sshfp.t 616 2006-10-18 09:15:48Z olaf $

use Test::More;
use strict;
use Net::DNS;
use Net::DNS::RR::SSHFP;

BEGIN {
	if ($Net::DNS::RR::SSHFP::HasBabble) {
		plan tests => 15;
	} else {
		plan skip_all => 'Digest::BubbleBabble not installed.';
	}
}

#------------------------------------------------------------------------------
# Canned data.
#------------------------------------------------------------------------------

my $name			= "foo.example.com";
my $class			= "IN";
my $ttl				= 43200;

my %data = (
	type         => 'SSHFP',
	algorithm    => 2,
	fptype       => 1,
	fingerprint  => '5E66E766416A3A3A60CB150CB3F9C01C43953FB6',
);

#------------------------------------------------------------------------------
# Create the packet.
#------------------------------------------------------------------------------

my $packet = Net::DNS::Packet->new($name);
ok($packet,         'Packet created');

$packet->push('answer', 
	Net::DNS::RR->new(
		name         => $name,
		ttl          => $ttl,
		%data,
	)
);


#------------------------------------------------------------------------------
# Re-create the packet from data.
#------------------------------------------------------------------------------

my $data = $packet->data;
ok($data,            'Packet has data after pushes');

undef $packet;
$packet = Net::DNS::Packet->new(\$data);

ok($packet,          'Packet reconstructed from data');

my @answer = $packet->answer;

ok(@answer && @answer == 1, 'Packet returned correct answer section');


my $rr = $answer[0];

	
isa_ok($rr, 'Net::DNS::RR::SSHFP');

is($rr->name,    $name,       	"name() correct");         
is($rr->class,   $class,      	"class() correct");  
is($rr->ttl,     $ttl,        	"ttl() correct");                
	
foreach my $meth (keys %data) {
	is($rr->$meth(), $data{$meth}, "$meth() correct");
}
	
my $rr2 = Net::DNS::RR->new($rr->string);
is($rr2->string, $rr->string,   "Parsing from string works");

is ($rr->babble, $rr2->babble, "SSHFP - Same babble at both sides");
is ($rr->babble, "xilik-kanuk-kebek-povyf-pamus-rahob-sysoz-nibac-saben-hezur-kuxex", "SSHFP - Same matches input")	;
