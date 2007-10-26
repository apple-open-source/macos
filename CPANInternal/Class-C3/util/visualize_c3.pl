#!/usr/bin/perl

use strict;
use warnings;

=pod

This is a visualization tool to help with 
understanding large MI hierarchies. It will
output a DOT file for rendering with Graphviz.

NOTE:
This program is currently very primative, and 
may break under some circumstances. If you 
encounter one of those circumstances, please
email me about it so that I can improve this 
tool. 

GRAPH LEGEND:
In the graphs the green arrows are the ISA, 
and the red arrows are the C3 dispatch order.

=cut

use Class::C3 ();

@ARGV || die "usage : visualize_c3.pl <class-to-visualize> | <file-to-load> <class-to-visualize> <file-to-output>";

my ($class, $OUT);
if (scalar @ARGV == 1) {
    $class = shift @ARGV;
    eval "use $class";
    die "Could not load '$class' :\n$@" if $@;
}
else {
    my $file = shift @ARGV;
    $class = shift @ARGV;    
    $OUT = shift @ARGV;
    do $file;
    die "Could not load '$file' :\n$@" if $@;    
}

Class::C3->initialize();

my @MRO = Class::C3::calculateMRO($class);

sub get_class_str {
    my $class = shift;
    (join "_" => (split '::' => $class));    
}

my $output = "graph test {\n";

my $prev;
foreach my $class (@MRO) {
    my $class_str = get_class_str($class);
    $output .= "node_${class_str} [ label = \"" . $class . "\" ];\n";
    {
        no strict 'refs';
        foreach my $super (@{"${class}::ISA"}) {
            $output .= "node_" . get_class_str($super) . 
                       " -- node_${class_str}" .
                       " [ dir = back, color = green ];\n";
        }
    }
    if ($prev) {
        $output .= "node_${class_str} -- node_${prev}  [ dir = back, color = red ];\n";
    }    
    $prev = $class_str;
}

$output .= "}\n";

warn $output;

if ($OUT) {
    open OUT, ">", $OUT || die "could not open '$OUT' for output";
    print OUT $output;
    close OUT;
}
else {
    print $output;
}