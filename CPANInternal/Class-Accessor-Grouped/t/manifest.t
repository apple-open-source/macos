#!perl -wT
# $Id: /local/CPAN/Class-Accessor-Grouped/t/manifest.t 1064 2007-12-28T23:18:25.520728Z claco  $
use strict;
use warnings;

BEGIN {
    use lib 't/lib';
    use Test::More;

    plan skip_all => 'set TEST_AUTHOR to enable this test' unless $ENV{TEST_AUTHOR};

    eval 'use Test::CheckManifest 0.09';
    if($@) {
        plan skip_all => 'Test::CheckManifest 0.09 not installed';
    };
};

ok_manifest({
    exclude => ['/t/var', '/cover_db'],
    filter  => [qr/\.svn/, qr/cover/, qr/Build(.(PL|bat))?/, qr/_build/, qr/\.DS_Store/],
    bool    => 'or'
});
