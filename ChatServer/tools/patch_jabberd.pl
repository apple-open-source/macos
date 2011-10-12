#!/usr/bin/perl -w
#
# Tool that creates or applies patches for jabberd2 source.  
#
# Apple Inc. (c) 2008 - All rights reserved.
#
# NOTE: If you update to a new jabberd build, update the $JABBERD_DIST_DIR constant.

use Getopt::Std;

# Defaults and globals
# The defaults are intended for execution from the ChatServer project base source directory
$g_my_name = "patch_jabberd.pl";
$g_default_log_dir = "/tmp";
$g_default_logfile = "$g_default_log_dir/$g_my_name.log";
$g_default_tmp = "/tmp";
$g_default_source_dir = "./jabberd2";
$g_default_patch_dir = "./apple_patch/pre_configure/jabberd2";

$g_tmp = $g_default_tmp;
$g_logfile = $g_default_logfile; 

$g_update_patches = 0;

$DEBUG = 0;
$SILENT = 0;
$LOGGING = 1;
$PATCHFILE_EXT = ".patch";
#This may change from build to build and we can't predict.
$JABBERD_DIST_DIR = "jabberd-2.2.14";

sub _usage() {
    print "$g_my_name: Tool for applying or creating patch files for jabberd source\n";
    print "Usage:\n\n";
	print "== Patch Creation ==\n";
	print "Creates a set of patches for a jabberd2 source tree, comparing the original jabberd distribution\n";
	print "with a modified source tree.\n\n";
    print "$g_my_name -c [options]\n";
	print "Required Options:\n";
    print "     -n PATH:	Path to the modified Apple source code to create patches from (Default: \"$g_default_source_dir\")\n";
    print "     -f FILE:	jabberd bzip'd tarfile to use for reference\n";
    print "     -p PATH:	Path to Apple patch directory (new patch files will be created here) (Default: \"$g_default_patch_dir\")\n";
	print "\n\n";
	print "== Patch Application ==\n";
	print "Extracts the jabberd2 source from an archive and applies a set of patches to the source, leaving the patched\n";
	print "source available for building or modification.\n\n";
    print "$g_my_name -a [options]\n";
    print "Required Options:\n";
    print "     -n PATH:	Path to place the newly patched source code (Default: \"$g_default_source_dir\")\n";
    print "     -f FILE:	jabberd bzip'd tarfile to use for reference\n";
    print "     -p PATH:	Path to Apple patch directory (patches here will be applied to the original source) (Default: \"$g_default_patch_dir\")\n";
	print "Other options:\n";
    print "     -d     :	debug mode\n";
    print "	    -l FILE:    Set log file location. Default: $g_default_logfile\n";
    print "		-t PATH:	Set temp directory.  Default: $g_default_tmp\n";
    print "		-s     :	Silent mode\n";
    print "\n";
	print "Examples:\n";
	print "$g_my_name -c -n $g_default_source_dir -f ./opensource_pkgs/jabberd-2.2.14.tar.bz2 -p $g_default_patch_dir\n";
	print "$g_my_name -a -n $g_default_source_dir -f ./opensource_pkgs/jabberd-2.2.14.tar.bz2 -p $g_default_patch_dir\n";
    print "\n";
}

sub _timestamp()
{
    my ( $sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst ) =
    localtime(time);
    $year += 1900;
    $mon  += 1;
    if ( $hour =~ /^\d$/ ) { $hour = "0" . $hour; }
    if ( $min  =~ /^\d$/ ) { $min  = "0" . $min; }
    if ( $sec  =~ /^\d$/ ) { $sec  = "0" . $sec; }

    my $ret = $year."-".$mon."-".$mday."-$hour:$min:$sec";
    return $ret;
}

sub _log() {
    my $t = &_timestamp;
    if ($LOGGING) {
        open(LOG, ">>$g_logfile") || die "ERROR: could not open log file at $g_logfile: $!";
        print LOG "$t: $_[0]\n";
    }
    if (! $SILENT) { print "$t: $_[0]\n"; }
}

sub _bail() {
    &_log($_[0]);
    exit 1;
}

# Diff 2 files and create patch file 
sub create_patch {
	my $path1 = shift;
	my $path2 = shift;
	my $patch_path = shift;
	
	my $created_directory = 0;
	my $created_new_file = 0;
	
	if (-f $patch_path) {
		&_bail("Exiting, patch already exists: $patch_path");
	}

	# Create any intermediate directories
	my $patch_dir = $patch_path;
	$patch_dir =~ s/(.*[\/\\]).*/$1/;
	if (! -d $patch_dir) {
		my $ret = `mkdir -p "$patch_dir"`;
		$created_directory = 1;
	}
	
	$patch_path .= $PATCHFILE_EXT;
	if (! -e $patch_path) {
		$created_new_file = 1;
	}
	
	# If updating existing patch files, we don't want to overwrite the
	# old files with a new file that only differs by a timestamp line.
	# Make a copy of the original patch to diff against the new patch.
	if ((! $created_new_file) && ($g_update_patches)) {
		system("cp \"$patch_path\" \"$patch_path.orig\"");
	}
	
	my $cmd = "diff -bwpduN \"$path1\" \"$path2\" > \"$patch_path\"";
	&_log("Executing: $cmd");
	$ret = `$cmd`;
	
	if ($ret ne "") {
		&_bail("diff appears to have failed with error: $ret");
	}

	# Check to see if we want to restore the original (only the timestamp differs)
	if ((! $created_new_file) && ($g_update_patches)) {
		$cmd = "diff \"$patch_path.orig\" \"$patch_path\"";
		my @ret_lines = `$cmd`;
		if (($#ret_lines == 3) && 
			($ret_lines[1] =~ /\d\d\d\d-\d\d-\d\d\s\d\d:\d\d:\d\d/) &&
			($ret_lines[3] =~ /\d\d\d\d-\d\d-\d\d\s\d\d:\d\d:\d\d/)) {
			# matched a timestamp-only diff, so restore the original patch file
			system("mv -f \"$patch_path.orig\" \"$patch_path\"");
		} else {
			unlink("$patch_path.orig");
		}
	}

	# Clean up empty files & dirs
	if (! -s $patch_path) {
		unlink($patch_path);
		if ($created_directory) {
			rmdir($patch_dir);
		}
	} else {
		if ($g_update_patches) {
			if ($created_directory) {
				&_log("NOTICE: Added new directory: $patch_dir");
			}
			if ($created_new_file) {
				&_log("NOTICE: Added new patch: $patch_path");
			}
		} else {
			&_log("Created patchfile: $patch_path");
		}
	}
	
	return 0;
}

# Create a set of patches, comparing the Apple-modified source dir with the untarred jabberd2 source dir
sub create_patches {
	my $base_apple_source_dir = shift; 		# ex.: ./jabberd2
	my $jabberd_dist_dir = shift;      		# ex.: /tmp/jabberd2
	my $patch_dir = shift;             		# ex.: ./apple_patch/pre_configure/jabberd2
	my $appended_apple_source_dir = shift;	# empty on first call
	
	if ($DEBUG) {
		print "DEBUG: create_patches($base_apple_source_dir, $jabberd_dist_dir, $patch_dir, $appended_apple_source_dir)\n";
	}
	if ($appended_apple_source_dir eq "") {
		if ($g_update_patches) {
			&_log("NOTICE: Updating existing patches in: $patch_dir");
		}
	}
	
	opendir(D, $base_apple_source_dir.$appended_apple_source_dir) || 
		&_bail("can't open dir $base_apple_source_dir$appended_apple_source_dir");
	my @files = readdir(D);
	closedir(D);
	
	foreach my $file (@files) {
		if ($file eq "." || $file eq ".." || $file eq ".svn" || $file eq "CVS") {
			next;
		}
		my $full_filename = "$base_apple_source_dir$appended_apple_source_dir/$file";
		if (-d $full_filename) {
			&create_patches($base_apple_source_dir, $jabberd_dist_dir, $patch_dir, $appended_apple_source_dir."/$file");
		} else {
			&create_patch($jabberd_dist_dir.$appended_apple_source_dir."/$file", $full_filename, 
				"$patch_dir/$appended_apple_source_dir/$file");
		}
	}	
	return 0;
}


sub apply_patches {
    my $patch_dir = shift;					# ex.: ./apple_patch/pre_configure/jabberd2
    my $jabberd_dist_dir = shift;			# ex.: /tmp/jabberd2
    my $appended_apple_patch_dir = shift;	# empty on first call
	
	opendir(D, $patch_dir.$appended_apple_patch_dir) || 
		&_bail("can't open dir $patch_dir$appended_apple_patch_dir");
	my @files = readdir(D);
	closedir(D);
	
	foreach my $file (@files) {
		if ($file eq "." || $file eq ".." || $file eq ".svn" || $file eq "CVS") {
			next;
		}
		my $full_filename = "$patch_dir$appended_apple_patch_dir/$file";
		if (-d $full_filename) {
			&apply_patches($patch_dir, $jabberd_dist_dir, $appended_apple_patch_dir."/$file");
		} else {
			my $patch_target = $file;
			$patch_target =~ s/$PATCHFILE_EXT$//;
			$cmd = "patch -l \"$jabberd_dist_dir$appended_apple_patch_dir/$patch_target\" \"$full_filename\"";

			if ($DEBUG) {
				print "DEBUG: $cmd\n";
			}
			system($cmd);
		}
	}	
    return 0;
}

####### MAIN
umask(077);

my $source_dir = $g_default_source_dir;
my $jabberd_tarball = "";
my $patch_dir = $g_default_patch_dir;

getopts('n:f:p:l:t:p:ac?hvds', \%opts);
if (defined $opts{'d'}) {
    $DEBUG = 1;
}
if (defined $opts{'s'}) {
	$SILENT = 1;
}
if (defined $opts{'l'}) {
	$g_logfile = $opts{'l'};
}
if (defined $opts{'?'} || defined $opts{'h'} || defined $opts{'v'}) {
    &_usage;
    exit 0;
}
if (defined $opts{'n'}) {
	$source_dir = $opts{'n'};
}
if (defined $opts{'f'}) {
	$jabberd_tarball = $opts{'f'};
} else {
	&_bail("You must specify an archive file using the -f flag.\n");
}
if (defined $opts{'p'}) {
	$patch_dir = $opts{'p'};
}
if (defined $opts{'l'}) {
	$LOGGING = 1;
	$g_logfile = $opts{'l'};
}
if (defined $opts{'t'}) {
	$g_tmp = $opts{'t'};
}
if (defined $opts{'c'}) {
	&_log("################ CREATING PATCH FILES ################");

	if (!(defined $source_dir) && (defined $jabberd_tarball) && (defined $patch_dir)) {
		&_bail("Some required arguments are not defined.");
	}

	if (! -e $jabberd_tarball) {
		&_bail("Could not find file: $jabberd_tarball");
	}

	if (! -e $source_dir) {
		&_bail("Could not find source directory: $source_dir");
	}

	if (-e $patch_dir) {
		# We will update the existing patch dir and provide notifications for versioning purposes
		$g_update_patches = 1;
	}
		
	# untar the jabberd source tar
	`tar -C $g_tmp -xjvf $jabberd_tarball`;
	my $jabberd_dist_dir = $g_tmp."/".$JABBERD_DIST_DIR;
	
	&create_patches($source_dir, $jabberd_dist_dir, $patch_dir, "");

	if ($jabberd_dist_dir ne "") {
		&_log("Removing temporary source dir: $jabberd_dist_dir");
		system("rm -Rf $jabberd_dist_dir");
	}
	&_log("Finished creating patch files.");

} elsif (defined $opts{'a'}) {
	&_log("################ APPLYING PATCH FILES ################");

	if (!(defined $source_dir) && (defined $jabberd_tarball) && (defined $patch_dir)) {
		&_bail("Some required arguments are not defined.");
	}

	if (! -e $jabberd_tarball) {
		&_bail("Could not find file: $jabberd_tarball");
	}

	if (! -d $patch_dir) {
		&_bail("Cannot find patch dir: $patch_dir");
	}
	if (-e $source_dir) {
		&_bail("Target directory already exists: $source_dir");
	}
		
	# untar the jabberd source tar
	`tar -C $g_tmp -xjvf $jabberd_tarball`;
	my $jabberd_dist_dir = $g_tmp."/".$JABBERD_DIST_DIR;

	&apply_patches($patch_dir, $jabberd_dist_dir, "");
	system("cp -R \"$jabberd_dist_dir\" \"$source_dir\"");
	
	if ($jabberd_dist_dir ne "") {
		&_log("Removing temporary source dir: $jabberd_dist_dir");
		system("rm -Rf $jabberd_dist_dir");
	}
	&_log("Finished applying patch files.  Final source is available in: $source_dir");

} else {
    print "You did not specify an operation to perform.\n";
    &_usage;
}
