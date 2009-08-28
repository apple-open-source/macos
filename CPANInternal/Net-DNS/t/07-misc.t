# $Id: 07-misc.t 625 2007-01-24 14:35:58Z olaf $ -*-perl-*-

use Test::More tests => 37;
use strict;

BEGIN { use_ok('Net::DNS'); 
}



# test to make sure that wildcarding works.
#
my $rr;
eval { $rr = Net::DNS::RR->new('*.t.net-dns.org 60 IN A 10.0.0.1'); };

ok($rr, 'RR got made');

is($rr->name,    '*.t.net-dns.org', 'Name is correct'   );
is($rr->ttl,      60,               'TTL is correct'    );
is($rr->class,   'IN',              'CLASS is correct'  );
is($rr->type,    'A',               'TYPE is correct'   );
is($rr->address, '10.0.0.1',        'Address is correct');

#
# Make sure the underscore in SRV hostnames work.
#
my $srv;
eval { $srv = Net::DNS::RR->new('_rvp._tcp.t.net-dns.org. 60 IN SRV 0 0 80 im.bastardsinc.biz'); };

ok(!$@,  'No errors');
ok($srv, 'SRV got made');



#
# Test that the 5.005 Use of uninitialized value at
# /usr/local/lib/perl5/site_perl/5.005/Net/DNS/RR.pm line 639. bug is gone
#
my $warning = 0;
{
	
	local $^W = 1;
	local $SIG{__WARN__} = sub { $warning++ };
	
	my $rr = Net::DNS::RR->new('mx.t.net-dns.org 60 IN MX 10 a.t.net-dns.org');
	ok($rr, 'RR created');

	is($rr->preference, 10, 'Preference works');
}

is($warning, 0, 'No evil warning');


{
	my $mx = Net::DNS::RR->new('mx.t.net-dns.org 60 IN MX 0 mail.net-dns.org');
	
	like($mx->string, '/0 mail.net-dns.org/');
	is($mx->preference, 0);
	is($mx->exchange, 'mail.net-dns.org');
}

{
	my $srv = Net::DNS::RR->new('srv.t.net-dns.org 60 IN SRV 0 2 3 target.net-dns.org');
	
	like($srv->string, '/0 2 3 target.net-dns.org\./');
	is($srv->rdatastr, '0 2 3 target.net-dns.org.');
}





#
#
# Below are some thests that have to do with TXT RRs 
#
#


#;; QUESTION SECTION:
#;txt2.t.net-dns.org.		IN	TXT

#;; ANSWER SECTION:
#txt2.t.net-dns.org.	60	IN	TXT	"Net-DNS\; complicated $tuff" "sort of \" text\; and binary \000 data"

#;; AUTHORITY SECTION:
#net-dns.org.		3600	IN	NS	ns1.net-dns.org.
#net-dns.org.		3600	IN	NS	ns.ripe.net.
#net-dns.org.		3600	IN	NS	ns.hactrn.net.

#;; ADDITIONAL SECTION:
#ns1.net-dns.org.	3600	IN	A	193.0.4.49
#ns1.net-dns.org.	3600	IN	AAAA

my $UUencodedPacket='
11 99 85 00 00 01
00 01 00 03 00 02 04 74  78 74 32 01 74 07 6e 65
74 2d 64 6e 73 03 6f 72  67 00 00 10 00 01 c0 0c
00 10 00 01 00 00 00 3c  00 3d 1a 4e 65 74 2d 44
4e 53 3b 20 63 6f 6d 70  6c 69 63 61 74 65 64 20
24 74 75 66 66 21 73 6f  72 74 20 6f 66 20 22 20
74 65 78 74 3b 20 61 6e  64 20 62 69 6e 61 72 79
20 00 20 64 61 74 61 c0  13 00 02 00 01 00 00 0e
10 00 06 03 6e 73 31 c0  13 c0 13 00 02 00 01 00
00 0e 10 00 0d 02 6e 73  04 72 69 70 65 03 6e 65
74 00 c0 13 00 02 00 01  00 00 0e 10 00 0c 02 6e
73 06 68 61 63 74 72 6e  c0 93 c0 79 00 01 00 01
00 00 0e 10 00 04 c1 00  04 31 c0 79 00 1c 00 01
00 00 0e 10 00 10 20 01  06 10 02 40 00 03 00 00
12 34 be 21 e3 1e                               
';




$UUencodedPacket =~ s/\s*//g;
my $packetdata = pack('H*',$UUencodedPacket);
my $packet     = Net::DNS::Packet->new(\$packetdata);
my $TXTrr=($packet->answer)[0];
is(($TXTrr->char_str_list())[0],'Net-DNS; complicated $tuff',"First Char string in TXT RR read from wireformat");

# Compare the second char_str this contains a NULL byte (space NULL
# space=200020 in hex)

is(unpack('H*',($TXTrr->char_str_list())[1]),"736f7274206f66202220746578743b20616e642062696e61727920002064617461", "Second Char string in TXT RR read from wireformat");


my $TXTrr2=Net::DNS::RR->new('txt2.t.net-dns.org.	60	IN	TXT  "Test1 \" \; more stuff"  "Test2"');

is(($TXTrr2->char_str_list())[0],'Test1 " ; more stuff', "First arg string in TXT RR read from zonefileformat");
is(($TXTrr2->char_str_list())[1],'Test2',"Second Char string in TXT RR read from zonefileformat");


my $TXTrr3   = Net::DNS::RR->new("baz.example.com 3600 HS TXT '\"' 'Char Str2'");

is( ($TXTrr3->char_str_list())[0],'"',"Escaped \" between the  single quotes");






ok(Net::DNS::Resolver::Base::_ip_is_ipv4("10.0.0.9"),"_ip_is_ipv4, test 1");

ok(Net::DNS::Resolver::Base::_ip_is_ipv4("1"),"_ip_is_ipv4, test 2");

# remember 1.1 expands to 1.0.0.1 and is legal.
ok( Net::DNS::Resolver::Base::_ip_is_ipv4("1.1"),"_ip_is_ipv4, test 3");

ok( ! Net::DNS::Resolver::Base::_ip_is_ipv4("256.1.0.9"),"_ip_is_ipv4, test 4");

ok( ! Net::DNS::Resolver::Base::_ip_is_ipv4("10.11.12.13.14"),"_ip_is_ipv4, test 5");

ok(Net::DNS::Resolver::Base::_ip_is_ipv6("::1"),"_ip_is_ipv6, test 1");
ok(Net::DNS::Resolver::Base::_ip_is_ipv6("1::1"),"_ip_is_ipv6, test 2");
ok(Net::DNS::Resolver::Base::_ip_is_ipv6("1::1:1"),"_ip_is_ipv6, test 3");
ok(! Net::DNS::Resolver::Base::_ip_is_ipv6("1::1:1::1"),"_ip_is_ipv6, test 4");
ok(Net::DNS::Resolver::Base::_ip_is_ipv6("1:2:3:4:4:6:7:8"),"_ip_is_ipv6, test 5");
ok(! Net::DNS::Resolver::Base::_ip_is_ipv6("1:2:3:4:4:6:7:8:9"),"_ip_is_ipv6, test 6");


ok( Net::DNS::Resolver::Base::_ip_is_ipv6("0001:0002:0003:0004:0004:0006:0007:0008"),"_ip_is_ipv6, test 7");

ok( Net::DNS::Resolver::Base::_ip_is_ipv6("abcd:ef01:2345:6789::"),"_ip_is_ipv6, test 8");

ok(! Net::DNS::Resolver::Base::_ip_is_ipv6("abcd:efgh:2345:6789::"),"_ip_is_ipv6, test 9");

ok( Net::DNS::Resolver::Base::_ip_is_ipv6("0001:0002:0003:0004:0004:0006:0007:10.0.0.1"),"_ip_is_ipv6, test 10");

