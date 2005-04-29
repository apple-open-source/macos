# $Id: 07-misc.t,v 1.1 2004/04/09 17:04:49 dasenbro Exp $

use Test::More tests => 15;
use strict;

BEGIN { use_ok('Net::DNS'); }


#
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
