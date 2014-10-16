#!/usr/bin/perl -w
use HTTP::Proxy qw( :log );
use HTTP::Proxy::BodyFilter::tags;
use HTTP::Proxy::BodyFilter::simple;
use HTTP::Proxy::BodyFilter::htmltext;
use strict;

my $proxy = HTTP::Proxy->new(@ARGV);

$proxy->push_filter(
    mime     => 'text/html',
    response => HTTP::Proxy::BodyFilter::tags->new,      # protect tags
    response => HTTP::Proxy::BodyFilter::simple->new(    # rainbow entities
        sub { ${ $_[1] } =~ s/(&[#\w]+;)/rainbow($1)/eg; }
    ),
    response => HTTP::Proxy::BodyFilter::htmltext->new(    # rainbow text
        sub { s/(\S)/rainbow($1)/eg; }
    )
);

sub rainbow {
    return sprintf qq{<font color="%s">%s</font>}, next_color(), shift;
}

# the following code courtesy David 'grinder' Landgren
# but adapted for our needs
use constant PI_2 => 3.14159265359 * 2;
my @PRIMES = qw/11 13 17 19 23 29 31 37 41 43 47 53 59/;
my $red    = rand() * PI_2;
my $green  = rand() * PI_2;
my $blue   = rand() * PI_2;
my $rdelta = PI_2 / $PRIMES[ rand scalar @PRIMES ];
my $gdelta = PI_2 / $PRIMES[ rand scalar @PRIMES ];
my $bdelta = PI_2 / $PRIMES[ rand scalar @PRIMES ];
my ( $rp, $gp, $bp ) = ( sin $red, sin $green, sin $blue );
my ( $rq, $gq, $bq ) = qw/ 0 0 0/;
my ( $rr, $gr, $br ) = qw/ 0 0 0/;

$proxy->start;

sub next_color {
    my $rs = sin( $red += $rdelta );
    my $rc = $rs * 120 + 120;
    my $gs = sin( $green += $gdelta );
    my $gc = $gs * 120 + 120;
    my $bs = sin( $blue += $bdelta );
    my $bc = $bs * 120 + 120;

    $rq = $rp <=> $rs;
    $gq = $gp <=> $gs;
    $bq = $bp <=> $bs;

    $rp = $rs;
    $gp = $gs;
    $bp = $bs;

    $rdelta = PI_2 / $PRIMES[ rand scalar @PRIMES ]
      if ( $rr == 1 and $rq < 1 and $rs < 1 );
    $gdelta = PI_2 / $PRIMES[ rand scalar @PRIMES ]
      if ( $gr == 1 and $gq < 1 and $gs < 1 );
    $bdelta = PI_2 / $PRIMES[ rand scalar @PRIMES ]
      if ( $br == 1 and $bq < 1 and $bs < 1 );

    $rr = $rq;
    $gr = $gq;
    $br = $bq;

    $rc = ( $rc < 0 ) ? 0 : ( $rc > 255 ) ? 255 : $rc;
    $gc = ( $gc < 0 ) ? 0 : ( $gc > 255 ) ? 255 : $gc;
    $bc = ( $bc < 0 ) ? 0 : ( $bc > 255 ) ? 255 : $bc;

    return sprintf( "#%02x%02x%02x", $rc, $gc, $bc );
}

