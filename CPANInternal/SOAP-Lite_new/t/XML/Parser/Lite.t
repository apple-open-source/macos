use strict;
use Test::More tests => 38;

use_ok qw(XML::Parser::Lite);

my $parser;
ok $parser = XML::Parser::Lite->new(), 'Create parser';
is $parser, $parser->new();
isa_ok $parser, 'XML::Parser::Lite';

my @start_from  = ();
my @text_from   = ();
my @end_from    = ();
my @final_from  = ();

sub _reset {
    @final_from     = ();
    @text_from      = ();
    @start_from     = ();
    @end_from       = ();
}

my %handler_of = (
    Final   => sub { push @final_from, 1; },
    Char    => sub { push @text_from, $_[1]; },
    Start   => sub { push @start_from, $_[1]; },
    End     => sub { push @end_from, $_[1]; },
);
$parser->setHandlers( Final => undef, );
$parser->setHandlers( %handler_of );

test_parse($parser);

ok $parser = XML::Parser::Lite->new( Handlers => { %handler_of }), 'Create parser';

test_parse($parser);

$parser->setHandlers( Char => undef );
eval { $parser->parse('<foo>bar</foo><bar>bar</bar>') };
like $@ , qr{ multiple \s roots, \s wrong \s element}x, 'detect multiple roots';

ok $parser->parse('

    <foo></foo>'
), 'parse xml with whitespaces/newlines before XML';
eval { $parser->parse('Foobar<foo>bar</foo><bar>bar</bar>') };
like $@ , qr{ junk \s 'Foobar' \s before \s XML \s element}x, 'detect junk before XML';
eval { $parser->parse('<foo>bar</foo>Foobar<bar>bar</bar>') };
like $@ , qr{ junk \s 'Foobar' \s after \s XML \s element}x, 'detect junk after XML';

SKIP: {
    skip 'need File::Basename for resolveing filename', 1
        if ( ! eval "require File::Basename");
    my $dir = File::Basename::dirname( __FILE__ );
    $parser = XML::Parser::Lite->new();
    open my $fh, '<', "$dir/adam.xml";
    my $xml = join ("\n", <$fh>);
    close $fh;
    $parser->parse($xml);
    pass 'parse XML with doctype and processing instructions';
};

sub test_parse {

    _reset();
    my $parser = shift;

    $parser->parse('<foo>bar</foo>');
    is $text_from[0], 'bar', 'char callback';
    is $start_from[0], 'foo', 'start callback';
    is $final_from[0], 1, 'final callback';
    _reset();

    $parser->parse('<baz><foo>bar</foo><foo>bar</foo></baz>');
    is $text_from[1], 'bar', 'char callback';
    is $start_from[0], 'baz', 'start callback';
    is $start_from[1], 'foo', 'start callback';
    is $start_from[2], 'foo', 'start callback';
    is $end_from[2], 'baz', 'start callback';
    is $end_from[1], 'foo', 'start callback';
    is $end_from[0], 'foo', 'start callback';
    is $final_from[0], 1, 'final callback';
    eval { $parser->parse('<foo><bar>baz</bar>'); };
    like $@, qr{ not \s properly \s closed \s }x, 'detect unclosed tag';
    eval { $parser->parse(''); };
    like $@, qr{ no \s element \s found \s }x, 'detect no element';
    eval { $parser->parse('<foo><bar>baz</foo>'); };
    like $@, qr{ mismatched \s tag\s }x, 'detect mismatched tag';

}
