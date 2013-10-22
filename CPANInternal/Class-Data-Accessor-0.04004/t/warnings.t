#!perl -wT
# $Id$
use strict;
use warnings;

BEGIN {
    use lib 't/lib';
    use Test::More;
    use File::Find;
    use File::Basename;

    plan skip_all => 'set TEST_AUTHOR to enable this test' unless $ENV{TEST_AUTHOR};

    eval 'use Test::Strict 0.05';
    plan skip_all => 'Test::Strict 0.05 not installed' if $@;
    plan skip_all => 'Need untaint in newer File::Find' if $] <= 5.006;
};

## I hope this can go away if Test::Strict or File::Find::Rule
## finally run under -T. Until then, I'm on my own here. ;-)
my @files;
my %trusted = (

);

find({  wanted => \&wanted,
        untaint => 1,
        untaint_pattern => qr|^([-+@\w./]+)$|,
        untaint_skip => 1,
        no_chdir => 1
}, qw(lib t));

sub wanted {
    my $name = $File::Find::name;
    my $file = fileparse($name);

    return if $name =~ /TestApp/;

    if ($name =~ /\.(pm|pl|t)$/i && !exists($trusted{$file})) {
        push @files, $name;
    };
};

if (scalar @files) {
    plan tests => scalar @files;
} else {
    plan tests => 1;
    fail 'No perl files found for Test::Strict checks!';
};

foreach (@files) {
   warnings_ok($_);
};
