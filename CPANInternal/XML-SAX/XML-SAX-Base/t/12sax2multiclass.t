use Test;
BEGIN { plan tests => 33 }
use XML::SAX::Base;
use strict;
use vars qw/%events $meth_count/;
require "t/events.pl";

# Multiclass SAX1 filter

my $terminus         = SAXAutoload->new();
my $content_handler  = MyContentHandler->new(Handler => $terminus);
my $lexical_handler  = MyLexicalHandler->new(Handler => $terminus);
my $decl_handler     = MyDeclHandler->new(Handler    => $terminus);
my $error_handler    = MyErrorHandler->new(Handler   => $terminus);
my $entity_resolver  = MyEntityResolver->new(Handler => $terminus);
my $dtd_handler      = MyDTDHandler->new(Handler     => $terminus);

my $driver       = Driver->new(ContentHandler => $content_handler,
                               LexicalHandler => $lexical_handler,
                               DeclHandler    => $decl_handler,
                               ErrorHandler   => $error_handler,
                               EntityResolver => $entity_resolver,
                               DTDHandler     => $dtd_handler);

$driver->parse();

ok($meth_count == 32);

# end main

package MyContentHandler;
use base qw(XML::SAX::Base);
# this space intentionally blank

1;

package MyLexicalHandler;
use base qw(XML::SAX::Base);
# this space intentionally blank

1;

package MyDeclHandler;
use base qw(XML::SAX::Base);
# this space intentionally blank

1;

package MyErrorHandler;
use base qw(XML::SAX::Base);
# this space intentionally blank

1;

package MyEntityResolver;
use base qw(XML::SAX::Base);
# this space intentionally blank

1;

package MyDTDHandler;
use base qw(XML::SAX::Base);
# this space intentionally blank

1;

package Driver;
use base qw(XML::SAX::Base);

sub parse {
    my $self = shift;
    my %events = %main::events;
 
    $self->SUPER::start_document($events{start_document});
    $self->SUPER::processing_instruction($events{processing_instruction});
    $self->SUPER::set_document_locator($events{set_document_locator});
    $self->SUPER::start_prefix_mapping($events{start_prefix_mapping});
    $self->SUPER::start_element($events{start_element});
    $self->SUPER::characters($events{characters});
    $self->SUPER::ignorable_whitespace($events{ignorable_whitespace});
    $self->SUPER::skipped_entity($events{skipped_entity});
    $self->SUPER::end_element($events{end_element});
    $self->SUPER::end_prefix_mapping($events{end_prefix_mapping});
    $self->SUPER::end_document($events{end_document});
    $self->SUPER::notation_decl($events{notation_decl});
    $self->SUPER::unparsed_entity_decl($events{unparsed_entity_decl});
    $self->SUPER::xml_decl($events{xml_decl});
    $self->SUPER::attlist_decl($events{attlist_decl});
    $self->SUPER::doctype_decl($events{doctype_decl});
    $self->SUPER::entity_decl($events{entity_decl});
    $self->SUPER::comment($events{comment});
    $self->SUPER::start_dtd($events{start_dtd});
    $self->SUPER::end_dtd($events{end_dtd});
    $self->SUPER::start_cdata($events{start_cdata});
    $self->SUPER::end_cdata($events{end_cdata});
    $self->SUPER::start_entity($events{start_entity});
    $self->SUPER::end_entity($events{end_entity});
    $self->SUPER::element_decl($events{element_decl});
    $self->SUPER::attribute_decl($events{attribute_decl});
    $self->SUPER::internal_entity_decl($events{internal_entity_decl});
    $self->SUPER::external_entity_decl($events{external_entity_decl});
    $self->SUPER::warning($events{warning});
    $self->SUPER::error($events{error});
    $self->SUPER::fatal_error($events{fatal_error});
    $self->SUPER::resolve_entity($events{resolve_entity});
}
1;

# basic single class SAX Handler
package SAXAutoload;
use vars qw($AUTOLOAD);
use strict;

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my %options = @_;
    $options{methods} = {};
    return bless \%options, $class;
}

sub AUTOLOAD {
    my $self = shift;
    my $data = shift;
    my $name = $AUTOLOAD;
    $name =~ s/.*://;   # strip fully-qualified portion
    return if $name eq 'DESTROY';
    #warn "name is $name \ndata is $data\n";
    my $okay_count = 0;
    foreach my $key (keys (%{$data})) {
       $okay_count++ if defined $main::events{$name}->{$key};
    }
    #warn "count $okay_count \n";
    main::ok($okay_count == scalar (keys (%{$data})));
    $main::meth_count++;
}
1;
