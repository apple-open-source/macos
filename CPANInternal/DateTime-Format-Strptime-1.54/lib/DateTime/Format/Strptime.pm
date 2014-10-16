package DateTime::Format::Strptime;
{
  $DateTime::Format::Strptime::VERSION = '1.54';
}

use strict;

use DateTime 1.00;
use DateTime::Locale 0.45;
use DateTime::TimeZone 0.79;
use Params::Validate 0.64 qw( validate SCALAR SCALARREF BOOLEAN OBJECT CODEREF );
use Carp;

use Exporter;
use vars qw( @ISA @EXPORT @EXPORT_OK %ZONEMAP %FORMATS $CROAK $errmsg);

@ISA       = 'Exporter';
@EXPORT_OK = qw( &strftime &strptime );
@EXPORT    = ();

%ZONEMAP = (
    'A'      => '+0100',     'ACDT'   => '+1030',     'ACST'   => '+0930',
    'ADT'    => 'Ambiguous', 'AEDT'   => '+1100',     'AES'    => '+1000',
    'AEST'   => '+1000',     'AFT'    => '+0430',     'AHDT'   => '-0900',
    'AHST'   => '-1000',     'AKDT'   => '-0800',     'AKST'   => '-0900',
    'AMST'   => '+0400',     'AMT'    => '+0400',     'ANAST'  => '+1300',
    'ANAT'   => '+1200',     'ART'    => '-0300',     'AST'    => 'Ambiguous',
    'AT'     => '-0100',     'AWST'   => '+0800',     'AZOST'  => '+0000',
    'AZOT'   => '-0100',     'AZST'   => '+0500',     'AZT'    => '+0400',
    'B'      => '+0200',     'BADT'   => '+0400',     'BAT'    => '+0600',
    'BDST'   => '+0200',     'BDT'    => '+0600',     'BET'    => '-1100',
    'BNT'    => '+0800',     'BORT'   => '+0800',     'BOT'    => '-0400',
    'BRA'    => '-0300',     'BST'    => 'Ambiguous', 'BT'     => 'Ambiguous',
    'BTT'    => '+0600',     'C'      => '+0300',     'CAST'   => '+0930',
    'CAT'    => 'Ambiguous', 'CCT'    => 'Ambiguous', 'CDT'    => 'Ambiguous',
    'CEST'   => '+0200',     'CET'    => '+0100',     'CETDST' => '+0200',
    'CHADT'  => '+1345',     'CHAST'  => '+1245',     'CKT'    => '-1000',
    'CLST'   => '-0300',     'CLT'    => '-0400',     'COT'    => '-0500',
    'CST'    => 'Ambiguous', 'CSuT'   => '+1030',     'CUT'    => '+0000',
    'CVT'    => '-0100',     'CXT'    => '+0700',     'ChST'   => '+1000',
    'D'      => '+0400',     'DAVT'   => '+0700',     'DDUT'   => '+1000',
    'DNT'    => '+0100',     'DST'    => '+0200',     'E'      => '+0500',
    'EASST'  => '-0500',     'EAST'   => 'Ambiguous', 'EAT'    => '+0300',
    'ECT'    => 'Ambiguous', 'EDT'    => 'Ambiguous', 'EEST'   => '+0300',
    'EET'    => '+0200',     'EETDST' => '+0300',     'EGST'   => '+0000',
    'EGT'    => '-0100',     'EMT'    => '+0100',     'EST'    => 'Ambiguous',
    'ESuT'   => '+1100',     'F'      => '+0600',     'FDT'    => 'Ambiguous',
    'FJST'   => '+1300',     'FJT'    => '+1200',     'FKST'   => '-0300',
    'FKT'    => '-0400',     'FST'    => 'Ambiguous', 'FWT'    => '+0100',
    'G'      => '+0700',     'GALT'   => '-0600',     'GAMT'   => '-0900',
    'GEST'   => '+0500',     'GET'    => '+0400',     'GFT'    => '-0300',
    'GILT'   => '+1200',     'GMT'    => '+0000',     'GST'    => 'Ambiguous',
    'GT'     => '+0000',     'GYT'    => '-0400',     'GZ'     => '+0000',
    'H'      => '+0800',     'HAA'    => '-0300',     'HAC'    => '-0500',
    'HAE'    => '-0400',     'HAP'    => '-0700',     'HAR'    => '-0600',
    'HAT'    => '-0230',     'HAY'    => '-0800',     'HDT'    => '-0930',
    'HFE'    => '+0200',     'HFH'    => '+0100',     'HG'     => '+0000',
    'HKT'    => '+0800',     'HL'     => 'local',     'HNA'    => '-0400',
    'HNC'    => '-0600',     'HNE'    => '-0500',     'HNP'    => '-0800',
    'HNR'    => '-0700',     'HNT'    => '-0330',     'HNY'    => '-0900',
    'HOE'    => '+0100',     'HST'    => '-1000',     'I'      => '+0900',
    'ICT'    => '+0700',     'IDLE'   => '+1200',     'IDLW'   => '-1200',
    'IDT'    => 'Ambiguous', 'IOT'    => '+0500',     'IRDT'   => '+0430',
    'IRKST'  => '+0900',     'IRKT'   => '+0800',     'IRST'   => '+0430',
    'IRT'    => '+0330',     'IST'    => 'Ambiguous', 'IT'     => '+0330',
    'ITA'    => '+0100',     'JAVT'   => '+0700',     'JAYT'   => '+0900',
    'JST'    => '+0900',     'JT'     => '+0700',     'K'      => '+1000',
    'KDT'    => '+1000',     'KGST'   => '+0600',     'KGT'    => '+0500',
    'KOST'   => '+1200',     'KRAST'  => '+0800',     'KRAT'   => '+0700',
    'KST'    => '+0900',     'L'      => '+1100',     'LHDT'   => '+1100',
    'LHST'   => '+1030',     'LIGT'   => '+1000',     'LINT'   => '+1400',
    'LKT'    => '+0600',     'LST'    => 'local',     'LT'     => 'local',
    'M'      => '+1200',     'MAGST'  => '+1200',     'MAGT'   => '+1100',
    'MAL'    => '+0800',     'MART'   => '-0930',     'MAT'    => '+0300',
    'MAWT'   => '+0600',     'MDT'    => '-0600',     'MED'    => '+0200',
    'MEDST'  => '+0200',     'MEST'   => '+0200',     'MESZ'   => '+0200',
    'MET'    => 'Ambiguous', 'MEWT'   => '+0100',     'MEX'    => '-0600',
    'MEZ'    => '+0100',     'MHT'    => '+1200',     'MMT'    => '+0630',
    'MPT'    => '+1000',     'MSD'    => '+0400',     'MSK'    => '+0300',
    'MSKS'   => '+0400',     'MST'    => '-0700',     'MT'     => '+0830',
    'MUT'    => '+0400',     'MVT'    => '+0500',     'MYT'    => '+0800',
    'N'      => '-0100',     'NCT'    => '+1100',     'NDT'    => '-0230',
    'NFT'    => 'Ambiguous', 'NOR'    => '+0100',     'NOVST'  => '+0700',
    'NOVT'   => '+0600',     'NPT'    => '+0545',     'NRT'    => '+1200',
    'NST'    => 'Ambiguous', 'NSUT'   => '+0630',     'NT'     => '-1100',
    'NUT'    => '-1100',     'NZDT'   => '+1300',     'NZST'   => '+1200',
    'NZT'    => '+1200',     'O'      => '-0200',     'OESZ'   => '+0300',
    'OEZ'    => '+0200',     'OMSST'  => '+0700',     'OMST'   => '+0600',
    'OZ'     => 'local',     'P'      => '-0300',     'PDT'    => '-0700',
    'PET'    => '-0500',     'PETST'  => '+1300',     'PETT'   => '+1200',
    'PGT'    => '+1000',     'PHOT'   => '+1300',     'PHT'    => '+0800',
    'PKT'    => '+0500',     'PMDT'   => '-0200',     'PMT'    => '-0300',
    'PNT'    => '-0830',     'PONT'   => '+1100',     'PST'    => 'Ambiguous',
    'PWT'    => '+0900',     'PYST'   => '-0300',     'PYT'    => '-0400',
    'Q'      => '-0400',     'R'      => '-0500',     'R1T'    => '+0200',
    'R2T'    => '+0300',     'RET'    => '+0400',     'ROK'    => '+0900',
    'S'      => '-0600',     'SADT'   => '+1030',     'SAST'   => 'Ambiguous',
    'SBT'    => '+1100',     'SCT'    => '+0400',     'SET'    => '+0100',
    'SGT'    => '+0800',     'SRT'    => '-0300',     'SST'    => 'Ambiguous',
    'SWT'    => '+0100',     'T'      => '-0700',     'TFT'    => '+0500',
    'THA'    => '+0700',     'THAT'   => '-1000',     'TJT'    => '+0500',
    'TKT'    => '-1000',     'TMT'    => '+0500',     'TOT'    => '+1300',
    'TRUT'   => '+1000',     'TST'    => '+0300',     'TUC '   => '+0000',
    'TVT'    => '+1200',     'U'      => '-0800',     'ULAST'  => '+0900',
    'ULAT'   => '+0800',     'USZ1'   => '+0200',     'USZ1S'  => '+0300',
    'USZ3'   => '+0400',     'USZ3S'  => '+0500',     'USZ4'   => '+0500',
    'USZ4S'  => '+0600',     'USZ5'   => '+0600',     'USZ5S'  => '+0700',
    'USZ6'   => '+0700',     'USZ6S'  => '+0800',     'USZ7'   => '+0800',
    'USZ7S'  => '+0900',     'USZ8'   => '+0900',     'USZ8S'  => '+1000',
    'USZ9'   => '+1000',     'USZ9S'  => '+1100',     'UTZ'    => '-0300',
    'UYT'    => '-0300',     'UZ10'   => '+1100',     'UZ10S'  => '+1200',
    'UZ11'   => '+1200',     'UZ11S'  => '+1300',     'UZ12'   => '+1200',
    'UZ12S'  => '+1300',     'UZT'    => '+0500',     'V'      => '-0900',
    'VET'    => '-0400',     'VLAST'  => '+1100',     'VLAT'   => '+1000',
    'VTZ'    => '-0200',     'VUT'    => '+1100',     'W'      => '-1000',
    'WAKT'   => '+1200',     'WAST'   => 'Ambiguous', 'WAT'    => '+0100',
    'WEST'   => '+0100',     'WESZ'   => '+0100',     'WET'    => '+0000',
    'WETDST' => '+0100',     'WEZ'    => '+0000',     'WFT'    => '+1200',
    'WGST'   => '-0200',     'WGT'    => '-0300',     'WIB'    => '+0700',
    'WIT'    => '+0900',     'WITA'   => '+0800',     'WST'    => 'Ambiguous',
    'WTZ'    => '-0100',     'WUT'    => '+0100',     'X'      => '-1100',
    'Y'      => '-1200',     'YAKST'  => '+1000',     'YAKT'   => '+0900',
    'YAPT'   => '+1000',     'YDT'    => '-0800',     'YEKST'  => '+0600',
    'YEKT'   => '+0500',     'YST'    => '-0900',     'Z'      => '+0000',
    'UTC'    => '+0000',
);

sub new {
    my $class = shift;
    my %args  = validate(
        @_, {
            pattern    => { type => SCALAR | SCALARREF },
            time_zone  => { type => SCALAR | OBJECT, optional => 1 },
            locale     => { type => SCALAR | OBJECT, default => 'English' },
            on_error   => { type => SCALAR | CODEREF, default => 'undef' },
            diagnostic => { type => SCALAR, default => 0 },
        }
    );

    croak(
        "The value supplied to on_error must be either 'croak', 'undef' or a code reference."
        )
        unless ref( $args{on_error} ) eq 'CODE'
            or $args{on_error} eq 'croak'
            or $args{on_error} eq 'undef';

    # Deal with locale
    unless ( ref( $args{locale} ) ) {
        my $locale = DateTime::Locale->load( $args{locale} );

        croak("Could not create locale from $args{locale}") unless $locale;

        $args{_locale} = $locale;
    }
    else {
        $args{_locale} = $args{locale};
        ( $args{locale} ) = ref( $args{_locale} ) =~ /::(\w+)[^:]+$/;
    }

    if ( $args{time_zone} ) {
        unless ( ref( $args{time_zone} ) ) {
            $args{time_zone}
                = DateTime::TimeZone->new( name => $args{time_zone} );

            croak("Could not create time zone from $args{time_zone}")
                unless $args{time_zone};
        }
        $args{set_time_zone} = $args{time_zone};
    }
    else {
        $args{time_zone} = DateTime::TimeZone->new( name => 'floating' );
        $args{set_time_zone} = '';
    }

    my $self = bless \%args, $class;

    # Deal with the parser
    $self->{parser} = $self->_build_parser( $args{pattern} );
    if ( $self->{parser} =~ /(%\{\w+\}|%\w)/ and $args{pattern} !~ /\%$1/ ) {
        croak("Unidentified token in pattern: $1 in $self->{pattern}");
    }

    return $self;
}

sub pattern {
    my $self    = shift;
    my $pattern = shift;

    if ($pattern) {
        my $possible_parser = $self->_build_parser($pattern);
        if ( $possible_parser =~ /(%\{\w+\}|%\w)/ and $pattern !~ /\%$1/ ) {
            $self->local_carp(
                "Unidentified token in pattern: $1 in $pattern. Leaving old pattern intact."
            ) and return undef;
        }
        else {
            $self->{parser}  = $possible_parser;
            $self->{pattern} = $pattern;
        }
    }
    return $self->{pattern};
}

sub locale {
    my $self   = shift;
    my $locale = shift;

    if ($locale) {
        my $possible_locale = DateTime::Locale->load($locale);
        unless ($possible_locale) {
            $self->local_carp(
                "Could not create locale from $locale. Leaving old locale intact."
            ) and return undef;
        }
        else {
            $self->{locale}  = $locale;
            $self->{_locale} = $possible_locale;

            # When the locale changes we need to rebuild the parser
            $self->{parser} = $self->_build_parser( $self->{pattern} );
        }
    }
    return $self->{locale};
}

sub time_zone {
    my $self      = shift;
    my $time_zone = shift;

    if ($time_zone) {
        my $possible_time_zone
            = DateTime::TimeZone->new( name => $time_zone );
        unless ($possible_time_zone) {
            $self->local_carp(
                "Could not create time zone from $time_zone. Leaving old time zone intact."
            ) and return undef;
        }
        else {
            $self->{time_zone}     = $possible_time_zone;
            $self->{set_time_zone} = $self->{time_zone};
        }
    }
    return $self->{time_zone}->name;
}

sub parse_datetime {
    my ( $self, $time_string ) = @_;

    local $^W = undef;

    # Variables from the parser
    my (
        $dow_name,   $month_name,        $century,    $day,
        $hour_24,    $hour_12,           $doy,        $month,
        $minute,     $ampm,              $second,     $week_sun_0,
        $dow_sun_0,  $dow_mon_1,         $week_mon_1, $year_100,
        $year,       $iso_week_year_100, $iso_week_year,
        $epoch,      $tz_offset,         $timezone,   $tz_olson,
        $nanosecond, $ce_year,

        $doy_dt, $epoch_dt, $use_timezone, $set_time_zone,
    );

    # Variables for DateTime
    my (
        $Year, $Month, $Day,
        $Hour, $Minute, $Second, $Nanosecond,
        $Am,   $Pm
    ) = ();

    # Run the parser
    my $parser = $self->{parser};
    eval($parser);
    die $@ if $@;

    if ( $self->{diagnostic} ) {
        print qq|

Entered     = $time_string
Parser		= $parser

dow_name    = $dow_name
month_name  = $month_name
century     = $century
day         = $day
hour_24     = $hour_24
hour_12     = $hour_12
doy         = $doy
month       = $month
minute      = $minute
ampm        = $ampm
second      = $second
nanosecond  = $nanosecond
week_sun_0  = $week_sun_0
dow_sun_0   = $dow_sun_0
dow_mon_1   = $dow_mon_1
week_mon_1  = $week_mon_1
year_100    = $year_100
year        = $year
ce_year     = $ce_year
tz_offset   = $tz_offset
tz_olson    = $tz_olson
timezone    = $timezone
epoch       = $epoch
iso_week_year     = $iso_week_year
iso_week_year_100 = $iso_week_year_100

		|;

    }

    $self->local_croak("Your datetime does not match your pattern.")
        and return undef
        if ( ( $self->{parser} =~ /\$dow_name\b/ and $dow_name eq '' )
        or ( $self->{parser} =~ /\$month_name\b/ and $month_name eq '' )
        or ( $self->{parser} =~ /\$century\b/    and $century    eq '' )
        or ( $self->{parser} =~ /\$day\b/        and $day        eq '' )
        or ( $self->{parser} =~ /\$hour_24\b/    and $hour_24    eq '' )
        or ( $self->{parser} =~ /\$hour_12\b/    and $hour_12    eq '' )
        or ( $self->{parser} =~ /\$doy\b/        and $doy        eq '' )
        or ( $self->{parser} =~ /\$month\b/      and $month      eq '' )
        or ( $self->{parser} =~ /\$minute\b/     and $minute     eq '' )
        or ( $self->{parser} =~ /\$ampm\b/       and $ampm       eq '' )
        or ( $self->{parser} =~ /\$second\b/     and $second     eq '' )
        or ( $self->{parser} =~ /\$nanosecond\b/ and $nanosecond eq '' )
        or ( $self->{parser} =~ /\$week_sun_0\b/ and $week_sun_0 eq '' )
        or ( $self->{parser} =~ /\$dow_sun_0\b/  and $dow_sun_0  eq '' )
        or ( $self->{parser} =~ /\$dow_mon_1\b/  and $dow_mon_1  eq '' )
        or ( $self->{parser} =~ /\$week_mon_1\b/ and $week_mon_1 eq '' )
        or ( $self->{parser} =~ /\$year_100\b/   and $year_100   eq '' )
        or ( $self->{parser} =~ /\$year\b/       and $year       eq '' )
        or ( $self->{parser} =~ /\$ce_year\b/    and $ce_year    eq '' )
        or ( $self->{parser} =~ /\$tz_offset\b/  and $tz_offset  eq '' )
        or ( $self->{parser} =~ /\$tz_olson\b/   and $tz_olson   eq '' )
        or ( $self->{parser} =~ /\$timezone\b/   and $timezone   eq '' )
        or ( $self->{parser} =~ /\$epoch\b/      and $epoch      eq '' ) );

    # Create a timezone to work with
    if ($tz_offset) {
        $use_timezone = $tz_offset;
    }

    if ($timezone) {
        $self->local_croak("I don't recognise the timezone $timezone.")
            and return undef
            unless $ZONEMAP{$timezone};
        $self->local_croak("The timezone '$timezone' is ambiguous.")
            and return undef
            if $ZONEMAP{$timezone} eq 'Ambiguous'
                and not( $tz_offset or $tz_olson );
        $self->local_croak(
            "Your timezones ('$tz_offset' and '$timezone') do not match.")
            and return undef
            if $tz_offset
                and $ZONEMAP{$timezone} ne 'Ambiguous'
                and $ZONEMAP{$timezone} != $tz_offset;
        $use_timezone = $ZONEMAP{$timezone}
            if $ZONEMAP{$timezone} ne 'Ambiguous';
    }

    if ($tz_olson) {
        my $tz = eval { DateTime::TimeZone->new( name => $tz_olson ) };
        if ( not $tz ) {
            print
                "Provided olson TZ didn't work ($tz_olson). Attempting to normalize it.\n"
                if $self->{diagnostic};
            $tz_olson = ucfirst lc $tz_olson;
            $tz_olson =~ s|([/_])(\w)|$1\U$2|g;
            print "   Trying $tz_olson.\n" if $self->{diagnostic};
            $tz = eval { DateTime::TimeZone->new( name => $tz_olson ) };
        }
        $self->local_croak("I don't recognise the time zone '$tz_olson'.")
            and return undef
            unless $tz;
        $use_timezone = $set_time_zone = $tz;

    }

    $use_timezone = $self->{time_zone} unless ($use_timezone);

    print "Using timezone $use_timezone.\n" if $self->{diagnostic};

    # If there's an epoch, we're done. Just need to check all the others
    if ($epoch) {
        $epoch_dt = DateTime->from_epoch(
            epoch     => $epoch,
            time_zone => $use_timezone
        );

        $Year  = $epoch_dt->year;
        $Month = $epoch_dt->month;
        $Day   = $epoch_dt->day;

        $Hour       = $epoch_dt->hour;
        $Minute     = $epoch_dt->minute;
        $Second     = $epoch_dt->second;
        $Nanosecond = $epoch_dt->nanosecond;

        print $epoch_dt->strftime("Epoch: %D %T.%N\n") if $self->{diagnostic};
    }

    # Work out the year we're working with:
    if ($year_100) {
        if ($century) {
            $Year = ( ( $century * 100 ) - 0 ) + $year_100;
        }
        else {
            print "No century, guessing for $year_100" if $self->{diagnostic};
            if ( $year_100 >= 69 and $year_100 <= 99 ) {
                print "Guessed 1900s" if $self->{diagnostic};
                $Year = 1900 + $year_100;
            }
            else {
                print "Guessed 2000s" if $self->{diagnostic};
                $Year = 2000 + $year_100;
            }
        }
    }
    if ($year) {
        $self->local_croak(
            "Your two year values ($year_100 and $year) do not match.")
            and return undef
            if ( $Year && ( $year != $Year ) );
        $Year = $year;
    }
    if ($ce_year) {
        $self->local_croak(
            "Your two year values ($ce_year and $year) do not match.")
            and return undef
            if ( $Year && ( $ce_year != $Year ) );
        $Year = $ce_year;
    }
    $self->local_croak("Your year value does not match your epoch.")
        and return undef
        if $epoch_dt
            and $Year
            and $Year != $epoch_dt->year;

    # Work out which month we want
    # Month names
    if ($month_name) {
        $self->local_croak(
            "There is no use providing a month name ($month_name) without providing a year."
            )
            and return undef
            unless $Year;
        my $month_count  = 0;
        my $month_number = 0;
        foreach my $month ( @{ $self->{_locale}->month_format_wide } ) {
            $month_count++;

            if ( lc $month eq lc $month_name ) {
                $month_number = $month_count;
                last;
            }
        }
        unless ($month_number) {
            my $month_count = 0;
            foreach
                my $month ( @{ $self->{_locale}->month_format_abbreviated } )
            {
                $month_count++;

                # When abbreviating, sometimes there's a period, sometimes not.
                $month      =~ s/\.$//;
                $month_name =~ s/\.$//;
                if ( lc $month eq lc $month_name ) {
                    $month_number = $month_count;
                    last;
                }
            }
        }
        unless ($month_number) {
            $self->local_croak(
                "$month_name is not a recognised month in this locale.")
                and return undef;
        }
        $Month = $month_number;
    }
    if ($month) {
        $self->local_croak(
            "There is no use providing a month without providing a year.")
            and return undef
            unless $Year;
        $self->local_croak("$month is too large to be a month of the year.")
            and return undef
            unless $month <= 12;
        $self->local_croak(
            "Your two month values ($month_name and $month) do not match.")
            and return undef
            if $Month
                and $month != $Month;
        $Month = $month;
    }
    $self->local_croak("Your month value does not match your epoch.")
        and return undef
        if $epoch_dt
            and $Month
            and $Month != $epoch_dt->month;
    if ($doy) {
        $self->local_croak(
            "There is no use providing a day of the year without providing a year."
            )
            and return undef
            unless $Year;
        $doy_dt = eval {
            DateTime->from_day_of_year(
                year      => $Year, day_of_year => $doy,
                time_zone => $use_timezone
            );
        };
        $self->local_croak("Day of year $Year-$doy is not valid")
            and return undef
            if $@;

        my $month = $doy_dt->month;
        $self->local_croak( "Your day of the year ($doy - in "
                . $doy_dt->month_name
                . ") is not in your month ($Month)" )
            and return undef
            if $Month
                and $month != $Month;
        $Month = $month;
    }
    $self->local_croak("Your day of the year does not match your epoch.")
        and return undef
        if $epoch_dt
            and $doy_dt
            and $doy_dt->doy != $epoch_dt->doy;

    # Day of the month
    $self->local_croak("$day is too large to be a day of the month.")
        and return undef
        unless $day <= 31;
    $self->local_croak(
        "Your day of the month ($day) does not match your day of the year.")
        and return undef
        if $doy_dt
            and $day
            and $day != $doy_dt->day;
    $Day ||=
          ($day)    ? $day
        : ($doy_dt) ? $doy_dt->day
        :             '';
    if ($Day) {
        $self->local_croak(
            "There is no use providing a day without providing a month and year."
            )
            and return undef
            unless $Year
                and $Month;
        my $dt = eval {
            DateTime->new(
                year => $Year + 0, month     => $Month + 0, day => $Day + 0,
                hour => 12,        time_zone => $use_timezone
            );
        };
        $self->local_croak("Datetime $Year-$Month-$Day is not a valid date")
            and return undef
            if $@;
        $self->local_croak("There is no day $Day in $dt->month_name, $Year")
            and return undef
            unless $dt->month == $Month;
    }
    $self->local_croak("Your day of the month does not match your epoch.")
        and return undef
        if $epoch_dt
            and $Day
            and $Day != $epoch_dt->day;

    # Hour of the day
    $self->local_croak("$hour_24 is too large to be an hour of the day.")
        and return undef
        unless $hour_24 <= 23;    #OK so leap seconds will break!
    $self->local_croak("$hour_12 is too large to be an hour of the day.")
        and return undef
        unless $hour_12 <= 12;
    $self->local_croak(
        "You must specify am or pm for 12 hour clocks ($hour_12|$ampm).")
        and return undef
        if ( $hour_12 && ( !$ampm ) );
    ( $Am, $Pm ) = @{ $self->{_locale}->am_pm_abbreviated };
    if ( lc $ampm eq lc $Pm ) {
        if ($hour_12) {
            $hour_12 += 12 if $hour_12 and $hour_12 != 12;
        }
        $self->local_croak(
            "Your am/pm value ($ampm) does not match your hour ($hour_24)")
            and return undef
            if $hour_24
                and $hour_24 < 12;
    }
    elsif ( lc $ampm eq lc $Am ) {
        if ($hour_12) {
            $hour_12 = 0 if $hour_12 == 12;
        }
        $self->local_croak(
            "Your am/pm value ($ampm) does not match your hour ($hour_24)")
            and return undef
            if $hour_24 >= 12;
    }
    if ( $hour_12 and $hour_24 ) {
        $self->local_croak(
            "You have specified mis-matching 12 and 24 hour clock information"
            )
            and return undef
            unless $hour_12 == $hour_24;
        $Hour = $hour_24;
    }
    elsif ($hour_12) {
        $Hour = $hour_12;
    }
    elsif ($hour_24) {
        $Hour = $hour_24;
    }
    $self->local_croak("Your hour does not match your epoch.")
        and return undef
        if $epoch_dt
            and $Hour
            and $Hour != $epoch_dt->hour;
    print "Set hour to $Hour.\n" if $self->{diagnostic};

    # Minutes
    $self->local_croak("$minute is too large to be a minute.")
        and return undef
        unless $minute <= 59;
    $Minute ||= $minute;
    $self->local_croak("Your minute does not match your epoch.")
        and return undef
        if $epoch_dt
            and $Minute
            and $Minute != $epoch_dt->minute;
    print "Set minute to $Minute.\n" if $self->{diagnostic};

    # Seconds
    $self->local_croak("$second is too large to be a second.")
        and return undef
        unless $second <= 59;    #OK so leap seconds will break!
    $Second ||= $second;
    $self->local_croak("Your second does not match your epoch.")
        and return undef
        if $epoch_dt
            and $Second
            and $Second != $epoch_dt->second;
    print "Set second to $Second.\n" if $self->{diagnostic};

    # Nanoeconds
    $self->local_croak("$nanosecond is too large to be a nanosecond.")
        and return undef
        unless length($nanosecond) <= 9;
    $Nanosecond ||= $nanosecond;
    $Nanosecond .= '0' while length($Nanosecond) < 9;

    #	Epoch doesn't return nanoseconds
    #	croak "Your nanosecond does not match your epoch." if $epoch_dt and $Nanosecond and $Nanosecond != $epoch_dt->nanosecond;
    print "Set nanosecond to $Nanosecond.\n" if $self->{diagnostic};

    my $potential_return = eval {
        DateTime->new(
            year  => ( $Year  || 1 ) + 0,
            month => ( $Month || 1 ) + 0,
            day   => ( $Day   || 1 ) + 0,

            hour       => ( $Hour       || 0 ) + 0,
            minute     => ( $Minute     || 0 ) + 0,
            second     => ( $Second     || 0 ) + 0,
            nanosecond => ( $Nanosecond || 0 ) + 0,

            locale    => $self->{_locale},
            time_zone => $use_timezone,
        );
    };
    $self->local_croak("Datetime is not a valid date") and return undef if $@;

    $self->local_croak(
        "Your day of the week ($dow_mon_1) does not match the date supplied: "
            . $potential_return->ymd )
        and return undef
        if $dow_mon_1
            and $potential_return->dow != $dow_mon_1;

    $self->local_croak(
        "Your day of the week ($dow_sun_0) does not match the date supplied: "
            . $potential_return->ymd )
        and return undef
        if $dow_sun_0
            and ( $potential_return->dow % 7 ) != $dow_sun_0;

    if ($dow_name) {
        my $dow_count  = 0;
        my $dow_number = 0;
        foreach my $dow ( @{ $self->{_locale}->day_format_wide } ) {
            $dow_count++;
            if ( lc $dow eq lc $dow_name ) {
                $dow_number = $dow_count;
                last;
            }
        }
        unless ($dow_number) {
            my $dow_count = 0;
            foreach my $dow ( @{ $self->{_locale}->day_format_abbreviated } )
            {
                $dow_count++;
                if ( lc $dow eq lc $dow_name ) {
                    $dow_number = $dow_count;
                    last;
                }
            }
        }
        unless ($dow_number) {
            $self->local_croak(
                "$dow_name is not a recognised day in this locale.")
                and return undef;
        }
        $self->local_croak(
            "Your day of the week ($dow_name) does not match the date supplied: "
                . $potential_return->ymd )
            and return undef
            if $dow_number
                and $potential_return->dow != $dow_number;
    }

    $self->local_croak(
        "Your week number ($week_sun_0) does not match the date supplied: "
            . $potential_return->ymd )
        and return undef
        if $week_sun_0
            and $potential_return->strftime('%U') != $week_sun_0;
    $self->local_croak(
        "Your week number ($week_mon_1) does not match the date supplied: "
            . $potential_return->ymd )
        and return undef
        if $week_mon_1
            and $potential_return->strftime('%W') != $week_mon_1;
    $self->local_croak(
        "Your ISO week year ($iso_week_year) does not match the date supplied: "
            . $potential_return->ymd )
        and return undef
        if $iso_week_year
            and $potential_return->strftime('%G') != $iso_week_year;
    $self->local_croak(
        "Your ISO week year ($iso_week_year_100) does not match the date supplied: "
            . $potential_return->ymd )
        and return undef
        if $iso_week_year_100
            and $potential_return->strftime('%g') != $iso_week_year_100;

    # Move into the timezone in the object - if there is one
    print "Potential Datetime: "
        . $potential_return->strftime("%F %T %z %Z") . "\n"
        if $self->{diagnostic};
    print "Setting timezone: " . $self->{set_time_zone} . "\n"
        if $self->{diagnostic};
    if ( $self->{set_time_zone} ) {
        $potential_return->set_time_zone( $self->{set_time_zone} );
    }
    elsif ($set_time_zone) {
        $potential_return->set_time_zone($set_time_zone);
    }
    print "Actual Datetime: "
        . $potential_return->strftime("%F %T %z %Z") . "\n"
        if $self->{diagnostic};

    return $potential_return;
}

sub parse_duration {
    croak "DateTime::Format::Strptime doesn't do durations.";
}

sub format_datetime {
    my ( $self, $dt ) = @_;
    my $pattern = $self->pattern;
    $pattern =~ s/%O/$dt->time_zone->name/eg;
    return $dt->clone->set_locale( $self->locale )->strftime($pattern);
}

sub format_duration {
    croak "DateTime::Format::Strptime doesn't do durations.";
}

sub _build_parser {
    my $self = shift;
    my $regex = my $field_list = shift;
    if ( ref $regex eq 'Regexp' ) {
        $field_list =~ s/^\(\?-xism:(.+)\)$/$1/;
    }
    my @fields = $field_list =~ m/(%\{\w+\}|%\d*.)/g;
    $field_list = join( '', @fields );

    # Locale-ize the parser
    my $ampm_list = join( '|', @{ $self->{_locale}->am_pm_abbreviated } );
    $ampm_list .= '|' . lc $ampm_list;

    my $default_date_format = $self->{_locale}->glibc_date_format;
    my @locale_format = $default_date_format =~ m/(%\{\w+\}|%\d*.)/g;
    $default_date_format = join( '', @locale_format );

    my $default_time_format = $self->{_locale}->glibc_time_format;
    @locale_format = $default_time_format =~ m/(%\{\w+\}|%\d*.)/g;
    $default_time_format = join( '', @locale_format );

    my $default_datetime_format = $self->{_locale}->glibc_datetime_format;
    @locale_format = $default_datetime_format =~ m/(%\{\w+\}|%\d*.)/g;
    $default_datetime_format = join( '', @locale_format );

    print
        "Date format: $default_date_format\nTime format: $default_time_format\nDatetime format: $default_datetime_format\n"
        if $self->{diagnostic};

    $regex      =~ s/%%/__ESCAPED_PERCENT_SIGN_MARKER__/g;
    $field_list =~ s/%%/__ESCAPED_PERCENT_SIGN_MARKER__/g;

    $regex      =~ s/%c/$self->{_locale}->glibc_datetime_format/eg;
    $field_list =~ s/%c/$default_datetime_format/eg;

    # %c is the locale's default datetime format.

    $regex      =~ s/%x/$self->{_locale}->glibc_date_format/eg;
    $field_list =~ s/%x/$default_date_format/eg;

    # %x is the locale's default date format.

    $regex      =~ s/%X/$self->{_locale}->glibc_time_format/eg;
    $field_list =~ s/%X/$default_time_format/eg;

    # %x is the locale's default time format.

    if ( ref $regex ne 'Regexp' ) {
        $regex = quotemeta($regex);
        $regex =~ s/(?<!\\)\\%/%/g;
        $regex =~ s/%\\\{([^\}]+)\\\}/%{$1}/g;
    }

    $regex      =~ s/%T/%H:%M:%S/g;
    $field_list =~ s/%T/%H%M%S/g;

    # %T is the time as %H:%M:%S.

    $regex      =~ s/%r/%I:%M:%S %p/g;
    $field_list =~ s/%r/%I%M%S%p/g;

    #is the time as %I:%M:%S %p.

    $regex      =~ s/%R/%H:%M/g;
    $field_list =~ s/%R/%H%M/g;

    #is the time as %H:%M.

    $regex      =~ s|%D|%m\\/%d\\/%y|g;
    $field_list =~ s|%D|%m%d%y|g;

    #is the same as %m/%d/%y.

    $regex      =~ s|%F|%Y\\-%m\\-%d|g;
    $field_list =~ s|%F|%Y%m%d|g;

    #is the same as %Y-%m-%d - the ISO date format.

    my $day_re = join(
        '|',
        map      { quotemeta $_ }
            sort { length $b <=> length $a }
            grep( /\W/, @{ $self->{_locale}->day_format_wide },
            @{ $self->{_locale}->day_format_abbreviated } )
    );
    $day_re .= '|' if $day_re;
    $regex      =~ s/%a/($day_re\\w+)/gi;
    $field_list =~ s/%a/#dow_name#/gi;

    # %a is the day of the week, using the locale's weekday names; either the abbreviated or full name may be specified.
    # %A is the same as %a.

    my $month_re = join(
        '|',
        map      { quotemeta $_ }
            sort { length $b <=> length $a }
            grep( /\s|\d/, @{ $self->{_locale}->month_format_wide },
            @{ $self->{_locale}->month_format_abbreviated } )
    );
    $month_re .= '|' if $month_re;
    $month_re .= '[^\\s\\d]+';
    $regex      =~ s/%[bBh]/($month_re)/g;
    $field_list =~ s/%[bBh]/#month_name#/g;

    #is the month, using the locale's month names; either the abbreviated or full name may be specified.
    # %B is the same as %b.
    # %h is the same as %b.

    #s/%c//g;
    #is replaced by the locale's appropriate date and time representation.

    $regex      =~ s/%C/([\\d ]?\\d)/g;
    $field_list =~ s/%C/#century#/g;

    #is the century number [0,99]; leading zeros are permitted by not required.

    $regex      =~ s/%[de]/([\\d ]?\\d)/g;
    $field_list =~ s/%[de]/#day#/g;

    #is the day of the month [1,31]; leading zeros are permitted but not required.
    #%e is the same as %d.

    $regex      =~ s/%[Hk]/([\\d ]?\\d)/g;
    $field_list =~ s/%[Hk]/#hour_24#/g;

    #is the hour (24-hour clock) [0,23]; leading zeros are permitted but not required.
    # %k is the same as %H

    $regex      =~ s/%g/([\\d ]?\\d)/g;
    $field_list =~ s/%g/#iso_week_year_100#/g;

    # The year corresponding to the ISO week number, but without the century (0-99).

    $regex      =~ s/%G/(\\d{4})/g;
    $field_list =~ s/%G/#iso_week_year#/g;

    # The year corresponding to the ISO week number.

    $regex      =~ s/%[Il]/([\\d ]?\\d)/g;
    $field_list =~ s/%[Il]/#hour_12#/g;

    #is the hour (12-hour clock) [1-12]; leading zeros are permitted but not required.
    # %l is the same as %I.

    $regex      =~ s/%j/(\\d{1,3})/g;
    $field_list =~ s/%j/#doy#/g;

    #is the day of the year [1,366]; leading zeros are permitted but not required.

    $regex      =~ s/%m/([\\d ]?\\d)/g;
    $field_list =~ s/%m/#month#/g;

    #is the month number [1-12]; leading zeros are permitted but not required.

    $regex      =~ s/%M/([\\d ]?\\d)/g;
    $field_list =~ s/%M/#minute#/g;

    #is the minute [0-59]; leading zeros are permitted but not required.

    $regex      =~ s/%[nt]/\\s+/g;
    $field_list =~ s/%[nt]//g;

    # %n is any white space.
    # %t is any white space.

    $regex      =~ s/%p/($ampm_list)/gi;
    $field_list =~ s/%p/#ampm#/gi;

    # %p is the locale's equivalent of either A.M./P.M. indicator for 12-hour clock.

    $regex      =~ s/%s/(\\d+)/g;
    $field_list =~ s/%s/#epoch#/g;

    # %s is the seconds since the epoch

    $regex      =~ s/%S/([\\d ]?\\d)/g;
    $field_list =~ s/%S/#second#/g;

    # %S is the seconds [0-61]; leading zeros are permitted but not required.

    $regex      =~ s/%(\d*)N/($1) ? "(\\d{$1})" : "(\\d+)"/eg;
    $field_list =~ s/%\d*N/#nanosecond#/g;

    # %N is the nanoseconds (or sub seconds really)

    $regex      =~ s/%U/([\\d ]?\\d)/g;
    $field_list =~ s/%U/#week_sun_0#/g;

    # %U is the week number of the year (Sunday as the first day of the week) as a decimal number [0-53]; leading zeros are permitted but not required.

    $regex      =~ s/%w/([0-6])/g;
    $field_list =~ s/%w/#dow_sun_0#/g;

    # is the weekday as a decimal number [0-6], with 0 representing Sunday.

    $regex      =~ s/%u/([1-7])/g;
    $field_list =~ s/%u/#dow_mon_1#/g;

    # is the weekday as a decimal number [1-7], with 1 representing Monday - a la DateTime.

    $regex      =~ s/%W/([\\d ]?\\d)/g;
    $field_list =~ s/%W/#week_mon_1#/g;

    #is the week number of the year (Monday as the first day of the week) as a decimal number [0,53]; leading zeros are permitted but not required.

    $regex      =~ s/%y/([\\d ]?\\d)/g;
    $field_list =~ s/%y/#year_100#/g;

    # is the year within the century. When a century is not otherwise specified, values in the range 69-99 refer to years in the twentieth century (1969 to 1999 inclusive); values in the range 0-68 refer to years in the twenty-first century (2000-2068 inclusive). Leading zeros are permitted but not required.

    $regex      =~ s/%Y/(\\d{4})/g;
    $field_list =~ s/%Y/#year#/g;

    # is the year including the century (for example, 1998).

    $regex      =~ s|%z|([+-]\\d{4})|g;
    $field_list =~ s/%z/#tz_offset#/g;

    # Timezone Offset.

    $regex      =~ s|%Z|(\\w+)|g;
    $field_list =~ s/%Z/#timezone#/g;

    # The short timezone name.

    $regex      =~ s|%O|(\\w+\\/\\w+)|g;
    $field_list =~ s/%O/#tz_olson#/g;

    # The Olson timezone name.

    $regex      =~ s|%\{(\w+)\}|(DateTime->can($1)) ? "(.+)" : ".+"|eg;
    $field_list =~ s|(%\{(\w+)\})|(DateTime->can($2)) ? "#$2#" : $1 |eg;

    # Any function in DateTime.

    $regex      =~ s/__ESCAPED_PERCENT_SIGN_MARKER__/\\%/g;
    $field_list =~ s/__ESCAPED_PERCENT_SIGN_MARKER__//g;

    # is replaced by %.
    #print $regex;

    $field_list =~ s/#([a-z0-9_]+)#/\$$1, /gi;
    $field_list =~ s/,\s*$//;

    return qq|($field_list) = \$time_string =~ /$regex/|;
}

# Utility functions

sub local_croak {
    my $self = $_[0];
    return &{ $self->{on_error} }(@_) if ref( $self->{on_error} );
    croak( $_[1] ) if $self->{on_error} eq 'croak';
    $self->{errmsg} = $_[1];
    return ( $self->{on_error} eq 'undef' );
}

sub local_carp {
    my $self = $_[0];
    return &{ $self->{on_error} }(@_) if ref( $self->{on_error} );
    carp( $_[1] ) if $self->{on_error} eq 'croak';
    $self->{errmsg} = $_[1];
    return ( $self->{on_error} eq 'undef' );
}

sub errmsg {
    $_[0]->{errmsg};
}

# Exportable functions:

sub strftime {
    my ( $pattern, $dt ) = @_;
    return $dt->strftime($pattern);
}

sub strptime {
    my ( $pattern, $time_string ) = @_;
    return DateTime::Format::Strptime->new(
        pattern  => $pattern,
        on_error => 'croak'
    )->parse_datetime($time_string);
}

1;

# ABSTRACT: Parse and format strp and strf time patterns

__END__

=pod

=head1 NAME

DateTime::Format::Strptime - Parse and format strp and strf time patterns

=head1 VERSION

version 1.54

=head1 SYNOPSIS

    use DateTime::Format::Strptime;

    my $strp = DateTime::Format::Strptime->new(
        pattern   => '%T',
        locale    => 'en_AU',
        time_zone => 'Australia/Melbourne',
    );

    my $dt = $strp->parse_datetime('23:16:42');

    $strp->format_datetime($dt);

    # 23:16:42

    # Croak when things go wrong:
    my $strp = DateTime::Format::Strptime->new(
        pattern   => '%T',
        locale    => 'en_AU',
        time_zone => 'Australia/Melbourne',
        on_error  => 'croak',
    );

    $newpattern = $strp->pattern('%Q');

    # Unidentified token in pattern: %Q in %Q at line 34 of script.pl

    # Do something else when things go wrong:
    my $strp = DateTime::Format::Strptime->new(
        pattern   => '%T',
        locale    => 'en_AU',
        time_zone => 'Australia/Melbourne',
        on_error  => \&phone_police,
    );

=head1 DESCRIPTION

This module implements most of C<strptime(3)>, the POSIX function that
is the reverse of C<strftime(3)>, for C<DateTime>. While C<strftime>
takes a C<DateTime> and a pattern and returns a string, C<strptime> takes
a string and a pattern and returns the C<DateTime> object
associated.

=head1 CONSTRUCTOR

=over 4

=item * new( pattern => $strptime_pattern )

Creates the format object. You must specify a pattern, you can also
specify a C<time_zone> and a C<locale>. If you specify a time zone
then any resulting C<DateTime> object will be in that time zone. If you
do not specify a C<time_zone> parameter, but there is a time zone in the
string you pass to C<parse_datetime>, then the resulting C<DateTime> will
use that time zone.

You can optionally use an on_error parameter. This parameter has three
valid options:

=over 4

=item * 'undef'

(not undef, 'undef', it's a string not an undefined value)

This is the default behavior. The module will return undef whenever it gets
upset. The error can be accessed using the C<< $object->errmsg >> method.
This is the ideal behaviour for interactive use where a user might provide an
illegal pattern or a date that doesn't match the pattern.

=item * 'croak'

(not croak, 'croak', it's a string, not a function)

This used to be the default behaviour. The module will croak with an
error message whenever it gets upset.

=item * sub{...} or \&subname

When given a code ref, the module will call that sub when it gets upset.
The sub receives two parameters: the object and the error message. Using
these two it is possible to emulate the 'undef' behavior. (Returning a
true value causes the method to return undef. Returning a false value
causes the method to bravely continue):

    sub { $_[0]->{errmsg} = $_[1]; 1 },

=back

=back

=head1 METHODS

This class offers the following methods.

=over 4

=item * parse_datetime($string)

Given a string in the pattern specified in the constructor, this method
will return a new C<DateTime> object.

If given a string that doesn't match the pattern, the formatter will
croak or return undef, depending on the setting of on_error in the constructor.

=item * format_datetime($datetime)

Given a C<DateTime> object, this methods returns a string formatted in
the object's format. This method is synonymous with C<DateTime>'s
strftime method.

=item * locale($locale)

When given a locale or C<DateTime::Locale> object, this method sets
its locale appropriately. If the locale is not understood, the method
will croak or return undef (depending on the setting of on_error in
the constructor)

If successful this method returns the current locale. (After
processing as above).

=item * pattern($strptime_pattern)

When given a pattern, this method sets the object's pattern. If the
pattern is invalid, the method will croak or return undef (depending on
the value of the C<on_error> parameter)

If successful this method returns the current pattern. (After processing
as above)

=item * time_zone($time_zone)

When given a name, offset or C<DateTime::TimeZone> object, this method
sets the object's time zone. This effects the C<DateTime> object
returned by parse_datetime

If the time zone is invalid, the method will croak or return undef
(depending on the value of the C<on_error> parameter)

If successful this method returns the current time zone. (After processing
as above)

=item * errmsg

If the on_error behavior of the object is 'undef', error messages with
this method so you can work out why things went wrong.

This code emulates a C<$DateTime::Format::Strptime> with
the C<on_error> parameter equal to C<'croak'>:

C<< $strp->pattern($pattern) or die $DateTime::Format::Strptime::errmsg >>

=back

=head1 EXPORTS

There are no methods exported by default, however the following are
available:

=over 4

=item * strptime( $strptime_pattern, $string )

Given a pattern and a string this function will return a new C<DateTime>
object.

=item * strftime( $strftime_pattern, $datetime )

Given a pattern and a C<DateTime> object this function will return a
formatted string.

=back

=head1 STRPTIME PATTERN TOKENS

The following tokens are allowed in the pattern string for strptime
(parse_datetime):

=over 4

=item * %%

The % character.

=item * %a or %A

The weekday name according to the current locale, in abbreviated form or
the full name.

=item * %b or %B or %h

The month name according to the current locale, in abbreviated form or
the full name.

=item * %C

The century number (0-99).

=item * %d or %e

The day of month (01-31). This will parse single digit numbers as well.

=item * %D

Equivalent to %m/%d/%y. (This is the American style date, very confusing
to non-Americans, especially since %d/%m/%y is	widely used in Europe.
The ISO 8601 standard pattern is %F.)

=item * %F

Equivalent to %Y-%m-%d. (This is the ISO style date)

=item * %g

The year corresponding to the ISO week number, but without the century
(0-99).

=item * %G

The year corresponding to the ISO week number.

=item * %H

The hour (00-23). This will parse single digit numbers as well.

=item * %I

The hour on a 12-hour clock (1-12).

=item * %j

The day number in the year (1-366).

=item * %m

The month number (01-12). This will parse single digit numbers as well.

=item * %M

The minute (00-59). This will parse single digit numbers as well.

=item * %n

Arbitrary whitespace.

=item * %N

Nanoseconds. For other sub-second values use C<%[number]N>.

=item * %p

The equivalent of AM or PM according to the locale in use. (See
L<DateTime::Locale>)

=item * %r

Equivalent to %I:%M:%S %p.

=item * %R

Equivalent to %H:%M.

=item * %s

Number of seconds since the Epoch.

=item * %S

The second (0-60; 60 may occur for leap seconds. See
L<DateTime::LeapSecond>).

=item * %t

Arbitrary whitespace.

=item * %T

Equivalent to %H:%M:%S.

=item * %U

The week number with Sunday the first day of the week (0-53). The first
Sunday of January is the first day of week 1.

=item * %u

The weekday number (1-7) with Monday = 1. This is the C<DateTime> standard.

=item * %w

The weekday number (0-6) with Sunday = 0.

=item * %W

The week number with Monday the first day of the week (0-53). The first
Monday of January is the first day of week 1.

=item * %y

The year within century (0-99). When a century is not otherwise specified
(with a value for %C), values in the range 69-99 refer to years in the
twentieth century (1969-1999); values in the range 00-68 refer to years in the
twenty-first century (2000-2068).

=item * %Y

The year, including century (for example, 1991).

=item * %z

An RFC-822/ISO 8601 standard time zone specification. (For example
+1100) [See note below]

=item * %Z

The timezone name. (For example EST -- which is ambiguous) [See note
below]

=item * %O

This extended token allows the use of Olson Time Zone names to appear
in parsed strings. B<NOTE>: This pattern cannot be passed to C<DateTime>'s
C<strftime()> method, but can be passed to C<format_datetime()>.

=back

=head1 AUTHOR EMERITUS

This module was created by Rick Measham.

=head1 BUGS

Please report any bugs or feature requests to
C<bug-datetime-format-strptime@rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org>. I will be notified, and then you'll automatically be
notified of progress on your bug as I make changes.

=head1 SEE ALSO

C<datetime@perl.org> mailing list.

http://datetime.perl.org/

L<perl>, L<DateTime>, L<DateTime::TimeZone>, L<DateTime::Locale>

=head1 AUTHORS

=over 4

=item *

Dave Rolsky <autarch@urth.org>

=item *

Rick Measham <rickm@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2013 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

=cut
