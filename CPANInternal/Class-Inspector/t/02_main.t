#!/usr/bin/perl

# Formal testing for Class::Inspector

# Do all the tests on ourself, since we know we will be loaded.


use strict;
use lib ();
use File::Spec::Functions ':ALL';
BEGIN {
	$| = 1;
	unless ( $ENV{HARNESS_ACTIVE} ) {
		require FindBin;
		$FindBin::Bin = $FindBin::Bin; # Avoid a warning
		chdir catdir( $FindBin::Bin, updir() );
		lib->import(
			catdir('blib', 'arch'),
			catdir('blib', 'lib' ),
			catdir('lib'),
			);
	}
}

use Test::More tests => 54;
use Class::Inspector ();

# To make maintaining this a little faster,
# CI is defined as Class::Inspector, and
# BAD for a class we know doesn't exist.
use constant CI  => 'Class::Inspector';
use constant BAD => 'Class::Inspector::Nonexistant';

# How many functions and public methods are there in Class::Inspector
my $base_functions = 17;
my $base_public    = 12;
my $base_private   = $base_functions - $base_public;





#####################################################################
# Begin Tests

# Check the good/bad class code
ok(   CI->_class( CI ),              'Class validator works for known valid' );
ok(   CI->_class( BAD ),             'Class validator works for correctly formatted, but not installed' );
ok(   CI->_class( 'A::B::C::D::E' ), 'Class validator works for long classes' );
ok(   CI->_class( '::' ),            'Class validator allows main' );
ok(   CI->_class( '::Blah' ),        'Class validator works for main aliased' );
ok( ! CI->_class(),                  'Class validator failed for missing class' );
ok( ! CI->_class( '4teen' ),         'Class validator fails for number starting class' );
ok( ! CI->_class( 'Blah::%f' ),      'Class validator catches bad characters' );






# Check the loaded method
ok(   CI->loaded( CI ), "->loaded detects loaded" );
ok( ! CI->loaded( BAD ), "->loaded detects not loaded" );





# Check the file name methods
my $filename = CI->filename( CI );
ok( $filename eq File::Spec->catfile( "Class", "Inspector.pm" ), "->filename works correctly" );
my $inc_filename = CI->_inc_filename( CI );
ok( $inc_filename eq "Class/Inspector.pm", "->_inc_filename works correctly" );
ok( index( CI->loaded_filename(CI), $filename ) >= 0, "->loaded_filename works" );
ok( ($filename eq $inc_filename or index( CI->loaded_filename(CI), $inc_filename ) == -1), "->loaded_filename works" );
ok( index( CI->resolved_filename(CI), $filename ) >= 0, "->resolved_filename works" );
ok( ($filename eq $inc_filename or index( CI->resolved_filename(CI), $inc_filename ) == -1), "->resolved_filename works" );





# Check the installed stuff
ok( CI->installed( CI ), "->installed detects installed" );
ok( ! CI->installed( BAD ), "->installed detects not installed" );





# Check the functions
my $functions = CI->functions( CI );
ok( (ref($functions) eq 'ARRAY'
	and $functions->[0] eq '_class'
	and scalar @$functions == $base_functions),
	"->functions works correctly" );
ok( ! CI->functions( BAD ), "->functions fails correctly" );





# Check function refs
$functions = CI->function_refs( CI );
ok( (ref($functions) eq 'ARRAY'
	and ref $functions->[0]
	and ref($functions->[0]) eq 'CODE'
	and scalar @$functions == $base_functions),
	"->function_refs works correctly" );
ok( ! CI->functions( BAD ), "->function_refs fails correctly" );





# Check function_exists
ok( CI->function_exists( CI, 'installed' ),
	"->function_exists detects function that exists" );
ok( ! CI->function_exists( CI, 'nsfladf' ),
	"->function_exists fails for bad function" );
ok( ! CI->function_exists( CI ),
	"->function_exists fails for missing function" );
ok( ! CI->function_exists( BAD, 'function' ),
	"->function_exists fails for bad class" );





# Check the methods method.
# First, defined a new subclass of Class::Inspector with some additional methods
package Class::Inspector::Dummy;

use strict;
use base 'Class::Inspector';

sub _a_first { 1; }
sub adummy1 { 1; }
sub _dummy2 { 1; }
sub dummy3 { 1; }
sub installed { 1; }

package main;

my $methods = CI->methods( CI );
ok( ( ref($methods) eq 'ARRAY'
	and $methods->[0] eq '_class'
	and scalar @$methods == $base_functions),
	"->methods works for non-inheriting class" );
$methods = CI->methods( 'Class::Inspector::Dummy' );
ok( (ref($methods) eq 'ARRAY'
	and $methods->[0] eq '_a_first'
	and scalar @$methods == ($base_functions + 4)
	and scalar( grep { /dummy/ } @$methods ) == 3),
	"->methods works for inheriting class" );
ok( ! CI->methods( BAD ), "->methods fails correctly" );

# Check the variety of different possible ->methods options

# Public option
$methods = CI->methods( CI, 'public' );
ok( (ref($methods) eq 'ARRAY'
	and $methods->[0] eq 'children'
	and scalar @$methods == $base_public),
	"Public ->methods works for non-inheriting class" );
$methods = CI->methods( 'Class::Inspector::Dummy', 'public' );
ok( (ref($methods) eq 'ARRAY'
	and $methods->[0] eq 'adummy1'
	and scalar @$methods == ($base_public + 2)
	and scalar( grep { /dummy/ } @$methods ) == 2),
	"Public ->methods works for inheriting class" );
ok( ! CI->methods( BAD ), "Public ->methods fails correctly" );

# Private option
$methods = CI->methods( CI, 'private' );
ok( (ref($methods) eq 'ARRAY'
	and $methods->[0] eq '_class'
	and scalar @$methods == $base_private),
	"Private ->methods works for non-inheriting class" );
$methods = CI->methods( 'Class::Inspector::Dummy', 'private' );
ok( (ref($methods) eq 'ARRAY'
	and $methods->[0] eq '_a_first'
	and scalar @$methods == ($base_private + 2)
	and scalar( grep { /dummy/ } @$methods ) == 1),
	"Private ->methods works for inheriting class" );
ok( ! CI->methods( BAD ), "Private ->methods fails correctly" );

# Full option
$methods = CI->methods( CI, 'full' );
ok( (ref($methods) eq 'ARRAY'
	and $methods->[0] eq 'Class::Inspector::_class'
	and scalar @$methods == $base_functions),
	"Full ->methods works for non-inheriting class" );
$methods = CI->methods( 'Class::Inspector::Dummy', 'full' );
ok( (ref($methods) eq 'ARRAY'
	and $methods->[0] eq 'Class::Inspector::Dummy::_a_first'
	and scalar @$methods == ($base_functions + 4)
	and scalar( grep { /dummy/ } @$methods ) == 3),
	"Full ->methods works for inheriting class" );
ok( ! CI->methods( BAD ), "Full ->methods fails correctly" );

# Expanded option
$methods = CI->methods( CI, 'expanded' );
ok( (ref($methods) eq 'ARRAY'
	and ref($methods->[0]) eq 'ARRAY'
	and $methods->[0]->[0] eq 'Class::Inspector::_class'
	and $methods->[0]->[1] eq 'Class::Inspector'
	and $methods->[0]->[2] eq '_class'
	and ref($methods->[0]->[3]) eq 'CODE'
	and scalar @$methods == $base_functions),
	"Expanded ->methods works for non-inheriting class" );
$methods = CI->methods( 'Class::Inspector::Dummy', 'expanded' );
ok( (ref($methods) eq 'ARRAY'
	and ref($methods->[0]) eq 'ARRAY'
	and $methods->[0]->[0] eq 'Class::Inspector::Dummy::_a_first'
	and $methods->[0]->[1] eq 'Class::Inspector::Dummy'
	and $methods->[0]->[2] eq '_a_first'
	and ref($methods->[0]->[3]) eq 'CODE'
	and scalar @$methods == ($base_functions + 4)
	and scalar( grep { /dummy/ } map { $_->[2] } @$methods ) == 3),
	"Expanded ->methods works for inheriting class" );
ok( ! CI->methods( BAD ), "Expanded ->methods fails correctly" );

# Check clashing between options
ok( ! CI->methods( CI, 'public', 'private' ), "Public and private ->methods clash correctly" );
ok( ! CI->methods( CI, 'private', 'public' ), "Public and private ->methods clash correctly" );
ok( ! CI->methods( CI, 'full', 'expanded' ), "Full and expanded ->methods class correctly" );
ok( ! CI->methods( CI, 'expanded', 'full' ), "Full and expanded ->methods class correctly" );

# Check combining options
$methods = CI->methods( CI, 'public', 'expanded' );
ok( (ref($methods) eq 'ARRAY'
	and ref($methods->[0]) eq 'ARRAY'
	and $methods->[0]->[0] eq 'Class::Inspector::children'
	and $methods->[0]->[1] eq 'Class::Inspector'
	and $methods->[0]->[2] eq 'children'
	and ref($methods->[0]->[3]) eq 'CODE'
	and scalar @$methods == $base_public),
	"Public + Expanded ->methods works for non-inheriting class" );
$methods = CI->methods( 'Class::Inspector::Dummy', 'public', 'expanded' );
ok( (ref($methods) eq 'ARRAY'
	and ref($methods->[0]) eq 'ARRAY'
	and $methods->[0]->[0] eq 'Class::Inspector::Dummy::adummy1'
	and $methods->[0]->[1] eq 'Class::Inspector::Dummy'
	and $methods->[0]->[2] eq 'adummy1'
	and ref($methods->[0]->[3]) eq 'CODE'
	and scalar @$methods == ($base_public + 2)
	and scalar( grep { /dummy/ } map { $_->[2] } @$methods ) == 2),
	"Public + Expanded ->methods works for inheriting class" );
ok( ! CI->methods( BAD ), "Expanded ->methods fails correctly" );





#####################################################################
# Search Tests

# Create the classes to use
CLASSES: {
	package Foo;
	
	sub foo { 1 };
	
	package Foo::Subclass;
	
	@Foo::Subclass::ISA = 'Foo';
	
	package Bar;
	
	@Bar::ISA = 'Foo';
	
	package This;
	
	sub isa { $_[1] eq 'Foo' ? 1 : undef }
	
	1;
}

# Check trivial ->find cases
{
	is( CI->subclasses( '' ), undef, '->subclasses(bad) returns undef'  );
	is( CI->subclasses( BAD ), '',   '->subclasses(none) returns false' );
	my $rv = CI->subclasses( CI );
	is_deeply( $rv, [ 'Class::Inspector::Dummy' ], '->subclasses(CI) returns just itself' );

	# Check non-trivial ->subclasses cases
	$rv = CI->subclasses( 'Foo' );
	is_deeply( $rv, [ 'Bar', 'Foo::Subclass', 'This' ],
		'->subclasses(nontrivial) returns the expected class list' );
}





#####################################################################
# Regression Tests

# Discovered in 1.06, fixed in 1.07
# In some cases, spurious empty GLOB entries can be created in a package.
# These contain no actual symbols, but were causing ->loaded to return true.
# An empty namespace with a single spurious empty glob entry (although
# created in this test with a scalar) should return FALSE for ->loaded
$Class::Inspector::SpuriousPackage::something = 1;
$Class::Inspector::SpuriousPackage::something = 1; # Avoid a warning
ok( ! Class::Inspector->loaded('Class::Inspector::SpuriousPackage'),
	'->loaded returns false for spurious glob in package' );



# Discovered in 1.11, fixed in 1.12
# With the introduction of ->subclasses, we exposed ourselves to
# non-local problems with ->isa method implementations.
PACKAGES: {
	# The busted package
	package Class::Inspector::BrokenISA;
	use vars qw{&isa $VERSION};
	$VERSION = '0.01';
	# The test packages
	package My::Foo;
	use vars qw{$VERSION};
	$VERSION = '0.01';
	package My::Bar;
	use vars qw{$VERSION @ISA};
	$VERSION = '0.01';
	@ISA     = 'My::Foo';
}
TESTS: {
	my $rv = Class::Inspector->subclasses( 'My::Foo' );
	is_deeply( $rv, [ 'My::Bar' ],
		'->subclasses in the presence of an evil ->isa does not crash' );
}
