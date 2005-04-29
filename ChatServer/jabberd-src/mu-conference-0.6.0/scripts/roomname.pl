#!/usr/bin/perl
#
# roomname.pl : Perl Utility to create sha1 hash for a jid
#
# Requires: Digest::SHA1
#
use Digest::SHA1  qw(sha1_hex);

my $data;
my $digest;

while(@ARGV)
{
    $data = shift @ARGV;
    $digest = sha1_hex($data);
    print "$digest <= $data\n";
}
