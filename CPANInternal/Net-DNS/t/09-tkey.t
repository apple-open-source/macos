#  $Id: 09-tkey.t 616 2006-10-18 09:15:48Z olaf $    -*-perl-*-

use Test::More tests => 7;
use strict;
use Digest::HMAC_MD5;

BEGIN { use_ok('Net::DNS'); } #1


sub is_empty {
	my ($string) = @_;
	return ($string eq "; no data" || $string eq "; rdlength = 0");
}

#------------------------------------------------------------------------------
# Canned data.
#------------------------------------------------------------------------------

my $zone	= "example.com";
my $name	= "123456789-test";
my $class	= "IN";
my $type	= "TKEY";
my $algorithm   = "fake.algorithm.example.com";
my $key         = "fake key";
my $inception   = 100000; # use a strange fixed inception time to give a fixed
                          # checksum
my $expiration  = $inception + 24*60*60;

my $rr      = undef;

#------------------------------------------------------------------------------
# Packet creation.
#------------------------------------------------------------------------------

$rr = Net::DNS::RR->new(
	Name       => "$name",
	Type       => "TKEY",
	TTL        => 0,
	Class      => "ANY",
	algorithm  => $algorithm,
	inception  => $inception,
	expiration => $expiration,
	mode       => 3, # GSSAPI
	key        => "fake key",
	other_data => "",
);

my $packet = Net::DNS::Packet->new("$name", "TKEY", "IN");
$packet->push("answer", $rr);

my $z = ($packet->zone)[0];

ok($packet,                                'new() returned packet');  #2
is($packet->header->opcode, 'QUERY',       'header opcode correct');  #3 
is($z->zname,  $name,                      'zname correct');          #4
is($z->zclass, "IN",                       'zclass correct');         #5
is($z->ztype,  'TKEY',                     'ztype correct');          #6       


#------------------------------------------------------------------------------
# create a signed TKEY query packet using an external signing function
# and compare it to a known good result. This effectively tests the
# sign_func and sig_data methods of TSIG as well.
#------------------------------------------------------------------------------

sub fake_sign {
	my ($key, $data) = @_;

	my $hmac = Digest::HMAC_MD5->new($key);
	$hmac->add($data);

	return $hmac->hexdigest;
}

my $tsig = Net::DNS::RR->new(
	Name        => $name,
	Type        => "TSIG",
	TTL         => 0,
	Class       => "ANY",
	Algorithm   => $algorithm,
	Time_Signed => $inception + 1,
	Fudge       => 36000,
	Mac_Size    => 0,
	Mac         => "",
	Key         => $key,
	Sign_Func   => \&fake_sign,
	Other_Len   => 0,
	Other_Data  => "",
	Error       => 0,
);

$packet->push("additional", $tsig);

# use a fixed packet id so we get a known checksum
$packet->header->id(1234);

# create the packet - this will fill in the 'mac' field
my $raw_packet = $packet->data;

is(($packet->additional)[0]->mac, 
   "6365643161343964663364643264656131306638303633626465366236643465",
   'MAC correct');

