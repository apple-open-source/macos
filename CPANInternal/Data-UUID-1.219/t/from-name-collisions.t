use strict;
use warnings;
use Test::More tests => 1;
use Data::UUID qw(NameSpace_DNS);

my $generator = new Data::UUID;

my %res;
for my $id ( 1 .. 1000 ) {
    $res{ $generator->create_from_name_str( NameSpace_DNS, $id ) }++;
}

my $collisions = 0;
while ( my ($k, $v) = each %res ) {
    next if $v == 1;
    $collisions += $v;
}

is($collisions, 0, "no collisions");
