print "1..4\n";

package MyBase;
use Class::Std;
{
    my %attr : ATTR( :default(42) );

    sub BUILD {
        my ($self, $id) = @_;
        print 'not ' if defined $attr{$id};
        print "ok 1 - Default not available in BUILD\n";
    }

    sub START {
        my ($self, $id) = @_;
        print 'not ' if !defined $attr{$id};
        print "ok 3 - Default set before START\n";
    }
}

package Der;
use Class::Std;
use base qw( MyBase );
{
    my %attr : ATTR( :init_arg<attr> );

    sub BUILD {
        my ($self, $id) = @_;
        print 'not ' if defined $attr{$id};
        print "ok 2 - Init arg not available in BUILD\n";
    }

    sub START {
        my ($self, $id) = @_;
        print 'not ' if !defined $attr{$id} || $attr{$id} ne '86';
        print "ok 4 - Init arg set before START\n";
    }
}

package main;

Der->new({ attr=>86 });

