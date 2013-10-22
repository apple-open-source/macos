# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl t/File-VirtualPath.t'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..136\n"; }
END {print "not ok 1\n" unless $loaded;}
use File::VirtualPath 1.011;
$loaded = 1;
print "ok 1\n";
use strict;
use warnings;

# Set this to 1 to see complete result text for each test
my $verbose = shift( @ARGV ) ? 1 : 0;  # set from command line

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

######################################################################
# Here are some utility methods:

my $test_num = 1;  # same as the first test, above

sub result {
	$test_num++;
	my ($worked, $detail) = @_;
	$verbose or 
		$detail = substr( $detail, 0, 50 ).
		(length( $detail ) > 47 ? "..." : "");	
	print "@{[$worked ? '' : 'not ']}ok $test_num $detail\n";
}

sub message {
	my ($detail) = @_;
	print "-- $detail\n";
}

sub vis {
	my ($str) = @_;
	$str =~ s/\n/\\n/g;  # make newlines visible
	$str =~ s/\t/\\t/g;  # make tabs visible
	return( $str );
}

sub serialize {
	my ($input,$is_key) = @_;
	return( join( '', 
		ref($input) eq 'HASH' ? 
			( '{ ', ( map { 
				( serialize( $_, 1 ), serialize( $input->{$_} ) ) 
			} sort keys %{$input} ), '}, ' ) 
		: ref($input) eq 'ARRAY' ? 
			( '[ ', ( map { 
				( serialize( $_ ) ) 
			} @{$input} ), '], ' ) 
		: defined($input) ?
			"'$input'".($is_key ? ' => ' : ', ')
		: "undef".($is_key ? ' => ' : ', ')
	) );
}

######################################################################

message( "START TESTING File::VirtualPath" );

######################################################################
# testing new(), initialize(), and clone()

{
	message( "testing new(), initialize(), and clone()" );

	my ($did, $should);

	# make new with default values

	my $fvp1 = File::VirtualPath->new();  
	result( UNIVERSAL::isa( $fvp1, "File::VirtualPath" ), 
		"fvp1 = new() ret FVP obj" );

	$did = $fvp1->physical_root();
	$should = "";
	result( $did eq $should, "on init fvp1->physical_root() returns '$did'" );

	$did = $fvp1->physical_delimiter();
	$should = "/";
	result( $did eq $should, "on init fvp1->physical_delimiter() returns '$did'" );

	$did = $fvp1->path_delimiter();
	$should = "/";
	result( $did eq $should, "on init fvp1->path_delimiter() returns '$did'" );

	$did = serialize( scalar( $fvp1->path() ) );
	$should = "[ '', ], ";
	result( $did eq $should, "on init fvp1->path() returns '$did'" );

	$did = $fvp1->current_path_level();
	result( $did == 0, "on init fvp1->current_path_level() returns '$did'" );

	# make new with provided values

	my $fvp2 = File::VirtualPath->new( '/home/joe/aardvark', '/', ':', ':init' );  
	result( UNIVERSAL::isa( $fvp2, "File::VirtualPath" ), 
		"fvp2 = new( '/home/joe/aardvark', '/', ':', ':init' ) ret FVP obj" );

	$did = $fvp2->physical_root();
	$should = "/home/joe/aardvark";
	result( $did eq $should, "on init fvp2->physical_root() returns '$did'" );

	$did = $fvp2->physical_delimiter();
	$should = "/";
	result( $did eq $should, "on init fvp2->physical_delimiter() returns '$did'" );

	$did = $fvp2->path_delimiter();
	$should = ":";
	result( $did eq $should, "on init fvp2->path_delimiter() returns '$did'" );

	$did = serialize( scalar( $fvp2->path() ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "on init fvp2->path() returns '$did'" );

	$did = $fvp2->current_path_level();
	result( $did == 0, "on init fvp2->current_path_level() returns '$did'" );

	$did = $fvp2->current_path_level( 1 );
	result( $did == 1, "on set fvp2->current_path_level( 1 ) returns '$did'" );
	
	# now we test clone of default values

	my $fvp3 = $fvp1->clone();  
	result( UNIVERSAL::isa( $fvp3, "File::VirtualPath" ), 
		"fvp3 = fvp1->clone() ret FVP obj" );

	$did = $fvp3->physical_root();
	$should = "";
	result( $did eq $should, "on init fvp3->physical_root() returns '$did'" );

	$did = $fvp3->physical_delimiter();
	$should = "/";
	result( $did eq $should, "on init fvp3->physical_delimiter() returns '$did'" );

	$did = $fvp3->path_delimiter();
	$should = "/";
	result( $did eq $should, "on init fvp3->path_delimiter() returns '$did'" );

	$did = serialize( scalar( $fvp3->path() ) );
	$should = "[ '', ], ";
	result( $did eq $should, "on init fvp3->path() returns '$did'" );

	$did = $fvp3->current_path_level();
	result( $did == 0, "on init fvp3->current_path_level() returns '$did'" );
	
	# now we test clone of provided values

	my $fvp4 = $fvp2->clone();  
	result( UNIVERSAL::isa( $fvp4, "File::VirtualPath" ), 
		"fvp4 = fvp2->clone() ret FVP obj" );

	$did = $fvp4->physical_root();
	$should = "/home/joe/aardvark";
	result( $did eq $should, "on init fvp4->physical_root() returns '$did'" );

	$did = $fvp4->physical_delimiter();
	$should = "/";
	result( $did eq $should, "on init fvp4->physical_delimiter() returns '$did'" );

	$did = $fvp4->path_delimiter();
	$should = ":";
	result( $did eq $should, "on init fvp4->path_delimiter() returns '$did'" );

	$did = serialize( scalar( $fvp4->path() ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "on init fvp4->path() returns '$did'" );

	$did = $fvp4->current_path_level();
	result( $did == 1, "on init fvp4->current_path_level() returns '$did'" );
}

######################################################################
# test the other methods

{
	my ($fvp, $did, $should);
	
	# first initialize data we will be reading from
	
	$fvp = File::VirtualPath->new(); 

	message( "testing setter/getter methods on default object" );

	# first check the main setter/getter methods

	$did = $fvp->physical_root( '/home/joe/aardvark' );
	$should = "/home/joe/aardvark";
	result( $did eq $should, "physical_root( '/home/joe/aardvark' ) returns '$did'" );

	$did = $fvp->physical_delimiter( '/' );
	$should = "/";
	result( $did eq $should, "physical_delimiter( '/' ) returns '$did'" );

	$did = $fvp->path_delimiter( ':' );
	$should = ":";
	result( $did eq $should, "path_delimiter( ':' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( '' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( '' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( '.' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( '.' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ':.' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( ':.' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( 'init' ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "path( 'init' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ':init' ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "path( ':init' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( '..' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( '..' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ':..' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( ':..' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( '..:init' ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "path( '..:init' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( 'init:..' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( 'init:..' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ':..:init' ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "path( ':..:init' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ':init:..' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( ':init:..' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ':..:init:' ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "path( ':..:init:' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ':init:..:' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( ':init:..:' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( 'one:two:#*:' ) ) );
	$should = "[ '', 'one', 'two', ], ";
	result( $did eq $should, "path( 'one:two:#*:' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( 'one:two:#*:three' ) ) );
	$should = "[ '', 'one', 'two', 'three', ], ";
	result( $did eq $should, "path( 'one:two:#*:three' ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( [''] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( [''] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['.'] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( ['.'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['', '.'] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( ['', '.'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['init'] ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "path( ['init'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['', 'init'] ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "path( ['', 'init'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['..'] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( ['..'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['', '..'] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( ['', '..'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['..', 'init'] ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "path( ['..', 'init'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['init', '..'] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( ['init', '..'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['', '..', 'init'] ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "path( ['', '..', 'init'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['', 'init', '..'] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( ['', 'init', '..'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['', '..', 'init', ''] ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "path( ['', '..', 'init', ''] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['', 'init', '..', ''] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "path( ['', 'init', '..', ''] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['one', 'two', '#*', ''] ) ) );
	$should = "[ '', 'one', 'two', ], ";
	result( $did eq $should, "path( ['one', 'two', '#*', ''] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ['one', 'two', '#*', 'three'] ) ) );
	$should = "[ '', 'one', 'two', 'three', ], ";
	result( $did eq $should, "path( ['one', 'two', '#*', 'three'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path( ':starter' ) ) );
	$should = "[ '', 'starter', ], ";
	result( $did eq $should, "path( ':starter' ) returns '$did'" );

	$did = $fvp->current_path_level( 1 );
	result( $did == 1, "current_path_level( 1 ) returns '$did'" );

	message( "testing child_path type methods on current object" );

	# first test child_path() itself

	$did = serialize( scalar( $fvp->child_path( '' ) ) );
	$should = "[ '', 'starter', ], ";
	result( $did eq $should, "child_path( '' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( '.' ) ) );
	$should = "[ '', 'starter', ], ";
	result( $did eq $should, "child_path( '.' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ':.' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "child_path( ':.' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( 'init' ) ) );
	$should = "[ '', 'starter', 'init', ], ";
	result( $did eq $should, "child_path( 'init' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ':init' ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "child_path( ':init' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( '..' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "child_path( '..' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ':..' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "child_path( ':..' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( '..:init' ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "child_path( '..:init' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( 'init:..' ) ) );
	$should = "[ '', 'starter', ], ";
	result( $did eq $should, "child_path( 'init:..' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ':..:init' ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "child_path( ':..:init' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ':init:..' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "child_path( ':init:..' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ':..:init:' ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "child_path( ':..:init:' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ':init:..:' ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "child_path( ':init:..:' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( 'one:two:#*:' ) ) );
	$should = "[ '', 'starter', 'one', 'two', ], ";
	result( $did eq $should, "child_path( 'one:two:#*:' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( 'one:two:#*:three' ) ) );
	$should = "[ '', 'starter', 'one', 'two', 'three', ], ";
	result( $did eq $should, "child_path( 'one:two:#*:three' ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( [''] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "child_path( [''] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['.'] ) ) );
	$should = "[ '', 'starter', ], ";
	result( $did eq $should, "child_path( ['.'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['', '.'] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "child_path( ['', '.'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['init'] ) ) );
	$should = "[ '', 'starter', 'init', ], ";
	result( $did eq $should, "child_path( ['init'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['', 'init'] ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "child_path( ['', 'init'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['..'] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "child_path( ['..'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['', '..'] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "child_path( ['', '..'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['..', 'init'] ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "child_path( ['..', 'init'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['init', '..'] ) ) );
	$should = "[ '', 'starter', ], ";
	result( $did eq $should, "child_path( ['init', '..'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['', '..', 'init'] ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "child_path( ['', '..', 'init'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['', 'init', '..'] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "child_path( ['', 'init', '..'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['', '..', 'init', ''] ) ) );
	$should = "[ '', 'init', ], ";
	result( $did eq $should, "child_path( ['', '..', 'init', ''] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['', 'init', '..', ''] ) ) );
	$should = "[ '', ], ";
	result( $did eq $should, "child_path( ['', 'init', '..', ''] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['one', 'two', '#*', ''] ) ) );
	$should = "[ '', 'starter', 'one', 'two', ], ";
	result( $did eq $should, "child_path( ['one', 'two', '#*', ''] ) returns '$did'" );

	$did = serialize( scalar( $fvp->child_path( ['one', 'two', '#*', 'three'] ) ) );
	$should = "[ '', 'starter', 'one', 'two', 'three', ], ";
	result( $did eq $should, "child_path( ['one', 'two', '#*', 'three'] ) returns '$did'" );

	# now test child_path_obj()

	my $fvp2 = $fvp->child_path_obj( 'further' );  
	result( UNIVERSAL::isa( $fvp2, "File::VirtualPath" ), 
		"fvp2 = child_path_obj( 'further' ) ret FVP obj" );

	$did = $fvp2->physical_root();
	$should = "/home/joe/aardvark";
	result( $did eq $should, "on init fvp2->physical_root() returns '$did'" );

	$did = $fvp2->physical_delimiter();
	$should = "/";
	result( $did eq $should, "on init fvp2->physical_delimiter() returns '$did'" );

	$did = $fvp2->path_delimiter();
	$should = ":";
	result( $did eq $should, "on init fvp2->path_delimiter() returns '$did'" );

	$did = serialize( scalar( $fvp2->path() ) );
	$should = "[ '', 'starter', 'further', ], ";
	result( $did eq $should, "on init fvp2->path() returns '$did'" );

	$did = $fvp2->current_path_level();
	result( $did == 1, "on init fvp2->current_path_level() returns '$did'" );
	
	# now test chdir() briefly, since its trivial
	
	$did = serialize( scalar( $fvp->chdir( ['inwego'] ) ) );
	$should = "[ '', 'starter', 'inwego', ], ";
	result( $did eq $should, "chdir( ['inwego'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path() ) );
	$should = "[ '', 'starter', 'inwego', ], ";
	result( $did eq $should, "path() returns '$did'" );
	
	$did = serialize( scalar( $fvp->chdir( ['..'] ) ) );
	$should = "[ '', 'starter', ], ";
	result( $did eq $should, "chdir( ['..'] ) returns '$did'" );

	$did = serialize( scalar( $fvp->path() ) );
	$should = "[ '', 'starter', ], ";
	result( $did eq $should, "path() returns '$did'" );
	
	message( "testing to_string type methods on current object" );

	$did = $fvp->path_string();
	$should = ":starter";
	result( $did eq $should, "path_string() returns '$did'" );

	$did = $fvp->physical_path_string();
	$should = "/home/joe/aardvark/starter";
	result( $did eq $should, "physical_path_string() returns '$did'" );

	$did = $fvp->path_string( 1 );
	$should = ":starter:";
	result( $did eq $should, "path_string( 1 ) returns '$did'" );

	$did = $fvp->physical_path_string( 1 );
	$should = "/home/joe/aardvark/starter/";
	result( $did eq $should, "physical_path_string( 1 ) returns '$did'" );

	$did = $fvp->child_path_string();
	$should = ":starter";
	result( $did eq $should, "child_path_string() returns '$did'" );

	$did = $fvp->physical_child_path_string();
	$should = "/home/joe/aardvark/starter";
	result( $did eq $should, "physical_child_path_string() returns '$did'" );

	$did = $fvp->child_path_string( 'myfile.pl' );
	$should = ":starter:myfile.pl";
	result( $did eq $should, "child_path_string( 'myfile.pl' ) returns '$did'" );

	$did = $fvp->physical_child_path_string( 'myfile.pl' );
	$should = "/home/joe/aardvark/starter/myfile.pl";
	result( $did eq $should, "physical_child_path_string( 'myfile.pl' ) returns '$did'" );

	$did = $fvp->child_path_string( 'myfile.pl', 1 );
	$should = ":starter:myfile.pl:";
	result( $did eq $should, "child_path_string( 'myfile.pl', 1 ) returns '$did'" );

	$did = $fvp->physical_child_path_string( 'myfile.pl', 1 );
	$should = "/home/joe/aardvark/starter/myfile.pl/";
	result( $did eq $should, "physical_child_path_string( 'myfile.pl', 1 ) returns '$did'" );

	$did = $fvp->child_path_string( undef, 1 );
	$should = ":starter:";
	result( $did eq $should, "child_path_string( undef, 1 ) returns '$did'" );

	$did = $fvp->physical_child_path_string( undef, 1 );
	$should = "/home/joe/aardvark/starter/";
	result( $did eq $should, "physical_child_path_string( undef, 1 ) returns '$did'" );
	
	message( "testing one-element type methods on current object" );

	$did = serialize( scalar( $fvp->path( 'one:two:three' ) ) );
	$should = "[ '', 'one', 'two', 'three', ], ";
	result( $did eq $should, "path( 'one:two:three' ) returns '$did'" );

	$did = $fvp->path_element();
	$should = "";
	result( $did eq $should, "path_element() returns '$did'" );

	$did = $fvp->path_element( 0 );
	$should = "";
	result( $did eq $should, "path_element( 0 ) returns '$did'" );

	$did = $fvp->path_element( 1 );
	$should = "one";
	result( $did eq $should, "path_element( 1 ) returns '$did'" );

	$did = $fvp->path_element( -1 );
	$should = "three";
	result( $did eq $should, "path_element( -1 ) returns '$did'" );

	$did = $fvp->path_element( undef, 'four' );
	$should = "four";
	result( $did eq $should, "path_element( undef, 'four' ) returns '$did'" );

	$did = $fvp->path_element( 2, 'five' );
	$should = "five";
	result( $did eq $should, "path_element( 2, 'five' ) returns '$did'" );

	$did = $fvp->current_path_level( 0 );
	result( $did == 0, "current_path_level( 0 ) returns '$did'" );

	$did = $fvp->current_path_level();
	result( $did == 0, "current_path_level() returns '$did'" );

	$did = $fvp->current_path_element();
	$should = "four";
	result( $did eq $should, "current_path_element() returns '$did'" );

	$did = $fvp->inc_path_level();
	result( $did == 1, "inc_path_level() returns '$did'" );

	$did = $fvp->current_path_level();
	result( $did == 1, "current_path_level() returns '$did'" );

	$did = $fvp->current_path_element();
	$should = "one";
	result( $did eq $should, "current_path_element() returns '$did'" );

	$did = $fvp->inc_path_level();
	result( $did == 2, "inc_path_level() returns '$did'" );

	$did = $fvp->current_path_level();
	result( $did == 2, "current_path_level() returns '$did'" );

	$did = $fvp->current_path_element();
	$should = "five";
	result( $did eq $should, "current_path_element() returns '$did'" );

	$did = $fvp->current_path_element( 'six' );
	$should = "six";
	result( $did eq $should, "current_path_element( 'six' ) returns '$did'" );

	$did = $fvp->current_path_element();
	$should = "six";
	result( $did eq $should, "current_path_element() returns '$did'" );

	$did = $fvp->dec_path_level();
	result( $did == 1, "dec_path_level() returns '$did'" );

	$did = $fvp->current_path_level();
	result( $did == 1, "current_path_level() returns '$did'" );

	$did = $fvp->current_path_element();
	$should = "one";
	result( $did eq $should, "current_path_element() returns '$did'" );

	$did = $fvp->path_string();
	$should = "four:one:six:three";
	result( $did eq $should, "path_string() returns '$did'" );

	$did = $fvp->physical_path_string();
	$should = "/home/joe/aardvarkfour/one/six/three";
	result( $did eq $should, "physical_path_string() returns '$did'" );
}

######################################################################

message( "DONE TESTING File::VirtualPath" );

######################################################################

1;
