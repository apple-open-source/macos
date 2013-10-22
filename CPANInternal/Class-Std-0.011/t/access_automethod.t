use Test::More 'no_plan';

package Object::POOF::DB;

use warnings;
use strict;
use Carp qw(cluck);
use Class::Std;

# Module implementation here
{
    my %dbname_of :ATTR;

    sub BUILD {
        my ($self, $ident, $arg_ref) = @_;
        
        # set optional constructor values:
        if ($arg_ref->{dbname}) { 
            $self->set_dbname( $arg_ref->{dbname} );
        }
    }

    sub AUTOMETHOD : RESTRICTED {
        my ($self, $ident, $value) = @_;

        my $subname = $_;  # subname passed via $_

        # return failure if not get_something or set_something
        my ($mode, $name) = $subname =~ m/\A ([gs]et)_(.*) \z/xms
            or return;

        # if get, return a sub that gives the value
        if ($mode eq 'get') {
            if ($name eq 'dbname') { return sub { return $dbname_of{$ident} } }
        }
        # if set, return a sub that sets the value
        elsif ($mode eq 'set') {
            if ($name eq 'dbname') { return sub { $dbname_of{$ident} = $value }}
        }
        return;  # for posterity
    }
}


package TestApp::DB;
use Class::Std;
use base qw( Object::POOF::DB );
{ 
    sub BUILD {
        my ($self, $ident, $arg_ref) = @_;

        # set defaults if not set in constructor
        $self->get_dbname  or $self->set_dbname('test');
    }

    sub verify {
        my ($self) = @_;
        ::is $self->get_dbname(), 'test'    => 'Restricted AUTOMETHOD ok';
    }
}

package main;

my $obj = TestApp::DB->new();

$obj->verify();

ok !eval { $obj->get_dbname() }    => 'Restricted AUTOMETHOD inaccessible';
like $@, qr/Can't call restricted/ => 'Restricted AUTOMETHOD error msg';



