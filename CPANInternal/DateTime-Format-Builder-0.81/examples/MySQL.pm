# we need to comment this out or PAUSE might index it
# pack age DateTime::Format::MySQL;

use strict;

use DateTime;

# Builder relevant stuff starts here.

use DateTime::Format::Builder (
    parsers => {
        parse_date => {
            params => [qw( year month day )],
            regex  => qr/^(\d{1,4})-(\d\d)-(\d\d)$/,
        },

        parse_datetime => {
            params => [qw( year month day hour minute second )],
            regex  => qr/^(\d{1,4})-(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d)$/,
            extra  => { time_zone => 'floating' },
        },

        parse_timestamp => [
            {
                length => 14,
                params => [qw( year month day hour minute second )],
                regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/,
                extra  => { time_zone => 'floating' },
            },
            {
                length      => 12,
                params      => [qw( year month day hour minute second )],
                regex       => qr/^(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/,
                extra       => { time_zone => 'floating' },
                postprocess => \&_fix_2_digit_year,
            },
            {
                length      => 10,
                params      => [qw( year month day hour minute )],
                regex       => qr/^(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/,
                extra       => { time_zone => 'floating' },
                postprocess => \&_fix_2_digit_year,
            },
            {
                length => 8,
                params => [qw( year month day )],
                regex  => qr/^(\d\d\d\d)(\d\d)(\d\d)$/,
                extra  => { time_zone => 'floating' },
            },
            {
                length      => 6,
                params      => [qw( year month day )],
                regex       => qr/^(\d\d)(\d\d)(\d\d)$/,
                extra       => { time_zone => 'floating' },
                postprocess => \&_fix_2_digit_year,
            },
            {
                length      => 4,
                params      => [qw( year month )],
                regex       => qr/^(\d\d)(\d\d)$/,
                extra       => { time_zone => 'floating' },
                postprocess => \&_fix_2_digit_year,
            },
            {
                length      => 2,
                params      => [qw( year )],
                regex       => qr/^(\d\d)$/,
                extra       => { time_zone => 'floating' },
                postprocess => \&_fix_2_digit_year,
            },
        ],
    },
);

sub _fix_2_digit_year {
    my %p = @_;

    $p{parsed}{year} += $p{parsed}{year} <= 69 ? 2000 : 1900;
}

# Builder relevant stuff ends here.

sub format_date {
    my ( $self, $dt ) = @_;

    return $dt->ymd('-');
}

sub format_time {
    my ( $self, $dt ) = @_;

    return $dt->hms(':');
}

sub format_datetime {
    my ( $self, $dt ) = @_;

    return $self->format_date($dt) . ' ' . $self->format_time($dt);
}

1;

__END__

=head1 NAME

DateTime::Format::MySQL - Parse and format MySQL dates and times

=head1 SYNOPSIS

  use DateTime::Format::MySQL;

  my $dt = DateTime::Format::MySQL->parse_datetime( '2003-01-16 23:12:01' );

  # 2003-01-16 23:12:01
  DateTime::Format::MySQL->format_datetime($dt);

=head1 DESCRIPTION

This module understands the formats used by MySQL for its DATE,
DATETIME, TIME, and TIMESTAMP data types.  It can be used to parse
these formats in order to create DateTime objects, and it can take a
DateTime object and produce a string representing it in the MySQL
format.

=head1 METHODS

This class offers the following methods.  All of the parsing methods
set the returned DateTime object's time zone to the floating time
zone, because MySQL does not provide time zone information.

=over 4

=item * parse_datetime($string)

=item * parse_date($string)

=item * parse_timestamp($string)

Given a value of the appropriate type, this method will return a new
C<DateTime> object.

If given an improperly formatted string, this method may die.

=item * format_date($datetime)

=item * format_time($datetime)

=item * format_datetime($datetime)

Given a C<DateTime> object, this methods returns an appropriately
formatted string.

=back

=head1 SUPPORT

Support for this module is provided via the datetime@perl.org email
list.  See http://lists.perl.org/ for more details.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT

Copyright (c) 2003 David Rolsky.  All rights reserved.  This program
is free software; you can redistribute it and/or modify it under the
same terms as Perl itself.

The full text of the license can be found in the LICENSE file included
with this module.

=head1 SEE ALSO

datetime@perl.org mailing list

http://datetime.perl.org/

=cut
