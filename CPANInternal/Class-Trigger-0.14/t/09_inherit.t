use strict;
use Test::More tests => 3;

my @data;

package Foo;
use Class::Trigger qw(init);

sub init {
    my $class = shift;
    my $self = bless {},$class;
    $self->call_trigger('init');
    return $self;
}

__PACKAGE__->add_trigger(init => sub { push @data, 'foo' });

package Bar;
use base 'Foo';

__PACKAGE__->add_trigger(init => sub { push @data, 'bar' });

package Baz;
use base 'Bar';

__PACKAGE__->add_trigger(init => sub { push @data, 'baz' });

package main;

Foo->init;
is join(':', @data), 'foo';
@data = ();

Bar->init;
is join(':', @data), 'foo:bar';
@data = ();

Baz->init;
is join(':', @data), 'foo:bar:baz';
@data = ();
