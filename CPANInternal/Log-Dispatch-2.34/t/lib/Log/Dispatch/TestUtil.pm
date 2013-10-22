package Log::Dispatch::TestUtil;
use Data::Dumper;
use strict;
use warnings;
use base qw(Exporter);

our @EXPORT_OK = qw(
    cmp_deeply
    dump_one_line
);

sub cmp_deeply {
    my ( $ref1, $ref2, $name ) = @_;

    my $tb = Test::Builder->new();
    $tb->is_eq( dump_one_line($ref1), dump_one_line($ref2), $name );
}

sub dump_one_line {
    my ($value) = @_;

    return Data::Dumper->new( [$value] )->Indent(0)->Sortkeys(1)->Quotekeys(0)
        ->Terse(1)->Dump();
}

1;

# ABSTRACT: Utilities used internally by Log::Dispatch for testing

__END__

=head1 METHODS

=over

=item cmp_deeply

A cheap version of Test::Deep::cmp_deeply.

=item dump_one_line

Dump a value to a single line using Data::Dumper.

=cut
