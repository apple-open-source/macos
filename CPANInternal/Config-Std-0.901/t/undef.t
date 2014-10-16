use Config::Std;
use Test::More 'no_plan';

my %data = (
    'FOO' => {
        'foo1'     => 'defined',
        'foo2'     =>  undef,
    },
);

local $SIG{__WARN__} = sub {
    ok 0 => "Bad warning: @_";
};

my $output;

ok !eval{ write_config %data => \$output }    => 'Write failed as expected';

like $@, qr/\ACan't save undefined value for key {'FOO'}{'foo2'}/
                                            => 'Failed with expected exception';
