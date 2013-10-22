#!/usr/bin/perl -w
use strict;

# This program reads .perltidyrc files and writes them back out
# into a standard format (but comments will be lost).
#
# It also demonstrates how to use the perltidy 'options-dump' and related call
# parameters to read a .perltidyrc file, convert to long names, put it in a
# hash, and write back to standard output in sorted order.  Requires
# Perl::Tidy.
#
# Steve Hancock, June 2006
#
my $usage = <<EOM;
 usage:
 perltidyrc_dump.pl [-d -s -q -h] [ filename ]
  filename is the name of a .perltidyrc config file to dump, or
   if no filename is given, find and dump the system default .perltidyrc.
  -d delete options which are the same as Perl::Tidy defaults 
     (default is to keep them)
  -s write short parameter names
     (default is long names with short name in side comment)
  -q quiet: no comments 
  -h help
EOM
use Getopt::Std;
my %my_opts;
my $cmdline = $0 . " " . join " ", @ARGV;
getopts( 'hdsq', \%my_opts ) or die "$usage";
if ( $my_opts{h} ) { die "$usage" }
if ( @ARGV > 1 )   { die "$usage" }

my $config_file = $ARGV[0];
my (
    $error_message, $rOpts,          $rGetopt_flags,
    $rsections,     $rabbreviations, $rOpts_default,
    $rabbreviations_default,

) = read_perltidyrc($config_file);

# always check the error message first
if ($error_message) {
    die "$error_message\n";
}

# make a list of perltidyrc options which are same as default
my %equals_default;
foreach my $long_name ( keys %{$rOpts} ) {
    my $val = $rOpts->{$long_name};
    if ( defined( $rOpts_default->{$long_name} ) ) {
        my $val2 = $rOpts_default->{$long_name};
        if ( defined($val2) && defined($val) ) {
            $equals_default{$long_name} = ( $val2 eq $val );
        }
    }
}

# Optional: minimize the perltidyrc file length by deleting long_names
# in $rOpts which are also in $rOpts_default and have the same value.
# This would be useful if a perltidyrc file has been constructed from a
# full parameter dump, for example.
if ( $my_opts{d} ) {
    foreach my $long_name ( keys %{$rOpts} ) {
        delete $rOpts->{$long_name} if $equals_default{$long_name};
    }
}

# find user-defined abbreviations
my %abbreviations_user;
foreach my $key ( keys %$rabbreviations ) {
    unless ( $rabbreviations_default->{$key} ) {
        $abbreviations_user{$key} = $rabbreviations->{$key};
    }
}

# dump the options, if any
if ( %$rOpts || %abbreviations_user ) {
    dump_options( $cmdline, \%my_opts, $rOpts, $rGetopt_flags, $rsections,
        $rabbreviations, \%equals_default, \%abbreviations_user );
}
else {
    if ($config_file) {
        print STDERR <<EOM;
No configuration parameters seen in file: $config_file
EOM
    }
    else {
        print STDERR <<EOM;
No .perltidyrc file found, use perltidy -dpro to see locations checked.
EOM
    }
}

sub dump_options {

    # write the options back out as a valid .perltidyrc file
    # This version writes long names by sections
    my ( $cmdline, $rmy_opts, $rOpts, $rGetopt_flags, $rsections,
        $rabbreviations, $requals_default, $rabbreviations_user )
      = @_;

    # $rOpts is a reference to the hash returned by Getopt::Long
    # $rGetopt_flags are the flags passed to Getopt::Long
    # $rsections is a hash giving manual section {long_name}

    # build a hash giving section->long_name->parameter_value
    # so that we can write parameters by section
    my %section_and_name;
    my $rsection_name_value = \%section_and_name;
    my %saw_section;
    foreach my $long_name ( keys %{$rOpts} ) {
        my $section = $rsections->{$long_name};
        $section = "UNKNOWN" unless ($section);    # shouldn't happen

        # build a hash giving section->long_name->parameter_value
        $rsection_name_value->{$section}->{$long_name} = $rOpts->{$long_name};

        # remember what sections are in this hash
        $saw_section{$section}++;
    }

    # build a table for long_name->short_name abbreviations
    my %short_name;
    foreach my $abbrev ( keys %{$rabbreviations} ) {
        foreach my $abbrev ( sort keys %$rabbreviations ) {
            my @list = @{ $$rabbreviations{$abbrev} };

            # an abbreviation may expand into one or more other words,
            # but only those that expand to a single word (which must be
            # one of the long names) are the short names that we want
            # here.
            next unless @list == 1;
            my $long_name = $list[0];
            $short_name{$long_name} = $abbrev;
        }
    }

    unless ( $rmy_opts->{q} ) {
        my $date = localtime();
        print "# perltidy configuration file created $date\n";
        print "# using: $cmdline\n";
    }

    # loop to write section-by-section
    foreach my $section ( sort keys %saw_section ) {
        unless ( $rmy_opts->{q} ) {
            print "\n";

            # remove leading section number, which is there
            # for sorting, i.e.,
            # 1. Basic formatting options -> Basic formatting options
            my $trimmed_section = $section;
            $trimmed_section =~ s/^\d+\. //;
            print "# $trimmed_section\n";
        }

        # loop over all long names for this section
        my $rname_value = $rsection_name_value->{$section};
        foreach my $long_name ( sort keys %{$rname_value} ) {

            # pull out getopt flag and actual parameter value
            my $flag  = $rGetopt_flags->{$long_name};
            my $value = $rname_value->{$long_name};

            # turn this it back into a parameter
            my $prefix       = '--';
            my $short_prefix = '-';
            my $suffix       = "";
            if ($flag) {
                if ( $flag =~ /^=/ ) {
                    if ( $value !~ /^\d+$/ ) { $value = '"' . $value . '"' }
                    $suffix = "=" . $value;
                }
                elsif ( $flag =~ /^!/ ) {
                    $prefix       .= "no" unless ($value);
                    $short_prefix .= "n"  unless ($value);
                }
                elsif ( $flag =~ /^:/ ) {
                    if ( $value !~ /^\d+$/ ) { $value = '"' . $value . '"' }
                    $suffix = "=" . $value;
                }
                else {

                    # shouldn't happen
                    print
"# ERROR in dump_options: unrecognized flag $flag for $long_name\n";
                }
            }

            # print the long version of the parameter
            # with the short version as a side comment
            my $short_name   = $short_name{$long_name};
            my $short_option = $short_prefix . $short_name . $suffix;
            my $long_option  = $prefix . $long_name . $suffix;
            my $note = $requals_default->{$long_name} ? "  [=default]" : "";
            if ( $rmy_opts->{s} ) {
                print $short_option. "\n";
            }
            else {
                my $side_comment = "";
                unless ( $rmy_opts->{q} ) {
                    my $spaces = 40 - length($long_option);
                    $spaces = 2 if ( $spaces < 2 );
                    $side_comment =
                      ' ' x $spaces . '# ' . $short_option . $note;
                }
                print $long_option . $side_comment . "\n";
            }
        }
    }

    if ( %{$rabbreviations_user} ) {
        unless ( $rmy_opts->{q} ) {
            print "\n";
            print "# Abbreviations\n";
        }
        foreach my $key ( keys %$rabbreviations_user ) {
            my @vals = @{ $rabbreviations_user->{$key} };
            print $key. ' {' . join( ' ', @vals ) . '}' . "\n";
        }
    }
}

sub read_perltidyrc {

    # Example routine to have Perl::Tidy read and validate perltidyrc
    # file, and return related flags and abbreviations.
    #
    # input parameter -
    #   $config_file is the name of a .perltidyrc file we want to read
    #   or a reference to a string or array containing the .perltidyrc file
    #   if not defined, Perl::Tidy will try to find the user's .perltidyrc
    # output parameters -
    #   $error_message will be blank unless an error occurs
    #   $rOpts - reference to the hash of options in the .perlticyrc
    # NOTE:
    #   Perl::Tidy will croak or die on certain severe errors

    my ($config_file) = @_;
    my $error_message = "";
    my %Opts;    # any options found will be put here

    # the module must be installed for this to work
    eval "use Perl::Tidy";
    if ($@) {
        $error_message = "Perl::Tidy not installed\n";
        return ( $error_message, \%Opts );
    }

    # be sure this version supports this
    my $version = $Perl::Tidy::VERSION;
    if ( $version < 20060528 ) {
        $error_message = "perltidy version $version cannot read options\n";
        return ( $error_message, \%Opts );
    }

    my $stderr = "";    # try to capture error messages
    my $argv   = "";    # do not let perltidy see our @ARGV

    # we are going to make two calls to perltidy...
    # first with an empty .perltidyrc to get the default parameters
    my $empty_file = "";    # this will be our .perltidyrc file
    my %Opts_default;       # this will receive the default options hash
    my %abbreviations_default;
    my $err = Perl::Tidy::perltidy(
        perltidyrc         => \$empty_file,
        dump_options       => \%Opts_default,
        dump_options_type  => 'full',                  # 'full' gives everything
        dump_abbreviations => \%abbreviations_default,
        stderr             => \$stderr,
        argv               => \$argv,
    );
    if ($err) {
        die "Error calling perltidy\n";
    }

    # now we call with a .perltidyrc file to get its parameters
    my %Getopt_flags;
    my %sections;
    my %abbreviations;
    Perl::Tidy::perltidy(
        perltidyrc            => $config_file,
        dump_options          => \%Opts,
        dump_options_type     => 'perltidyrc',      # default is 'perltidyrc'
        dump_getopt_flags     => \%Getopt_flags,
        dump_options_category => \%sections,
        dump_abbreviations    => \%abbreviations,
        stderr                => \$stderr,
        argv                  => \$argv,
    );

    # try to capture any errors generated by perltidy call
    # but for severe errors it will typically croak
    $error_message .= $stderr;

    # debug: show how everything is stored by printing it out
    my $DEBUG = 0;
    if ($DEBUG) {
        print "---Getopt Parameters---\n";
        foreach my $key ( sort keys %Getopt_flags ) {
            print "$key$Getopt_flags{$key}\n";
        }
        print "---Manual Sections---\n";
        foreach my $key ( sort keys %sections ) {
            print "$key -> $sections{$key}\n";
        }
        print "---Abbreviations---\n";
        foreach my $key ( sort keys %abbreviations ) {
            my @names = @{ $abbreviations{$key} };
            print "$key -> {@names}\n";
            unless ( $abbreviations_default{$key} ) {
                print "NOTE: $key is user defined\n";
            }
        }
    }

    return ( $error_message, \%Opts, \%Getopt_flags, \%sections,
        \%abbreviations, \%Opts_default, \%abbreviations_default, );
}
