use strict;
use warnings;
use utf8;

use Test::More tests => 121;

use DateTime;


if ( $] >= 5.008 )
{
    for my $fh ( Test::Builder->new()->output(),
                 Test::Builder->new()->failure_output(),
                 Test::Builder->new()->todo_output(),
               )
    {
        binmode $fh, ':utf8';
    }
}

{
    my $dt = DateTime->new( year       => 1976,
                            month      => 10,
                            day        => 20,
                            hour       => 18,
                            minute     => 34,
                            second     => 55,
                            nanosecond => 1_000_000,
                            locale     => 'en',
                            time_zone  => 'America/Chicago',
                          );

    my %tests = ( 'GGGGG'  => 'A',
                  'GGGG'   => 'Anno Domini',
                  'GGG'    => 'AD',
                  'GG'     => 'AD',
                  'G'      => 'AD',

                  'yyyyy'  => '01976',
                  'yyyy'   => '1976',
                  'yyy'    => '1976',
                  'yy'     => '76',
                  'y'      => '1976',

                  'uuuuuu' => '001976',
                  'uuuuu'  => '01976',
                  'uuuu'   => '1976',
                  'uuu'    => '1976',
                  'uu'     => '1976',
                  'u'      => '1976',

                  'YYYYY'  => '01976',
                  'YYYY'   => '1976',
                  'YYY'    => '1976',
                  'YY'     => '1976',
                  'Y'      => '1976',

                  'QQQQ'   => '4th quarter',
                  'QQQ'    => 'Q4',
                  'QQ'     => '04',
                  'Q'      => '4',

                  'qqqq'   => '4th quarter',
                  'qqq'    => 'Q4',
                  'qq'     => '04',
                  'q'      => '4',

                  'MMMMM'  => 'O',
                  'MMMM'   => 'October',
                  'MMM'    => 'Oct',
                  'MM'     => '10',
                  'M'      => '10',

                  'LLLLL'  => 'O',
                  'LLLL'   => 'October',
                  'LLL'    => 'Oct',
                  'LL'     => '10',
                  'L'      => '10',

                  'ww'     => '43',
                  'w'      => '43',
                  'W'      => '3',

                  'dd'     => '20',
                  'd'      => '20',

                  'DDD'    => '294',
                  'DD'     => '294',
                  'D'      => '294',

                  'F'      => '3',
                  'gggggg' => '043071',
                  'g'      => '43071',

                  'EEEEE'  => 'W',
                  'EEEE'   => 'Wednesday',
                  'EEE'    => 'Wed',
                  'EE'     => 'Wed',
                  'E'      => 'Wed',

                  'eeeee'  => 'W',
                  'eeee'   => 'Wednesday',
                  'eee'    => 'Wed',
                  'ee'     => '03',
                  'e'      => '3',

                  'ccccc'  => 'W',
                  'cccc'   => 'Wednesday',
                  'ccc'    => 'Wed',
                  'cc'     => '03',
                  'c'      => '3',

                  'a'      => 'PM',

                  'hh'     => '06',
                  'h'      => '6',
                  'HH'     => '18',
                  'H'      => '18',
                  'KK'     => '06',
                  'K'      => '6',
                  'kk'     => '18',
                  'kk'     => '18',
                  'j'      => '6',
                  'jj'     => '06',

                  'mm'     => '34',
                  'm'      => '34',

                  'ss'     => '55',
                  's'      => '55',
                  'SS'     => '00',
                  'SSSSSS' => '001000',
                  'A'      => '66895001',

                  'zzzz'   => 'America/Chicago',
                  'zzz'    => 'CDT',
                  'ZZZZ'   => 'CDT-0500',
                  'ZZZ'    => '-0500',
                  'vvvv'   => 'America/Chicago',
                  'vvv'    => 'CDT',
                  'VVVV'   => 'America/Chicago',
                  'VVV'    => 'CDT',

                  q{'one fine day'} => 'one fine day',
                  q{'yy''yy' yyyy}  => q{yy'yy 1976},

                  q{'yy''yy' 'hello' yyyy}  => q{yy'yy hello 1976},

                  # Non-pattern text should pass through unchanged
                  'd日' => '20日',
                );

    for my $k ( sort keys %tests )
    {
        is( $dt->format_cldr($k), $tests{$k},
            "format_cldr for $k" );
    }
}

{
    my $dt = DateTime->new( year       => 2008,
                            month      => 10,
                            day        => 20,
                            hour       => 18,
                            minute     => 34,
                            second     => 55,
                            nanosecond => 1_000_000,
                            locale     => 'en',
                            time_zone  => 'America/Chicago',
                          );

    is( $dt->format_cldr('yy'), '08',
        'format_cldr for yy in 2008 should be 08' );
}

{
    my $dt = DateTime->new( year       => 2008,
                            month      => 10,
                            day        => 20,
                            hour       => 18,
                            minute     => 34,
                            second     => 55,
                            nanosecond => 1_000_000,
                            locale     => 'en_US',
                            time_zone  => 'America/Chicago',
                          );

    is( $dt->format_cldr('j'), '6',
        'format_cldr for j in en_US should be 6 (at 18:34)' );
}

{
    my $dt = DateTime->new( year       => 2008,
                            month      => 10,
                            day        => 20,
                            hour       => 18,
                            minute     => 34,
                            second     => 55,
                            nanosecond => 1_000_000,
                            locale     => 'fr',
                            time_zone  => 'America/Chicago',
                          );

    is( $dt->format_cldr('j'), '18',
        'format_cldr for j in fr should be 18 (at 18:34)' );
}

{
    my $dt = DateTime->new( year       => 2009,
                            month      => 4,
                            day        => 13,
                            locale     => 'en_US',
                          );

    is( $dt->format_cldr('e'), '2',
        'format_cldr for e in en_US should be 2 (for Monday, 2009-04-13)' );

    is( $dt->format_cldr('c'), '1',
        'format_cldr for c in en_US should be 1 (for Monday, 2009-04-13)' );
}


{
    my $dt = DateTime->new( year       => 2009,
                            month      => 4,
                            day        => 13,
                            locale     => 'fr_FR',
                          );

    is( $dt->format_cldr('e'), '1',
        'format_cldr for e in fr_FR should be 1 (for Monday, 2009-04-13)' );

    is( $dt->format_cldr('c'), '1',
        'format_cldr for c in fr_FR should be 1 (for Monday, 2009-04-13)' );
}

{
    my $dt = DateTime->new( year => -10 );

    my %tests = ( 'y'     => '-10',
                  'yy'    => '-10',
                  'yyy'   => '-10',
                  'yyyy'  => '-010',
                  'yyyyy' => '-0010',

                  'u'     => '-10',
                  'uu'    => '-10',
                  'uuu'   => '-10',
                  'uuuu'  => '-010',
                  'uuuuu' => '-0010',
                );

    for my $k ( sort keys %tests )
    {
        is( $dt->format_cldr($k), $tests{$k},
            "format_cldr for $k" );
    }
}

{
    my $dt = DateTime->new( year => -1976 );

    my %tests = ( 'y'     => '-1976',
                  'yy'    => '-76',
                  'yyy'   => '-1976',
                  'yyyy'  => '-1976',
                  'yyyyy' => '-1976',

                  'u'     => '-1976',
                  'uu'    => '-1976',
                  'uuu'   => '-1976',
                  'uuuu'  => '-1976',
                  'uuuuu' => '-1976',
                );

    for my $k ( sort keys %tests )
    {
        is( $dt->format_cldr($k), $tests{$k},
            "format_cldr for $k" );
    }
}
