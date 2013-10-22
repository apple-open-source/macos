package MyBase;
use Class::Std;
{
    my %public_of  :ATTR( :init_arg<pub> );
    my %private_of :ATTR;

    sub BUILD {
        my ($self, $ident) = @_;

        $private_of{$ident} = 'base priv';
    }
}

package MyDer;
use base qw( MyBase );
use Class::Std;
{
    my %public_of  :ATTR( :init_arg<pub> );
    my %private_of :ATTR;

    sub BUILD {
        my ($self, $ident) = @_;

        $private_of{$ident} = 'der priv';
    }
}


package main;

my $rep = MyDer->new({
              MyBase => { pub => 'base pub' },
              MyDer  => { pub => 'der pub'  },
          })->_DUMP;

my $hash = eval $rep;

use Test::More 'no_plan';

ok !ref $rep                              => 'Representation is string';

ok $hash                                  => 'Representation is valid';

is $hash->{MyBase}{pub}, 'base pub'       => 'Public base attribute'; 
is $hash->{MyBase}{'????'}, 'base priv'   => 'Private base attribute'; 

is $hash->{MyDer}{pub}, 'der pub'         => 'Public derived attribute'; 
is $hash->{MyDer}{'????'}, 'der priv'     => 'Private derived attribute'; 

