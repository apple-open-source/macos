package t::lib::Test;

# Support code for DBD::SQLite tests

use strict;
use Exporter   ();
use File::Spec ();
use Test::More ();

use vars qw{$VERSION @ISA @EXPORT @CALL_FUNCS};
BEGIN {
	$VERSION = '1.29';
	@ISA     = 'Exporter';
	@EXPORT  = qw/connect_ok dies @CALL_FUNCS/;

	# Allow tests to load modules bundled in /inc
	unshift @INC, 'inc';
}

# Always load the DBI module
use DBI ();

# Delete temporary files
sub clean {
	unlink( 'foo'         );
	unlink( 'foo-journal' );
}

# Clean up temporary test files both at the beginning and end of the
# test script.
BEGIN { clean() }
END   { clean() }

# A simplified connect function for the most common case
sub connect_ok {
	my $attr = { @_ };
	my $dbfile = delete $attr->{dbfile} || ':memory:';
	my @params = ( "dbi:SQLite:dbname=$dbfile", '', '' );
	if ( %$attr ) {
		push @params, $attr;
	}
	my $dbh = DBI->connect( @params );
	Test::More::isa_ok( $dbh, 'DBI::db' );
	return $dbh;
}

=head2 dies

  dies(sub {...}, $regex_expected_error, $msg)

Tests that the given coderef (most probably a closure) dies with the
expected error message.

=cut

sub dies {
	my ($coderef, $regex, $msg) = @_;
        eval {$coderef->()};
        my $exception = $@;
	Test::More::ok($exception =~ $regex, 
                       $msg || "dies with exception: $exception");
}



=head2 @CALL_FUNCS

The exported array C<@CALL_FUNCS> contains a list of coderefs
for testing several ways of calling driver-private methods.
On DBI versions prior to 1.608, such methods were called
through "func". Starting from 1.608, methods should be installed
within the driver (see L<DBI::DBD>) and are called through
C<< $dbh->sqlite_method_name(...) >>. This array helps to test
both ways. Usage :

  for my $call_func (@CALL_FUNCS) {
    my $dbh = connect_ok();
    ...
    $dbh->$call_func(@args, 'method_to_call');
    ...
  }

On DBI versions prior to 1.608, the loop will run only once
and the method call will be equivalent to 
C<< $dbh->func(@args, 'method_to_call') >>.
On more recent versions, the loop will run twice;
the second execution will call
C<< $dbh->sqlite_method_to_call(@args) >>.

The number of tests to plan should be adapted accordingly.
It can be computed like this :

  plan tests => $n_normal_tests * @CALL_FUNCS + 1;

The additional C< + 1> is required when using
L<Test::NoWarnings>, because that module adds 
a final test in an END block outside of the loop.

=cut


# old_style way ("func")
push @CALL_FUNCS, sub {
  my $dbh = shift;
  return $dbh->func(@_);
};

# new_style, using $dbh->sqlite_*(...) --- starting from DBI v1.608
$DBI::VERSION >= 1.608 and push @CALL_FUNCS, sub {
  my $dbh       = shift;
  my $func_name = pop;
  my $method    = "sqlite_" . $func_name;
  return $dbh->$method(@_);
};

1;
