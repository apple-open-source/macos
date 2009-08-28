#!/usr/local/bin/perl

use Convert::ASN1 qw(:io);
use IO::Socket;

print "1..11\n";

my  $result = pack("C*", 0x30, 0x3D, 0x04, 0x04, 0x46, 0x72, 0x65, 0x64,
			 0x30, 0x13, 0x04, 0x11, 0x41, 0x20, 0x73, 0x74,
			 0x72, 0x69, 0x6E, 0x67, 0x20, 0x66, 0x6F, 0x72,
			 0x20, 0x66, 0x72, 0x65, 0x64, 0x04, 0x03, 0x4A,
			 0x6F, 0x65, 0x30, 0x1B, 0x04, 0x03, 0x68, 0x61,
			 0x73, 0x04, 0x01, 0x61, 0x04, 0x04, 0x6C, 0x69,
			 0x73, 0x74, 0x04, 0x02, 0x6F, 0x66, 0x04, 0x07,
			 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67, 0x73);

($file = $0) =~ s/t$/dat/;
open(OUT,"> $file");
asn_write(*OUT, $result);
asn_write(*OUT, $result);
close(OUT);

open(IN,"< $file");
sysread(IN,$buffer,1024);
close(IN);

print "not " unless $buffer eq $result x 2;
print "ok 1\n";

open(IN,"< $file");
asn_read(*IN, $input);
close(IN);

print "not " unless $input eq $result;
print "ok 2\n";

open(IN,"< $file");

undef $input;
$input = asn_get(*IN);
print "not " unless $input eq $result;
print "ok 3\n";

print "not " unless asn_ready(*IN);
print "ok 4\n";

undef $input;
$input = asn_get(*IN);
print "not " unless $input eq $result;
print "ok 5\n";

print "not " if asn_ready(*IN);
print "ok 6\n";

close(IN);

unlink($file);

my $src = IO::Socket::INET->new(Proto => 'udp');
my $dst = IO::Socket::INET->new(Proto => 'udp');
bind($dst, pack_sockaddr_in(0, INADDR_ANY));
my $host = $dst->sockhost eq '0.0.0.0' ? '127.0.0.1' : $dst->sockhost;
my $addr = pack_sockaddr_in($dst->sockport, inet_aton($host));

asn_send($src,$result,0,$addr) or print "not ";
print "ok 7\n";

asn_recv($dst, $in2, 0) or print "not ";
print "ok 8\n";

print "not " unless $in2 && $in2 eq $result;
print "ok 9\n";
  
open(IN,"t/07input.dat") or die "Cannot open 07input.dat: $!\n";
undef $input;
my $n = asn_read(*IN,$input);
print "not " unless $n and $n == length($input);
print "ok 10\n";
print "not " unless $n == 1283;
print "ok 11\n";

