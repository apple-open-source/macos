use Test::More 'no_plan';

package MyBase;
use Class::Std;
{
    sub everyone             { return 'everyone' }
    sub family   :RESTRICTED { return 'family'   }
    sub personal :PRIVATE    { return 'personal' }

    sub try_all {
        $self = shift;
        for my $method (qw(everyone family personal)) {
            ::is $self->$method(), $method => "Called $method";
        }
    }
}

package MyDer;
use Class::Std;
use base qw( MyBase );
{
    sub everyone             { my $self = shift; $self->SUPER::everyone(); }
    sub family   :RESTRICTED { my $self = shift; $self->SUPER::family(); }
    sub personal :PRIVATE    { my $self = shift; $self->SUPER::personal(); }
}

package main;

my $base_obj = MyBase->new();
my $der_obj  = MyDer->new();

$base_obj->try_all();

ok !eval { $der_obj->try_all(); 1 }   => 'Derived call failed';
like $@, qr/Can't call private method MyDer::personal\(\) from class MyBase/
                                      => '...with correct error message';


is $base_obj->everyone, 'everyone' => 'External everyone succeeded';
ok !eval { $base_obj->family }     => 'External family failed as expected';
like $@, qr/Can't call restricted method MyBase::family\(\) from class main/
                                      => '...with correct error message';

ok !eval { $base_obj->personal }   => 'External personal failed as expected';

like $@, qr/Can't call private method MyBase::personal\(\) from class main/
                                      => '...with correct error message';

is $der_obj->everyone, 'everyone' => 'External derived everyone succeeded';
ok !eval { $der_obj->family }     => 'External derived family failed as expected';
like $@, qr/Can't call restricted method MyDer::family\(\) from class main/
                                      => '...with correct error message';

ok !eval { $der_obj->personal }   => 'External derived personal failed as expected';

like $@, qr/Can't call private method MyDer::personal\(\) from class main/
                                      => '...with correct error message';

