use strict;
use t::TestYAML tests => 1;
use YAML::Syck;
use Data::Dumper;

my $bref = bless \eval{my $scalar = 'YAML::Syck' }, 'foo';
my $bref2bref = bless \$bref, 'bar';

my $dd =  Dumper $bref2bref;
my $edd;
{
    no strict 'vars';
    $edd = eval $dd;
}

my $x = 42;
bless \$x, "numifyffoo";

is Dumper(Load(Dump($bref2bref))),   $dd, 'YAML::Syck'
