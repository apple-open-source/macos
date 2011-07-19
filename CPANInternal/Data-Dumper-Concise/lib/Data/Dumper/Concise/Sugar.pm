package Data::Dumper::Concise::Sugar;

use 5.006;

use Exporter ();
use Data::Dumper::Concise ();

BEGIN { @ISA = qw(Exporter) }

@EXPORT = qw(Dwarn DwarnS);

sub Dwarn { warn Data::Dumper::Concise::Dumper @_; @_ }

sub DwarnS ($) { warn Data::Dumper::Concise::Dumper $_[0]; $_[0] }

=head1 NAME

Data::Dumper::Concise::Sugar - return Dwarn @return_value

=head1 SYNOPSIS

  use Data::Dumper::Concise::Sugar;

  return Dwarn some_call(...)

is equivalent to:

  use Data::Dumper::Concise;

  my @return = some_call(...);
  warn Dumper(@return);
  return @return;

but shorter. If you need to force scalar context on the value,

  use Data::Dumper::Concise::Sugar;

  return DwarnS some_call(...)

is equivalent to:

  use Data::Dumper::Concise;

  my $return = some_call(...);
  warn Dumper($return);
  return $return;

Another trick that is extremely useful when doing method chaining is the
following:

  my $foo = Bar->new;
  $foo->bar->baz->Data::Dumper::Concise::Sugar::DwarnS->biff;

which is the same as:

  my $foo = Bar->new;
  (DwarnS $foo->bar->baz)->biff;

=head1 DESCRIPTION

  use Data::Dumper::Concise::Sugar;

will import Dwarn and DwarnS into your namespace. Using L<Exporter>, so see
its docs for ways to make it do something else.

=head2 Dwarn

  sub Dwarn { warn Data::Dumper::Concise::Dumper @_; @_ }

=head3 DwarnS

  sub DwarnS ($) { warn Data::Dumper::Concise::Dumper $_[0]; $_[0] }

=head1 SEE ALSO

You probably want L<Devel::Dwarn>, it's the shorter name for this module.

=cut

1;
