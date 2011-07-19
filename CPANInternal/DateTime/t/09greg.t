#!/usr/bin/perl -w

use strict;

use Test::More tests => 35;

use DateTime;

# test _ymd2rd and _rd2ymd for various dates
# 2 tests are performed for each date (on _ymd2rd and _rd2ymd)
# dates are specified as [rd,year,month,day]
for (# min and max supported days (for 32-bit system)
     [-2 ** 31,    -5879610, 6, 22],
     [ 2 ** 31 - 1, 5879611, 7, 11],

     # some miscellaneous dates (these are actually epoch dates for
     # various calendars from Calendrical Calculations (1st ed) Table
     # 1.1)
     [-1721425,-4713,11,24],
     [-1373427,-3760,9,7],
     [-1137142,-3113,8,11],
     [-1132959,-3101,1,23],
     [-963099,-2636,2,15],
     [-1,0,12,30],[1,1,1,1],
     [2796,8,8,27],
     [103605,284,8,29],
     [226896,622,3,22],
     [227015,622,7,19],
     [654415,1792,9,22],
     [673222,1844,3,21]
) {
    is( join('/',DateTime->_rd2ymd($_->[0])), join('/',@{$_}[1..3]),
        $_->[0] . "   \t=> " . join '/', @{$_}[1..3] );

    is( DateTime->_ymd2rd(@{$_}[1..3]), $_->[0],
        join('/',@{$_}[1..3]) . "   \t=> " . $_->[0]);
}

# normalization tests
for ( [-1753469,-4797,-33,1],
      [-1753469,-4803,39,1],
      [-1753105,-4796,-34,28],
      [-1753105,-4802,38,28]
    )
{
    is(DateTime->_ymd2rd(@{$_}[1..3]), $_->[0],
       join('/',@{$_}[1..3])." \t=> ".$_->[0]." (normalization)");
}

# test first and last day of each month from Jan -4800..Dec 4800
# this test bails after the first failure with a not ok.
# if it comlpetes successfully, only one ok is issued.

my @mlen=(0,31,0,31,30,31,30,31,31,30,31,30,31);
my ($dno,$y,$m,$dno2,$y2,$m2,$d2,$mlen) = (-1753530,-4800,1);

while ( $y <= 4800 ) {

    # test $y,$m,1
    ++$dno;
    $dno2 = DateTime->_ymd2rd( $y, $m, 1 );
    if ( $dno != $dno2 ) {
        is( $dno2, $dno, "greg torture test: _ymd2rd($y,$m,1) should be $dno" );
        last;
    }
    ( $y2, $m2, $d2 ) = DateTime->_rd2ymd($dno);

    if ( $y2 != $y || $m2 != $m || $d2 != 1 ) {
        is( "$y2/$m2/$d2", "$y/$m/1",
          "greg torture test: _rd2ymd($dno) should be $y/$m/1" );
        last;
    }

    # test $y,$m,$mlen
    $mlen = $mlen[$m] || ( $y % 4 ? 28 : $y % 100 ? 29 : $y % 400 ? 28 : 29 );
    $dno += $mlen - 1;
    $dno2 = DateTime->_ymd2rd( $y, $m, $mlen );
    if ( $dno != $dno2 ) {
        is( $dno2, $dno,
          "greg torture test: _ymd2rd($y,$m,$mlen) should be $dno" );
        last;
    }
    ( $y2, $m2, $d2 ) = DateTime->_rd2ymd($dno);

    if ( $y2 != $y || $m2 != $m || $d2 != $mlen ) {
        is( "$y2/$m2/$d2", "$y/$m/$mlen",
          "greg torture test: _rd2ymd($dno) should be $y/$m/$mlen" );
        last;
    }

    # and on to the next month...
    if ( ++$m > 12 ) {
        $m = 1;
        ++$y;
    }
}

pass("greg torture test") if $y == 4801;

