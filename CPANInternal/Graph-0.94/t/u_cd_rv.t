use Graph;

use Test::More tests => 1;

package MyNode;
use overload ('""' => '_asstring', fallback=>1);
sub new {
    my ($class, %ops) = @_;
    return bless { %ops }, $class;
}
sub _asstring {
    my ($self) = @_;
    my $str = $self->{'name'};
    return $self->{'name'};
}
1;

package main;

my $gnoref = new Graph;
my $gwithref = new Graph(refvertexed_stringified=>1);
my $n1 = new MyNode('name'=>'alpha');
my $n2 = new MyNode('name'=>'beta');
$gnoref->add_edge($n1, $n2);

$gwithref->add_edge($n1, $n2);

is_deeply([sort keys %{$gnoref->[2]->[4]}],[sort keys %{$gwithref->[2]->[4]}]);
