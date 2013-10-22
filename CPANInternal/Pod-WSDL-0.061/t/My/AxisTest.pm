package My::AxisTest;

=begin WSDL

_IN in $string
_IN bla $My::Bar
_FAULT My::Foo
_RETURN $string
_DOC bla bla

=cut

sub test {}

=begin WSDL

_IN in @string
_ONEWAY

=cut

sub testOneway {}

sub testWithoutPod {}

1;
