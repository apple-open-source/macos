# $Id: 27new_callbacks_simple.t,v 1.1.1.1 2007/10/10 23:04:15 ahuda Exp $
use Test;
BEGIN { plan tests => 21 }
END { ok(0) unless $loaded }
use XML::LibXML;
use IO::File;
$loaded = 1;
ok(1);

# --------------------------------------------------------------------- #
# simple test
# --------------------------------------------------------------------- #
my $string = <<EOF;
<x xmlns:xinclude="http://www.w3.org/2001/XInclude"><xml>test<xinclude:include href="/example/test2.xml"/></xml></x>
EOF

my $icb    = XML::LibXML::InputCallback->new();
ok($icb);

$icb->register_callbacks( [ \&match_file, \&open_file, 
                            \&read_file, \&close_file ] );

my $parser = XML::LibXML->new();
$parser->expand_xinclude(1);
$parser->input_callbacks($icb);
my $doc = $parser->parse_string($string);

ok($doc);
ok($doc->string_value(),"test..");

my $icb2    = XML::LibXML::InputCallback->new();
ok($icb2);

$icb2->register_callbacks( [ \&match_hash, \&open_hash, 
                             \&read_hash, \&close_hash ] );

$parser->input_callbacks($icb2);
$doc = $parser->parse_string($string);

ok($doc);
ok($doc->string_value(),"testbar..");

# --------------------------------------------------------------------- #
# CALLBACKS
# --------------------------------------------------------------------- #
# --------------------------------------------------------------------- #
# callback set 1 (perl file reader)
# --------------------------------------------------------------------- #
sub match_file {
        my $uri = shift;
        if ( $uri =~ /^\/example\// ){
                ok(1);
                return 1;
        }
        return 0;        
}

sub open_file {
        my $uri = shift;
        $file = new IO::File;

        if ( $file->open( "< .$uri" ) ){
                ok(1);
        }
        else {
                # warn "cannot open file";
                $file = 0;
        }   
        return $file;
}

sub read_file {
        my $h   = shift;
        my $buflen = shift;
        my $rv   = undef;

        ok(1);
        
        my $n = $h->read( $rv , $buflen );

        return $rv;
}

sub close_file {
        my $h   = shift;
        ok(1);
        $h->close();
        return 1;
}

# --------------------------------------------------------------------- #
# callback set 2 (perl hash reader)
# --------------------------------------------------------------------- #
sub match_hash {
        my $uri = shift;
        if ( $uri =~ /^\/example\// ){
                ok(1);
                return 1;
        }
}

sub open_hash {
        my $uri = shift;
        my $hash = { line => 0,
                     lines => [ "<foo>", "bar", "<xsl/>", "..", "</foo>" ],
                   };                
        ok(1);

        return $hash;
}

sub read_hash {
        my $h   = shift;
        my $buflen = shift;

        my $id = $h->{line};
        $h->{line} += 1;
        my $rv= $h->{lines}->[$id];

        $rv = "" unless defined $rv;

        ok(1);
        return $rv;
}

sub close_hash {
        my $h   = shift;
        undef $h;
        ok(1);
}
