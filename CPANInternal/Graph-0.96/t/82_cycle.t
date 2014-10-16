use Graph;
use Test::More tests => 1;

eval 'require Devel::Cycle';
SKIP: {
    skip("no Devel::Cycle", 1) if $@;
    import Devel::Cycle;
    my $g = Graph->new;
    $g->add_edge(qw(a b));
    $g->add_edge(qw(b c));
    $g->add_edge(qw(c d));
    $g->add_edge(qw(c e));
    $g->add_cycle(qw(e f g)); # This is not a true cycle if weakrefs work.
    my $out = tie *STDOUT, 'FakeOut';
    find_cycle($g);
    is($$out, undef);
}

package FakeOut;

sub TIEHANDLE {
    bless(\(my $text), $_[0]);
}

sub PRINT {
    my $self = shift;
    $$self .= join('', @_);
}

