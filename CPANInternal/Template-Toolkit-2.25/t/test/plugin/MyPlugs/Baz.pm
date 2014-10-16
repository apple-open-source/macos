package MyPlugs::Baz;

sub new {
    my ($class, $value) = @_;
    bless {
	VALUE => $value,
    }, $class;
}

sub output {
    my $self = shift;
    return "This is the Baz module, value is $self->{ VALUE }";
}

1;
