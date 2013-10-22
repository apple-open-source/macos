package SQL::Abstract::Limit::Test;

# Lifted from DBIx::Class, originally was DBIC::SqlMakerTest.

use strict;
use warnings;

use base qw/Test::Builder::Module Exporter/;

use Exporter;

our @EXPORT = qw/
  &is_same_sql_bind
  &eq_sql
  &eq_bind
/;


{
  package DBIC::SqlMakerTest::SQLATest;

  # replacement for SQL::Abstract::Test if not available

  use strict;
  use warnings;

  use base qw/Test::Builder::Module Exporter/;

  use Scalar::Util qw(looks_like_number blessed reftype);
  use Data::Dumper;
  use Test::Builder;
  use Test::Deep qw(eq_deeply);

  our $tb = __PACKAGE__->builder;

  sub is_same_sql_bind
  {
    my ($sql1, $bind_ref1, $sql2, $bind_ref2, $msg) = @_;

    my $same_sql = eq_sql($sql1, $sql2);
    my $same_bind = eq_bind($bind_ref1, $bind_ref2);

    $tb->ok($same_sql && $same_bind, $msg);

    if (!$same_sql) {
      $tb->diag("SQL expressions differ\n"
        . "     got: $sql1\n"
        . "expected: $sql2\n"
      );
    }
    if (!$same_bind) {
      $tb->diag("BIND values differ\n"
        . "     got: " . Dumper($bind_ref1)
        . "expected: " . Dumper($bind_ref2)
      );
    }
  }

  sub eq_sql
  {
    my ($left, $right) = @_;

    $left =~ s/\s+//g;
    $right =~ s/\s+//g;

    return $left eq $right;
  }

  sub eq_bind
  {
    my ($bind_ref1, $bind_ref2) = @_;

    return eq_deeply($bind_ref1, $bind_ref2);
  }
}

eval "use SQL::Abstract::Test;";
if ($@ eq '') {
  # SQL::Abstract::Test available

  *is_same_sql_bind = \&SQL::Abstract::Test::is_same_sql_bind;
  *eq_sql = \&SQL::Abstract::Test::eq_sql;
  *eq_bind = \&SQL::Abstract::Test::eq_bind;
} else {
  # old SQL::Abstract

  *is_same_sql_bind = \&DBIC::SqlMakerTest::SQLATest::is_same_sql_bind;
  *eq_sql = \&DBIC::SqlMakerTest::SQLATest::eq_sql;
  *eq_bind = \&DBIC::SqlMakerTest::SQLATest::eq_bind;
}


1;

__END__


=head1 NAME

SQL::Abstract::Limit::Test - Helper package for testing generated SQL and bind values

=head1 SYNOPSIS

  use Test::More;
  use SQL::Abstract::Limit::Test;
  
  my ($sql, @bind) = $schema->storage->sql_maker->select(%args);
  is_same_sql_bind(
    $sql, \@bind, 
    $expected_sql, \@expected_bind,
    'foo bar works'
  );

=head1 DESCRIPTION

Exports functions that can be used to compare generated SQL and bind values.

If L<SQL::Abstract::Test> (packaged in L<SQL::Abstract> versions 1.50 and
above) is available, then it is used to perform the comparisons (all functions
are delegated to id). Otherwise uses simple string comparison for the SQL
statements and simple L<Data::Dumper>-like recursive stringification for
comparison of bind values.


=head1 FUNCTIONS

=head2 is_same_sql_bind

  is_same_sql_bind(
    $given_sql, \@given_bind, 
    $expected_sql, \@expected_bind,
    $test_msg
  );

Compares given and expected pairs of C<($sql, \@bind)>, and calls
L<Test::Builder/ok> on the result, with C<$test_msg> as message.

=head2 eq_sql

  my $is_same = eq_sql($given_sql, $expected_sql);

Compares the two SQL statements. Returns true IFF they are equivalent.

=head2 eq_bind

  my $is_same = eq_sql(\@given_bind, \@expected_bind);

Compares two lists of bind values. Returns true IFF their values are the same.


=head1 SEE ALSO

L<SQL::Abstract::Test>, L<Test::More>, L<Test::Builder>.

=head1 AUTHOR

Norbert Buchmuller, <norbi@nix.hu>

=head1 COPYRIGHT AND LICENSE

Copyright 2008 by Norbert Buchmuller.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 
