#! /usr/bin/perl

use Getopt::Long;
use FindBin qw($Bin);
                                  # Testing defaults.  
$gdb_dir = "$Bin/..";             # top of gdb hierarchy directly from cvs
$tools = "$Bin/lib/tools";        # shell scripts used by main test scripts
$config = "$Bin/config";          # Config files users can modify.  
$repository = "$config/logs";     # log file storage.  
@logexts = qw(log out sum);       # test output files use these extensions
@flavor_lists = ("$config/ppc_flavors.txt", "$config/i386_flavors.txt");

sub machine_type {
    open (ARCH, "arch |"); my $machine=(<ARCH>); close ARCH;
    return $machine;
}
#simple perl parser for colon separated files with a line of keys at the top.  
sub read_my_file {
    my ($file_name) = @_;
    my $test;
    my @hashes = ();
    
    open (my $flavors, $file_name); @lines=<$flavors>; close ($flavors);
    $flavor_line = shift(@lines); chomp($flavor_line); 
    @hash_keys = split (':', $flavor_line);

    while (my $line, @lines) {
	$line = shift (@lines); chomp ($line);
	if (index($line, "#") != -1) { next; }
	$test = {};
	@{$test}{@hash_keys} = (split(':', ($line)));
	push (@hashes, $test);
    }
    return (@hashes);
}

sub configure_directories {
    my (@tests) = @_; my $configure;
    foreach my $test (@tests) {
	print "Configuring directory for $test->{flavor}
============================================

";
	system "cd $gdb_dir/$test->{dir} && ../src/gdb/testsuite/configure --host $test->{host} --target $test->{target} && make site.exp";
    }
}

sub create_directories {
    my (@tests) = @_;
    foreach my $test (@tests) {
	print "\nCreating directory for $test->{flavor}\n";
	system ("mkdir $gdb_dir/$test->{dir}"); 
    }
}

sub modify_site_files {
    my (@tests) = @_;
    my $additional_info;
    foreach my $test (@tests) {
	$site_file = "$gdb_dir/$$test{dir}/site.exp";

	#"puts \\"\\n\\tRunning tests on /usr/bin/gdb\\n\n".
	# "\\tKnown bugs will show up as KFAILS\\n\\"".

	system ("echo \"".
		"set target_list $$test{target_board}\n".
		"set TOOL_EXECUTABLE /usr/bin/gdb\n".
		"set site_fail KFAIL\n\" >> $site_file");
	if ($$test{is_cross_flavor}) {
	    print "Setting up $site_file for cross-compilation\n";
	    system("echo \"set srcdir $gdb_dir/src/gdb/testsuite\" >> $site_file"); 
	}
    }
}

sub clean_all {
    my (@tests) = @_;
    foreach my $test (@tests) {
	print "\nCleaning $test->{flavor}\n";
	system ("cd $gdb_dir/$test->{dir} && make clean"); 
    }
}

# this should be run with an ssh-agent on both machines ... 
sub start_tests {
    my ($test_flavors, $args) = @_;
    for $test (@{$test_flavors}) {
	if (!$test) { next; }
	else { #$ENV{DEJAGNU}="$$test{DEJAGNU}";
	    $testing = "cd $gdb_dir/$$test{dir} && make check ".
		"RUNTESTFLAGS=\' @{$args}\'>gdb.out 2>&1 </dev/null &";
	    print "Running $testing\n\n";
	    system($testing);
	}
    }
}

sub lookup_info {
    my $file_names = $_[0]; shift (@_);
    my @chosen_flavors = @_; my $flavor_count = @_;
    my @file_names = @{$file_names};    
    my (@all_flavors, @chosen_flavor_info);

    for my $file_name (@file_names) {
	push (@all_flavors, read_my_file ($file_name));
    }
    if (!$flavor_count) {
	return (@all_flavors);
    } else {
	my $found = 0;
	# Linear search for the rest of the info for each given flavor
	for my $chosen_flavor (@chosen_flavors) {
	    for my $test (@all_flavors) {
		if ("$chosen_flavor" eq "$test->{flavor}") {
		    $flavor = $test;
		    push (@chosen_flavor_info, $test);
		    $found = 1;
		}
	    }
	    if (!$found) {
		print ("Invalid flavor $flavor ... ".
		       "Ignoring this selection\n");
		$flavor = undef;
	    } 
	    $found = 0;
	}
    } 
    return @chosen_flavor_info;
}

# Smart find - looks for default or specified repository or test directory.
sub choose_directory {
    my ($dir, $flavor_dir) = @_; my $test_dir; 
    if ($dir =~ /r:(.*)$/) {
	if ($1 eq "") { $test_dir = "$repository"; }
	else { $test_dir = "$1"; }
    } else {
	$test_dir = "$dir/$flavor_dir";
    }
    return ($test_dir);
}

sub get_ext {
    my ($ext, $test) = @_; 
    if ($$test{short_flavor} ne "") {
	return $ext."_$$test{short_flavor}";
    } else {
	return $ext;
    }
}

sub get_log_filename {
    my ($log_filename, $dir, $ext, $test) = @_;
    my $log_filename .= choose_directory ($dir,$$test{dir})."/".$log_filename;
    if (!($log_filename =~ /$$test{dir}/)) {
	$log_filename .= get_ext($ext, $test);
    } else {
	$log_filename .= $ext;
    }
    return $log_filename;
}

sub move_logs {
    my ($dirs, $exts, $test, $old, $new) = @_;
    my @dirs = @{$dirs}; my @exts = @{$exts};
    for my $log (@logexts) {
	$old = get_log_filename ("gdb.$log", $dirs[0], $exts[0], $test); 
	$new = get_log_filename ("gdb.$log", $dirs[1], $exts[1], $test); 
	system ("cd $gdb_dir; mv $old $new");
    }
}

1;
