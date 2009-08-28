#!/usr/local/bin/perl

#
# Test that the primitive operators are working
#

# Wolfgang Rosner

use Convert::ASN1 qw(:all);

print "1..24\n";

BEGIN { require 't/funcs.pl' }

my $t = 1;

btest $t++, $asn = Convert::ASN1->new or warn $asn->error;
btest $t++, $asn->prepare('date UTCTime') or warn $asn->error;

my $time = 987718268; # 2001-04-19 22:11:08 GMT
my $result;
my $ret;

# One hour ahead

$result = pack("C*",
  0x17, 0x11, 0x30, 0x31, 0x30, 0x34, 0x31, 0x39,
  0x32, 0x33, 0x31, 0x31, 0x30, 0x38, 0x2B, 0x30,
  0x31, 0x30, 0x30
);

$asn->configure( encode => { timezone => +3600 } );
stest $t++, $result, $asn->encode(date => $time) or warn $asn->error;
btest $t++, $ret = $asn->decode($result) or warn $asn->error;
ntest $t++, $time, $ret->{date};

# 2 hours ahead

$result = pack("C*",
  0x17, 0x11, 0x30, 0x31, 0x30, 0x34, 0x32, 0x30,
  0x30, 0x30, 0x31, 0x31, 0x30, 0x38, 0x2b, 0x30,
  0x32, 0x30, 0x30
);

$asn->configure( encode => { timezone => +7200 } );
stest $t++, $result, $asn->encode(date => $time) or warn $asn->error;
btest $t++, $ret = $asn->decode($result) or warn $asn->error;
ntest $t++, $time, $ret->{date};

# zulu

$result = pack("C*",
  0x17, 0x0D, 0x30, 0x31, 0x30, 0x34, 0x31, 0x39,
  0x32, 0x32, 0x31, 0x31, 0x30, 0x38, 0x5A
);

$asn->configure( encode => { 'time' => 'utctime' } );
stest $t++, $result, $asn->encode(date => $time) or warn $asn->error;
btest $t++, $ret = $asn->decode($result) or warn $asn->error;
ntest $t++, $time, $ret->{date};

# 1 hour ahead

btest $t++, $asn = Convert::ASN1->new or warn $asn->error;
btest $t++, $asn->prepare('date GeneralizedTime') or warn $asn->error;
$result = pack("C*",
  0x18, 0x13, 0x32, 0x30, 0x30, 0x31, 0x30, 0x34, 0x31, 0x39,
  0x32, 0x33, 0x31, 0x31, 0x30, 0x38, 0x2B, 0x30,
  0x31, 0x30, 0x30
);

$asn->configure( encode => { timezone => +3600 } );
stest $t++, $result, $asn->encode(date => $time) or warn $asn->error;
btest $t++, $ret = $asn->decode($result) or warn $asn->error;
ntest $t++, $time, $ret->{date};

# 4 hours behind

btest $t++, $asn = Convert::ASN1->new or warn $asn->error;
btest $t++, $asn->prepare('date GeneralizedTime') or warn $asn->error;
$result = pack("C*",
  0x18, 0x13, 0x32, 0x30, 0x30, 0x31, 0x30, 0x34, 0x31, 0x39,
  0x31, 0x38, 0x31, 0x31, 0x30, 0x38, 0x2D, 0x30,
  0x34, 0x30, 0x30
);

$asn->configure( encode => { timezone => -14400 } );
stest $t++, $result, $asn->encode(date => $time) or warn $asn->error;
btest $t++, $ret = $asn->decode($result) or warn $asn->error;
ntest $t++, $time, $ret->{date};

# fractional second

$time += 0.5;
$result = pack("C*",
  0x18, 0x17, 0x32, 0x30, 0x30, 0x31, 0x30, 0x34, 0x31,
  0x39, 0x32, 0x33, 0x31, 0x31, 0x30, 0x38,
  0x2E, 0x35, 0x30, 0x30, 0x2B, 0x30, 0x31, 0x30, 0x30
);

$asn->configure( encode => { timezone => +3600 } );
stest $t++, $result, $asn->encode(date => $time) or warn $asn->error;
btest $t++, $ret = $asn->decode($result) or warn $asn->error;
ntest $t++, $time, $ret->{date};

