#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite;

# example of custom serialization/deserialization

BEGIN {
  use XML::DOM;

  # deserializer has to specify new() and deserialize() methods
  package My::Deserializer;
  sub new { bless { _parser => XML::DOM::Parser->new } => ref($_[0]) || $_[0] }
  sub deserialize { shift->{_parser}->parse(shift) }

  # serializer is inherited from SOAP::Serializer
  package My::Serializer; @My::Serializer::ISA = 'SOAP::Serializer';

  # nothing special here. as_OBJECT_TYPE() method will catch serialization 
  # of the specified type (use '__' instead of '::'), so object of this 
  # type will be properly serializer even being inside complex data structures
  sub as_XML__DOM__Document { 
    my $self = shift;
    my($value, $name, $type, $attr) = @_;

    return [
      $name || $self->SUPER::gen_name(),                   # name
      {%$attr, 'xsi:type' => $self->maptypetouri($type)},  # attributes (optional)
      $value->toString,                                    # value
      $self->gen_id($value),                               # multiref id (optional)
    ];
  }
}

print "Deserialize to XML::DOM\n";
my $dom = My::Deserializer->new->deserialize('<a>1</a>');

print ref $dom, ': ', $dom->toString, "\n";

# serialize SOAP message using XML::DOM value
my $a = My::Serializer->maptype({'XML::DOM::Document' => 'http://my.something/'})
                      ->freeform(SOAP::Data->name('a' => [1, $dom, 2]));
print "Serialize array with @{[ref $dom]} element\n";
print $a, "\n";

print "Deserialize with default deserializer\n";
my $r = SOAP::Deserializer->deserialize($a)->freeform;

use Data::Dumper; $Data::Dumper::Terse = 1; $Data::Dumper::Indent = 1;
print Dumper($r);
