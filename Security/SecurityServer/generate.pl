#!/usr/bin/perl
#
#
#
use strict;

my $disclaimer = "Automatically generated - do not edit on penalty of futility!";


# arguments
my ($configfile, $out_h, $out_cpp, $types) = @ARGV;


# open configuration file
open(CFG, "$configfile") || die "$configfile: $!";

# open and load cssmtypes file
open(TYPES, "$types") || die "$types: $!";
$/=undef;
my $types_h = <TYPES>;
close(TYPES); $/="\n";

# open output files
open(H, ">$out_h") || die "$out_h: $!";
open(CPP, ">$out_cpp") || die "$out_cpp: $!";

# cautionary headings to each file
print H <<EOH;
//
// Flipping bytes for SecurityServer transition.
// $disclaimer
//
EOH

print CPP <<EOC;
//
// Flipping bytes for SecurityServer transition.
// $disclaimer
//
EOC

# process generation instructions
while (<CFG>) {
  chomp;
  next if /^[ 	]*#/;
  next if /^[ 	]*$/;
  
  my @args = split;
  $_ = shift @args;
  my ($cssmName, @aliases) = split /\//;
  
  print H "void flip($cssmName &obj);\n";
  for my $alias (@aliases) {
	print H "inline void flip($alias &obj) { flip(static_cast<$cssmName &>(obj)); }\n";
  }
  
  next if ($args[0] eq 'CUSTOM');
  
  if ($args[0] eq '*') {
	# extract definition from types file
	my ($list) = $types_h =~ /{\s+([^}]+)\s+}\s*$cssmName,/;
	die "cannot find struct definition for $cssmName in $types" unless $list;
	@args = $list =~ /([A-Za-z0-9_]+);/gm;
  }

  print CPP "void flip($cssmName &obj)\n{\n";
  for my $field (@args) {
	print CPP "\tflip(obj.$field);\n";
  }
  print CPP "}\n\n";
}
