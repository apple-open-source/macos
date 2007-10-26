package Foo;
use Class::Trigger;

sub new {
    bless {}, shift;
}

sub foo {
    my $self = shift;
    $self->call_trigger('before_foo');
    print "foo\n";
    $self->call_trigger('after_foo');
}

1;

