package DBIx::Class::TestSchemaComponent;

use strict;
use warnings;

our $test_component_ok = 0;

sub connection {
    my ($self, @info) = @_;

    $test_component_ok++;

    return $self->next::method(@info);
}

sub dbix_class_testschemacomponent { 'dbix_class_testschemacomponent works' }

1;
