package MyBase;
use Test::More 'no_plan';

sub MODIFY_HASH_ATTRIBUTES {
    my ($package, $referent, @attrs) = @_;
    for my $attr (@attrs) {
        if ($attr =~ /Loud/) {
            $referent->{Loud} = 1;
        }
        undef $attr
    }
    return grep {defined} @attrs;
}

use Class::Std;
{
    my %name_of :ATTR( :name<name> ) :Loud;

    sub verify {
        my ($self) = @_;
        is $name_of{ident $self}, "mha_test"    => ':ATTR handled correctly';
        is $name_of{Loud}, 1                    => ':Loud handled correctly';
    }
}

package main;

my $obj = MyBase->new({name=>'mha_test'});

$obj->verify();

