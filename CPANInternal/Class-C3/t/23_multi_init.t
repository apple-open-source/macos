#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 2;

BEGIN {
    use_ok('Class::C3');
}

=pod

rt.cpan.org # 21558

If compile-time code from another module issues a [re]initialize() part-way
through the process of setting up own our modules, that shouldn't prevent
our own initialize() call from working properly.

=cut

{
    package TestMRO::A;
    use Class::C3;
    sub testmethod { 42 }

    package TestMRO::B;
    use base 'TestMRO::A';
    use Class::C3;

    package TestMRO::C;
    use base 'TestMRO::A';
    use Class::C3;
    sub testmethod { shift->next::method + 1 }

    package TestMRO::D;
    BEGIN { Class::C3::initialize }
    use base 'TestMRO::B';
    use base 'TestMRO::C';
    use Class::C3;
    sub new {
        my $class = shift;
        my $self = {};
        bless $self => $class;
    }
}

Class::C3::initialize;
is(TestMRO::D->new->testmethod, 43, 'double-initialize works ok');
