#!/usr/bin/perl
#
# yapp -- Front end to the Parse::Yapp module
#
# (c) Copyright 1998-2001 Francois Desarmenien, all rights reserved.
# (see the pod text in Parse::Yapp module for use and distribution rights)
#
#

=head1 NAME

yapp - A perl frontend to the Parse::Yapp module


=head1 SYNOPSYS

yapp [options] I<grammar>[.yp]

yapp I<-V>

yapp I<-h>


=head1 DESCRIPTION

yapp is a frontend to the Parse::Yapp module, which lets you compile
Parse::Yapp grammar input files into Perl LALR(1) OO parser modules.


=head1 OPTIONS

Options, as of today, are all optionals :-)

=over 4

=item I<-v>

Creates a file F<grammar>.output describing your parser. It will
show you a summary of conflicts, rules, the DFA (Deterministic
Finite Automaton) states and overall usage of the parser.

=item I<-s>

Create a standalone module in which the driver is included.
Note that if you have more than one parser module called from
a program, to have it standalone, you need this option only
for one of your parser module.

=item I<-n>

Disable source file line numbering embedded in your parser module.
I don't know why one should need it, but it's there.

=item I<-m module>

Gives your parser module the package name (or name space or module name or
class name or whatever-you-call-it) of F<module>.  It defaults to F<grammar>

=item I<-o outfile>

The compiled output file will be named F<outfile> for your parser module.
It defaults to F<grammar>.pm or, if you specified the option
I<-m A::Module::Name> (see below), to F<Name.pm>.

=item I<-t filename>

The I<-t filename> option allows you to specify a file which should be 
used as template for generating the parser output.  The default is to 
use the internal template defined in F<Parse::Yapp::Output.pm>.
For how to write your own template and which substitutions are available,
have a look to the module F<Parse::Yapp::Output.pm> : it should be obvious. 

=item I<-b shebang>

If you work on systems that understand so called I<shebangs>, and your
generated parser is directly an executable script, you can specifie one
with the I<-b> option, ie:

    yapp -b '/usr/local/bin/perl -w' -o myscript.pl myscript.yp

This will output a file called F<myscript.pl> whose very first line is:

    #!/usr/local/bin/perl -w

The argument is mandatory, but if you specify an empty string, the value
of I<$Config{perlpath}> will be used instead.

=item I<grammar>

The input grammar file. If no suffix is given, and the file does not exists,
an attempt to open the file with a suffix of  F<.yp> is tried before exiting.

=item I<-V>

Display current version of Parse::Yapp and gracefully exits.

=item I<-h>

Display the usage screen.

=back

=head1 BUGS

None known now :-)

=head1 AUTHOR

Francois Desarmenien <francois@fdesar.net>

=head1 COPYRIGHT

(c) Copyright 1998-1999 Francois Desarmenien, all rights reserved.
See Parse::Yapp(3) for legal use and distribution rights

=head1 SEE ALSO

Parse::Yapp(3) Perl(1) yacc(1) bison(1)


=cut

require 5.004;

use File::Basename;
use Getopt::Std;
use Config;
use Parse::Yapp;

use strict;

use vars qw ( $opt_n $opt_m $opt_V $opt_v $opt_o $opt_h $opt_s $opt_t $opt_b);

sub Usage {
	my($prog)=(fileparse($0,'\..*'))[0];
	die <<EOF;

Usage:	$prog [options] grammar[.yp]
  or	$prog -V
  or	$prog -h

    -m module   Give your parser module the name <module>
                default is <grammar>
    -v          Create a file <grammar>.output describing your parser
    -s          Create a standalone module in which the driver is included
    -n          Disable source file line numbering embedded in your parser
    -o outfile  Create the file <outfile> for your parser module
                Default is <grammar>.pm or, if -m A::Module::Name is
                specified, Name.pm
    -t filename Uses the file <filename> as a template for creating the parser
                module file.  Default is to use internal template defined
                in Parse::Yapp::Output
    -b shebang  Adds '#!<shebang>' as the very first line of the output file

    grammar     The grammar file. If no suffix is given, and the file
                does not exists, .yp is added

    -V          Display current version of Parse::Yapp and gracefully exits
    -h          Display this help screen

EOF
}

my($nbargs)=@ARGV;

	getopts('Vhvsnb:m:t:o:')
or	Usage;

   (  ($opt_V and $nbargs > 1)
    or $opt_h)
and Usage;

	$opt_V
and do {

    @ARGV == 0 or  Usage;

    print "This is Parse::Yapp version $Parse::Yapp::Driver::VERSION.\n";
    exit(0);

};


# -t <filename> ($opt_t) option allows a file to be specified which 
# contains a 'template' to be used when generating the parser; 
# if defined, we open and read the file.   

	$opt_t
and do {
    local $/ = undef;
    local *TFILE;
    open(TFILE, $opt_t)
	or die "Cannot open template file $opt_t: $!\n";
    $opt_t = <TFILE>;
    close(TFILE);
};

    @ARGV == 1
or  Usage;

my($filename)=$ARGV[0];
my($base,$path,$sfx)=fileparse($filename,'\..*');

	-r "$filename"
or	do {
		$sfx eq '.yp'
	or	$filename.='.yp';

		-r "$filename"
	or	die "Cannot open $filename for reading.\n";
};

my($parser)=new Parse::Yapp(inputfile => $filename);

my($warnings)=$parser->Warnings();

	$warnings
and	print STDERR $warnings;

	$opt_v
and	do {
	my($output)="$path$base.output";
	my($tmp);

		open(OUT,">$output")
	or	die "Cannot create $base.output for writing.\n";

		$tmp=$parser->Warnings()
	and	print	OUT "Warnings:\n---------\n$tmp\n";
		$tmp=$parser->Conflicts()
	and	print	OUT "Conflicts:\n----------\n$tmp\n";
	print	OUT "Rules:\n------\n";
	print	OUT $parser->ShowRules()."\n";
	print	OUT "States:\n-------\n";
	print	OUT $parser->ShowDfa()."\n";
	print	OUT "Summary:\n--------\n";
	print	OUT $parser->Summary();

	close(OUT);
};

my($outfile)="$path$base.pm";
my($package)="$base";

	$opt_m
and	do {
    $package=$opt_m;
    $package=~/^(?:(?:[^:]|:(?!:))*::)*(.*)$/;
    $outfile="$1.pm";
};

	$opt_o
and	$outfile=$opt_o;

$opt_s = $opt_s ? 1 : 0;

$opt_n = $opt_n ? 0 : 1;

	open(OUT,">$outfile")
or	die "Cannot open $outfile for writing.\n";

    defined($opt_b)
and do {
        $opt_b
    or  $opt_b = $Config{perlpath};
    print OUT "#!$opt_b\n";
};

print OUT $parser->Output(classname  => $package,
                          standalone => $opt_s,
                          linenumbers => $opt_n,
                          template    => $opt_t,
                         );


close(OUT);

