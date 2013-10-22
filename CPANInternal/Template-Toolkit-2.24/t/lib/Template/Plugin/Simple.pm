package Template::Plugin::Simple;
use base 'Template::Plugin::Filter';

sub init {
    my $self = shift;
    $self->{ _DYNAMIC } = 1;
    my $name = $self->{ _CONFIG }->{ name } || 'simple';
    $self->install_filter($name);
    return $self;
}

sub filter {
    my ($self, $text, $args, $conf) = @_;
    return '**' . $text . '**';
}

1;
