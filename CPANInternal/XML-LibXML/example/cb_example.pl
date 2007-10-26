use XML::LibXML;
use IO::File;

# first instanciate the parser
my $parser = XML::LibXML->new();

# initialize the callbacks
$parser->match_callback( \&match_uri );
$parser->read_callback( \&read_uri );
$parser->open_callback( \&open_uri );
$parser->close_callback( \&close_uri );

# include XIncludes on the fly
$parser->expand_xinclude( 1 );

# parse the file "text.xml" in the current directory
$dom = $parser->parse_file("test.xml");

print $dom->toString() , "\n";

# the callbacks follow
# these callbacks are used for both the original parse AND the XInclude
sub match_uri {
    my $uri = shift;
    return $uri !~ /:\/\// ? 1 : 0; # we handle only files
}

sub open_uri {
    my $uri = shift;

    my $handler = new IO::File;
    if ( not $handler->open( "<$uri" ) ){
        $handler = 0;
    }
    
    return $handler;
}

sub read_uri {
    my $handler = shift;
    my $length  = shift;
    my $buffer = undef;
    if ( $handler ) {
        $handler->read( $buffer, $length );
    }
    return $buffer;
}

sub close_uri {
    my $handler = shift;
    if ( $handler ) {
        $handler->close();
    }
    return 1;
}

