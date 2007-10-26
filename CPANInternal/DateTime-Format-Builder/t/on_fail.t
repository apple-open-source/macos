# $Id: on_fail.t,v 1.1 2003/08/10 13:38:29 koschei Exp $
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
        package DTFB::OnFailTest;

        use DateTime::Format::Builder(
            parsers => {
                parse_datetime => [
                    [ on_fail => \&on_fail ],
                    { strptime => '%m/%d/%Y%n%I:%M%p'},
                    { strptime => '%Y/%m/%d%n%I:%M%p'},
                    { strptime => '%m-%d-%Y%n%I:%M%p'},
                    { strptime => '%F%n%I:%M%p'},
                    { strptime => '%b%n%d,%n%Y%n%I:%M%p'},
                    { strptime => '%m/%d/%Y%n%H:%M'},
                    { strptime => '%Y/%m/%d%n%H:%M'},
                    { strptime => '%m-%d-%Y%n%H:%M'},
                    { strptime => '%F%n%H:%M'},
                    { strptime => '%b%n%d,%n%Y%n%H:%M'},
                    { strptime => '%m/%d/%Y'},
                    { strptime => '%Y/%m/%d'},
                    { strptime => '%m-%d-%Y'},
                    { strptime => '%F'},
                    { strptime => '%b%n%d,%n%Y'}
                ]
            }
        );

        sub on_fail {
            return undef;
        }

        1;
    |;
    ok( !$@, "Made class" );
    diag $@ if $@;

    my $o = DTFB::OnFailTest->new;
    my $good_parse = $o->parse_datetime( "2003/08/09" );
    isa_ok( $good_parse, 'DateTime' );
    is( $good_parse->year => 2003, "Year good" );
    is( $good_parse->month => 8, "Month good" );
    is( $good_parse->day => 9, "Day good" );

    my $bad_parse = eval { $o->parse_datetime( "Fnerk" ) };
    ok( !$@, "Bad parse gives no error" );
    ok( (!defined($bad_parse)), "Bad parse correctly gives undef" );
}

pass 'All done';
