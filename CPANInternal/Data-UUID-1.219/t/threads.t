use strict;
use warnings;
use Test::More;
use Config;

BEGIN {
    plan skip_all => 'Perl not compiled with useithreads'
        if !$Config{useithreads};
    plan skip_all => 'This test does not cope well with this version of perl'
        if "$]" == 5.008_002
        or ("$]" < 5.013_004 and not $ENV{AUTHOR_TESTING});
    plan tests => 4;
}

use threads;
use Data::UUID;

my $ug = Data::UUID->new;

my @threads = map {
    threads->create(sub { ($ug->create_str, Data::UUID->new->create_str) });
} 1 .. 20;

my @ret = map {
    $_->join
} @threads;

pass 'we survived our threads';

is @ret, 40, 'got as all the uuids we expected';
ok !grep({ !defined } @ret), 'uuids look sane';

my %uuids = map { $_ => 1 } @ret;
is keys %uuids, @ret, "all UUIDs are unique";
