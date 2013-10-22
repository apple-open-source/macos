# t/004_croak.t - make sure we croak when we should

use Test::More 0.88;
use DateTime::Format::Strptime;

# 1..2
my $return;
eval { $return = DateTime::Format::Strptime->new( pattern => '%Y' ) };
isa_ok( $return, 'DateTime::Format::Strptime',
    'Legal Pattern in constructor should return object and not croak' );
is( $@, '', "Croak message should be empty" );

# 3..4
eval { DateTime::Format::Strptime->new( pattern => '%Y %Q' ) };
isnt( $@, undef, "Illegal pattern in constructor should croak" );
is( substr( $@, 0, 42 ), "Unidentified token in pattern: %Q in %Y %Q",
    "Croak message should reflect illegal pattern" );

#--------------------------------------------------------------------------------

#diag("\nTurned Croak Off");

my $object = DateTime::Format::Strptime->new(
    pattern    => '%Y %D',
    time_zone  => 'Australia/Melbourne',
    locale     => 'en_AU',
    on_error   => 'undef',
    diagnostic => 0,
);

# 5..6
is( $object->pattern('%Y %D'), '%Y %D',
    'Legal Pattern in pattern() should return the pattern' );
is( $object->{errmsg}, undef, "Error message should be undef" );

# 7..8
is( $object->pattern("%Q"), undef, "Illegal Pattern should return undef" );
is( $object->{errmsg},
    'Unidentified token in pattern: %Q in %Q. Leaving old pattern intact.',
    "Error message should reflect illegal pattern" );

# 9..10
is( $object->pattern("%{gumtree}"), undef,
    "Non-existing DateTime call should return undef" );
is( $object->{errmsg},
    'Unidentified token in pattern: %{gumtree} in %{gumtree}. Leaving old pattern intact.',
    "Error message should reflect illegal pattern" );

# Make sure pattern goes back to being useful
$object->pattern('%Y %D');

# 11..12
is( $object->parse_datetime("Not a datetime"), undef,
    "Non-matching date time string should return undef" );
is( $object->{errmsg}, 'Your datetime does not match your pattern.',
    "Error message should reflect non-matching datetime" );

# 13..14
is( $object->parse_datetime("2002 11/30/03"), undef,
    "Ambiguous date time string should return undef" );
is( $object->{errmsg}, 'Your two year values (03 and 2002) do not match.',
    "Error message should reflect Ambiguous date time string" );

#--------------------------------------------------------------------------------

#diag("\nTurned Croak On");
$object = DateTime::Format::Strptime->new(
    pattern    => '%Y %D',
    time_zone  => 'Australia/Melbourne',
    locale     => 'en_AU',
    on_error   => 'croak',
    diagnostic => 0,
);

{    # Make warn die so $@ is set. There's probably a better way.
    local $SIG{__WARN__} = sub { die "WARN: $_[0]" };
    eval { $object->pattern("%Q") };
}

# 15..16
isnt( $@, '', "Illegal Pattern should carp" );
is( substr( $@, 0, 74 ),
    'WARN: Unidentified token in pattern: %Q in %Q. Leaving old pattern intact.',
    "Croak message should reflect illegal pattern" );

# 17..18
eval { $object->parse_datetime("Not a datetime") };
isnt( $@, '', "Non-matching date time string should croak" );
is( substr( $@, 0, 42 ), "Your datetime does not match your pattern.",
    "Croak message should reflect non-matching datetime" );

# 19..20
eval { $object->parse_datetime("2002 11/30/03") };
isnt( $@, '', "Ambiguous date time string should croak" );
is( substr( $@, 0, 48 ), "Your two year values (03 and 2002) do not match.",
    "Croak message should reflect Ambiguous date time string" );

#--------------------------------------------------------------------------------

#diag("\nTurned Croak to Sub");
$object = DateTime::Format::Strptime->new(
    pattern    => '%Y %D',
    time_zone  => 'Australia/Melbourne',
    locale     => 'en_AU',
    on_error   => sub { $_[0]->{errmsg} = 'Oops! Teehee! ' . $_[1]; 1 },
    diagnostic => 0,
);

# 21..22
is( $object->pattern('%Y %D'), '%Y %D',
    'Legal Pattern in pattern() should return the pattern' );
is( $object->{errmsg}, undef, "Error message should be undef" );

# 23..24
is( $object->pattern("%Q"), undef, "Illegal Pattern should return undef" );
is( $object->{errmsg},
    'Oops! Teehee! Unidentified token in pattern: %Q in %Q. Leaving old pattern intact.',
    "Error message should reflect illegal pattern" );

# 25..26
is( $object->pattern("%{gumtree}"), undef,
    "Non-existing DateTime call should return undef" );
is( $object->{errmsg},
    'Oops! Teehee! Unidentified token in pattern: %{gumtree} in %{gumtree}. Leaving old pattern intact.',
    "Error message should reflect illegal pattern" );

# Make sure pattern goes back to being useful
$object->pattern('%Y %D');

# 27..28
is( $object->parse_datetime("Not a datetime"), undef,
    "Non-matching date time string should return undef" );
is( $object->{errmsg},
    'Oops! Teehee! Your datetime does not match your pattern.',
    "Error message should reflect non-matching datetime" );

# 29..30
is( $object->parse_datetime("2002 11/30/03"), undef,
    "Ambiguous date time string should return undef" );
is( $object->{errmsg},
    'Oops! Teehee! Your two year values (03 and 2002) do not match.',
    "Error message should reflect Ambiguous date time string" );

done_testing();
