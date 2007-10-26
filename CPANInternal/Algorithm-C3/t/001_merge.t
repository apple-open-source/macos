#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 5;

BEGIN {
    use_ok('Algorithm::C3');          
}

{
    package My::A;
    package My::C;
    our @ISA = ('My::A');
    package My::B;
    our @ISA = ('My::A');    
    package My::D;       
    our @ISA = ('My::B', 'My::C');         
}

{
    my @merged = Algorithm::C3::merge(
        'My::D',
        sub {
            no strict 'refs';
            @{$_[0] . '::ISA'};
        }
    );
            
    is_deeply(
        \@merged,
        [ qw/My::D My::B My::C My::A/ ],
        '... merged the lists correctly');
}

{
    package My::E;
    
    sub supers {
        no strict 'refs';
        @{$_[0] . '::ISA'};
    }    
    
    package My::F;
    our @ISA = ('My::E');
    package My::G;
    our @ISA = ('My::E');    
    package My::H;       
    our @ISA = ('My::G', 'My::F');
    sub method_exists_only_in_H { @ISA }
}

{
    my @merged = Algorithm::C3::merge('My::H', 'supers');

    is_deeply(
        \@merged,
        [ qw/My::H My::G My::F My::E/ ],
        '... merged the lists correctly');    
}

eval {
    Algorithm::C3::merge(
        'My::H',
        'this_method_does_not_exist'
    );
};
ok($@, '... this died as we expected');

eval {
    Algorithm::C3::merge(
        'My::H',
        'method_exists_only_in_H'
    );
};
ok($@, '... this died as we expected');
