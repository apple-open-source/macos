# $Id: LibXML.pm,v 1.1.1.1 2004/05/20 17:55:25 jpetri Exp $

package XML::LibXML;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS
            $skipDTD $skipXMLDeclaration $setTagCompression
            $MatchCB $ReadCB $OpenCB $CloseCB );
use Carp;

use XML::LibXML::Common qw(:encoding :libxml);

use XML::LibXML::NodeList;
use IO::Handle; # for FH reads called as methods


$VERSION = "1.58";
require Exporter;
require DynaLoader;

@ISA = qw(DynaLoader Exporter);

#-------------------------------------------------------------------------#
# export information                                                      #
#-------------------------------------------------------------------------#
%EXPORT_TAGS = (
                all => [qw(
                           XML_ELEMENT_NODE
                           XML_ATTRIBUTE_NODE
                           XML_TEXT_NODE
                           XML_CDATA_SECTION_NODE
                           XML_ENTITY_REF_NODE
                           XML_ENTITY_NODE
                           XML_PI_NODE
                           XML_COMMENT_NODE
                           XML_DOCUMENT_NODE
                           XML_DOCUMENT_TYPE_NODE
                           XML_DOCUMENT_FRAG_NODE
                           XML_NOTATION_NODE
                           XML_HTML_DOCUMENT_NODE
                           XML_DTD_NODE
                           XML_ELEMENT_DECL
                           XML_ATTRIBUTE_DECL
                           XML_ENTITY_DECL
                           XML_NAMESPACE_DECL
                           XML_XINCLUDE_END
                           XML_XINCLUDE_START
                           encodeToUTF8
                           decodeFromUTF8
                          )],
                libxml => [qw(
                           XML_ELEMENT_NODE
                           XML_ATTRIBUTE_NODE
                           XML_TEXT_NODE
                           XML_CDATA_SECTION_NODE
                           XML_ENTITY_REF_NODE
                           XML_ENTITY_NODE
                           XML_PI_NODE
                           XML_COMMENT_NODE
                           XML_DOCUMENT_NODE
                           XML_DOCUMENT_TYPE_NODE
                           XML_DOCUMENT_FRAG_NODE
                           XML_NOTATION_NODE
                           XML_HTML_DOCUMENT_NODE
                           XML_DTD_NODE
                           XML_ELEMENT_DECL
                           XML_ATTRIBUTE_DECL
                           XML_ENTITY_DECL
                           XML_NAMESPACE_DECL
                           XML_XINCLUDE_END
                           XML_XINCLUDE_START
                          )],
                encoding => [qw(
                                encodeToUTF8
                                decodeFromUTF8
                               )],
               );

@EXPORT_OK = (
              @{$EXPORT_TAGS{all}},
             );

@EXPORT = (
           @{$EXPORT_TAGS{all}},
          );

#-------------------------------------------------------------------------#
# initialization of the global variables                                  #
#-------------------------------------------------------------------------#
$skipDTD            = 0;
$skipXMLDeclaration = 0;
$setTagCompression  = 0;

$MatchCB = undef;
$ReadCB  = undef;
$OpenCB  = undef;
$CloseCB = undef;

#-------------------------------------------------------------------------#
# bootstrapping                                                           #
#-------------------------------------------------------------------------#
bootstrap XML::LibXML $VERSION;

#-------------------------------------------------------------------------#
# parser constructor                                                      #
#-------------------------------------------------------------------------#
sub new {
    my $class = shift;
    my %options = @_;
    if ( not exists $options{XML_LIBXML_KEEP_BLANKS} ) {
        $options{XML_LIBXML_KEEP_BLANKS} = 1;
    }

    if ( defined $options{catalog} ) {
        $class->load_catalog( $options{catalog} );
        delete $options{catalog};
    }

    my $self = bless \%options, $class;
    if ( defined $options{Handler} ) {
        $self->set_handler( $options{Handler} );
    }

    return $self;
}

#-------------------------------------------------------------------------#
# DOM Level 2 document constructor                                        #
#-------------------------------------------------------------------------#

sub createDocument {
   my $self = shift;
   if (!@_ or $_[0] =~ m/^\d\.\d$/) {
     # for backward compatibility
     return XML::LibXML::Document->new(@_);
   }
   else {
     # DOM API: createDocument(namespaceURI, qualifiedName, doctype?)
     my $doc = XML::LibXML::Document-> new;
     my $el = $doc->createElementNS(shift, shift);
     $doc->setDocumentElement($el);
     $doc->setExternalSubset(shift) if @_;
     return $doc;
   }
}

#-------------------------------------------------------------------------#
# callback functions                                                      #
#-------------------------------------------------------------------------#
sub match_callback {
    my $self = shift;
    if ( ref $self ) {
        $self->{XML_LIBXML_MATCH_CB} = shift if scalar @_;
        return $self->{XML_LIBXML_MATCH_CB};
    }
    else {
        $MatchCB = shift if scalar @_;
        return $MatchCB;
    }
}

sub read_callback {
    my $self = shift;
    if ( ref $self ) {
        $self->{XML_LIBXML_READ_CB} = shift if scalar @_;
        return $self->{XML_LIBXML_READ_CB};
    }
    else {
        $ReadCB = shift if scalar @_;
        return $ReadCB;
    }
}

sub close_callback {
    my $self = shift;
    if ( ref $self ) {
        $self->{XML_LIBXML_CLOSE_CB} = shift if scalar @_;
        return $self->{XML_LIBXML_CLOSE_CB};
    }
    else {
        $CloseCB = shift if scalar @_;
        return $CloseCB;
    }
}

sub open_callback {
    my $self = shift;
    if ( ref $self ) {
        $self->{XML_LIBXML_OPEN_CB} = shift if scalar @_;
        return $self->{XML_LIBXML_OPEN_CB};
    }
    else {
        $OpenCB = shift if scalar @_;
        return $OpenCB;
    }
}

sub callbacks {
    my $self = shift;
    if ( ref $self ) {
        if (@_) {
            my ($match, $open, $read, $close) = @_;
            @{$self}{qw(XML_LIBXML_MATCH_CB XML_LIBXML_OPEN_CB XML_LIBXML_READ_CB XML_LIBXML_CLOSE_CB)} = ($match, $open, $read, $close);
        }
        else {
            return @{$self}{qw(XML_LIBXML_MATCH_CB XML_LIBXML_OPEN_CB XML_LIBXML_READ_CB XML_LIBXML_CLOSE_CB)};
        }
    }
    else {
        if (@_) {
           ( $MatchCB, $OpenCB, $ReadCB, $CloseCB ) = @_;
        }
        else {
            return ( $MatchCB, $OpenCB, $ReadCB, $CloseCB );
        }
    }
}

#-------------------------------------------------------------------------#
# member variable manipulation                                            #
#-------------------------------------------------------------------------#
sub validation {
    my $self = shift;
    $self->{XML_LIBXML_VALIDATION} = shift if scalar @_;
    return $self->{XML_LIBXML_VALIDATION};
}

sub recover {
    my $self = shift;
    $self->{XML_LIBXML_RECOVER} = shift if scalar @_;
    return $self->{XML_LIBXML_RECOVER};
}

sub expand_entities {
    my $self = shift;
    $self->{XML_LIBXML_EXPAND_ENTITIES} = shift if scalar @_;
    return $self->{XML_LIBXML_EXPAND_ENTITIES};
}

sub keep_blanks {
    my $self = shift;
    $self->{XML_LIBXML_KEEP_BLANKS} = shift if scalar @_;
    return $self->{XML_LIBXML_KEEP_BLANKS};
}

sub pedantic_parser {
    my $self = shift;
    $self->{XML_LIBXML_PEDANTIC} = shift if scalar @_;
    return $self->{XML_LIBXML_PEDANTIC};
}

sub line_numbers {
    my $self = shift;
    $self->{XML_LIBXML_LINENUMBERS} = shift if scalar @_;
    return $self->{XML_LIBXML_LINENUMBERS};
}

sub load_ext_dtd {
    my $self = shift;
    $self->{XML_LIBXML_EXT_DTD} = shift if scalar @_;
    return $self->{XML_LIBXML_EXT_DTD};
}

sub complete_attributes {
    my $self = shift;
    $self->{XML_LIBXML_COMPLETE_ATTR} = shift if scalar @_;
    return $self->{XML_LIBXML_COMPLETE_ATTR};
}

sub expand_xinclude  {
    my $self = shift;
    $self->{XML_LIBXML_EXPAND_XINCLUDE} = shift if scalar @_;
    return $self->{XML_LIBXML_EXPAND_XINCLUDE};
}

sub base_uri {
    my $self = shift;
    $self->{XML_LIBXML_BASE_URI} = shift if scalar @_;
    return $self->{XML_LIBXML_BASE_URI};
}

sub gdome_dom {
    my $self = shift;
    $self->{XML_LIBXML_GDOME} = shift if scalar @_;
    return $self->{XML_LIBXML_GDOME};
}

sub clean_namespaces {
    my $self = shift;
    $self->{XML_LIBXML_NSCLEAN} = shift if scalar @_;
    return $self->{XML_LIBXML_NSCLEAN};
}

#-------------------------------------------------------------------------#
# set the optional SAX(2) handler                                         #
#-------------------------------------------------------------------------#
sub set_handler {
    my $self = shift;
    if ( defined $_[0] ) {
        $self->{HANDLER} = $_[0];

        $self->{SAX_ELSTACK} = [];
        $self->{SAX} = {State => 0};
    }
    else {
        # undef SAX handling
        $self->{SAX_ELSTACK} = [];
        delete $self->{HANDLER};
        delete $self->{SAX};
    }
}

#-------------------------------------------------------------------------#
# helper functions                                                        #
#-------------------------------------------------------------------------#
sub _auto_expand {
    my ( $self, $result, $uri ) = @_;

    $result->setBaseURI( $uri ) if defined $uri;

    if ( defined $self->{XML_LIBXML_EXPAND_XINCLUDE}
         and  $self->{XML_LIBXML_EXPAND_XINCLUDE} == 1 ) {
        $self->{_State_} = 1;
        eval { $self->processXIncludes($result); };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
            $result = undef;
            croak $err;
        }
    }
    return $result;
}

sub __read {
    read($_[0], $_[1], $_[2]);
}

sub __write {
    if ( ref( $_[0] ) ) {
        $_[0]->write( $_[1], $_[2] );
    }
    else {
        $_[0]->write( $_[1] );
    }
}

#-------------------------------------------------------------------------#
# parsing functions                                                       #
#-------------------------------------------------------------------------#
# all parsing functions handle normal as SAX parsing at the same time.
# note that SAX parsing is handled incomplete! use XML::LibXML::SAX for
# complete parsing sequences
#-------------------------------------------------------------------------#
sub parse_string {
    my $self = shift;
    croak("parse already in progress") if $self->{_State_};

    unless ( defined $_[0] and length $_[0] ) {
        croak("Empty String");
    }

    $self->{_State_} = 1;
    my $result;

    if ( defined $self->{SAX} ) {
        my $string = shift;
        $self->{SAX_ELSTACK} = [];
        eval { $result = $self->_parse_sax_string($string); };

        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
            croak $err;
        }
    }
    else {
        eval { $result = $self->_parse_string( @_ ); };

        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
            croak $err;
        }

        $result = $self->_auto_expand( $result, $self->{XML_LIBXML_BASE_URI} );
    }

    return $result;
}

sub parse_fh {
    my $self = shift;
    croak("parse already in progress") if $self->{_State_};
    $self->{_State_} = 1;
    my $result;
    if ( defined $self->{SAX} ) {
        $self->{SAX_ELSTACK} = [];
        eval { $self->_parse_sax_fh( @_ );  };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
            croak $err;
        }
    }
    else {
        eval { $result = $self->_parse_fh( @_ ); };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
            croak $err;
        }

        $result = $self->_auto_expand( $result, $self->{XML_LIBXML_BASE_URI} );
    }

    return $result;
}

sub parse_file {
    my $self = shift;
    croak("parse already in progress") if $self->{_State_};
    $self->{_State_} = 1;
    my $result;
    if ( defined $self->{SAX} ) {
        $self->{SAX_ELSTACK} = [];
        eval { $self->_parse_sax_file( @_ );  };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
            croak $err;
        }
    }
    else {
        eval { $result = $self->_parse_file(@_); };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
            croak $err;
        }

        $result = $self->_auto_expand( $result );
    }

    return $result;
}

sub parse_xml_chunk {
    my $self = shift;
    # max 2 parameter:
    # 1: the chunk
    # 2: the encoding of the string
    croak("parse already in progress") if $self->{_State_};    my $result;

    unless ( defined $_[0] and length $_[0] ) {
        croak("Empty String");
    }

    $self->{_State_} = 1;
    if ( defined $self->{SAX} ) {
        eval {
            $self->_parse_sax_xml_chunk( @_ );

            # this is required for XML::GenericChunk.
            # in normal case is_filter is not defined, an thus the parsing
            # will be terminated. in case of a SAX filter the parsing is not
            # finished at that state. therefore we must not reset the parsing
            unless ( $self->{IS_FILTER} ) {
                $result = $self->{HANDLER}->end_document();
            }
        };
    }
    else {
        eval { $result = $self->_parse_xml_chunk( @_ ); };
    }

    my $err = $@;
    $self->{_State_} = 0;
    if ($err) {
        croak $err;
    }

    return $result;
}

sub parse_balanced_chunk {
    my $self = shift;
    return $self->parse_xml_chunk( @_ );
}

# java style
sub processXIncludes {
    my $self = shift;
    my $doc = shift;
    return $self->_processXIncludes($doc || " ");
}

# perl style
sub process_xincludes {
    my $self = shift;
    my $doc = shift;
    return $self->_processXIncludes($doc || " ");
}


#-------------------------------------------------------------------------#
# push parser interface                                                   #
#-------------------------------------------------------------------------#
sub init_push {
    my $self = shift;

    if ( defined $self->{CONTEXT} ) {
        delete $self->{CONTEXT};
    }

    if ( defined $self->{SAX} ) {
        $self->{CONTEXT} = $self->_start_push(1);
    }
    else {
        $self->{CONTEXT} = $self->_start_push(0);
    }
}

sub push {
    my $self = shift;

    if ( not defined $self->{CONTEXT} ) {
        $self->init_push();
    }

    foreach ( @_ ) {
        $self->_push( $self->{CONTEXT}, $_ );
    }
}

# this function should be promoted!
# the reason is because libxml2 uses xmlParseChunk() for this purpose!
sub parse_chunk {
    my $self = shift;
    my $chunk = shift;
    my $terminate = shift;

    if ( not defined $self->{CONTEXT} ) {
        $self->init_push();
    }

    if ( defined $chunk and length $chunk ) {
        $self->_push( $self->{CONTEXT}, $chunk );
    }

    if ( $terminate ) {
        return $self->finish_push();
    }
}


sub finish_push {
    my $self = shift;
    my $restore = shift || 0;
    return undef unless defined $self->{CONTEXT};

    my $retval;

    if ( defined $self->{SAX} ) {
        eval {
            $self->_end_sax_push( $self->{CONTEXT} );
            $retval = $self->{HANDLER}->end_document( {} );
        };
    }
    else {
        eval { $retval = $self->_end_push( $self->{CONTEXT}, $restore ); };
    }

    delete $self->{CONTEXT};

    if ( $@ ) {
        croak( $@ );
    }
    return $retval;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Node Interface                                             #
#-------------------------------------------------------------------------#
package XML::LibXML::Node;

sub isSupported {
    my $self    = shift;
    my $feature = shift;
    return $self->can($feature) ? 1 : 0;
}

sub getChildNodes { my $self = shift; return $self->childNodes(); }

sub childNodes {
    my $self = shift;
    my @children = $self->_childNodes();
    return wantarray ? @children : XML::LibXML::NodeList->new( @children );
}

sub attributes {
    my $self = shift;
    my @attr = $self->_attributes();
    return wantarray ? @attr : XML::LibXML::NamedNodeMap->new( @attr );
}

sub iterator {
    warn "this function is obsolete!\nIt was disabled in version 1.54\n";
    return undef;
}


sub findnodes {
    my ($node, $xpath) = @_;
    my @nodes = $node->_findnodes($xpath);
    if (wantarray) {
        return @nodes;
    }
    else {
        return XML::LibXML::NodeList->new(@nodes);
    }
}

sub findvalue {
    my ($node, $xpath) = @_;
    my $res;
    eval {
        $res = $node->find($xpath);
    };
    if  ( $@ ) {
        die $@;
    }
    return $res->to_literal->value;
}

sub find {
    my ($node, $xpath) = @_;
    my ($type, @params) = $node->_find($xpath);
    if ($type) {
        return $type->new(@params);
    }
    return undef;
}

sub setOwnerDocument {
    my ( $self, $doc ) = @_;
    $doc->adoptNode( $self );
}

sub serialize_c14n {
    my $self = shift;
    return $self->toStringC14N( @_ );
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Document Interface                                         #
#-------------------------------------------------------------------------#
package XML::LibXML::Document;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');

sub setDocumentElement {
    my $doc = shift;
    my $element = shift;

    my $oldelem = $doc->documentElement;
    if ( defined $oldelem ) {
        $doc->removeChild($oldelem);
    }

    $doc->_setDocumentElement($element);
}

sub toString {
    my $self = shift;
    my $flag = shift;

    my $retval = "";

    if ( defined $XML::LibXML::skipXMLDeclaration
         and $XML::LibXML::skipXMLDeclaration == 1 ) {
        foreach ( $self->childNodes ){
            next if $_->nodeType == XML::LibXML::XML_DTD_NODE()
                    and $XML::LibXML::skipDTD;
            $retval .= $_->toString;
        }
    }
    else {
        $flag ||= 0 unless defined $flag;
        $retval =  $self->_toString($flag);
    }

    return $retval;
}

sub serialize {
    my $self = shift;
    return $self->toString( @_ );
}

#-------------------------------------------------------------------------#
# bad style xinclude processing                                           #
#-------------------------------------------------------------------------#
sub process_xinclude {
    my $self = shift;
    XML::LibXML->new->processXIncludes( $self );
}

sub insertProcessingInstruction {
    my $self   = shift;
    my $target = shift;
    my $data   = shift;

    my $pi     = $self->createPI( $target, $data );
    my $root   = $self->documentElement;

    if ( defined $root ) {
        # this is actually not correct, but i guess it's what the user
        # intends
        $self->insertBefore( $pi, $root );
    }
    else {
        # if no documentElement was found we just append the PI
        $self->appendChild( $pi );
    }
}

sub insertPI {
    my $self = shift;
    $self->insertProcessingInstruction( @_ );
}

#-------------------------------------------------------------------------#
# DOM L3 Document functions.
# added after robins implicit feature requst
#-------------------------------------------------------------------------#
sub getElementsByTagName {
    my ( $doc , $name ) = @_;
    my $xpath = "descendant-or-self::node()/$name";
    my @nodes = $doc->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new(@nodes);
}

sub  getElementsByTagNameNS {
    my ( $doc, $nsURI, $name ) = @_;
    my $xpath = "descendant-or-self::*[local-name()='$name' and namespace-uri()='$nsURI']";
    my @nodes = $doc->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new(@nodes);
}

sub getElementsByLocalName {
    my ( $doc,$name ) = @_;
    my $xpath = "descendant-or-self::*[local-name()='$name']";
    my @nodes = $doc->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new(@nodes);
}

sub getElementsById {
    my ( $doc, $id ) = @_;
    return ($doc->findnodes( "id('$id')" ))[0];
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::DocumentFragment Interface                                 #
#-------------------------------------------------------------------------#
package XML::LibXML::DocumentFragment;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');

sub toString {
    my $self = shift;
    my $retval = "";
    if ( $self->hasChildNodes() ) {
        foreach my $n ( $self->childNodes() ) {
            $retval .= $n->toString(@_);
        }
    }
    return $retval;
}


sub serialize {
    my $self = shift;
    return $self->toString(@_);
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Element Interface                                          #
#-------------------------------------------------------------------------#
package XML::LibXML::Element;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');

sub setNamespace {
    my $self = shift;
    my $n = $self->nodeName;
    if ( $self->_setNamespace(@_) ){
        if ( scalar @_ < 3 || $_[2] == 1 ){
            $self->setNodeName( $n );
        }
        return 1;
    }
    return 0;
}

sub setAttribute {
    my ( $self, $name, $value ) = @_;
    if ( $name =~ /^xmlns/ ) {
        # user wants to set a namespace ...

        (my $lname = $name )=~s/^xmlns://;
        my $nn = $self->nodeName;
        if ( $nn =~ /^$lname\:/ ) {
            $self->setNamespace($value, $lname);
        }
        else {
            # use a ($active = 0) namespace
            $self->setNamespace($value, $lname, 0);
        }
    }
    else {
        $self->_setAttribute($name, $value);
    }
}

sub getElementsByTagName {
    my ( $node , $name ) = @_;
    my $xpath = "descendant::$name";
    my @nodes = $node->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new(@nodes);
}

sub  getElementsByTagNameNS {
    my ( $node, $nsURI, $name ) = @_;
    my $xpath = "descendant::*[local-name()='$name' and namespace-uri()='$nsURI']";
    my @nodes = $node->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new(@nodes);
}

sub getElementsByLocalName {
    my ( $node,$name ) = @_;
    my $xpath = "descendant::*[local-name()='$name']";
        my @nodes = $node->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new(@nodes);
}

sub getChildrenByTagName {
    my ( $node, $name ) = @_;
    my @nodes = grep { $_->nodeName eq $name } $node->childNodes();
    return wantarray ? @nodes : XML::LibXML::NodeList->new(@nodes);
}

sub getChildrenByTagNameNS {
    my ( $node, $nsURI, $name ) = @_;
    my $xpath = "*[local-name()='$name' and namespace-uri()='$nsURI']";
    my @nodes = $node->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new(@nodes);
}

sub appendWellBalancedChunk {
    my ( $self, $chunk ) = @_;

    my $local_parser = XML::LibXML->new();
    my $frag = $local_parser->parse_xml_chunk( $chunk );

    $self->appendChild( $frag );
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Text Interface                                             #
#-------------------------------------------------------------------------#
package XML::LibXML::Text;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');

sub attributes { return undef; }

sub deleteDataString {
    my $node = shift;
    my $string = shift;
    my $all    = shift;
    my $data = $node->nodeValue();
    $string =~ s/([\\\*\+\^\{\}\&\?\[\]\(\)\$\%\@])/\\$1/g;
    if ( $all ) {
        $data =~ s/$string//g;
    }
    else {
        $data =~ s/$string//;
    }
    $node->setData( $data );
}

sub replaceDataString {
    my ( $node, $left, $right,$all ) = @_;

    #ashure we exchange the strings and not expressions!
    $left  =~ s/([\\\*\+\^\{\}\&\?\[\]\(\)\$\%\@])/\\$1/g;
    my $datastr = $node->nodeValue();
    if ( $all ) {
        $datastr =~ s/$left/$right/g;
    }
    else{
        $datastr =~ s/$left/$right/;
    }
    $node->setData( $datastr );
}

sub replaceDataRegEx {
    my ( $node, $leftre, $rightre, $flags ) = @_;
    return unless defined $leftre;
    $rightre ||= "";

    my $datastr = $node->nodeValue();
    my $restr   = "s/" . $leftre . "/" . $rightre . "/";
    $restr .= $flags if defined $flags;

    eval '$datastr =~ '. $restr;

    $node->setData( $datastr );
}

1;

package XML::LibXML::Comment;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Text');

1;

package XML::LibXML::CDATASection;

use vars qw(@ISA);
@ISA     = ('XML::LibXML::Text');

sub nodeName {
    return "cdata";
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Attribute Interface                                        #
#-------------------------------------------------------------------------#
package XML::LibXML::Attr;
use vars qw( @ISA ) ;
@ISA = ('XML::LibXML::Node') ;

sub setNamespace {
    my ($self,$href,$prefix) = @_;
    my $n = $self->nodeName;
    if ( $self->_setNamespace($href,$prefix) ) {
        $self->setNodeName($n);
        return 1;
    }

    return 0;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Dtd Interface                                              #
#-------------------------------------------------------------------------#
# this is still under construction
#
package XML::LibXML::Dtd;
use vars qw( @ISA );
@ISA = ('XML::LibXML::Node');

1;

#-------------------------------------------------------------------------#
# XML::LibXML::PI Interface                                               #
#-------------------------------------------------------------------------#
package XML::LibXML::PI;
use vars qw( @ISA );
@ISA = ('XML::LibXML::Node');

sub setData {
    my $pi = shift;

    my $string = "";
    if ( scalar @_ == 1 ) {
        $string = shift;
    }
    else {
        my %h = @_;
        $string = join " ", map {$_.'="'.$h{$_}.'"'} keys %h;
    }

    # the spec says any char but "?>" [17]
    $pi->_setData( $string ) unless  $string =~ /\?>/;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Namespace Interface                                        #
#-------------------------------------------------------------------------#
package XML::LibXML::Namespace;

# this is infact not a node!
sub prefix { return "xmlns"; }

sub getNamespaces { return (); }

sub nodeName {
    my $self = shift;
    my $nsP  = $self->name;
    return ( defined($nsP) && length($nsP) ) ? "xmlns:$nsP" : "xmlns";
}

sub getNodeName { my $self = shift; return $self->nodeName; }

sub isEqualNode {
    my ( $self, $ref ) = @_;
    if ( ref($ref) eq "XML::LibXML::Namespace" ) {
        return $self->_isEqual($ref);
    }
    return 0;
}

sub isSameNode {
    my ( $self, $ref ) = @_;
    if ( $$self == $$ref ){
        return 1;
    }
    return 0;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::NamedNodeMap Interface                                     #
#-------------------------------------------------------------------------#
package XML::LibXML::NamedNodeMap;

use XML::LibXML::Common qw(:libxml);

sub new {
    my $class = shift;
    my $self = bless { Nodes => [@_] }, $class;
    $self->{NodeMap} = { map { $_->nodeName => $_ } @_ };
    return $self;
}

sub length     { return scalar( @{$_[0]->{Nodes}} ); }
sub nodes      { return $_[0]->{Nodes}; }
sub item       { $_[0]->{Nodes}->[$_[1]]; }

sub getNamedItem {
    my $self = shift;
    my $name = shift;

    return $self->{NodeMap}->{$name};
}

sub setNamedItem {
    my $self = shift;
    my $node = shift;

    my $retval;
    if ( defined $node ) {
        if ( scalar @{$self->{Nodes}} ) {
            my $name = $node->nodeName();
            if ( $node->nodeType() == XML_NAMESPACE_DECL ) {
                return;
            }
            if ( defined $self->{NodeMap}->{$name} ) {
                if ( $node->isSameNode( $self->{NodeMap}->{$name} ) ) {
                    return;
                }
                $retval = $self->{NodeMap}->{$name}->replaceNode( $node );
            }
            else {
                $self->{Nodes}->[0]->addSibling($node);
            }

            $self->{NodeMap}->{$name} = $node;
            push @{$self->{Nodes}}, $node;
        }
        else {
            # not done yet
            # can this be properly be done???
            warn "not done yet\n";
        }
    }
    return $retval;
}

sub removeNamedItem {
    my $self = shift;
    my $name = shift;
    my $retval;
    if ( $name =~ /^xmlns/ ) {
        warn "not done yet\n";
    }
    elsif ( exists $self->{NodeMap}->{$name} ) {
        $retval = $self->{NodeMap}->{$name};
        $retval->unbindNode;
        delete $self->{NodeMap}->{$name};
        $self->{Nodes} = [grep {not($retval->isSameNode($_))} @{$self->{Nodes}}];
    }

    return $retval;
}

sub getNamedItemNS {
    my $self = shift;
    my $nsURI = shift;
    my $name = shift;
    return undef;
}

sub setNamedItemNS {
    my $self = shift;
    my $nsURI = shift;
    my $node = shift;
    return undef;
}

sub removeNamedItemNS {
    my $self = shift;
    my $nsURI = shift;
    my $name = shift;
    return undef;
}

1;

package XML::LibXML::_SAXParser;

# this is pseudo class!!! and it will be removed as soon all functions
# moved to XS level

use XML::SAX::Exception;

# these functions will use SAX exceptions as soon i know how things really work
sub warning {
    my ( $parser, $message, $line, $col ) = @_;
    my $error = XML::SAX::Exception::Parse->new( LineNumber   => $line,
                                                 ColumnNumber => $col,
                                                 Message      => $message, );
    $parser->{HANDLER}->warning( $error );
}

sub error {
    my ( $parser, $message, $line, $col ) = @_;

    my $error = XML::SAX::Exception::Parse->new( LineNumber   => $line,
                                                 ColumnNumber => $col,
                                                 Message      => $message, );
    $parser->{HANDLER}->error( $error );
}

sub fatal_error {
    my ( $parser, $message, $line, $col ) = @_;
    my $error = XML::SAX::Exception::Parse->new( LineNumber   => $line,
                                                 ColumnNumber => $col,
                                                 Message      => $message, );
    $parser->{HANDLER}->fatal_error( $error );
}

1;

package XML::LibXML::RelaxNG;

sub new {
    my $class = shift;
    my %args = @_;

    my $self = undef;
    if ( defined $args{location} ) {
        $self = $class->parse_location( $args{location} );
    }
    elsif ( defined $args{string} ) {
        $self = $class->parse_buffer( $args{string} );
    }
    elsif ( defined $args{DOM} ) {
        $self = $class->parse_document( $args{DOM} );
    }

    return $self;
}

1;

package XML::LibXML::Schema;

sub new {
    my $class = shift;
    my %args = @_;

    my $self = undef;
    if ( defined $args{location} ) {
        $self = $class->parse_location( $args{location} );
    }
    elsif ( defined $args{string} ) {
        $self = $class->parse_buffer( $args{string} );
    }

    return $self;
}

1;

__END__
