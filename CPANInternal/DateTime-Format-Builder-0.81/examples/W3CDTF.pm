# we need to comment this out or PAUSE might index it
# pack age DateTime::Format::W3CDTF;

use strict;

use DateTime::Format::Builder (
    parsers => {
        parse_datetime => [
            [ preprocess => \&_parse_tz ],
            {
                params => [qw( year month day hour minute second)],
                regex =>
                    qr/^(\d{4})-(\d\d)-(\d\d)T(\d\d):(\d\d):(\d\d)\.(\d\d)$/,
                length => 22,
            },
            {
                params => [qw( year month day hour minute second)],
                regex  => qr/^(\d{4})-(\d\d)-(\d\d)T(\d\d):(\d\d):(\d\d)$/,
                length => 19,
            },
            {
                params => [qw( year month day hour minute)],
                regex  => qr/^(\d{4})-(\d\d)-(\d\d)T(\d\d):(\d\d)$/,
                length => 16,
            },
            {
                params => [qw( year month day )],
                regex  => qr/^(\d{4})-(\d\d)-(\d\d)$/,
                length => 10,
            },
            {
                params => [qw( year month )],
                regex  => qr/^(\d{4})-(\d\d)$/,
                length => 7,
                extra  => { day => 1 },
            },
            {
                params => [qw( year )],
                regex  => qr/^(\d\d\d\d)$/,
                length => 4,
                extra  => { month => 1, day => 1 }
            }
        ]
    }
);

sub _parse_tz {
    my %args = @_;
    my ( $date, $p ) = @args{qw( input parsed )};
    if ( $date =~ s/([+-]\d\d:\d\d)$// ) {
        $p->{time_zone} = $1;
    }

    # Z at end means UTC
    elsif ( $date =~ s/Z$// ) {
        $p->{time_zone} = 'UTC';
    }
    else {
        $p->{time_zone} = 'floating';
    }
    return $date;
}

sub format_datetime {
    my ( $self, $dt ) = @_;

    my $base = (
        $dt->hour || $dt->min || $dt->sec
        ? sprintf(
            '%04d-%02d-%02dT%02d:%02d:%02d',
            $dt->year, $dt->month,  $dt->day,
            $dt->hour, $dt->minute, $dt->second
            )
        : sprintf( '%04d-%02d-%02d', $dt->year, $dt->month, $dt->day )
    );

    my $tz = $dt->time_zone;

    return $base if $tz->is_floating;

    # if there is a time component
    if ( $dt->hour || $dt->min || $dt->sec ) {
        return $base . 'Z' if $tz->is_utc;

        if ( $tz->{'offset'} ) {
            return $base . offset_as_string( $tz->{'offset'} );
        }
    }
    else {
        return $base;
    }
}

# minor offset_as_string variant w/ :
#
sub offset_as_string {
    my $offset = shift;

    return undef unless defined $offset;

    my $sign = $offset < 0 ? '-' : '+';

    my $hours = $offset / ( 60 * 60 );
    $hours = abs($hours) % 24;

    my $mins = ( $offset % ( 60 * 60 ) ) / 60;

    my $secs = $offset % 60;

    return (
        $secs
        ? sprintf( '%s%02d:%02d:%02d', $sign, $hours, $mins, $secs )
        : sprintf( '%s%02d:%02d',      $sign, $hours, $mins )
    );
}

1;

__END__

=head1 NAME

DateTime::Format::W3CDTF - Parse and format W3CDTF datetime strings

=head1 SYNOPSIS

  use DateTime::Format::W3CDTF;

  my $f = DateTime::Format::W3CDTF->new;
  my $dt = $f->parse_datetime( '2003-02-15T13:50:05-05:00' );

  # 2003-02-15T13:50:05-05:00
  $f->format_datetime($dt);

=head1 DESCRIPTION

This module understands the W3CDTF date/time format, an ISO 8601 profile,
defined at http://www.w3.org/TR/NOTE-datetime.  This format as the native
date format of RSS 1.0.

It can be used to parse these formats in order to create the appropriate
objects.

=head1 METHODS

This API is currently experimental and may change in the future.

=over 4

=item * parse_datetime($string)

Given a W3CDTF datetime string, this method will return a new
C<DateTime> object.

If given an improperly formatted string, this method may die.

=item * format_datetime($datetime)

Given a C<DateTime> object, this methods returns a W3CDTF datetime
string.

=back

=head1 SUPPORT

Support for this module is provided via the datetime@perl.org email
list.  See http://lists.perl.org/ for more details.

=head1 AUTHOR

Kellan Elliott-McCrea <kellan@protest.net>

This module was inspired by C<DateTime::Format::ICal>

=head1 COPYRIGHT

Copyright (c) 2003 Kellan Elliott-McCrea.  All rights reserved.  This program
is free software; you can redistribute it and/or modify it under the
same terms as Perl itself.

The full text of the license can be found in the LICENSE file included
with this module.

=head1 SEE ALSO

datetime@perl.org mailing list

http://datetime.perl.org/

=cut
