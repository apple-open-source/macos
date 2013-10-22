use strict;

use Test::More tests => 9;

use DateTime::Format::Builder;


my %parsers = (
    parsers => {
	parse_datetime =>
	{
	    length => 8,
	    regex => qr/^abcdef$/,
	    params => [qw( year month day )],
	}
    }
);

# Verify constructor (non-)creation

# Ensure we don't build a constructor when one isn't asked for
{
    my $class = 'SampleClass1';
    eval q[
	package SampleClass1;
	use DateTime::Format::Builder
	    constructor => undef,
	    %parsers;
	1;
    ];
    ok( !$@, "No errors when creating the class." );

    diag $@ if $@;

    {
	no strict 'refs';
	ok( !( *{"${class}::new"}{CV}), "There is indeed no 'new'" );
    }

    my $parser = eval { $class->new() };
    ok( $@, "Error when trying to instantiate (no new)");
    like( $@, qr/^Can't locate object method "new" via package "$class"/, "Right error" );
}

# Ensure we don't have people wiping out their constructors
{
    my $class = 'SampleClassHasNew';
    sub SampleClassHasNew::new { return "4" }
    eval q[
	package SampleClassHasNew;
	use DateTime::Format::Builder
	    constructor => 1,
	    %parsers;
	1;
    ];
    ok( $@, "Error when creating class." );
}

# Ensure we're not accidentally overriding when we don't itnend to.
{
    my $class = 'SampleClassDont';
    sub SampleClassDont::new { return "5" }
    eval q[
	package SampleClassDont;
	use DateTime::Format::Builder
	    constructor => 0,
	    %parsers;
	1;
    ];
    ok( !$@, "No error when creating class." );
    diag $@ if $@;

    my $parser = eval { $class->new()  };
    is( $parser => 5, "Didn't override new()" );
}

# Ensure we use the given constructor 
{
    my $class = 'SampleClassGiven';
    eval q[
	package SampleClassGiven;
	use DateTime::Format::Builder
	    constructor => sub { return "6" },
	    %parsers;
	1;
    ];
    ok( !$@, "No error when creating class." );
    diag $@ if $@;

    my $parser= eval { $class->new() };
    is( $parser => 6, "Used given new()" );
}
