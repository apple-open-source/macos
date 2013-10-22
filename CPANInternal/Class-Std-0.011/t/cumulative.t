package Base1;
use Class::Std;
BEGIN{ @ISA = qw( Base2 ); }
{
    sub base_first :CUMULATIVE(BASE FIRST) { return __PACKAGE__ }
    sub der_first  :CUMULATIVE             { return __PACKAGE__ }
    sub shift_obj  :CUMULATIVE             { return shift       }
}

package Base2;
use Class::Std;
{
    sub base_first :CUMULATIVE(BASE FIRST) { return __PACKAGE__ }
    sub der_first  :CUMULATIVE             { return __PACKAGE__ }
    sub shift_obj  :CUMULATIVE             { return shift       }
}

package Base3;
use Class::Std;
use base qw( Base2 );
{
    sub base_first :CUMULATIVE(BASE FIRST) { return __PACKAGE__ }
    sub der_first  :CUMULATIVE             { return __PACKAGE__ }
    sub shift_obj  :CUMULATIVE             { return shift       }
}

package Base4;
use Class::Std;
{
    sub base_first                         { return __PACKAGE__ }
    sub der_first                          { return __PACKAGE__ }
    sub shift_obj                          { return shift       }
}

package Der1;
use Class::Std;
use base qw(Base1 Base2 Base3 Base4);
{
    sub base_first :CUMULATIVE(BASE FIRST) { return __PACKAGE__ }
    sub der_first  :CUMULATIVE             { return __PACKAGE__ }
    sub shift_obj  :CUMULATIVE             { return shift       }
}

package Der2;
use Class::Std;
use base qw(Base1 Base2 Base3 Base4);
{
    sub base_first :CUMULATIVE(BASE FIRST) { return __PACKAGE__ }
    sub der_first  :CUMULATIVE             { return __PACKAGE__ }
    sub shift_obj  :CUMULATIVE             { return shift       }
}

package Reder1;
use Class::Std;
use base qw(Der1 Der2);
{
    sub base_first :CUMULATIVE(BASE FIRST) { return __PACKAGE__ }
    sub der_first  :CUMULATIVE             { return __PACKAGE__ }
    sub shift_obj  :CUMULATIVE             { return shift       }
}

package main;

use Test::More tests => 62;

my $obj = Reder1->new();

my @up_order   = qw( Reder1 Der1 Der2 Base1 Base3 Base2 );
my @down_order = qw( Base2 Base1 Base3 Der1 Der2 Reder1 );

my $up_string   = join q{}, @up_order;
my $down_string = join q{}, @down_order;

my @objs = ($obj) x 6;

for my $test_run (1..2) {
    my $res_up   = $obj->der_first();
    my $res_down = $obj->base_first();
    my $res_objs = $obj->shift_obj();

    is int $res_up,   int @up_order           => 'Numeric cumulative up';
    is int $res_down, int @down_order         => 'Numeric cumulative down';

    is "$res_up",   $up_string                => 'String cumulative up';
    is "$res_down", $down_string              => 'String cumulative down';

    is_deeply \@$res_up,   \@up_order         => 'List cumulative up';
    is_deeply \@$res_down, \@down_order       => 'List cumulative down';

    for my $classname (keys %$res_up) {
        ok grep($classname, @up_order)        => "Valid up hash key ($classname)";
        is $classname, $res_up->{$classname}  => "Valid up hash value ($classname)";
    }

    for my $classname (keys %$res_down) {
        ok grep($classname, @down_order)      => "Valid down hash key ($classname)";
        is $classname, $res_up->{$classname}  => "Valid down hash value ($classname)";
    }

    is_deeply \@$res_objs, \@objs             => "shift(\@_) used in method";
}

