package DBIC::SqlMakerTest;

use strict;
use warnings;

use base qw/Exporter/;

use Carp;
use SQL::Abstract::Test;

our @EXPORT = qw/
  is_same_sql_bind
  is_same_sql
  is_same_bind
/;
our @EXPORT_OK = qw/
  eq_sql
  eq_bind
  eq_sql_bind
/;

sub is_same_sql_bind {
  # unroll possible as_query arrayrefrefs
  my @args;

  for (1,2) {
    my $chunk = shift @_;

    if ( ref $chunk eq 'REF' and ref $$chunk eq 'ARRAY' ) {
      my ($sql, @bind) = @$$chunk;
      push @args, ($sql, \@bind);
    }
    else {
      push @args, $chunk, shift @_;
    }

  }

  push @args, shift @_;

  croak "Unexpected argument(s) supplied to is_same_sql_bind: " . join ('; ', @_)
    if @_;

  @_ = @args;
  goto &SQL::Abstract::Test::is_same_sql_bind;
}

*is_same_sql = \&SQL::Abstract::Test::is_same_sql;
*is_same_bind = \&SQL::Abstract::Test::is_same_bind;
*eq_sql = \&SQL::Abstract::Test::eq_sql;
*eq_bind = \&SQL::Abstract::Test::eq_bind;
*eq_sql_bind = \&SQL::Abstract::Test::eq_sql_bind;

1;

__END__


=head1 NAME

DBIC::SqlMakerTest - Helper package for testing sql_maker component of DBIC

=head1 SYNOPSIS

  use Test::More;
  use DBIC::SqlMakerTest;
  
  my ($sql, @bind) = $schema->storage->sql_maker->select(%args);
  is_same_sql_bind(
    $sql, \@bind, 
    $expected_sql, \@expected_bind,
    'foo bar works'
  );

=head1 DESCRIPTION

Exports functions that can be used to compare generated SQL and bind values.

This is a thin wrapper around L<SQL::Abstract::Test>, which makes it easier
to compare as_query sql/bind arrayrefrefs directly.

=head1 FUNCTIONS

=head2 is_same_sql_bind

  is_same_sql_bind(
    $given_sql, \@given_bind,
    $expected_sql, \@expected_bind,
    $test_msg
  );

  is_same_sql_bind(
    $rs->as_query
    $expected_sql, \@expected_bind,
    $test_msg
  );

  is_same_sql_bind(
    \[$given_sql, @given_bind],
    $expected_sql, \@expected_bind,
    $test_msg
  );

Compares given and expected pairs of C<($sql, \@bind)>, and calls
L<Test::Builder/ok> on the result, with C<$test_msg> as message.

=head2 is_same_sql

  is_same_sql(
    $given_sql,
    $expected_sql,
    $test_msg
  );

Compares given and expected SQL statement, and calls L<Test::Builder/ok> on the
result, with C<$test_msg> as message.

=head2 is_same_bind

  is_same_bind(
    \@given_bind, 
    \@expected_bind,
    $test_msg
  );

Compares given and expected bind value lists, and calls L<Test::Builder/ok> on
the result, with C<$test_msg> as message.

=head2 eq_sql

  my $is_same = eq_sql($given_sql, $expected_sql);

Compares the two SQL statements. Returns true IFF they are equivalent.

=head2 eq_bind

  my $is_same = eq_sql(\@given_bind, \@expected_bind);

Compares two lists of bind values. Returns true IFF their values are the same.

=head2 eq_sql_bind

  my $is_same = eq_sql_bind(
    $given_sql, \@given_bind,
    $expected_sql, \@expected_bind
  );

Compares the two SQL statements and the two lists of bind values. Returns true
IFF they are equivalent and the bind values are the same.


=head1 SEE ALSO

L<SQL::Abstract::Test>, L<Test::More>, L<Test::Builder>.

=head1 AUTHOR

Norbert Buchmuller, <norbi@nix.hu>

=head1 COPYRIGHT AND LICENSE

Copyright 2008 by Norbert Buchmuller.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.
