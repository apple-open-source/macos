use Class::C3;
use strict;
use Test::More;
use warnings;

BEGIN {
    eval "use DBD::SQLite";
    plan $@
        ? ( skip_all => 'needs DBD::SQLite for testing' )
        : ( tests => 3 );
}

use lib qw(t/lib);

use_ok( 'DBICTest' );

use_ok( 'DBICTest::Schema' );

{
       my $warnings;
       local $SIG{__WARN__} = sub { $warnings .= $_[0] };
       eval { DBICTest::CD->create({ title => 'vacation in antarctica' }) };
       ok( $warnings !~ /uninitialized value/, "No warning from Storage" );
}

