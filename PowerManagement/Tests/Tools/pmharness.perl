#!/usr/bin/perl

@ran_commands_list = {};
@all_tools_output = {};

run_harness();

sub print_leaks
{    
    
    $powerd_pid = `ps -axww | awk \'/powerd/{ print(\$1) }\'`;
    @powerd_pid = split(/\W/, $powerd_pid);
    $powerd_pid = @powerd_pid[0];
    
    chomp ($powerd_pid);
        
    if (($powerd_pid ne $last_powerd_pid) && $last_powerd_pid)
    {
        $error_str = "[FAIL] powerd crashed. pid changed ($last_powerd_pid -> $powerd_pid)\n";
        print $error_str;
        push(@all_tools_output, $error_str);
    }
    $last_powerd_pid = $powerd_pid;
    
    $leaks_count_configd = `leaks configd | grep -c "Leak:"`;
    chomp($leaks_count_configd);
    
    if (($leaks_count_configd gt $last_leak_count_configd) && $last_leak_count_configd) 
    {    	
        $warn_str = "[WARN] configd leak count increased ($last_leak_count_configd -> $leaks_count_configd)\n";
	    print $warn_str;
   	    push(@all_tools_output, $warn_str);
    }
    push(@all_tools_output, "configd leaks = ".$leaks_count_configd."\n");
    $last_leak_count_configd = $leaks_count_configd;

    
    $leaks_count_powerd = `leaks powerd | grep -c "Leak:"`;
    chomp ($leaks_count_powerd);
    
    if (($leaks_count_powerd gt $last_leak_count_powerd) && $last_leak_count_powerd) 
    {
        $warn_str = "[WARN] powerd leak count increased ($last_leak_count_powerd -> $leaks_count_powerd)\n";
	    print $warn_str;
    	push(@all_tools_output, $warn_str);

        my $leak_string = `leaks powerd`;
        push (@all_tools_output, $leak_string);
    }
    push(@all_tools_output, "powerd leaks = ".$leaks_powerd_output);
    $last_leak_count_powerd = $leaks_count_powerd;
}

sub run_tool
{
    my($tool_path) = @_;
    print "Running tool \"".$tool_path."\" now\n";
    
    push(@ran_commands_list, $tool_path);
    
    $tool_output = `$tool_path`;
    push(@all_tools_output, $tool_output);

    print_leaks();
}

sub run_harness
{
    print "Running tests!\n";
    
    push(@all_tools_output, localtime);
    push(@all_tools_output, "Starting tests"); 
    
    print_leaks();
    
    opendir(DIR, "Sources");

    @files = readdir(DIR);

    foreach $file(@files) {
    	$fullpath = "Sources/".$file;
        if (!(-d $fullpath) && -x $fullpath) {
            run_tool($fullpath);
        }
    }
    print "All done.\n";
            
    @now = localtime();
    $timeStamp = sprintf("%02d_%02d_%02d-%02d_%02d_%02d", 
                        $now[4]+1, $now[3], $now[5]%100, 
                        $now[2],      $now[1],   $now[0]);

    $TestResultsFileName = "PMTestResults-$timeStamp.log";
    print $TestResultsFileName;
    open(FILE, ">", $TestResultsFileName) or die $!;
    print FILE @all_tools_output;
    close FILE;
    
    
    print "\n Showing Test Results\n";
    print `"./Tools/pm_bsdtestsummarize" < $TestResultsFileName`;

}