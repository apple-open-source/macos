package My::BindingTest;

=begin WSDL

_IN in $string
_FAULT My::Foo
_RETURN $string
_DOC bla bla

=cut

sub testGeneral {}

=begin WSDL

_IN in @string
_ONEWAY

=cut

sub testOneway {}

sub testWithoutPod {}

1;
