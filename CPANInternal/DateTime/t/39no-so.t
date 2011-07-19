#!/usr/bin/perl -w

use strict;

use Test::More tests => 3;

no warnings 'once', 'redefine';


my $sub =
    sub { die q{Can't locate loadable object for module DateTime in @INC} };
if ( $] >= 5.006 )
{
    require XSLoader;
    *XSLoader::load = $sub;
}
else
{
    require DynaLoader;
    *DynaLoader::bootstrap = $sub;
}

eval { require DateTime };
is( $@, '', 'No error loading DateTime without DateTime.so file' );
ok( $DateTime::IsPurePerl, '$DateTime::IsPurePerl is true' );

ok( DateTime->new( year => 2005 ),
    'can make DateTime object without DateTime.so file' );
