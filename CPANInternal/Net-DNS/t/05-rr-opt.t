# $Id: 05-rr-opt.t 616 2006-10-18 09:15:48Z olaf $   -*-perl-*-

use Test::More tests => 7;
use strict;

BEGIN { use_ok('Net::DNS'); }

my $size=2048;
my $ednsflags=0x9e22;


my $optrr= Net::DNS::RR->new(
			  Type         => 'OPT',
			  Name         => '',
			  Class        => $size,  # Decimal UDPpayload
			  ednsflags    => $ednsflags, # first bit set see RFC 3225
			  );
ok($optrr->do,"DO bit set");
is($optrr->clear_do,0x1e22,"Clearing do, leaving the other bits ");
ok(!$optrr->do,"DO bit cleared");
is($optrr->set_do,0x9e22,"Clearing do, leaving the other bits ");


is($optrr->size(),2048,"Size read");
is($optrr->size(1498),1498,"Size set");
