#!/usr/bin/perl -wT

use strict;

use Test::More tests => 4;

use DateTime::Format::Builder;


# Does new() work properly?
{
    eval { DateTime::Format::Builder->new('fnar') };
    ok(( $@ and $@ =~ /takes no param/), "Too many parameters exception" );

    my $obj = eval { DateTime::Format::Builder->new() };
    ok( !$@, "Created object" );
    isa_ok( $obj, 'DateTime::Format::Builder' );

    eval { $obj->parse_datetime( "whenever" ) };
    ok(( $@ and $@ =~ /No parser/), "No parser exception" );

}
