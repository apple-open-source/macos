#!/usr/bin/perl

# This script processes the installed openssl headers, marking all prototypes as deprecated.

use strict;

my ($dir) = @ARGV;

if ($dir eq "clean") {
    # nothing to do
    exit 0;
}

if (! -d $dir || ! -d "$dir/openssl") {
    print STDERR "$dir is invalid; run with one argument, a directory to process\n";
    exit 1;
}

chdir("$dir/openssl");
my @files = `find . -name '*.h' -type f`;
chomp @files;

# some special characters that confuse Xcode's indentation/formatting
my $hash = "#";
my $leftp = "(";
my $rightp = ")";

sub fixProto($) {
    # fix a single prototype
    
    my ($proto) = @_;
    
    #print "'$proto'\n";
    
    if (($proto =~ m|typedef|si) ||
        ($proto =~ m|OPENSSL_DECLARE_GLOBAL|si)) {
        # if the line is a typedef or openssl global, don't bother to deprecate
        return $proto;
    }
    
    # insert a deprecation attribute between any right paren and semi-colon
    $proto =~ s|\Q${rightp}\E\s*;|${rightp} DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;|sg;
    return $proto;
}

sub readFile($) {
    my ($file) = @_;
    open my $f, "< $file";
    my $data = "";
    my @lines;
    while (my $l = <$f>) {
        $data .= $l;
        push @lines, $l;
    }
    return ($data, @lines);
}

sub countBalancedParens($) {
    my ($str) = @_;
    
    # delete comments
    $str =~ s|/\*.*?\*/||sg;

    # count left and right parens
    my @left = ($str =~ m|(\Q$leftp\E)|sg);
    my @right = ($str =~ m|(\Q$rightp\E)|sg);

    if ($#left != $#right) {
        return -1; # unbalanced
    }
    return $#left + 1;
}

foreach my $file (sort(@files)) {
    my ($data, @lines) = readFile($file);
    
    my $newdata = "";
    my $cur = "";
    
    # process each line of the file
    foreach my $line (@lines) {
        $cur .= $line;
        
        if (($cur =~ m|/\*|s) &&
            ($cur !~ m|\*/|s)) {
                # unfinished multi-line comment; keep adding lines
            } elsif ($cur =~ m|\\\n$|) {
                # unfinished continued line; keep adding lines
            } elsif ($cur =~ m|^\s*${hash}|s) {
                # a preprocessor directive; emit and continue
                $newdata .= $cur;
                $cur = "";
            } else {
                
                # we have a sequence of lines that we might want to process
                
                my $parenCount = countBalancedParens($cur);
                
                if ($parenCount == -1) {
                    # unbalanced parens, keep adding lines
                } else {
                    if ($parenCount > 0) {
                        # there's at least one pair of balanced parens, so let's fix up any protos
                        $cur = fixProto($cur);
                    }
                    # emit and continue
                    $newdata .= $cur;
                    $cur = "";
                }
            }
    }
    $newdata .= $cur;
    
    if ($data ne $newdata) {
        # if we changed the contents of the file, we should insert an include of AvailabilityMacros.h
        $newdata =~ s|(${hash}ifndef\s+(\w+_H)\s*\n${hash}define\s+\g2[ \t]*\n)|"$1\n#include <AvailabilityMacros.h>\n"|es;
        
        # rewrite the file
        open my $f, "> $file";
        print $f $newdata;
        close $f;
    }
}

# make sure we can include all the headers we processed

open my $f, "> /tmp/test.$$.c";

# this has to come first
print $f "#include <openssl/ssl.h>\n";

foreach my $file (@files) {
    # include each file
    $file =~ s|^\./||;
    print $f "#include <openssl/$file>\n";
}
close $f;

# make sure we can compile
my @cmd = ("clang", "-I$dir", "-c", "-o", "/tmp/test.$$.o", "/tmp/test.$$.c");
system(@cmd);

my $ret = 0;
# make sure the compilation succeeded
if (! -e "/tmp/test.$$.o") {
    print STDERR "There was an error including the deprecated headers\n";
    $ret = 1;
}

unlink("/tmp/test.$$.c", "/tmp/test.$$.o");
exit $ret;
