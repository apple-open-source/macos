package Heap::Elem::NumRev;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
use Heap::Elem;

require Exporter;

@ISA = qw(Exporter Heap::Elem);

# No names exported.
@EXPORT = ( );

# Available for export: NumRElem (to allocate a new Heap::Elem::NumRev value)
@EXPORT_OK = qw( NumRElem );

$VERSION = '0.80';

sub NumRElem {	# exportable synonym for new
    Heap::Elem::NumRev->new(@_);
}

# compare two NumR elems (reverse order)
sub cmp {
    return $_[1][0] <=> $_[0][0];
}

1;
__END__

=head1 NAME

Heap::Elem::NumRev - Reversed Numeric Heap Elements

=head1 SYNOPSIS

  use Heap::Elem::NumRev( NumRElem );
  use Heap::Fibonacci;

  my $heap = Heap::Fibonacci->new;
  my $elem;

  foreach $i ( 1..100 ) {
      $elem = NumRElem( $i );
      $heap->add( $elem );
  }

  while( defined( $elem = $heap->extract_top ) ) {
      print "Largest is ", $elem->val, "\n";
  }

=head1 DESCRIPTION

Heap::Elem::NumRev is used to wrap numeric values into an element
that can be managed on a heap.  The top of the heap will have
the largest element still remaining.  (See L<Heap::Elem::Num>
if you want the heap to always return the smallest element.)

The details of the Elem interface are described in L<Heap::Elem>.

The details of using a Heap interface are described in L<Heap>.

=head1 AUTHOR

John Macdonald, john@perlwolf.com

=head1 COPYRIGHT

Copyright 1998-2007, O'Reilly & Associates.

This code is distributed under the same copyright terms as perl itself.

=head1 SEE ALSO

Heap(3), Heap::Elem(3), Heap::Elem::Num(3).

=cut
