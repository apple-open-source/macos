package Foo::Valid;
use base qw(Foo);
use Class::Trigger qw(before_foo after_foo);

sub bar {
    my $self = shift;
    $self->call_trigger('invalid');
}

1;

