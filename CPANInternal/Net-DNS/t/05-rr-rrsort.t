# $Id: 05-rr-rrsort.t 682 2007-09-27 07:50:27Z olaf $   -*-perl-*-

use Test::More;
use strict;
use Net::DNS qw(rrsort);

plan tests => 22;

my $rr1=Net::DNS::RR->new("example.com.  600     IN      SRV     0 0 5060 A.example.com.");
is(ref($rr1),"Net::DNS::RR::SRV","SRV RR1 created");
my $rr2=Net::DNS::RR->new("example.com.  600     IN      SRV     1 0 5060 A.example.com.");
is(ref($rr2),"Net::DNS::RR::SRV","SRV RR2 created");
my $rr3=Net::DNS::RR->new("example.com.  600     IN      SRV     2 0 5060 A.example.com.");
is(ref($rr3),"Net::DNS::RR::SRV","SRV RR3 created");
my $rr4=Net::DNS::RR->new("example.com.  600     IN      SRV     3 0 5060 A.example.com.");
is(ref($rr4),"Net::DNS::RR::SRV","SRV RR4 created");
my $rr5=Net::DNS::RR->new("example.com.  600     IN      SRV     3 1 5060 A.example.com.");
is(ref($rr5),"Net::DNS::RR::SRV","SRV RR5 created");
my $rr6=Net::DNS::RR->new("example.com.  600     IN      SRV     3 2 5060 A.example.com.");
is(ref($rr6),"Net::DNS::RR::SRV","SRV RR6 created");
my $rr7=Net::DNS::RR->new("example.com.  600     IN      SRV     1 3 5070 A.example.com.");
is(ref($rr7),"Net::DNS::RR::SRV","SRV RR7 created");
my $rr8=Net::DNS::RR->new("example.com.  600     IN      SRV     3 3 5070 A.example.com.");
is(ref($rr8),"Net::DNS::RR::SRV","SRV RR8 created");
my $rr9=Net::DNS::RR->new("example.com.  600     IN     A 192.168.0.1");
is(ref($rr9),"Net::DNS::RR::A","A RR9 created");


my @rrarray=($rr1, $rr2, $rr3, $rr4, $rr5, $rr6, $rr7, $rr8, $rr9);
my @expectedrdata=($rr1, $rr2, $rr3, $rr7, $rr4, $rr5, $rr6,  $rr8);
my @expectedpriority=($rr1, $rr7, $rr2, $rr3, $rr8, $rr6, $rr5, $rr4);
my @expectedweight=($rr7, $rr8, $rr6, $rr5, $rr1, $rr2, $rr3, $rr4);



is (rrsort("SRV"),undef,"rrsort returns rrerly whith undefined arguments");

is (rrsort("SRV",@rrarray),8,"rrsort returns properly whith undefined attribute (1)");

is (rrsort("SRV",,@rrarray),8,"rrsort returns properly whith undefined attribute (2)");

is (rrsort("SRV","",@rrarray),8,"rrsort returns properly whith undefined attribute (3)");

my @prioritysorted= rrsort("SRV","priority",@rrarray);
my @weightsorted= rrsort("SRV","weight",@rrarray);
my @defaultsorted= rrsort("SRV",@rrarray);
my @portsorted= rrsort("SRV","port",@rrarray);

my @foosorted= rrsort("SRV","foo",@rrarray);
is (@foosorted,8,"rrsort returns properly whith undefined attribute (3)");

is ( @prioritysorted,8,"rrsort correctly maintains RRs test 2");


ok (eq_array(\@expectedpriority, \@prioritysorted), "Sorting on SRV priority works");
ok (eq_array(\@expectedpriority, \@defaultsorted), "Default SRV sort works");
ok (eq_array(\@expectedweight, \@weightsorted), "Weight sorted SRV sort works");


is (rrsort("A","priority",@rrarray),1,"rrsort correctly maintains RRs test 1");
is (rrsort("MX","priority",@rrarray),undef,"rrsort correctly maintains RRs test 3");


#
# Test with MX RRs.
#

my $mxrr1=Net::DNS::RR->new("example.com.  600     IN      MX 10 mx1.example.com");
my $mxrr2=Net::DNS::RR->new("example.com.  600     IN      MX 6 mx2.example.com");

my $mxrr3=Net::DNS::RR->new("example.com.  600     IN      MX 66 mx3.example.com");
my $mxrr4=Net::DNS::RR->new("example.com.  600     IN      RT 6 rt1.example.com");


my @mxrrarray=($mxrr1, $mxrr2, $mxrr3, $mxrr4);
my @expectedmxarray=($mxrr2,$mxrr1,$mxrr3);
my @sortedmxarray=rrsort("MX",@mxrrarray);

ok (eq_array(\@expectedmxarray,\@sortedmxarray),"MX sorting");




my $nsrr1=Net::DNS::RR->new("example.com.  600     IN      NS ns2.example.com");
my $nsrr2=Net::DNS::RR->new("example.com.  600     IN      NS ns4.example.com");
my $nsrr3=Net::DNS::RR->new("example.com.  600     IN      NS ns1.example.com");
my $nsrr4=Net::DNS::RR->new("example.com.  600     IN      RT 6 rt1.example.com");

my @nsrrarray=($nsrr1, $nsrr2, $nsrr3, $nsrr4);
my @expectednsarray=($nsrr3,$nsrr1,$nsrr2);
my @sortednsarray=rrsort("NS",@nsrrarray);




ok (eq_array(\@expectednsarray,\@sortednsarray),"NS sorting");
