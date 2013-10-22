use Test::More 'no_plan';

package MyBase;
use Class::Std;
{
    my %name : ATTR( :name<name>    );
    my %rank : ATTR( name => 'rank' :set('RANK')   );
    my %snum : ATTR( :name<snum>    );

    sub verify :CUMULATIVE {
        my ($self) = @_;
        my $ident = ident $self;

        ::is $name{$ident}, 'MyBase::name'    => 'MyBase::name initialized';
        ::is $rank{$ident}, 'MyBase::rank'    => 'MyBase::rank initialized';
        ::is $snum{$ident}, 'MyBase::snum'    => 'MyBase::snum initialized';
    }
}

package Der;
use Class::Std;
use base qw( MyBase );
{
    my %name : ATTR( :name<name>                 );
    my %rank : ATTR( name => 'rank'              );
    my %snum : ATTR( :name('snum')  :get<sernum> );

    sub verify :CUMULATIVE {
        my ($self) = @_;
        my $ident = ident $self;

        ::is $name{$ident}, 'MyBase::name'   => 'Der::name initialized';
        ::is $rank{$ident}, 'generic rank'   => 'Der::rank initialized';
        ::is $snum{$ident}, 'Der::snum'      => 'Der::snum initialized';
    }
}

package main;

my $obj = MyBase->new({
    name => 'MyBase::name',
    snum => 'MyBase::snum',
    rank => 'generic rank',
    MyBase => {
        rank => 'MyBase::rank',
    }
});

$obj->verify();

ok eval { $obj->set_RANK('new rank'); 1; }      =>  'set_RANK defined';
ok !eval { $obj->set_rank('new rank'); 1; }     =>  'set_rank not defined';

my $derobj = Der->new({
    name => 'MyBase::name',
    snum => 'MyBase::snum',
    rank => 'generic rank',
    MyBase => {
        rank => 'MyBase::rank',
    },
    Der => {
        snum => 'Der::snum',
    },
});

$derobj->verify();

is $derobj->get_name(), 'MyBase::name'  => 'Der name read accessor';
is $derobj->get_rank(), 'generic rank'  => 'Der rank read accessor';
is $derobj->get_sernum(), 'Der::snum'     => 'Der rank read accessor';

$derobj->set_rank('new rank');
is $derobj->get_rank(), 'new rank'      => 'Der rank write accessor';

eval { $derobj->setname('new name') };
ok $@ =~ m/\ACan't locate object method "setname" via package "Der"/
                                        => 'Read only name attribute';

my $der2 = Der->new({
    name => 'MyBase::name',
    snum => 'MyBase::snum',
    rank => 'generic rank',
    MyBase => {
        rank => 'MyBase::rank',
    },
    Der => {
        snum => 0,
    },
});
is( $der2->get_sernum(), 0, 'false values allowable as attribute parameters' );

