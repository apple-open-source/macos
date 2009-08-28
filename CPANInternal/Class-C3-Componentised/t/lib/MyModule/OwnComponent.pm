package # hide from pause
  MyModule::OwnComponent;

use Class::C3;

sub message {
  my $self = shift;

  return join(" ", "OwnComponent", $self->next::method);
}

1;
