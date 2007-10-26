use strict;
use t::TestYAML tests => 1;
use YAML::Syck;
use Tie::Hash;

my($foo, $bar);
{
    my %h;
    my $rh = \%h;
    %h = (a=>1, b=>'2', c=>3.1415, d=>4);
    bless $rh => 'Tie::StdHash';
    $foo = Dump($rh);
}
{
    my %h;
    my $th = tie %h, 'Tie::StdHash';
    %h = (a=>1, b=>'2', c=>3.1415, d=>4);
    $bar = Dump($th);
}

is $foo, $bar;

