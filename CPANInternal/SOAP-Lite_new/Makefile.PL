# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

require 5.005;
use strict;
use ExtUtils::MakeMaker;
use diagnostics;

my @PREREQS = (
#   ["<feature>","<installed module>",<dependency hash>,<install by default, 2=always>];
    [
        "Core Package",
        "SOAP::Lite",
        {
          "Test::More"      => 0,
          "XML::Parser"     => "2.23",
          "MIME::Base64"    => 0,
          "URI"             => 0,
          "Scalar::Util"    => 0,
          "version"         => 0,
        },
        2
    ],
    [
        "Client HTTP support",
        "SOAP::Transport::HTTP",
        {
            "LWP::UserAgent" => 0
        },
        2
    ],
    ["Client HTTPS support","SOAP::Transport::HTTPS::Client",{"Crypt::SSLeay" => 0},0],
    ["Client SMTP/sendmail support","SOAP::Transport::MAILTO::Client",{"MIME::Lite" => 0},1],
    ["Client FTP support","SOAP::Transport::FTP::Client",{"Net::FTP" => 0,"IO::File" => 0},0],
    ["Standalone HTTP server","SOAP::Transport::HTTP::Daemon",{"HTTP::Daemon" => 0},1],
    ["Apache/mod_perl server","SOAP::Transport::HTTP::Apache",{"Apache" => 0},0],
    ["FastCGI server","SOAP::Transport::HTTP::FCGI",{"FCGI" => 0},0],
    ["POP3 server","SOAP::Transport::POP3::Server",{"Net::POP3" => 0,"MIME::Parser" => 0},1],
    ["IO server","SOAP::Transport::IO::Server",{"IO::File" => 0},0],
    ["MQ transport support","SOAP::Transport::MQ",{"MQSeries" => 0},1],
    ["JABBER transport support","SOAP::Transport::JABBER",{"Net::Jabber" => 0},0],
    ["MIME messages","SOAP::Packager::MIME",{"MIME::Parser" => 0},1],
    ["DIME messages","SOAP::Packager::DIME",{"IO::Scalar" => "2.105", "DIME::Tools" => 0.03, "Data::UUID" => "0.11"},0],
    ["SSL Support for TCP Transport","SOAP::Transport::TCP",{"IO::Socket::SSL" => 0},0],
    ["Compression support for HTTP","SOAP::Transport::HTTP",{"Compress::Zlib" => 0},0],
    ["MIME interoperability w/ Axis","SOAP::Lite",{"MIME::Parser" => "6.106"},0],
);

use Getopt::Long;

my $helptext = <<EOI;
Usage: perl Makefile.PL <options>

Possible options are:

  --noprompt            Disable interactive dialog
  --alltests            Perform extra testing       (Has no effect any more)
  --help, -?            Display this help text

  [Do not] install prerequisites for appropriate module:

EOI

# Create config parameters using module names and expand help text
# We will create a hash (%config) that has each module => (1|0) for install
my(%options, %config, %has_module_cache);

# Initialize the prereq table and all help text
foreach my $prereq (@PREREQS) {
    my ($feature,$dep,$modules,$default) = @$prereq;
    next unless $dep ne "";
    $prereq->[3] = has_all_modules($modules) unless $prereq->[3] == 2;

    my $module = do { $dep =~ s/::/-/g; $dep };
    my $shortcut = do { (my $t = $module) =~ s/SOAP-(?:Transport-)?//; $t };
    $config{$dep} = has_all_modules($modules);
    $options{"install-$dep|$shortcut!"} = \$config{$dep};
    $helptext .= sprintf "  --[no]install-%-28s --[no]%s\n", $dep, $shortcut;
}

use vars qw($noprompt $alltests $help $intro);
#my ($noprompt,$alltests,$help);

GetOptions (
    "noprompt" => \$noprompt,
    "alltests" => \$alltests,
    "help|?" => \$help,
);

# don't prompt when run on a smoke test system.
# See http://cpantest.grango.org/wiki/CPANAuthorNotes
# Thanks to Andreas J Koenig && all CPAN testers !
$noprompt++ if $ENV{AUTOMATED_TESTING};

$help and print($helptext), exit;

$intro = <<EOI;
We are about to install SOAP::Lite and for your convenience will provide
you with list of modules and prerequisites, so you'll be able to choose
only modules you need for your configuration.

XMLRPC::Lite, UDDI::Lite, and XML::Parser::Lite are included by default.
Installed transports can be used for both SOAP::Lite and XMLRPC::Lite.

EOI

if ($noprompt) {
    print "These are the modules that will get installed:\n\n";
} else {
    ExtUtils::MakeMaker::prompt($intro . "Press <enter> to see the detailed list.");
}

# This hash will contain a list of all perl modules we would like to
# explicitly depend upon in our Makefile
my %prereqs;
my $proceed = 0;
do {
    print "\n".generate_prereq_table()."\n";
    # Ask the user if this is the configuration they desire
    if ($noprompt) {
        $proceed = 1;
    } else {
        $proceed = ExtUtils::MakeMaker::prompt("Do you want to proceed with this configuration?" => 'yes') =~ /^\s*y/i;
    }
    # Loop through each prerequisite and ask the user if they wish to
    # install it or not - reset prereqs, cause they change each time
    %prereqs = ();
    for my $prereq (@PREREQS) {
        my ($feature,$dep,$modules,$default) = @$prereq;
        next unless $dep ne "";
        unless ($proceed || $prereq->[3] == 2) { # need to use $prereq, because we modify actual value
            $prereq->[3] = (ExtUtils::MakeMaker::prompt("Do you plan to use ${feature}?" => ($prereq->[3] ? 'yes' : 'no')) =~ /^\s*(y)/i);
        }
        (%prereqs = (%prereqs, map { $_,$modules->{$_} } keys %$modules)) if $prereq->[3];
    }
} while (!$proceed);


# Old way of selecting tests - replaced by extended test suite in
# subdirectories...
# my $tests = join ' ', glob ($noncoretests ? 't/*.t' : 't/0*.t');

# Additional tests require Test::More
# only run noncoretests if we have Test::More
my $noncoretests;
if (eval { require Test::More; }) {
    $noncoretests++;
}

my $tests;
if ($noncoretests) {
    my @test_from = ();
    require File::Find;
    File::Find::find(sub {
            return if -d $_;
            return if $_ !~m{\.t$}x;
            return if $File::Find::dir=~m{\bCVS\b};
            return if $File::Find::dir=~m{\b\.svn\b};
            return if $_=~m{^[1-9]}x;
            push @test_from, $File::Find::name;
        }, 't/'
    );
    $tests = join ' ' , @test_from;
}
else {
    $tests = join ' ', glob ('t/0*.t');
}

ExtUtils::MakeMaker::WriteMakefile(
    'NAME'	   => 'SOAP::Lite',
    'VERSION_FROM' => 'lib/SOAP/Lite.pm',
    'PREREQ_PM'    => {
        %prereqs
     },
    'EXE_FILES'    => ['bin/SOAPsh.pl', 'bin/XMLRPCsh.pl', 'bin/stubmaker.pl'],
    test           => {TESTS => $tests},
);


######################################################
# Supporting subroutines
######################################################
# Maintains a cache of what 3rd party modules you have
# installed
sub has_module {
    my ($mod, $version) = @_;
    $version ||= '';
    # use "require Foo; Exporter::require_version('Foo' => 1)" instead of
    # obvious "use Foo 1". The later doesn't work with MIME::Parser
    # wonder why? --PK
    return ($has_module_cache{"$mod$version"}
        ||= eval("require $mod; Exporter::require_version('$mod', $version) if ($version); 1"));
}

# Return 1 if all modules contained in the inputed array
# are installed, 0 otherwise
sub has_all_modules {
    my ($mods) = @_;
    foreach my $mod (keys %$mods) {
        return 0 if !has_module($mod, $mods->{$mod});
    }
    return 1;
}

# Print a single prerequisite to the screen
sub generate_prereq {
    my ($feature,$dep,$modules,$install) = @_;
    my $i = 0;
    my $buf = "";
    foreach my $module (keys %$modules) {
        my $detected = (has_module($module, $modules->{$module}) ? "*" : " ");
        $buf .= sprintf("%-29s [%s] %-24s %-8s\n",($i++ ? "" : $feature),$detected,$module . ($modules->{$module} ? " (v$modules->{$module})" : ""),($i == 1 ? ($install ? ($install == 2 ? "always" : "[ yes ]") : "[ no ]") : ""));
    }
    return $buf;
}

# Print the entire prerequisites table
sub generate_prereq_table {
    my $buf = sprintf("%-29s %-28s %-8s\n","Feature","Prerequisites","Install?");
    $buf .= sprintf("%s %s %s\n","-" x 29,"-" x 28,"-" x 8);
    foreach my $pre (@PREREQS) {
        my ($feature,$dep,$modules,$default) = @{$pre};
        $buf .= generate_prereq(@{$pre});
    }
    $buf .= "--- An asterix '[*]' indicates if the module is currently installed.\n";
    return $buf;
}

1;
