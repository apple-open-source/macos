package Data::Dumper::Concise;

use 5.006;

$VERSION = '1.200';

require Exporter;
require Data::Dumper;

BEGIN { @ISA = qw(Exporter) }

@EXPORT = qw(Dumper);

sub Dumper {
  my $dd = Data::Dumper->new([]);
  $dd->Terse(1)->Indent(1)->Useqq(1)->Deparse(1)->Quotekeys(0)->Sortkeys(1);
  return $dd unless @_;
  return $dd->Values([ @_ ])->Dump;
}

=head1 NAME

Data::Dumper::Concise - Less indentation and newlines plus sub deparsing

=head1 SYNOPSIS

  use Data::Dumper::Concise;

  warn Dumper($var);

is equivalent to:

  use Data::Dumper;
  {
    local $Data::Dumper::Terse = 1;
    local $Data::Dumper::Indent = 1;
    local $Data::Dumper::Useqq = 1;
    local $Data::Dumper::Deparse = 1;
    local $Data::Dumper::Quotekeys = 0;
    local $Data::Dumper::Sortkeys = 1;
    warn Dumper($var);
  }

whereas

  my $dd = Dumper;

is equivalent to:

  my $dd = Data::Dumper->new([])
                       ->Terse(1)
                       ->Indent(1)
                       ->Useqq(1)
                       ->Deparse(1)
                       ->Quotekeys(0)
                       ->Sortkeys(1);

So for the structure:

  { foo => "bar\nbaz", quux => sub { "fleem" } };

Data::Dumper::Concise will give you:

  {
    foo => "bar\nbaz",
    quux => sub {
        use warnings;
        use strict 'refs';
        'fleem';
    }
  }

instead of the default Data::Dumper output:

  $VAR1 = {
  	'quux' => sub { "DUMMY" },
  	'foo' => 'bar
  baz'
  };

(note the tab indentation, oh joy ...)

=head1 DESCRIPTION

This module always exports a single function, Dumper, which can be called
with an array of values to dump those values or with no arguments to
return the Data::Dumper object it's created. Note that this means that

  Dumper @list

will probably not do what you wanted when @list is empty. In this case use

  Dumper \@list

instead.

It exists, fundamentally, as a convenient way to reproduce a set of Dumper
options that we've found ourselves using across large numbers of applications,
primarily for debugging output.

The principle guiding theme is "all the concision you can get while still
having a useful dump and not doing anything cleverer than setting Data::Dumper
options" - it's been pointed out to us that Data::Dump::Streamer can produce
shorter output with less lines of code. We know. This is simpler and we've
never seen it segfault. But for complex/weird structures, it generally rocks.
You should use it as well, when Concise is underkill. We do.

Why is deparsing on when the aim is concision? Because you often want to know
what subroutine refs you have when debugging and because if you were planning
to eval this back in you probably wanted to remove subrefs first and add them
back in a custom way anyway. Note that this -does- force using the pure perl
Dumper rather than the XS one, but I've never in my life seen Data::Dumper
show up in a profile so "who cares?".

=head1 BUT BUT BUT ...

Yes, we know. Consider this module in the ::Tiny spirit and feel free to
write a Data::Dumper::Concise::ButWithExtraTwiddlyBits if it makes you
happy. Then tell us so we can add it to the see also section.

=head1 SUGARY SYNTAX

This package also provides:

L<Data::Dumper::Concise::Sugar> - provides Dwarn and DwarnS convenience functions

L<Devel::Dwarn> - shorter form for Data::Dumper::Concise::Sugar

=head1 SEE ALSO

We use for some purposes, and dearly love, the following alternatives:

L<Data::Dump> - prettiness oriented but not amazingly configurable

L<Data::Dump::Streamer> - brilliant. beautiful. insane. extensive. excessive. try it.

L<JSON::XS> - no, really. If it's just plain data, JSON is a great option.

=head1 AUTHOR

mst - Matt S. Trout <mst@shadowcat.co.uk>

=head1 CONTRIBUTORS

frew - Arthur Axel "fREW" Schmidt <frioux@gmail.com>

=head1 COPYRIGHT

Copyright (c) 2009 the Data::Dumper::Concise L</AUTHOR> and L</CONTRIBUTORS>
as listed above.

=head1 LICENSE

This library is free software and may be distributed under the same terms
as perl itself.

=cut

1;
