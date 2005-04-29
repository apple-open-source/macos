#!/usr/bin/perl

package Razor2::Signature::Ephemeral;
use strict;
use Digest::SHA1;
use Data::Dumper;

sub new {

    my ($class, %args) = @_;

    my $self = bless {
        seed => $args{seed} || 42, 
        separator => encode_separator($args{separator}) || encode_separator("10"),
    }, $class;
    $self;

}


sub hexdigest { 

    my ($self, $content) = @_;

    # Initialize PRNG with $seed
    srand($$self{seed});

    my @content = split /$$self{separator}/, $content;
    # $content =~ s/$$self{separator}//g; -- We don't do this anyore

    # my $size = length($content); 
    my $lines = scalar @content;

    debug("\nNumber of lines: $lines");
   
    # Randomly choose relative locations and section sizes (in percent)
    my $sections = 6;
    my $ssize = 100/$sections;
    my @rel_lineno = map { rand($ssize) + ($_*$ssize) } 0 .. ($sections-1);
    my @lineno = map { int(($_ * $lines)/100) } @rel_lineno;

    debug("Relative Line Numbers (in percent): @rel_lineno");
    debug("Absolute Line Numbers: @lineno");

    my @rel_offset1 = map { rand(50) + ($_*50) } qw(0 1);
    my @rel_offset2 = map { rand(50) + ($_*50) } qw(0 1);

    debug("Relative Offsets for section 1: @rel_offset1");
    debug("Relative Offsets for section 2: @rel_offset2");

    my  ($l1, $l2) = (0, 0);
    for ($lineno[1] .. $lineno[2]) { $l1 += length($content[$_]) if $content[$_]}
    for ($lineno[3] .. $lineno[4]) { $l2 += length($content[$_]) if $content[$_] } 

    debug("Length of the first section: $l1 bytes");
    debug("Length of the second section: $l2 bytes");

    my @offset1 = map { int(($_ * $l1)/100) } @rel_offset1;
    my @offset2 = map { int(($_ * $l2)/100) } @rel_offset2;

    debug("Chunk start/end positions in Section 1: @offset1 (length: " . ($offset1[1] - $offset1[0]) .") ");
    debug("Chunk start/end positions in Section 2: @offset2 (length: " . ($offset2[1] - $offset2[0]) .") ");

    my $x = 0;
    my ($sc, $sl, $ec, $el) = (0,0,0,0);
    my $section1 = picksection( \@content, 
                                $lineno[1], $lineno[2], 
                                $offset1[0], $offset1[1]
                              );
    my $section2 = picksection( \@content, 
                                $lineno[3], $lineno[4], 
                                $offset2[0], $offset2[1]
                              );
 
    debug("Section 1: $section1");
    debug("Section 2: $section2");

    my $seclength = length($section1.$section2);

    debug("Total length of stuff that will be hashed: $seclength");

    if ($section1 =~ /^\s+$/ && $section2 =~ /^\s+$/) { 
        debug("Both sections were whitespace only!");
        $section1 = "";
        $section2 = "";
    }

    my $digest;
    my $ctx = Digest::SHA1->new;

    if ($seclength > 128) { 
        $ctx->add($section1);
        $ctx->add($section2);
        $digest = $ctx->hexdigest;
    } else { 
        debug("Sections too small... reverting back to orginal content.");
        $ctx->add($content);
        $digest = $ctx->hexdigest;
    }

    debug("Computed e-hash is $digest");

    return $digest;

    
}


sub picksection { 

    my ($content, $sline, $eline, $soffset, $eoffset) = @_;
    my $x = 0;
    my ($sc, $sl, $ec, $el) = (0,0,0,0);
    
    for ($sline .. $eline) {
        next unless $content->[$_]; 
        $x = $x + length($content->[$_]);
        if (($x > $soffset) && ($sc == 0)) { # we come here first time
            $sc = length($content->[$_]) - ($x - $soffset);  # $x is greater than start
            $sl = $_;                        # offset
        }
        if ($x > $eoffset) { 
            $ec = length($content->[$_]) - ($x - $eoffset);    
            $el = $_;
        } 
        last if $ec;                       
    }

    $sc = 0 if $sc < 0;
    $ec = 0 if $ec < 0; # FIX!  not verified to work correctly.

    debug("Absolute chunk offsets: Line $sl charachter $sc to line $el character $ec");

    my $section = "";

    if ($sl == $el) {
        if ($content->[$sl]) { 
            $section = substr ($content->[$sl], $sc, $ec - $sc + 1);
        } else { 
            $section = "";
        }
    } else {
	    $section .= substr($content->[$sl], $sc);
        for ($sl+1 .. $el-1) { 
            $section .= $content->[$_];
        }
        $section .= substr($content->[$el], 0, $ec);
    }
    return $section;
}


sub encode_separator { 

    my ($self, $separator) = @_;
    my $rv;

    unless (ref $self) { $separator = $self }
    my @chars = split/-/, $separator;
    push @chars, $separator unless scalar @chars;
    for (@chars) { $rv .= chr($_) } 
    return $rv;

}


sub debug { 
    my $message = shift;
    # print "debug: $message\n";
    #open TMP, ">>/tmp/ehash";
    #print TMP "$message\n";
    #close TMP;
}


1;

