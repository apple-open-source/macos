use Scalar::Util 'refaddr';
use Class::Std::Utils;
use Test::More 'no_plan';

my @objects = (
    do{\my $scalar},
    { hash => 'anon' },
    [ 1..10 ],
    sub {},
    qr//,
);

for my $ref (@objects) {
    is ident $ref, refaddr $ref   => 'ident acts like refaddr on '.ref $ref;
    is ident $ref, int $ref       => 'ident acts like int on '.ref $ref;
}

bless $_ for @objects;

for my $ref (@objects) {
    is ident $ref, refaddr $ref   => 'ident acts like refaddr on object';
}
