use Test::More tests => 45;

eval q{
    package MyBase;
    use Class::Std;
    {
        my %name : ATTR( :init_arg<name>    :get<name>             );
        my %rank : ATTR( init_arg => 'rank' :get<rank> :set<rank>  );
        my %snum : ATTR( :init_arg('snum')  :get<snum>             );
        my %priv : ATTR;
        my %def  : ATTR( :default<MyBase::def> :get<default>       );
        my %dval : ATTR( :default('dval') :get<dval>               );

        sub BUILD {
            my ($self, $ident, $arg_ref) = @_;

            ::is ref $arg_ref, 'HASH'  => 'Args passed to MyBase::BUILD in hash-ref';
            ::is ident $self, $ident   => 'Identity correct in MyBase::BUILD';

            $priv{$ident} = $arg_ref->{priv};
            ::is $priv{$ident}, 'MyBase::priv'  => 'MyBase priv arg unpacked correctly';

            $snum{$ident} = $arg_ref->{snum} . '!';
            ::is $snum{$ident}, 'MyBase::snum!'  => 'MyBase snum arg unpacked correctly';
        }

        sub DEMOLISH {
            my ($self, $ident) = @_;

            ::is ident $self, $ident   => 'Identity correct in MyBase::DEMOLISH'
        }

        sub verify :CUMULATIVE {
            my ($self) = @_;
            my $ident = ident $self;

            ::is $name{$ident}, 'MyBase::name'    => 'MyBase::name initialized';
            ::is $rank{$ident}, 'MyBase::rank'    => 'MyBase::rank initialized';
            ::is $snum{$ident}, 'MyBase::snum!'   => 'MyBase::snum initialized';
            ::is $priv{$ident}, 'MyBase::priv'    => 'MyBase::name initialized';
            ::is $def{$ident},  'MyBase::def'     => 'MyBase::def initialized';
        }

        sub rest : RESTRICTED {
            ::ok 1, 'Accessed restricted';
        }
    }

    package Der;
    use Class::Std;
    use base qw( MyBase );
    {
        my %name : ATTR( :init_arg<name>                );
        my %rank : ATTR( init_arg => 'rank'             );
        my %snum : ATTR( :init_arg('snum')  :get<snum> );
        my %priv : ATTR( :init_arg<priv>    :get<priv> );
        my %def  : ATTR( :init_arg<def> :default<default def> :get<default>  );

        sub BUILD {
            my ($self, $ident, $arg_ref) = @_;

            ::is ref $arg_ref, 'HASH'  => 'Args passed to Der::BUILD in hash-ref';
            ::is ident $self, $ident   => 'Identity correct in Der::BUILD';
        }

        sub DEMOLISH {
            my ($self, $ident) = @_;

            ::is ident $self, $ident   => 'Identity correct in Der::DEMOLISH'
        }

        sub verify :CUMULATIVE {
            my ($self) = @_;
            my $ident = ident $self;

            ::is $name{$ident}, 'MyBase::name'   => 'Der::name initialized';
            ::is $rank{$ident}, 'generic rank'   => 'Der::rank initialized';
            ::is $snum{$ident}, 'Der::snum'      => 'Der::snum initialized';
            ::is $priv{$ident}, 'Der::priv'      => 'Der::name initialized';
            ::is $def{$ident},  'Der::def'       => 'Der::def initialized';

            $self->rest();
        }
    }
};

package main;

my $obj = MyBase->new({
    name => 'MyBase::name',
    snum => 'MyBase::snum',
    rank => 'generic rank',
    priv => 'generic priv',
    MyBase => {
        rank => 'MyBase::rank',
        priv => 'MyBase::priv',
    }
});

$obj->verify();

my $derobj = Der->new({
    name => 'MyBase::name',
    snum => 'MyBase::snum',
    rank => 'generic rank',
    priv => 'generic priv',
    MyBase => {
        rank => 'MyBase::rank',
        priv => 'MyBase::priv',
    },
    Der => {
        snum => 'Der::snum',
        priv => 'Der::priv',
        def  => 'Der::def',
    },
});

$derobj->verify();

is $derobj->get_name(), 'MyBase::name'  => 'Der name read accessor';
is $derobj->get_rank(), 'MyBase::rank'  => 'Der rank read accessor';
is $derobj->get_snum(), 'Der::snum'     => 'Der rank read accessor';
is $derobj->get_priv(), 'Der::priv'     => 'Der priv read accessor';

$derobj->set_rank('new rank');
is $derobj->get_rank(), 'new rank'      => 'Der rank write accessor';

eval { $derobj->setname('new name') };
ok $@ =~ m/\ACan't locate object method "setname" via package "Der"/
                                        => 'Read only name attribute';

my $der2 = Der->new({
    name => 'MyBase::name',
    snum => 'MyBase::snum',
    rank => 'generic rank',
    priv => 'generic priv',
    MyBase => {
        rank => 'MyBase::rank',
        priv => 'MyBase::priv',
    },
    Der => {
        snum => 0,
        priv => 'Der::priv',
    },
});
is( $der2->get_snum(), 0, 'false values allowable as attribute parameters' );

is( $der2->get_dval, 'dval', 'default values evaled correctly' );

