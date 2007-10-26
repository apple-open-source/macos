use Test;
BEGIN { plan tests => 1 }
use XML::SAX::Base;

my $sax_it = SAXIterator->new();
my $driver = Driver->new(Handler => $sax_it);

my $retval  = $driver->parse();

ok ($retval == 32);
# end main

package Driver;
use base qw(XML::SAX::Base);

sub parse {
    my $self = shift;

    $self->SUPER::start_document;
    $self->SUPER::start_element;
    $self->SUPER::characters;
    $self->SUPER::processing_instruction;
    $self->SUPER::end_prefix_mapping;
    $self->SUPER::start_prefix_mapping;
    $self->SUPER::set_document_locator;
    $self->SUPER::xml_decl;
    $self->SUPER::ignorable_whitespace;
    $self->SUPER::skipped_entity;
    $self->SUPER::start_cdata;
    $self->SUPER::end_cdata;
    $self->SUPER::comment;
    $self->SUPER::entity_reference;
    $self->SUPER::unparsed_entity_decl;
    $self->SUPER::element_decl;
    $self->SUPER::attlist_decl;
    $self->SUPER::doctype_decl;
    $self->SUPER::entity_decl;
    $self->SUPER::attribute_decl;
    $self->SUPER::internal_entity_decl;
    $self->SUPER::external_entity_decl;
    $self->SUPER::resolve_entity;
    $self->SUPER::start_dtd;
    $self->SUPER::end_dtd;
    $self->SUPER::start_entity; 
    $self->SUPER::end_entity; 
    $self->SUPER::warning;
    $self->SUPER::error;
    $self->SUPER::fatal_error;
    $self->SUPER::end_element;
    return $self->SUPER::end_document;
}
1;

# basic single class SAX Handler
package SAXIterator;
use strict;

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my %options = @_;
    $options{_cnt} = 0;
    return bless \%options, $class;
}

sub start_document {
    my ($self, $document) = @_;
    $self->{_cnt}++;
}

sub start_element {
    my ($self, $element) = @_;
    $self->{_cnt}++;
}

sub characters {
    my ($self, $chars) = @_;
    $self->{_cnt}++;
}

sub end_element {
    my ($self, $element) = @_;
    $self->{_cnt}++;
}

sub end_document {
    my ($self, $document) = @_;
    $self->{_cnt}++;
    return $self->{_cnt};
}

sub processing_instruction {
    my ($self, $pi) = @_;
    $self->{_cnt}++;
}

sub end_prefix_mapping {
    my ($self, $mapping) = @_;
    $self->{_cnt}++;
}

sub start_prefix_mapping {
    my ($self, $mapping) = @_;
    $self->{_cnt}++;
}

sub set_document_locator {
    my ($self, $mapping) = @_;
    $self->{_cnt}++;
}


sub xml_decl {
    my ($self, $mapping) = @_;
    $self->{_cnt}++;
}

sub ignorable_whitespace {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub skipped_entity {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub start_cdata {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub end_cdata {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub comment {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub entity_reference {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub unparsed_entity_decl {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub element_decl {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub attlist_decl {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub doctype_decl {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub entity_decl {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub attribute_decl {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub internal_entity_decl {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub external_entity_decl {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub resolve_entity {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub start_dtd {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub end_dtd {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub start_entity {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub end_entity {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub warning {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub error {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

sub fatal_error {
    my ($self, $wtf) = @_;
    $self->{_cnt}++;
}

1;

