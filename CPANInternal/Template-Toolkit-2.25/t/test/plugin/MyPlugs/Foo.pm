package MyPlugs::Foo;

use Template::Plugin;
use base qw( Template::Plugin );

sub new {
    my ($class, $context, $value) = @_;
    bless {
	VALUE => $value,
    }, $class;
}

sub output {
    my $self = shift;
    return "This is the Foo plugin, value is $self->{ VALUE }";
}

1;
