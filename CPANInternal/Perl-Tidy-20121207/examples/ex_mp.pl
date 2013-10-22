#!/usr/bin/perl -w

# example call to perltidy from man page documentation of Perl::Tidy

use strict;
use Perl::Tidy;

my $source_string = <<'EOT';
my$error=Perl::Tidy::perltidy(argv=>$argv,source=>\$source_string,
    destination=>\$dest_string,stderr=>\$stderr_string,
errorfile=>\$errorfile_string,);
EOT

my $dest_string;
my $stderr_string;
my $errorfile_string;
my $argv = "-npro";   # Ignore any .perltidyrc at this site
$argv .= " -pbp";     # Format according to perl best practices
$argv .= " -nst";     # Must turn off -st in case -pbp is specified
$argv .= " -se";      # -se appends the errorfile to stderr
## $argv .= " --spell-check";  # uncomment to trigger an error

print "<<RAW SOURCE>>\n$source_string\n";

my $error = Perl::Tidy::perltidy(
    argv        => $argv,
    source      => \$source_string,
    destination => \$dest_string,
    stderr      => \$stderr_string,
    errorfile   => \$errorfile_string,    # not used when -se flag is set
    ##phasers     => 'stun',                # uncomment to trigger an error
);

if ($error) {

    # serious error in input parameters, no tidied output
    print "<<STDERR>>\n$stderr_string\n";
    die "Exiting because of serious errors\n";
}

if ($dest_string)      { print "<<TIDIED SOURCE>>\n$dest_string\n" }
if ($stderr_string)    { print "<<STDERR>>\n$stderr_string\n" }
if ($errorfile_string) { print "<<.ERR file>>\n$errorfile_string\n" }
