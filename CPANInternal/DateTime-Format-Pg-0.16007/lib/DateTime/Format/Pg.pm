package DateTime::Format::Pg;

use strict;
use vars qw ($VERSION);

use Carp;
use DateTime 0.13;
use DateTime::Duration;
use DateTime::Format::Builder 0.72;
use DateTime::TimeZone 0.06;
use DateTime::TimeZone::UTC;
use DateTime::TimeZone::Floating;

$VERSION = '0.16007';
$VERSION = eval $VERSION;

our @ISA = ('DateTime::Format::Builder');

=head1 NAME

DateTime::Format::Pg - Parse and format PostgreSQL dates and times

=head1 SYNOPSIS

  use DateTime::Format::Pg;

  my $dt = DateTime::Format::Pg->parse_datetime( '2003-01-16 23:12:01' );

  # 2003-01-16T23:12:01+0200
  DateTime::Format::Pg->format_datetime($dt);

=head1 DESCRIPTION

This module understands the formats used by PostgreSQL for its DATE, TIME,
TIMESTAMP, and INTERVAL data types.  It can be used to parse these formats in
order to create C<DateTime> or C<DateTime::Duration> objects, and it can take a
C<DateTime> or C<DateTime::Duration> object and produce a string representing
it in a format accepted by PostgreSQL.

=head1 CONSTRUCTORS

The following methods can be used to create C<DateTime::Format::Pg> objects.

=over 4

=item * new( name => value, ... )

Creates a new C<DateTime::Format::Pg> instance. This is generally not
required for simple operations. If you wish to use a different parsing
style from the default then it is more comfortable to create an object.

  my $parser = DateTime::Format::Pg->new()
  my $copy = $parser->new( 'european' => 1 );

This method accepts the following options:

=over 8

=item * european

If european is set to non-zero, dates are assumed to be in european
dd/mm/yyyy format. The default is to assume US mm/dd/yyyy format
(because this is the default for PostgreSQL).

This option only has an effect if PostgreSQL is set to output dates in
the 'PostgreSQL' (DATE only) and 'SQL' (DATE and TIMESTAMP) styles.

Note that you don't have to set this option if the PostgreSQL server has
been set to use the 'ISO' format, which is the default.

=item * server_tz

This option can be set to a C<DateTime::TimeZone> object or a string
that contains a time zone name.

This value must be set to the same value as the PostgreSQL server's time
zone in order to parse TIMESTAMP WITH TIMEZONE values in the
'PostgreSQL', 'SQL', and 'German' formats correctly.

Note that you don't have to set this option if the PostgreSQL server has
been set to use the 'ISO' format, which is the default.

=back

=cut

sub _add_param
{
  my ($to,%param) = @_;
  foreach(keys %param)
  {
    if($_ eq 'european') {
      $$to{'_european'} = $param{$_};
    } elsif($_ eq 'server_tz') {
      $$to{'_server_tz'} = $param{$_};
    } else {
      croak("Unknown option $_." );
    }
  }
}

sub european {
  my ($self,%param) = @_;
  return $param{'european'} if exists $param{'european'};
  return $self->{'_european'} if ref $self;
}

sub server_tz {
  my ($self,%param) = @_;
  return $param{''} if (ref($param{'server_tz'})) =~ /TimeZone/;
  return DateTime::TimeZone->new('name' => $param{''}) if exists $param{'server_tz'};
  return ((ref $self) && $self->{'_server_tz'});
}

sub new
{
  my $class = shift;
  my $self = bless {}, ref($class)||$class;
  if (ref $class)
  {
    $self->{'_european'} 	    = ( scalar $class->{'_european'} );
  }
  _add_param($self,@_);
  return $self;
}

=item * clone()

This method is provided for those who prefer to explicitly clone via a
method called C<clone()>.

   my $clone = $original->clone();

If called as a class method it will die.

=back

=cut

sub clone
{
  my $self = shift;
  croak('Calling object method as class method!') unless ref $self;
  return $self->new();
}

sub _create_infinity
{
    my $self = shift;
    my %p    = @_;

    if ($p{sign}) {
        return DateTime::Infinite::Past->new;
    } else {
        return DateTime::Infinite::Future->new;
    }
}

# infinite datetimes
my $pg_infinity =
{
  regex => qr/^(-)?infinity$/,
  params => [ qw(sign) ],
  constructor => \&_create_infinity,
};

# Dates (without time zone)
#
# see EncodeDateOnly() in
# pgsql-server/src/backend/utils/adt/datetime.c
#
# 2003-04-18 (USE_ISO_DATES)
#
my $pg_dateonly_iso =
{
  regex		=> qr/^(\d{4,})-(\d{2,})-(\d{2,})( BC)?$/,
  params 	=> [ qw( year    month    day     era ) ],
  postprocess	=> \&_fix_era,
};

# 18/04/2003 (USE_SQL_DATES, EuroDates)
# 18-04-2003 (USE_POSTGRES_DATES, EuroDates)
# 04/18/2003 (USE_SQL_DATES, !EuroDates)
# 04-18-2003 (USE_POSTGRES_DATES, !EuroDates)
#
my $pg_dateonly_sql =
{
  regex		=> qr/^(\d{2,})[\/-](\d{2,})[\/-](\d{4,})( BC)?$/,
  params 	=> [ qw( month       day          year    era) ],
  postprocess	=> [ \&_fix_era, \&_fix_eu ],
};

#   18.04.2003 (USE_GERMAN_DATES)
#
my $pg_dateonly_german =
{
  regex		=> qr/^(\d{2,})\.(\d{2,})\.(\d{4,})( BC)?$/,
  params 	=> [ qw( day      month     year    era ) ],
  postprocess	=> \&_fix_era
};

# Times (with/without time zone)
#
# see EncodeTimeOnly() in
# pgsql-server/src/backend/utils/adt/datetime.c
#
# 17:20:24.373942+02
# (NB: always uses numerical tz)
#
my $pg_timeonly =
{
  regex		=> qr/^(\d{2,}):(\d{2,}):(\d{2,})(\.\d+)? *([-\+][\d:]+)?$/,
  params 	=> [ qw( hour    minute  second nanosecond  time_zone) ],
  extra		=> { year => '1970' },
  postprocess	=> [ \&_fix_timezone, \&_fix_nanosecond ],
};

# Timestamps (with/without time zone)
#
# see EncodeDateTime() in
# pgsql-server/src/backend/utils/adt/datetime.c
#
# 2003-04-18 17:20:24.373942+02 (USE_ISO_DATES)
# (NB: always uses numerical tz)
#
my $pg_datetime_iso =
{
  regex		=> qr/^(\d{4,})-(\d{2,})-(\d{2,}) (\d{2,}):(\d{2,}):(\d{2,})(\.\d+)? *([-\+][\d:]+)?( BC)?$/,
  params 	=> [ qw( year    month    day      hour     minute  second nanosecond time_zone       era) ],
  postprocess 	=> [ \&_fix_era, \&_fix_timezone, \&_fix_nanosecond ],
};

# Fri 18 Apr 17:20:24.373942 2003 CEST (USE_POSTGRES_DATES, EuroDates)
#
my $pg_datetime_pg_eu =
{
  regex		=> qr/^\S{3,} (\d{2,}) (\S{3,}) (\d{2,}):(\d{2,}):(\d{2,})(\.\d+)? (\d{4,}) *((?:[-\+][\d:]+)|(?:\S+))?( BC)?$/,
  params 	=> [ qw(       day      month    hour     minute  second nanosecond year      time_zone                 era ) ],
  postprocess 	=> [ \&_fix_era, \&_fix_timezone, \&_fix_nanosecond ],
};

# Fri Apr 18 17:20:24.373942 2003 CEST (USE_POSTGRES_DATES, !EuroDates)
#
my $pg_datetime_pg_us =
{
  regex		=> qr/^\S{3,} (\S{3,}) (\s{2,}) (\d{2,}):(\d{2,}):(\d{2,})(\.\d+)? (\d{4,}) *((?:[-\+][\d:]+)|(?:\S+))?( BC)?$/,
  params 	=> [ qw(       month    day      hour     minute  second nanosecond year     time_zone                 era ) ],
  postprocess 	=> [ \&_fix_era, \&_fix_month_names, \&_fix_timezone, \&_fix_nanosecond ],
};

# 18/04/2003 17:20:24.373942 CEST (USE_SQL_DATES, EuroDates)
# 04/18/2003 17:20:24.373942 CEST (USE_SQL_DATES, !EuroDates)
#
my $pg_datetime_sql =
{
  regex		=> qr/^(\d{2,})\/(\d{2,})\/(\d{4,}) (\d{2,}):(\d{2,}):(\d{2,})(\.\d+)? *((?:[-\+][\d:]+)|(?:\S+))?( BC)?$/,
  params 	=> [ qw( month    day       year    hour     minute   second nanosecond    time_zone               era ) ],
  postprocess 	=> [ \&_fix_era, \&_fix_eu, \&_fix_timezone, \&_fix_nanosecond ],
};

# 18.04.2003 17:20:24.373942 CEST (USE_GERMAN_DATES)
#
my $pg_datetime_german =
{
  regex		=> qr/^(\d{2,})\.(\d{2,})\.(\d{4,}) (\d{2,}):(\d{2,}):(\d{2,})(\.\d+)? *((?:[-\+][\d:]+)|(?:\S+))?( BC)?$/,
  params 	=> [ qw( day      month     year    hour     minute   second nanosecond time_zone                 era ) ],
  postprocess 	=> [ \&_fix_era, \&_fix_timezone, \&_fix_nanosecond ],
};

# Helper functions
#
# Fix BC dates (1 BC => year 0, 2 BC => year -1)
#
sub _fix_era {
  my %args = @_;
  my $era = (delete $args{'parsed'}->{'era'}) || '';
  if ($era =~ m/BC/) {
    $args{'parsed'}->{'year'} = 1-$args{'parsed'}->{'year'}
  }
  return 1;
}

# Fix European dates (swap month and day)
#
sub _fix_eu {
  my %args = @_;
  if($args{'self'}->european(@{$args{'args'}}) ) {
    my $save = $args{'parsed'}->{'month'};
    $args{'parsed'}->{'month'} = $args{'parsed'}->{'day'};
    $args{'parsed'}->{'day'} = $save;
  }
  return 1;
}

# Fix month names (name => numeric)
#
my %months = (
  'jan' => 1, 'feb' => 2, 'mar' => 3, 'apr' => 4,
  'may' => 5, 'jun' => 6, 'jul' => 7, 'aug' => 8,
  'sep' => 9, 'oct' =>10, 'nov' =>11, 'dec' =>12, );

sub _fix_month_names {
  my %args = @_;
  $args{'parsed'}->{'month'} = $months{lc( $args{'parsed'}->{'month'} )};
  return $args{'parsed'}->{'month'} ? 1 : undef;
}

# Fix time zones
#
sub _fix_timezone {
  my %args = @_;
  my %param = $args{'args'} ? (@{$args{'args'}}) : ();
 
  if($param{'_force_tz'}) {
    $args{'parsed'}->{'time_zone'} = $param{'_force_tz'};
  }

  elsif(!defined($args{'parsed'}->{'time_zone'})) {
    # For very early and late dates, PostgreSQL always returns times in
    # UTC and does not tell us that it did so.
    #
    if ( $args{'parsed'}->{'year'} < 1901
    || ( $args{'parsed'}->{'year'} == 1901 && ($args{'parsed'}->{'month'} < 12 || $args{'parsed'}->{'day'} < 14) )
    ||   $args{'parsed'}->{'year'} > 2038
    || ( $args{'parsed'}->{'year'} == 2038 && ($args{'parsed'}->{'month'} > 01 || $args{'parsed'}->{'day'} > 18) )
    ) {
    $args{'parsed'}->{'time_zone'} = DateTime::TimeZone::UTC->new();
    }

    # DT->new() does not like undef time_zone params, which are generated
    # by the regexps
    #
    else {
      delete $args{'parsed'}->{'time_zone'};
    }
  }

  # Numerical time zone
  #
  
  elsif($args{'parsed'}->{'time_zone'} =~ m/^([-\+])(\d+)(?::(\d+))?(?::(\d+))?$/) {
    my $tz;
    if (length($2) == 2) {
        # regular hour notation
        my ($min, $sec) = ($3 || '00', $4 || '00');
        $tz = sprintf "%s%02d:%02d:%02d", $1, $2, $min, $sec;
    } else {
        $tz = "$1$2";
    }
    $args{'parsed'}->{'time_zone'} = $tz;
  }
  
  # Non-numerical time zone returned, which can be ambiguous :(
  #
  else
  {
    # XXX This barfs because 'self' may not necessarily be initialized
    # Need to fix it
    my $stz = $args{'self'}->_server_tz($args{'args'} ? @{$args{'args'}} : ());
    $args{'parsed'}->{'time_zone'} = $stz || 'floating';
  }

  return 1;
}

# Fix fractional seconds
#
sub _fix_nanosecond {
  my %args = @_;
  if(defined $args{'parsed'}->{'nanosecond'}) {
    $args{'parsed'}->{'nanosecond'} *= 1.0E9;
  } else {
    delete $args{'parsed'}->{'nanosecond'}
  };
  return 1;
}

# Parser generation
#
DateTime::Format::Builder->create_class
(
  parsers =>
  {
    parse_date		=> [ $pg_dateonly_iso, $pg_dateonly_sql,
    			     $pg_dateonly_german, $pg_infinity ],
    parse_timetz	=> [ $pg_timeonly, ],
    parse_timestamptz	=> [ $pg_datetime_iso, $pg_datetime_pg_eu,
                             $pg_datetime_pg_us, $pg_datetime_sql,
			     $pg_datetime_german, $pg_infinity ],
    parse_datetime	=> [ $pg_datetime_iso, $pg_datetime_pg_eu,
			     $pg_datetime_pg_us, $pg_datetime_sql,
			     $pg_datetime_german,
			     $pg_dateonly_iso, $pg_dateonly_german,
			     $pg_dateonly_sql, $pg_timeonly, $pg_infinity],
  }
);

=head1 METHODS

This class provides the following methods. The parse_datetime, parse_duration,
format_datetime, and format_duration methods are general-purpose methods
provided for compatibility with other C<DateTime::Format> modules.

The other methods are specific to the corresponding PostgreSQL date/time data
types. The names of these methods are derived from the name of the PostgreSQL
data type.  (Note: Prior to PostgreSQL 7.3, the TIMESTAMP type was equivalent
to the TIMESTAMP WITH TIME ZONE type. This data type corresponds to the
format/parse_timestamp_with_time_zone method but not to the
format/parse_timestamp method.)

=head2 PARSING METHODS

This class provides the following parsing methods.

As a general rule, the parsing methods accept input in any format that the
PostgreSQL server can produce. However, if PostgreSQL's DateStyle is set to
'SQL' or 'PostgreSQL', dates can only be parsed correctly if the 'european'
option is set correctly (i.e. same as the PostgreSQL server).  The same is true
for time zones and the 'australian_timezones' option in all modes but 'ISO'.

The default DateStyle, 'ISO', will always produce unambiguous results
and is also parsed most efficiently by this parser class. I stronlgly
recommend using this setting unless you have a good reason not to.

=over 4

=item * parse_datetime($string,...)

Given a string containing a date and/or time representation, this method
will return a new C<DateTime> object.

If the input string does not contain a date, it is set to 1970-01-01.
If the input string does not contain a time, it is set to 00:00:00. 
If the input string does not contain a time zone, it is set to the
floating time zone.

If given an improperly formatted string, this method may die.

=cut

# sub parse_datetime {
#   *** created autmatically ***
# }

=item * parse_timestamptz($string,...)

=item * parse_timestamp_with_time_zone($string,...)

Given a string containing a timestamp (date and time) representation,
this method will return a new C<DateTime> object. This method is
suitable for the TIMESTAMPTZ (or TIMESTAMP WITH TIME ZONE) type.

If the input string does not contain a time zone, it is set to the
floating time zone.

Please note that PostgreSQL does not actually store a time zone along
with the TIMESTAMP WITH TIME ZONE (or TIMESTAMPTZ) type but will just
return a time stamp converted for the server's local time zone.

If given an improperly formatted string, this method may die.

=cut

# sub parse_timestamptz {
#   *** created autmatically ***
# }

*parse_timestamp_with_time_zone = \&parse_timestamptz;

=item * parse_timestamp($string,...)

=item * parse_timestamp_without_time_zone($string,...)

Similar to the functions above, but always returns a C<DateTime> object
with a floating time zone. This method is suitable for the TIMESTAMP (or
TIMESTAMP WITHOUT TIME ZONE) type.

If the server does return a time zone, it is ignored.

If given an improperly formatted string, this method may die.

=cut

sub parse_timestamp {
  parse_timestamptz(@_,'_force_tz' => DateTime::TimeZone::Floating->new());
}

*parse_timestamp_without_time_zone = \&parse_timestamp;

=item * parse_timetz($string,...)

=item * parse_time_with_time_zone($string,...)

Given a string containing a time representation, this method will return
a new C<DateTime> object. The date is set to 1970-01-01. This method is
suitable for the TIMETZ (or TIME WITH TIME ZONE) type.

If the input string does not contain a time zone, it is set to the
floating time zone.

Please note that PostgreSQL stores a numerical offset with its TIME WITH
TIME ZONE (or TIMETZ) type. It does not store a time zone name (such as
'Europe/Rome').

If given an improperly formatted string, this method may die.

=cut

# sub parse_timetz {
#   *** created autmatically ***
# }

*parse_time_with_time_zone = \&parse_timetz;

=item * parse_time($string,...)

=item * parse_time_without_time_zone($string,...)

Similar to the functions above, but always returns an C<DateTime> object
with a floating time zone. If the server returns a time zone, it is
ignored. This method is suitable for use with the TIME (or TIME WITHOUT
TIME ZONE) type.

This ensures that the resulting C<DateTime> object will always have the
time zone expected by your application.

If given an improperly formatted string, this method may die.

=cut

sub parse_time {
  parse_timetz(@_,'_force_tz' => 'floating');
}

*parse_time_without_time_zone = \&parse_time;

=item * parse_date($string,...)

Given a string containing a date representation, this method will return
a new C<DateTime> object. The time is set to 00:00:00 (floating time
zone). This method is suitable for the DATE type.

If given an improperly formatted string, this method may die.

=cut

# sub parse_date {
#   *** generated autmatically ***
# }

=item * parse_duration($string)

=item * parse_interval($string)

Given a string containing a duration (SQL type INTERVAL) representation,
this method will return a new C<DateTime::Duration> object.

If given an improperly formatted string, this method may die.

=cut

sub parse_duration {
    my ($self, $string) = @_;
    my ($year, $mon, $day, $sgn, $hour, $min, $sec, $frc, $ago) = $string =~ m{
        \A                                     # Start of string.
        (?:\@\s*)?                             # Optional leading @.
        (?:([-+]?\d+)\s+years?\s*)?            # years
        (?:([-+]?\d+)\s+mons?\s*)?             # months
        (?:([-+]?\d+)\s+days?\s*)?             # days
        (?:                                    # Start h/m/s
          # hours
          (?:([-+])?([0-9]\d|[1-9]\d{2,}(?=:)|\d+(?=\s+hour))(?:\s+hours?)?\s*)?
          # minutes
          (?::?((?<=:)[012345]\d|\d+(?=\s+mins?))(?:\s+mins?)?\s*)?
          # seconds
          (?::?((?<=:)[012345]\d|\d+(?=\.|\s+secs?))(\.\d+)?(?:\s+secs?)?\s*)?
        ?)                                     # End hh:mm:ss
        (ago)?                                 # Optional inversion
        \z                                     # End of string
    }xms or croak "Invalid interval string $string";

    # NB: We can't just pass our values to new() because it treats all
    # arguments as negative if we have a single negative component.
    # PostgreSQL might return mixed signs, e.g. '1 mon -1day'.
    my $du = DateTime::Duration->new;

    # Define for calculations
    $_ ||= 0 for $sec, $frc, $min, $day, $mon;

    # DT::Duration only stores years, days, months, seconds (and
    # nanoseconds)
    $mon += 12 * $year if $year;
    $min += 60 * $hour if $hour;

    # HH:MM:SS.FFFF share a single sign
    if ($sgn && $sgn eq '-') {
        $_ *= -1 for $min, $sec, $frc;
    }

    $du->add(
        months      => $mon,
        days        => $day,
        minutes     => $min,
        seconds     => $sec,
        nanoseconds => $frc * DateTime::Duration::MAX_NANOSECONDS,
    );
    return $ago ? $du->inverse : $du;
}

*parse_interval = \&parse_duration;

=back

=head2 FORMATTING METHODS

This class provides the following formatting methods.

The output is always in the format mandated by the SQL standard (derived
from ISO 8601), which is parsed by PostgreSQL unambiguously in all
DateStyle modes.

=over 4

=item * format_datetime($datetime,...)

Given a C<DateTime> object, this method returns a string appropriate as
input for all date and date/time types of PostgreSQL. It will contain
date and time.

If the time zone of the C<DateTime> part is floating, the resulting
string will contain no time zone, which will result in the server's time
zone being used. Otherwise, the numerical offset of the time zone is
used.

=cut

*format_datetime = \&format_timestamptz;

=item * format_time($datetime,...)

=item * format_time_without_time_zone($datetime,...)

Given a C<DateTime> object, this method returns a string appropriate as
input for the TIME type (also known as TIME WITHOUT TIME ZONE), which
will contain the local time of the C<DateTime> object and no time zone.

=cut

sub _format_fractional
{
  my $ns = shift->nanosecond;
  return $ns ? sprintf(".%09d", "$ns") : ''
}

sub format_time
{
  my ($self,$dt,%param) = @_;
  return $dt->hms(':')._format_fractional($dt);
}

*format_time_without_time_zone = \&format_time;

=item * format_timetz($datetime)

=item * format_time_with_time_zone($datetime)

Given a C<DateTime> object, this method returns a string appropriate as
input for the TIME WITH TIME ZONE type (also known as TIMETZ), which
will contain the local part of the C<DateTime> object and a numerical
time zone.

You should not use the TIME WITH TIME ZONE type to store dates with
floating time zones.  If the time zone of the C<DateTime> part is
floating, the resulting string will contain no time zone, which will
result in the server's time zone being used.

=cut

sub _format_time_zone
{
  my $dt = shift;
  return '' if $dt->time_zone->is_floating;
  return &DateTime::TimeZone::offset_as_string($dt->offset);
}

sub format_timetz
{
  my ($self,$dt) = @_;
  return $dt->hms(':')._format_fractional($dt)._format_time_zone($dt);
}

*format_time_with_time_zone = \&format_timetz;

=item * format_date($datetime)

Given a C<DateTime> object, this method returns a string appropriate as
input for the DATE type, which will contain the date part of the
C<DateTime> object.

=cut

sub format_date
{
  my ($self,$dt) = @_;
  if($dt->is_infinite) {
    return $dt->isa('DateTime::Infinite::Future') ? 'infinity' : '-infinity';
  } elsif($dt->year()<=0) {
    return sprintf('%04d-%02d-%02d BC',
      1-$dt->year(),
      $dt->month(),
      $dt->day());
  } else {
    return $dt->ymd('-');
  }
}

=item * format_timestamp($datetime)

=item * format_timestamp_without_time_zone($datetime)

Given a C<DateTime> object, this method returns a string appropriate as
input for the TIMESTAMP type (also known as TIMESTAMP WITHOUT TIME
ZONE), which will contain the local time of the C<DateTime> object and
no time zone.

=cut

sub format_timestamp
{
  my ($self,$dt,%param) = @_;
  if($dt->is_infinite) {
    return $dt->isa('DateTime::Infinite::Future') ? 'infinity' : '-infinity';
  } elsif($dt->year()<=0) {
    return sprintf('%04d-%02d-%02d %s BC',
      1-$dt->year(),
      $dt->month(),
      $dt->day(),
      $dt->hms(':')._format_fractional($dt));
  } else {
    return $dt->ymd('-').' '.$dt->hms(':')._format_fractional($dt);
  }
}

*format_timestamp_without_time_zone = \&format_timestamp;

=item * format_timestamptz($datetime)

=item * format_timestamp_with_time_zone($datetime)

Given a C<DateTime> object, this method returns a string appropriate as
input for the TIMESTAMP WITH TIME ZONE type, which will contain the
local part of the C<DateTime> object and a numerical time zone.

You should not use the TIMESTAMP WITH TIME ZONE type to store dates with
floating time zones.  If the time zone of the C<DateTime> part is
floating, the resulting string will contain no time zone, which will
result in the server's time zone being used.

=cut

sub format_timestamptz
{
  my ($self,$dt,%param) = @_;
  if($dt->is_infinite) {
    return $dt->isa('DateTime::Infinite::Future') ? 'infinity' : '-infinity';
  } elsif($dt->year()<=0) {
    return sprintf('%04d-%02d-%02d',
      1-$dt->year(),
      $dt->month(),
      $dt->day()).
      ' '.
      $dt->hms(':').
      _format_fractional($dt).
      _format_time_zone($dt).
      ' BC';
  } else {
    return $dt->ymd('-').' '.$dt->hms(':').
      _format_fractional($dt).
      _format_time_zone($dt);
  }
}

*format_timestamp_with_time_zone = \&format_timestamptz;

=item * format_duration($du)

=item * format_interval($du)

Given a C<DateTime::Duration> object, this method returns a string appropriate
as input for the INTERVAL type.

=cut

sub format_duration {
  shift if UNIVERSAL::isa($_[0], __PACKAGE__) || $_[0] eq __PACKAGE__;
  my($du,%param) = @_;
  croak 'DateTime::Duration object expected' unless UNIVERSAL::isa($du,'DateTime::Duration');

  my %deltas = $du->deltas();
  my $output = '@';

  if($deltas{'nanoseconds'}) {
    $deltas{'seconds'} = 
      sprintf('%f', $deltas{'seconds'} + $deltas{'nanoseconds'} / 
                                        DateTime::Duration::MAX_NANOSECONDS);
  }

  foreach(qw(months days minutes seconds)) {
    $output .= ' '.$deltas{$_}.' '.$_ if $deltas{$_};
  }

  $output .= ' 0' if(length($output)<=2);
  return $output;
}

*format_interval = \&format_duration;

=back

=cut



1;

__END__

=head1 LIMITATIONS

Some output formats of PostgreSQL have limitations that can only be passed on
by this class.

As a general rules, none of these limitations apply to the 'ISO' output
format.  It is strongly recommended to use this format (and to use
PostgreSQL's to_char function when another output format that's not
supposed to be handled by a parser of this class is desired). 'ISO' is
the default but you are advised to explicitly set it at the beginnig of
the session by issuing a SET DATESTYLE TO 'ISO'; command in case the
server administrator changes that setting.

When formatting DateTime objects, this class always uses a format that's
handled unambiguously by PostgreSQL.

=head2 TIME ZONES

If DateStyle is set to 'PostgreSQL', 'SQL', or 'German', PostgreSQL does
not send numerical time zones for the TIMESTAMPTZ (or TIMESTAMP WITH
TIME ZONE) type. Unfortunatly, the time zone names used instead can be
ambiguous: For example, 'EST' can mean -0500, +1000, or +1100.

You must set the 'server_tz' variable to a time zone that is identical to that
of the PostgreSQL server. If the server is set to a different time zone (or the
underlying operating system interprets the time zone differently), the parser
will return wrong times.

You can avoid such problems by setting the server's time zone to UTC
using the SET TIME ZONE 'UTC' command and setting 'server_tz' parameter
to 'UTC' (or by using the ISO output format, of course).

=head2 EUROPEAN DATES

For the SQL (for DATE and TIMSTAMP[TZ]) and the PostgreSQL (for DATE)
output format, the server can send dates in both European-style
'dd/mm/yyyy' and in US-style 'mm/dd/yyyy' format. In order to parse
these dates correctly, you have to pass the 'european' option to the
constructor or to the C<parse_xxx> routines.

This problem does not occur when using the ISO or German output format
(and for PostgreSQL with TIMESTAMP[TZ] as month names are used then).

=head2 INTERVAL ELEMENTS

C<DateTime::Duration> stores months, days, minutes and seconds
separately. PostgreSQL only stores months and seconds and disregards the
irregular length of days due to DST switching and the irregular length
of minutes due to leap seconds. Therefore, it is not possitble to store
C<DateTime::Duration> objects as SQL INTERVALs without the loss of some
information.

=head2 NEGATIVE INTERVALS

In the SQL and German output formats, the server does not send an
indication of the sign with intervals. This means that '1 month ago' and
'1 month' are both returned as '1 mon'.

This problem can only be avoided by using the 'ISO' or 'PostgreSQL'
output format.

=head1 SUPPORT

Support for this module is provided via the datetime@perl.org email
list.  See http://lists.perl.org/ for more details.

=head1 AUTHOR

Daisuke Maki E<lt>daisuke@endeworks.jpE<gt>

=head1 AUTHOR EMERITUS 

Claus A. Faerber <perl@faerber.muc.de>

=head1 COPYRIGHT

Copyright (c) 2003 Claus A. Faerber. Copyright (c) 2005-2007 Daisuke Maki

This program is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

The full text of the license can be found in the LICENSE file included with
this module.

=head1 SEE ALSO

datetime@perl.org mailing list

http://datetime.perl.org/

=cut
