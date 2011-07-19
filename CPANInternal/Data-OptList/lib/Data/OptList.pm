
package Data::OptList;
use strict;
use warnings;

use List::Util ();
use Params::Util ();
use Sub::Install 0.921 ();

=head1 NAME

Data::OptList - parse and validate simple name/value option pairs

=head1 VERSION

version 0.106

=cut

our $VERSION = '0.106';

=head1 SYNOPSIS

  use Data::OptList;

  my $options = Data::OptList::mkopt([
    qw(key1 key2 key3 key4),
    key5 => { ... },
    key6 => [ ... ],
    key7 => sub { ... },
    key8 => { ... },
    key8 => [ ... ],
  ]);

...is the same thing, more or less, as:

  my $options = [
    [ key1 => undef,        ],
    [ key2 => undef,        ],
    [ key3 => undef,        ],
    [ key4 => undef,        ],
    [ key5 => { ... },      ],
    [ key6 => [ ... ],      ],
    [ key7 => sub { ... },  ],
    [ key8 => { ... },      ],
    [ key8 => [ ... ],      ],
  ]);

=head1 DESCRIPTION

Hashes are great for storing named data, but if you want more than one entry
for a name, you have to use a list of pairs.  Even then, this is really boring
to write:

  $values = [
    foo => undef,
    bar => undef,
    baz => undef,
    xyz => { ... },
  ];

Just look at all those undefs!  Don't worry, we can get rid of those:

  $values = [
    map { $_ => undef } qw(foo bar baz),
    xyz => { ... },
  ];

Aaaauuugh!  We've saved a little typing, but now it requires thought to read,
and thinking is even worse than typing... and it's got a bug!  It looked right,
didn't it?  Well, the C<< xyz => { ... } >> gets consumed by the map, and we
don't get the data we wanted.

With Data::OptList, you can do this instead:

  $values = Data::OptList::mkopt([
    qw(foo bar baz),
    xyz => { ... },
  ]);

This works by assuming that any defined scalar is a name and any reference
following a name is its value.

=head1 FUNCTIONS

=head2 mkopt

  my $opt_list = Data::OptList::mkopt(
    $input,
    $moniker,
    $require_unique,
    $must_be,
  );

This produces an array of arrays; the inner arrays are name/value pairs.
Values will be either "undef" or a reference.

Valid values for C<$input>:

 undef    -> []
 hashref  -> [ [ key1 => value1 ] ... ] # non-ref values become undef
 arrayref -> every value followed by a ref becomes a pair: [ value => ref   ]
             every value followed by undef becomes a pair: [ value => undef ]
             otherwise, it becomes [ value => undef ] like so:
             [ "a", "b", [ 1, 2 ] ] -> [ [ a => undef ], [ b => [ 1, 2 ] ] ]

C<$moniker> is a name describing the data, which will be used in error
messages.

If C<$require_unique> is true, an error will be thrown if any name is given
more than once.

C<$must_be> is either a scalar or array of scalars; it defines what kind(s) of
refs may be values.  If an invalid value is found, an exception is thrown.  If
no value is passed for this argument, any reference is valid.  If C<$must_be>
specifies that values must be CODE, HASH, ARRAY, or SCALAR, then Params::Util
is used to check whether the given value can provide that interface.
Otherwise, it checks that the given value is an object of the kind.

In other words:

  [ qw(SCALAR HASH Object::Known) ]

Means:

  _SCALAR0($value) or _HASH($value) or _INSTANCE($value, 'Object::Known')

=cut

my %test_for;
BEGIN {
  %test_for = (
    CODE   => \&Params::Util::_CODELIKE,  ## no critic
    HASH   => \&Params::Util::_HASHLIKE,  ## no critic
    ARRAY  => \&Params::Util::_ARRAYLIKE, ## no critic
    SCALAR => \&Params::Util::_SCALAR0,   ## no critic
  );
}

sub __is_a {
  my ($got, $expected) = @_;

  return List::Util::first { __is_a($got, $_) } @$expected if ref $expected;

  return defined (
    exists($test_for{$expected})
    ? $test_for{$expected}->($got)
    : Params::Util::_INSTANCE($got, $expected) ## no critic
  );
}

sub mkopt {
  my ($opt_list, $moniker, $require_unique, $must_be) = @_;

  return [] unless $opt_list;

  $opt_list = [
    map { $_ => (ref $opt_list->{$_} ? $opt_list->{$_} : ()) } keys %$opt_list
  ] if ref $opt_list eq 'HASH';

  my @return;
  my %seen;

  for (my $i = 0; $i < @$opt_list; $i++) { ## no critic
    my $name = $opt_list->[$i];
    my $value;

    if ($require_unique) {
      Carp::croak "multiple definitions provided for $name" if $seen{$name}++;
    }

    if    ($i == $#$opt_list)             { $value = undef;            }
    elsif (not defined $opt_list->[$i+1]) { $value = undef; $i++       }
    elsif (ref $opt_list->[$i+1])         { $value = $opt_list->[++$i] }
    else                                  { $value = undef;            }

    if ($must_be and defined $value) {
      unless (__is_a($value, $must_be)) {
        my $ref = ref $value;
        Carp::croak "$ref-ref values are not valid in $moniker opt list";
      }
    }

    push @return, [ $name => $value ];
  }

  return \@return;
}

=head2 mkopt_hash

  my $opt_hash = Data::OptList::mkopt_hash($input, $moniker, $must_be);

Given valid C<L</mkopt>> input, this routine returns a reference to a hash.  It
will throw an exception if any name has more than one value.

=cut

sub mkopt_hash {
  my ($opt_list, $moniker, $must_be) = @_;
  return {} unless $opt_list;

  $opt_list = mkopt($opt_list, $moniker, 1, $must_be);
  my %hash = map { $_->[0] => $_->[1] } @$opt_list;
  return \%hash;
}

=head1 EXPORTS

Both C<mkopt> and C<mkopt_hash> may be exported on request.

=cut

BEGIN {
  *import = Sub::Install::exporter {
    exports => [qw(mkopt mkopt_hash)],
  };
}

=head1 AUTHOR

Ricardo SIGNES, C<< <rjbs@cpan.org> >>

=head1 BUGS

Please report any bugs or feature requests at L<http://rt.cpan.org>. I will be
notified, and then you'll automatically be notified of progress on your bug as
I make changes.

=head1 COPYRIGHT

Copyright 2006-2007, Ricardo SIGNES.  This program is free software;  you can
redistribute it and/or modify it under the same terms as Perl itself.

=cut

1;
