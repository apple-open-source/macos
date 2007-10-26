# $Id: on_fail_sub.t,v 1.2 2003/08/10 13:38:29 koschei Exp $
use strict;
use lib 'inc';
use blib;
use Test::More tests => 9;
use vars qw( $class );

BEGIN {
    $class = 'DateTime::Format::Builder';
    use_ok $class;
}

# ------------------------------------------------------------------------

{
    eval q|
        package DTFB::Sub;
        use base qw( DateTime::Format::Builder );
        
        sub on_fail {
            return undef;
        }

        1;
        
        package DTFB::OnFailSubTest;
        
        BEGIN {
            DTFB::Sub->import(
                parsers => {
                    parse_datetime => [
                    {strptime=> '%m/%d/%Y'},
                    {strptime=> '%Y/%m/%d'},
                    ]
                }
            );
        }
        
        1;
    |;
    ok( !$@, "Made class" );
    diag $@ if $@;

    my $o = DTFB::OnFailSubTest->new;
    my $good_parse = $o->parse_datetime( "2003/08/09" );
    isa_ok( $good_parse, 'DateTime' );
    is( $good_parse->year => 2003, "Year good" );
    is( $good_parse->month => 8, "Month good" );
    is( $good_parse->day => 9, "Day good" );

    my $bad_parse = eval { $o->parse_datetime( "Fnerk" ) };
    ok( !$@, "Bad parse gives no error" );
    diag $@ if $@;
    ok( (!defined($bad_parse)), "Bad parse correctly gives undef" );
}

pass 'All done';
