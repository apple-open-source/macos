# we need to comment this out or PAUSE might index it
# pack age DateTime::Format::ICal;

use strict;

use DateTime;

# Builder relevant stuff starts here.

use DateTime::Format::Builder
    parsers => {
	parse_datetime => [
	[ preprocess => \&_parse_tz ],
	{
	    length => 15,
	    params => [ qw( year month day hour minute second ) ],
	    regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)T(\d\d)(\d\d)(\d\d)$/,
	},
	{
	    length => 13,
	    params => [ qw( year month day hour minute ) ],
	    regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)T(\d\d)(\d\d)$/,
	},
	{
	    length => 11,
	    params => [ qw( year month day hour ) ],
	    regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)T(\d\d)$/,
	},
	{
	    length => 8,
	    params => [ qw( year month day ) ],
	    regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)$/,
	},
	],
    };

sub _parse_tz
{
    my %args = @_;
    my ($date, $p) = @args{qw( input parsed )};
    if ( $date =~ s/^TZID=([^:]+):// )
    {
	$p->{time_zone} = $1;
    }
    # Z at end means UTC
    elsif ( $date =~ s/Z$// )
    {
	$p->{time_zone} = 'UTC';
    }
    else
    {
	$p->{time_zone} = 'floating';
    }
    return $date;
}

# Builder relevant stuff ends here.

sub parse_duration
{
    my ( $self, $dur ) = @_;

    my @units = qw( weeks days hours minutes seconds );

    $dur =~ m{ ([\+\-])?         # Sign
               P                 # 'P' for period? This is our magic character)
               (?:
                   (?:(\d+)W)?   # Weeks
                   (?:(\d+)D)?   # Days
               )?
               (?: T             # Time prefix
                   (?:(\d+)H)?   # Hours
                   (?:(\d+)M)?   # Minutes
                   (?:(\d+)S)?   # Seconds
               )?
             }x;

    my $sign = $1;

    my %units;
    $units{weeks}   = $2 if defined $2;
    $units{days}    = $3 if defined $3;
    $units{hours}   = $4 if defined $4;
    $units{minutes} = $5 if defined $5;
    $units{seconds} = $6 if defined $6;

    die "Invalid ICal duration string ($dur)\n"
        unless %units;

    if ( $sign eq '-' )
    {
        $_ *= -1 foreach values %units;
    }

    return DateTime::Duration->new(%units);
}

sub format_datetime
{
    my ( $self, $dt ) = @_;

    my $tz = $dt->time_zone;

    unless ( $tz->is_floating || $tz->is_utc || $tz->name )
    {
        $dt = $dt->clone->set_time_zone('UTC');
        $tz = $dt->time_zone;
    }

    my $base =
        ( $dt->hour || $dt->min || $dt->sec ?
          sprintf( '%04d%02d%02dT%02d%02d%02d',
                   $dt->year, $dt->month, $dt->day,
                   $dt->hour, $dt->minute, $dt->second ) :
          sprintf( '%04d%02d%02d', $dt->year, $dt->month, $dt->day )
        );


    return $base if $tz->is_floating;

    return $base . 'Z' if $tz->is_utc;

    return 'TZID=' . $tz->name . ':' . $base;
}

sub format_duration
{
    my ( $self, $duration ) = @_;

    die "Cannot represent years or months in an iCal duration\n"
        if $duration->delta_months;

    # simple string for 0-length durations
    return '+PT0S'
        unless $duration->delta_days || $duration->delta_seconds;

    my $ical = $duration->is_positive ? '+' : '-';
    $ical .= 'P';

    if ( $duration->delta_days )
    {
        $ical .= $duration->weeks . 'W' if $duration->weeks;
        $ical .= $duration->days  . 'D' if $duration->days;
    }

    if ( $duration->delta_seconds )
    {
        $ical .= 'T';

        $ical .= $duration->hours   . 'H' if $duration->hours;
        $ical .= $duration->minutes . 'M' if $duration->minutes;
        $ical .= $duration->seconds . 'S' if $duration->seconds;
    }

    return $ical;
}


1;
