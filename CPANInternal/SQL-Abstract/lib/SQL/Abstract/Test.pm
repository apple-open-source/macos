package SQL::Abstract::Test; # see doc at end of file

use strict;
use warnings;
use base qw/Test::Builder::Module Exporter/;
use Data::Dumper;
use Carp;
use Test::Builder;
use Test::Deep qw(eq_deeply);

our @EXPORT_OK = qw/&is_same_sql_bind &is_same_sql &is_same_bind
                    &eq_sql_bind &eq_sql &eq_bind 
                    $case_sensitive $sql_differ/;

our $case_sensitive = 0;
our $parenthesis_significant = 0;
our $sql_differ; # keeps track of differing portion between SQLs
our $tb = __PACKAGE__->builder;

# Parser states for _recurse_parse()
use constant PARSE_TOP_LEVEL => 0;
use constant PARSE_IN_EXPR => 1;
use constant PARSE_IN_PARENS => 2;
use constant PARSE_RHS => 3;

# These SQL keywords always signal end of the current expression (except inside
# of a parenthesized subexpression).
# Format: A list of strings that will be compiled to extended syntax (ie.
# /.../x) regexes, without capturing parentheses. They will be automatically
# anchored to word boundaries to match the whole token).
my @expression_terminator_sql_keywords = (
  'SELECT',
  'FROM',
  '(?:
    (?:
        (?: \b (?: LEFT | RIGHT | FULL ) \s+ )?
        (?: \b (?: CROSS | INNER | OUTER ) \s+ )?
    )?
    JOIN
  )',
  'ON',
  'WHERE',
  'EXISTS',
  'GROUP \s+ BY',
  'HAVING',
  'ORDER \s+ BY',
  'LIMIT',
  'OFFSET',
  'FOR',
  'UNION',
  'INTERSECT',
  'EXCEPT',
  'RETURNING',
);

# These are binary operator keywords always a single LHS and RHS
# * AND/OR are handled separately as they are N-ary
# * so is NOT as being unary
# * BETWEEN without paranthesis around the ANDed arguments (which
#   makes it a non-binary op) is detected and accomodated in 
#   _recurse_parse()
my $stuff_around_mathops = qr/[\w\s\`\'\"\)]/;
my @binary_op_keywords = (
  ( map
    {
      ' ^ '  . quotemeta ($_) . "(?= \$ | $stuff_around_mathops ) ",
      " (?<= $stuff_around_mathops)" . quotemeta ($_) . "(?= \$ | $stuff_around_mathops ) ",
    }
    (qw/< > != <> = <= >=/)
  ),
  ( map
    { '\b (?: NOT \s+)?' . $_ . '\b' }
    (qw/IN BETWEEN LIKE/)
  ),
);

my $tokenizer_re_str = join("\n\t|\n",
  ( map { '\b' . $_ . '\b' } @expression_terminator_sql_keywords, 'AND', 'OR', 'NOT'),
  @binary_op_keywords,
);

my $tokenizer_re = qr/ \s* ( $tokenizer_re_str | \( | \) | \? ) \s* /xi;

# All of these keywords allow their parameters to be specified with or without parenthesis without changing the semantics
my @unrollable_ops = (
  'ON',
  'WHERE',
  'GROUP \s+ BY',
  'HAVING',
  'ORDER \s+ BY',
);

sub is_same_sql_bind {
  my ($sql1, $bind_ref1, $sql2, $bind_ref2, $msg) = @_;

  # compare
  my $same_sql  = eq_sql($sql1, $sql2);
  my $same_bind = eq_bind($bind_ref1, $bind_ref2);

  # call Test::Builder::ok
  my $ret = $tb->ok($same_sql && $same_bind, $msg);

  # add debugging info
  if (!$same_sql) {
    _sql_differ_diag($sql1, $sql2);
  }
  if (!$same_bind) {
    _bind_differ_diag($bind_ref1, $bind_ref2);
  }

  # pass ok() result further
  return $ret;
}

sub is_same_sql {
  my ($sql1, $sql2, $msg) = @_;

  # compare
  my $same_sql  = eq_sql($sql1, $sql2);

  # call Test::Builder::ok
  my $ret = $tb->ok($same_sql, $msg);

  # add debugging info
  if (!$same_sql) {
    _sql_differ_diag($sql1, $sql2);
  }

  # pass ok() result further
  return $ret;
}

sub is_same_bind {
  my ($bind_ref1, $bind_ref2, $msg) = @_;

  # compare
  my $same_bind = eq_bind($bind_ref1, $bind_ref2);

  # call Test::Builder::ok
  my $ret = $tb->ok($same_bind, $msg);

  # add debugging info
  if (!$same_bind) {
    _bind_differ_diag($bind_ref1, $bind_ref2);
  }

  # pass ok() result further
  return $ret;
}

sub _sql_differ_diag {
  my ($sql1, $sql2) = @_;

  $tb->diag("SQL expressions differ\n"
      ."     got: $sql1\n"
      ."expected: $sql2\n"
      ."differing in :\n$sql_differ\n"
      );
}

sub _bind_differ_diag {
  my ($bind_ref1, $bind_ref2) = @_;

  $tb->diag("BIND values differ\n"
      ."     got: " . Dumper($bind_ref1)
      ."expected: " . Dumper($bind_ref2)
      );
}

sub eq_sql_bind {
  my ($sql1, $bind_ref1, $sql2, $bind_ref2) = @_;

  return eq_sql($sql1, $sql2) && eq_bind($bind_ref1, $bind_ref2);
}


sub eq_bind {
  my ($bind_ref1, $bind_ref2) = @_;

  return eq_deeply($bind_ref1, $bind_ref2);
}

sub eq_sql {
  my ($sql1, $sql2) = @_;

  # parse
  my $tree1 = parse($sql1);
  my $tree2 = parse($sql2);

  return 1 if _eq_sql($tree1, $tree2);
}

sub _eq_sql {
  my ($left, $right) = @_;

  # one is defined the other not
  if ( (defined $left) xor (defined $right) ) {
    return 0;
  }
  # one is undefined, then so is the other
  elsif (not defined $left) {
    return 1;
  }
  # one is a list, the other is an op with a list
  elsif (ref $left->[0] xor ref $right->[0]) {
    $sql_differ = sprintf ("left: %s\nright: %s\n", map { unparse ($_) } ($left, $right) );
    return 0;
  }
  # one is a list, so is the other
  elsif (ref $left->[0]) {
    for (my $i = 0; $i <= $#$left or $i <= $#$right; $i++ ) {
      return 0 if (not _eq_sql ($left->[$i], $right->[$i]) );
    }
    return 1;
  }
  # both are an op-list combo
  else {

    # unroll parenthesis if possible/allowed
    _parenthesis_unroll ($_) for ($left, $right);

    # if operators are different
    if ( $left->[0] ne $right->[0] ) {
      $sql_differ = sprintf "OP [$left->[0]] != [$right->[0]] in\nleft: %s\nright: %s\n",
        unparse($left),
        unparse($right);
      return 0;
    }
    # elsif operators are identical, compare operands
    else { 
      if ($left->[0] eq 'LITERAL' ) { # unary
        (my $l = " $left->[1][0] " ) =~ s/\s+/ /g;
        (my $r = " $right->[1][0] ") =~ s/\s+/ /g;
        my $eq = $case_sensitive ? $l eq $r : uc($l) eq uc($r);
        $sql_differ = "[$l] != [$r]\n" if not $eq;
        return $eq;
      }
      else {
        my $eq = _eq_sql($left->[1], $right->[1]);
        $sql_differ ||= sprintf ("left: %s\nright: %s\n", map { unparse ($_) } ($left, $right) ) if not $eq;
        return $eq;
      }
    }
  }
}

sub parse {
  my $s = shift;

  # tokenize string, and remove all optional whitespace
  my $tokens = [];
  foreach my $token (split $tokenizer_re, $s) {
    push @$tokens, $token if (length $token) && ($token =~ /\S/);
  }

  my $tree = _recurse_parse($tokens, PARSE_TOP_LEVEL);
  return $tree;
}

sub _recurse_parse {
  my ($tokens, $state) = @_;

  my $left;
  while (1) { # left-associative parsing

    my $lookahead = $tokens->[0];
    if ( not defined($lookahead)
          or
        ($state == PARSE_IN_PARENS && $lookahead eq ')')
          or
        ($state == PARSE_IN_EXPR && grep { $lookahead =~ /^ $_ $/xi } ('\)', @expression_terminator_sql_keywords ) )
          or
        ($state == PARSE_RHS && grep { $lookahead =~ /^ $_ $/xi } ('\)', @expression_terminator_sql_keywords, @binary_op_keywords, 'AND', 'OR', 'NOT' ) )
    ) {
      return $left;
    }

    my $token = shift @$tokens;

    # nested expression in ()
    if ($token eq '(' ) {
      my $right = _recurse_parse($tokens, PARSE_IN_PARENS);
      $token = shift @$tokens   or croak "missing closing ')' around block " . unparse ($right);
      $token eq ')'             or croak "unexpected token '$token' terminating block " . unparse ($right);
      $left = $left ? [@$left, [PAREN => [$right] ]]
                    : [PAREN  => [$right] ];
    }
    # AND/OR
    elsif ($token =~ /^ (?: OR | AND ) $/xi )  {
      my $op = uc $token;
      my $right = _recurse_parse($tokens, PARSE_IN_EXPR);

      # Merge chunks if logic matches
      if (ref $right and $op eq $right->[0]) {
        $left = [ (shift @$right ), [$left, map { @$_ } @$right] ];
      }
      else {
       $left = [$op => [$left, $right]];
      }
    }
    # binary operator keywords
    elsif (grep { $token =~ /^ $_ $/xi } @binary_op_keywords ) {
      my $op = uc $token;
      my $right = _recurse_parse($tokens, PARSE_RHS);

      # A between with a simple LITERAL for a 1st RHS argument needs a
      # rerun of the search to (hopefully) find the proper AND construct
      if ($op eq 'BETWEEN' and $right->[0] eq 'LITERAL') {
        unshift @$tokens, $right->[1][0];
        $right = _recurse_parse($tokens, PARSE_IN_EXPR);
      }

      $left = [$op => [$left, $right] ];
    }
    # expression terminator keywords (as they start a new expression)
    elsif (grep { $token =~ /^ $_ $/xi } @expression_terminator_sql_keywords ) {
      my $op = uc $token;
      my $right = _recurse_parse($tokens, PARSE_IN_EXPR);
      $left = $left ? [@$left,  [$op => [$right] ]]
                    : [[ $op => [$right] ]];
    }
    # NOT (last as to allow all other NOT X pieces first)
    elsif ( $token =~ /^ not $/ix ) {
      my $op = uc $token;
      my $right = _recurse_parse ($tokens, PARSE_RHS);
      $left = $left ? [ @$left, [$op => [$right] ]]
                    : [[ $op => [$right] ]];

    }
    # literal (eat everything on the right until RHS termination)
    else {
      my $right = _recurse_parse ($tokens, PARSE_RHS);
      $left = $left ? [$left, [LITERAL => [join ' ', $token, unparse($right)||()] ] ]
                    : [ LITERAL => [join ' ', $token, unparse($right)||()] ];
    }
  }
}

sub _parenthesis_unroll {
  my $ast = shift;

  return if $parenthesis_significant;
  return unless (ref $ast and ref $ast->[1]);

  my $changes;
  do {
    my @children;
    $changes = 0;

    for my $child (@{$ast->[1]}) {
      if (not ref $child or not $child->[0] eq 'PAREN') {
        push @children, $child;
        next;
      }

      # unroll nested parenthesis
      while ($child->[1][0][0] eq 'PAREN') {
        $child = $child->[1][0];
        $changes++;
      }

      # if the parenthesis are wrapped around an AND/OR matching the parent AND/OR - open the parenthesis up and merge the list
      if (
        ( $ast->[0] eq 'AND' or $ast->[0] eq 'OR')
            and
          $child->[1][0][0] eq $ast->[0]
      ) {
        push @children, @{$child->[1][0][1]};
        $changes++;
      }

      # if the parent operator explcitly allows it nuke the parenthesis
      elsif ( grep { $ast->[0] =~ /^ $_ $/xi } @unrollable_ops ) {
        push @children, $child->[1][0];
        $changes++;
      }

      # only one LITERAL element in the parenthesis
      elsif (
        @{$child->[1]} == 1 && $child->[1][0][0] eq 'LITERAL'
      ) {
        push @children, $child->[1][0];
        $changes++;
      }

      # only one element in the parenthesis which is a binary op with two LITERAL sub-children
      elsif (
        @{$child->[1]} == 1
          and
        grep { $child->[1][0][0] =~ /^ $_ $/xi } (@binary_op_keywords)
          and
        $child->[1][0][1][0][0] eq 'LITERAL'
          and
        $child->[1][0][1][1][0] eq 'LITERAL'
      ) {
        push @children, $child->[1][0];
        $changes++;
      }

      # otherwise no more mucking for this pass
      else {
        push @children, $child;
      }
    }

    $ast->[1] = \@children;

  } while ($changes);

}

sub unparse {
  my $tree = shift;

  if (not $tree ) {
    return '';
  }
  elsif (ref $tree->[0]) {
    return join (" ", map { unparse ($_) } @$tree);
  }
  elsif ($tree->[0] eq 'LITERAL') {
    return $tree->[1][0];
  }
  elsif ($tree->[0] eq 'PAREN') {
    return sprintf '(%s)', join (" ", map {unparse($_)} @{$tree->[1]});
  }
  elsif ($tree->[0] eq 'OR' or $tree->[0] eq 'AND' or (grep { $tree->[0] =~ /^ $_ $/xi } @binary_op_keywords ) ) {
    return join (" $tree->[0] ", map {unparse($_)} @{$tree->[1]});
  }
  else {
    return sprintf '%s %s', $tree->[0], unparse ($tree->[1]);
  }
}


1;


__END__

=head1 NAME

SQL::Abstract::Test - Helper function for testing SQL::Abstract

=head1 SYNOPSIS

  use SQL::Abstract;
  use Test::More;
  use SQL::Abstract::Test import => [qw/
    is_same_sql_bind is_same_sql is_same_bind
    eq_sql_bind eq_sql eq_bind
  /];

  my ($sql, @bind) = SQL::Abstract->new->select(%args);

  is_same_sql_bind($given_sql,    \@given_bind, 
                   $expected_sql, \@expected_bind, $test_msg);

  is_same_sql($given_sql, $expected_sql, $test_msg);
  is_same_bind(\@given_bind, \@expected_bind, $test_msg);

  my $is_same = eq_sql_bind($given_sql,    \@given_bind, 
                            $expected_sql, \@expected_bind);

  my $sql_same = eq_sql($given_sql, $expected_sql);
  my $bind_same = eq_bind(\@given_bind, \@expected_bind);

=head1 DESCRIPTION

This module is only intended for authors of tests on
L<SQL::Abstract|SQL::Abstract> and related modules;
it exports functions for comparing two SQL statements
and their bound values.

The SQL comparison is performed on I<abstract syntax>,
ignoring differences in spaces or in levels of parentheses.
Therefore the tests will pass as long as the semantics
is preserved, even if the surface syntax has changed.

B<Disclaimer> : the semantic equivalence handling is pretty limited.
A lot of effort goes into distinguishing significant from
non-significant parenthesis, including AND/OR operator associativity.
Currently this module does not support commutativity and more
intelligent transformations like Morgan laws, etc.

For a good overview of what this test framework is capable of refer 
to C<t/10test.t>

=head1 FUNCTIONS

=head2 is_same_sql_bind

  is_same_sql_bind($given_sql,    \@given_bind, 
                   $expected_sql, \@expected_bind, $test_msg);

Compares given and expected pairs of C<($sql, \@bind)>, and calls
L<Test::Builder/ok> on the result, with C<$test_msg> as message. If the test
fails, a detailed diagnostic is printed. For clients which use L<Test::More>,
this is the one of the three functions (L</is_same_sql_bind>, L</is_same_sql>,
L</is_same_bind>) that needs to be imported.

=head2 is_same_sql

  is_same_sql($given_sql, $expected_sql, $test_msg);

Compares given and expected SQL statements, and calls L<Test::Builder/ok> on
the result, with C<$test_msg> as message. If the test fails, a detailed
diagnostic is printed. For clients which use L<Test::More>, this is the one of
the three functions (L</is_same_sql_bind>, L</is_same_sql>, L</is_same_bind>)
that needs to be imported.

=head2 is_same_bind

  is_same_bind(\@given_bind, \@expected_bind, $test_msg);

Compares given and expected bind values, and calls L<Test::Builder/ok> on the
result, with C<$test_msg> as message. If the test fails, a detailed diagnostic
is printed. For clients which use L<Test::More>, this is the one of the three
functions (L</is_same_sql_bind>, L</is_same_sql>, L</is_same_bind>) that needs
to be imported.

=head2 eq_sql_bind

  my $is_same = eq_sql_bind($given_sql,    \@given_bind, 
                            $expected_sql, \@expected_bind);

Compares given and expected pairs of C<($sql, \@bind)>. Similar to
L</is_same_sql_bind>, but it just returns a boolean value and does not print
diagnostics or talk to L<Test::Builder>.

=head2 eq_sql

  my $is_same = eq_sql($given_sql, $expected_sql);

Compares the abstract syntax of two SQL statements. Similar to L</is_same_sql>,
but it just returns a boolean value and does not print diagnostics or talk to
L<Test::Builder>. If the result is false, the global variable L</$sql_differ>
will contain the SQL portion where a difference was encountered; this is useful
for printing diagnostics.

=head2 eq_bind

  my $is_same = eq_sql(\@given_bind, \@expected_bind);

Compares two lists of bind values, taking into account the fact that some of
the values may be arrayrefs (see L<SQL::Abstract/bindtype>). Similar to
L</is_same_bind>, but it just returns a boolean value and does not print
diagnostics or talk to L<Test::Builder>.

=head1 GLOBAL VARIABLES

=head2 $case_sensitive

If true, SQL comparisons will be case-sensitive. Default is false;

=head2 $parenthesis_significant

If true, SQL comparison will preserve and report difference in nested
parenthesis. Useful for testing the C<-nest> modifier. Defaults to false;

=head2 $sql_differ

When L</eq_sql> returns false, the global variable
C<$sql_differ> contains the SQL portion
where a difference was encountered.


=head1 SEE ALSO

L<SQL::Abstract>, L<Test::More>, L<Test::Builder>.

=head1 AUTHORS

Laurent Dami, E<lt>laurent.dami AT etat  geneve  chE<gt>

Norbert Buchmuller <norbi@nix.hu>

Peter Rabbitson <ribasushi@cpan.org>

=head1 COPYRIGHT AND LICENSE

Copyright 2008 by Laurent Dami.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 
