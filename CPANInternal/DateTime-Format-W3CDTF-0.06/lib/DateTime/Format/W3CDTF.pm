package DateTime::Format::W3CDTF;

use strict;
use warnings;

use vars qw ($VERSION);

$VERSION = '0.06';

use DateTime;
use DateTime::TimeZone;

sub new {
    my $class = shift;

    return bless {}, $class;
}

sub parse_datetime {
    my ( $self, $date ) = @_;

    my @fields = qw/ year month day hour minute second fraction time_zone /;
    my @values = 
       ( $date =~ /^(\d\d\d\d) # Year
                    (?:-(\d\d) # -Month
                     (?:-(\d\d) # -Day
                      (?:T
                       (\d\d):(\d\d) # Hour:Minute
                       (?:
                          :(\d\d)     # :Second
                          (\.\d+)?    # .Fractional_Second
                       )?
                       ( Z          # UTC
                       | [+-]\d\d:\d\d    # Hour:Minute TZ offset
                         (?::\d\d)?       # :Second TZ offset
                       )?)?)?)?$/x )
         or die "Invalid W3CDTF datetime string ($date)";
    my %p;
    for ( my $i=0; $i < @values; $i++ ) {  # Oh how I wish Perl had zip
       next unless defined $values[$i];
       $p{$fields[$i]} = $values[$i];
    }
    
### support for YYYY-MM-DDT24:00:00 as a syntactic form for 00:00:00 on the day following YYYY-MM-DD
### this is allowed in xsd dateTime syntactic forms, but not W3CDTF.
#     my $next_day    = 0;
#     if (defined($p{hour}) and defined($p{minute}) and defined($p{second})) {
#         if ($p{hour} eq '24') {
#             if ($p{second} eq '00' and $p{minute} eq '00') {
#                 $p{hour}    = '00';
#                 $next_day++;
#             } else {
#                 die "Cannot use hour value '24' with non-zero minutes and seconds\n";
#             }
#         }
#     }
    
    if ( !$p{time_zone} ) {
        $p{time_zone} = 'floating';
    } elsif ( $p{time_zone} eq 'Z' ) {
        $p{time_zone} = 'UTC';
    }

    if ( $p{fraction} ) {
        $p{nanosecond} = $p{fraction} * 1_000_000_000;
        delete $p{fraction}
    }

    my $dt = DateTime->new( %p );
#     if ($next_day) {
#         $dt->add( day => 1 );
#     }
    return $dt;
}

sub format_datetime {
    my ( $self, $dt ) = @_;

    my $base = sprintf(
        '%04d-%02d-%02dT%02d:%02d:%02d',
        $dt->year, $dt->month,  $dt->day,
        $dt->hour, $dt->minute, $dt->second
    );

    if ( $dt->nanosecond ) {
        my $secs = sprintf "%f", $dt->nanosecond / 1_000_000_000;
        $secs =~ s/^0//;
        $base .= $secs;
    }

    my $tz = $dt->time_zone;

    return $base if $tz->is_floating;

    return $base . 'Z' if $tz->is_utc;

    my $offset = $dt->offset();

    return $base unless defined $offset;

    return $base . _offset_as_string($offset)
}

sub format_date {
    my ( $self, $dt ) = @_;

    my $base = sprintf( '%04d-%02d-%02d', $dt->year, $dt->month, $dt->day );
    return $base;
}

# minor offset_as_string variant w/ :
#
sub _offset_as_string {
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

  my $w3c = DateTime::Format::W3CDTF->new;
  my $dt = $w3c->parse_datetime( '2003-02-15T13:50:05-05:00' );

  # 2003-02-15T13:50:05-05:00
  $w3c->format_datetime($dt);

=head1 DESCRIPTION

This module understands the W3CDTF date/time format, an ISO 8601 profile,
defined at http://www.w3.org/TR/NOTE-datetime.  This format as the native
date format of RSS 1.0.

It can be used to parse these formats in order to create the appropriate 
objects.

=head1 METHODS

This API is currently experimental and may change in the future.

=over 4

=item * new()

Returns a new W3CDTF parser object.

=item * parse_datetime($string)

Given a W3CDTF datetime string, this method will return a new
C<DateTime> object.

If given an improperly formatted string, this method may die.

=item * format_datetime($datetime)

Given a C<DateTime> object, this methods returns a W3CDTF datetime
string.

NOTE: As of version 0.4, format_datetime no longer attempts to truncate
datetimes without a time component.  This is due to the fact that C<DateTime>
doesn't distinguish between a date with no time component, and midnight.

=item * format_date($datetime)

Given a C<DateTime> object, return a W3CDTF datetime string without the time component.

=back

=head1 SUPPORT

Support for this module is provided via the datetime@perl.org email
list. See http://datetime.perl.org/?MailingList for details.

Please submit bugs to the CPAN RT system at
http://rt.cpan.org/NoAuth/ReportBug.html?Queue=datetime-format-w3cdtf or via
email at bug-datetime-format-w3cdtf@rt.cpan.org.

=head1 AUTHOR

Dave Rolsky E<lt>autarch@urth.orgE<gt>

=head1 CREDITS

This module is maintained by Gregory Todd Williams E<lt>gwilliams@cpan.orgE<gt>.
It was originally created by Kellan Elliott-McCrea E<lt>kellan@protest.netE<gt>.

This module was inspired by L<DateTime::Format::ICal>

=head1 COPYRIGHT

Copyright (c) 2009 David Rolsky.  All rights reserved.  This
program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

Copyright (c) 2003 Kellan Elliott-McCrea

Portions of the code in this distribution are derived from other
works.  Please see the CREDITS file for more details.

The full text of the license can be found in the LICENSE file included
with this module.

=head1 SEE ALSO

datetime@perl.org mailing list

http://datetime.perl.org/

=cut
