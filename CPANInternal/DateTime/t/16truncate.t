#!/usr/bin/perl -w

use strict;

use Test::More tests => 52;

use DateTime;

my %vals =
    ( year       => 50,
      month      => 3,
      day        => 15,
      hour       => 10,
      minute     => 55,
      second     => 17,
      nanosecond => 1234,
    );

{
    my $dt = DateTime->new(%vals);
    $dt->truncate( to => 'second' );
    foreach my $f ( qw( year month day hour minute second ) )
    {
        is( $dt->$f(), $vals{$f}, "$f should be $vals{$f}" );
    }

    foreach my $f ( qw( nanosecond ) )
    {
        is( $dt->$f(), 0, "$f should be 0" );
    }
}

{
    my $dt = DateTime->new(%vals);
    $dt->truncate( to => 'minute' );
    foreach my $f ( qw( year month day hour minute ) )
    {
        is( $dt->$f(), $vals{$f}, "$f should be $vals{$f}" );
    }

    foreach my $f ( qw( second nanosecond ) )
    {
        is( $dt->$f(), 0, "$f should be 0" );
    }
}

{
    my $dt = DateTime->new(%vals);
    $dt->truncate( to => 'hour' );
    foreach my $f ( qw( year month day hour ) )
    {
        is( $dt->$f(), $vals{$f}, "$f should be $vals{$f}" );
    }

    foreach my $f ( qw( minute second nanosecond ) )
    {
        is( $dt->$f(), 0, "$f should be 0" );
    }
}

{
    my $dt = DateTime->new(%vals);
    $dt->truncate( to => 'day' );
    foreach my $f ( qw( year month day ) )
    {
        is( $dt->$f(), $vals{$f}, "$f should be $vals{$f}" );
    }

    foreach my $f ( qw( hour minute second nanosecond ) )
    {
        is( $dt->$f(), 0, "$f should be 0" );
    }
}

{
    my $dt = DateTime->new(%vals);
    $dt->truncate( to => 'month' );
    foreach my $f ( qw( year month ) )
    {
        is( $dt->$f(), $vals{$f}, "$f should be $vals{$f}" );
    }

    foreach my $f ( qw( day ) )
    {
        is( $dt->$f(), 1, "$f should be 1" );
    }

    foreach my $f ( qw( hour minute second nanosecond ) )
    {
        is( $dt->$f(), 0, "$f should be 0" );
    }
}

{
    my $dt = DateTime->new(%vals);
    $dt->truncate( to => 'year' );
    foreach my $f ( qw( year ) )
    {
        is( $dt->$f(), $vals{$f}, "$f should be $vals{$f}" );
    }

    foreach my $f ( qw( month day ) )
    {
        is( $dt->$f(), 1, "$f should be 1" );
    }

    foreach my $f ( qw( hour minute second nanosecond ) )
    {
        is( $dt->$f(), 0, "$f should be 0" );
    }
}

{
    my $dt = DateTime->new( year => 2003, month => 11, day => 17 );

    for (1..6)
    {
	my $trunc = $dt->clone->add( days => $_ )->truncate( to => 'week' );

	is( $trunc->day, 17, 'truncate to week should always truncate to monday of week' );
    }

    {
	my $trunc = $dt->clone->add( days => 7 )->truncate( to => 'week' );

	is( $trunc->day, 24, 'truncate to week should always truncate to monday of week' );
    }

    {
        my $dt =
            DateTime->new( year => 2003, month => 10, day => 2 )->truncate( to => 'week' );

        is( $dt->year, 2003, 'truncation to week across month boundary' );
        is( $dt->month, 9, 'truncation to week across month boundary' );
        is( $dt->day, 29, 'truncation to week across month boundary' );
    }
}
