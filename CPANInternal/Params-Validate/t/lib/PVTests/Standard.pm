package PVTests::Standard;

use strict;
use warnings;

use Params::Validate qw(:all);

use PVTests;
use Test::More;


my $String = 'foo';

my ( $v1, $v2, $v3, $v4 );
my $Foo  = bless \$v1, 'Foo';
my $Bar  = bless \$v2, 'Bar';
my $Baz  = bless \$v3, 'Baz';
my $Quux = bless \$v4, 'Quux';

my @Tests =
    ( { sub    => 'sub1',
        p      => [ foo => 'a', bar => 'b' ],
        expect => q{},
      },

      {
       sub    => 'sub1',
       p      => [ foo => 'a' ],
       expect => qr|^Mandatory parameter 'bar' missing|,
      },

      { sub    => 'sub1',
        p      => [],
        expect => qr|^Mandatory parameters .* missing|,
      },

      { sub    => 'sub1',
        p      => [ foo => 'a', bar => 'b', baz => 'c' ],
        expect => qr|^The following parameter .* baz|,
      },

      { sub    => 'sub2',
        p      => [ foo => 'a', bar => 'b', baz => 'c' ],
        expect => q{},
      },

      { sub    => 'sub2',
        p      => [ foo => 'a', bar => 'b' ],
        expect => q{},
      },

      { sub    => 'sub2a',
        p      => [ foo => 'a', bar => 'b' ],
        expect => q{},
      },

      { sub    => 'sub2a',
        p      => [ foo => 'a' ],
        expect => q{},
      },

      # simple types
      { sub    => 'sub3',
        p      => [ foo => 'a',
                    bar => [ 1, 2, 3 ],
                    baz => { a => 1 },
                    quux => 'yadda',
                    brax => { qw( a b c d ) },
                  ],
        expect => q{},
      },

      { sub    => 'sub3',
        p      => [ foo => ['a'],
                    bar => [ 1, 2, 3 ],
                    baz => { a => 1 },
                    quux => 'yadda',
                    brax => { qw( a b c d ) },
                  ],
        expect =>
        qr|^The 'foo' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to [\w:]+sub3 was an 'arrayref'.* types: scalar|,
      },

      { sub    => 'sub3',
        p      => [ foo => 'foobar',
                    bar => [ 1, 2, 3 ],
                    baz => { a => 1 },
                    quux => 'yadda',
                    brax => [ qw( a b c d ) ],
                  ],
        expect =>
        qr|^The 'brax' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to [\w:]+sub3 was an 'arrayref'.* types: scalar hash|,
      },

      { sub    => 'sub3',
        p      => [ foo => 'foobar',
                    bar => { 1, 2, 3, 4 },
                    baz => { a => 1 },
                    quux => 'yadda',
                    brax => 'a',
                  ],
        expect =>
        qr|^The 'bar' parameter \("HASH\(0x[a-f0-9]+\)"\) to [\w:]+sub3 was a 'hashref'.* types: arrayref|,
      },

      # more unusual types
      { sub    => 'sub4',
        p      => [ foo => \$String,
                    bar => do { local *FH; *FH; },
                    baz => \*BAZZY,
                    quux => sub { 'a coderef' },
                  ],
        expect => q{},
      },

      { sub    => 'sub4',
        p      => [ foo => \$String,
                    bar => \*BARRY,
                    baz => \*BAZZY,
                    quux => sub { 'a coderef' },
                  ],
        expect =>
        qr|^The 'bar' parameter \("GLOB\(0x[a-f0-9]+\)"\) to [\w:]+sub4 was a 'globref'.* types: glob|,
      },

      { sub    => 'sub4',
        p      => [ foo => \$String,
                    bar => *GLOBBY,
                    baz => do { local *FH; *FH; },
                    quux => sub { 'a coderef' },
                  ],
        expect =>
        qr|^The 'baz' parameter \((?:"\*[\w:]+FH"\|GLOB)\) to [\w:]+sub4 was a 'glob'.* types: globref|,
      },

      { sub    => 'sub4',
        p      => [ foo => $String,
                    bar => do { local *FH; *FH; },
                    baz => \*BAZZY,
                    quux => sub { 'a coderef' },
                  ],
        expect =>
        qr|^The 'foo' parameter \("foo"\) to [\w:]+sub4 was a 'scalar'.* types: scalarref|,
      },

      { sub     => 'sub4',
        p       => [ foo => \$String,
                     bar => do { local *FH; *FH; },
                     baz => \*BAZZY,
                     quux => \*CODEREF,
                   ],
        expect =>
        qr|^The 'quux' parameter \("GLOB\(0x[a-f0-9]+\)"\) to [\w:]+sub4 was a 'globref'.* types: coderef|,
      },

      # test HANDLE type
      { sub    => 'sub4a',
        p      => [ foo => \*HANDLE ],
        expect => q{},
      },

      { sub    => 'sub4a',
        p      => [ foo => *HANDLE ],
        expect => q{},
      },

      { sub    => 'sub4a',
        p      => [ foo => ['not a handle'] ],
        expect => qr|^The 'foo' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to [\w:]+sub4a was an 'arrayref'.* types: glob globref|,
      },

      # test BOOLEAN type
      { sub    => 'sub4b',
        p      => [ foo => undef ],
        expect => q{},
      },

      { sub    => 'sub4b',
        p      => [ foo => 124125 ],
        expect => q{},
      },

      # isa
      { sub    => 'sub5',
        p      => [ foo => $Foo ],
        expect => q{},
      },
      { sub    => 'sub5',
        p      => [ foo => $Bar ],
        expect => q{},
      },
      { sub    => 'sub5',
        p      => [ foo => $Baz ],
        expect => q{},
      },

      { sub    => 'sub6',
        p      => [ foo => $Foo ],
        expect =>
        qr|^The 'foo' parameter \("Foo=SCALAR\(0x[a-f0-9]+\)"\) to [\w:]+sub6 was not a 'Bar'|,
      },
      { sub    => 'sub6',
        p      => [ foo => $Bar ],
        expect => q{},
      },
      { sub    => 'sub7',
        p      => [ foo => $Baz ],
        expect => q{},
      },

      { sub    => 'sub7',
        p      => [ foo => $Foo ],
        expect => qr|^The 'foo' parameter \("Foo=SCALAR\(0x[a-f0-9]+\)"\) to [\w:]+sub7 was not a 'Baz'|,
      },
      { sub    => 'sub7',
        p      => [ foo => $Bar ],
        expect => qr|^The 'foo' parameter \("Bar=SCALAR\(0x[a-f0-9]+\)"\) to [\w:]+sub7 was not a 'Baz'|,
      },
      { sub    => 'sub7',
        p      => [ foo => $Baz ],
        expect => q{},
      },

      { sub    => 'sub8',
        p      => [ foo => $Foo ],
        expect => qr|^The 'foo' parameter \("Foo=SCALAR\(0x[a-f0-9]+\)"\) to [\w:]+sub8 was not a 'Yadda'|,
      },

      { sub    => 'sub8',
        p      => [ foo => $Quux ],
        expect => q{},
      },

      # can
      { sub    => 'sub9',
        p      => [ foo => $Foo ],
        expect => q{},
      },
      { sub    => 'sub9',
        p      => [ foo => $Quux ],
        expect => q{},
      },

      { sub    => 'sub9a',
        p      => [ foo => $Foo ],
        expect =>
        qr|^The 'foo' parameter \("Foo=SCALAR\(0x[a-f0-9]+\)"\) to [\w:]+sub9a does not have the method: 'barify'|,
      },
      { sub    => 'sub9a',
        p      => [ foo => $Bar ],
        expect => q{},
      },

      { sub    => 'sub9b',
        p      => [ foo => $Baz ],
        expect =>
        qr|^The 'foo' parameter \("Baz=SCALAR\(0x[a-f0-9]+\)"\) to [\w:]+sub9b does not have the method: 'yaddaify'|,
      },
      { sub    => 'sub9b',
        p      => [ foo => $Quux ],
        expect =>
        qr|^The 'foo' parameter \("Quux=SCALAR\(0x[a-f0-9]+\)"\) to [\w:]+sub9b does not have the method: 'barify'|,
      },

      { sub    => 'sub9c',
        p      => [ foo => $Bar ],
        expect =>
        qr|^The 'foo' parameter \("Bar=SCALAR\(0x[a-f0-9]+\)"\) to [\w:]+sub9c does not have the method: 'yaddaify'|,
      },

      { sub    => 'sub9c',
        p      => [ foo => $Quux ],
        expect => q{},
      },

      # callbacks
      { sub    => 'sub10',
        p      => [ foo => 1 ],
        expect => q{},
      },

      { sub    => 'sub10',
        p      => [ foo => 19 ],
        expect => q{},
      },

      { sub    => 'sub10',
        p      => [ foo => 20 ],
        expect =>
        qr|^The 'foo' parameter \("20"\) to [\w:]+sub10 did not pass the 'less than 20' callback|,
      },

      { sub    => 'sub11',
        p      => [ foo => 1 ],
        expect => q{},
      },
      { sub    => 'sub11',
        p      => [ foo => 20 ],
        expect =>
        qr|^The 'foo' parameter \("20"\) to [\w:]+sub11 did not pass the 'less than 20' callback|,
      },

      { sub    => 'sub11',
        p      => [ foo => 0 ],
        expect =>
        qr|^The 'foo' parameter \("0"\) to [\w:]+sub11 did not pass the 'more than 0' callback|,
      },

      # mix n' match
      { sub    => 'sub12',
        p      => [ foo => 1 ],
        expect =>
        qr|^The 'foo' parameter \("1"\) to [\w:]+sub12 was a 'scalar'.* types: arrayref|,
      },

      { sub    => 'sub12',
        p      => [ foo => [ 1, 2, 3 ] ],
        expect =>
        qr|^The 'foo' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to [\w:]+sub12 did not pass the '5 elements' callback|,
      },

      { sub    => 'sub12',
        p      => [ foo => [ 1, 2, 3, 4, 5 ] ],
        expect => q{},
      },

      # positional - 1
      { sub    => 'sub13',
        p      => [ 'a' ],
        expect => qr|^1 parameter was passed to .* but 2 were expected|,
      },

      { sub    => 'sub13',
        p      => [ 'a', [ 1, 2, 3 ] ],
        expect =>
        qr|^Parameter #2 \("ARRAY\(0x[a-f0-9]+\)"\) to .* did not pass the '5 elements' callback|,
      },

      # positional - 2
      { sub    => 'sub14',
        p      => [ 'a', [ 1, 2, 3 ], $Foo ],
        expect => qr|^Parameter #3 \("Foo=SCALAR\(0x[a-f0-9]+\)"\) to .* was not a 'Bar'|,
      },

      { sub    => 'sub14',
        p      => [ 'a', [ 1, 2, 3 ], $Bar ],
        expect => q{},
      },

      # hashref named params
      { sub    => 'sub15',
        p      => [ { foo => 1, bar => { a => 1 } } ],
        expect =>
        qr|^The 'bar' parameter \("HASH\(0x[a-f0-9]+\)"\) to .* was a 'hashref'.* types: arrayref|,
      },

      { sub    => 'sub15',
        p      => [ { foo => 1 } ],
        expect => qr|^Mandatory parameter 'bar' missing|,
      },

      # positional - 3
      { sub    => 'sub16',
        p      => [ 1, 2, 3 ],
        expect => qr|^3 parameters were passed .* but 1 - 2 were expected|,
      },

      { sub    => 'sub16',
        p      => [ 1, 2 ],
        expect => q{},
      },

      { sub    => 'sub16',
        p      => [ 1 ],
        expect => q{},
      },

      { sub    => 'sub16',
        p      => [],
        expect => qr|^0 parameters were passed .* but 1 - 2 were expected|,
      },

      # positional - 4
      { sub    => 'sub17',
        p =>[ 1, 2, 3 ],
        expect => qr|^3 parameters were passed .* but 1 - 2 were expected|,
      },

      { sub    => 'sub17',
        p      => [ 1, 2 ],
        expect => q{},
      },

      { sub    => 'sub17',
        p      => [ 1 ],
        expect => q{},
      },

      { sub    => 'sub17',
        p      => [],
        expect => qr|^0 parameters were passed .* but 1 - 2 were expected|,
      },

      # positional - too few arguments supplied
      { sub    => 'sub17a',
        p      => [],
        expect => qr|^0 parameters were passed .* but 3 - 4 were expected|,
      },

      { sub    => 'sub17a',
        p      => [ 1, 2 ],
        expect => qr|^2 parameters were passed .* but 3 - 4 were expected|,
      },

      { sub    => 'sub17b',
        p      => [],
        expect => qr|^0 parameters were passed .* but 3 - 4 were expected|,
      },

      { sub    => 'sub17b',
        p      => [ 42, 2 ],
        expect => qr|^2 parameters were passed .* but 3 - 4 were expected|,
      },

      # validation options - ignore case
      { sub     => 'Foo::sub18',
        p       => [ FOO => 1 ],
        options => { ignore_case => 1 },
        expect  => q{},
      },

      { sub    => 'sub18',
        p      => [ FOO => 1 ],
        expect => qr|^The following parameter .* FOO|,
      },

      # validation options - strip leading
      { sub     => 'Foo::sub18',
        p       => [ -foo => 1 ],
        options => { strip_leading => '-' },
        expect  => q{},
      },

      { sub    => 'sub18',
        p      => [ -foo => 1 ],
        expect => qr|^The following parameter .* -foo|,
      },

      # validation options - allow extra
      { sub     => 'Foo::sub18',
        p       => [ foo => 1, bar => 1 ],
        options => { allow_extra => 1 },
        expect  =>  q{},
        return  => { foo => 1, bar => 1 },
      },

      { sub    => 'sub18',
        p      => [ foo => 1, bar => 1 ],
        expect => qr|^The following parameter .* bar|,
      },

      { sub    => 'Foo::sub19',
        p      => [ 1, 2 ],
        options => { allow_extra => 1 },
        expect => q{},
        return => [ 1, 2 ],
      },

      { sub    => 'sub19',
        p      => [ 1, 2 ],
        expect => qr|^2 parameters were passed .* but 1.*|,
      },

      # validation options - on fail
      { sub    => 'Foo::sub18',
        p      => [ bar => 1 ],
        options => { on_fail => sub { die "ERROR WAS: $_[0]" } },
        expect => qr|^ERROR WAS: The following parameter .* bar|,
      },

      { sub    => 'sub18',
        p      => [ bar => 1 ],
        expect => qr|^The following parameter .* bar|,
      },

      { sub    => 'sub20',
        p      => [ foo => undef ],
        expect => qr|^The 'foo' parameter \(undef\) to .* was an 'undef'.*|,
      },

      { sub    => 'sub21',
        p      => [ foo => undef ],
        expect => q{},
      },

      { sub    => 'sub22',
        p      => [ foo => [1] ],
        expect => qr|^The 'foo' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to .* was an 'arrayref'.*|,
      },

      { sub    => 'sub22',
        p      => [ foo => bless [1], 'object' ],
        expect => q{},
      },

      { sub    => 'sub22a',
        p      => [],
        expect => q{},
      },
      { sub    => 'sub22a',
        p      => [ foo => [1] ],
        expect => qr|^The 'foo' parameter \("ARRAY\(0x[a-f0-9]+\)"\) to .* was an 'arrayref'.*|,
      },
      { sub    => 'sub22a',
        p      => [ foo => bless [1], 'object' ],
        expect => q{},
      },

      { sub    => 'sub23',
        p      => [ '1 element' ],
        expect => q{},
      },

      { sub    => 'sub24',
        p      => [],
        expect => q{},
      },
      { sub    => 'sub24',
        p      => [ '1 element' ],
        expect => qr|^Parameter #1 \("1 element"\) to .* was a 'scalar'.*|,
      },

      { sub    => 'sub24',
        p      => [ bless [1], 'object' ],
        expect => q{},
      },

      { sub           => 'sub25',
        p             => [ 1 ],
        expect        => qr|^Odd number|,
        always_errors => 1,
      },

      # optional glob
      { sub    => 'sub26',
        p      => [ foo => 1, bar => do { local *BAR; *BAR } ],
        expect => q{},
      },
    );

sub run_tests
{
    my $count = scalar @Tests;
    $count++ for grep { $_->{return} } @Tests;

    plan tests => $count;

    for my $test (@Tests)
    {
        if ( $test->{options} )
        {
            package Foo;
            validation_options( %{ $test->{options} } );
        }

        my $sub = $test->{sub};
        my @r   = eval "$sub( \@{ \$test->{p} } )";

        if ( $test->{expect}
             && ( $test->{always_errors} 
                  || ! $ENV{PERL_NO_VALIDATION} )
           )
        {
            like( $@, $test->{expect}, "expect error with $sub" );
        }
        else
        {
            is( $@, q{}, "no error with $sub" );
        }

        next unless $test->{return};

        if ( eval { %{ $test->{return} } } )
        {
            my %r = @r;
            is_deeply( \%r, $test->{return}, "check return value for $sub - hash" );
        }
        else
        {
            is_deeply( \@r, $test->{return}, "check return value for $sub - array" );
        }
    }
}

sub sub1
{
    validate( @_, { foo => 1, bar => 1 } );
}

sub sub2
{
    validate( @_, { foo => 1, bar => 1, baz => 0 } );
}

sub sub2a
{
    validate( @_, { foo => 1, bar => { optional => 1 } } );
}

sub sub3
{
    validate( @_, { foo =>
		    { type => SCALAR },
		    bar =>
		    { type => ARRAYREF },
		    baz =>
		    { type => HASHREF },
		    quux =>
		    { type => SCALAR | ARRAYREF },
		    brax =>
		    { type => SCALAR | HASHREF },
		  }
	    );
}

sub sub4
{
    validate( @_, { foo =>
		    { type => SCALARREF },
		    bar =>
		    { type => GLOB },
		    baz =>
		    { type => GLOBREF },
		    quux =>
		    { type => CODEREF },
		  }
	    );
}

sub sub4a
{
    validate( @_, { foo => { type => HANDLE } } );
}

sub sub4b
{
    validate( @_, { foo => { type => BOOLEAN } } );
}

sub sub5
{
    validate( @_, { foo => { isa => 'Foo' } } );
}

sub sub6
{
    validate( @_, { foo => { isa => 'Bar' } } );
}

sub sub7
{
    validate( @_, { foo => { isa => 'Baz' } } );
}

sub sub8
{
    validate( @_, { foo => { isa => [ 'Foo', 'Yadda' ] } } );
}

sub sub9
{
    validate( @_, { foo => { can => 'fooify'} } );
}

sub sub9a
{
    validate( @_, { foo => { can => [ 'fooify', 'barify' ] } } );
}

sub sub9b
{
    validate( @_, { foo => { can => [ 'barify', 'yaddaify' ] } } );
}

sub sub9c
{
    validate( @_, { foo => { can => [ 'fooify', 'yaddaify' ] } } );
}

sub sub10
{
    validate( @_, { foo =>
		    { callbacks =>
		      { 'less than 20' => sub { shift() < 20 } }
		    } } );
}

sub sub11
{
    validate( @_, { foo =>
		    { callbacks =>
		      { 'less than 20' => sub { shift() < 20 },
			'more than 0'  => sub { shift() > 0 },
		      }
		    } } );
}

sub sub12
{
    validate( @_, { foo =>
		    { type => ARRAYREF,
		      callbacks =>
		      { '5 elements' => sub { @{shift()} == 5 } }
		    } } );
}

sub sub13
{
    validate_pos( @_,
		  { type => SCALAR },
		  { type => ARRAYREF,
		    callbacks => 
		    { '5 elements' => sub { @{shift()} == 5 } }
		  } );
}

sub sub14
{
    validate_pos( @_,
		  { type => SCALAR },
		  { type => ARRAYREF },
		  { isa => 'Bar' },
		);
}

sub sub15
{
    validate( @_,
	      { foo => 1,
		bar => { type => ARRAYREF }
	      } );
}

sub sub16
{
    validate_pos( @_, 1, 0 );
}

sub sub17
{
    validate_pos( @_, { type => SCALAR }, { type => SCALAR, optional => 1 } );
}

{
    package Foo;
    use Params::Validate;
    sub sub18
    {
	validate( @_, { foo => 1 } );
    }

    sub sub19
    {
	validate_pos( @_, 1 );
    }
}

sub sub17a
{
    validate_pos( @_, 1, 1, 1, 0 );
}

sub sub17b
{
    validate_pos( @_, 
		  { callbacks =>
		    { 'less than 43' => sub { shift() < 43 } }},
		  { type => SCALAR },
		  1,
		  {optional => 1});
}

sub sub18
{
    validate( @_, { foo => 1 } );
}

sub sub19
{
    validate_pos( @_, 1 );
}

sub sub20
{
    validate( @_, { foo => { type => SCALAR } } );
}

sub sub21
{
    validate( @_, { foo => { type => UNDEF | SCALAR } } );
}

sub sub22
{
    validate( @_, { foo => { type => OBJECT } } );
}

sub sub22a
{
    validate( @_, { foo => { type => OBJECT, optional => 1 } } );
}

sub sub23
{
    validate_pos( @_, 1 );
}

sub sub24
{
    validate_pos( @_, { type => OBJECT, optional => 1 } );
}

sub sub25
{
    validate( @_, { foo => 1 } );
}

sub sub26
{
    validate( @_, { foo =>
                    { type => SCALAR },
                    bar =>
		    { type => HANDLE, optional => 1 },
                  },
	    );
}


package Foo;

use Params::Validate qw(:all);

sub fooify {1}

package Bar;

@Bar::ISA = ('Foo');

sub barify {1}

package Baz;

@Baz::ISA = ('Bar');

sub bazify {1}

package Yadda;

sub yaddaify {1}

package Quux;

@Quux::ISA = ('Foo', 'Yadda');

sub quuxify {1}


1;
