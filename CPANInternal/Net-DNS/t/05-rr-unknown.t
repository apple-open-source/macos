# $Id: 05-rr-unknown.t 639 2007-05-25 12:00:15Z olaf $   -*-perl-*-
#
# RFC 3597 Unknown typecode implemntation test code.
# O.M. Kolkman RIPE NCC.
#


use Test::More tests => 18;
use strict;


BEGIN { use_ok('Net::DNS'); }




is(Net::DNS::typesbyname('TYPE10226'), 10226,      'typesbyname(TYPE10226) returns 10226');
is(Net::DNS::typesbyval(10226),        'TYPE10226','typesbyval(10226) returns TYPE10226');
is(Net::DNS::typesbyval(1),            'A','       typesbyval(1) returns A');

is(Net::DNS::typesbyval(Net::DNS::typesbyname('TYPE001')), 'A', 'typesbyval(typebyname(TYPE001)) returns A');


eval {
	Net::DNS::typesbyval(0xffff+1);
};


like($@, '/Net::DNS::typesbyval\(\) argument larger than 65535/', 'Fails on large TYPE code');


is(Net::DNS::classesbyname('CLASS124'), 124,       'classesbyname(CLASS124) returns 124');
is(Net::DNS::classesbyval(125),         'CLASS125','classesbyval(125) returns CLASS125');
is(Net::DNS::classesbyval(1),           'IN',      'classesbyval(1) returns IN');

is(Net::DNS::classesbyval(Net::DNS::classesbyname('CLASS04')), 'HS', 'classesbyval(typebyname(CLASS04)) returns HS');

eval {
    Net::DNS::classesbyval(0xffff+1);
};

like($@, '/Net::DNS::classesbyname\(\) argument larger than 65535/', 'Fails on large CLASS code');


{
	my $rr = Net::DNS::RR->new('e.example CLASS01 TYPE01 10.0.0.2');
	is($rr->type,  'A', 'TYPE01 parsed OK');
	is($rr->class,'IN', 'CLASS01 parsed OK');
}
{
	my $rr = Net::DNS::RR->new('e.example IN A \# 4  0A0000 01  ');
	is($rr->address,'10.0.0.1', 'Unknown RR representation for A parsed OK');
}

eval {
   Net::DNS::RR->new('e.example IN A \# 4  0A0000 01 11 ');
};

like($@, '/\\\# 4  0A0000 01 11 is inconsistent\; length does not match content/', 'Fails on inconsistent length and hex presentation');

{
	my $rr = Net::DNS::RR->new('e.example IN TYPE4555 \# 4  0A0000 01  ');
	is($rr->string, 'e.example.	0	IN	TYPE4555	\# 4 0a000001', 'Fully unknown RR parsed correctly');
}
{
	my $rr4 = Net::DNS::RR->new('e.example CLASS122 TYPE4555 \# 4  0A0000 01  ');
	is($rr4->string, 'e.example.	0	CLASS122	TYPE4555	\# 4 0a000001', 'Fully unknown RR in unknown CLASS parsed correctly');
}

my $UUencodedPacket='
02 79 85 00 00 01 
00 01 00 01 00 01 04 54  45 53 54 07 65 78 61 6d 
70 6c 65 03 63 6f 6d 00  00 ff 00 01 c0 0c 30 39 
00 01 00 00 00 7b 00 0a  11 22 33 44 55 aa bb cc 
dd ee c0 11 00 02 00 01  00 00 03 84 00 05 02 6e 
73 c0 11 c0 44 00 01 00  01 00 00 03 84 00 04 7f 
00 00 01';
                                                       
$UUencodedPacket =~ s/\s*//g;


my $packetdata = pack('H*',$UUencodedPacket);
my $packet     = Net::DNS::Packet->new(\$packetdata);

my $string_representation = ($packet->answer)[0]->string;
$string_representation =~ s/\s+/ /g,
is (
	$string_representation,
	'TEST.example.com. 123 IN TYPE12345 \# 10 1122334455aabbccddee',
	'Packet read from a packet dumped by bind...'
);



