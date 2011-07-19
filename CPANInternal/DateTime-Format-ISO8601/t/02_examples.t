#!/usr/bin/perl

# Copyright (C) 2003-2005  Joshua Hoblitt
#
# $Id: 02_examples.t,v 1.11 2010/01/18 06:36:23 jhoblitt Exp $

use strict;
use warnings;

use lib qw( ./lib );

use Test::More tests => 174;

use DateTime::Format::ISO8601;

# parse_datetime
my $base_year = 2000;
my $base_month = "01";
my $iso8601 = DateTime::Format::ISO8601->new(
    base_datetime => DateTime->new( year => $base_year, month => $base_month ),
);

{
    #YYYYMMDD 19850412
    my $dt = $iso8601->parse_datetime( '19850412' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #YYYY-MM-DD 1985-04-12
    my $dt = $iso8601->parse_datetime( '1985-04-12' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #YYYY-MM 1985-04
    my $dt = $iso8601->parse_datetime( '1985-04' );
    is( $dt->ymd, '1985-04-01' );
}

{
    #YYYY 1985
    my $dt = $iso8601->parse_datetime( '1985' );
    is( $dt->ymd, '1985-01-01' );
}

{
    #YY 19 (century)
    my $dt = $iso8601->parse_datetime( '19' );
    is( $dt->ymd, '1901-01-01' );
}

{
    #YYMMDD 850412
    my $dt = $iso8601->parse_datetime( '850412' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #YY-MM-DD 85-04-12
    my $dt = $iso8601->parse_datetime( '85-04-12' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #-YYMM -8504
    my $dt = $iso8601->parse_datetime( '-8504' );
    is( $dt->ymd, '1985-04-01' );
}

{
    #-YY-MM -85-04
    my $dt = $iso8601->parse_datetime( '-85-04' );
    is( $dt->ymd, '1985-04-01' );
}

{
    #-YY -85
    my $dt = $iso8601->parse_datetime( '-85' );
    is( $dt->year, '1985' );
}

{
    #--MMDD --0412
    my $dt = $iso8601->parse_datetime( '--0412' );
    is( $dt->ymd, "${base_year}-04-12" );
}

{
    #--MM-DD --04-12
    my $dt = $iso8601->parse_datetime( '--04-12' );
    is( $dt->ymd, "${base_year}-04-12" );
}

{
    #--MM --04
    my $dt = $iso8601->parse_datetime( '--04' );
    is( $dt->ymd, "${base_year}-04-01" );
}

{
    #---DD ---12
    my $dt = $iso8601->parse_datetime( '---12' );
    is( $dt->ymd, "${base_year}-${base_month}-12" );
}

{
    #+[YY]YYYYMMDD +0019850412
    my $dt = $iso8601->parse_datetime( '+0019850412' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #+[YY]YYYY-MM-DD +001985-04-12
    my $dt = $iso8601->parse_datetime( '+001985-04-12' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #+[YY]YYYY-MM +001985-04
    my $dt = $iso8601->parse_datetime( '+001985-04' );
    is( $dt->ymd, '1985-04-01' );
}

{
    #+[YY]YYYY +001985
    my $dt = $iso8601->parse_datetime( '+001985' );
    is( $dt->ymd, '1985-01-01' );
}

{
    #+[YY]YY +0019 (century)
    my $dt = $iso8601->parse_datetime( '+0019' );
    is( $dt->ymd, '1901-01-01' );
}

{
    #YYYYDDD 1985102
    my $dt = $iso8601->parse_datetime( '1985102' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #YYYY-DDD 1985-102
    my $dt = $iso8601->parse_datetime( '1985-102' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #YYDDD 85102
    my $dt = $iso8601->parse_datetime( '85102' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #YY-DDD 85-102
    my $dt = $iso8601->parse_datetime( '85-102' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #-DDD -102
    my $dt = $iso8601->parse_datetime( '-102' );
    my $year = sprintf( "%04i", $base_year );
    is( $dt->strftime( "%j" ), 102 );
}

{
    #+[YY]YYYYDDD +001985102
    my $dt = $iso8601->parse_datetime( '+001985102' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #+[YY]YYYY-DDD +001985-102
    my $dt = $iso8601->parse_datetime( '+001985-102' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #YYYYWwwD 1985W155
    my $dt = $iso8601->parse_datetime( '1985W155' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #YYYY-Www-D 1985-W15-5
    my $dt = $iso8601->parse_datetime( '1985-W15-5' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #YYYYWww 1985W15
    my $dt = $iso8601->parse_datetime( '1985W15' );
    is( $dt->ymd, '1985-04-08' );
}

{
    #YYYY-Www 1985-W15
    my $dt = $iso8601->parse_datetime( '1985-W15' );
    is( $dt->ymd, '1985-04-08' );
}

{
    #YYWwwD 85W155
    my $dt = $iso8601->parse_datetime( '85W155' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #YY-Www-D 85-W15-5
    my $dt = $iso8601->parse_datetime( '85-W15-5' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #YYWww 85W15
    my $dt = $iso8601->parse_datetime( '85W15' );
    is( $dt->ymd, '1985-04-08' );
}

{
    #YY-Www 85-W15
    my $dt = $iso8601->parse_datetime( '85-W15' );
    is( $dt->ymd, '1985-04-08' );
}

{
    #-YWwwD -5W155
    my $dt = $iso8601->parse_datetime( '-5W155' );
    is( $dt->year, '2005' );
    is( $dt->week_number, '15' );
    is( $dt->day_of_week, '5' );
}

{
    #-Y-Www-D -5-W15-5
    my $dt = $iso8601->parse_datetime( '-5-W15-5' );
    is( $dt->year, '2005' );
    is( $dt->week_number, '15' );
    is( $dt->day_of_week, '5' );
}

{
    #-YWww -5W15
    my $dt = $iso8601->parse_datetime( '-5W15' );
    is( $dt->year, '2005' );
    is( $dt->week_number, '15' );
}

{
    #-Y-Www -5-W15
    my $dt = $iso8601->parse_datetime( '-5-W15' );
    is( $dt->year, '2005' );
    is( $dt->week_number, '15' );
}

{
    #-WwwD -W155
    my $dt = $iso8601->parse_datetime( '-W155' );
    is( $dt->week_number, '15' );
    is( $dt->day_of_week, '5' );
}

{
    #-Www-D -W15-5
    my $dt = $iso8601->parse_datetime( '-W15-5' );
    is( $dt->week_number, '15' );
    is( $dt->day_of_week, '5' );
}

# {
#     #-Www -W15
#     my $dt = $iso8601->parse_datetime( '-W15' );
#     is( $dt->week_number, '15' );
# }

{
    #-W-D -W-5
    my $dt = $iso8601->parse_datetime( '-W-5' );
    is( $dt->day_of_week, '5' );
}

{
    #+[YY]YYYYWwwD +001985W155
    my $dt = $iso8601->parse_datetime( '+001985W155' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #+[YY]YYYY-Www-D +001985-W15-5
    my $dt = $iso8601->parse_datetime( '+001985-W15-5' );
    is( $dt->ymd, '1985-04-12' );
}

{
    #+[YY]YYYYWww +001985W15
    my $dt = $iso8601->parse_datetime( '+001985W15' );
    is( $dt->ymd, '1985-04-08' );
}

{
    #+[YY]YYYY-Www +001985-W15
    my $dt = $iso8601->parse_datetime( '+001985-W15' );
    is( $dt->ymd, '1985-04-08' );
}

{
    #hh:mm:ss 23:20:50
    my $dt = $iso8601->parse_datetime( '23:20:50' );
    is( $dt->hms, '23:20:50' );
}

{
    #hh:mm 23:20
    my $dt = $iso8601->parse_datetime( '23:20' );
    is( $dt->hms, '23:20:00' );
}

{
    #hhmmss,ss 232050,5
    my $dt = $iso8601->parse_datetime( '232050,5' );
    is( $dt->hms, '23:20:50' );
    is( $dt->nanosecond, 500_000_000 );
}

{
    #hh:mm:ss,ss 23:20:50,5
    my $dt = $iso8601->parse_datetime( '23:20:50,5' );
    is( $dt->hms, '23:20:50' );
    is( $dt->nanosecond, 500_000_000 );
}

{
    #hhmm,mm 2320,8
    my $dt = $iso8601->parse_datetime( '2320,8' );
    is( $dt->hms, '23:20:48' );
}

{
    #hh:mm,mm 23:20,8
    my $dt = $iso8601->parse_datetime( '23:20,8' );
    is( $dt->hms, '23:20:48' );
}

{
    #hh,hh 23,3
    my $dt = $iso8601->parse_datetime( '23,3' );
    is( $dt->hms, '23:18:00' );
}

{
    #-mm:ss -20:50
    my $dt = $iso8601->parse_datetime( '-20:50' );
    is( $dt->minute, '20' );
    is( $dt->second, '50' );
}

{
    #-mmss,s -2050,5
    my $dt = $iso8601->parse_datetime( '-2050,5' );
    is( $dt->minute, '20' );
    is( $dt->second, '50' );
    is( $dt->nanosecond, 500_000_000 );
}

{
    #-mm:ss,s -20:50,5
    my $dt = $iso8601->parse_datetime( '-20:50,5' );
    is( $dt->minute, '20' );
    is( $dt->second, '50' );
    is( $dt->nanosecond, 500_000_000 );
}

{
    #-mm,m -20,8
    my $dt = $iso8601->parse_datetime( '-20,8' );
    is( $dt->minute, '20' );
    is( $dt->second, '48' );
}

{
    #--ss,s --50,5
    my $dt = $iso8601->parse_datetime( '--50,5' );
    is( $dt->second, '50' );
    is( $dt->nanosecond, 500_000_000 );
}

{
    #hhmmssZ 232030Z
    my $dt = $iso8601->parse_datetime( '232030Z' );
    is( $dt->hms, '23:20:30' );
    is( $dt->time_zone->name, 'UTC' );
}

{
    #hhmmss.ssZ 232030.5Z
    my $dt = $iso8601->parse_datetime( '232030.5Z' );
    is( $dt->hms, '23:20:30' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, 'UTC' );
}


{
    #hh:mm:ssZ 23:20:30Z
    my $dt = $iso8601->parse_datetime( '23:20:30Z' );
    is( $dt->hms, '23:20:30' );
    is( $dt->time_zone->name, 'UTC' );
}

{
    #hhmmssZ 23:20:30.5Z
    my $dt = $iso8601->parse_datetime( '23:20:30.5Z' );
    is( $dt->hms, '23:20:30' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, 'UTC' );
}


{
    #hhmmZ 2320Z
    my $dt = $iso8601->parse_datetime( '2320Z' );
    is( $dt->hms, '23:20:00' );
    is( $dt->time_zone->name, 'UTC' );
}

{
    #hh:mmZ 23:20Z
    my $dt = $iso8601->parse_datetime( '23:20Z' );
    is( $dt->hms, '23:20:00' );
    is( $dt->time_zone->name, 'UTC' );
}

{
    #hhZ 23Z
    my $dt = $iso8601->parse_datetime( '23Z' );
    is( $dt->hms, '23:00:00' );
    is( $dt->time_zone->name, 'UTC' );
}

{
    #hhmmss[+/-]hhmm 152746+0100 152746-0500
    my $dt = $iso8601->parse_datetime( '152746+0100' );
    is( $dt->hms, '15:27:46' );
    is( $dt->time_zone->name, '+0100' );
}

{
    #hhmmss[+/-]hhmm 152746+0100 152746-0500
    my $dt = $iso8601->parse_datetime( '152746-0500' );
    is( $dt->hms, '15:27:46' );
    is( $dt->time_zone->name, '-0500' );
}

{
    #hhmmss.ss[+/-]hhmm 152746.5+0100 152746.5-0500
    my $dt = $iso8601->parse_datetime( '152746.5+0100' );
    is( $dt->hms, '15:27:46' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, '+0100' );
}

{
    #hhmmss.ss[+/-]hh:mm 152746.5+01:00 152746.5-05:00
    my $dt = $iso8601->parse_datetime( '152746.5-05:00' );
    is( $dt->hms, '15:27:46' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, '-0500' );
}

{
    #hhmmss[+/-]hh:mm 152746.05+01:00 152746.05-05:00
    my $dt = $iso8601->parse_datetime( '152746.05-05:00' );
    is( $dt->hms, '15:27:46' );
    is( $dt->nanosecond, 50_000_000 );
    is( $dt->time_zone->name, '-0500' );
}


{
    #hh:mm:ss[+/-]hh:mm 15:27:46+01:00 15:27:46-05:00
    my $dt = $iso8601->parse_datetime( '15:27:46+01:00' );
    is( $dt->hms, '15:27:46' );
    is( $dt->time_zone->name, '+0100' );
}

{
    #hh:mm:ss[+/-]hh:mm 15:27:46+01:00 15:27:46-05:00
    my $dt = $iso8601->parse_datetime( '15:27:46-05:00' );
    is( $dt->hms, '15:27:46' );
    is( $dt->time_zone->name, '-0500' );
}

{
    #hhmmss.ss[+/-]hhmm 15:27:46.5+0100 15:27:46.5-0500
    my $dt = $iso8601->parse_datetime( '15:27:46.5+0100' );
    is( $dt->hms, '15:27:46' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, '+0100' );
}

{
    #hh:mm:ss.ss[+/-]hh:mm 15:27:46.5+01:00 15:27:46.5-05:00
    my $dt = $iso8601->parse_datetime( '15:27:46.5-05:00' );
    is( $dt->hms, '15:27:46' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, '-0500' );
}

{
    #hhmmss[+/-]hh 152746+01 152746-05
    my $dt = $iso8601->parse_datetime( '152746+01' );
    is( $dt->hms, '15:27:46' );
    is( $dt->time_zone->name, '+0100' );
}

{
    #hhmmss[+/-]hh 152746+01 152746-05
    my $dt = $iso8601->parse_datetime( '152746-05' );
    is( $dt->hms, '15:27:46' );
    is( $dt->time_zone->name, '-0500' );
}

{
    #hh:mm:ss[+/-]hh 15:27:46+01 15:27:46-05
    my $dt = $iso8601->parse_datetime( '15:27:46+01' );
    is( $dt->hms, '15:27:46' );
    is( $dt->time_zone->name, '+0100' );
}

{
    #hh:mm:ss[+/-]hh 15:27:46+01 15:27:46-05
    my $dt = $iso8601->parse_datetime( '15:27:46-05' );
    is( $dt->hms, '15:27:46' );
    is( $dt->time_zone->name, '-0500' );
}

{
    #YYYYMMDDThhmmss 19850412T101530
    my $dt = $iso8601->parse_datetime( '19850412T101530' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->time_zone->name, 'floating' );
}

{
    #YYYY-MM-DDThh:mm:ss 1985-04-12T10:15:30
    my $dt = $iso8601->parse_datetime( '1985-04-12T10:15:30' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->time_zone->name, 'floating' );
}

{
    #YYYYMMDDThhmmss.ss 19850412T101530.5
    my $dt = $iso8601->parse_datetime( '19850412T101530.5' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, 'floating' );
}

{
    #YYYY-MM-DDThh:mm:ss.ss 1985-04-12T10:15:30.5
    my $dt = $iso8601->parse_datetime( '1985-04-12T10:15:30.5' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, 'floating' );
}

{
    #YYYYMMDDThhmmssZ 19850412T101530Z
    my $dt = $iso8601->parse_datetime( '19850412T101530Z' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->time_zone->name, 'UTC' );
}

{
    #YYYY-MM-DDThh:mm:ssZ 1985-04-12T10:15:30Z
    my $dt = $iso8601->parse_datetime( '1985-04-12T10:15:30Z' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->time_zone->name, 'UTC' );
}

{
    #YYYYMMDDThhmmss.ssZ 19850412T101530.5Z
    my $dt = $iso8601->parse_datetime( '19850412T101530.5Z' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, 'UTC' );
}

{
    #YYYY-MM-DDThh:mm:ss.ssZ 1985-04-12T10:15:30.5Z
    my $dt = $iso8601->parse_datetime( '1985-04-12T10:15:30.5Z' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, 'UTC' );
}


{
    #YYYYMMDDThhmmss+hhmm 19850412T101530+0400
    my $dt = $iso8601->parse_datetime( '19850412T101530+0400' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->time_zone->name, '+0400' );
}

{
    #YYYY-MM-DDThh:mm:ss+hh:mm 1985-04-12T10:15:30+04:00
    my $dt = $iso8601->parse_datetime( '1985-04-12T10:15:30+04:00' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->time_zone->name, '+0400' );
}

{
    #YYYYMMDDThhmmss.ss+hhmm 19850412T101530.5+0400
    my $dt = $iso8601->parse_datetime( '19850412T101530.5+0400' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, '+0400' );
}

{
    #YYYY-MM-DDThh:mm:ss.ss+hh:mm 1985-04-12T10:15:30.5+04:00
    my $dt = $iso8601->parse_datetime( '1985-04-12T10:15:30.5+04:00' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->nanosecond, 500_000_000 );
    is( $dt->time_zone->name, '+0400' );
}


{
    #YYYYMMDDThhmmss+hh 19850412T101530+04
    my $dt = $iso8601->parse_datetime( '19850412T101530+04' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->time_zone->name, '+0400' );
}

{
    #YYYY-MM-DDThh:mm:ss+hh 1985-04-12T10:15:30+04
    my $dt = $iso8601->parse_datetime( '1985-04-12T10:15:30+04' );
    is( $dt->iso8601, '1985-04-12T10:15:30' );
    is( $dt->time_zone->name, '+0400' );
}

{
    #YYYYMMDDThhmm 19850412T1015
    my $dt = $iso8601->parse_datetime( '19850412T1015' );
    is( $dt->iso8601, '1985-04-12T10:15:00' );
    is( $dt->time_zone->name, 'floating' );
}

{
    #YYYY-MM-DDThh:mm 1985-04-12T10:15
    my $dt = $iso8601->parse_datetime( '1985-04-12T10:15' );
    is( $dt->iso8601, '1985-04-12T10:15:00' );
    is( $dt->time_zone->name, 'floating' );
}

{
    #YYYYDDDThhmmZ 1985102T1015Z
    my $dt = $iso8601->parse_datetime( '1985102T1015Z' );
    is( $dt->iso8601, '1985-04-12T10:15:00' );
    is( $dt->time_zone->name, 'UTC' );
}

{
    #YYYY-DDDThh:mmZ 1985-102T10:15Z
    my $dt = $iso8601->parse_datetime( '1985-102T10:15Z' );
    is( $dt->iso8601, '1985-04-12T10:15:00' );
    is( $dt->time_zone->name, 'UTC' );
}

{
    #YYYYWwwDThhmm+hhmm 1985W155T1015+0400
    my $dt = $iso8601->parse_datetime( '1985W155T1015+0400' );
    is( $dt->iso8601, '1985-04-12T10:15:00' );
    is( $dt->time_zone->name, '+0400' );
}

{
    #YYYY-Www-DThh:mm+hh 1985-W15-5T10:15+04
    my $dt = $iso8601->parse_datetime( '1985-W15-5T10:15+04' );
    is( $dt->iso8601, '1985-04-12T10:15:00' );
    is( $dt->time_zone->name, '+0400' );
}

# parse_time

{
    #hhmmss 232050
    my $dt = $iso8601->parse_time( '232050' );
    is( $dt->hms, '23:20:50' );
}

{
    #hhmm 2320
    my $dt = $iso8601->parse_time( '2320' );
    is( $dt->hms, '23:20:00' );
}

{
    #hh 23
    my $dt = $iso8601->parse_time( '23' );
    is( $dt->hms, '23:00:00' );
}

{
    #-mmss -2050
    my $dt = $iso8601->parse_time( '-2050' );
    is( $dt->minute, '20' );
    is( $dt->second, '50' );
}

{
    #-mm -20
    my $dt = $iso8601->parse_time( '-20' );
    is( $dt->minute, '20' );
}

{
    #--ss --50
    my $dt = $iso8601->parse_time( '--50' );
    is( $dt->second, '50' );
}
