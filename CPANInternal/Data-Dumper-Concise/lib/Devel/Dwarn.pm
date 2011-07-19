package Devel::Dwarn;

use Data::Dumper::Concise::Sugar;

sub import {
  Data::Dumper::Concise::Sugar->export_to_level(1, @_);
}

=head1 NAME

Devel::Dwarn - return Dwarn @return_value

=head1 SYNOPSIS

  use Devel::Dwarn;

  return Dwarn some_call(...)

is equivalent to:

  use Data::Dumper::Concise;

  my @return = some_call(...);
  warn Dumper(@return);
  return @return;

but shorter. If you need to force scalar context on the value,

  use Devel::Dwarn;

  return DwarnS some_call(...)

is equivalent to:

  use Data::Dumper::Concise;

  my $return = some_call(...);
  warn Dumper($return);
  return $return;

Another trick that is extremely useful when doing method chaining is the
following:

  my $foo = Bar->new;
  $foo->bar->baz->Devel::Dwarn::DwarnS->biff;

which is the same as:

  my $foo = Bar->new;
  (DwarnS $foo->bar->baz)->biff;

=head1 SEE ALSO

This module is really just a shortcut for L<Data::Dumper::Concise::Sugar>, check
it out for more complete documentation.

=cut

1;
